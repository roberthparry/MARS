"""SQLCipher calendar regressions; README examples run after ordinary tests."""

import ctypes
import ctypes.util
import datetime
import hashlib
import io
import json
import math
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import jurisdiction_calendar as calendar
import jurisdiction_calendar_languages as languages
import configure_mars_lab_jurisdiction_db as installer


ROOT = Path(__file__).resolve().parents[2]
VIEW_SCRIPT = "packaging/jurisdiction-db/mars_calendar_local.sql"
LOCALE_SCRIPT = "packaging/jurisdiction-db/mars_calendar_locale_names.sql"
SCHEMA_SCRIPT = "packaging/jurisdiction-db/mars_holiday_rules.sql"
HOLIDAY_NAMES_SCRIPT = "packaging/jurisdiction-db/mars_holiday_localized_names.sql"
TEST_KEY = "calendar-regression-only"
HIJRI_MONTHS = ("محرم", "صفر", "ربيع الأول", "ربيع الآخر", "جمادى الأولى", "جمادى الآخرة",
                "رجب", "شعبان", "رمضان", "شوال", "ذو القعدة", "ذو الحجة")
SPANISH_HOLIDAYS = {
    "Armistice Day": "Día del Armisticio",
    "Christmas Day": "Día de Navidad",
    "Columbus Day": "Día de la Raza",
    "Independence Day": "Día de la Independencia",
    "Juneteenth National Independence Day": "Día de la Liberación (Juneteenth)",
    "Labor Day": "Día del Trabajo",
    "Martin Luther King Jr. Day": "Día de Martin Luther King, Jr.",
    "Memorial Day": "Día de la Conmemoración de los Caídos",
    "New Year's Day": "Día de Año Nuevo",
    "Thanksgiving Day": "Día de Acción de Gracias",
    "Veterans Day": "Día de los Veteranos",
    "Washington's Birthday": "Natalicio de George Washington",
}
INUPIAQ_MONTHS = ("Siqiññaatchiaq", "Siqiññaasugruk", "Paniqsiqsiivik", "Umiaqqavik", "Suppivik", "Iġñivik",
                  "Iñukkuksaivik", "Amiġaiqsivik", "Sikuaqtuġvik", "Sikkuvik", "Nippivik", "Siqiñġiḷaq")
INUPIAQ_DAYS = ("Atautchiiġñiq", "Aippiġñiq", "Piŋatchiġñiq", "Sisammiġñiq", "Tallimmiġñiq", "Itchaksriġñiq", "Savaiññiq")
PATWA_MONTHS = ("Januari", "Febiweri", "Maach", "April", "May", "Juun", "July", "Aagus",
                "Septemba", "Aktoba", "Novemba", "Disemba")
PATWA_DAYS = ("Mondeh", "Tuesdeh", "Wenzdeh", "Turzdeh", "Frideh", "Satdeh", "Sundeh")
PATWA_TOWNS = ("Birmingham", "Bristol", "Leeds", "Leicester", "London", "Manchester", "Nottingham")
FRISIAN_HOLIDAYS = {
    "Nieuwjaarsdag": "Nijjiersdei",
    "Goede Vrijdag": "Goed Freed",
    "Eerste Paasdag": "Earste Peaskedei",
    "Tweede Paasdag": "Twadde Peaskedei",
    "Koningsdag": "Keningsdei",
    "Bevrijdingsdag": "Befrijingsdei",
    "Hemelvaartsdag": "Himelfeartsdei",
    "Eerste Pinksterdag": "Earste Pinksterdei",
    "Tweede Pinksterdag": "Twadde Pinksterdei",
    "Eerste Kerstdag": "Earste Krystdei",
    "Tweede Kerstdag": "Twadde Krystdei",
    "Koninginnedag": "Keninginnedei",
}
CANADIAN_FRENCH_HOLIDAYS = {
    "New Year's Day": "Jour de l’An",
    "Good Friday": "Vendredi saint",
    "Canada Day": "Fête du Canada",
    "Labour Day": "Fête du Travail",
    "Christmas Day": "Noël",
    "Dominion Day": "Fête du Dominion",
}


def civil_hijri_reference(date):
    """Independent cycle-and-month walk, bounded by 30 years and 12 months."""
    cycles, remaining = divmod((date - datetime.date(622, 7, 19)).days, 10631)
    year = cycles * 30 + 1
    while remaining >= 354 + ((11 * year + 14) % 30 < 11):
        remaining -= 354 + ((11 * year + 14) % 30 < 11)
        year += 1
    month = 1
    while month < 12 and remaining >= 30 - (month + 1) % 2:
        remaining -= 30 - (month + 1) % 2
        month += 1
    return year, month, remaining + 1


def arabic_hijri_text(date):
    year, month, day = civil_hijri_reference(date)
    return f"{day} {HIJRI_MONTHS[month - 1]} {year}".translate(str.maketrans("0123456789", "٠١٢٣٤٥٦٧٨٩"))


def english_ordinal(day):
    suffix = 'ᵗʰ' if 11 <= day % 100 <= 13 else {1: 'ˢᵗ', 2: 'ⁿᵈ', 3: 'ʳᵈ'}.get(day % 10, 'ᵗʰ')
    return f'{day}{suffix}'


