#!/usr/bin/env python3
"""Repair AstroNav 2000-2040.ods macros for LibreOffice compatibility."""

from __future__ import annotations

import datetime as dt
import json
import re
import shutil
import sqlite3
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from copy import deepcopy
from pathlib import Path
from xml.sax.saxutils import escape


SCRIPT_NS = {"script": "http://openoffice.org/2000/script"}
ODS_NS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
    "draw": "urn:oasis:names:tc:opendocument:xmlns:drawing:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
    "xlink": "http://www.w3.org/1999/xlink",
    "script": "urn:oasis:names:tc:opendocument:xmlns:script:1.0",
}
MANIFEST_NS = {"manifest": "urn:oasis:names:tc:opendocument:xmlns:manifest:1.0"}
SCRIPT_URI = "http://openoffice.org/2000/script"
LIBRARY_URI = "http://openoffice.org/2000/library"
MACRO_SOURCE_WORKBOOK = Path("src/almanac/Astro2026-36.ods")
DATA_SOURCE_WORKBOOK = Path("src/almanac/Astro2002 - old.ods")
ALMANAC_SQL_PATH = Path("packaging/almanac-db/mars_almanac.sql")
EARTH_STATE_PATH = Path("tools/earth_state_2000_2040.json")
MOON_STATE_PATH = Path("tools/moon_state_2000_2064.json")
REFERENCE_DATA_SHEETS = {"Star Data", "Planet Data", "Moon Data", "Mars Data"}
PROTECTED_SHEETS = REFERENCE_DATA_SHEETS | {"Earth State", "Moon State", "Help"}
WORKBOOK_SHEET_ORDER = [
    "Help",
    "Star Data",
    "Planet Data",
    "Moon Data",
    "Moon State",
    "Mars Data",
    "Earth State",
    "Location of Navigational Bodies",
]
# Target workbook validity range: 2000-01-01 through 2040-12-31.
# The end date is exclusive so every date in 2040 remains covered.
PLANET_WINDOW_START = dt.date(2000, 1, 1)
PLANET_WINDOW_END = dt.date(2041, 1, 1)
PLANET_WINDOW_MONTHS = 6
PLANET_TABLE_ROW_COUNT = 21
PLANET_TABLE_STRIDE = 22
PLANET_TABLE_START_ROW = 2
EARTH_STATE_TABLE_ROW_COUNT = 13
EARTH_STATE_TABLE_STRIDE = 13
EARTH_STATE_TABLE_START_ROW = 2
MOON_STATE_TABLE_ROW_COUNT = 14
MOON_STATE_TABLE_STRIDE = 14
MOON_STATE_TABLE_START_ROW = 2
CONTENT_NAMESPACE_DECLS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
    "style": "urn:oasis:names:tc:opendocument:xmlns:style:1.0",
    "draw": "urn:oasis:names:tc:opendocument:xmlns:drawing:1.0",
    "form": "urn:oasis:names:tc:opendocument:xmlns:form:1.0",
    "svg": "urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0",
    "fo": "urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0",
    "number": "urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0",
    "of": "urn:oasis:names:tc:opendocument:xmlns:of:1.2",
    "ooo": "http://openoffice.org/2004/office",
    "oooc": "http://openoffice.org/2004/calc",
    "tableooo": "http://openoffice.org/2009/table",
    "xlink": "http://www.w3.org/1999/xlink",
    "script": "urn:oasis:names:tc:opendocument:xmlns:script:1.0",
    "grddl": "http://www.w3.org/2003/g/data-view#",
    "css3t": "http://www.w3.org/TR/css3-text/",
}

for prefix, uri in CONTENT_NAMESPACE_DECLS.items():
    ET.register_namespace(prefix, uri)
ET.register_namespace("script", SCRIPT_URI)
ET.register_namespace("library", LIBRARY_URI)


BODY_HELPERS = """
' ------------------------------------------------------------------------------------------------
' LibreOffice-compatible named range helpers.
Private Function GetNamedRange(ByVal rangeName As String) As Object
   Dim oNamedRange As Object
   Dim oRange As Object

   On Error GoTo MissingRange
   oNamedRange = ThisComponent.NamedRanges.getByName(rangeName)
   On Error GoTo 0
   oRange = oNamedRange.ReferredCells
   If IsNull(oRange) Then
      Err.Raise 91, "GetNamedRange", "Named range has no referred cells: " & rangeName
   End If
   Set GetNamedRange = oRange
   Exit Function

MissingRange:
   Err.Raise 5, "GetNamedRange", "Named range not found: " & rangeName
End Function

' ------------------------------------------------------------------------------------------------
Public Function NamedRangeData(ByVal rangeName As String) As Variant
   Dim oRange As Object
   Dim oCell As Object
   Dim rowCount As Integer, colCount As Integer
   Dim rowIndex As Integer, colIndex As Integer
   Dim outData() As Variant

   Set oRange = GetNamedRange(rangeName)
   rowCount = oRange.Rows.Count
   colCount = oRange.Columns.Count
   ReDim outData(1 To rowCount, 1 To colCount)

   For rowIndex = 1 To rowCount
      For colIndex = 1 To colCount
         Set oCell = oRange.getCellByPosition(colIndex - 1, rowIndex - 1)
         If oCell.Type = 1 Then
            outData(rowIndex, colIndex) = oCell.Value
         ElseIf Trim(oCell.String) <> "" Then
            outData(rowIndex, colIndex) = oCell.String
         Else
            outData(rowIndex, colIndex) = oCell.Value
         End If
      Next colIndex
   Next rowIndex

   NamedRangeData = outData
End Function

' ------------------------------------------------------------------------------------------------
Public Sub SetNamedRangeText(ByVal rangeName As String, ByVal textValue As String)
   Dim oRange As Object

   Set oRange = GetNamedRange(rangeName)
   oRange.String = textValue
End Sub

' ------------------------------------------------------------------------------------------------
Public Function PlanetAttr(ByVal Planet As String, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, ByVal fieldIndex As Integer) As Variant
   Dim attrs As Variant
   attrs = PlanetPosition(Planet, JD, lat, lon)
   PlanetAttr = attrs(fieldIndex)
End Function

' ------------------------------------------------------------------------------------------------
Public Function StarAttr(ByVal rowStar As Integer, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, ByVal fieldIndex As Integer) As Variant
   Dim attrs As Variant
   attrs = StarPosition(rowStar, JD, lat, lon)
   StarAttr = attrs(fieldIndex)
End Function

' ------------------------------------------------------------------------------------------------
Public Function MoonAttr(ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, ByVal fieldIndex As Integer) As Variant
   Dim attrs As Variant
   attrs = MoonPosition(JD, lat, lon)
   MoonAttr = attrs(fieldIndex)
End Function
"""

VECTOR_SUPPORT = """
' ------------------------------------------------------------------------------------------------
' Definition of a 3-Vector.
Public Type Vector
   X(1 To 3) As Double
End Type

' ------------------------------------------------------------------------------------------------
' Zero 3-Vector.
Public Function vZero() As Vector
   Dim vecNew As Vector
   vZero = vecNew
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector dot product.
Public Function vDot(ByRef vec1 As Vector, ByRef vec2 As Vector) As Double
   vDot = vec1.X(1) * vec2.X(1) + vec1.X(2) * vec2.X(2) + vec1.X(3) * vec2.X(3)
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector cross product.
Public Function vCross(ByRef vec1 As Vector, ByRef vec2 As Vector) As Vector
   Dim cross As Vector
   With cross
      .X(1) = vec1.X(2) * vec2.X(3) - vec1.X(3) * vec2.X(2)
      .X(2) = vec1.X(3) * vec2.X(1) - vec1.X(1) * vec2.X(3)
      .X(3) = vec1.X(1) * vec2.X(2) - vec1.X(2) * vec2.X(1)
   End With
   vCross = cross
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector magnitude.
Public Function vMod(ByRef vec As Vector) As Double
   vMod = Sqr(vDot(vec, vec))
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector scalar multiplication.
Public Function vMul(ByRef vec As Vector, ByVal multiplier As Double) As Vector
   Dim vecNew As Vector
   vecNew.X(1) = multiplier * vec.X(1)
   vecNew.X(2) = multiplier * vec.X(2)
   vecNew.X(3) = multiplier * vec.X(3)
   vMul = vecNew
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector scalar division.
Public Function vDiv(ByRef vec As Vector, ByVal divisor As Double) As Vector
   vDiv = vMul(vec, 1 / divisor)
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector negation.
Public Function vNeg(ByRef vec As Vector) As Vector
   Dim vecNew As Vector
   vecNew.X(1) = -vec.X(1)
   vecNew.X(2) = -vec.X(2)
   vecNew.X(3) = -vec.X(3)
   vNeg = vecNew
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector unit normalisation.
Public Function vUnit(ByRef vec As Vector) As Vector
   vUnit = vDiv(vec, vMod(vec))
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector addition.
Public Function vPlus(ByRef vec1 As Vector, ByRef vec2 As Vector) As Vector
   Dim vecNew As Vector
   vecNew.X(1) = vec1.X(1) + vec2.X(1)
   vecNew.X(2) = vec1.X(2) + vec2.X(2)
   vecNew.X(3) = vec1.X(3) + vec2.X(3)
   vPlus = vecNew
End Function

' ------------------------------------------------------------------------------------------------
' 3-Vector subtraction.
Public Function vMinus(ByRef vec1 As Vector, ByRef vec2 As Vector) As Vector
   Dim vecNew As Vector
   vecNew.X(1) = vec1.X(1) - vec2.X(1)
   vecNew.X(2) = vec1.X(2) - vec2.X(2)
   vecNew.X(3) = vec1.X(3) - vec2.X(3)
   vMinus = vecNew
End Function

' ------------------------------------------------------------------------------------------------
' Rotate the 3-Vector, vec by angle radians anti-clockwise around axis (1=X,2=Y,3=Z)
Public Sub vRotate(ByRef vec As Vector, ByVal angle As Double, ByVal axis As Integer)
   Dim cosAngle As Double
   cosAngle = Cos(angle)
   vRotate2 vec, cosAngle, Sin2(angle, cosAngle), axis
End Sub

' ------------------------------------------------------------------------------------------------
' Rotate the 3-Vector, vec by angle radians anti-clockwise around axis (1=X,2=Y,3=Z)
Public Sub vRotate2(ByRef vec As Vector, ByVal cosAngle As Double, ByVal sinAngle As Double, ByVal axis As Integer)
   Dim I1 As Integer, I2 As Integer
   I1 = (axis Mod 3) + 1: I2 = (I1 Mod 3) + 1
   Dim xTemp As Double
   With vec
      xTemp = .X(I1)
      .X(I1) = xTemp * cosAngle - .X(I2) * sinAngle
      .X(I2) = xTemp * sinAngle + .X(I2) * cosAngle
   End With
End Sub
"""


CELESTIAL_MODULE = """Rem Attribute VBA_ModuleType=VBADocumentModule
Option VBASupport 1
Option Explicit
' ------------------------------------------------------------------------------------------------
Private Sub FilterToggleButton_Click()
   ' Excel-style sheet filtering is not portable in LibreOffice VBA mode.
End Sub

' ------------------------------------------------------------------------------------------------
Private Sub NowButton_Click()
   SetNamedRangeText "LocalDate", FormatDateTime(Date, vbLongDate)
   SetNamedRangeText "LocalTime", FormatDateTime(Now, vbShortTime)
End Sub
"""


FILTER_MODULE = """Rem Attribute VBA_ModuleType=VBAModule
Option VBASupport 1
Option Explicit

' ------------------------------------------------------------------------------------------------
Sub ProtectSheet()
End Sub

' ------------------------------------------------------------------------------------------------
Sub UnprotectSheet()
End Sub

' ------------------------------------------------------------------------------------------------
Private Function CelestialSheet() As Object
   Set CelestialSheet = ThisComponent.Sheets.getByName("Location of Navigational Bodies")
End Function

' ------------------------------------------------------------------------------------------------
Private Function BoolCellValue(ByVal oCell As Object) As Boolean
   Dim textValue As String

   On Error Resume Next
   If oCell.Value <> 0 Then
      BoolCellValue = True
      Exit Function
   End If
   textValue = UCase(Trim(oCell.String))
   BoolCellValue = (textValue = "TRUE" Or textValue = "YES")
End Function

' ------------------------------------------------------------------------------------------------
Sub FilterVisibles()
   Dim oSheet As Object
   Dim rowIndex As Integer
   Dim visibleCell As Object

   Set oSheet = CelestialSheet()
   For rowIndex = 5 To 71
      Set visibleCell = oSheet.getCellByPosition(8, rowIndex)
      oSheet.Rows.getByIndex(rowIndex).IsVisible = BoolCellValue(visibleCell)
   Next rowIndex
End Sub

' ------------------------------------------------------------------------------------------------
Sub ShowAllBodies()
   Dim oSheet As Object
   Dim rowIndex As Integer

   Set oSheet = CelestialSheet()
   For rowIndex = 4 To 71
      oSheet.Rows.getByIndex(rowIndex).IsVisible = True
   Next rowIndex
End Sub
"""


WORKBOOK_MODULE = """Rem Attribute VBA_ModuleType=VBADocumentModule
Option VBASupport 1
' ------------------------------------------------------------------------------------------------
Private Sub Workbook_Open()
   ' Keep document open passive. LibreOffice can otherwise spend a long time
   ' compiling/running the almanac macros before the sheet becomes usable.
End Sub

' ------------------------------------------------------------------------------------------------
Private Function IsCelestialInput(ByVal Target As Object) As Boolean
   Dim addr As String

   addr = Target.AbsoluteName
   IsCelestialInput = (InStr(addr, "$C$1") > 0 Or InStr(addr, "$C$2") > 0 Or _
                       InStr(addr, "$F$1") > 0 Or InStr(addr, "$F$2") > 0 Or _
                       InStr(addr, "$F$3") > 0)
End Function

' ------------------------------------------------------------------------------------------------
Private Function IsCalculateTrigger(ByVal Target As Object) As Boolean
   IsCalculateTrigger = False
End Function

' ------------------------------------------------------------------------------------------------
' Filter-toggle control state is not portable in LibreOffice VBA mode.
Private Sub Workbook_SheetChange(ByVal Sh As Object, ByVal Target As Object)
   On Error Resume Next
   If IsCalculateTrigger(Target) And Trim(Target.String) <> "" Then
      Target.String = ""
      RecalculateNavigationTable
   End If
End Sub
"""


