#!/usr/bin/env python3
"""Repair Astro2002.ods VBA macros for LibreOffice compatibility."""

from __future__ import annotations

import re
import shutil
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape


SCRIPT_NS = {"script": "http://openoffice.org/2000/script"}
ODS_NS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
    "draw": "urn:oasis:names:tc:opendocument:xmlns:drawing:1.0",
}
MANIFEST_NS = {"manifest": "urn:oasis:names:tc:opendocument:xmlns:manifest:1.0"}
SCRIPT_URI = "http://openoffice.org/2000/script"
LIBRARY_URI = "http://openoffice.org/2000/library"
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
   Dim sourceData As Variant
   Dim rowCount As Integer, colCount As Integer
   Dim rowIndex As Integer, colIndex As Integer
   Dim outData() As Variant

   sourceData = GetNamedRange(rangeName).DataArray
   rowCount = UBound(sourceData, 1) - LBound(sourceData, 1) + 1
   colCount = UBound(sourceData, 2) - LBound(sourceData, 2) + 1
   ReDim outData(1 To rowCount, 1 To colCount)

   For rowIndex = 1 To rowCount
      For colIndex = 1 To colCount
         outData(rowIndex, colIndex) = sourceData(rowIndex - 1, colIndex - 1)
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
   BoolCellValue = (textValue = "TRUE")
End Function

' ------------------------------------------------------------------------------------------------
Sub FilterVisibles()
   ' Row-visibility control is not reliable under LibreOffice VBA compatibility.
End Sub

' ------------------------------------------------------------------------------------------------
Sub ShowAllBodies()
   ' Keep as a no-op so callers do not trip unsupported row property writes.
End Sub
"""


WORKBOOK_MODULE = """Rem Attribute VBA_ModuleType=VBADocumentModule
Option VBASupport 1
' ------------------------------------------------------------------------------------------------
Private Sub Workbook_Open()
   On Error Resume Next
   RecalculateNavigationTable
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
' Filter-toggle control state is not portable in LibreOffice VBA mode.
Private Sub Workbook_SheetChange(ByVal Sh As Object, ByVal Target As Object)
   On Error Resume Next
   If IsCelestialInput(Target) Then RecalculateNavigationTable
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
   Else
      CoerceDateValue = DateValue(CStr(dateVal))
   End If
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceTimeValue(ByVal timeVal As Variant) As Date
   If IsDate(timeVal) Then
      CoerceTimeValue = CDate(timeVal)
   Else
      CoerceTimeValue = TimeValue(CStr(timeVal))
   End If
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceText(ByVal value As Variant) As String
   If IsNull(value) Or IsEmpty(value) Then
      CoerceText = ""
   Else
      CoerceText = CStr(value)
   End If
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceDouble(ByVal value As Variant) As Double
   If IsNull(value) Or IsEmpty(value) Then
      CoerceDouble = 0#
   ElseIf IsNumeric(value) Then
      CoerceDouble = CDbl(value)
   Else
      CoerceDouble = CDbl(Val(CStr(value)))
   End If
End Function

' ------------------------------------------------------------------------------------------------
Private Function CoerceInteger(ByVal value As Variant) As Integer
   CoerceInteger = CInt(CoerceDouble(value))
End Function

' ------------------------------------------------------------------------------------------------
Public Function JulianValueCalc(ByVal dateVal As Variant, ByVal timeVal As Variant) As Variant
   JulianValueCalc = JulianValue(CoerceDateValue(dateVal), CoerceTimeValue(timeVal))
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

   Set oSheet = CelestialSheet()
   For i = 1 To 7
      oSheet.getCellByPosition(i, rowIndex - 1).String = CStr(attrs(i))
   Next i
   visibleText = "FALSE"
   If Trim(CStr(attrs(4))) <> "" Then
      If Left(Trim(CStr(attrs(4))), 1) <> "-" Then visibleText = "TRUE"
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
   Dim zoneHours As Double, lat As Double, lon As Double, jd As Double
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

   oSheet.getCellByPosition(255, 0).Value = jd

   For rowIndex = 7 To 73
      bodyName = CellText(oSheet.getCellByPosition(0, rowIndex - 1))
      attrs = BlankAttrs()

      Select Case bodyName
         Case "Sun", "Mercury", "Venus", "Mars", "Jupiter", "Saturn"
            attrs = PlanetPosition(bodyName, jd, lat, lon)
         Case "Moon"
            attrs = MoonPosition(jd, lat, lon)
         Case "Aries"
            attrs(3) = DegMinSec(GHAAries(jd))
         Case Else
            starRow = FindStarRow(bodyName)
            If starRow > 0 Then
               attrs = StarPosition(starRow, jd, lat, lon)
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
        for row_number in range(7, min(len(rows), 74) + 1):
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
        julian_cell = row1[255]
        julian_cell.attrib.pop(formula_attr, None)
        julian_cell.attrib[value_type_attr] = "float"
        julian_cell.attrib[value_attr] = "0"
        break

    tree.write(content_path, encoding="UTF-8", xml_declaration=True)