class CalendarLocaleRenderingTests(unittest.TestCase):
    def setUp(self):
        self.connection = sqlite3.connect(":memory:")
        self.addCleanup(self.connection.close)
        self.connection.executescript((ROOT / LOCALE_SCRIPT).read_text())
        self.connection.executescript((ROOT / VIEW_SCRIPT).read_text())
        self.connection.execute("insert into calendar_local_settings values "
                                "(1, 'Test', 'GB-ENG', 'Europe/London', 52, -2, 2024, 2025, 'en_GB')")
        self.dates = ("2024-01-01", "2024-02-29", "2024-06-21", "2024-07-07", "2024-12-31", "2025-01-01")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(day,) for day in self.dates])

    def test_uk_english_lingua_ordinals_for_every_day_and_jurisdiction(self):
        self.connection.execute('delete from calendar_local_days')
        self.connection.executemany('insert into calendar_local_days values (?, null, 0, 0)',
                                   [(f'2026-01-{day:02d}',) for day in range(1, 32)])
        for town, jurisdiction in (('Shrewsbury', 'GB-ENG'), ('Rhyl', 'GB-WLS'),
                                   ('Edinburgh', 'GB-SCT'), ('Belfast', 'GB-NIR'), ('London', 'GB')):
            for locale in ('en_GB', None):
                with self.subTest(town=town, locale=locale):
                    self.connection.execute('update calendar_local_settings set location=?, jurisdiction=?, locale=?',
                                            (town, jurisdiction, locale))
                    rows = self.connection.execute('select [Date Lingua], [Date UK], [Date Regional] '
                                                   'from calendar_local order by FullDateAlternateKey').fetchall()
                    self.assertEqual(rows, [(f'{english_ordinal(day)} January 2026',
                                             f'{english_ordinal(day)} January 2026', f'{day} January 2026')
                                            for day in range(1, 32)])

    def test_uk_lingua_suffix_does_not_change_other_languages_or_overseas_english(self):
        for locale in ('cy_GB', 'ga_GB', 'gd_GB', 'kw_GB', 'jam', 'fr_CA'):
            with self.subTest(locale=locale):
                self.connection.execute("update calendar_local_settings set jurisdiction='NL', locale=?", (locale,))
                expected = self.connection.execute('select [Date Lingua] from calendar_local '
                                                   'order by FullDateAlternateKey').fetchall()
                self.connection.execute("update calendar_local_settings set jurisdiction='GB-WLS'")
                self.assertEqual(self.connection.execute('select [Date Lingua] from calendar_local '
                                 'order by FullDateAlternateKey').fetchall(), expected)
        for jurisdiction, locale, expected in (
                ('IE', 'en_GB', '1 January 2024'), ('NL', 'en_GB', '1 January 2024'),
                ('US-NY', 'en_US', 'January 1, 2024'), ('CA-QC', 'en_CA', 'January 1, 2024'),
                ('IM', 'en_GB', '1 January 2024'), ('JE', 'en_GB', '1 January 2024')):
            with self.subTest(jurisdiction=jurisdiction):
                self.connection.execute('update calendar_local_settings set jurisdiction=?, locale=?',
                                        (jurisdiction, locale))
                self.assertEqual(self.connection.execute('select [Date Lingua] from calendar_local '
                                 "where FullDateAlternateKey='2024-01-01'").fetchone(), (expected,))

    def test_french_first_day_ordinal_across_locales_and_months(self):
        self.connection.execute("delete from calendar_local_days")
        dates = [datetime.date(2024, 1, 1) + datetime.timedelta(days=offset) for offset in range(366)]
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(day.isoformat(),) for day in dates])
        locales = self.connection.execute("select locale from calendar_locale_date_pattern "
                                          "where locale = 'fr' or locale glob 'fr_*' order by locale").fetchall()
        self.assertTrue(locales)
        months = ("janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août",
                  "septembre", "octobre", "novembre", "décembre")
        expected = [(f"{day.day}{'ᵉʳ' if day.day == 1 else ''} {months[day.month - 1]} {day.year}",) for day in dates]
        for (locale,) in locales:
            with self.subTest(locale=locale):
                self.connection.execute("update calendar_local_settings set jurisdiction = 'CA-QC', locale = ?", (locale,))
                for column in ("Date Lingua", "Date Regional"):
                    rows = self.connection.execute(f'select [{column}] from calendar_local '
                                                   'order by FullDateAlternateKey').fetchall()
                    self.assertEqual(rows, expected)
        self.connection.execute("update calendar_local_settings set locale = 'en_CA'")
        self.assertEqual(self.connection.execute('select [Date Lingua], [Date Regional] from calendar_local '
                         "where FullDateAlternateKey = '2024-01-01'").fetchone(),
                         ("January 1, 2024", "1ᵉʳ janvier 2024"))

    def test_regional_dates_ignore_selected_language_and_use_subdivision_default(self):
        self.connection.execute("insert into calendar_local_days values ('2026-07-04', null, 1, 0)")
        for jurisdiction, selected, regional in (
                ("US-NY", "yi", "July 4, 2026"), ("US-NY", "lad", "July 4, 2026"),
                ("US-NY", "es_US", "July 4, 2026"), ("CA-QC", "en_CA", "4 juillet 2026"),
                ("NL", "en_GB", "4 juli 2026"), ("IL", "yi", "4 ביולי 2026"),
                ("PS", "lad", "٤ تموز ٢٠٢٦")):
            with self.subTest(jurisdiction=jurisdiction, selected=selected):
                self.connection.execute("update calendar_local_settings set jurisdiction = ?, locale = ?",
                                        (jurisdiction, selected))
                row = self.connection.execute('select [Date Regional], [Date UK], [Date Lingua] from calendar_local '
                                               "where FullDateAlternateKey = '2026-07-04'").fetchone()
                self.assertEqual(row[:2], (regional, "4ᵗʰ July 2026"))
                if selected == "yi":
                    self.assertEqual(row[2], "4טן יולי 2026")
                if selected == "lad":
                    self.assertEqual(row[2], "4 de djulyo de 2026")
        self.connection.execute("update calendar_local_settings set jurisdiction = 'US-NY', locale = null")
        self.assertEqual(self.connection.execute('select [Date Regional] = [Date Lingua] from calendar_local '
                         "where FullDateAlternateKey = '2026-07-04'").fetchone(), (1,))

    def test_patwa_calendar_names_and_abbreviations(self):
        self.connection.execute("update calendar_local_settings set locale = 'jam', jurisdiction = 'JM'")
        self.connection.execute("delete from calendar_local_days")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(f"2026-{month:02d}-06",) for month in range(1, 13)])
        rows = self.connection.execute('select [Date Lingua], [Month Name], [Month Name Abbrev] '
                                       'from calendar_local order by FullDateAlternateKey').fetchall()
        short = ("Jan.", "Feb.", "Maa.", "Apr.", "May", "Juun", "July", "Aag.", "Sep.", "Akt.", "Nov.", "Dis.")
        self.assertEqual(rows, [(f"6 {name} 2026", name, abbreviation)
                                for name, abbreviation in zip(PATWA_MONTHS, short)])
        self.connection.execute("delete from calendar_local_days")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(f"2026-08-{day:02d}",) for day in range(3, 10)])
        rows = self.connection.execute('select [Day Name], [Day Name Abbrev] from calendar_local '
                                       'order by FullDateAlternateKey').fetchall()
        self.assertEqual(rows, list(zip(PATWA_DAYS, ("Mon.", "Tue.", "Wen.", "Tur.", "Fri.", "Sat.", "Sun."))))

    def test_ladino_gregorian_names_and_abbreviations(self):
        self.connection.execute("delete from calendar_local_days")
        self.connection.execute("update calendar_local_settings set locale = 'lad'")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(f"2026-{month:02d}-01",) for month in range(1, 13)])
        rows = self.connection.execute('select [Date Lingua], [Month Name], [Month Name Abbrev] from calendar_local '
                                       'order by FullDateAlternateKey').fetchall()
        months = ("jenero", "fevrero", "marso", "avril", "mayo", "djunyo", "djulyo", "agosto",
                  "septembre", "oktovre", "novembre", "desembre")
        abbreviations = ("jen.", "fev.", "mar.", "avr.", "may.", "djun.", "djul.", "ago.", "sep.", "okt.", "nov.", "des.")
        self.assertEqual(rows, [(f"1 de {month} de 2026", month, short) for month, short in zip(months, abbreviations)])
        self.connection.execute("delete from calendar_local_days")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(f"2026-06-{day}",) for day in range(15, 22)])
        self.assertEqual(self.connection.execute('select [Day Name], [Day Name Abbrev] from calendar_local '
                         'order by FullDateAlternateKey').fetchall(),
                         [("lunes", "lun."), ("martes", "mar."), ("mierkoles", "mie."), ("djueves", "dju."),
                          ("viernes", "vie."), ("shabat", "sha."), ("alhad", "alh.")])

    def test_cajun_and_pennsylvania_calendar_names(self):
        cases = (
            ("frc", "4 de ", "janvier février mars avril mai juin juillet août septembre octobre novembre décembre",
             "janv. févr. mars avr. mai juin juil. août sept. oct. nov. déc.",
             "lundi mardi mercredi jeudi vendredi samedi dimanche", "lun. mar. mer. jeu. ven. sam. dim."),
            ("pdc", "4. ", "Yenner Hanning Matz Abrill Moi Tschunn Tschulei Aaguscht September Oktower Nofember Diesember",
             "Yen. Han. Matz Abr. Moi Tschun. Tschul. Aag. Sep. Okt. Nof. Die.",
             "Mundaag Dinschdaag Mittwoch Dunnerschdaag Freidaag Samschdaag Sunndaag", "Mu. Di. Mi. Du. Fr. Sa. Su."),
        )
        for locale, prefix, months, short_months, days, short_days in cases:
            with self.subTest(locale=locale):
                self.connection.execute("update calendar_local_settings set locale = ?, jurisdiction = 'US-LA'", (locale,))
                self.connection.execute("delete from calendar_local_days")
                self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                    [(f"2026-{month:02d}-04",) for month in range(1, 13)])
                rows = self.connection.execute('select [Date Lingua], [Month Name], [Month Name Abbrev] '
                    'from calendar_local order by FullDateAlternateKey').fetchall()
                self.assertEqual(rows, [(prefix + month + " 2026", month, short)
                                       for month, short in zip(months.split(), short_months.split())])
                self.connection.execute("delete from calendar_local_days")
                self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                    [(f"2026-06-{day}",) for day in range(15, 22)])
                self.assertEqual(self.connection.execute('select [Day Name], [Day Name Abbrev] from calendar_local '
                    'order by FullDateAlternateKey').fetchall(), list(zip(days.split(), short_days.split())))

    def test_inupiaq_calendar_names_and_abbreviations(self):
        self.connection.execute("update calendar_local_settings set locale = 'ik', jurisdiction = 'US-AK'")
        self.connection.execute("delete from calendar_local_days")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
            [(f"2026-{month:02d}-04",) for month in range(1, 13)])
        short_months = ("Siqiññaat.", "Siqiññaas.", "Pani.", "Umia.", "Supp.", "Iġñ.", "Iñuk.", "Amiġ.",
                        "Siku.", "Sikk.", "Nipp.", "Siqiñġi.")
        self.assertEqual(self.connection.execute('select [Date Lingua], [Month Name], [Month Name Abbrev] '
            'from calendar_local order by FullDateAlternateKey').fetchall(),
            [(month + " 4, 2026", month, short) for month, short in zip(INUPIAQ_MONTHS, short_months)])
        self.connection.execute("delete from calendar_local_days")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
            [(f"2026-06-{day}",) for day in range(15, 22)])
        self.assertEqual(self.connection.execute('select [Day Name], [Day Name Abbrev] from calendar_local '
            'order by FullDateAlternateKey').fetchall(),
            list(zip(INUPIAQ_DAYS, ("Ata.", "Aip.", "Piŋ.", "Sis.", "Tal.", "Itc.", "Sav."))))

    def test_working_day_count_uses_policy_and_keeps_initial_zero(self):
        self.connection.execute("delete from calendar_local_days")
        self.assertEqual(self.connection.execute('select [Working Day No] from calendar_local').fetchall(), [])
        # Friday/Saturday weekends, a working Sunday, holidays and a year boundary.
        days = [("2024-12-27", None, 1, 0), ("2024-12-28", None, 1, 0),
                ("2024-12-29", None, 0, 0), ("2024-12-30", "Holiday", 0, 0),
                ("2024-12-31", None, 0, 0), ("2025-01-01", "Holiday", 0, 0)]
        self.connection.executemany("insert into calendar_local_days values (?, ?, ?, ?)", reversed(days))
        rows = self.connection.execute('select [Working Day No], [Workday Type] from calendar_local '
                                       'order by FullDateAlternateKey').fetchall()
        self.assertEqual(rows, [(0, 0), (0, 0), (1, 1), (1, 0), (2, 1), (2, 0)])
        self.assertEqual(self.connection.execute('select [Working Day No] from calendar_local '
                         "where FullDateAlternateKey = '2025-01-01'").fetchall(), [(2,)])
        self.connection.execute("delete from calendar_local_days")
        self.connection.execute("insert into calendar_local_days values ('2024-12-29', null, 0, 0)")
        self.assertEqual(self.connection.execute('select [Working Day No] from calendar_local').fetchall(), [(1,)])

    def test_date_order_literals_eras_and_standalone_month_grammar(self):
        for locale, expected_date, expected_month in (
                ("nl_NL", "21 juni 2024", "juni"), ("fr_CA", "21 juin 2024", "juin"),
                ("en_US", "June 21ˢᵗ, 2024", "June"), ("en_GB", "21ˢᵗ June 2024", "June"),
                ("de_DE", "21. Juni 2024", "Juni"), ("ja_JP", "2024年6月21日", "6月"),
                ("la_VA", "21 Iunii 2024", "Iunius"),
                ("ru_RU", "21 июня 2024\u202fг.", "июнь"),
                ("fi_FI", "21. kesäkuuta 2024", "kesäkuu"),
                ("ar_EG", "١٤ ذو الحجة ١٤٤٥", "يونيو"),
                ("th_TH", "21 มิถุนายน ค.ศ. 2024", "มิถุนายน")):
            with self.subTest(locale=locale):
                self.connection.execute("update calendar_local_settings set locale = ?", (locale,))
                row = self.connection.execute('select [Date Lingua], "Month Name", [Date UK] from calendar_local '
                                               "where FullDateAlternateKey = '2024-06-21'").fetchone()
                self.assertEqual(row, (expected_date, expected_month, "21ˢᵗ June 2024"))

    def test_latin_dates_and_all_weekdays_omit_day_prefixes(self):
        self.connection.execute("delete from calendar_local_days")
        self.connection.execute("update calendar_local_settings set locale = 'la_VA', first_year = 2027, last_year = 2027")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(f"2027-05-{day}",) for day in range(16, 23)])
        rows = self.connection.execute('select [Date Lingua], [Day Name], [Day Name Abbrev], [Month Name] '
                                       'from calendar_local order by FullDateAlternateKey').fetchall()
        weekdays = (("Solis", "Sol"), ("Lunae", "Lun"), ("Martis", "Mar"), ("Mercurii", "Mer"),
                    ("Iovis", "Iov"), ("Veneris", "Ven"), ("Saturni", "Sat"))
        self.assertEqual(rows, [(f"{day} Maii 2027", full, short, "Maius")
                                for day, (full, short) in enumerate(weekdays, 16)])

    def test_arabic_local_dates_use_arabic_indic_digits_only(self):
        self.connection.execute("delete from calendar_local_days")
        self.connection.execute("update calendar_local_settings set first_year = 2026, last_year = 2026")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(f"2026-01-{day}",) for day in range(10, 20)])
        for locale in ("ar_IL", "ar_PS", "ar_EG", "ar_MA", "ar_DZ"):
            with self.subTest(locale=locale):
                self.connection.execute("update calendar_local_settings set locale = ?", (locale,))
                rows = self.connection.execute('select FullDateAlternateKey, [Date Lingua], [Date UK], '
                                               '[Year], [Month], [Day] from calendar_local '
                                               'order by FullDateAlternateKey').fetchall()
                for day, (key, local_date, uk_date, year, month, number) in enumerate(rows, 10):
                    self.assertEqual(key, f"2026-01-{day}")
                    self.assertEqual(local_date, arabic_hijri_text(datetime.date.fromisoformat(key)))
                    self.assertNotRegex(local_date, r"[0-9]")
                    self.assertEqual(uk_date, f"{day}ᵗʰ January 2026")
                    self.assertEqual((year, month, number), (2026, 1, day))
                self.assertEqual(set("".join(row[1] for row in rows)) & set("٠١٢٣٤٥٦٧٨٩"), set("٠١٢٣٤٥٦٧٨٩"))

    def calendar_oracle(self, calendar_name):
        library = ctypes.util.find_library("icui18n")
        if not library:
            self.skipTest("ICU is not installed (independent calendar reference only)")
        icu = ctypes.CDLL(library)
        api = {}
        for name, arguments, result in (
                ("ucal_open", [ctypes.c_void_p, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int,
                               ctypes.POINTER(ctypes.c_int)], ctypes.c_void_p),
                ("ucal_setMillis", [ctypes.c_void_p, ctypes.c_double, ctypes.POINTER(ctypes.c_int)], None),
                ("ucal_get", [ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int)], ctypes.c_int),
                ("ucal_close", [ctypes.c_void_p], None)):
            function = getattr(icu, name, None) or getattr(icu, name + "_" + library.rsplit(".", 1)[-1])
            function.argtypes, function.restype = arguments, result
            api[name] = function
        status = ctypes.c_int(0)
        zone = (ctypes.c_uint16 * 3)(85, 84, 67)  # UTC; avoid the host's time zone.
        oracle = api["ucal_open"](zone, 3, ("en_US@calendar=" + calendar_name).encode(), 0, ctypes.byref(status))
        self.assertTrue(oracle)
        self.assertLessEqual(status.value, 0)
        self.addCleanup(api["ucal_close"], oracle)

        def fields(date):
            milliseconds = (date - datetime.date(1970, 1, 1)).days * 86400000
            api["ucal_setMillis"](oracle, milliseconds, ctypes.byref(status))
            result = tuple(api["ucal_get"](oracle, field, ctypes.byref(status)) + (field == 2)
                           for field in (1, 2, 5))
            self.assertLessEqual(status.value, 0)
            return result

        return fields

    def test_hijri_full_leap_cycle_matches_icu_and_native(self):
        oracle = self.calendar_oracle("islamic-civil")
        native = ctypes.CDLL(str(ROOT / "build/release/libmars.so"))
        for name, arguments, result in (
                ("datetime_from_string", [ctypes.c_char_p], ctypes.c_void_p),
                ("datetime_muslim_calendar_date_text", [ctypes.c_void_p], ctypes.c_void_p),
                ("string_c_str", [ctypes.c_void_p], ctypes.c_char_p),
                ("string_free", [ctypes.c_void_p], None),
                ("datetime_dealloc", [ctypes.c_void_p], None)):
            function = getattr(native, name)
            function.argtypes, function.restype = arguments, result
        english_months = ("Muharram", "Safar", "Rabi al-awwal", "Rabi al-thani", "Jumada al-awwal", "Jumada al-thani",
                          "Rajab", "Sha'ban", "Ramadan", "Shawwal", "Dhu al-Qadah", "Dhu al-Hijjah")
        first = datetime.date(2015, 1, 1)
        dates = [first + datetime.timedelta(days=offset) for offset in range(10631)]
        self.connection.execute("delete from calendar_local_days")
        self.connection.execute("update calendar_local_settings set locale = 'ar_EG', first_year = 2015, last_year = 2044")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(date.isoformat(),) for date in dates])
        rows = self.connection.execute('select FullDateAlternateKey, [Date Lingua] from calendar_local '
                                       'order by FullDateAlternateKey').fetchall()
        self.assertEqual(len(rows), len(dates))
        for date, (key, text) in zip(dates, rows):
            year, month, day = oracle(date)
            self.assertEqual(civil_hijri_reference(date), (year, month, day), key)
            expected = f"{day} {HIJRI_MONTHS[month - 1]} {year}"
            self.assertEqual(text, expected.translate(str.maketrans("0123456789", "٠١٢٣٤٥٦٧٨٩")), key)
            native_date = native.datetime_from_string(key.encode())
            native_text = native.datetime_muslim_calendar_date_text(native_date)
            try:
                self.assertEqual(native.string_c_str(native_text).decode(),
                                 f"{day} {english_months[month - 1]} {year} AH", key)
            finally:
                native.string_free(native_text)
                native.datetime_dealloc(native_date)


    def test_hebrew_dates_cover_new_year_leap_months_and_gregorian_keys(self):
        expected = {
            "2005-10-03": "29 באלול 5765",
            "2005-10-04": "1 בתשרי 5766",
            "2024-02-10": "1 באדר א׳ 5784",
            "2024-03-11": "1 באדר ב׳ 5784",
            "2024-04-23": "15 בניסן 5784",
            "2024-10-02": "29 באלול 5784",
            "2024-10-03": "1 בתשרי 5785",
            "2025-03-01": "1 באדר 5785",
            "2026-04-02": "15 בניסן 5786",
            "2028-09-20": "29 באלול 5788",
            "2028-09-21": "1 בתשרי 5789",
        }
        self.connection.execute("delete from calendar_local_days")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(day,) for day in expected])
        for locale in ("he", "he_IL"):
            with self.subTest(locale=locale):
                self.connection.execute("update calendar_local_settings set locale = ?", (locale,))
                rows = self.connection.execute('select FullDateAlternateKey, [Date Lingua], [Year], [Month], [Day] '
                                               'from calendar_local order by FullDateAlternateKey').fetchall()
                self.assertEqual(len(rows), len(expected))
                for key, local_date, year, month, day in rows:
                    self.assertEqual(local_date, expected[key], key)
                    self.assertEqual((year, month, day), tuple(map(int, key.split("-"))))
                self.assertEqual(self.connection.execute('select [Date UK] from calendar_local '
                    "where FullDateAlternateKey = '2026-04-02'").fetchone(), ("2ⁿᵈ April 2026",))

    def test_hebrew_two_leap_cycles_match_icu_and_native(self):
        oracle = self.calendar_oracle("hebrew")
        native = ctypes.CDLL(str(ROOT / "build/release/libmars.so"))
        for name, arguments, result in (
                ("datetime_from_string", [ctypes.c_char_p], ctypes.c_void_p),
                ("datetime_jewish_calendar_date_text", [ctypes.c_void_p], ctypes.c_void_p),
                ("string_c_str", [ctypes.c_void_p], ctypes.c_char_p),
                ("string_free", [ctypes.c_void_p], None),
                ("datetime_dealloc", [ctypes.c_void_p], None)):
            function = getattr(native, name)
            function.argtypes, function.restype = arguments, result
        hebrew_months = ("תשרי", "חשוון", "כסלו", "טבת", "שבט", "אדר א׳", "אדר",
                         "ניסן", "אייר", "סיוון", "תמוז", "אב", "אלול")
        english_months = ("Tishrei", "Cheshvan", "Kislev", "Tevet", "Shevat", "Adar I", "Adar",
                          "Nisan", "Iyar", "Sivan", "Tammuz", "Av", "Elul")
        # Thirty-eight years cover all six year lengths and the exceptional postponements.
        first, last = datetime.date(2000, 1, 1), datetime.date(2038, 1, 1)
        dates = [first + datetime.timedelta(days=offset) for offset in range((last - first).days)]
        self.connection.execute("delete from calendar_local_days")
        self.connection.execute("update calendar_local_settings set locale = 'he_IL', first_year = 2000, last_year = 2037")
        self.connection.executemany("insert into calendar_local_days values (?, null, 0, 0)",
                                    [(date.isoformat(),) for date in dates])
        rows = self.connection.execute('select FullDateAlternateKey, [Date Lingua] from calendar_local '
                                       'order by FullDateAlternateKey').fetchall()
        self.assertEqual(len(rows), len(dates))
        starts, month_lengths, observed_months = {}, set(), set()
        previous = None
        for date, (key, text) in zip(dates, rows):
            year, month, day = oracle(date)
            self.assertEqual(key, date.isoformat())
            leap = (7 * year + 1) % 19 < 7
            hebrew_month = "אדר ב׳" if month == 7 and leap else hebrew_months[month - 1]
            english_month = "Adar II" if month == 7 and leap else english_months[month - 1]
            self.assertEqual(text, f"{day} ב{hebrew_month} {year}", key)
            if month == 1 and day == 1:
                starts[year] = date
            if previous is not None and day == 1:
                month_lengths.add(previous)
            observed_months.add(hebrew_month)
            previous = day
            native_date = native.datetime_from_string(key.encode())
            native_text = native.datetime_jewish_calendar_date_text(native_date)
            try:
                self.assertTrue(native_text, key)
                self.assertEqual(native.string_c_str(native_text).decode(), f"{day} {english_month} {year} AM", key)
            finally:
                native.string_free(native_text)
                native.datetime_dealloc(native_date)
        self.assertEqual({(starts[year + 1] - start).days for year, start in starts.items() if year + 1 in starts},
                         {353, 354, 355, 383, 384, 385})
        self.assertEqual(month_lengths, {29, 30})
        self.assertTrue({"אדר", "אדר א׳", "אדר ב׳"} <= observed_months)

    def test_every_packaged_locale_against_babel_reference(self):
        # Optional generation-data regression; Babel remains unnecessary for
        # installation and for every SQL query above.
        try:
            import babel
            from babel import localedata
            from babel.dates import format_date, get_month_names, get_day_names
        except ImportError:
            self.skipTest("Babel is not installed (generation-only dependency)")
        if babel.__version__ != "2.17.0":
            self.skipTest("Reference comparison requires the pinned Babel 2.17.0 / CLDR 46")
        locales = self.connection.execute("select locale from calendar_locale_name_set order by locale").fetchall()
        for (locale,) in locales:
            if locale in ("frc", "ik", "jam", "lad", "pdc"):
                continue  # Hand-maintained supplement; tested separately, not attributed to Babel.
            with self.subTest(locale=locale):
                self.connection.execute("update calendar_local_settings set locale = ?", (locale,))
                rows = self.connection.execute('select FullDateAlternateKey, [Date Lingua], "Month Name", '
                                                '"Month Name Abbrev", "Day Name", "Day Name Abbrev" '
                                                'from calendar_local order by FullDateAlternateKey').fetchall()
                self.assertEqual(len(rows), len(self.dates))
                # Babel 2.17's shared lazy aliases otherwise contaminate the
                # reference when unrelated locales are formatted in one process.
                localedata._cache.clear()
                names = get_month_names("wide", context="stand-alone", locale=locale)
                short_months = get_month_names("abbreviated", locale=locale)
                days = get_day_names("wide", locale=locale)
                short_days = get_day_names("abbreviated", locale=locale)
                for date_text, local_date, month_name, month_abbrev, day_name, day_abbrev in rows:
                    day = datetime.date.fromisoformat(date_text)
                    latin = locale.split("_")[0] == "la"
                    pattern = "d MMMM y" if latin else "long"
                    expected_date = format_date(day, pattern, locale=locale)
                    if locale.split("_")[0] == "en":
                        expected_date = re.sub(r'\b0?' + str(day.day) + r'\b',
                                               lambda match: match[0] + english_ordinal(day.day)[len(str(day.day)):],
                                               expected_date)
                    if locale.split("_")[0] == "fr" and day.day == 1:
                        expected_date = "1ᵉʳ" + expected_date[1:]
                    if locale.split("_")[0] == "ar":
                        expected_date = arabic_hijri_text(day)
                    if locale.split("_")[0] == "he":
                        # These six dates are also covered by the full ICU comparison above.
                        expected_date = {
                            "2024-01-01": "20 בטבת 5784", "2024-02-29": "20 באדר א׳ 5784",
                            "2024-06-21": "15 בסיוון 5784", "2024-07-07": "1 בתמוז 5784",
                            "2024-12-31": "30 בכסלו 5785", "2025-01-01": "1 בטבת 5785",
                        }[date_text]
                    self.assertEqual(local_date, expected_date)
                    self.assertEqual(month_name, names[day.month])
                    self.assertEqual(month_abbrev, short_months[day.month])
                    expected_day = days[day.weekday()]
                    self.assertEqual(day_name, expected_day.removeprefix("dies ") if latin else expected_day)
                    self.assertEqual(day_abbrev, short_days[day.weekday()])


class CalendarDatabase:
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.temporary = tempfile.TemporaryDirectory(prefix="mars-calendar-test-")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.database = Path(cls.temporary.name) / "jurisdiction.db"
        cls.query(f".read {SCHEMA_SCRIPT}\n")
        cls.catalogue = calendar.LocationCatalogue(cls.database, TEST_KEY)
        cls.location = cls.catalogue.resolve("Shrewsbury")
        calendar.populate_calendar(cls.database, TEST_KEY, cls.location)
        rows = cls.query("select * from calendar_local order by FullDateAlternateKey;")
        cls.rows = {row["FullDateAlternateKey"]: row for row in rows}
        cls.row_count = len(rows)

    @classmethod
    def query(cls, sql):
        completed = subprocess.run(
            [shutil.which("sqlcipher"), str(cls.database)], cwd=ROOT,
            input=(".bail on\n.output /dev/null\n"
                   f"pragma key = '{TEST_KEY}';\n.output stdout\n.mode json\n{sql}\n"),
            text=True, capture_output=True, timeout=60,
        )
        if completed.returncode:
            raise AssertionError(completed.stderr)
        return json.loads(completed.stdout) if completed.stdout.strip() else []