CALC_API_MODULE = """Rem Attribute VBA_ModuleType=VBAModule
Option Explicit

' ------------------------------------------------------------------------------------------------
Public Function CalcApiSmokeTest() As Variant
   CalcApiSmokeTest = "OK"
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceDateValue(ByVal dateVal As Variant) As Date
   If IsDate(dateVal) Then
      CoerceDateValue = CDate(dateVal)
   ElseIf IsNumeric(dateVal) Then
      CoerceDateValue = CDate(CDbl(dateVal))
   Else
      CoerceDateValue = DateValue(CStr(dateVal))
   End If
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceTimeValue(ByVal timeVal As Variant) As Date
   If IsDate(timeVal) Then
      CoerceTimeValue = CDate(timeVal)
   ElseIf IsNumeric(timeVal) Then
      CoerceTimeValue = CDate(CDbl(timeVal))
   Else
      CoerceTimeValue = TimeValue(CStr(timeVal))
   End If
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceText(ByVal value As Variant) As String
   On Error GoTo BadValue
   If IsNull(value) Or IsEmpty(value) Then
      CoerceText = ""
   Else
      CoerceText = CStr(value)
   End If
   Exit Function

BadValue:
   CoerceText = ""
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceDouble(ByVal value As Variant) As Double
   On Error GoTo BadValue
   If IsNull(value) Or IsEmpty(value) Then
      CoerceDouble = 0#
   ElseIf IsNumeric(value) Then
      CoerceDouble = CDbl(value)
   Else
      CoerceDouble = CDbl(Val(CStr(value)))
   End If
   Exit Function

BadValue:
   CoerceDouble = 0#
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceInteger(ByVal value As Variant) As Integer
   On Error GoTo BadValue
   CoerceInteger = CInt(CoerceDouble(value))
   Exit Function

BadValue:
   CoerceInteger = 0
End Function

' ------------------------------------------------------------------------------------------------
Public Function JulianValueCalc(ByVal dateVal As Variant, ByVal timeVal As Variant) As Variant
   On Error GoTo BadValue
   JulianValueCalc = JulianValue(CoerceDateValue(dateVal), CoerceTimeValue(timeVal))
   Exit Function

BadValue:
   JulianValueCalc = 0#
End Function

' ------------------------------------------------------------------------------------------------
Public Function PlanetAttrCalc(ByVal Planet As Variant, ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant, ByVal fieldIndex As Variant) As Variant
   PlanetAttrCalc = PlanetAttr(CoerceText(Planet), CoerceDouble(JD), CoerceDouble(lat), CoerceDouble(lon), CoerceInteger(fieldIndex))
End Function

' ------------------------------------------------------------------------------------------------
Public Function StarAttrCalc(ByVal rowStar As Variant, ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant, ByVal fieldIndex As Variant) As Variant
   StarAttrCalc = StarAttr(CoerceInteger(rowStar), CoerceDouble(JD), CoerceDouble(lat), CoerceDouble(lon), CoerceInteger(fieldIndex))
End Function

' ------------------------------------------------------------------------------------------------
Public Function MoonAttrCalc(ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant, ByVal fieldIndex As Variant) As Variant
   MoonAttrCalc = MoonAttr(CoerceDouble(JD), CoerceDouble(lat), CoerceDouble(lon), CoerceInteger(fieldIndex))
End Function

' ------------------------------------------------------------------------------------------------
Private Function RowMatrix(ByVal attrs As Variant) As Variant
   Dim outData(0 To 0, 0 To 6) As Variant
   Dim i As Integer

   On Error GoTo BadValue
   For i = 0 To 6
      If IsNull(attrs(i + 1)) Or IsEmpty(attrs(i + 1)) Then
         outData(0, i) = ""
      Else
         outData(0, i) = attrs(i + 1)
      End If
   Next i
   RowMatrix = outData
   Exit Function

BadValue:
   RowMatrix = BlankAttrsMatrix()
End Function

' ------------------------------------------------------------------------------------------------
Private Function BlankAttrsMatrix() As Variant
   Dim outData(0 To 0, 0 To 6) As Variant
   Dim i As Integer

   For i = 0 To 6
      outData(0, i) = ""
   Next i
   BlankAttrsMatrix = outData
End Function

' ------------------------------------------------------------------------------------------------
Public Function PlanetPositionCalc(ByVal Planet As Variant, ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant) As Variant
   On Error GoTo CalculationError
   PlanetPositionCalc = RowMatrix(PlanetPosition(CoerceText(Planet), CoerceDouble(JD), CoerceDouble(lat), CoerceDouble(lon)))
   Exit Function

CalculationError:
   PlanetPositionCalc = BlankAttrsMatrix()
End Function

' ------------------------------------------------------------------------------------------------
Public Function MoonPositionCalc(ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant) As Variant
   On Error GoTo CalculationError
   MoonPositionCalc = RowMatrix(MoonPosition(CoerceDouble(JD), CoerceDouble(lat), CoerceDouble(lon)))
   Exit Function

CalculationError:
   MoonPositionCalc = BlankAttrsMatrix()
End Function

' ------------------------------------------------------------------------------------------------
Public Function StarPositionByNameCalc(ByVal bodyName As Variant, ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant) As Variant
   Dim starRow As Integer

   On Error GoTo CalculationError
   starRow = FindStarRow(CoerceText(bodyName))
   If starRow > 0 Then
      StarPositionByNameCalc = RowMatrix(StarPosition(starRow, CoerceDouble(JD), CoerceDouble(lat), CoerceDouble(lon)))
   Else
      StarPositionByNameCalc = BlankAttrsMatrix()
   End If
   Exit Function

CalculationError:
   StarPositionByNameCalc = BlankAttrsMatrix()
End Function

' ------------------------------------------------------------------------------------------------
Public Function GHAAriesTextCalc(ByVal JD As Variant) As Variant
   On Error GoTo CalculationError
   GHAAriesTextCalc = DegMinSec(GHAAries(CoerceDouble(JD)))
   Exit Function

CalculationError:
   GHAAriesTextCalc = ""
End Function

' ------------------------------------------------------------------------------------------------
Private Function JulianFromInputs(ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant) As Double
   JulianFromInputs = JulianValue(CoerceDateValue(dateVal), CoerceTimeValue(timeVal)) - CoerceDouble(zoneVal) / 24#
End Function

' ------------------------------------------------------------------------------------------------
Private Function DeltaTSeconds(ByVal yearValue As Integer) As Double
   Dim offsetYears As Double

   If yearValue < 1800 Then
      offsetYears = yearValue - 1700
      DeltaTSeconds = (((-0.0000000851788756 * offsetYears + 0.00013336) * offsetYears - 0.0059285) * offsetYears + 0.1603) * offsetYears + 8.83
   ElseIf yearValue < 1860 Then
      offsetYears = yearValue - 1800
      DeltaTSeconds = (((((0.000000000875 * offsetYears - 0.0000001699) * offsetYears + 0.0000121272) * offsetYears - 0.00037436) * offsetYears + 0.0041116) * offsetYears + 0.0068612) * offsetYears + 13.72
   ElseIf yearValue < 1900 Then
      offsetYears = yearValue - 1860
      DeltaTSeconds = ((((0.0000042886428 * offsetYears - 0.0004473624) * offsetYears + 0.01680668) * offsetYears - 0.251754) * offsetYears + 0.5737) * offsetYears + 7.62
   ElseIf yearValue < 1920 Then
      offsetYears = yearValue - 1900
      DeltaTSeconds = (((-0.000197 * offsetYears + 0.0061966) * offsetYears - 0.0598939) * offsetYears + 1.494119) * offsetYears - 2.79
   ElseIf yearValue < 1941 Then
      offsetYears = yearValue - 1920
      DeltaTSeconds = ((0.0020936 * offsetYears - 0.0761) * offsetYears + 0.84493) * offsetYears + 21.2
   ElseIf yearValue < 1961 Then
      offsetYears = yearValue - 1950
      DeltaTSeconds = ((0.000392618767177 * offsetYears - 0.004291845493562231) * offsetYears + 0.407) * offsetYears + 29.107
   ElseIf yearValue < 1986 Then
      offsetYears = yearValue - 1975
      DeltaTSeconds = ((-0.00139275766016713 * offsetYears - 0.00384615384615385) * offsetYears + 1.067) * offsetYears + 45.45
   ElseIf yearValue < 2005 Then
      offsetYears = yearValue - 2000
      DeltaTSeconds = ((((0.00002373599 * offsetYears + 0.000651814) * offsetYears + 0.0017275) * offsetYears - 0.060374) * offsetYears + 0.3345) * offsetYears + 63.86
   ElseIf yearValue < 2050 Then
      offsetYears = yearValue - 2000
      DeltaTSeconds = (0.005589 * offsetYears + 0.32217) * offsetYears + 62.92
   ElseIf yearValue < 2150 Then
      offsetYears = (yearValue - 1820) / 100#
      DeltaTSeconds = -20# + 32# * offsetYears * offsetYears - 0.5628 * (2150 - yearValue)
   Else
      offsetYears = (yearValue - 1820) / 100#
      DeltaTSeconds = -20# + 32# * offsetYears * offsetYears
   End If
End Function

' ------------------------------------------------------------------------------------------------
Private Function EphemerisFromInputs(ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant) As Double
   Dim civilDate As Date
   Dim jdTT As Double
   Dim gRadians As Double
   Dim tdbCorrectionSeconds As Double

   civilDate = CoerceDateValue(dateVal)
   jdTT = JulianFromInputs(dateVal, timeVal, zoneVal) + DeltaTSeconds(Year(civilDate)) / 86400#
   gRadians = (357.53 + 0.9856003 * (jdTT - 2451545#)) * kToRad
   tdbCorrectionSeconds = 0.001657 * Sin(gRadians) + 0.000022 * Sin(2# * gRadians)
   EphemerisFromInputs = jdTT + tdbCorrectionSeconds / 86400#
End Function

' ------------------------------------------------------------------------------------------------
Public Function CivilJulianDateCalc(ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant) As Variant
   On Error GoTo CalculationError
   CivilJulianDateCalc = JulianFromInputs(dateVal, timeVal, zoneVal)
   Exit Function

CalculationError:
   CivilJulianDateCalc = 0#
End Function

' ------------------------------------------------------------------------------------------------
Public Function EphemerisJulianDateCalc(ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant) As Variant
   On Error GoTo CalculationError
   EphemerisJulianDateCalc = EphemerisFromInputs(dateVal, timeVal, zoneVal)
   Exit Function

CalculationError:
   EphemerisJulianDateCalc = 0#
End Function

' ------------------------------------------------------------------------------------------------
Public Function DeltaTSecondsDateCalc(ByVal dateVal As Variant) As Variant
   On Error GoTo CalculationError
   DeltaTSecondsDateCalc = DeltaTSeconds(Year(CoerceDateValue(dateVal)))
   Exit Function

CalculationError:
   DeltaTSecondsDateCalc = 0#
End Function

' ------------------------------------------------------------------------------------------------
Public Function PlanetPositionDateCalc(ByVal Planet As Variant, ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant, ByVal lat As Variant, ByVal lon As Variant) As Variant
   On Error GoTo CalculationError
   PlanetPositionDateCalc = RowMatrix(PlanetPosition(CoerceText(Planet), EphemerisFromInputs(dateVal, timeVal, zoneVal), CoerceDouble(lat), CoerceDouble(lon), JulianFromInputs(dateVal, timeVal, zoneVal)))
   Exit Function

CalculationError:
   PlanetPositionDateCalc = BlankAttrsMatrix()
End Function

' ------------------------------------------------------------------------------------------------
Public Function MoonPositionDateCalc(ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant, ByVal lat As Variant, ByVal lon As Variant) As Variant
   On Error GoTo CalculationError
   MoonPositionDateCalc = RowMatrix(MoonPosition(EphemerisFromInputs(dateVal, timeVal, zoneVal), CoerceDouble(lat), CoerceDouble(lon), JulianFromInputs(dateVal, timeVal, zoneVal)))
   Exit Function

CalculationError:
   MoonPositionDateCalc = BlankAttrsMatrix()
End Function

' ------------------------------------------------------------------------------------------------
Public Function StarPositionByNameDateCalc(ByVal bodyName As Variant, ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant, ByVal lat As Variant, ByVal lon As Variant) As Variant
   Dim starRow As Integer

   On Error GoTo CalculationError
   starRow = FindStarRow(CoerceText(bodyName))
   If starRow > 0 Then
      StarPositionByNameDateCalc = RowMatrix(StarPosition(starRow, EphemerisFromInputs(dateVal, timeVal, zoneVal), CoerceDouble(lat), CoerceDouble(lon), JulianFromInputs(dateVal, timeVal, zoneVal)))
   Else
      StarPositionByNameDateCalc = BlankAttrsMatrix()
   End If
   Exit Function

CalculationError:
   StarPositionByNameDateCalc = BlankAttrsMatrix()
End Function

' ------------------------------------------------------------------------------------------------
Public Function GHAAriesTextDateCalc(ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant) As Variant
   On Error GoTo CalculationError
   GHAAriesTextDateCalc = DegMinSec(GHAAries(JulianFromInputs(dateVal, timeVal, zoneVal)))
   Exit Function

CalculationError:
   GHAAriesTextDateCalc = ""
End Function

' ------------------------------------------------------------------------------------------------
Public Function BodyAttrCalc(ByVal bodyName As Variant, ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant, ByVal fieldIndex As Variant) As Variant
   Dim nameText As String
   Dim starRow As Integer
   Dim fieldNo As Integer

   On Error GoTo CalculationError
   nameText = CoerceText(bodyName)
   If nameText = "" Then
      BodyAttrCalc = ""
      Exit Function
   End If
   fieldNo = CoerceInteger(fieldIndex)

   Select Case nameText
      Case "Sun", "Mercury", "Venus", "Mars", "Jupiter", "Saturn"
         BodyAttrCalc = PlanetAttrCalc(nameText, JD, lat, lon, fieldNo)
      Case "Moon"
         BodyAttrCalc = MoonAttrCalc(JD, lat, lon, fieldNo)
      Case "Aries"
         If fieldNo = 3 Then
            BodyAttrCalc = DegMinSec(GHAAries(CoerceDouble(JD)))
         Else
            BodyAttrCalc = ""
         End If
      Case Else
         starRow = FindStarRow(nameText)
         If starRow > 0 Then
            BodyAttrCalc = StarAttrCalc(starRow, JD, lat, lon, fieldNo)
         Else
            BodyAttrCalc = ""
         End If
   End Select
   Exit Function

CalculationError:
   BodyAttrCalc = ""
End Function

' ------------------------------------------------------------------------------------------------
Public Function BodyVisibleCalc(ByVal bodyName As Variant, ByVal JD As Variant, ByVal lat As Variant, ByVal lon As Variant) As Variant
   Dim altitudeText As String

   On Error GoTo CalculationError
   If CoerceText(bodyName) = "Aries" Then
      BodyVisibleCalc = "TRUE"
      Exit Function
   End If

   altitudeText = Trim(CStr(BodyAttrCalc(bodyName, JD, lat, lon, 4)))
   If altitudeText <> "" And Left(altitudeText, 1) <> "-" Then
      BodyVisibleCalc = "TRUE"
   Else
      BodyVisibleCalc = "FALSE"
   End If
   Exit Function

CalculationError:
   BodyVisibleCalc = "FALSE"
End Function

' ------------------------------------------------------------------------------------------------
Public Function NavigationTableCalc(ByVal dateVal As Variant, ByVal timeVal As Variant, ByVal zoneVal As Variant, ByVal latVal As Variant, ByVal lonVal As Variant) As Variant
   Dim oSheet As Object
   Dim jd As Double, ephemerisJD As Double, lat As Double, lon As Double
   Dim rowIndex As Integer, outRow As Integer, outCol As Integer, starRow As Integer
   Dim bodyName As String
   Dim attrs As Variant
   Dim outData(0 To 66, 0 To 7) As Variant

   On Error GoTo CalculationError
   jd = JulianValue(CoerceDateValue(dateVal), CoerceTimeValue(timeVal)) - CoerceDouble(zoneVal) / 24#
   ephemerisJD = EphemerisFromInputs(dateVal, timeVal, zoneVal)
   lat = CoerceDouble(latVal)
   lon = CoerceDouble(lonVal)

   Set oSheet = CelestialSheet()
   For rowIndex = 6 To 72
      outRow = rowIndex - 6
      bodyName = CellText(oSheet.getCellByPosition(0, rowIndex - 1))
      attrs = BlankAttrs()

      Select Case bodyName
         Case "Sun", "Mercury", "Venus", "Mars", "Jupiter", "Saturn"
            attrs = PlanetPosition(bodyName, ephemerisJD, lat, lon, jd)
         Case "Moon"
            attrs = MoonPosition(ephemerisJD, lat, lon, jd)
         Case "Aries"
            attrs(3) = DegMinSec(GHAAries(jd))
         Case Else
            starRow = FindStarRow(bodyName)
            If starRow > 0 Then
               attrs = StarPosition(starRow, ephemerisJD, lat, lon, jd)
            End If
      End Select

      For outCol = 0 To 6
         If IsNull(attrs(outCol + 1)) Or IsEmpty(attrs(outCol + 1)) Then
            outData(outRow, outCol) = ""
         Else
            outData(outRow, outCol) = CStr(attrs(outCol + 1))
         End If
      Next outCol
      outData(outRow, 7) = BodyVisibleCalc(bodyName, jd, lat, lon)
      If bodyName = "Aries" Then outData(outRow, 7) = "TRUE"
   Next rowIndex

   NavigationTableCalc = outData
   Exit Function

CalculationError:
   For outRow = 0 To 66
      For outCol = 0 To 7
         outData(outRow, outCol) = ""
      Next outCol
   Next outRow
   NavigationTableCalc = outData
End Function

' ------------------------------------------------------------------------------------------------
Private Function CelestialSheet() As Object
   Set CelestialSheet = ThisComponent.Sheets.getByName("Location of Navigational Bodies")
End Function

' ------------------------------------------------------------------------------------------------
Private Function CellText(ByVal oCell As Object) As String
   CellText = Trim(oCell.String)
End Function

' ------------------------------------------------------------------------------------------------
Private Function FindStarRow(ByVal bodyName As String) As Integer
   Dim starTable As Variant
   Dim i As Integer

   starTable = NamedRangeData("StTable")
   For i = 1 To UBound(starTable, 1)
      If UCase(CStr(starTable(i, 1))) = UCase(bodyName) Then
         FindStarRow = CoerceInteger(starTable(i, 2))
         Exit Function
      End If
   Next i
   FindStarRow = 0
End Function

' ------------------------------------------------------------------------------------------------
Private Sub WriteResultRow(ByVal rowIndex As Integer, ByVal attrs As Variant)
   Dim oSheet As Object
   Dim i As Integer
   Dim visibleText As String
   Dim textValue As String

   Set oSheet = CelestialSheet()
   For i = 1 To 7
      textValue = ""
      If Not IsNull(attrs(i)) And Not IsEmpty(attrs(i)) Then textValue = CStr(attrs(i))
      oSheet.getCellByPosition(i, rowIndex - 1).String = textValue
   Next i
   visibleText = "FALSE"
   textValue = ""
   If Not IsNull(attrs(4)) And Not IsEmpty(attrs(4)) Then textValue = Trim(CStr(attrs(4)))
   If textValue <> "" Then
      If Left(textValue, 1) <> "-" Then visibleText = "TRUE"
   End If
   oSheet.getCellByPosition(8, rowIndex - 1).String = visibleText
End Sub

' ------------------------------------------------------------------------------------------------
Private Function BlankAttrs() As Variant
   Dim attrs(1 To 7) As Variant
   BlankAttrs = attrs
End Function

' ------------------------------------------------------------------------------------------------
Public Sub RecalculateNavigationTable()
   Dim oSheet As Object
   Dim dateVal As Date, timeVal As Date
   Dim zoneHours As Double, lat As Double, lon As Double, jd As Double, ephemerisJD As Double
   Dim rowIndex As Integer, starRow As Integer
   Dim bodyName As String
   Dim attrs As Variant

   Set oSheet = CelestialSheet()
   dateVal = CoerceDateValue(oSheet.getCellByPosition(2, 0).String)
   timeVal = CoerceTimeValue(oSheet.getCellByPosition(2, 1).String)
   zoneHours = CoerceDouble(oSheet.getCellByPosition(5, 0).String)
   lat = CoerceDouble(oSheet.getCellByPosition(5, 1).String)
   lon = CoerceDouble(oSheet.getCellByPosition(5, 2).String)
   jd = JulianValue(dateVal, timeVal) - zoneHours / 24#
   ephemerisJD = EphemerisFromInputs(dateVal, timeVal, zoneHours)

   oSheet.getCellByPosition(10, 0).Value = jd

   For rowIndex = 6 To 72
      bodyName = CellText(oSheet.getCellByPosition(0, rowIndex - 1))
      attrs = BlankAttrs()

      Select Case bodyName
         Case "Sun", "Mercury", "Venus", "Mars", "Jupiter", "Saturn"
            attrs = PlanetPosition(bodyName, ephemerisJD, lat, lon, jd)
         Case "Moon"
            attrs = MoonPosition(ephemerisJD, lat, lon, jd)
         Case "Aries"
            attrs(3) = DegMinSec(GHAAries(jd))
         Case Else
            starRow = FindStarRow(bodyName)
            If starRow > 0 Then
               attrs = StarPosition(starRow, ephemerisJD, lat, lon, jd)
            End If
      End Select

      WriteResultRow rowIndex, attrs
      If bodyName = "Aries" Then oSheet.getCellByPosition(8, rowIndex - 1).String = "TRUE"
   Next rowIndex
End Sub
"""