def add_debug_block(content_path: Path) -> None:
    tree = ET.parse(content_path)
    root = tree.getroot()

    formula_attr = f"{{{ODS_NS['table']}}}formula"
    value_type_attr = f"{{{ODS_NS['office']}}}value-type"
    value_attr = f"{{{ODS_NS['office']}}}value"

    debug_values = [
        ("Debug", None),
        ("Calc", None),
        ("Julian", None),
        ("Sun", None),
        ("Moon", None),
        ("Star", None),
    ]

    for table in root.findall(".//table:table", ODS_NS):
        if table.attrib.get(f"{{{ODS_NS['table']}}}name") != "Location of Navigational Bodies":
            continue

        rows = table.findall("table:table-row", ODS_NS)
        for offset, (label, formula) in enumerate(debug_values, start=1):
            row = expanded_cells(row_at(rows, offset))
            label_cell = row[9]
            value_cell = row[10]

            set_cell_text(label_cell, label)
            set_cell_text(value_cell, "")
            if formula is not None:
                value_cell.attrib[formula_attr] = formula
            else:
                value_cell.attrib.pop(formula_attr, None)
            value_cell.attrib[value_type_attr] = "string"
            value_cell.attrib.pop(value_attr, None)
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
    return text


def patch_mathematical(text: str) -> str:
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
        "calc_api",
    ]

    for module_name in module_names:
        if module_name == "calc_api":
            target_path = standard_dir / "calc_api.xml"
            write_text_xml(target_path, module_xml_text("calc_api", CALC_API_MODULE))
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
        ("Basic/Standard/calc_api.xml", "text/xml"),
    ]

    for full_path, media_type in wanted_entries:
        if full_path in existing:
            continue
        entry = ET.SubElement(root, file_entry_tag)
        entry.attrib[media_type_attr] = media_type
        entry.attrib[full_path_attr] = full_path

    tree.write(manifest_path, encoding="UTF-8", xml_declaration=True)


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
    workbook = Path("src/almanac/Astro2002.ods")
    with tempfile.TemporaryDirectory(prefix="astro2002-fix-") as tmp:
        tmpdir = Path(tmp)
        with zipfile.ZipFile(workbook) as zf:
            zf.extractall(tmpdir)

        content_path = tmpdir / "content.xml"
        body_location = tmpdir / "Basic" / "VBAProject" / "mo_BodyLocation.xml"
        mathematical = tmpdir / "Basic" / "VBAProject" / "mo_Mathematical.xml"
        sh_celestial = tmpdir / "Basic" / "VBAProject" / "sh_Celestial.xml"
        mo_filter = tmpdir / "Basic" / "VBAProject" / "mo_Filter.xml"
        wbk_astro = tmpdir / "Basic" / "VBAProject" / "wbk_Astro.xml"

        strip_sheet_controls(content_path)
        update_location_defaults(content_path)
        update_navigation_formulas(content_path)
        add_debug_block(content_path)
        save_module_text(body_location, patch_body_location(load_module_text(body_location)))
        save_module_text(mathematical, patch_mathematical(load_module_text(mathematical)))
        save_module_text(sh_celestial, CELESTIAL_MODULE)
        save_module_text(mo_filter, FILTER_MODULE)
        save_module_text(wbk_astro, WORKBOOK_MODULE)
        publish_standard_modules(tmpdir)
        ensure_manifest_entries(tmpdir)
        ensure_content_namespaces(content_path)

        repaired = tmpdir / "Astro2002.repaired.ods"
        repackage_ods(tmpdir, repaired)
        shutil.copy2(repaired, workbook)


if __name__ == "__main__":
    main()