@unittest.skipUnless(shutil.which("sqlcipher"), "SQLCipher shell is not installed")
class CalendarLocalTests(CalendarDatabase, unittest.TestCase):
    def test_quebec_offers_english_with_french_default(self):
        def choose(**kwargs):
            return languages.choose_language(self.database, TEST_KEY, "CA-QC", **kwargs)
        self.assertEqual(choose(), "fr_CA")
        self.assertEqual(choose(requested="English"), "en_CA")
        self.assertEqual(choose(requested="French"), "fr_CA")
        self.assertEqual(choose(saved="en_CA"), "en_CA")
        self.assertEqual(choose(requested="French", saved="en_CA"), "fr_CA")
        for answer, expected in (("", "fr_CA"), ("1", "en_CA"), ("2", "fr_CA"), ("en-CA", "en_CA")):
            menu = io.StringIO()
            with self.subTest(answer=answer), mock.patch("builtins.input", return_value=answer), \
                    mock.patch("sys.stdout", menu):
                self.assertEqual(choose(interactive=True), expected)
            output = menu.getvalue()
            self.assertIn("1. English", output)
            self.assertIn("2. French", output)
            self.assertIn("fr_CA) [Enter]", output)

    def test_canadian_french_holiday_names_preserve_policy(self):
        for jurisdiction in ("CA", "CA-QC", "CA-ON"):
            with self.subTest(jurisdiction=jurisdiction):
                original, weekends = calendar.native_policy(self.database, TEST_KEY, jurisdiction, 2026, 2026)
                translated, actual_weekends = calendar.native_policy(
                    self.database, TEST_KEY, jurisdiction, 2026, 2026, "fr_CA")
                self.assertTrue(original)
                self.assertEqual(translated, {date: {CANADIAN_FRENCH_HOLIDAYS[name] for name in names}
                                             for date, names in original.items()})
                self.assertEqual(actual_weekends, weekends)

        for locale in ("fr", "fr-CA", "fr_CA"):
            for year, expected in ((1982, "Fête du Dominion"), (1983, "Fête du Canada")):
                with self.subTest(locale=locale, year=year):
                    holidays, _ = calendar.native_policy(self.database, TEST_KEY, "CA-QC", year, year, locale)
                    self.assertEqual(holidays[f"{year}-07-01"], {expected})
        sql = ("select n.* from holiday_name as n join holiday_definition as d using (holiday_id) "
               "where d.jurisdiction_id = 'CA' order by n.holiday_name_id;")
        before = self.query(sql)
        self.assertEqual({row["localized_name"] for row in before if row["locale"] == "fr"},
                         set(CANADIAN_FRENCH_HOLIDAYS.values()))
        self.assertEqual({row["localized_name"] for row in before if row["is_primary"] == "Y"
                          and row["locale"] == "en_CA"}, set(CANADIAN_FRENCH_HOLIDAYS))
        self.query(f".read {HOLIDAY_NAMES_SCRIPT}\n")
        self.assertEqual(self.query(sql), before)

    def test_installer_quebec_languages_preserve_regional_dates_and_policy(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-quebec-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True):
                database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                sql = ('select FullDateAlternateKey, [Date Lingua], [Date Regional], [Public Holiday], '
                       '[Workday Type], [Working Day No] from calendar_local order by FullDateAlternateKey;')
                previous = None
                for arguments, locale in ((["Quebec"], "fr_CA"),
                                          (["--language", "English", "Quebec"], "en_CA"), ([], "en_CA")):
                    with self.subTest(locale=locale, arguments=arguments), mock.patch.object(sys, "argv",
                            ["installer", "--noninteractive", *arguments]):
                        self.assertEqual(installer.main(), 0)
                        self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], locale)
                        rows = calendar.run_sql(database, TEST_KEY, sql)
                        self.assertTrue(rows)
                        if previous is not None:
                            fields = ("FullDateAlternateKey", "Date Regional", "Workday Type", "Working Day No")
                            self.assertEqual([tuple(row[field] for field in fields) for row in rows],
                                             [tuple(row[field] for field in fields) for row in previous])
                        original, _ = calendar.native_policy(database, TEST_KEY, "CA-QC", 2015,
                                                              int(rows[-1]["FullDateAlternateKey"][:4]))
                        for row in rows:
                            names = original.get(row["FullDateAlternateKey"], set())
                            expected = {CANADIAN_FRENCH_HOLIDAYS[name] if locale == "fr_CA" else name for name in names}
                            self.assertEqual(set((row["Public Holiday"] or "").split("; ")) - {""}, expected)
                            if row["FullDateAlternateKey"] == "2026-07-01":
                                self.assertEqual(row["Date Regional"], "1ᵉʳ juillet 2026")
                                self.assertEqual(row["Date Lingua"],
                                                 "1ᵉʳ juillet 2026" if locale == "fr_CA" else "July 1, 2026")
                        if arguments == []:
                            self.assertEqual(rows, previous)
                        previous = rows

    def test_western_frisian_holiday_names_and_observances(self):
        original, weekends = calendar.native_policy(self.database, TEST_KEY, "NL", 2026, 2026)
        self.assertEqual(len(original), 11)
        for locale in ("fy", "fy_NL", "fy-NL"):
            with self.subTest(locale=locale):
                names, actual_weekends = calendar.native_policy(self.database, TEST_KEY, "NL", 2026, 2026, locale)
                self.assertEqual(names, {date: {FRISIAN_HOLIDAYS[name] for name in values}
                                         for date, values in original.items()})
                self.assertEqual(actual_weekends, weekends)
        for year, date, label in ((1927, "1927-08-31", "Keninginnedei"),
                                  (1950, "1950-05-01", "Keninginnedei"),
                                  (1989, "1989-04-29", "Keninginnedei"),
                                  (2025, "2025-04-26", "Keningsdei")):
            with self.subTest(year=year):
                original, weekends = calendar.native_policy(self.database, TEST_KEY, "NL", year, year)
                translated, actual_weekends = calendar.native_policy(self.database, TEST_KEY, "NL", year, year, "fy_NL")
                self.assertEqual(translated, {date: {FRISIAN_HOLIDAYS[name] for name in values}
                                             for date, values in original.items()})
                self.assertEqual(actual_weekends, weekends)
                self.assertEqual(translated[date], {label})
        self.assertEqual(self.query("""
select d.holiday_id from holiday_definition as d
left join holiday_name as n on n.holiday_id = d.holiday_id and n.locale = 'fy'
where d.jurisdiction_id = 'NL' and n.holiday_name_id is null;
"""), [])
        labels = calendar.holiday_labels(self.database, TEST_KEY, "NL", "fy_NL")
        self.assertNotIn((234, "Koningsdag (unrecognised event)"), labels)
        query = "select * from holiday_name where locale = 'fy' order by holiday_id;"
        before = self.query(query)
        self.assertEqual(len(before), 14)
        self.assertTrue(all(row["is_primary"] == "N" for row in before))
        self.query(f".read {HOLIDAY_NAMES_SCRIPT}\n")
        self.assertEqual(self.query(query), before)

    def test_installer_western_frisian_preserves_dutch_regional_dates_and_policy(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-frisian-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch.object(sys, "argv",
                    ["installer", "--noninteractive", "--language", "Western Frisian", "Limmen"]):
                self.assertEqual(installer.main(), 0)
                self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "fy_NL")
                database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                sql = ('select FullDateAlternateKey, [Date Lingua], [Date Regional], [Public Holiday], '
                       '[Day Name], [Month Name], [Workday Type], [Working Day No] '
                       'from calendar_local order by FullDateAlternateKey;')
                rows = calendar.run_sql(database, TEST_KEY, sql)
                self.assertTrue(rows)
                original, weekends = calendar.native_policy(database, TEST_KEY, "NL", 2015,
                                                               int(rows[-1]["FullDateAlternateKey"][:4]))
                total = 0
                for row in rows:
                    key = row["FullDateAlternateKey"]
                    date = datetime.date.fromisoformat(key)
                    if key in original:
                        self.assertEqual(set(row["Public Holiday"].split("; ")),
                                         {FRISIAN_HOLIDAYS[name] for name in original[key]})
                    else:
                        self.assertIsNone(row["Public Holiday"])
                    workday = key not in original and not weekends[date.year, date.weekday()]
                    total += workday
                    self.assertEqual((row["Workday Type"], row["Working Day No"]), (int(workday), total))
                    if key == "2026-01-01":
                        self.assertEqual((row["Date Lingua"], row["Date Regional"], row["Public Holiday"]),
                                         ("1 Jannewaris 2026", "1 januari 2026", "Nijjiersdei"))
                    if key == "2026-12-25":
                        self.assertEqual((row["Date Lingua"], row["Date Regional"], row["Public Holiday"],
                                          row["Day Name"], row["Month Name"]),
                                         ("25 Desimber 2026", "25 december 2026", "Earste Krystdei", "freed", "Desimber"))
                # Reinstallation keeps the saved language without consulting the host locale.
                with mock.patch.object(sys, "argv", ["installer", "--noninteractive"]):
                    self.assertEqual(installer.main(), 0)
                self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "fy_NL")
                self.assertEqual(calendar.run_sql(database, TEST_KEY, sql), rows)

    def test_irish_holiday_names_cover_uk_and_ireland_without_changing_policy(self):
        expected = {
            "2026-01-01": {"Lá Caille"},
            "2026-04-03": {"Aoine an Chéasta"},
            "2026-04-06": {"Luan Cásca"},
            "2026-05-04": {"Lá saoire bainc na Bealtaine"},
            "2026-05-25": {"Lá saoire bainc an earraigh"},
            "2026-08-31": {"Lá saoire bainc mhí Lúnasa"},
            "2026-12-25": {"Lá Nollag"},
            "2026-12-28": {"Lá Fhéile Stiofáin (lá ionaid)"},
        }
        for jurisdiction in ("GB-ENG", "GB-WLS"):
            with self.subTest(jurisdiction=jurisdiction):
                names, _ = calendar.native_policy(self.database, TEST_KEY, jurisdiction, 2026, 2026, "ga_GB")
                self.assertEqual(names, expected)
        for jurisdiction in ("GB-ENG", "GB-WLS", "GB-SCT", "IE"):
            with self.subTest(jurisdiction=jurisdiction):
                original, weekends = calendar.native_policy(self.database, TEST_KEY, jurisdiction, 2015, 2033)
                self.assertTrue(original)
                for locale in ("ga", "ga-GB", "ga_IE"):
                    labels = calendar.holiday_labels(self.database, TEST_KEY, jurisdiction, locale)
                    by_source = {source: translated for (_, source), translated in labels.items()}
                    self.assertTrue(set().union(*original.values()).issubset(by_source))
                    translated, actual_weekends = calendar.native_policy(
                        self.database, TEST_KEY, jurisdiction, 2015, 2033, locale)
                    self.assertEqual(translated, {date: {by_source[name] for name in names}
                                                 for date, names in original.items()})
                    self.assertEqual(actual_weekends, weekends)
                self.assertNotIn((next(iter(labels))[0], "Unknown special closure"), labels)
        names, _ = calendar.native_policy(self.database, TEST_KEY, "IE", 2026, 2026, "ga_IE")
        self.assertEqual(names["2026-02-02"], {"Lá Fhéile Bríde"})
        self.assertEqual(names["2026-03-17"], {"Lá Fhéile Pádraig"})
        self.assertEqual(names["2026-06-01"], {"Lá saoire bainc mhí an Mheithimh"})
        self.assertEqual(names["2026-08-03"], {"Lá saoire bainc mhí Lúnasa"})
        self.assertEqual(names["2026-10-26"], {"Lá saoire bainc mhí Dheireadh Fómhair"})
        self.assertEqual(names["2026-12-26"], {"Lá Fhéile Stiofáin"})
        historic, _ = calendar.native_policy(self.database, TEST_KEY, "IE", 1960, 1960, "ga_IE")
        self.assertEqual(historic["1960-06-06"], {"Luan Cincíse"})
        missing = self.query("""
select d.holiday_id from holiday_definition as d
left join holiday_name as n on n.holiday_id = d.holiday_id and n.locale = 'ga'
where (d.jurisdiction_id = 'IE' or d.jurisdiction_id = 'GB' or d.jurisdiction_id glob 'GB-*')
  and n.holiday_name_id is null;
""")
        self.assertEqual(missing, [])
        queries = ("select * from holiday_name where locale = 'ga' order by holiday_id;",
                   "select * from holiday_event_localized_name where locale = 'ga' order by holiday_id, source_name;",
                   "select * from holiday_name_qualifier where locale = 'ga' order by source_suffix;")
        before = [self.query(query) for query in queries]
        self.query(f".read {HOLIDAY_NAMES_SCRIPT}\n")
        self.assertEqual([self.query(query) for query in queries], before)

    def test_installer_irish_translates_holidays_but_preserves_regional_dates(self):
        for town, jurisdiction, locale in (("Rhyl", "GB-WLS", "ga_GB"), ("Dublin, IE", "IE", "ga_IE")):
            with self.subTest(town=town), tempfile.TemporaryDirectory(prefix="mars-calendar-irish-") as temporary:
                environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
                with mock.patch.dict(os.environ, environment, clear=True), mock.patch.object(sys, "argv",
                        ["installer", "--noninteractive", "--language", "Irish", town]):
                    self.assertEqual(installer.main(), 0)
                    self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], locale)
                database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                rows = calendar.run_sql(database, TEST_KEY,
                    'select FullDateAlternateKey, [Date Lingua], [Date Regional], [Public Holiday], '
                    '[Workday Type], [Working Day No] from calendar_local order by FullDateAlternateKey;')
                self.assertTrue(rows)
                labels = calendar.holiday_labels(database, TEST_KEY, jurisdiction, locale)
                by_source = {source: translated for (_, source), translated in labels.items()}
                original, weekends = calendar.native_policy(database, TEST_KEY, jurisdiction, 2015,
                                                               int(rows[-1]["FullDateAlternateKey"][:4]))
                total = 0
                for row in rows:
                    key = row["FullDateAlternateKey"]
                    date = datetime.date.fromisoformat(key)
                    if key in original:
                        self.assertEqual(set(row["Public Holiday"].split("; ")),
                                         {by_source[name] for name in original[key]})
                    else:
                        self.assertIsNone(row["Public Holiday"])
                    workday = key not in original and not weekends[date.year, date.weekday()]
                    total += workday
                    self.assertEqual((row["Workday Type"], row["Working Day No"]), (int(workday), total))
                    if key == "2026-12-25":
                        self.assertEqual(row["Date Lingua"], "25 Nollaig 2026")
                        self.assertEqual(row["Date Regional"], "25 December 2026")
                        self.assertEqual(row["Public Holiday"], "Lá Nollag")

    def test_installer_rhyl_welsh_has_complete_calendar(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-rhyl-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch.object(sys, "argv",
                    ["installer", "--noninteractive", "--language", "Welsh", "Rhyl"]):
                self.assertEqual(installer.main(), 0)
            database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
            settings = calendar.run_sql(database, TEST_KEY, "select * from calendar_local_settings;")[0]
            self.assertEqual((settings["jurisdiction"], settings["locale"]), ("GB-WLS", "cy_GB"))
            rows = calendar.run_sql(database, TEST_KEY, 'select FullDateAlternateKey, [Date UK], [Date Lingua], '
                '[Date Regional], [Day Name], [Month Name], [Public Holiday], Sunrise, Sunset, '
                '[Workday Type], [Working Day No] '
                'from calendar_local order by FullDateAlternateKey;')
            expected = (datetime.date(settings["last_year"] + 1, 1, 1)
                        - datetime.date(settings["first_year"], 1, 1)).days
            self.assertEqual(len(rows), expected)
            self.assertTrue(all(row[column] for row in rows for column in
                ("Date UK", "Date Lingua", "Date Regional", "Day Name", "Month Name", "Sunrise", "Sunset")))
            christmas = next(row for row in rows if row["FullDateAlternateKey"] == "2026-12-25")
            self.assertEqual(christmas["Date Lingua"], "25 Rhagfyr 2026")
            self.assertEqual(christmas["Day Name"], "Dydd Gwener")
            self.assertEqual(christmas["Public Holiday"], "Dydd Nadolig")
            expected_holidays = {
                "2026-01-01": "Dydd Calan", "2026-04-03": "Gwener y Groglith",
                "2026-04-06": "Llun y Pasg", "2026-05-04": "Gŵyl banc dechrau Mai",
                "2026-05-25": "Gŵyl banc y gwanwyn", "2026-08-31": "Gŵyl banc yr haf",
                "2026-12-25": "Dydd Nadolig", "2026-12-28": "Dydd San Steffan (diwrnod amgen)",
            }
            self.assertEqual({row["FullDateAlternateKey"]: row["Public Holiday"] for row in rows
                if row["FullDateAlternateKey"].startswith("2026-") and row["Public Holiday"]}, expected_holidays)
            english, weekends = calendar.native_policy(database, TEST_KEY, "GB-ENG",
                                                         settings["first_year"], settings["last_year"])
            welsh, welsh_weekends = calendar.native_policy(database, TEST_KEY, "GB-WLS",
                                                           settings["first_year"], settings["last_year"])
            self.assertEqual((welsh, welsh_weekends), (english, weekends))
            labels = calendar.holiday_labels(database, TEST_KEY, "GB-WLS", "cy_GB")
            by_source = {source: translated for (_, source), translated in labels.items()}
            total = 0
            for row in rows:
                key = row["FullDateAlternateKey"]
                date = datetime.date.fromisoformat(key)
                if key in english:
                    self.assertTrue(english[key].issubset(by_source))
                    self.assertEqual(set(row["Public Holiday"].split("; ")),
                                     {by_source[source] for source in english[key]})
                else:
                    self.assertIsNone(row["Public Holiday"])
                workday = key not in english and not weekends[date.year, date.weekday()]
                total += workday
                self.assertEqual((row["Workday Type"], row["Working Day No"]), (int(workday), total))
            # Every Welsh catalogue town resolves to the corrected policy, without
            # inventing a political parent relationship with England.
            towns = calendar.run_sql(database, TEST_KEY,
                "select town_name from jurisdiction_town where jurisdiction_id = 'GB-WLS';")
            self.assertGreater(len(towns), 1)
            for town in towns:
                self.assertEqual(self.catalogue.resolve(town["town_name"] + ", GB-WLS").jurisdiction, "GB-WLS")
            self.assertEqual(calendar.run_sql(database, TEST_KEY,
                "select parent_jurisdiction_id from jurisdiction where jurisdiction_id = 'GB-WLS';"),
                [{"parent_jurisdiction_id": "GB"}])
            before = calendar.run_sql(database, TEST_KEY,
                "select * from holiday_event_localized_name order by holiday_id, locale, source_name;")
            calendar.run_sql(database, TEST_KEY, f".read {HOLIDAY_NAMES_SCRIPT}\n")
            self.assertEqual(calendar.run_sql(database, TEST_KEY,
                "select * from holiday_event_localized_name order by holiday_id, locale, source_name;"), before)
            self.assertEqual(calendar.run_sql(database, TEST_KEY, "pragma foreign_key_check;"), [])

    def test_installer_rejects_incomplete_view_without_replacing_database(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-validation-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch.object(sys, "argv",
                    ["installer", "--noninteractive", "--language", "Welsh", "Rhyl"]):
                self.assertEqual(installer.main(), 0)
                database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                original_db = hashlib.sha256(database.read_bytes()).digest()
                original_config = installer.config_path().read_bytes()
                populate = calendar.populate_calendar
                for corruption in (
                        "delete from calendar_locale_date_pattern where locale = 'cy_GB';",
                        "delete from calendar_month_names where name_set = 'cy' and month_no = 4;"):
                    with self.subTest(corruption=corruption):
                        def incomplete(database, key, location, executable, locale):
                            calendar.run_sql(database, key, corruption, executable)
                            return populate(database, key, location, executable, locale)
                        errors = io.StringIO()
                        with mock.patch.object(installer, "populate_calendar", side_effect=incomplete), \
                                mock.patch("sys.stderr", errors):
                            self.assertEqual(installer.main(), 1)
                        self.assertIn("Incomplete calendar for Rhyl, GB-WLS", errors.getvalue())
                        self.assertEqual(hashlib.sha256(database.read_bytes()).digest(), original_db)
                        self.assertEqual(installer.config_path().read_bytes(), original_config)

    def test_location_catalogue_and_selection_precedence(self):
        limmen = self.catalogue.resolve("LiMmEn")
        self.assertEqual(limmen.label, "Limmen, NL")
        self.assertEqual(limmen.timezone, "Europe/Amsterdam")
        self.assertEqual(self.catalogue.resolve("Shrewsbury").jurisdiction, "GB-ENG")
        self.assertEqual(self.catalogue.resolve("Shrewsbury, GB").jurisdiction, "GB")
        with mock.patch.dict(os.environ, {"LANG": "en_GB.UTF-8"}, clear=True):
            self.assertEqual(calendar.choose_location(self.catalogue, "limmen", "Shrewsbury", False), limmen)
            self.assertEqual(calendar.choose_location(self.catalogue, "", "Limmen, NL", False), limmen)
            self.assertEqual(calendar.choose_location(self.catalogue, "", "", False).label, "Shrewsbury, GB-ENG")
        with mock.patch.dict(os.environ, {"LANG": "nl_NL.UTF-8"}, clear=True):
            self.assertEqual(calendar.choose_location(self.catalogue, "", "", False), limmen)
            with mock.patch("builtins.input", side_effect=["Shrewsbury", "1"]):
                self.assertEqual(calendar.choose_location(self.catalogue, "", "", True).jurisdiction, "GB-ENG")
        with mock.patch.dict(os.environ, {"LANG": "C"}, clear=True):
            with self.assertRaisesRegex(ValueError, "No calendar location"):
                calendar.choose_location(self.catalogue, "", "", False)
            with mock.patch("builtins.input", side_effect=["limmen", "1"]):
                self.assertEqual(calendar.choose_location(self.catalogue, "", "", True), limmen)
        with self.assertRaisesRegex(ValueError, "Unknown calendar location"):
            self.catalogue.resolve("limmen'; drop table jurisdiction; --")
        # Two genuine places sharing a name require explicit jurisdiction selection.
        with mock.patch.dict(self.catalogue.by_name, {"ambiguous": [
                calendar.Location("Ambiguous", "NL", 52, 4, "Europe/Amsterdam"),
                calendar.Location("Ambiguous", "GB-ENG", 53, -2, "Europe/London")]}):
            with self.assertRaisesRegex(ValueError, "Ambiguous calendar location"):
                self.catalogue.resolve("ambiguous")
            self.assertEqual(self.catalogue.resolve("ambiguous, NL").jurisdiction, "NL")

    def test_quebec_aliases_accents_and_suggestions(self):
        for name in ("Quebec", "Québec", "QUE\u0301BEC", "  Quebec  City ", "Québec, ca-qc"):
            with self.subTest(name=name):
                place = self.catalogue.resolve(name)
                self.assertEqual(place.label, "Québec, CA-QC")
                self.assertEqual(place.timezone, "America/Toronto")
        self.assertEqual(self.catalogue.defaults["CA-QC"].name, "Québec")
        self.assertEqual(self.catalogue.resolve("Quebec, CA").jurisdiction, "CA")
        self.assertEqual(self.catalogue.resolve("Montreal"), self.catalogue.resolve("Montréal"))
        comma_town = calendar.Location("Washington, D.C.", "US-DC", 38.9, -77, "America/New_York")
        with mock.patch.dict(self.catalogue.by_name, {"washington, d.c.": [comma_town]}):
            self.assertEqual(self.catalogue.resolve("Washington, D.C."), comma_town)
            self.assertEqual(self.catalogue.resolve("Washington, D.C., US-DC"), comma_town)
        with self.assertRaisesRegex(ValueError, "Did you mean:.*Québec, CA-QC"):
            self.catalogue.resolve("Quebc")

    def test_town_menu_paging_filtering_invalid_choices_and_cancellation(self):
        output = io.StringIO()
        with mock.patch.dict(os.environ, {"LANG": "C"}, clear=True), \
                mock.patch("sys.stdout", output), \
                mock.patch("builtins.input", side_effect=["n", "p", "999", "Quebc", "1"]):
            result = calendar.choose_location(self.catalogue, "", "", True)
        self.assertEqual(result.label, "Québec, CA-QC")
        self.assertIn("Towns — page 2/", output.getvalue())
        self.assertIn("Choose one of the numbers shown", output.getvalue())
        # Invalid initial names are recoverable, but suggestions are not accepted automatically.
        with mock.patch("builtins.input", side_effect=["/", "no-such-town-9182", "Quebec", "1"]):
            self.assertEqual(calendar.choose_location(self.catalogue, "Quebc", "", True).label, "Québec, CA-QC")
        with mock.patch("builtins.input", return_value=""):
            self.assertEqual(calendar.choose_location(self.catalogue, "", "Limmen, NL", True).label, "Limmen, NL")
        for answer in ("q", EOFError(), KeyboardInterrupt()):
            with self.subTest(answer=answer), mock.patch("builtins.input", side_effect=[answer]):
                with self.assertRaisesRegex(ValueError, "cancelled"):
                    calendar.choose_location(self.catalogue, "Quebec", "", True)

    def test_language_choices_cover_jurisdictions_and_have_names(self):
        self.assertEqual(self.query("""
select jurisdiction from calendar_jurisdiction_language as choices
left join calendar_language_name using (language)
left join calendar_locale_name_set using (locale)
where english_name is null or native_name is null or (locale is not null and month_set is null);
"""), [])
        self.assertEqual(self.query("""
select jurisdiction from calendar_jurisdiction_language group by jurisdiction having count(locale) = 0;
"""), [])
        rows = self.query("select language, status from calendar_jurisdiction_language where jurisdiction = 'VA';")
        self.assertEqual(rows, [{"language": "it", "status": "de_facto_official"},
                                {"language": "la", "status": "holy_see"}])
        rows = self.query("select language from calendar_jurisdiction_language where jurisdiction = 'BE' order by language;")
        self.assertEqual(rows, [{"language": "de"}, {"language": "fr"}, {"language": "nl"}])

    def test_language_menu_latin_saved_choice_and_rejection(self):
        def choose(jurisdiction, **kwargs):
            return languages.choose_language(self.database, TEST_KEY, jurisdiction, **kwargs)
        output = io.StringIO()
        with mock.patch("builtins.input", side_effect=["99", "2"]), mock.patch("sys.stdout", output):
            self.assertEqual(choose("VA", interactive=True), "la_VA")
        self.assertIn("Latin — Latina (Holy See; la_VA)", output.getvalue())
        self.assertIn("Choose a listed language", output.getvalue())
        for request in ("la", "Latin", "Latina", "la-VA"):
            self.assertEqual(choose("VA", requested=request), "la_VA")
        self.assertEqual(choose("VA", saved="la_VA"), "la_VA")
        self.assertEqual(choose("CA-QC", saved="la_VA"), "fr_CA")
        self.assertEqual(choose("CA", requested="French"), "fr_CA")
        self.assertEqual(choose("CA", requested="English", saved="fr_CA"), "en_CA")
        with self.assertRaisesRegex(ValueError, "unavailable.*Choose:"):
            choose("VA", requested="Dutch")
        with mock.patch("builtins.input", side_effect=["q"]):
            with self.assertRaisesRegex(ValueError, "cancelled"):
                choose("VA", interactive=True)
        with mock.patch("builtins.input", side_effect=["q"]):
            with self.assertRaisesRegex(ValueError, "cancelled"):
                choose("CA-QC", requested="Dutch", interactive=True)

    def test_breton_and_cornish_choices_preserve_defaults(self):
        for jurisdiction, code, locale, english, native, default in (
                ("FR", "br", "br_FR", "Breton", "brezhoneg", "fr_FR"),
                ("GB-ENG", "kw", "kw_GB", "Cornish", "kernewek", "en_GB")):
            with self.subTest(language=code):
                def choose(**kwargs):
                    return languages.choose_language(self.database, TEST_KEY, jurisdiction, **kwargs)
                for spelling in (code, locale, english, native, locale.replace("_", "-")):
                    self.assertEqual(choose(requested=spelling), locale)
                self.assertEqual(choose(), default)
                self.assertEqual(choose(saved=locale), locale)
                output = io.StringIO()
                with mock.patch("builtins.input", return_value=native), mock.patch("sys.stdout", output):
                    self.assertEqual(choose(interactive=True), locale)
                self.assertIn(f"{english} — {native} (regional language; {locale})", output.getvalue())
                with mock.patch("builtins.input", return_value=""), mock.patch("sys.stdout", io.StringIO()):
                    self.assertEqual(choose(interactive=True), default)
        with self.assertRaisesRegex(ValueError, "unavailable"):
            languages.choose_language(self.database, TEST_KEY, "FR", requested="Cornish")
        with self.assertRaisesRegex(ValueError, "unavailable"):
            languages.choose_language(self.database, TEST_KEY, "GB-ENG", requested="Breton")

    def test_ps_and_il_language_menu_order_and_selection(self):
        for jurisdiction, locales, default in (
                ("PS", ("ar_PS", "en", "he", "lad", "yi"), "ar_PS"),
                ("IL", ("ar_IL", "en_IL", "he_IL", "lad", "yi"), "he_IL")):
            with self.subTest(jurisdiction=jurisdiction):
                def choose(**kwargs):
                    return languages.choose_language(self.database, TEST_KEY, jurisdiction, **kwargs)
                rows = self.query("select language, locale from calendar_jurisdiction_language where jurisdiction = "
                                  + calendar.sql_quote(jurisdiction) + " order by language;")
                self.assertEqual(rows, [{"language": language, "locale": locale}
                                       for language, locale in zip(("ar", "en", "he", "lad", "yi"), locales)])
                for number, name, locale in zip(("1", "2", "3", "4", "5"),
                                                ("Arabic", "English", "Hebrew", "Ladino", "Yiddish"), locales):
                    output = io.StringIO()
                    with mock.patch("builtins.input", return_value=number), mock.patch("sys.stdout", output):
                        self.assertEqual(choose(interactive=True), locale)
                    lines = [line.strip() for line in output.getvalue().splitlines() if line.startswith("  ")]
                    self.assertEqual([line.split(" — ")[0] for line in lines],
                                     ["1. Arabic", "2. English", "3. Hebrew", "4. Ladino", "5. Yiddish"])
                    self.assertEqual(choose(requested=name), locale)
                    self.assertEqual(choose(saved=locale), locale)
                self.assertEqual(choose(), default)
                self.assertEqual(choose(requested="Arabic", saved=locales[2]), locales[0])

    def test_community_languages_cover_towns_and_preserve_defaults(self):
        for town, jurisdiction, default in (
                ("Jerusalem", "IL", "he_IL"), ("West Jerusalem", "IL", "he_IL"),
                ("Gaza", "PS", "ar_PS"), ("New York City", "US-NY", "en_US")):
            with self.subTest(town=town):
                location = self.catalogue.resolve(town)
                self.assertEqual(location.jurisdiction, jurisdiction)
                def choose(**kwargs):
                    return languages.choose_language(self.database, TEST_KEY, jurisdiction,
                                                     timezone=location.timezone, **kwargs)
                self.assertEqual(choose(), default)
                for locale, english, native in (("yi", "Yiddish", "ייִדיש"),
                                                 ("lad", "Ladino", "Djudeo-espanyol")):
                    for spelling in (locale, english, native):
                        self.assertEqual(choose(requested=spelling), locale)
                    self.assertEqual(choose(saved=locale), locale)

    def test_location_language_choices_and_saved_values(self):
        towns = ("Al Bīrah", "Battir", "East Jerusalem", "Hebron", "Ma‘ale Adummim", "Nablus",
                 "Old City", "Qalqīlyah", "Ramallah", "Yaţţā", "Ţūlkarm")
        catalogue_towns = {place.name for places in self.catalogue.by_name.values() for place in places
                           if place.jurisdiction == "PS" and place.timezone != "Asia/Gaza"}
        self.assertEqual(catalogue_towns, set(towns))
        excluded = ("he", "Hebrew", "עברית", "yi", "Yiddish", "ייִדיש", "lad", "Ladino", "Djudeo-espanyol")
        for town in towns:
            location = self.catalogue.resolve(town)
            with self.subTest(town=town):
                def choose(**kwargs):
                    return languages.choose_language(self.database, TEST_KEY, location.jurisdiction,
                                                     timezone=location.timezone, **kwargs)
                for value in excluded:
                    with self.assertRaisesRegex(ValueError, "unavailable"):
                        choose(requested=value)
                for value in ("he", "yi", "lad"):
                    self.assertEqual(choose(saved=value), "ar_PS")
                self.assertEqual(choose(requested="English"), "en")
                output = io.StringIO()
                with mock.patch("builtins.input", return_value=""), mock.patch("sys.stdout", output):
                    self.assertEqual(choose(interactive=True, saved="yi"), "ar_PS")
                lines = [line.strip().split(" — ")[0] for line in output.getvalue().splitlines()
                         if line.startswith("  ")]
                self.assertEqual(lines, ["1. Arabic", "2. English"])
        output = io.StringIO()
        with mock.patch("builtins.input", side_effect=["3", "Hebrew", "Yiddish", "Ladino", "2"]), \
                mock.patch("sys.stdout", output):
            self.assertEqual(choose(interactive=True), "en")
        self.assertEqual(output.getvalue().count("Choose a listed language"), 4)
        for value in ("he", "yi", "lad"):
            self.assertEqual(languages.choose_language(self.database, TEST_KEY, "PS",
                             timezone="Asia/Gaza", requested=value), value)

    def test_installer_location_language_validation_preserves_existing_database(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-language-validation-") as temporary:
            database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True):
                with mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", "Yiddish",
                                                    "New York City"]):
                    self.assertEqual(installer.main(), 0)
                with mock.patch.object(sys, "argv", ["installer", "--noninteractive", "East Jerusalem"]):
                    self.assertEqual(installer.main(), 0)
                self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "ar_PS")
                rows = calendar.run_sql(database, TEST_KEY,
                    "select location, locale from calendar_local_settings;")
                self.assertEqual(rows, [{"location": "East Jerusalem", "locale": "ar_PS"}])
                database_before = database.read_bytes()
                config_before = installer.config_path().read_bytes()
                for value in ("Hebrew", "Yiddish", "Ladino"):
                    with self.subTest(language=value), mock.patch.object(sys, "argv",
                            ["installer", "--noninteractive", "--language", value, "Ramallah"]):
                        self.assertEqual(installer.main(), 1)
                    self.assertEqual(database.read_bytes(), database_before)
                    self.assertEqual(installer.config_path().read_bytes(), config_before)

    def test_patwa_town_choices_aliases_defaults_and_seed_reload(self):
        jamaican = [place for places in self.catalogue.by_name.values() for place in places
                    if place.jurisdiction == "JM"]
        self.assertEqual(len(jamaican), 11)
        british = [self.catalogue.resolve(town + ", " + jurisdiction)
                   for town in PATWA_TOWNS for jurisdiction in ("GB", "GB-ENG")]
        for place in jamaican + british:
            with self.subTest(location=place.label):
                self.assertEqual(languages.choose_language(self.database, TEST_KEY, place.jurisdiction,
                    requested="jam", timezone=place.timezone, town=place.name), "jam")
                default = languages.choose_language(self.database, TEST_KEY, place.jurisdiction, town=place.name)
                self.assertTrue(default.startswith("en"))
        for alias in ("Patwa", "Patwah", "Patois", "Jamaican Patois", "Jamaican Patwa", "Jamaican Patwah",
                      "Jamaican Creole", "Jamaican English Patois"):
            self.assertEqual(languages.choose_language(self.database, TEST_KEY, "JM", requested=alias), "jam")
        for jurisdiction, town in (("GB-ENG", "Shrewsbury"), ("GB", ""), ("GB-SCT", "Edinburgh"),
                                   ("US-NY", "London"), ("GB-ENG", "London'; drop table holiday_name; --")):
            with self.subTest(jurisdiction=jurisdiction, town=town):
                with self.assertRaisesRegex(ValueError, "unavailable"):
                    languages.choose_language(self.database, TEST_KEY, jurisdiction, requested="Patwa", town=town)
                self.assertNotEqual(languages.choose_language(self.database, TEST_KEY, jurisdiction,
                                    saved="jam", town=town), "jam")
        output = io.StringIO()
        with mock.patch("builtins.input", return_value="Patwa"), mock.patch("sys.stdout", output):
            self.assertEqual(languages.choose_language(self.database, TEST_KEY, "GB-ENG",
                             interactive=True, town="bIrMiNgHaM"), "jam")
        labels = [line.split(". ", 1)[1].split(" — ")[0] for line in output.getvalue().splitlines()
                  if line.startswith("  ")]
        self.assertEqual(labels, sorted(labels))
        self.assertEqual(labels.count("Jamaican Patois"), 1)
        query = "select * from calendar_town_language order by country, town_key, language;"
        before = self.query(query)
        self.assertEqual([(row["country"], row["town_key"], row["locale"]) for row in before],
                         [("GB", town.lower(), "jam") for town in PATWA_TOWNS])
        self.query(f".read {LOCALE_SCRIPT}\n")
        self.assertEqual(self.query(query), before)

    def test_patwa_holiday_labels_and_installation(self):
        before = self.query("select * from holiday_name where locale = 'jam' order by holiday_id;")
        self.assertEqual(len(before), 17)
        self.query(f".read {HOLIDAY_NAMES_SCRIPT}\n")
        self.assertEqual(self.query("select * from holiday_name where locale = 'jam' order by holiday_id;"), before)
        for town, jurisdiction in (("Kingston", "JM"), ("Birmingham", "GB-ENG")):
            with self.subTest(town=town), tempfile.TemporaryDirectory(prefix="mars-calendar-patwa-") as temporary:
                environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
                with mock.patch.dict(os.environ, environment, clear=True), mock.patch.object(sys, "argv",
                        ["installer", "--noninteractive", "--language", "Patwa", town + ", " + jurisdiction]):
                    self.assertEqual(installer.main(), 0)
                    self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "jam")
                database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                rows = calendar.run_sql(database, TEST_KEY, 'select FullDateAlternateKey, [Date Lingua], [Date Regional], '
                    '[Day Name], [Month Name], [Public Holiday], [Workday Type], [Working Day No] '
                    'from calendar_local order by FullDateAlternateKey;')
                original, weekends = calendar.native_policy(database, TEST_KEY, jurisdiction, 2015,
                                                              int(rows[-1]["FullDateAlternateKey"][:4]))
                labels = calendar.holiday_labels(database, TEST_KEY, jurisdiction, "jam")
                by_source = {source: name for (_, source), name in labels.items()}
                total = 0
                for row in rows:
                    key = row["FullDateAlternateKey"]
                    date = datetime.date.fromisoformat(key)
                    self.assertEqual(row["Date Lingua"], f"{date.day} {PATWA_MONTHS[date.month - 1]} {date.year}")
                    self.assertEqual(row["Day Name"], PATWA_DAYS[date.weekday()])
                    self.assertEqual(row["Month Name"], PATWA_MONTHS[date.month - 1])
                    workday = key not in original and not weekends[date.year, date.weekday()]
                    total += workday
                    self.assertEqual((row["Workday Type"], row["Working Day No"]), (int(workday), total))
                    if key in original:
                        self.assertEqual(set(row["Public Holiday"].split("; ")),
                                         {by_source.get(name, name) for name in original[key]})
                    else:
                        self.assertIsNone(row["Public Holiday"])
                    if key == "2026-12-25":
                        self.assertEqual(row["Public Holiday"], "Krismuss Deh")
                        self.assertIn("December", row["Date Regional"])
                    if key == "2026-08-06" and jurisdiction == "JM":
                        self.assertEqual(row["Public Holiday"], "Independence Deh")
                with mock.patch.dict(os.environ, environment, clear=True), mock.patch.object(sys, "argv",
                        ["installer", "--noninteractive"]):
                    self.assertEqual(installer.main(), 0)
                    self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "jam")

    def test_community_holiday_translations_cover_catalogues_and_preserve_policy(self):
        query = "select * from holiday_name order by holiday_name_id;"
        before = self.query(query)
        for jurisdiction, count in (("IL", 8), ("PS", 19), ("US", 12)):
            for locale in ("yi", "lad"):
                with self.subTest(jurisdiction=jurisdiction, locale=locale):
                    rows = self.query("select d.holiday_id, n.localized_name from holiday_definition d "
                        "left join holiday_name n on n.holiday_id = d.holiday_id and n.locale = "
                        + calendar.sql_quote(locale) + " where d.jurisdiction_id = "
                        + calendar.sql_quote(jurisdiction) + ";")
                    self.assertEqual(len(rows), count)
                    self.assertTrue(all(row["localized_name"] for row in rows))
                    target = "US-NY" if jurisdiction == "US" else jurisdiction
                    original, weekends = calendar.native_policy(self.database, TEST_KEY, target, 2026, 2026)
                    translated, translated_weekends = calendar.native_policy(
                        self.database, TEST_KEY, target, 2026, 2026, locale)
                    labels = calendar.holiday_labels(self.database, TEST_KEY, target, locale)
                    by_source = {source: name for (_, source), name in labels.items()}
                    self.assertTrue(original)
                    self.assertEqual(translated, {date: {by_source[name] for name in names}
                                                 for date, names in original.items()})
                    self.assertEqual(translated_weekends, weekends)
                    if jurisdiction == "IL":
                        self.assertEqual(translated["2026-04-02"], {"פּסח" if locale == "yi" else "Pesah"})
                    if jurisdiction == "US":
                        name = "אומאָפּהענגיקייט־טאָג" if locale == "yi" else "Diya de la independensya"
                        self.assertEqual(translated["2026-07-04"], {name})
                        suffix = " (פֿאַרלייגטער פֿײַערטאָג)" if locale == "yi" else " (fiesta trasladada)"
                        self.assertEqual(labels[1005125, "Independence Day (observed)"], name + suffix)
                        self.assertNotIn((1005125, "Independence Day (special closure)"), labels)
        self.query(f".read {HOLIDAY_NAMES_SCRIPT}\n")
        self.assertEqual(self.query(query), before)

    def test_installer_community_languages_and_regional_dates(self):
        for town, jurisdiction, regional in (("New York City", "US-NY", "July 4, 2026"),
                ("Jerusalem", "IL", "4 ביולי 2026"), ("Gaza", "PS", "٤ تموز ٢٠٢٦")):
            reference_policy = None
            for locale, date_local in (("yi", "4טן יולי 2026"), ("lad", "4 de djulyo de 2026")):
                with self.subTest(town=town, locale=locale), \
                        tempfile.TemporaryDirectory(prefix="mars-calendar-community-") as temporary:
                    environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
                    with mock.patch.dict(os.environ, environment, clear=True), \
                            mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", locale, town]):
                        self.assertEqual(installer.main(), 0)
                        self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], locale)
                    database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                    rows = calendar.run_sql(database, TEST_KEY,
                        'select FullDateAlternateKey, [Public Holiday], [Working Day No], [Workday Type], '
                        '[Date Regional], [Date Lingua], [Date UK] from calendar_local order by FullDateAlternateKey;')
                    policy = [(row["Working Day No"], row["Workday Type"]) for row in rows]
                    if reference_policy is not None:
                        self.assertEqual(policy, reference_policy)
                    reference_policy = policy
                    original, weekends = calendar.native_policy(database, TEST_KEY, jurisdiction, 2015,
                                                                int(rows[-1]["FullDateAlternateKey"][:4]))
                    total = 0
                    for row in rows:
                        key = row["FullDateAlternateKey"]
                        date = datetime.date.fromisoformat(key)
                        workday = key not in original and not weekends[date.year, date.weekday()]
                        total += workday
                        self.assertEqual((row["Workday Type"], row["Working Day No"]), (int(workday), total))
                        self.assertTrue(row["Date Lingua"].endswith(str(date.year)))
                        self.assertEqual(row["Public Holiday"] is not None, key in original)
                        if key == "2026-07-04":
                            self.assertEqual(row["Date Lingua"], date_local)
                            self.assertEqual(row["Date Regional"], regional)
                            self.assertEqual(row["Date UK"], "4ᵗʰ July 2026")
                        if row["Public Holiday"] and locale == "yi":
                            self.assertRegex(row["Public Holiday"], r"[\u0590-\u05ff]")
                        elif row["Public Holiday"]:
                            self.assertNotRegex(row["Public Holiday"], r"[\u0590-\u06ff]")

    def test_cajun_and_amish_menu_aliases_and_defaults(self):
        for town, jurisdiction in (("New Orleans", "US-LA"), ("Philadelphia", "US-PA"), ("New York City", "US-NY")):
            self.assertEqual(self.catalogue.resolve(town).jurisdiction, jurisdiction)
            def choose(**kwargs):
                return languages.choose_language(self.database, TEST_KEY, jurisdiction, **kwargs)
            self.assertEqual(choose(), "en_US")
            for locale, names in (("frc", ("Cajun French", "Cajun", "Louisiana French", "français cadien")),
                    ("pdc", ("Amish", "Pennsylvania German", "Pennsylvania Dutch", "Pennsilfaanisch Deitsch", "Deitsch"))):
                for name in (locale, *names):
                    self.assertEqual(choose(requested=name), locale)
                self.assertEqual(choose(saved=locale), locale)
                output = io.StringIO()
                with mock.patch("builtins.input", return_value=names[0]), mock.patch("sys.stdout", output):
                    self.assertEqual(choose(interactive=True), locale)
                labels = [line.split(". ", 1)[1].split(" — ")[0] for line in output.getvalue().splitlines()
                          if line.startswith("  ")]
                self.assertEqual(labels, sorted(labels))
                self.assertIn("additional language; " + locale, output.getvalue())
        for name in ("Amish", "Cajun"):
            with self.assertRaisesRegex(ValueError, "unavailable"):
                languages.choose_language(self.database, TEST_KEY, "NL", requested=name)

    def test_cajun_and_pennsylvania_holidays_and_installation(self):
        cases = (("frc", "Cajun French", "New Orleans", "US-LA", "4 de juillet 2026",
                  "Jour de l'An", "Fête de l'Indépendance", "Noël", " (jour férié reporté)"),
                 ("pdc", "Amish", "Philadelphia", "US-PA", "4. Tschulei 2026",
                  "Neiyaahrsdaag", "Der Viert vun Tschulei", "Grischtdaag", " (verschoowener Feierdaag)"))
        for locale, name, town, jurisdiction, date_local, new_year, independence, christmas, suffix in cases:
            with self.subTest(locale=locale), tempfile.TemporaryDirectory(prefix="mars-calendar-us-languages-") as temporary:
                environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
                with mock.patch.dict(os.environ, environment, clear=True), \
                        mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", name, town]):
                    self.assertEqual(installer.main(), 0)
                    self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], locale)
                database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                query = "select * from holiday_name where locale = " + calendar.sql_quote(locale) + " order by holiday_id;"
                before = calendar.run_sql(database, TEST_KEY, query)
                self.assertEqual(len(before), 12)
                self.assertTrue(all(row["localized_name"] and row["is_primary"] == "N" for row in before))
                calendar.run_sql(database, TEST_KEY, f".read {HOLIDAY_NAMES_SCRIPT}\n")
                self.assertEqual(calendar.run_sql(database, TEST_KEY, query), before)
                labels = calendar.holiday_labels(database, TEST_KEY, jurisdiction, locale)
                self.assertEqual(labels[1005125, "Independence Day (observed)"], independence + suffix)
                self.assertNotIn((1005125, "Independence Day (special closure)"), labels)
                original, weekends = calendar.native_policy(database, TEST_KEY, jurisdiction, 2026, 2026)
                translated, actual_weekends = calendar.native_policy(database, TEST_KEY, jurisdiction, 2026, 2026, locale)
                by_source = {source: target for (_, source), target in labels.items()}
                self.assertEqual(translated, {date: {by_source[source] for source in names}
                                             for date, names in original.items()})
                self.assertEqual(actual_weekends, weekends)
                self.assertEqual(translated["2026-01-01"], {new_year})
                self.assertEqual(translated["2026-07-04"], {independence})
                self.assertEqual(translated["2026-12-25"], {christmas})
                rows = calendar.run_sql(database, TEST_KEY, 'select [Date Lingua], [Date Regional], [Public Holiday] '
                    "from calendar_local where FullDateAlternateKey = '2026-07-04';")
                self.assertEqual(rows, [{"Date Lingua": date_local, "Date Regional": "July 4, 2026",
                                         "Public Holiday": independence}])

    def test_alaska_inupiaq_menu_inherits_country_choices(self):
        national = self.query("select language, locale, status from calendar_jurisdiction_language "
                              "where jurisdiction = 'US' order by language;")
        regional = self.query("select language, locale, status from calendar_jurisdiction_language "
                              "where jurisdiction = 'US-AK' order by language;")
        self.assertEqual(regional, sorted(national + [{"language": "ik", "locale": "ik", "status": "official_regional"}],
                                         key=lambda row: row["language"]))
        for town in ("Anchorage", "Fairbanks", "Juneau"):
            self.assertEqual(self.catalogue.resolve(town).jurisdiction, "US-AK")
        def choose(**kwargs):
            return languages.choose_language(self.database, TEST_KEY, "US-AK", **kwargs)
        for name in ("ik", "ipk", "Iñupiaq", "Inupiaq", "Iñupiatun", "Alaskan Inuit", "Iñupiaq (North Slope)"):
            self.assertEqual(choose(requested=name), "ik")
        self.assertEqual(choose(saved="ik"), "ik")
        self.assertEqual(choose(), "en_US")
        output = io.StringIO()
        with mock.patch("builtins.input", return_value="Iñupiaq"), mock.patch("sys.stdout", output):
            self.assertEqual(choose(interactive=True), "ik")
        labels = [line.split(". ", 1)[1].split(" — ")[0] for line in output.getvalue().splitlines()
                  if line.startswith("  ")]
        self.assertEqual(labels, sorted(labels))
        for jurisdiction, name in (("US-NY", "Iñupiaq"), ("US-AK", "Inuktitut"), ("US-AK", "Yup'ik")):
            with self.assertRaisesRegex(ValueError, "unavailable"):
                languages.choose_language(self.database, TEST_KEY, jurisdiction, requested=name)

    def test_installer_inupiaq_dates_and_attested_holiday_names(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-inupiaq-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), \
                    mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", "Iñupiaq", "Anchorage"]):
                self.assertEqual(installer.main(), 0)
                self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "ik")
            database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
            rows = calendar.run_sql(database, TEST_KEY, 'select FullDateAlternateKey, [Date Lingua], [Date Regional], '
                '[Month Name], [Day Name], [Public Holiday], [Working Day No] from calendar_local order by FullDateAlternateKey;')
            original, weekends = calendar.native_policy(database, TEST_KEY, "US-AK", 2015,
                                                        int(rows[-1]["FullDateAlternateKey"][:4]))
            translations = {"Christmas Day": "Kuraisimaġvik", "Thanksgiving Day": "Quyyavik"}
            total = 0
            for row in rows:
                key = row["FullDateAlternateKey"]
                date = datetime.date.fromisoformat(key)
                total += key not in original and not weekends[date.year, date.weekday()]
                self.assertEqual(row["Working Day No"], total)
                self.assertEqual(row["Date Lingua"], f"{INUPIAQ_MONTHS[date.month - 1]} {date.day}, {date.year}")
                self.assertEqual(row["Month Name"], INUPIAQ_MONTHS[date.month - 1])
                self.assertEqual(row["Day Name"], INUPIAQ_DAYS[date.weekday()])
                expected = {translations.get(name, name) for name in original.get(key, ())}
                self.assertEqual(set(row["Public Holiday"].split("; ")) if row["Public Holiday"] else set(), expected)
                if key == "2026-07-04":
                    self.assertEqual(row["Date Regional"], "July 4, 2026")
                if key == "2026-11-26":
                    self.assertEqual(row["Public Holiday"], "Quyyavik")
                if key == "2026-12-25":
                    self.assertEqual(row["Public Holiday"], "Kuraisimaġvik")
            query = "select * from holiday_name where locale = 'ik' order by holiday_id;"
            before = calendar.run_sql(database, TEST_KEY, query)
            self.assertEqual(len(before), 2)
            calendar.run_sql(database, TEST_KEY, f".read {HOLIDAY_NAMES_SCRIPT}\n")
            self.assertEqual(calendar.run_sql(database, TEST_KEY, query), before)

    def test_town_menu_disambiguates_and_keeps_accent_collisions(self):
        places = [calendar.Location("Twin", "NL", 52, 4, "Europe/Amsterdam"),
                  calendar.Location("Twín", "GB-ENG", 53, -2, "Europe/London")]
        with mock.patch.dict(self.catalogue.by_name, {"twin": places}):
            with self.assertRaisesRegex(ValueError, "Ambiguous"):
                self.catalogue.resolve("Twin")
            with mock.patch("builtins.input", side_effect=["2"]):
                self.assertEqual(calendar.choose_location(self.catalogue, "Twin", "", True).jurisdiction, "NL")

    def test_explicit_locale_controls_view_without_changing_policy(self):
        try:
            self.query("update calendar_local_settings set locale = 'la_VA';")
            row = self.query('select "Day Name", "Month Name Abbrev", [Date UK], "Public Holiday" from calendar_local '
                             "where FullDateAlternateKey = '2024-06-21';")[0]
            self.assertEqual(row, {"Day Name": "Veneris", "Month Name Abbrev": "Iun",
                                   "Date UK": "21ˢᵗ June 2024", "Public Holiday": self.rows["2024-06-21"]["Public Holiday"]})
        finally:
            self.query("update calendar_local_settings set locale = null;")

    def test_installer_limmen_saved_location_and_failed_replacement(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-install-") as temporary:
            home = Path(temporary) / "mars"
            database = home / "jurisdiction/mars_jurisdiction_rules.db"
            environment = {"MARS_HOME": str(home), "MARS_JURISDICTION_DB_KEY": TEST_KEY,
                           "LANG": "en_GB.UTF-8"}
            # Run the real installer with no access to the user's private configuration.
            with mock.patch.dict(os.environ, environment, clear=True):
                with mock.patch.object(sys, "argv", ["installer", "--noninteractive", "limmen"]):
                    self.assertEqual(installer.main(), 0)
                config = installer.read_config(installer.config_path())
                self.assertEqual(config[calendar.LOCATION_ENV], "Limmen, NL")
                self.assertEqual(config[languages.LANGUAGE_ENV], "nl_NL")
                self.assertEqual(database.stat().st_mode & 0o777, 0o600)
                settings = calendar.run_sql(database, TEST_KEY, "select * from calendar_local_settings;")[0]
                self.assertEqual((settings["location"], settings["jurisdiction"], settings["timezone"]),
                                 ("Limmen", "NL", "Europe/Amsterdam"))
                # The chosen location, not the English host LANG, controls every
                # ordinary month/day label. Date UK explicitly remains British English.
                local_rows = calendar.run_sql(database, TEST_KEY,
                    'select FullDateAlternateKey, "Day Name", "Month Name Abbrev", "ME Dates Text", '
                    '"WE Dates Text", [Date UK] from calendar_local order by FullDateAlternateKey;')
                month_names = ("jan", "feb", "mrt", "apr", "mei", "jun", "jul", "aug", "sep", "okt", "nov", "dec")
                day_names = ("maandag", "dinsdag", "woensdag", "donderdag", "vrijdag", "zaterdag", "zondag")
                for row in local_rows:
                    date = datetime.date.fromisoformat(row["FullDateAlternateKey"])
                    week_end = date + datetime.timedelta(days=6 - date.weekday())
                    self.assertEqual(row["Day Name"], day_names[date.weekday()])
                    self.assertEqual(row["Month Name Abbrev"], month_names[date.month - 1])
                    self.assertEqual(row["ME Dates Text"], f"{month_names[date.month - 1]} {date.year}")
                    self.assertEqual(row["WE Dates Text"],
                                     f"{week_end.day:02d} {month_names[week_end.month - 1]} {week_end.year}")
                june = next(row for row in local_rows if row["FullDateAlternateKey"] == "2024-06-21")
                self.assertEqual(june["Date UK"], "21ˢᵗ June 2024")
                rows = calendar.run_sql(database, TEST_KEY, """
select FullDateAlternateKey, "Public Holiday", Sunrise
from calendar_local where FullDateAlternateKey in ('2025-04-26', '2025-05-26', '2024-06-21')
order by FullDateAlternateKey;
""")
                self.assertIn("Koningsdag", rows[1]["Public Holiday"])
                self.assertIsNone(rows[2]["Public Holiday"])
                self.assertNotEqual(rows[0]["Sunrise"], self.rows["2024-06-21"]["Sunrise"])
                offsets = calendar.run_sql(database, TEST_KEY, """
select utc_offset_hours from calendar_local_days
where calendar_date in ('2024-01-01', '2024-06-21') order by calendar_date;
""")
                self.assertEqual(offsets, [{"utc_offset_hours": 1}, {"utc_offset_hours": 2}])
                with mock.patch.object(sys, "argv", ["installer", "--noninteractive"]):
                    self.assertEqual(installer.main(), 0)
                self.assertEqual(installer.read_config(installer.config_path())[calendar.LOCATION_ENV], "Limmen, NL")
                self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "nl_NL")
                previous = database.read_bytes()
                config_before = installer.config_path().read_bytes()
                with mock.patch.object(sys, "argv", ["installer", "--noninteractive", "no-such-town-9182"]):
                    self.assertEqual(installer.main(), 1)
                self.assertEqual(database.read_bytes(), previous)
                self.assertEqual(installer.config_path().read_bytes(), config_before)
                with mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", "Latin", "limmen"]):
                    self.assertEqual(installer.main(), 1)
                self.assertEqual(database.read_bytes(), previous)
                self.assertEqual(installer.config_path().read_bytes(), config_before)

    def test_holiday_translations_preserve_dates_and_source_names(self):
        original, weekends = calendar.native_policy(self.database, TEST_KEY, "IL", 2026, 2026)
        arabic = {
            "יום העצמאות": "يوم الاستقلال", "יום כיפור": "يوم الغفران", "סוכות": "عيد المظال",
            "פסח": "عيد الفصح اليهودي", "ראש השנה": "رأس السنة العبرية", "שבועות": "عيد الأسابيع",
            "שביעי של פסח": "اليوم السابع من عيد الفصح اليهودي", "שמחת תורה/שמיני עצרת": "فرحة التوراة / شميني عتسيرت",
        }
        english = dict(zip(arabic, ("Independence Day", "Yom Kippur", "Sukkot", "Passover", "Rosh Hashanah",
                                   "Shavuot", "Seventh day of Passover", "Simchat Torah / Shemini Atzeret")))
        self.assertEqual(set().union(*original.values()), set(arabic))
        for locale, translations in (("ar_IL", arabic), ("ar-PS", arabic), ("en_IL", english),
                                      ("en-US", english), ("he_IL", {}), ("la_VA", {})):
            with self.subTest(locale=locale):
                names, actual_weekends = calendar.native_policy(self.database, TEST_KEY, "IL", 2026, 2026, locale)
                self.assertEqual(names, {date: {translations.get(name, name) for name in values}
                                         for date, values in original.items()})
                self.assertEqual(actual_weekends, weekends)
        query = "select * from holiday_name where holiday_id between 1002192 and 1002199 order by holiday_name_id;"
        before = self.query(query)
        self.assertFalse(any(row["locale"] == "en_US" for row in before))
        self.query(f".read {HOLIDAY_NAMES_SCRIPT}\n")
        self.assertEqual(self.query(query), before)

    def test_holiday_translation_precedence_inheritance_and_unmatched_names(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-translations-") as temporary:
            database = Path(temporary) / "jurisdiction.db"
            shutil.copyfile(self.database, database)
            calendar.run_sql(database, TEST_KEY, """
insert into jurisdiction_entity values ('TEST-CHILD');
insert into jurisdiction_parent_jurisdiction_id values ('TEST-CHILD', 'GB-ENG');
insert into holiday_name(holiday_id, locale, localized_name, is_primary) values
    (1, 'ar', 'رأس السنة', 'N'),
    (1, 'ar-GB', 'رأس السنة الميلادية', 'N');
insert into holiday_event_localized_name values
    (1, 'ar', 'Named closure', 'عام'),
    (1, 'ar-GB', 'Named closure', 'خاص');
""")
            exact = calendar.holiday_labels(database, TEST_KEY, "TEST-CHILD", "AR_gb")
            base = calendar.holiday_labels(database, TEST_KEY, "TEST-CHILD", "ar_PS")
            self.assertEqual(exact[1, "New Years Day"], "رأس السنة الميلادية")
            self.assertEqual(base[1, "New Years Day"], "رأس السنة")
            self.assertEqual(exact[1, "Named closure"], "خاص")
            self.assertEqual(base[1, "Named closure"], "عام")
            self.assertNotIn((1, "Unknown closure"), exact)
            self.assertNotIn((2, "New Years Day"), exact)
            self.assertNotIn((1, "New Years Day (substitute day)"), exact)
            # Untranslated event-specific/observed names must not be replaced
            # with a base name which silently loses their qualification.
            original, weekends = calendar.native_policy(database, TEST_KEY, "GB-ENG", 2022, 2022)
            translated, translated_weekends = calendar.native_policy(database, TEST_KEY, "GB-ENG", 2022, 2022, "ar_GB")
            self.assertEqual(translated, {date: {exact.get((1, name), name) for name in names}
                                          for date, names in original.items()})
            self.assertEqual(weekends, translated_weekends)
            self.assertEqual(calendar.holiday_labels(database, TEST_KEY, "GB-ENG", ""), {})

    def test_spanish_holiday_translations_preserve_policy_and_source_names(self):
        labels = calendar.holiday_labels(self.database, TEST_KEY, "US", "es_US")
        base_names = {source: translated for (_, source), translated in labels.items()
                      if source in SPANISH_HOLIDAYS}
        self.assertEqual(base_names, SPANISH_HOLIDAYS)
        for jurisdiction, year, locale in (("US", 2026, "es_US"), ("US-NY", 2027, "es-US"),
                                           ("US", 1944, "es"), ("US", 2026, "es_MX")):
            with self.subTest(jurisdiction=jurisdiction, year=year, locale=locale):
                original, weekends = calendar.native_policy(self.database, TEST_KEY, jurisdiction, year, year)
                translated, translated_weekends = calendar.native_policy(self.database, TEST_KEY, jurisdiction,
                                                                          year, year, locale)
                self.assertTrue(original)
                self.assertEqual(translated, {date: {SPANISH_HOLIDAYS[name] for name in names}
                                             for date, names in original.items()})
                self.assertEqual(translated_weekends, weekends)
        query = "select * from holiday_name where holiday_id between 1005122 and 1005133 order by holiday_name_id;"
        before = self.query(query)
        qualifiers = self.query("select * from holiday_name_qualifier order by locale, source_suffix;")
        self.query(f".read {HOLIDAY_NAMES_SCRIPT}\n")
        self.assertEqual(self.query(query), before)
        self.assertEqual(self.query("select * from holiday_name_qualifier order by locale, source_suffix;"), qualifiers)
        self.assertEqual({row["localized_name"] for row in before if row["is_primary"] == "Y"}, set(SPANISH_HOLIDAYS))

    def test_translated_qualifiers_preserve_observance_and_unknown_names(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-qualifiers-") as temporary:
            database = Path(temporary) / "jurisdiction.db"
            shutil.copyfile(self.database, database)
            # A private fixture exercises qualified events without changing packaged holiday policy.
            calendar.run_sql(database, TEST_KEY, """
insert into holiday_observance_rule(holiday_id, holiday_rule_id, observed_rule_kind, observed_name,
                                    weekend_mask, suppress_original, valid_from_year, valid_to_year, priority)
values (1005125, 3279799, 'previous_weekday', 'Independence Day (observed)', '6,7', 'N', 2026, 2026, 100);
""")
            original, weekends = calendar.native_policy(database, TEST_KEY, "US", 2026, 2026)
            translated, translated_weekends = calendar.native_policy(database, TEST_KEY, "US", 2026, 2026, "es_US")
            self.assertEqual(original["2026-07-03"], {"Independence Day (observed)"})
            self.assertEqual(translated["2026-07-03"], {"Día de la Independencia (día de observancia)"})
            self.assertEqual(translated["2026-07-04"], {"Día de la Independencia"})
            self.assertEqual(set(translated), set(original))
            self.assertEqual(translated_weekends, weekends)
            labels = calendar.holiday_labels(database, TEST_KEY, "US", "es_US")
            self.assertNotIn((1005125, "Independence Day (special closure)"), labels)
            self.assertNotIn((1005125, "Unknown Day (observed)"), labels)
            calendar.run_sql(database, TEST_KEY, """
insert into holiday_name_qualifier values ('es_US', ' (observed)', ' (festivo trasladado)');
""")
            exact = calendar.holiday_labels(database, TEST_KEY, "US", "ES-us")
            self.assertEqual(exact[1005125, "Independence Day (observed)"],
                             "Día de la Independencia (festivo trasladado)")
            base = calendar.holiday_labels(database, TEST_KEY, "US", "es_MX")
            self.assertEqual(base[1005125, "Independence Day (observed)"],
                             "Día de la Independencia (día de observancia)")

    def test_installer_spanish_localises_public_holidays(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-spanish-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), \
                    mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", "Spanish", "New York City"]):
                self.assertEqual(installer.main(), 0)
            database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
            rows = calendar.run_sql(database, TEST_KEY,
                'select FullDateAlternateKey, [Public Holiday], [Workday Type], [Working Day No], [Date Lingua], [Date UK] '
                'from calendar_local order by FullDateAlternateKey;')
            original, weekends = calendar.native_policy(database, TEST_KEY, "US-NY", 2015,
                                                         int(rows[-1]["FullDateAlternateKey"][:4]))
            total = 0
            for row in rows:
                key = row["FullDateAlternateKey"]
                date = datetime.date.fromisoformat(key)
                is_workday = key not in original and not weekends[date.year, date.weekday()]
                total += is_workday
                self.assertEqual(row["Workday Type"], int(is_workday))
                self.assertEqual(row["Working Day No"], total)
                expected = {SPANISH_HOLIDAYS[name] for name in original.get(key, ())}
                self.assertEqual(set(row["Public Holiday"].split("; ")) if row["Public Holiday"] else set(), expected)
                if key == "2026-01-01":
                    self.assertEqual(row["Date Lingua"], "1 de enero de 2026")
                    self.assertEqual(row["Date UK"], "1ˢᵗ January 2026")
                    self.assertEqual(row["Public Holiday"], "Día de Año Nuevo")

    def test_installer_arabic_localises_public_holidays(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-arabic-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), \
                    mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", "Arabic", "Haifa"]):
                self.assertEqual(installer.main(), 0)
                self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "ar_IL")
            database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
            rows = calendar.run_sql(database, TEST_KEY,
                'select FullDateAlternateKey, [Public Holiday], [Workday Type], [Working Day No], [Date Lingua], [Date UK] '
                'from calendar_local order by FullDateAlternateKey;')
            original, weekends = calendar.native_policy(database, TEST_KEY, "IL", 2015,
                                                         int(rows[-1]["FullDateAlternateKey"][:4]))
            total = 0
            for row in rows:
                text = row["FullDateAlternateKey"]
                date = datetime.date.fromisoformat(text)
                is_workday = text not in original and not weekends[date.year, date.weekday()]
                total += is_workday
                self.assertEqual(row["Workday Type"], int(is_workday))
                self.assertEqual(row["Working Day No"], total)
                self.assertEqual(row["Public Holiday"] is not None, text in original)
                self.assertNotRegex(row["Date Lingua"], r"[0-9]")
                self.assertRegex(row["Date Lingua"], r"[٠-٩]")
                if row["Public Holiday"]:
                    self.assertRegex(row["Public Holiday"], r"[\u0600-\u06ff]")
                    self.assertNotRegex(row["Public Holiday"], r"[\u0590-\u05ff]")
                if text == "2026-04-02":
                    self.assertEqual(row["Public Holiday"], "عيد الفصح اليهودي")
                    self.assertEqual(row["Date Lingua"], "١٤ شوال ١٤٤٧")
                    self.assertEqual(row["Date UK"], "2ⁿᵈ April 2026")

    def test_installer_hebrew_uses_jewish_dates_and_hebrew_holidays(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-hebrew-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), \
                    mock.patch.object(sys, "argv", ["installer", "--noninteractive", "--language", "Hebrew", "Haifa"]):
                self.assertEqual(installer.main(), 0)
                self.assertEqual(installer.read_config(installer.config_path())[languages.LANGUAGE_ENV], "he_IL")
            database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
            rows = calendar.run_sql(database, TEST_KEY,
                'select FullDateAlternateKey, [Public Holiday], [Workday Type], [Working Day No], [Date Lingua], [Date UK] '
                'from calendar_local order by FullDateAlternateKey;')
            original, weekends = calendar.native_policy(database, TEST_KEY, "IL", 2015,
                                                         int(rows[-1]["FullDateAlternateKey"][:4]))
            total = 0
            for row in rows:
                key = row["FullDateAlternateKey"]
                date = datetime.date.fromisoformat(key)
                is_workday = key not in original and not weekends[date.year, date.weekday()]
                total += is_workday
                self.assertEqual(row["Workday Type"], int(is_workday))
                self.assertEqual(row["Working Day No"], total)
                self.assertEqual(row["Public Holiday"] is not None, key in original)
                self.assertRegex(row["Date Lingua"], r"^\d{1,2} ב[\u0590-\u05ff ]+ 5\d{3}$")
                if row["Public Holiday"]:
                    self.assertRegex(row["Public Holiday"], r"[\u0590-\u05ff]")
                if key == "2026-04-02":
                    self.assertEqual(row["Public Holiday"], "פסח")
                    self.assertEqual(row["Date Lingua"], "15 בניסן 5786")
                    self.assertEqual(row["Date UK"], "2ⁿᵈ April 2026")

    def test_make_location_argument_does_not_mask_other_targets(self):
        for arguments, success in ((["install-jurisdiction-db", "limmen"], True),
                                   (["install-jurisdiction-db"], True),
                                   (["install-jurisdiction-db", "LOCATION=New York, US-NY"], True),
                                   (["install-jurisdiction-db", "clean"], False),
                                   (["install-jurisdiction-db", "release-evidence"], False),
                                   (["install-jurisdiction-db", "test_jurisdiction"], False),
                                   (["install-jurisdiction-db", "limmen", "test"], False),
                                   (["not-a-mars-target"], False)):
            with self.subTest(arguments=arguments):
                result = subprocess.run(["make", "-n", "-j1", *arguments], cwd=ROOT,
                                        text=True, capture_output=True, timeout=30)
                self.assertEqual(result.returncode == 0, success, result.stderr)
                if success:
                    self.assertIn("configure_mars_lab_jurisdiction_db.py", result.stdout)

    def test_interactive_installer_saves_latin_and_cancellation_preserves_database(self):
        with tempfile.TemporaryDirectory(prefix="mars-calendar-menu-") as temporary:
            environment = {"MARS_HOME": temporary, "MARS_JURISDICTION_DB_KEY": TEST_KEY, "LANG": "C"}
            with mock.patch.dict(os.environ, environment, clear=True), \
                    mock.patch.object(sys, "argv", ["installer"]), \
                    mock.patch.object(sys.stdin, "isatty", return_value=True), \
                    mock.patch("getpass.getpass", return_value=""), \
                    mock.patch("builtins.input", side_effect=["Vatican City", "1", "2"]):
                self.assertEqual(installer.main(), 0)
                config = installer.read_config(installer.config_path())
                self.assertEqual(config[calendar.LOCATION_ENV], "Vatican City, VA")
                self.assertEqual(config[languages.LANGUAGE_ENV], "la_VA")
                database = installer.default_db_path()
                previous_database = database.read_bytes()
                previous_config = installer.config_path().read_bytes()
                # Reuse the saved town, then cancel at the language menu.
                with mock.patch("builtins.input", side_effect=["", "q"]):
                    self.assertEqual(installer.main(), 1)
                self.assertEqual(database.read_bytes(), previous_database)
                self.assertEqual(installer.config_path().read_bytes(), previous_config)

    def test_complete_date_range_without_duplicates(self):
        first = datetime.date(2015, 1, 1)
        last = datetime.date(datetime.date.today().year + 7, 12, 31)
        self.assertEqual(min(self.rows), first.isoformat())
        self.assertEqual(max(self.rows), last.isoformat())
        self.assertEqual(self.row_count, (last - first).days + 1)
        self.assertEqual(self.row_count, len(self.rows))

    def test_locale_seed_covers_catalogue_and_other_languages(self):
        missing = self.query("""
select distinct t.jurisdiction_id from jurisdiction_town as t
left join calendar_territory_locale as l on l.territory = substr(t.jurisdiction_id, 1, 2)
where l.locale is null;
""")
        self.assertEqual(missing, [])
        counts = self.query("""
select locale from calendar_territory_locale
where (select count(*) from calendar_locale_month as m where m.locale = calendar_territory_locale.locale) != 12
   or (select count(*) from calendar_locale_weekday as d where d.locale = calendar_territory_locale.locale) != 7;
""")
        self.assertEqual(counts, [])
        # Verify the view itself, not merely the data, also handles non-Dutch locales.
        try:
            for jurisdiction, weekday, month, week_text in (
                    ("FR", "mardi", "déc.", "05 janv. 2025"),
                    ("CA-QC", "mardi", "déc.", "05 janv. 2025"),
                    ("DE", "Dienstag", "Dez.", "05 Jan. 2025"),
                    ("NL", "dinsdag", "dec", "05 jan 2025")):
                self.query("update calendar_local_settings set jurisdiction = " + calendar.sql_quote(jurisdiction) + ";")
                row = self.query('select "Day Name", "Month Name Abbrev", "WE Dates Text", [Date UK] from calendar_local '
                                 "where FullDateAlternateKey = '2024-12-31';")[0]
                self.assertEqual(row, {"Day Name": weekday, "Month Name Abbrev": month, "WE Dates Text": week_text,
                                      "Date UK": "31ˢᵗ December 2024"})
        finally:
            self.query("update calendar_local_settings set jurisdiction = 'GB-ENG';")

    def test_shared_locale_names_are_isolated_and_deduplicated(self):
        tables = {
            name: [list(row.values()) for row in self.query("select " +
                ("locale, weekday_no, day_name" if name == "calendar_locale_weekday" else "*") + " from " + name +
                (" where length(territory) = 2" if name == "calendar_territory_locale" else
                 " where locale in (select locale from calendar_territory_locale where length(territory) = 2) "
                 "or locale = 'en_GB'") + " order by 1, 2;")]
            for name in ("calendar_territory_locale", "calendar_locale_month", "calendar_locale_weekday")
        }
        # Original 249 mappings/245 locales, with CLDR aliases resolved in isolated
        # dictionaries. Shared Babel caches previously leaked parent abbreviations.
        digest = hashlib.sha256(json.dumps(tables, ensure_ascii=False).encode()).hexdigest()
        self.assertEqual(digest, "96648e3c749f92cb73f50e809ceba5b3e4730671230074f137c9a1e42121645f")
        self.assertEqual(self.query("select short_name, full_name from calendar_locale_month "
                                   "where locale = 'ar_DZ' and month_no = 1;"),
                         [{"short_name": "جانفي", "full_name": "جانفي"}])
        # Canadian French really differs from standard French; do not merge it.
        self.assertEqual(self.query("select locale, short_name from calendar_locale_month "
                                   "where locale in ('fr_CA', 'fr_FR') and month_no = 7 order by locale;"),
                         [{"locale": "fr_CA", "short_name": "juill."}, {"locale": "fr_FR", "short_name": "juil."}])
        for table, count in (("calendar_month_names", 12), ("calendar_weekday_names", 7)):
            sets = {}
            for row in self.query("select * from " + table + " order by 1, 2;"):
                values = list(row.values())
                sets.setdefault(values[0], []).append(tuple(values[1:]))
            signatures = [tuple(rows) for rows in sets.values()]
            self.assertEqual(len(signatures), len(set(signatures)), table)
            self.assertTrue(all(len(rows) == count for rows in sets.values()), table)
            self.assertLess(len(sets), 245)
        mappings = {row["locale"]: row for row in self.query("select * from calendar_locale_name_set;")}
        self.assertEqual(mappings["en_GB"]["weekday_set"], mappings["en_US"]["weekday_set"])
        self.assertNotEqual(mappings["en_GB"]["month_set"], mappings["en_US"]["month_set"])
        original_locales = {row[0] for row in tables["calendar_locale_month"]}
        french = [row for locale, row in mappings.items() if locale.startswith("fr_") and locale in original_locales]
        self.assertGreater(len(french), 1)
        self.assertEqual({row["weekday_set"] for row in french}, {"fr"})
        self.assertEqual({row["month_set"] for row in french}, {"fr"})
        # Loading the seed again must preserve the same public lookup interfaces.
        self.query(f".read {LOCALE_SCRIPT}\n")
        self.assertEqual(self.query("select count(*) as total from calendar_locale_month;"), [{"total": 12 * len(mappings)}])

    def test_original_projection_with_display_dates_appended(self):
        self.assertEqual(list(self.rows["2024-01-01"]), [
            "FullDateAlternateKey", "FullDate and End Time", "Year", "Month", "Day",
            "Days in Month", "MC Dates", "ME Dates", "ME Dates Text", "WC Dates", "WE Dates",
            "WEDates (Friday)", "WE Dates plus End Time", "WE Dates Text", "Day of Week",
            "Day Name", "Month Name", "Fiscal Year End", "Fiscal Month", "Fiscal Quarter No",
            "Fiscal Qtr", "Fiscal Quarter Name", "Fiscal Years", "Public Holiday", "Non working day Type", "Workday Type",
            "Working Day No", "Day Type", "Sunrise", "Sunset", "Moon Phase %", "Date UK", "Month Name Abbrev", "Day Name Abbrev", "Date Lingua", "Date Regional",
        ])

    def test_date_uk_uses_full_month_and_superscript_ordinal(self):
        months = ("January", "February", "March", "April", "May", "June",
                  "July", "August", "September", "October", "November", "December")
        suffixes = (
            "ˢᵗ", "ⁿᵈ", "ʳᵈ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ",
            "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ",
            "ˢᵗ", "ⁿᵈ", "ʳᵈ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ˢᵗ",
        )
        for date_text, row in self.rows.items():
            with self.subTest(date=date_text):
                day = datetime.date.fromisoformat(date_text)
                expected = f"{day.day}{suffixes[day.day - 1]} {months[day.month - 1]} {day.year}"
                self.assertEqual(row["Date UK"], expected)
                self.assertEqual(row["Date Lingua"], expected)

    def test_calendar_and_fiscal_fields_for_every_date(self):
        for date_text, row in self.rows.items():
            with self.subTest(date=date_text):
                day = datetime.date.fromisoformat(date_text)
                monday = day - datetime.timedelta(days=day.weekday())
                sunday = monday + datetime.timedelta(days=6)
                next_month = (day.replace(day=28) + datetime.timedelta(days=4)).replace(day=1)
                month_end = next_month - datetime.timedelta(days=1)
                fiscal_end = day.year + (day.month > 3)
                fiscal_month = (day.month + 8) % 12 + 1
                self.assertEqual((row["Year"], row["Month"], row["Day"]),
                                 (day.year, day.month, day.day))
                self.assertEqual(row["FullDate and End Time"], date_text + " 23:59:59")
                self.assertEqual(row["Days in Month"], month_end.day)
                self.assertEqual(row["MC Dates"], day.replace(day=1).isoformat() + " 00:00:00")
                self.assertEqual(row["ME Dates"], month_end.isoformat() + " 23:59:59")
                self.assertEqual(row["WC Dates"], monday.isoformat())
                self.assertEqual(row["WE Dates"], sunday.isoformat())
                self.assertEqual(row["WEDates (Friday)"], (monday + datetime.timedelta(days=4)).isoformat())
                self.assertEqual(row["WE Dates plus End Time"], sunday.isoformat() + " 23:59:59")
                self.assertEqual(row["Day of Week"], day.weekday())
                self.assertEqual(row["Fiscal Year End"], fiscal_end)
                self.assertEqual(row["Fiscal Month"], fiscal_month)
                self.assertEqual(row["Fiscal Quarter No"], (fiscal_month - 1) // 3 + 1)
                self.assertEqual(row["Fiscal Qtr"], f"Qtr {(fiscal_month - 1) // 3 + 1}")
                self.assertEqual(row["Fiscal Quarter Name"], f"{fiscal_end:04d} Q{(fiscal_month - 1) // 3 + 1}")
                self.assertEqual(row["Fiscal Years"], f"{(fiscal_end - 1) % 100:02d}-{fiscal_end % 100:02d}")

    def test_bank_holidays_match_tsql_reference(self):
        # The five-year example accompanying ft_BankHolidays in date_arithmetic.sql.
        expected = {
            2019: "01-01 04-19 04-22 05-06 05-27 08-26 12-25 12-26",
            2020: "01-01 04-10 04-13 05-08 05-25 08-31 12-25 12-28",
            2021: "01-01 04-02 04-05 05-03 05-31 08-30 12-27 12-28",
            2022: "01-03 04-15 04-18 05-02 06-02 06-03 08-29 09-19 12-26 12-27",
            2023: "01-02 04-07 04-10 05-01 05-08 05-29 08-28 12-25 12-26",
        }
        for year, dates in expected.items():
            with self.subTest(year=year):
                actual = {date for date, row in self.rows.items()
                          if row["Year"] == year and row["Public Holiday"] is not None}
                self.assertEqual(actual, {f"{year}-{date}" for date in dates.split()})
        self.assertIsNone(self.rows["2020-05-04"]["Public Holiday"])
        self.assertIsNone(self.rows["2022-05-30"]["Public Holiday"])
        self.assertEqual(self.rows["2022-06-03"]["Public Holiday"], "Platinum Jubilee Bank Holiday")
        self.assertEqual(self.rows["2022-09-19"]["Public Holiday"], "State Funeral of Queen Elizabeth II")
        self.assertEqual(self.rows["2023-05-08"]["Public Holiday"], "Coronation of King Charles III")

    def test_working_day_number_is_cumulative_for_every_date_and_filtered_queries(self):
        total = 0
        for date_text, row in sorted(self.rows.items()):
            total += int(row["Day of Week"] < 5 and row["Public Holiday"] is None)
            with self.subTest(date=date_text):
                self.assertIsInstance(row["Working Day No"], int)
                self.assertEqual(row["Working Day No"], total)
        # Caller filters, ordering and LIMIT must not renumber the selected days.
        for predicate in ("FullDateAlternateKey >= '2024-12-28'", "[Workday Type] = 0"):
            rows = self.query('select FullDateAlternateKey, [Working Day No] from calendar_local where ' +
                              predicate + ' order by FullDateAlternateKey desc limit 10;')
            self.assertTrue(rows)
            for row in rows:
                self.assertEqual(row["Working Day No"], self.rows[row["FullDateAlternateKey"]]["Working Day No"])

    def test_substitute_days_and_workday_flags(self):
        for row in self.rows.values():
            weekend = row["Day of Week"] >= 5
            holiday = row["Public Holiday"] is not None
            self.assertEqual(row["Workday Type"], int(not (weekend or holiday)))
            self.assertEqual(row["Day Type"], "NonWorkDay" if weekend or holiday else "WorkDay")
            self.assertEqual(row["Non working day Type"],
                             "Weekend" if weekend else "Public Holiday" if holiday else None)
            if holiday:
                self.assertFalse(weekend)
        for year in range(2015, datetime.date.today().year + 8):
            for month, day, name in ((1, 1, "New Years Day"), (12, 25, "Christmas Day"),
                                     (12, 26, "Boxing Day")):
                actual = datetime.date(year, month, day)
                observed = actual
                while observed.weekday() >= 5:
                    observed += datetime.timedelta(days=1)
                if (month, day) == (12, 25) and actual.weekday() >= 5:
                    observed = datetime.date(year, 12, 27)
                if (month, day) == (12, 26) and actual.weekday() in (5, 6):
                    observed = datetime.date(year, 12, 28)
                label = name if actual == observed else "Bank Holiday in Lieu of " + name
                self.assertEqual(self.rows[observed.isoformat()]["Public Holiday"], label)

    def test_english_labels_and_week_crossing_year(self):
        row = self.rows["2024-12-31"]
        self.assertEqual(row["Day Name"], "Tuesday")
        self.assertEqual(row["Month Name Abbrev"], "Dec")
        self.assertEqual(row["ME Dates Text"], "Dec 2024")
        self.assertEqual(row["WE Dates Text"], "05 Jan 2025")

    def test_solar_times_and_daylight_saving(self):
        # Minute-rounded results of the original Shrewsbury solar model.
        self.assertEqual((self.rows["2024-06-21"]["Sunrise"], self.rows["2024-06-21"]["Sunset"]),
                         ("04:47:00", "21:39:00"))
        self.assertEqual((self.rows["2024-12-21"]["Sunrise"], self.rows["2024-12-21"]["Sunset"]),
                         ("08:21:00", "15:58:00"))
        for before, after, change in (("2024-03-30", "2024-03-31", 60),
                                       ("2024-10-26", "2024-10-27", -60),
                                       ("2025-03-29", "2025-03-30", 60),
                                       ("2025-10-25", "2025-10-26", -60)):
            for column in ("Sunrise", "Sunset"):
                def minutes(value):
                    hour, minute, _ = map(int, value.split(":"))
                    return hour * 60 + minute
                difference = minutes(self.rows[after][column]) - minutes(self.rows[before][column])
                self.assertAlmostEqual(difference, change, delta=3)
        for row in self.rows.values():
            self.assertLess(row["Sunrise"], row["Sunset"])
            self.assertTrue(row["Sunrise"].endswith(":00"))
            self.assertTrue(row["Sunset"].endswith(":00"))

    def test_moon_phase_preserves_rough_original_model(self):
        for date_text, row in self.rows.items():
            day = datetime.date.fromisoformat(date_text)
            lunations = (day - datetime.date(2000, 1, 6)).days / 29.53
            expected = int(100 - abs(lunations - math.floor(lunations) - 0.5) * 200 + 0.5)
            self.assertEqual(row["Moon Phase %"], expected)
            self.assertLessEqual(0, expected)
            self.assertLessEqual(expected, 100)

    def test_database_is_encrypted(self):
        with self.database.open("rb") as database:
            self.assertNotEqual(database.read(16), b"SQLite format 3\x00")
        result = subprocess.run([shutil.which("sqlcipher"), str(self.database),
                                 "select name from sqlite_master;"],
                                capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)

    def test_view_works_in_shared_sqlcipher_library_without_custom_functions(self):
        library = ctypes.util.find_library("sqlcipher")
        self.assertIsNotNone(library, "MARS requires the SQLCipher shared library")
        api = ctypes.CDLL(library)
        api.sqlite3_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        api.sqlite3_open.restype = ctypes.c_int
        api.sqlite3_exec.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p,
                                    ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)]
        api.sqlite3_exec.restype = ctypes.c_int
        api.sqlite3_close.argtypes = [ctypes.c_void_p]
        api.sqlite3_close.restype = ctypes.c_int
        api.sqlite3_free.argtypes = [ctypes.c_void_p]
        api.sqlite3_free.restype = None
        database = ctypes.c_void_p()
        self.assertEqual(api.sqlite3_open(str(self.database).encode(), ctypes.byref(database)), 0)
        try:
            error = ctypes.c_char_p()
            sql = (f"pragma key = '{TEST_KEY}'; select * from calendar_local "
                   "where FullDateAlternateKey = '2024-06-21';").encode()
            result = api.sqlite3_exec(database, sql, None, None, ctypes.byref(error))
            message = error.value.decode() if error.value else ""
            api.sqlite3_free(error)
            self.assertEqual(result, 0, message)
        finally:
            self.assertEqual(api.sqlite3_close(database), 0)

    def test_standalone_view_reload_is_idempotent(self):
        expected = [{"Sunrise": "04:47:00", "Sunset": "21:39:00"}]
        query = "select Sunrise, Sunset from calendar_local where FullDateAlternateKey = '2024-06-21';"
        for _ in range(2):
            self.query(f".read {VIEW_SCRIPT}\n")
            self.assertEqual(self.query(query), expected)
        # No holiday tables, extensions or application-defined functions are required.
        original_database = self.database
        try:
            type(self).database = Path(self.temporary.name) / "standalone.db"
            self.query(f".read {LOCALE_SCRIPT}\n")
            self.query(f".read {VIEW_SCRIPT}\n")
            self.assertEqual(self.query("select * from calendar_local;"), [])
            self.query("insert into calendar_local_settings values "
                       "(1, 'Shrewsbury', 'GB-ENG', 'Europe/London', 52.7077, -2.7541, 2024, 2024, null);"
                       "insert into calendar_local_days values ('2024-06-21', null, 0, 1);")
            self.assertEqual(self.query(query), expected)
            self.query("update calendar_local_settings set latitude = 90;")
            self.assertEqual(self.query(query), [{"Sunrise": None, "Sunset": None}])
        finally:
            type(self).database = original_database