def load_module_text(path: Path) -> str:
    root = ET.parse(path).getroot()
    return root.text or ""


def write_text_xml(path: Path, payload: str) -> None:
    path.write_text(payload, encoding="utf-8")


def module_xml_text(module_name: str, text: str) -> str:
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<script:module xmlns:script="{SCRIPT_URI}" '
        f'script:name="{module_name}" script:language="StarBasic" '
        f'script:moduleType="normal">{escape(text)}</script:module>\n'
    )


def save_module_text(path: Path, text: str) -> None:
    tree = ET.parse(path)
    root = tree.getroot()
    module_name = root.attrib.get(f"{{{SCRIPT_URI}}}name", path.stem)
    write_text_xml(path, module_xml_text(module_name, text))


def ensure_content_namespaces(content_path: Path) -> None:
    text = content_path.read_text(encoding="utf-8")
    match = re.search(r"<([A-Za-z0-9_]+:document-content)([^>]*)>", text, re.DOTALL)
    if match is None:
        return

    root_tag = match.group(0)
    inserts: list[str] = []
    for prefix, uri in CONTENT_NAMESPACE_DECLS.items():
        marker = f'xmlns:{prefix}="'
        if marker not in root_tag:
            inserts.append(f' xmlns:{prefix}="{uri}"')

    if "office:version=" not in root_tag and "ns0:version=" not in root_tag:
        inserts.append(' office:version="1.2"')

    if inserts:
        replacement = root_tag[:-1] + "".join(inserts) + ">"
        text = text.replace(root_tag, replacement, 1)
        content_path.write_text(text, encoding="utf-8")


def set_library_elements(path: Path, module_names: list[str]) -> None:
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<!DOCTYPE library:library PUBLIC "-//OpenOffice.org//DTD OfficeDocument 1.0//EN" "library.dtd">',
        f'<library:library xmlns:library="{LIBRARY_URI}" '
        'library:name="Standard" library:readonly="false" library:passwordprotected="false">',
    ]
    for module_name in module_names:
        lines.append(f' <library:element library:name="{module_name}"/>')
    lines.append("</library:library>")
    lines.append("")
    write_text_xml(path, "\n".join(lines))


def set_basic_libraries(path: Path, library_names: list[str]) -> None:
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<!DOCTYPE library:libraries PUBLIC "-//OpenOffice.org//DTD OfficeDocument 1.0//EN" "libraries.dtd">',
        f'<library:libraries xmlns:library="{LIBRARY_URI}" xmlns:xlink="http://www.w3.org/1999/xlink">',
    ]
    for library_name in library_names:
        lines.append(f' <library:library library:name="{library_name}" library:link="false"/>')
    lines.append("</library:libraries>")
    lines.append("")
    write_text_xml(path, "\n".join(lines))


def expanded_cells(row: ET.Element) -> list[ET.Element]:
    cells: list[ET.Element] = []
    for child in row:
        if child.tag != f"{{{ODS_NS['table']}}}table-cell":
            continue
        repeat = int(child.attrib.get(f"{{{ODS_NS['table']}}}number-columns-repeated", "1"))
        cells.extend([child] * repeat)
    return cells


def row_at(rows: list[ET.Element], row_number: int) -> ET.Element:
    return rows[row_number - 1]


def set_cell_text(cell: ET.Element, text_value: str) -> None:
    cell.attrib.pop(f"{{{ODS_NS['office']}}}value", None)
    cell.attrib.pop(f"{{{ODS_NS['office']}}}date-value", None)
    cell.attrib.pop(f"{{{ODS_NS['office']}}}time-value", None)
    cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = "string"
    paragraph = None
    for child in cell:
        if child.tag.endswith("}p"):
            paragraph = child
            break
    if paragraph is None:
        paragraph = ET.SubElement(cell, "{urn:oasis:names:tc:opendocument:xmlns:text:1.0}p")
    paragraph.text = text_value


def set_cell_date(cell: ET.Element, date_value: str, display_text: str) -> None:
    cell.attrib.pop(f"{{{ODS_NS['office']}}}value", None)
    cell.attrib.pop(f"{{{ODS_NS['office']}}}time-value", None)
    cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = "date"
    cell.attrib[f"{{{ODS_NS['office']}}}date-value"] = date_value
    paragraph = None
    for child in cell:
        if child.tag.endswith("}p"):
            paragraph = child
            break
    if paragraph is None:
        paragraph = ET.SubElement(cell, "{urn:oasis:names:tc:opendocument:xmlns:text:1.0}p")
    paragraph.text = display_text