@unittest.skipUnless(shutil.which("sqlcipher"), "SQLCipher shell is not installed")
class ZZCalendarLocalReadmeTests(CalendarDatabase, unittest.TestCase):
    def test_readme_western_frisian_calendar(self):
        """README example: Frisian date/holiday text beside the Dutch regional date."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        sql = '''select [Date Lingua], [Date Regional], [Public Holiday]
from calendar_local where FullDateAlternateKey = '2026-12-25';'''
        self.assertIn(sql, guide)
        with tempfile.TemporaryDirectory(prefix="mars-calendar-readme-frisian-") as temporary:
            database = Path(temporary) / "jurisdiction.db"
            shutil.copyfile(self.database, database)
            calendar.populate_calendar(database, TEST_KEY, self.catalogue.resolve("Limmen"), locale="fy_NL")
            rows = calendar.run_sql(database, TEST_KEY, sql)
        self.assertEqual(rows, [{"Date Lingua": "25 Desimber 2026", "Date Regional": "25 december 2026",
                                "Public Holiday": "Earste Krystdei"}])
        output = "Date Lingua|Date Regional|Public Holiday\n" + "|".join(
            rows[0][column] for column in ("Date Lingua", "Date Regional", "Public Holiday"))
        self.assertIn(output, guide)
        print(output)

    def test_readme_rhyl_holiday_languages(self):
        """README examples: Welsh and Irish Christmas holidays, including the substitute day."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        sql = '''select FullDateAlternateKey, [Date Lingua], [Public Holiday]
from calendar_local
where FullDateAlternateKey between '2026-12-25' and '2026-12-28'
  and [Public Holiday] is not null
order by FullDateAlternateKey;'''
        self.assertIn(sql, guide)
        for locale, month, christmas, substitute in (
                ("cy_GB", "Rhagfyr", "Dydd Nadolig", "Dydd San Steffan (diwrnod amgen)"),
                ("ga_GB", "Nollaig", "Lá Nollag", "Lá Fhéile Stiofáin (lá ionaid)")):
            with self.subTest(locale=locale), tempfile.TemporaryDirectory(prefix="mars-calendar-readme-rhyl-") as temporary:
                database = Path(temporary) / "jurisdiction.db"
                shutil.copyfile(self.database, database)
                calendar.populate_calendar(database, TEST_KEY, self.catalogue.resolve("Rhyl"), locale=locale)
                rows = calendar.run_sql(database, TEST_KEY, sql)
                self.assertEqual(rows, [
                    {"FullDateAlternateKey": "2026-12-25", "Date Lingua": f"25 {month} 2026",
                     "Public Holiday": christmas},
                    {"FullDateAlternateKey": "2026-12-28", "Date Lingua": f"28 {month} 2026",
                     "Public Holiday": substitute},
                ])
                output = "FullDateAlternateKey|Date Lingua|Public Holiday\n" + "\n".join(
                    "|".join(row[column] for column in ("FullDateAlternateKey", "Date Lingua", "Public Holiday"))
                    for row in rows)
                self.assertIn(output, guide)
                print(output)

    def test_readme_patwa_calendar(self):
        """README example: Patwa calendar labels retain Jamaican holiday dates."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        sql = '''select FullDateAlternateKey, [Date Lingua], [Day Name], [Public Holiday]