def update_location_defaults(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()

    for table in root.findall(".//table:table", ODS_NS):
        if table.attrib.get(f"{{{ODS_NS['table']}}}name") != "Location of Navigational Bodies":
            continue
        rows = table.findall("table:table-row", ODS_NS)
        row1 = expanded_cells(row_at(rows, 1))
        row2 = expanded_cells(row_at(rows, 2))
        row3 = expanded_cells(row_at(rows, 3))
        set_cell_date(row1[2], "2002-01-01", "01/01/02 00:00")
        set_cell_text(row2[2], "10:00 PM")
        set_cell_text(row1[5], "0")
        set_cell_text(row2[5], "52.7077")
        set_cell_text(row3[5], "-2.7541")
        break

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def update_navigation_formulas(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()

    formula_attr = f"{{{ODS_NS['table']}}}formula"
    value_type_attr = f"{{{ODS_NS['office']}}}value-type"
    value_attr = f"{{{ODS_NS['office']}}}value"
    string_attr = f"{{{ODS_NS['office']}}}string-value"

    for table in root.findall(".//table:table", ODS_NS):
        if table.attrib.get(f"{{{ODS_NS['table']}}}name") != "Location of Navigational Bodies":
            continue
        rows = table.findall("table:table-row", ODS_NS)
        for row_number in range(6, min(len(rows), 73) + 1):
            cells = expanded_cells(row_at(rows, row_number))
            for column_number in range(2, 10):
                cell = cells[column_number - 1]
                cell.attrib.pop(formula_attr, None)
                cell.attrib[value_type_attr] = "string"
                cell.attrib.pop(value_attr, None)
                cell.attrib.pop(string_attr, None)
                paragraph = None
                for child in cell:
                    if child.tag.endswith("}p"):
                        paragraph = child
                        break
                if paragraph is None:
                    paragraph = ET.SubElement(cell, "{urn:oasis:names:tc:opendocument:xmlns:text:1.0}p")
                paragraph.text = ""
        break

    for table in root.findall(".//table:table", ODS_NS):
        if table.attrib.get(f"{{{ODS_NS['table']}}}name") != "Location of Navigational Bodies":
            continue
        rows = table.findall("table:table-row", ODS_NS)
        row1 = expanded_cells(row_at(rows, 1))
        if len(row1) > 255:
            julian_cell = row1[255]
            julian_cell.attrib.pop(formula_attr, None)
            julian_cell.attrib[value_type_attr] = "float"
            julian_cell.attrib[value_attr] = "0"
        break

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def strip_sheet_controls(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()

    forms_tag = f"{{{ODS_NS['office']}}}forms"
    draw_control_tag = f"{{{ODS_NS['draw']}}}control"

    for table in root.findall(".//table:table", ODS_NS):
        for child in list(table):
            if child.tag == forms_tag:
                table.remove(child)

        for row in table.findall("table:table-row", ODS_NS):
            for cell in row.findall("table:table-cell", ODS_NS):
                for child in list(cell):
                    if child.tag == draw_control_tag:
                        cell.remove(child)

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def cell_has_content(cell: ET.Element) -> bool:
    formula_attr = f"{{{ODS_NS['table']}}}formula"
    value_attrs = {
        f"{{{ODS_NS['office']}}}value",
        f"{{{ODS_NS['office']}}}string-value",
        f"{{{ODS_NS['office']}}}date-value",
        f"{{{ODS_NS['office']}}}time-value",
    }

    if formula_attr in cell.attrib:
        return True
    if any(attr in cell.attrib for attr in value_attrs):
        return True
    return any("".join(child.itertext()).strip() for child in cell)


def trim_repeated_columns(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()
    repeat_attr = f"{{{ODS_NS['table']}}}number-columns-repeated"
    table_column_tag = f"{{{ODS_NS['table']}}}table-column"
    table_cell_tag = f"{{{ODS_NS['table']}}}table-cell"

    for table in root.findall(".//table:table", ODS_NS):
        last_used_col = 1
        for row in table.findall("table:table-row", ODS_NS):
            logical_col = 1
            for cell in row.findall("table:table-cell", ODS_NS):
                repeat = int(cell.attrib.get(repeat_attr, "1"))
                if cell_has_content(cell):
                    last_used_col = max(last_used_col, logical_col + repeat - 1)
                logical_col += repeat

        logical_col = 1
        for col in list(table.findall("table:table-column", ODS_NS)):
            repeat = int(col.attrib.get(repeat_attr, "1"))
            end_col = logical_col + repeat - 1
            if logical_col > last_used_col:
                table.remove(col)
            elif end_col > last_used_col:
                new_repeat = last_used_col - logical_col + 1
                if new_repeat <= 1:
                    col.attrib.pop(repeat_attr, None)
                else:
                    col.attrib[repeat_attr] = str(new_repeat)
            logical_col = end_col + 1

        for row in table.findall("table:table-row", ODS_NS):
            logical_col = 1
            for cell in list(row):
                if cell.tag != table_cell_tag:
                    continue
                repeat = int(cell.attrib.get(repeat_attr, "1"))
                end_col = logical_col + repeat - 1
                if logical_col > last_used_col:
                    row.remove(cell)
                elif end_col > last_used_col:
                    new_repeat = last_used_col - logical_col + 1
                    if new_repeat <= 1:
                        cell.attrib.pop(repeat_attr, None)
                    else:
                        cell.attrib[repeat_attr] = str(new_repeat)
                logical_col = end_col + 1

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def strip_named_and_database_ranges(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()
    spreadsheet = root.find(".//office:spreadsheet", ODS_NS)
    if spreadsheet is None:
        return

    removable_tags = {
        f"{{{ODS_NS['table']}}}named-expressions",
        f"{{{ODS_NS['table']}}}database-ranges",
    }
    for child in list(spreadsheet):
        if child.tag in removable_tags:
            spreadsheet.remove(child)

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def replace_reference_data_sheets(content_path: Path, source_workbook: Path) -> None:
    if not source_workbook.exists():
        raise FileNotFoundError(f"Reference workbook not found: {source_workbook}")

    with zipfile.ZipFile(source_workbook) as zf:
        source_content = ET.fromstring(zf.read("content.xml"))

    source_tables: dict[str, ET.Element] = {}
    table_name_attr = f"{{{ODS_NS['table']}}}name"
    for table in source_content.findall(".//table:table", ODS_NS):
        sheet_name = table.attrib.get(table_name_attr)
        if sheet_name in REFERENCE_DATA_SHEETS:
            source_tables[sheet_name] = table

    missing = sorted(REFERENCE_DATA_SHEETS - set(source_tables))
    if missing:
        raise RuntimeError(f"Reference workbook is missing data sheets: {', '.join(missing)}")

    tree = ET.parse(content_path)
    root = tree.getroot()
    spreadsheet = root.find(".//office:spreadsheet", ODS_NS)
    if spreadsheet is None:
        raise RuntimeError("No spreadsheet body found in content.xml")

    for index, table in enumerate(list(spreadsheet)):
        if table.tag != f"{{{ODS_NS['table']}}}table":
            continue
        sheet_name = table.attrib.get(table_name_attr)
        if sheet_name in source_tables:
            spreadsheet.remove(table)
            spreadsheet.insert(index, deepcopy(source_tables[sheet_name]))

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def add_months(value: dt.date, months: int) -> dt.date:
    month_index = value.month - 1 + months
    year = value.year + month_index // 12
    month = month_index % 12 + 1
    month_lengths = [31, 29 if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0) else 28,
                     31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    return dt.date(year, month, min(value.day, month_lengths[month - 1]))


def planet_window_jds() -> list[tuple[float, float]]:
    """Return the fixed workbook-owned Planet Data windows."""
    windows: list[tuple[float, float]] = []
    start_date = PLANET_WINDOW_START
    while start_date < PLANET_WINDOW_END:
        end_date = add_months(start_date, PLANET_WINDOW_MONTHS)
        if end_date > PLANET_WINDOW_END:
            end_date = PLANET_WINDOW_END
        windows.append((jd_from_date(start_date), jd_from_date(end_date)))
        start_date = end_date
    return windows


def planet_windows() -> list[tuple[float, float]]:
    return planet_window_jds()


def load_earth_state_model() -> dict:
    return json.loads(EARTH_STATE_PATH.read_text(encoding="utf-8"))


def load_moon_state_model() -> dict:
    return json.loads(MOON_STATE_PATH.read_text(encoding="utf-8"))


def jd_from_date(value: dt.date) -> float:
    a = (14 - value.month) // 12
    y = value.year + 4800 - a
    m = value.month + 12 * a - 3
    jdn = value.day + ((153 * m + 2) // 5) + 365 * y + y // 4 - y // 100 + y // 400 - 32045
    return float(jdn) - 0.5


def workbook_date_text(value: dt.date) -> str:
    return value.strftime("%d-%b-%y")


def ods_cell(text_value: str = "", value_type: str | None = None, value: str | None = None) -> ET.Element:
    cell = ET.Element(f"{{{ODS_NS['table']}}}table-cell")
    if value_type:
        cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = value_type
    if value is not None:
        cell.attrib[f"{{{ODS_NS['office']}}}value"] = value
    if text_value:
        paragraph = ET.SubElement(cell, f"{{{ODS_NS['text']}}}p")
        paragraph.text = text_value
    return cell


def ods_number_cell(number: float, text_value: str | None = None) -> ET.Element:
    return ods_cell(text_value if text_value is not None else f"{number:.12g}", "float", f"{number:.12g}")


def ods_text_cell(text_value: str) -> ET.Element:
    return ods_cell(text_value, "string")


def ods_link_cell(text_value: str, url: str) -> ET.Element:
    cell = ET.Element(f"{{{ODS_NS['table']}}}table-cell")
    cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = "string"
    paragraph = ET.SubElement(cell, f"{{{ODS_NS['text']}}}p")
    link = ET.SubElement(paragraph, f"{{{ODS_NS['text']}}}a")
    link.attrib[f"{{{ODS_NS['xlink']}}}type"] = "simple"
    link.attrib[f"{{{ODS_NS['xlink']}}}href"] = url
    link.text = text_value
    return cell


def replace_earth_state_with_generated_windows(content_path: Path) -> None:
    model = load_earth_state_model()
    table_uri = ODS_NS["table"]
    repeat_attr = f"{{{table_uri}}}number-columns-repeated"
    generated = ET.Element(f"{{{table_uri}}}table")
    generated.attrib[f"{{{table_uri}}}name"] = "Earth State"
    column = ET.SubElement(generated, f"{{{table_uri}}}table-column")
    column.attrib[repeat_attr] = "8"
    ET.SubElement(generated, f"{{{table_uri}}}table-row")

    for row in model["rows"]:
        start_jd = float(row["start_jd"])
        end_jd = float(row["end_jd"])
        mid_jd = float(row["mid_jd"])
        span_days = float(row["span_days"])

        header = ET.SubElement(generated, f"{{{table_uri}}}table-row")
        for cell in [
            ods_text_cell("Start JD"),
            ods_number_cell(start_jd, f"{start_jd:.5f}"),
            ods_text_cell("End JD"),
            ods_number_cell(end_jd, f"{end_jd:.5f}"),
            ods_text_cell("Mid JD"),
            ods_number_cell(mid_jd, f"{mid_jd:.5f}"),
            ods_text_cell("Span days"),
            ods_number_cell(span_days, f"{span_days:.5f}"),
        ]:
            header.append(cell)

        label_row = ET.SubElement(generated, f"{{{table_uri}}}table-row")
        for text_value in ["", "X", "Y", "Z", "VX", "VY", "VZ", ""]:
            label_row.append(ods_text_cell(text_value) if text_value else ods_cell())

        coeffs = row["coefficients"]
        for coeff_index in range(int(model["degree"]) + 1):
            coeff_row = ET.SubElement(generated, f"{{{table_uri}}}table-row")
            coeff_row.append(ods_text_cell(f"c{coeff_index}"))
            for component in range(6):
                number = float(coeffs[component][coeff_index])
                coeff_row.append(ods_number_cell(number, f"{number:.16g}"))
            coeff_row.append(ods_cell())

        ET.SubElement(generated, f"{{{table_uri}}}table-row")

    tree = ET.parse(content_path)
    root = tree.getroot()
    spreadsheet = root.find(".//office:spreadsheet", ODS_NS)
    if spreadsheet is None:
        raise RuntimeError("No spreadsheet body found in content.xml")
    for index, table in enumerate(list(spreadsheet)):
        if table.tag == f"{{{ODS_NS['table']}}}table" and table.attrib.get(f"{{{ODS_NS['table']}}}name") == "Earth State":
            spreadsheet.remove(table)
            spreadsheet.insert(index, generated)
            break
    else:
        spreadsheet.append(generated)
    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def replace_moon_state_with_generated_windows(content_path: Path) -> None:
    model = load_moon_state_model()
    table_uri = ODS_NS["table"]
    repeat_attr = f"{{{table_uri}}}number-columns-repeated"
    generated = ET.Element(f"{{{table_uri}}}table")
    generated.attrib[f"{{{table_uri}}}name"] = "Moon State"
    column = ET.SubElement(generated, f"{{{table_uri}}}table-column")
    column.attrib[repeat_attr] = "8"
    ET.SubElement(generated, f"{{{table_uri}}}table-row")

    for row in model["rows"]:
        start_jd = float(row["start_jd"])
        end_jd = float(row["end_jd"])
        mid_jd = float(row["mid_jd"])
        half_span_days = float(row["span_days"])

        header = ET.SubElement(generated, f"{{{table_uri}}}table-row")
        for cell in [
            ods_text_cell("Start JD"),
            ods_number_cell(start_jd, f"{start_jd:.5f}"),
            ods_text_cell("End JD"),
            ods_number_cell(end_jd, f"{end_jd:.5f}"),
            ods_text_cell("Mid JD"),
            ods_number_cell(mid_jd, f"{mid_jd:.5f}"),
            ods_text_cell("Half span days"),
            ods_number_cell(half_span_days, f"{half_span_days:.5f}"),
        ]:
            header.append(cell)

        label_row = ET.SubElement(generated, f"{{{table_uri}}}table-row")
        for text_value in ["", "X", "Y", "Z", "Distance AU", "Ecliptic lon", "", ""]:
            label_row.append(ods_text_cell(text_value) if text_value else ods_cell())

        coeffs = row["coefficients"]
        for coeff_index in range(int(model["degree"]) + 1):
            coeff_row = ET.SubElement(generated, f"{{{table_uri}}}table-row")
            coeff_row.append(ods_text_cell(f"c{coeff_index}"))
            for component in range(5):
                number = float(coeffs[component][coeff_index])
                coeff_row.append(ods_number_cell(number, f"{number:.16g}"))
            coeff_row.append(ods_cell())
            coeff_row.append(ods_cell())

        ET.SubElement(generated, f"{{{table_uri}}}table-row")

    tree = ET.parse(content_path)
    root = tree.getroot()
    spreadsheet = root.find(".//office:spreadsheet", ODS_NS)
    if spreadsheet is None:
        raise RuntimeError("No spreadsheet body found in content.xml")
    for index, table in enumerate(list(spreadsheet)):
        if table.tag == f"{{{ODS_NS['table']}}}table" and table.attrib.get(f"{{{ODS_NS['table']}}}name") == "Moon State":
            spreadsheet.remove(table)
            spreadsheet.insert(index, generated)
            break
    else:
        spreadsheet.append(generated)
    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def replace_help_sheet(content_path: Path) -> None:
    model = load_moon_state_model()
    validation = model.get("validation", {})
    post_2040 = validation.get("post_2040_worst_arcsec_by_year", {})
    table_uri = ODS_NS["table"]
    repeat_attr = f"{{{table_uri}}}number-columns-repeated"

    generated = ET.Element(f"{{{table_uri}}}table")
    generated.attrib[f"{{{table_uri}}}name"] = "Help"
    column = ET.SubElement(generated, f"{{{table_uri}}}table-column")
    column.attrib[repeat_attr] = "6"

    def row(*cells: ET.Element) -> None:
        tr = ET.SubElement(generated, f"{{{table_uri}}}table-row")
        for cell in cells:
            tr.append(cell)

    islands = [
        (
            "Atlantic",
            "Azores: Faial and Pico",
            "Mid-ocean landfall, volcanic peaks, whale-watching, and Horta's blue-water sailor culture.",
            "https://commons.wikimedia.org/wiki/Category:Azores",
            "https://www.openstreetmap.org/?mlat=38.58&mlon=-28.70#map=7/38.58/-28.70",
        ),
        (
            "Atlantic",
            "Madeira and Porto Santo",
            "Subtropical cliffs, levada walks, good provisioning, and a classic Atlantic stopover feel.",
            "https://commons.wikimedia.org/wiki/Category:Madeira",
            "https://www.openstreetmap.org/?mlat=32.75&mlon=-16.95#map=8/32.75/-16.95",
        ),
        (
            "Atlantic",
            "Cape Verde: Santo Antao and Sao Vicente",
            "Dry volcanic drama, music, trade-wind sailing, and a proper ocean-crossing staging post.",
            "https://commons.wikimedia.org/wiki/Category:Cape_Verde",
            "https://www.openstreetmap.org/?mlat=16.88&mlon=-25.00#map=8/16.88/-25.00",
        ),
        (
            "Atlantic",
            "Canaries: La Gomera and La Palma",
            "Reliable services, steep green ravines, dark-sky nights, and forgiving Atlantic logistics.",
            "https://commons.wikimedia.org/wiki/Category:Canary_Islands",
            "https://www.openstreetmap.org/?mlat=28.25&mlon=-17.10#map=7/28.25/-17.10",
        ),
        (
            "Pacific",
            "Bora Bora",
            "A ridiculous lagoon, Mount Otemanu on the horizon, and reef colours that make instruments feel underdressed.",
            "https://commons.wikimedia.org/wiki/Category:Bora_Bora",
            "https://www.openstreetmap.org/?mlat=-16.50&mlon=-151.74#map=11/-16.50/-151.74",
        ),
        (
            "Pacific",
            "Fiji: Yasawa Islands",
            "Warm village welcomes, blue-water anchorages, reefs, beaches, and proper South Pacific scale.",
            "https://commons.wikimedia.org/wiki/Category:Yasawa_Islands",
            "https://www.openstreetmap.org/?mlat=-16.92&mlon=177.34#map=9/-16.92/177.34",
        ),
        (
            "Pacific",
            "Galapagos: Santa Cruz",
            "Unique wildlife and volcanic landscapes; stunning, but plan formalities and environmental rules carefully.",
            "https://commons.wikimedia.org/wiki/Category:Santa_Cruz_Island_(Gal%C3%A1pagos)",
            "https://www.openstreetmap.org/?mlat=-0.64&mlon=-90.36#map=9/-0.64/-90.36",
        ),
        (
            "Pacific",
            "Vanuatu: Tanna",
            "A living volcano, kastom culture, black-sand coasts, and anchorages that feel genuinely far away.",
            "https://commons.wikimedia.org/wiki/Category:Tanna",
            "https://www.openstreetmap.org/?mlat=-19.53&mlon=169.28#map=10/-19.53/169.28",
        ),
        (
            "Indian",
            "Seychelles: La Digue and Praslin",
            "Granite boulders, clear water, birdlife, and islands that look like someone overdid paradise in a good way.",
            "https://commons.wikimedia.org/wiki/Category:La_Digue",
            "https://www.openstreetmap.org/?mlat=-4.35&mlon=55.84#map=10/-4.35/55.84",
        ),
        (
            "Indian",
            "Maldives: Ari Atoll",
            "Atoll navigation, luminous lagoons, diving, and sunrise passages over impossibly blue water.",
            "https://commons.wikimedia.org/wiki/Category:Ari_Atoll",
            "https://www.openstreetmap.org/?mlat=3.92&mlon=72.83#map=8/3.92/72.83",
        ),
        (
            "Indian",
            "Mauritius and Rodrigues",
            "Creole food, reef passes, mountain backdrops, and trade-wind sailing with serious colour.",
            "https://commons.wikimedia.org/wiki/Category:Rodrigues",
            "https://www.openstreetmap.org/?mlat=-19.70&mlon=63.42#map=9/-19.70/63.42",
        ),
        (
            "Indian",
            "Reunion",
            "Volcanic hikes, French-Creole harbour culture, dramatic coasts, and a useful Indian Ocean waypoint.",
            "https://commons.wikimedia.org/wiki/Category:R%C3%A9union",
            "https://www.openstreetmap.org/?mlat=-21.12&mlon=55.53#map=9/-21.12/55.53",
        ),
    ]

    row(ods_text_cell("AstroNav 2000-2040 Navigation Almanac"))
    row(ods_text_cell("Purpose"), ods_text_cell("This workbook is intended as a navigation almanac aid. Enter GMT date/time, zone, latitude, and longitude on the Location of Navigational Bodies sheet."))
    row(ods_text_cell("Safety disclaimer"), ods_text_cell("Not a certified navigation instrument, not an official nautical almanac, and not a substitute for official charts, notices to mariners, pilot books, tide/current data, compass, depth sounder, GNSS, radar/AIS, visual bearings, celestial practice, seamanship, or judgement. Use redundant methods and check critical observations. No warranty is given."))
    row(ods_text_cell("Legal note"), ods_text_cell("This disclaimer is practical safety wording, not legal advice. If this workbook is distributed, ask a qualified lawyer to review the exact licence/disclaimer for your jurisdiction."))
    row(ods_text_cell("Certified range"), ods_text_cell("2000-01-01 through 2040-12-31 GMT. Target accuracy is within 6 arc-seconds after rounding to the nearest arc-second for the listed navigation bodies."))
    row(ods_text_cell("Beyond 2040"), ods_text_cell("Beyond 2040, use at your own risk! Moon data is included through 2064-12-31 GMT for graceful overrun checks, but the workbook target remains 2000-2040 unless every body and calculation path is revalidated for a wider range."))
    row(ods_text_cell("Moon model"), ods_text_cell("The Moon uses SPICE DE440s apparent geocentric data fitted directly in the true equator/equinox of date. This avoids the old low-order lunar series and old precession/nutation path."))
    row(ods_text_cell("Sheet protection"), ods_text_cell("Supporting data sheets and this Help sheet are protected without a password to reduce accidental corruption. The main Location sheet is left editable for date/time, position, and visible-body filtering."))
    row(ods_cell())
    row(ods_text_cell("Year"), ods_text_cell("Expected Moon worst coordinate error (arcsec)"), ods_text_cell("Validation sampling"))
    for year in [*range(2041, 2051), *range(2060, 2065)]:
        value = float(post_2040.get(str(year), 0.0))
        row(ods_text_cell(str(year)), ods_text_cell(f"{value:.1f} arcsec"), ods_text_cell(validation.get("sampling", "weekly at 22:00 GMT")))
    row(ods_cell())
    row(ods_text_cell("Navigation accident statistics"))
    row(ods_text_cell("What I could source"), ods_text_cell("I could not find a reliable global statistic for yachts lost at sea solely because of faulty navigation tools. Official accident data usually records multiple factors and does not cleanly isolate 'yachts lost at sea due to faulty navigation tools'."))
    row(ods_text_cell("Closest official example"), ods_text_cell("The U.S. Coast Guard Recreational Boating Statistics 2024 tables list, for all U.S. recreational vessels: Onboard navigation aid equipment failure: 0 accidents, 0 deaths, 0 injuries; Missing/inadequate navigation aid: 40 accidents, 2 deaths, 15 injuries; Navigation rules violation: 288 accidents, 17 deaths, 163 injuries. Treat these as U.S. recreational-boating incident factors, not a worldwide yacht-loss count."))
    row(ods_link_cell("USCG accident statistics page", "https://uscgboating.org/statistics/accident_statistics.php"), ods_link_cell("USCG 2024 statistics PDF", "https://uscgboating.org/library/accident-statistics/Recreational-Boating-Statistics-2024.pdf"))
    row(ods_cell())
    row(ods_text_cell("Visible filter"), ods_text_cell("Use the filter on the Visible column to show YES only, or clear it to show all bodies."))
    row(ods_text_cell("Data sheets"), ods_text_cell("Star, planet, Moon, Mars, and Earth State sheets are supporting data for the calculations. Keep them with the workbook."))
    row(ods_cell())
    row(ods_text_cell("Island daydreams for the passage plan"))
    row(ods_text_cell("Ocean"), ods_text_cell("Island"), ods_text_cell("Why it is lovely"), ods_text_cell("Images"), ods_text_cell("Map"))
    for ocean, island, why, images, map_url in islands:
        row(
            ods_text_cell(ocean),
            ods_text_cell(island),
            ods_text_cell(why),
            ods_link_cell("Commons images", images),
            ods_link_cell("OpenStreetMap", map_url),
        )
    row(ods_cell())
    row(ods_text_cell("Media/map caution"), ods_text_cell("External image links are for inspiration and may have their own licences. OpenStreetMap links are useful overview maps, not nautical charts. Always navigate with current official nautical charts and local notices."))

    tree = ET.parse(content_path)
    root = tree.getroot()
    spreadsheet = root.find(".//office:spreadsheet", ODS_NS)
    if spreadsheet is None:
        raise RuntimeError("No spreadsheet body found in content.xml")
    for index, table in enumerate(list(spreadsheet)):
        if table.tag == f"{{{ODS_NS['table']}}}table" and table.attrib.get(f"{{{ODS_NS['table']}}}name") == "Help":
            spreadsheet.remove(table)
            spreadsheet.insert(index, generated)
            break
    else:
        spreadsheet.append(generated)
    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def cell_text_value(cell: ET.Element) -> str:
    parts: list[str] = []
    for paragraph in cell.findall(".//text:p", ODS_NS):
        parts.append("".join(paragraph.itertext()))
    return "".join(parts)


def rebuild_minimal_content(content_path: Path) -> None:
    source_root = ET.parse(content_path).getroot()
    office_uri = ODS_NS["office"]
    table_uri = ODS_NS["table"]
    text_uri = ODS_NS["text"]
    xlink_uri = ODS_NS["xlink"]
    repeat_attr = f"{{{table_uri}}}number-columns-repeated"
    value_attrs = [
        f"{{{office_uri}}}value-type",
        f"{{{office_uri}}}value",
        f"{{{office_uri}}}boolean-value",
        f"{{{office_uri}}}date-value",
        f"{{{office_uri}}}time-value",
        f"{{{office_uri}}}string-value",
    ]
    max_cols_by_sheet = {
        "Location of Navigational Bodies": 9,
    }

    root = ET.Element(f"{{{office_uri}}}document-content")
    root.attrib[f"{{{office_uri}}}version"] = "1.2"
    body = ET.SubElement(root, f"{{{office_uri}}}body")
    spreadsheet = ET.SubElement(body, f"{{{office_uri}}}spreadsheet")

    source_tables = {
        table.attrib.get(f"{{{table_uri}}}name", "Sheet"): table
        for table in source_root.findall(".//table:table", ODS_NS)
    }
    ordered_tables = []
    for sheet_name in WORKBOOK_SHEET_ORDER:
        if sheet_name in source_tables:
            ordered_tables.append(source_tables.pop(sheet_name))
    ordered_tables.extend(source_tables.values())

    for source_table in ordered_tables:
        sheet_name = source_table.attrib.get(f"{{{table_uri}}}name", "Sheet")
        max_col = max_cols_by_sheet.get(sheet_name)
        rows_out: list[list[tuple[int, ET.Element]]] = []
        table_max_col = 0

        for row_number, source_row in enumerate(source_table.findall("table:table-row", ODS_NS), start=1):
            logical_col = 1
            row_cells: list[tuple[int, ET.Element]] = []
            for source_cell in source_row.findall("table:table-cell", ODS_NS):
                repeat = int(source_cell.attrib.get(repeat_attr, "1"))
                for _ in range(repeat):
                    if max_col is not None and logical_col > max_col:
                        logical_col += 1
                        continue
                    text_value = cell_text_value(source_cell)
                    has_value = any(attr in source_cell.attrib for attr in value_attrs)
                    if text_value or has_value:
                        new_cell = ET.Element(f"{{{table_uri}}}table-cell")
                        for attr in value_attrs:
                            if attr in source_cell.attrib:
                                new_cell.attrib[attr] = source_cell.attrib[attr]
                        if text_value:
                            paragraph = ET.SubElement(new_cell, f"{{{text_uri}}}p")
                            paragraph.text = text_value
                        row_cells.append((logical_col, new_cell))
                        table_max_col = max(table_max_col, logical_col)
                    logical_col += 1
            rows_out.append(row_cells)

        table = ET.SubElement(spreadsheet, f"{{{table_uri}}}table")
        table.attrib.update(source_table.attrib)
        table.attrib[f"{{{table_uri}}}name"] = sheet_name
        if table_max_col > 0:
            col = ET.SubElement(table, f"{{{table_uri}}}table-column")
            if table_max_col > 1:
                col.attrib[repeat_attr] = str(table_max_col)

        for row_cells in rows_out:
            row = ET.SubElement(table, f"{{{table_uri}}}table-row")
            current_col = 1
            for logical_col, cell in row_cells:
                gap = logical_col - current_col
                if gap > 0:
                    empty = ET.SubElement(row, f"{{{table_uri}}}table-cell")
                    if gap > 1:
                        empty.attrib[repeat_attr] = str(gap)
                row.append(cell)
                current_col = logical_col + 1

    add_functional_named_ranges(spreadsheet)
    ET.ElementTree(root).write(content_path, encoding="UTF-8", xml_declaration=True)


def add_functional_named_ranges(spreadsheet: ET.Element) -> None:
    table_uri = ODS_NS["table"]
    named_ranges = [
        ("GHA\u03b3", "$'Location of Navigational Bodies'.$D$73"),
        ("Latitude", "$'Location of Navigational Bodies'.$F$2"),
        ("LocalDate", "$'Location of Navigational Bodies'.$C$1"),
        ("LocalTime", "$'Location of Navigational Bodies'.$C$2"),
        ("LocalZone", "$'Location of Navigational Bodies'.$F$1"),
        ("Longitude", "$'Location of Navigational Bodies'.$F$3"),
        ("LunarFundamentals", "$'Moon Data'.$A$3:.$E$7"),
        ("LunarLat", "$'Moon Data'.$N$3:.$R$62"),
        ("LunarLonRad", "$'Moon Data'.$G$3:.$L$62"),
        ("Mars2ndOrderPert", "$'Mars Data'.$O$3:.$AA$20"),
        ("MarsPeriodicPert", "$'Mars Data'.$A$3:.$M$78"),
        ("StTable", "$'Star Data'.$A$2:.$J$60"),
    ]
    for table_index in range(1, len(planet_windows()) + 1):
        start_row = PLANET_TABLE_START_ROW + (table_index - 1) * PLANET_TABLE_STRIDE
        end_row = start_row + PLANET_TABLE_ROW_COUNT - 1
        named_ranges.append(
            (f"PlTable{table_index}", f"$'Planet Data'.$A${start_row}:.$H${end_row}")
        )
    named_expressions = ET.SubElement(spreadsheet, f"{{{table_uri}}}named-expressions")
    for name, range_address in named_ranges:
        named_range = ET.SubElement(named_expressions, f"{{{table_uri}}}named-range")
        named_range.attrib[f"{{{table_uri}}}name"] = name
        named_range.attrib[f"{{{table_uri}}}base-cell-address"] = "$'Star Data'.$A$1"
        named_range.attrib[f"{{{table_uri}}}cell-range-address"] = range_address

    database_ranges = ET.SubElement(spreadsheet, f"{{{table_uri}}}database-ranges")
    nav_filter = ET.SubElement(database_ranges, f"{{{table_uri}}}database-range")
    nav_filter.attrib[f"{{{table_uri}}}name"] = "NavigationBodiesFilter"
    nav_filter.attrib[f"{{{table_uri}}}target-range-address"] = "$'Location of Navigational Bodies'.$A$5:.$I$72"
    nav_filter.attrib[f"{{{table_uri}}}display-filter-buttons"] = "true"
    nav_filter.attrib[f"{{{table_uri}}}contains-header"] = "true"


def formula_cell(formula: str, value_type: str = "string") -> ET.Element:
    cell = ET.Element(f"{{{ODS_NS['table']}}}table-cell")
    cell.attrib[f"{{{ODS_NS['table']}}}formula"] = formula
    cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = value_type
    if value_type == "float":
        cell.attrib[f"{{{ODS_NS['office']}}}value"] = "0"
    ET.SubElement(cell, f"{{{ODS_NS['text']}}}p")
    return cell


def matrix_formula_cell(formula: str, rows: int, cols: int) -> ET.Element:
    cell = formula_cell(formula, "string")
    cell.attrib[f"{{{ODS_NS['table']}}}number-matrix-rows-spanned"] = str(rows)
    cell.attrib[f"{{{ODS_NS['table']}}}number-matrix-columns-spanned"] = str(cols)
    return cell


def covered_matrix_cell(rows: int, cols: int, repeat: int = 1) -> ET.Element:
    cell = ET.Element(f"{{{ODS_NS['table']}}}covered-table-cell")
    cell.attrib[f"{{{ODS_NS['table']}}}number-matrix-rows-spanned"] = str(rows)
    cell.attrib[f"{{{ODS_NS['table']}}}number-matrix-columns-spanned"] = str(cols)
    if repeat > 1:
        cell.attrib[f"{{{ODS_NS['table']}}}number-columns-repeated"] = str(repeat)
    return cell


def row_cell_map(row: ET.Element) -> dict[int, ET.Element]:
    repeat_attr = f"{{{ODS_NS['table']}}}number-columns-repeated"
    cells: dict[int, ET.Element] = {}
    logical_col = 1
    for cell in row:
        if cell.tag not in {
            f"{{{ODS_NS['table']}}}table-cell",
            f"{{{ODS_NS['table']}}}covered-table-cell",
        }:
            continue
        repeat = int(cell.attrib.get(repeat_attr, "1"))
        if cell_has_content(cell):
            cells[logical_col] = cell
        logical_col += repeat
    return cells


def replace_row_cells(row: ET.Element, cells: dict[int, ET.Element]) -> None:
    repeat_attr = f"{{{ODS_NS['table']}}}number-columns-repeated"
    table_cell_tag = f"{{{ODS_NS['table']}}}table-cell"
    covered_cell_tag = f"{{{ODS_NS['table']}}}covered-table-cell"

    for child in list(row):
        if child.tag in {table_cell_tag, covered_cell_tag}:
            row.remove(child)

    current_col = 1
    for logical_col in sorted(cells):
        gap = logical_col - current_col
        if gap > 0:
            empty = ET.SubElement(row, table_cell_tag)
            if gap > 1:
                empty.attrib[repeat_attr] = str(gap)
        row.append(cells[logical_col])
        current_col = logical_col + 1


def add_navigation_formulas(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()
    repeat_attr = f"{{{ODS_NS['table']}}}number-columns-repeated"

    for table in root.findall(".//table:table", ODS_NS):
        if table.attrib.get(f"{{{ODS_NS['table']}}}name") != "Location of Navigational Bodies":
            continue

        rows = table.findall("table:table-row", ODS_NS)
        if len(rows) < 72:
            break

        for row_number in range(6, 73):
            row = row_cell_map(row_at(rows, row_number))
            if row_number == 7:
                row[2] = matrix_formula_cell(
                    "of:=MOONPOSITIONDATECALC([.$C$1];[.$C$2];[.$F$1];[.$F$2];[.$F$3])",
                    1,
                    7,
                )
            elif row_number == 72:
                row[4] = formula_cell("of:=GHAARIESTEXTDATECALC([.$C$1];[.$C$2];[.$F$1])")
            elif row_number < 13:
                row[2] = matrix_formula_cell(
                    f"of:=PLANETPOSITIONDATECALC([.A{row_number}];[.$C$1];[.$C$2];[.$F$1];[.$F$2];[.$F$3])",
                    1,
                    7,
                )
            else:
                row[2] = matrix_formula_cell(
                    f"of:=STARPOSITIONBYNAMEDATECALC([.A{row_number}];[.$C$1];[.$C$2];[.$F$1];[.$F$2];[.$F$3])",
                    1,
                    7,
                )

            if row_number == 72:
                row[9] = formula_cell('of:=""')
            elif row_number in {6, 7}:
                row[9] = formula_cell(f'of:=IFERROR(IF(MID([.E{row_number}];1;1)="-";"NO";"YES");"NO")')
            else:
                row[9] = formula_cell(f'of:=IFERROR(IF(OR(MID([.E{row_number}];1;1)="-";[.$I$6]="YES");"NO";"YES");"NO")')
            replace_row_cells(row_at(rows, row_number), row)

        for col in table.findall("table:table-column", ODS_NS):
            col.attrib.pop(repeat_attr, None)
        table_columns = table.findall("table:table-column", ODS_NS)
        if table_columns:
            table_columns[0].attrib[repeat_attr] = "9"
        break

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def qname(namespace_uri: str, local_name: str) -> str:
    return f"{{{namespace_uri}}}{local_name}"


def clear_cell_style(cell: ET.Element) -> None:
    cell.attrib.pop(qname(ODS_NS["table"], "style-name"), None)


def set_cell_style(cell: ET.Element, style_name: str) -> None:
    if cell.tag == qname(ODS_NS["table"], "table-cell"):
        cell.attrib[qname(ODS_NS["table"], "style-name")] = style_name


def styled_empty_cell(style_name: str) -> ET.Element:
    cell = ET.Element(qname(ODS_NS["table"], "table-cell"))
    set_cell_style(cell, style_name)
    return cell


def replace_row_with_styled_cells(row: ET.Element, cells: dict[int, ET.Element], max_cols: int, default_style: str) -> None:
    table_cell_tag = qname(ODS_NS["table"], "table-cell")
    covered_cell_tag = qname(ODS_NS["table"], "covered-table-cell")
    for child in list(row):
        if child.tag in {table_cell_tag, covered_cell_tag}:
            row.remove(child)
    for col_index in range(1, max_cols + 1):
        cell = cells.get(col_index)
        if cell is None:
            cell = styled_empty_cell(default_style)
        elif cell.tag == table_cell_tag and qname(ODS_NS["table"], "style-name") not in cell.attrib:
            set_cell_style(cell, default_style)
        row.append(cell)


def add_table_cell_style(
    automatic_styles: ET.Element,
    name: str,
    background: str,
    text_color: str,
    *,
    bold: bool = False,
    font_size: str = "10pt",
    border: str = "0.5pt solid #40251e",
    align: str | None = None,
) -> None:
    style_uri = CONTENT_NAMESPACE_DECLS["style"]
    fo_uri = CONTENT_NAMESPACE_DECLS["fo"]
    style = ET.SubElement(automatic_styles, qname(style_uri, "style"))
    style.attrib[qname(style_uri, "name")] = name
    style.attrib[qname(style_uri, "family")] = "table-cell"

    cell_props = ET.SubElement(style, qname(style_uri, "table-cell-properties"))
    cell_props.attrib[qname(fo_uri, "background-color")] = background
    cell_props.attrib[qname(fo_uri, "border")] = border
    cell_props.attrib[qname(fo_uri, "padding")] = "0.035in"
    if align:
        para_props = ET.SubElement(style, qname(style_uri, "paragraph-properties"))
        para_props.attrib[qname(fo_uri, "text-align")] = align

    text_props = ET.SubElement(style, qname(style_uri, "text-properties"))
    text_props.attrib[qname(fo_uri, "color")] = text_color
    text_props.attrib[qname(fo_uri, "font-size")] = font_size
    if bold:
        text_props.attrib[qname(fo_uri, "font-weight")] = "bold"


def add_table_column_style(automatic_styles: ET.Element, name: str, width: str) -> None:
    style_uri = CONTENT_NAMESPACE_DECLS["style"]
    style = ET.SubElement(automatic_styles, qname(style_uri, "style"))
    style.attrib[qname(style_uri, "name")] = name
    style.attrib[qname(style_uri, "family")] = "table-column"
    props = ET.SubElement(style, qname(style_uri, "table-column-properties"))
    props.attrib[qname(style_uri, "column-width")] = width


def ensure_mars_lab_styles(root: ET.Element) -> None:
    office_uri = ODS_NS["office"]
    style_uri = CONTENT_NAMESPACE_DECLS["style"]
    automatic_styles = root.find("office:automatic-styles", ODS_NS)
    if automatic_styles is None:
        automatic_styles = ET.Element(qname(office_uri, "automatic-styles"))
        body = root.find("office:body", ODS_NS)
        if body is not None:
            root.insert(list(root).index(body), automatic_styles)
        else:
            root.insert(0, automatic_styles)

    for child in list(automatic_styles):
        if child.attrib.get(qname(style_uri, "name"), "").startswith("ML_"):
            automatic_styles.remove(child)

    add_table_cell_style(automatic_styles, "ML_Console", "#141312", "#f2e4c8", border="0.5pt solid #30231f")
    add_table_cell_style(automatic_styles, "ML_Canvas", "#12100f", "#d8c9af", border="0.5pt solid #27211d")
    add_table_cell_style(automatic_styles, "ML_Title", "#5a2418", "#fff1d5", bold=True, font_size="11pt", border="0.75pt solid #8f4b2f")
    add_table_cell_style(automatic_styles, "ML_InputLabel", "#2b2521", "#f0c987", bold=True)
    add_table_cell_style(automatic_styles, "ML_InputValue", "#f1c37a", "#21120d", bold=True, border="0.75pt solid #b96832")
    add_table_cell_style(automatic_styles, "ML_Header", "#873d25", "#fff3dc", bold=True, border="0.75pt solid #b15d36", align="center")
    add_table_cell_style(automatic_styles, "ML_BodyOdd", "#191615", "#f4ead7", border="0.5pt solid #302823")
    add_table_cell_style(automatic_styles, "ML_BodyEven", "#1f1a17", "#f4ead7", border="0.5pt solid #332923")
    add_table_cell_style(automatic_styles, "ML_Planet", "#2f211b", "#ffe4b5", border="0.5pt solid #5a3729")
    add_table_cell_style(automatic_styles, "ML_Visible", "#19332b", "#c7ffd9", bold=True, border="0.5pt solid #356451", align="center")
    add_table_cell_style(automatic_styles, "ML_DataHeader", "#5f2d1e", "#ffe5a9", bold=True, border="0.75pt solid #8e4f32")
    add_table_cell_style(automatic_styles, "ML_DataOdd", "#171514", "#eadcc8", border="0.5pt solid #2e2823")
    add_table_cell_style(automatic_styles, "ML_DataEven", "#1b1715", "#eadcc8", border="0.5pt solid #332a23")

    for index, width in enumerate(["2.2cm", "2.5cm", "2.7cm", "2.6cm", "2.8cm", "2.8cm", "2.1cm", "2.1cm", "2.4cm"], start=1):
        add_table_column_style(automatic_styles, f"ML_NavCol{index}", width)
    add_table_column_style(automatic_styles, "ML_CanvasCol", "2.3cm")
    add_table_column_style(automatic_styles, "ML_DataCol", "2.4cm")


def apply_mars_lab_theme(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()
    table_uri = ODS_NS["table"]
    repeat_attr = qname(table_uri, "number-columns-repeated")
    style_attr = qname(table_uri, "style-name")

    ensure_mars_lab_styles(root)
    visible_cols = 21

    for table in root.findall(".//table:table", ODS_NS):
        sheet_name = table.attrib.get(qname(table_uri, "name"), "")
        columns = table.findall("table:table-column", ODS_NS)
        if sheet_name == "Location of Navigational Bodies":
            for col in list(columns):
                table.remove(col)
            for index in range(1, visible_cols + 1):
                col = ET.Element(qname(table_uri, "table-column"))
                col.attrib[style_attr] = f"ML_NavCol{index}" if index <= 9 else "ML_CanvasCol"
                table.insert(index - 1, col)

            rows = table.findall("table:table-row", ODS_NS)
            for row_number, row in enumerate(rows, start=1):
                cells = row_cell_map(row)
                for cell in cells.values():
                    clear_cell_style(cell)
                if row_number in {1, 2, 3}:
                    for col_index in (2, 5):
                        if col_index in cells:
                            set_cell_style(cells[col_index], "ML_InputLabel")
                    for col_index in (3, 6):
                        if col_index in cells:
                            set_cell_style(cells[col_index], "ML_InputValue")
                    replace_row_with_styled_cells(row, cells, visible_cols, "ML_Canvas")
                    continue
                if row_number == 6:
                    for col_index, cell in cells.items():
                        set_cell_style(cell, "ML_Header" if col_index <= 9 else "ML_Canvas")
                    replace_row_with_styled_cells(row, cells, visible_cols, "ML_Canvas")
                    continue
                if 7 <= row_number <= 13:
                    for col_index, cell in cells.items():
                        if col_index == 9:
                            set_cell_style(cell, "ML_Visible")
                        elif col_index <= 8:
                            set_cell_style(cell, "ML_Planet")
                        else:
                            set_cell_style(cell, "ML_Canvas")
                    replace_row_with_styled_cells(row, cells, visible_cols, "ML_Canvas")
                    continue
                if 14 <= row_number <= 73:
                    row_style = "ML_BodyEven" if row_number % 2 == 0 else "ML_BodyOdd"
                    for col_index, cell in cells.items():
                        if col_index == 9:
                            set_cell_style(cell, "ML_Visible")
                        elif col_index <= 8:
                            set_cell_style(cell, row_style)
                        else:
                            set_cell_style(cell, "ML_Canvas")
                    replace_row_with_styled_cells(row, cells, visible_cols, "ML_Canvas")
                    continue
                for cell in cells.values():
                    set_cell_style(cell, "ML_Console")
                replace_row_with_styled_cells(row, cells, visible_cols, "ML_Canvas")
        elif sheet_name in REFERENCE_DATA_SHEETS or sheet_name in {"Earth State", "Moon State"}:
            for col in list(columns):
                table.remove(col)
            for index in range(1, visible_cols + 1):
                col = ET.Element(qname(table_uri, "table-column"))
                col.attrib[style_attr] = "ML_DataCol"
                table.insert(index - 1, col)
            for row_number, row in enumerate(table.findall("table:table-row", ODS_NS), start=1):
                cells = row_cell_map(row)
                style = "ML_DataHeader" if row_number in {1, 2, 4, 24, 26} else ("ML_DataEven" if row_number % 2 == 0 else "ML_DataOdd")
                for cell in cells.values():
                    set_cell_style(cell, style)
                replace_row_with_styled_cells(row, cells, visible_cols, "ML_Canvas")
        elif sheet_name == "Help":
            for col in list(columns):
                table.remove(col)
            for index in range(1, visible_cols + 1):
                col = ET.Element(qname(table_uri, "table-column"))
                col.attrib[style_attr] = "ML_DataCol" if index <= 6 else "ML_CanvasCol"
                table.insert(index - 1, col)
            for row_number, row in enumerate(table.findall("table:table-row", ODS_NS), start=1):
                cells = row_cell_map(row)
                first_text = cell_text_value(cells.get(1)) if 1 in cells else ""
                if row_number == 1:
                    style = "ML_Title"
                elif first_text in {
                    "Year",
                    "Navigation accident statistics",
                    "Island daydreams for the passage plan",
                    "Ocean",
                }:
                    style = "ML_Header"
                else:
                    style = "ML_DataEven" if row_number % 2 == 0 else "ML_DataOdd"
                for cell in cells.values():
                    set_cell_style(cell, style)
                replace_row_with_styled_cells(row, cells, visible_cols, "ML_Canvas")

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def protect_support_sheets(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()
    table_uri = ODS_NS["table"]
    protected_attr = qname(table_uri, "protected")
    for table in root.findall(".//table:table", ODS_NS):
        sheet_name = table.attrib.get(qname(table_uri, "name"), "")
        if sheet_name in PROTECTED_SHEETS:
            table.attrib[protected_attr] = "true"
        else:
            table.attrib.pop(protected_attr, None)
    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def write_minimal_sidecars(tmpdir: Path) -> None:
    (tmpdir / "styles.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<office:document-styles xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
        'office:version="1.2"/>\n',
        encoding="utf-8",
    )
    (tmpdir / "settings.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<office:document-settings xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
        'office:version="1.2"/>\n',
        encoding="utf-8",
    )
    (tmpdir / "meta.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<office:document-meta xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
        'office:version="1.2"/>\n',
        encoding="utf-8",
    )


def patch_body_location(text: str) -> str:
    anchor = "' Planetary orbital parameters."
    helper_start = "' LibreOffice-compatible named range helpers."
    helper_end = "' Planetary orbital parameters."
    if helper_start in text and helper_end in text:
        start_idx = text.index(helper_start)
        end_idx = text.index(helper_end)
        text = text[:start_idx] + BODY_HELPERS + "\n" + text[end_idx:]
    elif "GetNamedRange" not in text:
        text = text.replace(anchor, BODY_HELPERS + "\n" + anchor, 1)

    if "Public Type Vector" not in text:
        if anchor not in text:
            raise RuntimeError("could not locate vector support insertion point")
        text = text.replace(anchor, VECTOR_SUPPORT + "\n\n" + anchor, 1)

    replacements = {
        'Application.Names("PlTable" & i).RefersToRange.Value2': 'NamedRangeData("PlTable" & i)',
        'Application.Names("StTable").RefersToRange.Value2': 'NamedRangeData("StTable")',
        'Application.Names("LunarFundamentals").RefersToRange.Value2': 'NamedRangeData("LunarFundamentals")',
        'Application.Names("LunarLonRad").RefersToRange.Value2': 'NamedRangeData("LunarLonRad")',
        'Application.Names("LunarLat").RefersToRange.Value2': 'NamedRangeData("LunarLat")',
        'Application.Names("Mars2ndOrderPert").RefersToRange.Value2': 'NamedRangeData("Mars2ndOrderPert")',
        'Application.Names("MarsPeriodicPert").RefersToRange.Value2': 'NamedRangeData("MarsPeriodicPert")',
    }
    for old, new in replacements.items():
        text = text.replace(old, new)

    # Remove an experimental C-style apparent-direction path if it was embedded in a
    # previous repair pass. The workbook's native precession/nutation staging is
    # closer to the SPICE+IAU reference for apparent RA/Dec of date.
    for helper_name in (
        "MeanObliquityRadians",
        "NutationAngles",
        "ApparentDirectionFromEcliptic",
    ):
        text = re.sub(
            rf"\nPrivate (?:Function|Sub) {helper_name}\b.*?\nEnd (?:Function|Sub)\n",
            "\n",
            text,
            count=1,
            flags=re.DOTALL,
        )

    canonical_get_geocentric_polars = """Private Sub GetGeocentricPolars(ByVal Emag As Double, ByRef Ehat As Vector, ByRef Phat As Vector, ByRef Qhat As Vector, ByRef Edot As Vector, _
                                ByRef elemEarth As OrbitalElements, ByRef RA As Double, ByRef decl As Double, ByRef GHA As Double)

   ' Gravitational Deflection
   Dim P1 As Vector
   P1 = vPlus(Phat, vDiv(vMul(vCross(vCross(Qhat, Ehat), Phat), Emag * k2Mu_C2), 1 + vDot(Qhat, Ehat)))
   
   ' Aberration
   Dim P2 As Vector, V As Vector, Beta As Double
   V = vDiv(Edot, kC)
   Beta = 1 / Sqr(1 - vDot(V, V))
   P2 = vDiv(vPlus(vDiv(P1, Beta), vMul(V, 1 + vDot(P1, V) / (1 + 1 / Beta))), 1 + vDot(P1, V))
   
   ' Transform to Geocentric
   Dim P3 As Vector
   P3 = ConvertHeliocentricToGeocentric(elemEarth, P2)
   
   ' Get Declination and Right Ascension
   RA = Modulo(ATan2(P3.X(2), P3.X(1)) * kToDeg, 360)
   decl = ASin(P3.X(3)) * kToDeg
   GHA = Modulo(elemEarth.GHA_aries - RA, 360)

End Sub"""
    text = re.sub(
        r"Private Sub GetGeocentricPolars\b.*?\nEnd Sub",
        canonical_get_geocentric_polars,
        text,
        count=1,
        flags=re.DOTALL,
    )
    text = text.replace("Round(", "RoundTo(")
    text = text.replace(
        ".meanLon = CubicEval(P, PlanetTable(r0, kMEAN_LON), PlanetTable(r1, kMEAN_LON), PlanetTable(r2, kMEAN_LON))",
        ".meanLon = Modulo(CubicEval(P, PlanetTable(r0, kMEAN_LON), PlanetTable(r1, kMEAN_LON), PlanetTable(r2, kMEAN_LON)), 360)",
    )
    text = text.replace(
        ".lonPerihel = CubicEval(P, PlanetTable(r0, kLON_PERIHEL), PlanetTable(r1, kLON_PERIHEL), PlanetTable(r2, kLON_PERIHEL))",
        ".lonPerihel = Modulo(CubicEval(P, PlanetTable(r0, kLON_PERIHEL), PlanetTable(r1, kLON_PERIHEL), PlanetTable(r2, kLON_PERIHEL)), 360)",
    )
    text = text.replace(
        ".ascend = CubicEval(P, PlanetTable(r0, kASCEND), PlanetTable(r1, kASCEND), PlanetTable(r2, kASCEND))",
        ".ascend = Modulo(CubicEval(P, PlanetTable(r0, kASCEND), PlanetTable(r1, kASCEND), PlanetTable(r2, kASCEND)), 360)",
    )
    text = text.replace(
        ".meanAnomaly = .meanLon - .lonPerihel",
        ".meanAnomaly = Modulo(.meanLon - .lonPerihel, 360)",
    )
    text = text.replace(
        ".argPerihel = .lonPerihel - .ascend",
        ".argPerihel = Modulo(.lonPerihel - .ascend, 360)",
    )
    text = text.replace(
        ".eccAnomaly = SolveKeplersEquation(.meanAnomaly, .eccent)",
        ".eccAnomaly = Modulo(SolveKeplersEquation(.meanAnomaly, .eccent), 360)",
    )

    window_cases = "\n".join(
        f"      Case {index}\n         JDo = {start_jd:.8f}: JDn = {end_jd:.8f}"
        for index, (start_jd, end_jd) in enumerate(planet_windows(), start=1)
    )
    cached_fetch = f"""Private Sub PlanetTableBounds(ByVal tableIndex As Integer, ByRef JDo As Double, ByRef JDn As Double)
   Select Case tableIndex
{window_cases}
      Case Else
         JDo = 0#: JDn = 0#
   End Select
End Sub

' ------------------------------------------------------------------------------------------------
Private Function FetchPlanetTable(ByVal JD As Double) As Variant
   Static cachedTable As Variant
   Static cachedJDo As Double
   Static cachedJDn As Double
   Static hasCachedTable As Boolean

   If hasCachedTable Then
      If cachedJDo <= JD And JD <= cachedJDn Then
         FetchPlanetTable = cachedTable
         Exit Function
      End If
   End If

   Dim PlanetTable As Variant
   Dim JDo As Double, JDn As Double

   Dim i As Integer
   For i = 1 To kPLANET_TABLE_COUNT
      PlanetTableBounds i, JDo, JDn
      If JDo <= JD And JD <= JDn Then
         PlanetTable = NamedRangeData("PlTable" & i)
         cachedTable = PlanetTable
         cachedJDo = JDo
         cachedJDn = JDn
         hasCachedTable = True
         FetchPlanetTable = PlanetTable
         Exit Function
      End If
   Next i

   Err.Raise 5, "FetchPlanetTable", "No planet table covers JD " & CStr(JD)
End Function"""
    text = re.sub(
        r"(?:Private Sub PlanetTableBounds\(ByVal tableIndex As Integer, ByRef JDo As Double, ByRef JDn As Double\).*?End Sub\s*'\s*-+\s*)?Private Function FetchPlanetTable\(ByVal JD As Double\) As Variant.*?End Function",
        cached_fetch,
        text,
        count=1,
        flags=re.DOTALL,
    )
    earth_state = load_earth_state_model()
    moon_state = load_moon_state_model()
    earth_cases = "\n".join(
        f"      Case {row['index']}\n         JDo = {float(row['start_jd']):.8f}: JDn = {float(row['end_jd']):.8f}"
        for row in earth_state["rows"]
    )
    earth_state_helpers = f"""Private Sub EarthStateBounds(ByVal tableIndex As Integer, ByRef JDo As Double, ByRef JDn As Double)
   Select Case tableIndex
{earth_cases}
      Case Else
         JDo = 0#: JDn = 0#
   End Select
End Sub

' ------------------------------------------------------------------------------------------------
Private Function SheetRangeData(ByVal sheetName As String, ByVal startRow As Long, ByVal startColumn As Long, ByVal rowCount As Long, ByVal colCount As Long) As Variant
   Dim oSheet As Object
   Dim oCell As Object
   Dim rowIndex As Long, colIndex As Long
   Dim outData() As Variant

   Set oSheet = ThisComponent.Sheets.getByName(sheetName)
   ReDim outData(1 To rowCount, 1 To colCount)

   For rowIndex = 1 To rowCount
      For colIndex = 1 To colCount
         Set oCell = oSheet.getCellByPosition(startColumn + colIndex - 2, startRow + rowIndex - 2)
         outData(rowIndex, colIndex) = oCell.Value
      Next colIndex
   Next rowIndex

   SheetRangeData = outData
End Function

' ------------------------------------------------------------------------------------------------
Private Function FetchMoonState(ByVal JD As Double) As Variant
   Static cachedTable As Variant
   Static cachedJDo As Double
   Static cachedJDn As Double
   Static hasCachedTable As Boolean

   If hasCachedTable Then
      If cachedJDo <= JD And JD <= cachedJDn Then
         FetchMoonState = cachedTable
         Exit Function
      End If
   End If

   Dim tableIndex As Long
   tableIndex = Int((JD - {float(moon_state["start_jd"]):.8f}) / {float(moon_state["segment_span_days"]):.8f}) + 1
   If tableIndex < 1 Or tableIndex > {len(moon_state["rows"])} Then
      Err.Raise 5, "FetchMoonState", "No Moon state table covers JD " & CStr(JD)
   End If

   Dim JDo As Double, JDn As Double
   JDo = {float(moon_state["start_jd"]):.8f} + (tableIndex - 1) * {float(moon_state["segment_span_days"]):.8f}
   If tableIndex = {len(moon_state["rows"])} Then
      JDn = {float(moon_state["end_jd"]):.8f}
   Else
      JDn = JDo + {float(moon_state["segment_span_days"]):.8f}
   End If
   If JD < JDo Or JD > JDn Then
      Err.Raise 5, "FetchMoonState", "No Moon state table covers JD " & CStr(JD)
   End If

   Dim StateTable As Variant
   StateTable = SheetRangeData("Moon State", {MOON_STATE_TABLE_START_ROW} + (tableIndex - 1) * {MOON_STATE_TABLE_STRIDE}, 1, {MOON_STATE_TABLE_ROW_COUNT}, 8)
   cachedTable = StateTable
   cachedJDo = JDo
   cachedJDn = JDn
   hasCachedTable = True
   FetchMoonState = StateTable
End Function

' ------------------------------------------------------------------------------------------------
Private Function FetchEarthState(ByVal JD As Double) As Variant
   Static cachedTable As Variant
   Static cachedJDo As Double
   Static cachedJDn As Double
   Static hasCachedTable As Boolean

   If hasCachedTable Then
      If cachedJDo <= JD And JD <= cachedJDn Then
         FetchEarthState = cachedTable
         Exit Function
      End If
   End If

   Dim StateTable As Variant
   Dim JDo As Double, JDn As Double
   Dim i As Integer

   For i = 1 To {len(earth_state["rows"])}
      EarthStateBounds i, JDo, JDn
      If JDo <= JD And JD <= JDn Then
         StateTable = SheetRangeData("Earth State", {EARTH_STATE_TABLE_START_ROW} + (i - 1) * {EARTH_STATE_TABLE_STRIDE}, 1, {EARTH_STATE_TABLE_ROW_COUNT}, 8)
         cachedTable = StateTable
         cachedJDo = JDo
         cachedJDn = JDn
         hasCachedTable = True
         FetchEarthState = StateTable
         Exit Function
      End If
   Next i

   Err.Raise 5, "FetchEarthState", "No Earth state table covers JD " & CStr(JD)
End Function

' ------------------------------------------------------------------------------------------------
Private Sub GetEarthHeliocentricPos(ByRef elemEarth As OrbitalElements, ByRef pos As Vector, ByRef vel As Vector)
   On Error GoTo OrbitalFallback

   Dim StateTable As Variant
   Dim X As Double
   Dim component As Integer
   Dim coeffIndex As Integer
   Dim value As Double

   StateTable = FetchEarthState(elemEarth.JD)
   X = (elemEarth.JD - CDbl(StateTable(1, 6))) / CDbl(StateTable(1, 8))

   For component = 1 To 6
      value = 0#
      For coeffIndex = {int(earth_state["degree"])} To 0 Step -1
         value = value * X + CDbl(StateTable(coeffIndex + 3, component + 1))
      Next coeffIndex
      If component <= 3 Then
         pos.X(component) = value
      Else
         vel.X(component - 3) = value
      End If
   Next component

   Exit Sub

OrbitalFallback:
   ' Keep the workbook functional if LibreOffice cannot read the correction sheet.
   GetPlanetHeliocentricPos elemEarth, pos, vel
End Sub"""
    for state_helper in (
        "EarthStateBounds",
        "SheetRangeData",
        "FetchMoonState",
        "FetchEarthState",
        "GetEarthHeliocentricPos",
    ):
        text = re.sub(
            rf"\n'\s*-+\nPrivate (?:Sub|Function) {state_helper}\b.*?\nEnd (?:Sub|Function)\n",
            "\n",
            text,
            flags=re.DOTALL,
        )

    planet_position_anchor = "Public Function PlanetPosition("
    anchor_index = text.find(planet_position_anchor)
    if anchor_index == -1:
        raise RuntimeError("could not locate state helper insertion point")
    text = text[:anchor_index] + earth_state_helpers + "\n\n" + text[anchor_index:]
    text = text.replace(
        "Public Function PlanetPosition(ByVal Planet As String, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double) As Variant",
        "Public Function PlanetPosition(ByVal Planet As String, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant",
    )
    text = text.replace(
        "Public Function MoonPosition(ByVal JD As Double, ByVal lat As Double, ByVal lon As Double) As Variant",
        "Public Function MoonPosition(ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant",
    )
    text = text.replace(
        "Public Function StarPosition(ByVal rowStar As Integer, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double) As Variant",
        "Public Function StarPosition(ByVal rowStar As Integer, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant",
    )
    text = text.replace(
        "Public Function PlanetPosition(ByVal Planet As String, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant\n   Dim PlanetTable As Variant",
        "Public Function PlanetPosition(ByVal Planet As String, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant\n   If IsMissing(civilJD) Or IsNull(civilJD) Then civilJD = JD\n\n   Dim PlanetTable As Variant",
    )
    text = text.replace(
        "Public Function MoonPosition(ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant\n   Dim lunarFundamentals As Variant",
        "Public Function MoonPosition(ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant\n   If IsMissing(civilJD) Or IsNull(civilJD) Then civilJD = JD\n\n   Dim lunarFundamentals As Variant",
    )
    text = text.replace(
        "Public Function StarPosition(ByVal rowStar As Integer, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant\n   \n   ' Earth",
        "Public Function StarPosition(ByVal rowStar As Integer, ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant\n   If IsMissing(civilJD) Or IsNull(civilJD) Then civilJD = JD\n   \n   ' Earth",
    )
    moon_position = f"""Public Function MoonPosition(ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, Optional ByVal civilJD As Variant) As Variant
   If IsMissing(civilJD) Or IsNull(civilJD) Then civilJD = JD
   On Error GoTo LegacyFallback

   Dim StateTable As Variant
   Dim X As Double
   Dim component As Long
   Dim coeffIndex As Long
   Dim value As Double
   Dim moonValue(1 To 5) As Double

   StateTable = FetchMoonState(JD)
   X = (JD - CDbl(StateTable(1, 6))) / CDbl(StateTable(1, 8))

   For component = 1 To 5
      value = 0#
      For coeffIndex = {int(moon_state["degree"])} To 0 Step -1
         value = value * X + CDbl(StateTable(coeffIndex + 3, component + 1))
      Next coeffIndex
      moonValue(component) = value
   Next component

   Dim posMoon As Vector
   posMoon.X(1) = moonValue(1)
   posMoon.X(2) = moonValue(2)
   posMoon.X(3) = moonValue(3)
   posMoon = vUnit(posMoon)

   Dim RA As Double, Dec As Double, GHA As Double
   RA = Modulo(ATan2(posMoon.X(2), posMoon.X(1)) * kToDeg, 360)
   Dec = ASin(posMoon.X(3)) * kToDeg
   GHA = Modulo(GHAAries(CDbl(civilJD)) - RA, 360)

   Dim MRad As Double
   MRad = moonValue(4) / kKmToAu

   Dim alt As Double, azim As Double
   GetBodySighting Dec, GHA, MRad, lat, lon, alt, azim

   Dim SD As Double
   SD = 0.272493 * kRadiusEarth / MRad
   SD = ASin(SD) * kToDeg

   Dim MoonAttrs(kANS_DECL To kANS_VMAG) As Variant
   MoonAttrs(kANS_DECL) = DegMinSec(Dec, "S", "N")
   MoonAttrs(kANS_RA) = DegMinSec(RA)
   MoonAttrs(kANS_GHA) = DegMinSec(GHA)
   MoonAttrs(kANS_ALT) = DegMinSec(RefrCorr(alt, 10, 1010))
   MoonAttrs(kANS_AZIM) = DegMinSec(azim)
   MoonAttrs(kANS_SD) = MinSec(SD * 60)
   MoonAttrs(kANS_VMAG) = MoonMagnitude(Modulo(moonValue(5), 360), JD) & " "

   MoonPosition = MoonAttrs
   Exit Function

LegacyFallback:
   On Error GoTo 0
   MoonPosition = LegacyMoonPosition(JD, lat, lon, civilJD)
End Function"""
    moon_function_pattern = (
        r"Public Function MoonPosition\(ByVal JD As Double, ByVal lat As Double, ByVal lon As Double, "
        r"Optional ByVal civilJD As Variant\) As Variant.*?\nEnd Function"
    )
    if "Private Function LegacyMoonPosition" in text:
        text = re.sub(moon_function_pattern, moon_position, text, count=1, flags=re.DOTALL)
    else:
        moon_match = re.search(moon_function_pattern, text, flags=re.DOTALL)
        if moon_match is None:
            raise RuntimeError("could not locate MoonPosition for replacement")
        legacy_moon = moon_match.group(0)
        legacy_moon = legacy_moon.replace("Public Function MoonPosition(", "Private Function LegacyMoonPosition(", 1)
        legacy_moon = legacy_moon.replace("MoonPosition = MoonAttrs", "LegacyMoonPosition = MoonAttrs")
        text = text[: moon_match.start()] + moon_position + "\n\n" + legacy_moon + text[moon_match.end() :]

    text = text.replace(
        "   ' Get Declination, Right Ascension and Greenwich Hour Angle\n   Dim RA As Double, decl As Double, GHA As Double\n   GetGeocentricPolars Emag, Ehat, Phat, Qhat, velEarth, elemEarth, RA, decl, GHA",
        "   ' Get Declination, Right Ascension and Greenwich Hour Angle\n   elemEarth.JD = CDbl(civilJD)\n   Dim RA As Double, decl As Double, GHA As Double\n   GetGeocentricPolars Emag, Ehat, Phat, Qhat, velEarth, elemEarth, RA, decl, GHA",
        1,
    )
    text = text.replace(
        "   ' Compute GHA aries\n   Dim GHA_aries As Double\n   GHA_aries = GHAAries(JD)",
        "   ' Compute GHA aries\n   Dim GHA_aries As Double\n   GHA_aries = GHAAries(CDbl(civilJD))",
    )
    text = text.replace(
        "   ' Get Declination, Right Ascension and Greenwich Hour Angle\n   Dim RA As Double, decl As Double, GHA As Double\n   GetGeocentricPolars Emag, Ehat, Phat, Phat, velEarth, elemEarth, RA, decl, GHA",
        "   ' Get Declination, Right Ascension and Greenwich Hour Angle\n   elemEarth.JD = CDbl(civilJD)\n   Dim RA As Double, decl As Double, GHA As Double\n   GetGeocentricPolars Emag, Ehat, Phat, Phat, velEarth, elemEarth, RA, decl, GHA",
    )
    text = re.sub(
        r"\n' -+\nPublic Function PlanetDebugValue\b.*?\nEnd Function\n",
        "\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    if False and "Public Function PlanetDebugValue(" not in text:
        text += """

' ------------------------------------------------------------------------------------------------
Public Function PlanetDebugValue(ByVal Planet As String, ByVal JD As Double, ByVal fieldName As String) As Variant
   Dim PlanetTable As Variant
   Dim rowPlanet As Integer
   Dim elemPlanet As OrbitalElements

   PlanetTable = FetchPlanetTable(JD)
   Select Case Planet
      Case "Earth"
         rowPlanet = kROW_EARTH
      Case "Mercury"
         rowPlanet = kROW_MERCURY
      Case "Venus"
         rowPlanet = kROW_VENUS
      Case "Mars"
         rowPlanet = kROW_MARS
      Case "Jupiter"
         rowPlanet = kROW_JUPITER
      Case "Saturn"
         rowPlanet = kROW_SATURN
      Case Else
         PlanetDebugValue = ""
         Exit Function
   End Select

   If rowPlanet = kROW_MARS Then
      GetMarsOrbitalElements JD, elemPlanet
   Else
      GetOrbitalElements PlanetTable, rowPlanet, JD, elemPlanet
   End If

   Select Case UCase(fieldName)
      Case "JDO"
         PlanetDebugValue = elemPlanet.JDo
      Case "JD"
         PlanetDebugValue = elemPlanet.JD
      Case "T"
         PlanetDebugValue = elemPlanet.T
      Case "A"
         PlanetDebugValue = elemPlanet.semMaj
      Case "E"
         PlanetDebugValue = elemPlanet.eccent
      Case "I"
         PlanetDebugValue = elemPlanet.inclin
      Case "L"
         PlanetDebugValue = elemPlanet.meanLon
      Case "VARPI"
         PlanetDebugValue = elemPlanet.lonPerihel
      Case "OMEGA"
         PlanetDebugValue = elemPlanet.ascend
      Case "M"
         PlanetDebugValue = elemPlanet.meanAnomaly
      Case "W"
         PlanetDebugValue = elemPlanet.argPerihel
      Case "E_ANOM"
         PlanetDebugValue = elemPlanet.eccAnomaly
      Case Else
         PlanetDebugValue = ""
   End Select
End Function
"""
    text = re.sub(
        r"\nPublic Function PlanetVectorDebugValue\b.*?\nEnd Function\n",
        "\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    if False:
        text += """

' ------------------------------------------------------------------------------------------------
Public Function PlanetVectorDebugValue(ByVal Planet As String, ByVal JD As Double, ByVal civilJD As Double, ByVal fieldName As String) As Variant
   Dim PlanetTable As Variant
   PlanetTable = FetchPlanetTable(JD)

   Dim rowPlanet As Integer
   Select Case Planet
      Case "Sun"
         rowPlanet = -1
      Case "Mercury"
         rowPlanet = kROW_MERCURY
      Case "Venus"
         rowPlanet = kROW_VENUS
      Case "Mars"
         rowPlanet = kROW_MARS
      Case "Jupiter"
         rowPlanet = kROW_JUPITER
      Case "Saturn"
         rowPlanet = kROW_SATURN
      Case Else
         PlanetVectorDebugValue = ""
         Exit Function
   End Select

   Dim elemEarth As OrbitalElements, posEarth As Vector, velEarth As Vector
   GetOrbitalElements PlanetTable, kROW_EARTH, JD, elemEarth
   GetEarthHeliocentricPos elemEarth, posEarth, velEarth

   Dim Emag As Double, Pmag As Double, Qmag As Double
   Dim Ehat As Vector, Phat As Vector, Qhat As Vector
   Emag = vMod(posEarth)
   Ehat = vDiv(posEarth, Emag)

   Dim elemPlanet As OrbitalElements
   Dim posPlanet As Vector, velPlanet As Vector, posRelPlanet As Vector, velRelPlanet As Vector
   Dim Tau As Double, TauPrev As Double
   Tau = 0

   If rowPlanet = -1 Then
      posPlanet = vZero()
      velPlanet = vZero()
      posRelPlanet = vNeg(posEarth)
      velRelPlanet = vNeg(velEarth)
      Qmag = 0
      Qhat = posPlanet
      Pmag = vMod(posRelPlanet)
   Else
      Dim iterations As Integer
      iterations = 0
      Do
         If rowPlanet = kROW_MARS Then
            GetMarsOrbitalElements JD - Tau, elemPlanet
         Else
            GetOrbitalElements PlanetTable, rowPlanet, JD - Tau, elemPlanet
         End If
         GetPlanetHeliocentricPos elemPlanet, posPlanet, velPlanet

         posRelPlanet = vMinus(posPlanet, posEarth)
         velRelPlanet = vMinus(velPlanet, velEarth)
         Qmag = vMod(posPlanet)
         Pmag = vMod(posRelPlanet)

         TauPrev = Tau
         Tau = (Pmag + k2Mu_C2 * Log((Emag + Pmag + Qmag) / (Emag - Pmag + Qmag))) / kC

         iterations = iterations + 1
         If iterations > 5 Then Exit Do
         If Abs(Tau - TauPrev) < 0.000000000001 Then Exit Do
      Loop
      Qhat = vDiv(posPlanet, Qmag)
   End If

   Phat = vDiv(posRelPlanet, Pmag)

   Dim P1 As Vector
   P1 = vPlus(Phat, vDiv(vMul(vCross(vCross(Qhat, Ehat), Phat), Emag * k2Mu_C2), 1 + vDot(Qhat, Ehat)))

   Dim P2 As Vector, V As Vector, Beta As Double
   V = vDiv(velEarth, kC)
   Beta = 1 / Sqr(1 - vDot(V, V))
   P2 = vDiv(vPlus(vDiv(P1, Beta), vMul(V, 1 + vDot(P1, V) / (1 + 1 / Beta))), 1 + vDot(P1, V))

   elemEarth.JD = civilJD
   Dim P3 As Vector
   P3 = ConvertHeliocentricToGeocentric(elemEarth, P2)

   Dim RA As Double, decl As Double, GHA As Double
   RA = Modulo(ATan2(P3.X(2), P3.X(1)) * kToDeg, 360)
   decl = ASin(P3.X(3)) * kToDeg
   GHA = Modulo(elemEarth.GHA_aries - RA, 360)

   Select Case UCase(fieldName)
      Case "JD"
         PlanetVectorDebugValue = JD
      Case "TAU"
         PlanetVectorDebugValue = Tau
      Case "EMAG"
         PlanetVectorDebugValue = Emag
      Case "PMAG"
         PlanetVectorDebugValue = Pmag
      Case "QMAG"
         PlanetVectorDebugValue = Qmag
      Case "EARTHX"
         PlanetVectorDebugValue = posEarth.X(1)
      Case "EARTHY"
         PlanetVectorDebugValue = posEarth.X(2)
      Case "EARTHZ"
         PlanetVectorDebugValue = posEarth.X(3)
      Case "EARTHVX"
         PlanetVectorDebugValue = velEarth.X(1)
      Case "EARTHVY"
         PlanetVectorDebugValue = velEarth.X(2)
      Case "EARTHVZ"
         PlanetVectorDebugValue = velEarth.X(3)
      Case "BODYX"
         PlanetVectorDebugValue = posPlanet.X(1)
      Case "BODYY"
         PlanetVectorDebugValue = posPlanet.X(2)
      Case "BODYZ"
         PlanetVectorDebugValue = posPlanet.X(3)
      Case "RELX"
         PlanetVectorDebugValue = posRelPlanet.X(1)
      Case "RELY"
         PlanetVectorDebugValue = posRelPlanet.X(2)
      Case "RELZ"
         PlanetVectorDebugValue = posRelPlanet.X(3)
      Case "PHATX"
         PlanetVectorDebugValue = Phat.X(1)
      Case "PHATY"
         PlanetVectorDebugValue = Phat.X(2)
      Case "PHATZ"
         PlanetVectorDebugValue = Phat.X(3)
      Case "QHATX"
         PlanetVectorDebugValue = Qhat.X(1)
      Case "QHATY"
         PlanetVectorDebugValue = Qhat.X(2)
      Case "QHATZ"
         PlanetVectorDebugValue = Qhat.X(3)
      Case "P1X"
         PlanetVectorDebugValue = P1.X(1)
      Case "P1Y"
         PlanetVectorDebugValue = P1.X(2)
      Case "P1Z"
         PlanetVectorDebugValue = P1.X(3)
      Case "P2X"
         PlanetVectorDebugValue = P2.X(1)
      Case "P2Y"
         PlanetVectorDebugValue = P2.X(2)
      Case "P2Z"
         PlanetVectorDebugValue = P2.X(3)
      Case "P3X"
         PlanetVectorDebugValue = P3.X(1)
      Case "P3Y"
         PlanetVectorDebugValue = P3.X(2)
      Case "P3Z"
         PlanetVectorDebugValue = P3.X(3)
      Case "RA"
         PlanetVectorDebugValue = RA
      Case "DECL"
         PlanetVectorDebugValue = decl
      Case "GHA"
         PlanetVectorDebugValue = GHA
      Case Else
         PlanetVectorDebugValue = ""
   End Select
End Function
"""
    return text


def patch_constants(text: str) -> str:
    return text


def patch_mathematical(text: str) -> str:
    if "Public Function RoundTo(ByVal value As Double, ByVal digits As Integer) As Double" not in text:
        anchor = "' Convert from decimal minutes to minutes and minutes decimal suitable for display."
        wrapper = """' ------------------------------------------------------------------------------------------------
' Round to the requested number of decimal places without relying on VBA Round().
Public Function RoundTo(ByVal value As Double, ByVal digits As Integer) As Double
   Dim factor As Double

   factor = 10 ^ digits
   If value >= 0 Then
      RoundTo = Int(value * factor + 0.5) / factor
   Else
      RoundTo = -Int(-value * factor + 0.5) / factor
   End If
End Function

"""
        if anchor in text:
            text = text.replace(anchor, wrapper + anchor, 1)
    text = text.replace("Round(min, 2)", "RoundTo(min, 2)")

    if "Public Function Modulo(ByVal number As Double, ByVal baseNum As Double) As Double" in text:
        return text

    anchor = "' Returns number (modulo base)."
    wrapper = """' ------------------------------------------------------------------------------------------------
' Returns number (modulo base).
Public Function Modulo(ByVal number As Double, ByVal baseNum As Double) As Double
   Modulo = ModuloNumber(number, baseNum)
End Function

"""
    if anchor in text:
        return text.replace(anchor, wrapper + anchor, 1)
    return text


def extract_vector_support(text: str) -> str:
    marker = "' ------------------------------------------------------------------------------------------------\n' Definition of a 3-Vector."
    idx = text.find(marker)
    if idx == -1:
        marker = "Public Type Vector"
        idx = text.find(marker)
    if idx == -1:
        raise RuntimeError("could not locate Vector support block")
    return text[idx:]


def inline_vector_support(body_text: str, vector_text: str) -> str:
    support = extract_vector_support(vector_text).strip()
    anchor = "' Planetary orbital parameters."
    if anchor not in body_text:
        raise RuntimeError("could not locate body-location insertion point")
    return body_text.replace(anchor, support + "\n\n" + anchor, 1)


def publish_standard_modules(tmpdir: Path) -> None:
    standard_dir = tmpdir / "Basic" / "Standard"
    vba_dir = tmpdir / "Basic" / "VBAProject"
    standard_dir.mkdir(parents=True, exist_ok=True)

    module_names = [
        "mo_Constants",
        "mo_Mathematical",
        "mo_DateTime",
        "mo_BodyLocation",
        "mo_Filter",
        "calc_api",
    ]

    for module_name in module_names:
        if module_name == "calc_api":
            target_path = standard_dir / "calc_api.xml"
            write_text_xml(target_path, module_xml_text("calc_api", CALC_API_MODULE))
            continue
        if module_name == "mo_Filter":
            target_path = standard_dir / "mo_Filter.xml"
            write_text_xml(target_path, module_xml_text("mo_Filter", FILTER_MODULE))
            continue
        source_path = vba_dir / f"{module_name}.xml"
        target_path = standard_dir / f"{module_name}.xml"
        shutil.copy2(source_path, target_path)
        text = load_module_text(source_path)
        text = text.replace("Option VBASupport 1", "")
        text = text.replace("Option Private Module", "")
        if module_name == "mo_BodyLocation":
            vector_text = load_module_text(vba_dir / "mo_Vector.xml")
            text = inline_vector_support(text, vector_text)
        save_module_text(target_path, text)

    vector_path = standard_dir / "mo_Vector.xml"
    if vector_path.exists():
        vector_path.unlink()

    set_library_elements(standard_dir / "script-lb.xml", module_names)


def ensure_manifest_entries(tmpdir: Path) -> None:
    manifest_path = tmpdir / "META-INF" / "manifest.xml"
    tree = ET.parse(manifest_path)
    root = tree.getroot()
    file_entry_tag = "{urn:oasis:names:tc:opendocument:xmlns:manifest:1.0}file-entry"
    full_path_attr = "{urn:oasis:names:tc:opendocument:xmlns:manifest:1.0}full-path"
    media_type_attr = "{urn:oasis:names:tc:opendocument:xmlns:manifest:1.0}media-type"

    stale_entries = {
        "Basic/Standard/mo_Vector.xml",
    }
    for child in list(root.findall("manifest:file-entry", MANIFEST_NS)):
        if child.attrib.get(full_path_attr) in stale_entries:
            root.remove(child)

    existing = {child.attrib.get(full_path_attr) for child in root.findall("manifest:file-entry", MANIFEST_NS)}

    wanted_entries = [
        ("Basic/Standard/", ""),
        ("Basic/Standard/script-lb.xml", "text/xml"),
        ("Basic/Standard/mo_BodyLocation.xml", "text/xml"),
        ("Basic/Standard/mo_Constants.xml", "text/xml"),
        ("Basic/Standard/mo_DateTime.xml", "text/xml"),
        ("Basic/Standard/mo_Mathematical.xml", "text/xml"),
        ("Basic/Standard/mo_Filter.xml", "text/xml"),
        ("Basic/Standard/calc_api.xml", "text/xml"),
    ]

    for full_path, media_type in wanted_entries:
        if full_path in existing:
            continue
        entry = ET.SubElement(root, file_entry_tag)
        entry.attrib[media_type_attr] = media_type
        entry.attrib[full_path_attr] = full_path

    tree.write(manifest_path, encoding="UTF-8", xml_declaration=True)


def strip_basic_libraries(tmpdir: Path) -> None:
    basic_dir = tmpdir / "Basic"
    manifest_path = tmpdir / "META-INF" / "manifest.xml"
    full_path_attr = "{urn:oasis:names:tc:opendocument:xmlns:manifest:1.0}full-path"

    if basic_dir.exists():
        shutil.rmtree(basic_dir)

    tree = ET.parse(manifest_path)
    root = tree.getroot()
    for child in list(root.findall("manifest:file-entry", MANIFEST_NS)):
        full_path = child.attrib.get(full_path_attr, "")
        if full_path == "Basic/" or full_path.startswith("Basic/"):
            root.remove(child)

    tree.write(manifest_path, encoding="UTF-8", xml_declaration=True)


def restore_basic_libraries(tmpdir: Path, source_workbook: Path) -> None:
    if (tmpdir / "Basic" / "VBAProject" / "mo_BodyLocation.xml").exists():
        return
    if (tmpdir / "Basic" / "Standard" / "mo_BodyLocation.xml").exists():
        return
    if not source_workbook.exists():
        raise RuntimeError(f"missing macro source workbook: {source_workbook}")

    with zipfile.ZipFile(source_workbook) as zf:
        for member in zf.namelist():
            if member.startswith("Basic/") and not member.endswith("/"):
                target = tmpdir / member
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(zf.read(member))


def strip_vba_project(tmpdir: Path) -> None:
    vba_dir = tmpdir / "Basic" / "VBAProject"
    if vba_dir.exists():
        shutil.rmtree(vba_dir)
    set_basic_libraries(tmpdir / "Basic" / "script-lc.xml", ["Standard"])


def strip_nonessential_package_dirs(tmpdir: Path) -> None:
    for relative in ("Configurations2", "Thumbnails", "Scripts"):
        path = tmpdir / relative
        if path.exists():
            shutil.rmtree(path)


def rewrite_manifest(tmpdir: Path) -> None:
    manifest_path = tmpdir / "META-INF" / "manifest.xml"
    manifest_uri = MANIFEST_NS["manifest"]
    root = ET.Element(f"{{{manifest_uri}}}manifest")
    entries = [("/", "application/vnd.oasis.opendocument.spreadsheet")]

    dirs = sorted(
        p.relative_to(tmpdir).as_posix() + "/"
        for p in tmpdir.rglob("*")
        if p.is_dir() and p.relative_to(tmpdir).as_posix() != "META-INF"
    )
    for directory in dirs:
        entries.append((directory, ""))

    files = sorted(
        p.relative_to(tmpdir).as_posix()
        for p in tmpdir.rglob("*")
        if p.is_file() and p.relative_to(tmpdir).as_posix() not in {"mimetype", "META-INF/manifest.xml"}
    )
    for full_path in files:
        media_type = "text/xml" if full_path.endswith(".xml") else ""
        entries.append((full_path, media_type))

    for full_path, media_type in entries:
        entry = ET.SubElement(root, f"{{{manifest_uri}}}file-entry")
        entry.attrib[f"{{{manifest_uri}}}full-path"] = full_path
        entry.attrib[f"{{{manifest_uri}}}media-type"] = media_type
        if full_path == "/":
            entry.attrib[f"{{{manifest_uri}}}version"] = "1.2"
    ET.ElementTree(root).write(manifest_path, encoding="UTF-8", xml_declaration=True)


def repackage_ods(source_dir: Path, dest_path: Path) -> None:
    entries = sorted(
        [p for p in source_dir.rglob("*") if p.is_file()],
        key=lambda p: (p.name != "mimetype", str(p.relative_to(source_dir))),
    )
    with zipfile.ZipFile(dest_path, "w") as zf:
        for path in entries:
            arcname = path.relative_to(source_dir).as_posix()
            compress_type = zipfile.ZIP_STORED if arcname == "mimetype" else zipfile.ZIP_DEFLATED
            zf.write(path, arcname=arcname, compress_type=compress_type)


def main() -> None:
    workbook = Path("src/almanac/AstroNav 2000-2040.ods")
    with tempfile.TemporaryDirectory(prefix="astro2002-fix-") as tmp:
        tmpdir = Path(tmp)
        with zipfile.ZipFile(workbook) as zf:
            zf.extractall(tmpdir)

        content_path = tmpdir / "content.xml"
        macro_source = MACRO_SOURCE_WORKBOOK if MACRO_SOURCE_WORKBOOK.exists() else workbook
        data_source = DATA_SOURCE_WORKBOOK if DATA_SOURCE_WORKBOOK.exists() else workbook
        restore_basic_libraries(tmpdir, macro_source)
        module_dir = tmpdir / "Basic" / "VBAProject"
        source_is_vba_project = module_dir.exists()
        if not source_is_vba_project:
            module_dir = tmpdir / "Basic" / "Standard"

        body_location = module_dir / "mo_BodyLocation.xml"
        constants = module_dir / "mo_Constants.xml"
        mathematical = module_dir / "mo_Mathematical.xml"
        calc_api = module_dir / "calc_api.xml"
        sh_celestial = module_dir / "sh_Celestial.xml"
        mo_filter = module_dir / "mo_Filter.xml"
        wbk_astro = module_dir / "wbk_Astro.xml"
        has_basic_project = body_location.exists()

        replace_reference_data_sheets(content_path, data_source)
        # Planet Data is now workbook-owned; the almanac DB no longer carries
        # the old orbital-elements fallback tables that used to regenerate it.
        replace_earth_state_with_generated_windows(content_path)
        replace_moon_state_with_generated_windows(content_path)
        strip_sheet_controls(content_path)
        update_location_defaults(content_path)
        update_navigation_formulas(content_path)
        strip_named_and_database_ranges(content_path)
        trim_repeated_columns(content_path)
        rebuild_minimal_content(content_path)
        replace_help_sheet(content_path)
        add_navigation_formulas(content_path)
        apply_mars_lab_theme(content_path)
        protect_support_sheets(content_path)
        write_minimal_sidecars(tmpdir)
        if has_basic_project:
            save_module_text(body_location, patch_body_location(load_module_text(body_location)))
            save_module_text(constants, patch_constants(load_module_text(constants)))
            save_module_text(mathematical, patch_mathematical(load_module_text(mathematical)))
            if calc_api.exists():
                save_module_text(calc_api, CALC_API_MODULE)
            else:
                write_text_xml(calc_api, module_xml_text("calc_api", CALC_API_MODULE))
            if sh_celestial.exists():
                save_module_text(sh_celestial, CELESTIAL_MODULE)
            write_text_xml(mo_filter, module_xml_text("mo_Filter", FILTER_MODULE))
            if wbk_astro.exists():
                save_module_text(wbk_astro, WORKBOOK_MODULE)
            if source_is_vba_project:
                publish_standard_modules(tmpdir)
            else:
                set_library_elements(
                    module_dir / "script-lb.xml",
                    ["mo_Constants", "mo_Mathematical", "mo_DateTime", "mo_BodyLocation", "mo_Filter", "calc_api"],
                )
            ensure_manifest_entries(tmpdir)
            if source_is_vba_project:
                strip_vba_project(tmpdir)
        strip_nonessential_package_dirs(tmpdir)
        rewrite_manifest(tmpdir)
        ensure_content_namespaces(content_path)

        repaired = tmpdir / "AstroNav 2000-2040.repaired.ods"
        repackage_ods(tmpdir, repaired)
        shutil.copy2(repaired, workbook)


if __name__ == "__main__":
    main()