from calendar_local
where FullDateAlternateKey = '2026-08-06';'''
        self.assertIn(sql, guide)
        with tempfile.TemporaryDirectory(prefix="mars-calendar-readme-patwa-") as temporary:
            database = Path(temporary) / "jurisdiction.db"
            shutil.copyfile(self.database, database)
            calendar.populate_calendar(database, TEST_KEY, self.catalogue.resolve("Kingston, JM"), locale="jam")
            rows = calendar.run_sql(database, TEST_KEY, sql)
        self.assertEqual(rows, [{"FullDateAlternateKey": "2026-08-06", "Date Lingua": "6 Aagus 2026",
                                "Day Name": "Turzdeh", "Public Holiday": "Independence Deh"}])
        output = ("FullDateAlternateKey|Date Lingua|Day Name|Public Holiday\n"
                  "2026-08-06|6 Aagus 2026|Turzdeh|Independence Deh")
        self.assertIn(output, guide)
        print(output)

    def test_readme_translated_holiday_names(self):
        """README example: local dates and holiday names use the selected language."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        sql = '''select FullDateAlternateKey, [Date Lingua], [Public Holiday]
from calendar_local
where FullDateAlternateKey in ('2026-01-01', '2026-07-04', '2026-12-25')
order by FullDateAlternateKey;'''
        self.assertIn(sql, guide)
        with tempfile.TemporaryDirectory(prefix="mars-calendar-readme-translations-") as temporary:
            database = Path(temporary) / "jurisdiction.db"
            shutil.copyfile(self.database, database)
            calendar.populate_calendar(database, TEST_KEY, self.catalogue.resolve("New York City"), locale="es_US")
            rows = calendar.run_sql(database, TEST_KEY, sql)
        expected = [
            {"FullDateAlternateKey": "2026-01-01", "Date Lingua": "1 de enero de 2026", "Public Holiday": "Día de Año Nuevo"},
            {"FullDateAlternateKey": "2026-07-04", "Date Lingua": "4 de julio de 2026", "Public Holiday": "Día de la Independencia"},
            {"FullDateAlternateKey": "2026-12-25", "Date Lingua": "25 de diciembre de 2026", "Public Holiday": "Día de Navidad"},
        ]
        self.assertEqual(rows, expected)
        output = "FullDateAlternateKey|Date Lingua|Public Holiday\n" + "\n".join(
            "|".join(row[column] for column in ("FullDateAlternateKey", "Date Lingua", "Public Holiday")) for row in rows)
        self.assertIn(output, guide)
        print(output)

    def test_readme_hebrew_jewish_date(self):
        """README example: Hebrew Jewish date alongside its Gregorian UK date."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        sql = '''select [Date Lingua], [Date UK]
from calendar_local
where FullDateAlternateKey = '2026-04-02';'''
        self.assertIn(sql, guide)
        try:
            self.query("update calendar_local_settings set locale = 'he_IL';")
            self.assertEqual(self.query(sql), [{"Date Lingua": "15 בניסן 5786", "Date UK": "2ⁿᵈ April 2026"}])
            output = "15 בניסן 5786|2ⁿᵈ April 2026"
            self.assertIn(output, guide)
            print(output)
        finally:
            self.query("update calendar_local_settings set locale = null;")

    def test_readme_arabic_hijri_date(self):
        """README example: Arabic Hijri date alongside its Gregorian UK date."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        sql = '''select [Date Lingua], [Date UK]
from calendar_local
where FullDateAlternateKey = '2026-04-02';'''
        self.assertIn(sql, guide)
        try:
            self.query("update calendar_local_settings set locale = 'ar_PS';")
            self.assertEqual(self.query(sql), [{"Date Lingua": "١٤ شوال ١٤٤٧", "Date UK": "2ⁿᵈ April 2026"}])
            output = "١٤ شوال ١٤٤٧|2ⁿᵈ April 2026"
            self.assertIn(output, guide)
            print(output)
        finally:
            self.query("update calendar_local_settings set locale = null;")

    def test_readme_breton_and_cornish_dates(self):
        """README example: regional choices supply full and abbreviated calendar names."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        sql = '''select [Date Lingua], [Month Name], [Month Name Abbrev], [Day Name], [Day Name Abbrev]
from calendar_local
where FullDateAlternateKey = '2024-06-21';'''
        self.assertIn(sql, guide)
        try:
            for jurisdiction, language, expected in (
                    ("FR", "br", "21 Mezheven 2024|Mezheven|Mezh.|Gwener|Gwe."),
                    ("GB-ENG", "kw", "21 mis Metheven 2024|mis Metheven|Met|dy Gwener|Gwe")):
                with self.subTest(language=language):
                    locale = languages.choose_language(self.database, TEST_KEY, jurisdiction, requested=language)
                    self.query("update calendar_local_settings set locale = " + calendar.sql_quote(locale) + ";")
                    rows = self.query(sql)
                    self.assertEqual(len(rows), 1)
                    output = "|".join(rows[0].values())
                    self.assertEqual(output, expected)
                    self.assertIn(output, guide)
                    print(output)
        finally:
            self.query("update calendar_local_settings set locale = null;")

    def test_readme_working_day_number(self):
        """README example: running working-day count at the start of a Shrewsbury calendar."""
        sql = '''select FullDateAlternateKey, [Workday Type], [Working Day No]
from calendar_local
where FullDateAlternateKey between '2015-01-01' and '2015-01-05'
order by FullDateAlternateKey;'''
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        self.assertIn(sql, guide)
        self.assertEqual(self.query(sql), [
            {"FullDateAlternateKey": "2015-01-01", "Workday Type": 0, "Working Day No": 0},
            {"FullDateAlternateKey": "2015-01-02", "Workday Type": 1, "Working Day No": 1},
            {"FullDateAlternateKey": "2015-01-03", "Workday Type": 0, "Working Day No": 1},
            {"FullDateAlternateKey": "2015-01-04", "Workday Type": 0, "Working Day No": 1},
            {"FullDateAlternateKey": "2015-01-05", "Workday Type": 1, "Working Day No": 2},
        ])
        output = ("FullDateAlternateKey|Workday Type|Working Day No\n"
                  "2015-01-01|0|0\n2015-01-02|1|1\n2015-01-03|0|1\n2015-01-04|0|1\n2015-01-05|1|2")
        self.assertIn(output, guide)
        print(output)

    def test_readme_fiscal_quarter_name(self):
        """README example: fiscal quarter labels across the April year boundary."""
        sql = '''select FullDateAlternateKey, [Fiscal Quarter Name]
from calendar_local
where FullDateAlternateKey in ('2024-03-31', '2024-04-01', '2024-07-01', '2025-01-01')
order by FullDateAlternateKey;'''
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        self.assertIn(sql, guide)
        self.assertEqual(self.query(sql), [
            {"FullDateAlternateKey": "2024-03-31", "Fiscal Quarter Name": "2024 Q4"},
            {"FullDateAlternateKey": "2024-04-01", "Fiscal Quarter Name": "2025 Q1"},
            {"FullDateAlternateKey": "2024-07-01", "Fiscal Quarter Name": "2025 Q2"},
            {"FullDateAlternateKey": "2025-01-01", "Fiscal Quarter Name": "2025 Q4"},
        ])
        output = ("FullDateAlternateKey|Fiscal Quarter Name\n"
                  "2024-03-31|2024 Q4\n2024-04-01|2025 Q1\n2024-07-01|2025 Q2\n2025-01-01|2025 Q4")
        self.assertIn(output, guide)
        print(output)

    def test_readme_quebec_and_latin_installation(self):
        """README examples: numbered Quebec selection and Vatican Latin calendar."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        menu = io.StringIO()
        with mock.patch("builtins.input", return_value="1"), mock.patch("sys.stdout", menu):
            self.assertEqual(calendar.choose_location(self.catalogue, "Quebec", "", True).label, "Québec, CA-QC")
        menu_line = "  1. Québec, CA-QC (America/Toronto)"
        self.assertIn(menu_line, menu.getvalue())
        self.assertIn(menu_line, guide)
        print(menu_line)
        cases = (
            ("make install-jurisdiction-db Quebec", ["Quebec"], "Québec, CA-QC", "fr_CA"),
            ("make install-jurisdiction-db LOCATION='Vatican City' CALENDAR_LANGUAGE=la",
             ["LOCATION=Vatican City", "CALENDAR_LANGUAGE=la"], "Vatican City, VA", "la_VA"),
        )
        for command, arguments, label, locale in cases:
            with self.subTest(label=label), tempfile.TemporaryDirectory(prefix="mars-calendar-readme-") as temporary:
                self.assertIn(command, guide)
                environment = dict(os.environ, MARS_HOME=temporary, MARS_JURISDICTION_DB_KEY=TEST_KEY)
                for name in ("MARS_JURISDICTION_DB_PATH", calendar.LOCATION_ENV, "MARS_CALENDAR_LOCATION_ARGUMENT",
                             languages.LANGUAGE_ENV, "MARS_CALENDAR_LANGUAGE_ARGUMENT", "CALENDAR_LANGUAGE"):
                    environment.pop(name, None)
                result = subprocess.run(
                    ["make", "-j1", "-o", "check-jurisdiction-db-deps", "-o", "build/release/libmars.so",
                     "install-jurisdiction-db", *arguments], cwd=ROOT, env=environment,
                    stdin=subprocess.DEVNULL, text=True, capture_output=True, timeout=120,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                output = f"calendar_local language: {locale}\ncalendar_local location: {label}"
                self.assertIn(output, result.stdout)
                self.assertIn(output, guide)
                print(output)
                config = installer.read_config(Path(temporary) / "config/jurisdiction-db.env")
                self.assertEqual(config[languages.LANGUAGE_ENV], locale)
                database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
                settings = calendar.run_sql(database, TEST_KEY, "select locale from calendar_local_settings;")
                self.assertEqual(settings, [{"locale": locale}])
                if locale == "fr_CA":
                    self.assertEqual(calendar.run_sql(database, TEST_KEY,
                        "select jurisdiction from calendar_local_settings;"), [{"jurisdiction": "CA-QC"}])
                    # CA-QC currently inherits packaged Canadian rules; do not
                    # assert provincial holidays which the source does not supply.
                    row = calendar.run_sql(database, TEST_KEY,
                        'select "Day Name", "Public Holiday" from calendar_local '
                        "where FullDateAlternateKey = '2024-07-01';")[0]
                    self.assertEqual(row["Day Name"], "lundi")
                    self.assertEqual(row["Public Holiday"], "Fête du Canada")
                    sql = '''select [Date Lingua], [Date Regional], [Public Holiday]
from calendar_local where FullDateAlternateKey = '2026-07-01';'''
                    self.assertIn(sql, guide)
                    row = calendar.run_sql(database, TEST_KEY, sql)[0]
                    output = "|".join(row[column] for column in ("Date Lingua", "Date Regional", "Public Holiday"))
                    self.assertEqual(output, "1ᵉʳ juillet 2026|1ᵉʳ juillet 2026|Fête du Canada")
                    self.assertIn(output, guide)
                    print(output)
                else:
                    sql = '''select [Date Lingua], "Day Name", "Day Name Abbrev", "Month Name", "Month Name Abbrev", [Date UK]
from calendar_local
where FullDateAlternateKey = '2024-06-21';'''
                    self.assertIn(sql, guide)
                    self.assertEqual(calendar.run_sql(database, TEST_KEY, sql), [{
                        "Date Lingua": "21 Iunii 2024", "Day Name": "Veneris", "Day Name Abbrev": "Ven",
                        "Month Name": "Iunius", "Month Name Abbrev": "Iun", "Date UK": "21ˢᵗ June 2024",
                    }])
                    output = ("Date Lingua|Day Name|Day Name Abbrev|Month Name|Month Name Abbrev|Date UK\n"
                              "21 Iunii 2024|Veneris|Ven|Iunius|Iun|21ˢᵗ June 2024")
                    self.assertIn(output, guide)
                    print(output)

    def test_readme_install_limmen(self):
        """README example: docs/jurisdiction.md, Local Calendar View installation."""
        guide = (ROOT / "docs/jurisdiction.md").read_text()
        self.assertIn("make install-jurisdiction-db limmen", guide)
        with tempfile.TemporaryDirectory(prefix="mars-calendar-readme-") as temporary:
            environment = dict(os.environ, MARS_HOME=temporary, MARS_JURISDICTION_DB_KEY=TEST_KEY)
            for name in ("MARS_JURISDICTION_DB_PATH", calendar.LOCATION_ENV, "MARS_CALENDAR_LOCATION_ARGUMENT",
                         languages.LANGUAGE_ENV, "MARS_CALENDAR_LANGUAGE_ARGUMENT", "CALENDAR_LANGUAGE"):
                environment.pop(name, None)
            # Exercise the actual Make recipe and argument forwarding. The already-built
            # library and dependency checks are not rebuilt during this integration test.
            result = subprocess.run(
                ["make", "-j1", "-o", "check-jurisdiction-db-deps", "-o", "build/release/libmars.so",
                 "install-jurisdiction-db", "limmen"], cwd=ROOT, env=environment,
                stdin=subprocess.DEVNULL, text=True, capture_output=True, timeout=120,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            output = "calendar_local location: Limmen, NL"
            self.assertIn(output, result.stdout)
            self.assertIn(output, guide)
            database = Path(temporary) / "jurisdiction/mars_jurisdiction_rules.db"
            self.assertEqual(calendar.run_sql(database, TEST_KEY,
                             "select location, jurisdiction from calendar_local_settings;"),
                             [{"location": "Limmen", "jurisdiction": "NL"}])
            print(output)
            # README example: Dutch labels in the installed Limmen calendar.
            sql = '''select [Date Lingua], "Day Name", "Day Name Abbrev", "Month Name", "Month Name Abbrev",
       "ME Dates Text", "WE Dates Text"
from calendar_local
where FullDateAlternateKey = '2024-06-21';'''
            self.assertIn(sql, guide)
            self.assertEqual(calendar.run_sql(database, TEST_KEY, sql), [{
                "Date Lingua": "21 juni 2024", "Day Name": "vrijdag", "Day Name Abbrev": "vr",
                "Month Name": "juni", "Month Name Abbrev": "jun",
                "ME Dates Text": "jun 2024", "WE Dates Text": "23 jun 2024",
            }])
            output = ("Date Lingua|Day Name|Day Name Abbrev|Month Name|Month Name Abbrev|ME Dates Text|WE Dates Text\n"
                      "21 juni 2024|vrijdag|vr|juni|jun|jun 2024|23 jun 2024")
            self.assertIn(output, guide)
            print(output)

    def test_readme_shrewsbury_calendar_query(self):
        """README example: docs/jurisdiction.md, Local Calendar View query."""
        sql = '''select FullDateAlternateKey, [Date UK], [Date Lingua], "Day Name", Sunrise, Sunset
from calendar_local
where FullDateAlternateKey = '2024-06-21';'''
        self.assertIn(sql, (ROOT / "docs/jurisdiction.md").read_text())
        rows = self.query(sql)
        self.assertEqual(rows, [{"FullDateAlternateKey": "2024-06-21", "Date UK": "21ˢᵗ June 2024", "Day Name": "Friday",
                                 "Date Lingua": "21ˢᵗ June 2024",
                                 "Sunrise": "04:47:00", "Sunset": "21:39:00"}])
        output = ("FullDateAlternateKey|Date UK|Date Lingua|Day Name|Sunrise|Sunset\n"
                  "2024-06-21|21ˢᵗ June 2024|21ˢᵗ June 2024|Friday|04:47:00|21:39:00")
        self.assertIn(output, (ROOT / "docs/jurisdiction.md").read_text())
        print(output)


if __name__ == "__main__":
    focus = json.loads((ROOT / "tests/test_config.json").read_text()).get("calendar_local_focus")
    suite = unittest.defaultTestLoader.loadTestsFromNames(
        focus, sys.modules[__name__]
    ) if focus else unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(not result.wasSuccessful())
