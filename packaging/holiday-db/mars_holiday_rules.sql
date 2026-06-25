-- SQLCipher jurisdiction database source script.
-- The installer supplies the chosen PRAGMA key before reading this file.
--
-- Worldwide holiday rules need to handle:
--   - country and subdivision inheritance
--   - Gregorian and non-Gregorian calendar systems
--   - recurrence rules, algorithmic rules, and one-off proclamations
--   - observed/substitute-day policies
--   - legal/source provenance
--   - jurisdictions where state/province calendars are materially richer than
--     the country-level calendar, such as Australia, Canada, New Zealand,
--     the United States, Portugal, Italy, and the United Kingdom
--
-- Design notes:
--   - Jurisdictions follow ISO-style country and subdivision codes.
--   - Calendar systems use CLDR/RSCALE identifiers where possible.
--   - RRULE text is reserved for RFC 5545 / RFC 7529 style recurrence data.
--   - expression_text is the escape hatch for rules that need a custom
--     evaluator, such as astronomical calculations or government-specific
--     observance policies. For expression_language = 'mars_sql', the runtime
--     expects expression_text to be a single SQL SELECT that returns one
--     holiday date in YYYY-MM-DD form for the bound rule year/context.
--   - Targeted subdivision coverage is loaded alongside the worldwide country
--     set so UI clients can expose first-class jurisdiction choices such as
--     AU-NSW and AU-VIC instead of only the national AU calendar.

PRAGMA foreign_keys = ON;

DROP TABLE IF EXISTS holiday_rule_source;
DROP TABLE IF EXISTS holiday_instance;
DROP TABLE IF EXISTS holiday_exception;
DROP TABLE IF EXISTS holiday_observance_rule;
DROP TABLE IF EXISTS jurisdiction_weekend_rule;
DROP TABLE IF EXISTS holiday_rule;
DROP TABLE IF EXISTS holiday_definition;
DROP TABLE IF EXISTS holiday_name;
DROP TABLE IF EXISTS source_document;
DROP TABLE IF EXISTS calendar_system;
DROP TABLE IF EXISTS jurisdiction;
DROP TABLE IF EXISTS holiday_override;
DROP TABLE IF EXISTS locality;
DROP TABLE IF EXISTS jurisdiction_location_default;
DROP TABLE IF EXISTS timezone_transition_rule;
DROP TABLE IF EXISTS timezone_era;
DROP TABLE IF EXISTS timezone_definition;

CREATE TABLE jurisdiction (
    jurisdiction_id TEXT PRIMARY KEY,
    parent_jurisdiction_id TEXT REFERENCES jurisdiction(jurisdiction_id) ON DELETE CASCADE,
    jurisdiction_type TEXT NOT NULL CHECK (jurisdiction_type IN (
        'country',
        'subdivision',
        'dependency',
        'municipality',
        'special',
        'supranational'
    )),
    iso_country_code TEXT,
    iso_subdivision_code TEXT,
    cldr_region_code TEXT,
    name TEXT NOT NULL,
    valid_from_year INTEGER,
    valid_to_year INTEGER,
    notes TEXT
);

CREATE TABLE jurisdiction_location_default (
    jurisdiction_id TEXT PRIMARY KEY REFERENCES jurisdiction(jurisdiction_id) ON DELETE CASCADE,
    latitude TEXT NOT NULL,
    longitude TEXT NOT NULL,
    timezone_name TEXT NOT NULL,
    locality_name TEXT,
    notes TEXT
);

CREATE TABLE timezone_definition (
    timezone_name TEXT PRIMARY KEY,
    canonical_timezone_name TEXT NOT NULL,
    notes TEXT
);

CREATE TABLE timezone_era (
    timezone_era_id INTEGER PRIMARY KEY,
    timezone_name TEXT NOT NULL REFERENCES timezone_definition(timezone_name) ON DELETE CASCADE,
    sequence_no INTEGER NOT NULL,
    gmtoff_minutes INTEGER NOT NULL,
    rules_kind TEXT NOT NULL CHECK (rules_kind IN ('none', 'fixed', 'named')),
    fixed_save_minutes INTEGER,
    rule_name TEXT,
    format_text TEXT NOT NULL,
    until_year INTEGER,
    until_month INTEGER CHECK (until_month BETWEEN 1 AND 12),
    until_day_kind TEXT CHECK (until_day_kind IN (
        'day_of_month',
        'last_weekday',
        'weekday_on_or_after',
        'weekday_on_or_before'
    )),
    until_day_value INTEGER,
    until_weekday INTEGER CHECK (until_weekday BETWEEN 1 AND 7),
    until_seconds INTEGER,
    until_suffix TEXT CHECK (until_suffix IN ('w', 's', 'u', 'g', 'z')),
    UNIQUE(timezone_name, sequence_no)
);

CREATE TABLE timezone_transition_rule (
    timezone_transition_rule_id INTEGER PRIMARY KEY,
    rule_name TEXT NOT NULL,
    from_year INTEGER,
    to_year INTEGER,
    in_month INTEGER NOT NULL CHECK (in_month BETWEEN 1 AND 12),
    on_kind TEXT NOT NULL CHECK (on_kind IN (
        'day_of_month',
        'last_weekday',
        'weekday_on_or_after',
        'weekday_on_or_before'
    )),
    on_day INTEGER NOT NULL,
    on_weekday INTEGER CHECK (on_weekday BETWEEN 1 AND 7),
    at_seconds INTEGER NOT NULL,
    at_suffix TEXT NOT NULL CHECK (at_suffix IN ('w', 's', 'u', 'g', 'z')),
    save_minutes INTEGER NOT NULL,
    letters TEXT
);

CREATE TABLE calendar_system (
    calendar_system_id TEXT PRIMARY KEY,
    cldr_rscale TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    family TEXT NOT NULL CHECK (family IN ('solar', 'lunar', 'lunisolar', 'other')),
    notes TEXT
);

CREATE TABLE source_document (
    source_document_id INTEGER PRIMARY KEY,
    jurisdiction_id TEXT REFERENCES jurisdiction(jurisdiction_id) ON DELETE CASCADE,
    source_type TEXT NOT NULL CHECK (source_type IN (
        'gazette',
        'statute',
        'government_site',
        'standards',
        'internal',
        'other'
    )),
    citation TEXT NOT NULL,
    source_url TEXT,
    published_on TEXT,
    accessed_on TEXT,
    notes TEXT
);

CREATE TABLE holiday_definition (
    holiday_id INTEGER PRIMARY KEY,
    jurisdiction_id TEXT NOT NULL REFERENCES jurisdiction(jurisdiction_id) ON DELETE CASCADE,
    holiday_key TEXT NOT NULL,
    default_name TEXT NOT NULL,
    holiday_class TEXT NOT NULL CHECK (holiday_class IN (
        'public',
        'bank',
        'observance',
        'school',
        'religious',
        'half_day',
        'special'
    )),
    scope TEXT NOT NULL DEFAULT 'full_day' CHECK (scope IN (
        'full_day',
        'half_day',
        'hours',
        'market_close',
        'school_only'
    )),
    calendar_system_id TEXT NOT NULL REFERENCES calendar_system(calendar_system_id),
    valid_from_year INTEGER,
    valid_to_year INTEGER,
    is_active INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    notes TEXT,
    UNIQUE(jurisdiction_id, holiday_key, valid_from_year)
);

CREATE TABLE holiday_name (
    holiday_name_id INTEGER PRIMARY KEY,
    holiday_id INTEGER NOT NULL REFERENCES holiday_definition(holiday_id) ON DELETE CASCADE,
    locale TEXT NOT NULL,
    localized_name TEXT NOT NULL,
    is_primary INTEGER NOT NULL DEFAULT 0 CHECK (is_primary IN (0, 1)),
    UNIQUE(holiday_id, locale, localized_name)
);

CREATE TABLE holiday_rule (
    rule_id INTEGER PRIMARY KEY,
    holiday_id INTEGER NOT NULL REFERENCES holiday_definition(holiday_id) ON DELETE CASCADE,
    sequence_no INTEGER NOT NULL DEFAULT 1,
    rule_kind TEXT NOT NULL CHECK (rule_kind IN (
        'fixed_date',
        'nth_weekday',
        'last_weekday',
        'weekday_after_date',
        'weekday_before_date',
        'relative_to_holiday',
        'easter_offset',
        'orthodox_easter_offset',
        'rrule',
        'algorithmic',
        'one_off'
    )),
    month INTEGER CHECK (month BETWEEN 1 AND 12),
    day INTEGER CHECK (day BETWEEN 1 AND 31),
    weekday INTEGER CHECK (weekday BETWEEN 1 AND 7),
    ordinal INTEGER,
    offset_days INTEGER,
    anchor_holiday_key TEXT,
    rrule_text TEXT,
    expression_language TEXT CHECK (expression_language IN (
        'mars_sql',
        'rfc5545_rrule',
        'json',
        'text'
    )),
    expression_text TEXT,
    holiday_date TEXT,
    valid_from_year INTEGER,
    valid_to_year INTEGER,
    priority INTEGER NOT NULL DEFAULT 100,
    notes TEXT,
    CHECK (
        holiday_date IS NULL
        OR holiday_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'
    )
);

CREATE TABLE holiday_observance_rule (
    observance_rule_id INTEGER PRIMARY KEY,
    holiday_id INTEGER NOT NULL REFERENCES holiday_definition(holiday_id) ON DELETE CASCADE,
    applies_to_rule_id INTEGER REFERENCES holiday_rule(rule_id) ON DELETE CASCADE,
    observed_rule_kind TEXT NOT NULL CHECK (observed_rule_kind IN (
        'none',
        'next_weekday',
        'next_monday',
        'nearest_weekday',
        'next_non_holiday',
        'previous_weekday',
        'christmas_pair',
        'custom_expression'
    )),
    observed_name TEXT,
    weekend_mask TEXT,
    suppress_original INTEGER NOT NULL DEFAULT 0 CHECK (suppress_original IN (0, 1)),
    move_days INTEGER,
    second_move_days INTEGER,
    expression_language TEXT CHECK (expression_language IN (
        'mars_sql',
        'json',
        'text'
    )),
    expression_text TEXT,
    valid_from_year INTEGER,
    valid_to_year INTEGER,
    priority INTEGER NOT NULL DEFAULT 100,
    notes TEXT,
    CHECK (weekend_mask IS NULL OR weekend_mask <> '')
);

CREATE TABLE jurisdiction_weekend_rule (
    weekend_rule_id INTEGER PRIMARY KEY,
    jurisdiction_id TEXT NOT NULL REFERENCES jurisdiction(jurisdiction_id) ON DELETE CASCADE,
    weekend_mask TEXT NOT NULL,
    valid_from_year INTEGER,
    valid_to_year INTEGER,
    source_document_id INTEGER REFERENCES source_document(source_document_id) ON DELETE SET NULL,
    notes TEXT
);

CREATE TABLE holiday_exception (
    exception_id INTEGER PRIMARY KEY,
    jurisdiction_id TEXT NOT NULL REFERENCES jurisdiction(jurisdiction_id) ON DELETE CASCADE,
    holiday_id INTEGER REFERENCES holiday_definition(holiday_id) ON DELETE CASCADE,
    target_rule_id INTEGER REFERENCES holiday_rule(rule_id) ON DELETE CASCADE,
    holiday_date TEXT NOT NULL,
    action TEXT NOT NULL CHECK (action IN ('add', 'replace', 'suppress', 'rename')),
    name TEXT,
    replacement_holiday_key TEXT,
    expression_language TEXT CHECK (expression_language IN (
        'mars_sql',
        'json',
        'text'
    )),
    expression_text TEXT,
    valid_from_year INTEGER,
    valid_to_year INTEGER,
    priority INTEGER NOT NULL DEFAULT 0,
    source_document_id INTEGER REFERENCES source_document(source_document_id) ON DELETE SET NULL,
    notes TEXT,
    CHECK (holiday_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]')
);

CREATE TABLE holiday_rule_source (
    holiday_rule_source_id INTEGER PRIMARY KEY,
    holiday_id INTEGER REFERENCES holiday_definition(holiday_id) ON DELETE CASCADE,
    rule_id INTEGER REFERENCES holiday_rule(rule_id) ON DELETE CASCADE,
    observance_rule_id INTEGER REFERENCES holiday_observance_rule(observance_rule_id) ON DELETE CASCADE,
    exception_id INTEGER REFERENCES holiday_exception(exception_id) ON DELETE CASCADE,
    source_document_id INTEGER NOT NULL REFERENCES source_document(source_document_id) ON DELETE CASCADE,
    role TEXT NOT NULL CHECK (role IN ('definition', 'observance', 'exception', 'background')),
    notes TEXT
);

CREATE TABLE holiday_instance (
    holiday_instance_id INTEGER PRIMARY KEY,
    jurisdiction_id TEXT NOT NULL REFERENCES jurisdiction(jurisdiction_id) ON DELETE CASCADE,
    holiday_id INTEGER REFERENCES holiday_definition(holiday_id) ON DELETE SET NULL,
    holiday_date TEXT NOT NULL,
    holiday_name TEXT NOT NULL,
    holiday_class TEXT NOT NULL CHECK (holiday_class IN (
        'public',
        'bank',
        'observance',
        'school',
        'religious',
        'half_day',
        'special'
    )),
    language TEXT,
    source_document_id INTEGER REFERENCES source_document(source_document_id) ON DELETE SET NULL,
    notes TEXT,
    CHECK (holiday_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]')
);

CREATE INDEX jurisdiction_parent_idx
    ON jurisdiction(parent_jurisdiction_id, jurisdiction_type, name);

CREATE INDEX holiday_definition_jurisdiction_idx
    ON holiday_definition(jurisdiction_id, holiday_class, holiday_key);

CREATE INDEX holiday_rule_holiday_idx
    ON holiday_rule(holiday_id, priority, sequence_no);

CREATE INDEX holiday_observance_holiday_idx
    ON holiday_observance_rule(holiday_id, priority);

CREATE INDEX jurisdiction_weekend_rule_idx
    ON jurisdiction_weekend_rule(jurisdiction_id, valid_from_year, valid_to_year);

CREATE INDEX holiday_exception_jurisdiction_date_idx
    ON holiday_exception(jurisdiction_id, holiday_date, priority);

CREATE INDEX holiday_rule_source_doc_idx
    ON holiday_rule_source(source_document_id, role);

CREATE INDEX holiday_instance_jurisdiction_date_idx
    ON holiday_instance(jurisdiction_id, holiday_date, holiday_name);

CREATE INDEX timezone_era_timezone_idx
    ON timezone_era(timezone_name, sequence_no);

CREATE INDEX timezone_transition_rule_name_idx
    ON timezone_transition_rule(rule_name, from_year, to_year, in_month);

.read packaging/holiday-db/mars_country_jurisdictions.sql
.read packaging/holiday-db/mars_target_subdivisions.sql

INSERT INTO calendar_system(calendar_system_id, cldr_rscale, name, family, notes)
VALUES
    ('gregory', 'gregory', 'Gregorian', 'solar',
     'Default civil calendar used by most public holiday statutes.'),
    ('hebrew', 'hebrew', 'Hebrew', 'lunisolar',
     'Needed for holidays defined by the Hebrew calendar.'),
    ('islamic', 'islamic', 'Islamic', 'lunar',
     'Base identifier for lunar Islamic calendar rules.'),
    ('islamic_umalqura', 'islamic-umalqura', 'Islamic (Umm al-Qura)', 'lunar',
     'Common civil variant used in some official calendars.'),
    ('chinese', 'chinese', 'Chinese', 'lunisolar',
     'Needed for Chinese New Year and related observances.'),
    ('ethiopic', 'ethiopic', 'Ethiopic', 'solar',
     'Included to support Ethiopic calendar recurrence rules.'),
    ('indian', 'indian', 'Indian National Calendar', 'solar',
     'Needed for jurisdictions that define holidays in the Saka calendar.');

INSERT INTO jurisdiction(
    jurisdiction_id,
    parent_jurisdiction_id,
    jurisdiction_type,
    iso_country_code,
    iso_subdivision_code,
    cldr_region_code,
    name,
    notes
)
VALUES
    ('XK', NULL, 'special', 'XK', NULL, 'XK', 'Kosovo',
     'Special-case jurisdiction used for imported holiday datasets that expose Kosovo under XK.'),
    ('GB-ENG', 'GB', 'subdivision', 'GB', 'GB-ENG', 'GB', 'England',
     'Subdivision rule set for English bank holidays.');

.read packaging/holiday-db/mars_jurisdiction_location_defaults.sql
.read packaging/holiday-db/mars_timezone_rules.sql

INSERT INTO source_document(
    source_document_id,
    jurisdiction_id,
    source_type,
    citation,
    source_url,
    accessed_on,
    notes
)
VALUES
    (1, NULL, 'standards',
     'ISO 3166 country and subdivision codes',
     'https://www.iso.org/iso-3166-country-codes.html',
     '2026-06-23',
     'Jurisdiction coding model.'),
    (2, NULL, 'standards',
     'tz database iso3166.tab country list',
     'https://data.iana.org/time-zones/tzdb-2025b/iso3166.tab',
     '2026-06-23',
     'Bulk country/territory jurisdiction seed used by this fixture.'),
    (3, NULL, 'standards',
     'RFC 5545 iCalendar recurrence rules',
     'https://www.rfc-editor.org/rfc/rfc5545',
     '2026-06-23',
     'Recurrence-rule representation baseline.'),
    (4, NULL, 'standards',
     'RFC 7529 non-Gregorian recurrence rules',
     'https://www.rfc-editor.org/rfc/rfc7529',
     '2026-06-23',
     'RSCALE support for non-Gregorian holiday recurrence.'),
    (5, NULL, 'standards',
     'Unicode LDML / CLDR locale and calendar identifiers',
     'https://www.unicode.org/reports/tr35/',
     '2026-06-23',
     'Calendar-system and subdivision identifier model.'),
    (6, NULL, 'other',
     'vacanza/holidays reference implementations',
     'https://github.com/vacanza/holidays',
     '2026-06-23',
     'Used as a practical cross-country seed reference for holiday rules and observance conventions.'),
    (7, 'GB-ENG', 'internal',
     'MARS English bank holiday seed rules',
     NULL,
     '2026-06-23',
     'Current repo seed data migrated into the generalized schema.'),
    (8, NULL, 'other',
     'workalendar reference implementations',
     'https://github.com/workalendar/workalendar',
     '2026-06-23',
     'Supplementary historical backfill source for jurisdictions where the primary import starts too late.'),
    (9, NULL, 'internal',
     'Automatically inferred first-class holiday rules from materialized history',
     NULL,
     '2026-06-23',
     'Generated country-level holiday_definition and holiday_rule rows inferred from historical materialized holidays.'),
    (10, 'ZA', 'statute',
     'Public Holidays Act, 1994 (Act No. 36 of 1994) of South Africa',
     NULL,
     '2026-06-23',
     'Primary legal basis for annual South African public holidays and Sunday-to-Monday observance.'),
    (11, 'ZA', 'government_site',
     'South African Government public holidays guidance',
     NULL,
     '2026-06-23',
     'Used to align holiday naming and present-day public guidance for South Africa.'),
    (12, 'DK', 'other',
     'Denmark public holiday reference set',
     NULL,
     '2026-06-23',
     'Used to hand-model Denmark''s current nationwide public holidays, including Great Prayer Day through 2023.'),
    (13, 'NL', 'other',
     'Netherlands public holiday reference set',
     NULL,
     '2026-06-23',
     'Used to hand-model the Netherlands'' current nationwide holidays, including King''s Day and dual Christmas days.'),
    (14, NULL, 'other',
     'Weekend reference set for seeded jurisdictions',
     NULL,
     '2026-06-23',
     'Used to seed jurisdiction-wide weekly rest-day rules where holiday observance behaviour depends on the normal weekend.'),
    (15, 'ZA', 'other',
     'Historical South Africa public holiday reference set',
     NULL,
     '2026-06-24',
     'Used to hand-model South Africa''s national public-holiday regimes from the Union period through the post-1994 calendar.'),
    (16, 'IE', 'other',
     'Historical Ireland public holiday reference set',
     NULL,
     '2026-06-24',
     'Used to hand-model Ireland''s public-holiday regimes from the Free State period onward, including the Whit Monday to June Holiday change.'),
    (17, 'NL', 'other',
     'Historical Netherlands royal-holiday reference set',
     NULL,
     '2026-06-24',
     'Used to hand-model the Netherlands'' royal-holiday transitions from Queen''s Day to King''s Day, including Sunday replacement behaviour.');

INSERT INTO jurisdiction_weekend_rule(
    weekend_rule_id,
    jurisdiction_id,
    weekend_mask,
    valid_from_year,
    valid_to_year,
    source_document_id,
    notes
)
VALUES
    (1, 'GB', '6,7', NULL, NULL, 14, 'United Kingdom Saturday-Sunday weekend baseline.'),
    (2, 'GB-ENG', '6,7', NULL, NULL, 14, 'England Saturday-Sunday weekend baseline.'),
    (3, 'GB-NIR', '6,7', NULL, NULL, 14, 'Northern Ireland Saturday-Sunday weekend baseline.'),
    (4, 'GB-SCT', '6,7', NULL, NULL, 14, 'Scotland Saturday-Sunday weekend baseline.'),
    (5, 'GB-WLS', '6,7', NULL, NULL, 14, 'Wales Saturday-Sunday weekend baseline.'),
    (6, 'AU', '6,7', NULL, NULL, 14, 'Australia Saturday-Sunday weekend baseline.'),
    (7, 'NZ', '6,7', NULL, NULL, 14, 'New Zealand Saturday-Sunday weekend baseline.'),
    (8, 'IE', '6,7', NULL, NULL, 14, 'Ireland Saturday-Sunday weekend baseline.'),
    (9, 'FR', '6,7', NULL, NULL, 14, 'France Saturday-Sunday weekend baseline.'),
    (10, 'DE', '6,7', NULL, NULL, 14, 'Germany Saturday-Sunday weekend baseline.'),
    (11, 'ZA', '6,7', NULL, NULL, 14, 'South Africa Saturday-Sunday weekend baseline.'),
    (12, 'DK', '6,7', NULL, NULL, 14, 'Denmark Saturday-Sunday weekend baseline.'),
    (13, 'NL', '6,7', NULL, NULL, 14, 'Netherlands Saturday-Sunday weekend baseline.'),
    (14, 'CA', '6,7', NULL, NULL, 14, 'Canada Saturday-Sunday weekend baseline.'),
    (15, 'US', '6,7', NULL, NULL, 14, 'United States Saturday-Sunday weekend baseline.'),
    (16, 'PT', '6,7', NULL, NULL, 14, 'Portugal Saturday-Sunday weekend baseline.'),
    (17, 'IT', '6,7', NULL, NULL, 14, 'Italy Saturday-Sunday weekend baseline.'),
    (18, 'GR', '6,7', NULL, NULL, 14, 'Greece Saturday-Sunday weekend baseline.'),
    (19, 'SA', '4,5', NULL, 2012, 14, 'Saudi Arabia Thursday-Friday weekend before the 2013 reform.'),
    (20, 'SA', '5,6', 2013, NULL, 14, 'Saudi Arabia Friday-Saturday weekend since 29 June 2013.'),
    (21, 'AE', '5,6', NULL, 2021, 14, 'United Arab Emirates Friday-Saturday weekend before the 2022 reform.'),
    (22, 'AE', '6,7', 2022, NULL, 14, 'United Arab Emirates Saturday-Sunday weekend from January 2022.'),
    (23, 'OM', '5,6', NULL, NULL, 14, 'Oman Friday-Saturday weekend baseline.'),
    (24, 'BH', '5,6', NULL, NULL, 14, 'Bahrain Friday-Saturday weekend baseline.'),
    (25, 'KW', '5,6', NULL, NULL, 14, 'Kuwait Friday-Saturday weekend baseline.'),
    (26, 'QA', '5,6', NULL, NULL, 14, 'Qatar Friday-Saturday weekend baseline.'),
    (27, 'JO', '5,6', NULL, NULL, 14, 'Jordan Friday-Saturday weekend baseline.'),
    (28, 'EG', '5,6', NULL, NULL, 14, 'Egypt Friday-Saturday weekend baseline.'),
    (29, 'BN', '5,7', NULL, NULL, 14, 'Brunei non-contiguous Friday-Sunday weekend baseline.'),
    (30, 'IR', '5', NULL, NULL, 14, 'Iran weekly rest-day baseline with Friday as the ordinary weekend holiday.');

INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (1, 'GB-ENG', 'new_years_day', 'New Years Day', 'bank', 'full_day', 'gregory', NULL, NULL,
     'Observed on the next weekday if 1 January falls on a weekend.'),
    (2, 'GB-ENG', 'good_friday', 'Good Friday', 'bank', 'full_day', 'gregory', NULL, NULL,
     'Two days before Gregorian Easter Sunday.'),
    (3, 'GB-ENG', 'easter_monday', 'Easter Monday', 'bank', 'full_day', 'gregory', NULL, NULL,
     'One day after Gregorian Easter Sunday.'),
    (4, 'GB-ENG', 'may_day_bank_holiday', 'May Day Bank Holiday', 'bank', 'full_day', 'gregory', NULL, NULL,
     'First Monday in May, except where a one-off exception replaces it.'),
    (5, 'GB-ENG', 'spring_bank_holiday', 'Spring Bank Holiday', 'bank', 'full_day', 'gregory', NULL, NULL,
     'Last Monday in May, except where a one-off exception replaces it.'),
    (6, 'GB-ENG', 'platinum_jubilee_bank_holiday', 'Platinum Jubilee Bank Holiday', 'special', 'full_day', 'gregory', 2022, 2022,
     'Additional one-off bank holiday.'),
    (7, 'GB-ENG', 'state_funeral_qe2', 'State Funeral of Queen Elizabeth II', 'special', 'full_day', 'gregory', 2022, 2022,
     'Additional one-off bank holiday.'),
    (8, 'GB-ENG', 'coronation_king_charles_iii', 'Coronation of King Charles III', 'special', 'full_day', 'gregory', 2023, 2023,
     'Additional one-off bank holiday.'),
    (9, 'GB-ENG', 'august_bank_holiday', 'August Bank Holiday', 'bank', 'full_day', 'gregory', NULL, NULL,
     'Last Monday in August.'),
    (10, 'GB-ENG', 'christmas_day', 'Christmas Day', 'bank', 'full_day', 'gregory', NULL, NULL,
     'Christmas Day with paired substitute handling.'),
    (11, 'GB-ENG', 'boxing_day', 'Boxing Day', 'bank', 'full_day', 'gregory', NULL, NULL,
     'Boxing Day with paired substitute handling.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (1, 'en-GB', 'New Years Day', 1),
    (2, 'en-GB', 'Good Friday', 1),
    (3, 'en-GB', 'Easter Monday', 1),
    (4, 'en-GB', 'May Day Bank Holiday', 1),
    (5, 'en-GB', 'Spring Bank Holiday', 1),
    (6, 'en-GB', 'Platinum Jubilee Bank Holiday', 1),
    (7, 'en-GB', 'State Funeral of Queen Elizabeth II', 1),
    (8, 'en-GB', 'Coronation of King Charles III', 1),
    (9, 'en-GB', 'August Bank Holiday', 1),
    (10, 'en-GB', 'Christmas Day', 1),
    (11, 'en-GB', 'Boxing Day', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    anchor_holiday_key,
    rrule_text,
    expression_language,
    expression_text,
    holiday_date,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (1, 1, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 10,
     'Base legal date.'),
    (2, 2, 1, 'easter_offset', NULL, NULL, NULL, NULL, -2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 20,
     'Relative to Gregorian Easter Sunday.'),
    (3, 3, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 30,
     'Relative to Gregorian Easter Sunday.'),
    (4, 4, 1, 'nth_weekday', 5, NULL, 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 40,
     'First Monday in May.'),
    (5, 5, 1, 'last_weekday', 5, NULL, 1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 50,
     'Last Monday in May.'),
    (6, 6, 1, 'one_off', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '2022-06-03', 2022, 2022, 60,
     'Additional bank holiday for the Platinum Jubilee.'),
    (7, 7, 1, 'one_off', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '2022-09-19', 2022, 2022, 70,
     'Additional bank holiday for the State Funeral of Queen Elizabeth II.'),
    (8, 8, 1, 'one_off', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '2023-05-08', 2023, 2023, 80,
     'Additional bank holiday for the Coronation of King Charles III.'),
    (9, 9, 1, 'last_weekday', 8, NULL, 1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 90,
     'Last Monday in August.'),
    (10, 10, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (11, 11, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 110,
     'Base legal date.'),
    (12, 4, 2, 'one_off', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '2020-05-08', 2020, 2020, 5,
     'VE Day exception date represented as a high-priority override candidate.'),
    (13, 5, 2, 'one_off', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '2022-06-02', 2022, 2022, 5,
     'Moved Spring Bank Holiday date for 2022.');

INSERT INTO holiday_observance_rule(
    observance_rule_id,
    holiday_id,
    applies_to_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    move_days,
    second_move_days,
    expression_language,
    expression_text,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (1, 1, 1, 'next_weekday', 'Bank Holiday in Lieu of New Years Day', NULL, 1,
     NULL, NULL, NULL, NULL, NULL, NULL, 10,
     'If 1 January falls on the ordinary weekend for the jurisdiction, observe on the following weekday.'),
    (2, 10, 10, 'christmas_pair', 'Bank Holiday in Lieu of Christmas Day', NULL, 1,
     NULL, NULL, NULL, NULL, NULL, NULL, 20,
     'Christmas and Boxing Day substitutions interact and must be resolved together.'),
    (3, 11, 11, 'christmas_pair', 'Bank Holiday in Lieu of Boxing Day', NULL, 1,
     NULL, NULL, NULL, NULL, NULL, NULL, 30,
     'Christmas and Boxing Day substitutions interact and must be resolved together.');

INSERT INTO holiday_exception(
    exception_id,
    jurisdiction_id,
    holiday_id,
    target_rule_id,
    holiday_date,
    action,
    name,
    replacement_holiday_key,
    expression_language,
    expression_text,
    valid_from_year,
    valid_to_year,
    priority,
    source_document_id,
    notes
)
VALUES
    (1, 'GB-ENG', 4, 4, '2020-05-08', 'replace',
     '75th anniversary of Victory in Europe (VE Day)', 'may_day_bank_holiday',
     NULL, NULL, 2020, 2020, 0, 7,
     'May Day moved from Monday 2020-05-04 to Friday 2020-05-08.'),
    (2, 'GB-ENG', 5, 5, '2022-06-02', 'replace',
     'Spring Bank Holiday', 'spring_bank_holiday',
     NULL, NULL, 2022, 2022, 0, 7,
     'Spring Bank Holiday moved from Monday 2022-05-30 to Thursday 2022-06-02.'),
    (3, 'GB-ENG', 6, 6, '2022-06-03', 'add',
     'Platinum Jubilee Bank Holiday', NULL,
     NULL, NULL, 2022, 2022, 0, 7,
     'Additional bank holiday for the Platinum Jubilee.'),
    (4, 'GB-ENG', 7, 7, '2022-09-19', 'add',
     'State Funeral of Queen Elizabeth II', NULL,
     NULL, NULL, 2022, 2022, 0, 7,
     'Additional bank holiday for the State Funeral of Queen Elizabeth II.'),
    (5, 'GB-ENG', 8, 8, '2023-05-08', 'add',
     'Coronation of King Charles III', NULL,
     NULL, NULL, 2023, 2023, 0, 7,
     'Additional bank holiday for the Coronation of King Charles III.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (1, 1, 1, NULL, 7, 'definition', 'English New Year and substitute-day seed rule.'),
    (2, 2, NULL, NULL, 7, 'definition', 'English Good Friday seed rule.'),
    (3, 3, NULL, NULL, 7, 'definition', 'English Easter Monday seed rule.'),
    (4, 4, NULL, 1, 7, 'exception', 'VE Day exception seed rule.'),
    (5, 5, NULL, 2, 7, 'exception', 'Moved Spring Bank Holiday seed rule.'),
    (6, 6, NULL, 3, 7, 'exception', 'Platinum Jubilee seed rule.'),
    (7, 7, NULL, 4, 7, 'exception', 'State Funeral seed rule.'),
    (8, 8, NULL, 5, 7, 'exception', 'Coronation seed rule.'),
    (9, 9, NULL, NULL, 7, 'definition', 'English August Bank Holiday seed rule.'),
    (10, 10, 2, NULL, 7, 'observance', 'Christmas substitution seed rule.'),
    (11, 11, 3, NULL, 7, 'observance', 'Boxing Day substitution seed rule.');

-- Australia national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (100, 'AU', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Country-level seed; subdivision-specific holidays remain separate.'),
    (101, 'AU', 'australia_day', 'Australia Day', 'public', 'full_day', 'gregory', 1935, NULL,
     'Seeded from vacanza/holidays national rules.'),
    (102, 'AU', 'good_friday', 'Good Friday', 'public', 'full_day', 'gregory', NULL, NULL,
     'Seeded from vacanza/holidays national rules.'),
    (103, 'AU', 'easter_monday', 'Easter Monday', 'public', 'full_day', 'gregory', NULL, NULL,
     'Seeded from vacanza/holidays national rules.'),
    (104, 'AU', 'anzac_day', 'Anzac Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Seeded from vacanza/holidays national rules.'),
    (105, 'AU', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Christmas Day with substitute handling.'),
    (106, 'AU', 'boxing_day', 'Boxing Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Boxing Day with substitute handling.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (100, 'en-AU', 'New Year''s Day', 1),
    (101, 'en-AU', 'Australia Day', 1),
    (102, 'en-AU', 'Good Friday', 1),
    (103, 'en-AU', 'Easter Monday', 1),
    (104, 'en-AU', 'Anzac Day', 1),
    (105, 'en-AU', 'Christmas Day', 1),
    (106, 'en-AU', 'Boxing Day', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    priority,
    notes
)
VALUES
    (100, 100, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (101, 101, 1, 'fixed_date', 1, 26, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (102, 102, 1, 'easter_offset', NULL, NULL, NULL, NULL, -2, 100,
     'Relative to Gregorian Easter Sunday.'),
    (103, 103, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, 100,
     'Relative to Gregorian Easter Sunday.'),
    (104, 104, 1, 'fixed_date', 4, 25, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (105, 105, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (106, 106, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, 100,
     'Base legal date.');

INSERT INTO holiday_observance_rule(
    observance_rule_id,
    holiday_id,
    applies_to_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    priority,
    notes
)
VALUES
    (100, 100, 100, 'next_weekday', 'New Year''s Day (observed)', NULL, 1, 100,
     'Australian observed-holiday convention for New Year''s Day falling on the ordinary weekend.'),
    (101, 101, 101, 'next_weekday', 'Australia Day (observed)', NULL, 1, 100,
     'Australian observed-holiday convention for Australia Day falling on the ordinary weekend.'),
    (102, 104, 104, 'next_weekday', 'Anzac Day (observed)', NULL, 1, 100,
     'Seed simplification for national-level observed Anzac Day handling tied to the ordinary weekend.'),
    (103, 105, 105, 'christmas_pair', 'Christmas Day (observed)', NULL, 1, 100,
     'Paired substitute handling for Christmas/Boxing Day.'),
    (104, 106, 106, 'christmas_pair', 'Boxing Day (observed)', NULL, 1, 100,
     'Paired substitute handling for Christmas/Boxing Day.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (100, 100, 100, NULL, 6, 'observance', 'Australia New Year seed rule.'),
    (101, 101, 101, NULL, 6, 'observance', 'Australia Day seed rule.'),
    (102, 102, NULL, NULL, 6, 'definition', 'Australia Good Friday seed rule.'),
    (103, 103, NULL, NULL, 6, 'definition', 'Australia Easter Monday seed rule.'),
    (104, 104, 102, NULL, 6, 'observance', 'Australia Anzac Day seed rule.'),
    (105, 105, 103, NULL, 6, 'observance', 'Australia Christmas seed rule.'),
    (106, 106, 104, NULL, 6, 'observance', 'Australia Boxing Day seed rule.');

-- New Zealand national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (120, 'NZ', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Observed using New Zealand Mondayisation conventions.'),
    (121, 'NZ', 'day_after_new_years_day', 'Day after New Year''s Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Observed using New Zealand Mondayisation conventions.'),
    (122, 'NZ', 'waitangi_day', 'Waitangi Day', 'public', 'full_day', 'gregory', 1974, NULL,
     'Observed since 2014 when falling on a weekend.'),
    (123, 'NZ', 'anzac_day', 'Anzac Day', 'public', 'full_day', 'gregory', 1921, NULL,
     'Observed since 2014 when falling on a weekend.'),
    (124, 'NZ', 'good_friday', 'Good Friday', 'public', 'full_day', 'gregory', NULL, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (125, 'NZ', 'easter_monday', 'Easter Monday', 'public', 'full_day', 'gregory', NULL, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (126, 'NZ', 'kings_birthday', 'King''s Birthday', 'public', 'full_day', 'gregory', 1902, NULL,
     'Name varies historically with the sovereign.'),
    (127, 'NZ', 'labour_day', 'Labour Day', 'public', 'full_day', 'gregory', 1899, NULL,
     'Fourth Monday in October in the modern system.'),
    (128, 'NZ', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Christmas Day with substitute handling.'),
    (129, 'NZ', 'boxing_day', 'Boxing Day', 'public', 'full_day', 'gregory', NULL, NULL,
     'Boxing Day with substitute handling.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (120, 'en-NZ', 'New Year''s Day', 1),
    (121, 'en-NZ', 'Day after New Year''s Day', 1),
    (122, 'en-NZ', 'Waitangi Day', 1),
    (123, 'en-NZ', 'Anzac Day', 1),
    (124, 'en-NZ', 'Good Friday', 1),
    (125, 'en-NZ', 'Easter Monday', 1),
    (126, 'en-NZ', 'King''s Birthday', 1),
    (127, 'en-NZ', 'Labour Day', 1),
    (128, 'en-NZ', 'Christmas Day', 1),
    (129, 'en-NZ', 'Boxing Day', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    valid_from_year,
    priority,
    notes
)
VALUES
    (120, 120, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (121, 121, 1, 'fixed_date', 1, 2, NULL, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (122, 122, 1, 'fixed_date', 2, 6, NULL, NULL, NULL, 1974, 100,
     'Base legal date.'),
    (123, 123, 1, 'fixed_date', 4, 25, NULL, NULL, NULL, 1921, 100,
     'Base legal date.'),
    (124, 124, 1, 'easter_offset', NULL, NULL, NULL, NULL, -2, NULL, 100,
     'Relative to Gregorian Easter Sunday.'),
    (125, 125, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, NULL, 100,
     'Relative to Gregorian Easter Sunday.'),
    (126, 126, 1, 'nth_weekday', 6, NULL, 1, 1, NULL, 1902, 100,
     'First Monday in June.'),
    (127, 127, 1, 'nth_weekday', 10, NULL, 1, 4, NULL, 1910, 100,
     'Fourth Monday in October in the modern system.'),
    (128, 128, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, NULL, 100,
     'Base legal date.'),
    (129, 129, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, NULL, 100,
     'Base legal date.');

INSERT INTO holiday_observance_rule(
    observance_rule_id,
    holiday_id,
    applies_to_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    valid_from_year,
    priority,
    notes
)
VALUES
    (120, 120, 120, 'next_weekday', 'New Year''s Day (observed)', NULL, 1, NULL, 100,
     'If 1 January falls on the ordinary weekend, observe on the next weekday.'),
    (121, 121, 121, 'next_non_holiday', 'Day after New Year''s Day (observed)', NULL, 1, NULL, 100,
     'Move to the next non-holiday weekday after an ordinary-weekend clash to avoid overlap with New Year''s Day.'),
    (122, 122, 122, 'next_monday', 'Waitangi Day (observed)', NULL, 1, 2014, 100,
     'Mondayised from 2014.'),
    (123, 123, 123, 'next_monday', 'Anzac Day (observed)', NULL, 1, 2014, 100,
     'Mondayised from 2014.'),
    (124, 128, 128, 'christmas_pair', 'Christmas Day (observed)', NULL, 1, NULL, 100,
     'Paired substitute handling for Christmas/Boxing Day.'),
    (125, 129, 129, 'christmas_pair', 'Boxing Day (observed)', NULL, 1, NULL, 100,
     'Paired substitute handling for Christmas/Boxing Day.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (120, 120, 120, NULL, 6, 'observance', 'New Zealand New Year seed rule.'),
    (121, 121, 121, NULL, 6, 'observance', 'New Zealand Day after New Year seed rule.'),
    (122, 122, 122, NULL, 6, 'observance', 'New Zealand Waitangi Day seed rule.'),
    (123, 123, 123, NULL, 6, 'observance', 'New Zealand Anzac Day seed rule.'),
    (124, 124, NULL, NULL, 6, 'definition', 'New Zealand Good Friday seed rule.'),
    (125, 125, NULL, NULL, 6, 'definition', 'New Zealand Easter Monday seed rule.'),
    (126, 126, NULL, NULL, 6, 'definition', 'New Zealand Sovereign''s Birthday seed rule.'),
    (127, 127, NULL, NULL, 6, 'definition', 'New Zealand Labour Day seed rule.'),
    (128, 128, 124, NULL, 6, 'observance', 'New Zealand Christmas seed rule.'),
    (129, 129, 125, NULL, 6, 'observance', 'New Zealand Boxing Day seed rule.');

-- Ireland national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (140, 'IE', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', 1974, NULL,
     'Country-level public holiday seed.'),
    (141, 'IE', 'saint_brigids_day', 'Saint Brigid''s Day', 'public', 'full_day', 'gregory', 2023, NULL,
     '1 February when Friday, otherwise first Monday from 1 February.'),
    (142, 'IE', 'saint_patricks_day', 'Saint Patrick''s Day', 'public', 'full_day', 'gregory', 1903, NULL,
     'Country-level public holiday seed.'),
    (143, 'IE', 'easter_monday', 'Easter Monday', 'public', 'full_day', 'gregory', 1926, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (144, 'IE', 'may_day', 'May Day', 'public', 'full_day', 'gregory', 1994, NULL,
     'Usually first Monday in May.'),
    (145, 'IE', 'june_bank_holiday', 'June Bank Holiday', 'public', 'full_day', 'gregory', 1973, NULL,
     'First Monday in June.'),
    (146, 'IE', 'august_bank_holiday', 'August Bank Holiday', 'public', 'full_day', 'gregory', 1926, NULL,
     'First Monday in August.'),
    (147, 'IE', 'october_bank_holiday', 'October Bank Holiday', 'public', 'full_day', 'gregory', 1977, NULL,
     'Last Monday in October.'),
    (148, 'IE', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', 1926, NULL,
     'Country-level public holiday seed.'),
    (149, 'IE', 'saint_stephens_day', 'Saint Stephen''s Day', 'public', 'full_day', 'gregory', 1926, NULL,
     'Country-level public holiday seed.'),
    (150, 'IE', 'whit_monday', 'Whit Monday', 'public', 'full_day', 'gregory', 1926, 1972,
     'Historic holiday observed on the Monday after Pentecost before the move to the June Holiday in 1973.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (140, 'en-IE', 'New Year''s Day', 1),
    (141, 'en-IE', 'Saint Brigid''s Day', 1),
    (142, 'en-IE', 'Saint Patrick''s Day', 1),
    (143, 'en-IE', 'Easter Monday', 1),
    (144, 'en-IE', 'May Day', 1),
    (145, 'en-IE', 'June Bank Holiday', 1),
    (146, 'en-IE', 'August Bank Holiday', 1),
    (147, 'en-IE', 'October Bank Holiday', 1),
    (148, 'en-IE', 'Christmas Day', 1),
    (149, 'en-IE', 'Saint Stephen''s Day', 1),
    (150, 'en-IE', 'Whit Monday', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (140, 140, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, 1974, NULL, 100,
     'Base legal date.'),
    (141, 141, 1, 'weekday_after_date', 2, 1, 1, NULL, NULL, 2023, NULL, 100,
     'First Monday from 1 February.'),
    (142, 142, 1, 'fixed_date', 3, 17, NULL, NULL, NULL, 1903, NULL, 100,
     'Base legal date.'),
    (143, 143, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, 1926, NULL, 100,
     'Relative to Gregorian Easter Sunday.'),
    (144, 144, 1, 'nth_weekday', 5, NULL, 1, 1, NULL, 1994, NULL, 100,
     'First Monday in May.'),
    (145, 145, 1, 'nth_weekday', 6, NULL, 1, 1, NULL, 1973, NULL, 100,
     'First Monday in June.'),
    (146, 146, 1, 'nth_weekday', 8, NULL, 1, 1, NULL, 1926, NULL, 100,
     'First Monday in August.'),
    (147, 147, 1, 'last_weekday', 10, NULL, 1, -1, NULL, 1977, NULL, 100,
     'Last Monday in October.'),
    (148, 148, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, 1926, NULL, 100,
     'Base legal date.'),
    (149, 149, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, 1926, NULL, 100,
     'Base legal date.'),
    (150, 150, 1, 'easter_offset', NULL, NULL, NULL, NULL, 50, 1926, 1972, 100,
     'Whit Monday before replacement by the June Holiday.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (140, 140, NULL, NULL, 6, 'definition', 'Ireland New Year seed rule.'),
    (141, 141, NULL, NULL, 6, 'definition', 'Ireland Saint Brigid''s Day seed rule.'),
    (142, 142, NULL, NULL, 6, 'definition', 'Ireland Saint Patrick''s Day seed rule.'),
    (143, 143, NULL, NULL, 6, 'definition', 'Ireland Easter Monday seed rule.'),
    (144, 144, NULL, NULL, 6, 'definition', 'Ireland May Day seed rule.'),
    (145, 145, NULL, NULL, 6, 'definition', 'Ireland June Bank Holiday seed rule.'),
    (146, 146, NULL, NULL, 6, 'definition', 'Ireland August Bank Holiday seed rule.'),
    (147, 147, NULL, NULL, 6, 'definition', 'Ireland October Bank Holiday seed rule.'),
    (148, 148, NULL, NULL, 6, 'definition', 'Ireland Christmas Day seed rule.'),
    (149, 149, NULL, NULL, 6, 'definition', 'Ireland Saint Stephen''s Day seed rule.'),
    (150, 150, NULL, NULL, 16, 'definition', 'Ireland Whit Monday historic rule window.');

-- France national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (160, 'FR', 'new_years_day', 'Jour de l''an', 'public', 'full_day', 'gregory', 1811, NULL,
     'Country-level public holiday seed.'),
    (161, 'FR', 'easter_monday', 'Lundi de Pâques', 'public', 'full_day', 'gregory', 1886, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (162, 'FR', 'labour_day', 'Fête du Travail', 'public', 'full_day', 'gregory', 1948, NULL,
     'Country-level public holiday seed.'),
    (163, 'FR', 'victory_day', 'Fête de la Victoire', 'public', 'full_day', 'gregory', 1982, NULL,
     '8 May victory commemoration.'),
    (164, 'FR', 'ascension_day', 'Ascension', 'public', 'full_day', 'gregory', NULL, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (165, 'FR', 'whit_monday', 'Lundi de Pentecôte', 'public', 'full_day', 'gregory', 1886, NULL,
     'Removed during 2005-2007 and restored from 2008.'),
    (166, 'FR', 'national_day', 'Fête nationale', 'public', 'full_day', 'gregory', 1880, NULL,
     'Bastille Day.'),
    (167, 'FR', 'assumption_day', 'Assomption', 'public', 'full_day', 'gregory', NULL, NULL,
     'Country-level public holiday seed.'),
    (168, 'FR', 'all_saints_day', 'Toussaint', 'public', 'full_day', 'gregory', NULL, NULL,
     'Country-level public holiday seed.'),
    (169, 'FR', 'armistice_day', 'Armistice', 'public', 'full_day', 'gregory', 1922, NULL,
     '11 November armistice commemoration.'),
    (170, 'FR', 'christmas_day', 'Noël', 'public', 'full_day', 'gregory', NULL, NULL,
     'Country-level public holiday seed.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (160, 'fr-FR', 'Jour de l''an', 1),
    (161, 'fr-FR', 'Lundi de Pâques', 1),
    (162, 'fr-FR', 'Fête du Travail', 1),
    (163, 'fr-FR', 'Fête de la Victoire', 1),
    (164, 'fr-FR', 'Ascension', 1),
    (165, 'fr-FR', 'Lundi de Pentecôte', 1),
    (166, 'fr-FR', 'Fête nationale', 1),
    (167, 'fr-FR', 'Assomption', 1),
    (168, 'fr-FR', 'Toussaint', 1),
    (169, 'fr-FR', 'Armistice', 1),
    (170, 'fr-FR', 'Noël', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (160, 160, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, 1811, NULL, 100, 'Base legal date.'),
    (161, 161, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, 1886, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (162, 162, 1, 'fixed_date', 5, 1, NULL, NULL, NULL, 1948, NULL, 100, 'Base legal date.'),
    (163, 163, 1, 'fixed_date', 5, 8, NULL, NULL, NULL, 1982, NULL, 100, 'Base legal date.'),
    (164, 164, 1, 'easter_offset', NULL, NULL, NULL, NULL, 39, NULL, NULL, 100, 'Ascension Thursday.'),
    (165, 165, 1, 'easter_offset', NULL, NULL, NULL, NULL, 50, 1886, 2004, 100, 'Whit Monday before temporary removal.'),
    (166, 165, 2, 'easter_offset', NULL, NULL, NULL, NULL, 50, 2008, NULL, 100, 'Whit Monday after restoration.'),
    (167, 166, 1, 'fixed_date', 7, 14, NULL, NULL, NULL, 1880, NULL, 100, 'Base legal date.'),
    (168, 167, 1, 'fixed_date', 8, 15, NULL, NULL, NULL, NULL, NULL, 100, 'Base legal date.'),
    (169, 168, 1, 'fixed_date', 11, 1, NULL, NULL, NULL, NULL, NULL, 100, 'Base legal date.'),
    (170, 169, 1, 'fixed_date', 11, 11, NULL, NULL, NULL, 1922, NULL, 100, 'Base legal date.'),
    (171, 170, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, NULL, NULL, 100, 'Base legal date.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (160, 160, NULL, NULL, 6, 'definition', 'France New Year seed rule.'),
    (161, 161, NULL, NULL, 6, 'definition', 'France Easter Monday seed rule.'),
    (162, 162, NULL, NULL, 6, 'definition', 'France Labour Day seed rule.'),
    (163, 163, NULL, NULL, 6, 'definition', 'France Victory Day seed rule.'),
    (164, 164, NULL, NULL, 6, 'definition', 'France Ascension seed rule.'),
    (165, 165, NULL, NULL, 6, 'definition', 'France Whit Monday seed rule, first interval.'),
    (165, 166, NULL, NULL, 6, 'definition', 'France Whit Monday seed rule, restored interval.'),
    (166, 167, NULL, NULL, 6, 'definition', 'France National Day seed rule.'),
    (167, 168, NULL, NULL, 6, 'definition', 'France Assumption seed rule.'),
    (168, 169, NULL, NULL, 6, 'definition', 'France All Saints'' Day seed rule.'),
    (169, 170, NULL, NULL, 6, 'definition', 'France Armistice Day seed rule.'),
    (170, 171, NULL, NULL, 6, 'definition', 'France Christmas seed rule.');

-- Germany national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (180, 'DE', 'new_years_day', 'Neujahr', 'public', 'full_day', 'gregory', 1991, NULL,
     'Country-level public holiday seed.'),
    (181, 'DE', 'good_friday', 'Karfreitag', 'public', 'full_day', 'gregory', 1991, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (182, 'DE', 'easter_monday', 'Ostermontag', 'public', 'full_day', 'gregory', 1991, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (183, 'DE', 'labour_day', 'Erster Mai', 'public', 'full_day', 'gregory', 1991, NULL,
     'Country-level public holiday seed.'),
    (184, 'DE', 'ascension_day', 'Christi Himmelfahrt', 'public', 'full_day', 'gregory', 1991, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (185, 'DE', 'whit_monday', 'Pfingstmontag', 'public', 'full_day', 'gregory', 1991, NULL,
     'Relative to Gregorian Easter Sunday.'),
    (186, 'DE', 'german_unity_day', 'Tag der Deutschen Einheit', 'public', 'full_day', 'gregory', 1991, NULL,
     'Country-level public holiday seed.'),
    (187, 'DE', 'christmas_day', 'Erster Weihnachtstag', 'public', 'full_day', 'gregory', 1991, NULL,
     'Country-level public holiday seed.'),
    (188, 'DE', 'second_day_of_christmas', 'Zweiter Weihnachtstag', 'public', 'full_day', 'gregory', 1991, NULL,
     'Country-level public holiday seed.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (180, 'de-DE', 'Neujahr', 1),
    (181, 'de-DE', 'Karfreitag', 1),
    (182, 'de-DE', 'Ostermontag', 1),
    (183, 'de-DE', 'Erster Mai', 1),
    (184, 'de-DE', 'Christi Himmelfahrt', 1),
    (185, 'de-DE', 'Pfingstmontag', 1),
    (186, 'de-DE', 'Tag der Deutschen Einheit', 1),
    (187, 'de-DE', 'Erster Weihnachtstag', 1),
    (188, 'de-DE', 'Zweiter Weihnachtstag', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    valid_from_year,
    priority,
    notes
)
VALUES
    (180, 180, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, 1991, 100, 'Base legal date.'),
    (181, 181, 1, 'easter_offset', NULL, NULL, NULL, NULL, -2, 1991, 100, 'Relative to Gregorian Easter Sunday.'),
    (182, 182, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, 1991, 100, 'Relative to Gregorian Easter Sunday.'),
    (183, 183, 1, 'fixed_date', 5, 1, NULL, NULL, NULL, 1991, 100, 'Base legal date.'),
    (184, 184, 1, 'easter_offset', NULL, NULL, NULL, NULL, 39, 1991, 100, 'Ascension Thursday.'),
    (185, 185, 1, 'easter_offset', NULL, NULL, NULL, NULL, 50, 1991, 100, 'Whit Monday.'),
    (186, 186, 1, 'fixed_date', 10, 3, NULL, NULL, NULL, 1991, 100, 'Base legal date.'),
    (187, 187, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, 1991, 100, 'Base legal date.'),
    (188, 188, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, 1991, 100, 'Base legal date.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (180, 180, NULL, NULL, 6, 'definition', 'Germany New Year seed rule.'),
    (181, 181, NULL, NULL, 6, 'definition', 'Germany Good Friday seed rule.'),
    (182, 182, NULL, NULL, 6, 'definition', 'Germany Easter Monday seed rule.'),
    (183, 183, NULL, NULL, 6, 'definition', 'Germany Labour Day seed rule.'),
    (184, 184, NULL, NULL, 6, 'definition', 'Germany Ascension seed rule.'),
    (185, 185, NULL, NULL, 6, 'definition', 'Germany Whit Monday seed rule.'),
    (186, 186, NULL, NULL, 6, 'definition', 'Germany Unity Day seed rule.'),
    (187, 187, NULL, NULL, 6, 'definition', 'Germany Christmas Day seed rule.'),
    (188, 188, NULL, NULL, 6, 'definition', 'Germany Second Day of Christmas seed rule.');

-- South Africa national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (190, 'ZA', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (191, 'ZA', 'human_rights_day', 'Human Rights Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (192, 'ZA', 'good_friday', 'Good Friday', 'public', 'full_day', 'gregory', 1910, NULL,
     'Two days before Gregorian Easter Sunday.'),
    (193, 'ZA', 'family_day', 'Family Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Monday after Gregorian Easter Sunday.'),
    (194, 'ZA', 'freedom_day', 'Freedom Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (195, 'ZA', 'workers_day', 'Workers'' Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (196, 'ZA', 'youth_day', 'Youth Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (197, 'ZA', 'national_womens_day', 'National Women''s Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (198, 'ZA', 'heritage_day', 'Heritage Day', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (199, 'ZA', 'day_of_reconciliation', 'Day of Reconciliation', 'public', 'full_day', 'gregory', 1995, NULL,
     'Observed on Monday only when the holiday falls on a Sunday.'),
    (200, 'ZA', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', 1910, NULL,
     'Observed on Monday only when the holiday falls on a Sunday; special extra holidays may still be declared separately.'),
    (201, 'ZA', 'day_of_goodwill', 'Day of Goodwill', 'public', 'full_day', 'gregory', 1980, NULL,
     'Observed on Monday only when the holiday falls on a Sunday; special extra holidays may still be declared separately.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (190, 'en-ZA', 'New Year''s Day', 1),
    (191, 'en-ZA', 'Human Rights Day', 1),
    (192, 'en-ZA', 'Good Friday', 1),
    (193, 'en-ZA', 'Family Day', 1),
    (194, 'en-ZA', 'Freedom Day', 1),
    (195, 'en-ZA', 'Workers'' Day', 1),
    (196, 'en-ZA', 'Youth Day', 1),
    (197, 'en-ZA', 'National Women''s Day', 1),
    (198, 'en-ZA', 'Heritage Day', 1),
    (199, 'en-ZA', 'Day of Reconciliation', 1),
    (200, 'en-ZA', 'Christmas Day', 1),
    (201, 'en-ZA', 'Day of Goodwill', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    anchor_holiday_key,
    rrule_text,
    expression_language,
    expression_text,
    holiday_date,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (190, 190, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (191, 191, 1, 'fixed_date', 3, 21, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (192, 192, 1, 'easter_offset', NULL, NULL, NULL, NULL, -2, NULL, NULL, NULL, NULL, NULL, 1910, NULL, 100,
     'Relative to Gregorian Easter Sunday.'),
    (193, 193, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Relative to Gregorian Easter Sunday.'),
    (194, 194, 1, 'fixed_date', 4, 27, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (195, 195, 1, 'fixed_date', 5, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (196, 196, 1, 'fixed_date', 6, 16, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (197, 197, 1, 'fixed_date', 8, 9, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (198, 198, 1, 'fixed_date', 9, 24, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (199, 199, 1, 'fixed_date', 12, 16, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Base legal date.'),
    (200, 200, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1910, NULL, 100,
     'Base legal date.'),
    (201, 201, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1980, NULL, 100,
     'Base legal date.');

INSERT INTO holiday_observance_rule(
    observance_rule_id,
    holiday_id,
    applies_to_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    move_days,
    second_move_days,
    expression_language,
    expression_text,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (190, 190, 190, 'next_monday', 'New Year''s Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Under the Public Holidays Act, when a public holiday falls on a Sunday the following Monday is a public holiday.'),
    (191, 191, 191, 'next_monday', 'Human Rights Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Sunday-only Monday observance.'),
    (194, 194, 194, 'next_monday', 'Freedom Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Sunday-only Monday observance.'),
    (195, 195, 195, 'next_monday', 'Workers'' Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Sunday-only Monday observance.'),
    (196, 196, 196, 'next_monday', 'Youth Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Sunday-only Monday observance.'),
    (197, 197, 197, 'next_monday', 'National Women''s Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Sunday-only Monday observance.'),
    (198, 198, 198, 'next_monday', 'Heritage Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Sunday-only Monday observance.'),
    (199, 199, 199, 'next_monday', 'Day of Reconciliation (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1995, NULL, 100,
     'Sunday-only Monday observance.'),
    (200, 200, 200, 'next_monday', 'Christmas Day (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1910, NULL, 100,
     'Sunday-only Monday observance; special presidential decrees may add an extra day in some years.'),
    (201, 201, 201, 'next_monday', 'Day of Goodwill (observed)', '7', 0,
     NULL, NULL, NULL, NULL, 1980, NULL, 100,
     'Sunday-only Monday observance; special presidential decrees may add an extra day in some years.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (190, 190, 190, NULL, 10, 'observance', 'South Africa New Year''s Day legal rule.'),
    (191, 191, 191, NULL, 10, 'observance', 'South Africa Human Rights Day legal rule.'),
    (192, 192, NULL, NULL, 10, 'definition', 'South Africa Good Friday legal rule.'),
    (193, 193, NULL, NULL, 10, 'definition', 'South Africa Family Day legal rule.'),
    (194, 194, 194, NULL, 10, 'observance', 'South Africa Freedom Day legal rule.'),
    (195, 195, 195, NULL, 10, 'observance', 'South Africa Workers'' Day legal rule.'),
    (196, 196, 196, NULL, 10, 'observance', 'South Africa Youth Day legal rule.'),
    (197, 197, 197, NULL, 10, 'observance', 'South Africa National Women''s Day legal rule.'),
    (198, 198, 198, NULL, 10, 'observance', 'South Africa Heritage Day legal rule.'),
    (199, 199, 199, NULL, 10, 'observance', 'South Africa Day of Reconciliation legal rule.'),
    (200, 200, 200, NULL, 10, 'observance', 'South Africa Christmas Day legal rule.'),
    (201, 201, 201, NULL, 10, 'observance', 'South Africa Day of Goodwill legal rule.');

-- Denmark national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (210, 'DK', 'new_years_day', 'Nytarsdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Nationwide public holiday.'),
    (211, 'DK', 'maundy_thursday', 'Skartorsdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Thursday before Easter Sunday.'),
    (212, 'DK', 'good_friday', 'Langfredag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Friday before Easter Sunday.'),
    (213, 'DK', 'easter_sunday', 'Paskedag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Gregorian Easter Sunday.'),
    (214, 'DK', 'easter_monday', 'Anden paskedag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Monday after Easter Sunday.'),
    (215, 'DK', 'great_prayer_day', 'Store bededag', 'public', 'full_day', 'gregory', 1926, 2023,
     'Great Prayer Day remained a public holiday through 2023.'),
    (216, 'DK', 'ascension_day', 'Kristi himmelfartsdag', 'public', 'full_day', 'gregory', 1926, NULL,
     '39 days after Easter Sunday.'),
    (217, 'DK', 'whit_sunday', 'Pinsedag', 'public', 'full_day', 'gregory', 1926, NULL,
     '49 days after Easter Sunday.'),
    (218, 'DK', 'whit_monday', 'Anden pinsedag', 'public', 'full_day', 'gregory', 1926, NULL,
     '50 days after Easter Sunday.'),
    (219, 'DK', 'christmas_day', 'Juledag', 'public', 'full_day', 'gregory', 1926, NULL,
     'First Christmas Day; Christmas Eve is culturally important but not modeled here as a nationwide public holiday.'),
    (220, 'DK', 'second_christmas_day', 'Anden juledag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Second Christmas Day.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (210, 'da-DK', 'Nytarsdag', 1),
    (211, 'da-DK', 'Skartorsdag', 1),
    (212, 'da-DK', 'Langfredag', 1),
    (213, 'da-DK', 'Paskedag', 1),
    (214, 'da-DK', 'Anden paskedag', 1),
    (215, 'da-DK', 'Store bededag', 1),
    (216, 'da-DK', 'Kristi himmelfartsdag', 1),
    (217, 'da-DK', 'Pinsedag', 1),
    (218, 'da-DK', 'Anden pinsedag', 1),
    (219, 'da-DK', 'Juledag', 1),
    (220, 'da-DK', 'Anden juledag', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    anchor_holiday_key,
    rrule_text,
    expression_language,
    expression_text,
    holiday_date,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (210, 210, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Base legal date.'),
    (211, 211, 1, 'easter_offset', NULL, NULL, NULL, NULL, -3, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (212, 212, 1, 'easter_offset', NULL, NULL, NULL, NULL, -2, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (213, 213, 1, 'easter_offset', NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Gregorian Easter Sunday.'),
    (214, 214, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (215, 215, 1, 'easter_offset', NULL, NULL, NULL, NULL, 26, NULL, NULL, NULL, NULL, NULL, 1926, 2023, 100, 'Relative to Gregorian Easter Sunday.'),
    (216, 216, 1, 'easter_offset', NULL, NULL, NULL, NULL, 39, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (217, 217, 1, 'easter_offset', NULL, NULL, NULL, NULL, 49, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (218, 218, 1, 'easter_offset', NULL, NULL, NULL, NULL, 50, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (219, 219, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Base legal date.'),
    (220, 220, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Base legal date.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (210, 210, NULL, NULL, 12, 'definition', 'Denmark New Year seed rule.'),
    (211, 211, NULL, NULL, 12, 'definition', 'Denmark Maundy Thursday seed rule.'),
    (212, 212, NULL, NULL, 12, 'definition', 'Denmark Good Friday seed rule.'),
    (213, 213, NULL, NULL, 12, 'definition', 'Denmark Easter Sunday seed rule.'),
    (214, 214, NULL, NULL, 12, 'definition', 'Denmark Easter Monday seed rule.'),
    (215, 215, NULL, NULL, 12, 'definition', 'Denmark Great Prayer Day seed rule through 2023.'),
    (216, 216, NULL, NULL, 12, 'definition', 'Denmark Ascension Day seed rule.'),
    (217, 217, NULL, NULL, 12, 'definition', 'Denmark Whit Sunday seed rule.'),
    (218, 218, NULL, NULL, 12, 'definition', 'Denmark Whit Monday seed rule.'),
    (219, 219, NULL, NULL, 12, 'definition', 'Denmark Christmas Day seed rule.'),
    (220, 220, NULL, NULL, 12, 'definition', 'Denmark Second Christmas Day seed rule.');

-- Netherlands national/public holiday seed rules.
INSERT INTO holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
VALUES
    (230, 'NL', 'new_years_day', 'Nieuwjaarsdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Nationwide New Year holiday.'),
    (231, 'NL', 'good_friday', 'Goede Vrijdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'National holiday, though not universally a mandatory paid day off.'),
    (232, 'NL', 'easter_sunday', 'Eerste Paasdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'First Easter Day.'),
    (233, 'NL', 'easter_monday', 'Tweede Paasdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Second Easter Day.'),
    (234, 'NL', 'kings_day', 'Koningsdag', 'public', 'full_day', 'gregory', 2014, NULL,
     '27 April, moved to the previous weekday when 27 April falls on a Sunday.'),
    (235, 'NL', 'liberation_day', 'Bevrijdingsdag', 'public', 'full_day', 'gregory', 1945, NULL,
     'National holiday, but employer practice differs on whether it is a paid day off every year.'),
    (236, 'NL', 'ascension_day', 'Hemelvaartsdag', 'public', 'full_day', 'gregory', 1926, NULL,
     '39 days after Easter Sunday.'),
    (237, 'NL', 'whit_sunday', 'Eerste Pinksterdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'First Pentecost Day.'),
    (238, 'NL', 'whit_monday', 'Tweede Pinksterdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Second Pentecost Day.'),
    (239, 'NL', 'christmas_day', 'Eerste Kerstdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'First Christmas Day; Christmas Eve is widely celebrated but not modeled here as a nationwide public holiday.'),
    (240, 'NL', 'second_christmas_day', 'Tweede Kerstdag', 'public', 'full_day', 'gregory', 1926, NULL,
     'Second Christmas Day.');

INSERT INTO holiday_name(holiday_id, locale, localized_name, is_primary)
VALUES
    (230, 'nl-NL', 'Nieuwjaarsdag', 1),
    (231, 'nl-NL', 'Goede Vrijdag', 1),
    (232, 'nl-NL', 'Eerste Paasdag', 1),
    (233, 'nl-NL', 'Tweede Paasdag', 1),
    (234, 'nl-NL', 'Koningsdag', 1),
    (235, 'nl-NL', 'Bevrijdingsdag', 1),
    (236, 'nl-NL', 'Hemelvaartsdag', 1),
    (237, 'nl-NL', 'Eerste Pinksterdag', 1),
    (238, 'nl-NL', 'Tweede Pinksterdag', 1),
    (239, 'nl-NL', 'Eerste Kerstdag', 1),
    (240, 'nl-NL', 'Tweede Kerstdag', 1);

INSERT INTO holiday_rule(
    rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    anchor_holiday_key,
    rrule_text,
    expression_language,
    expression_text,
    holiday_date,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (230, 230, 1, 'fixed_date', 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Base legal date.'),
    (231, 231, 1, 'easter_offset', NULL, NULL, NULL, NULL, -2, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (232, 232, 1, 'easter_offset', NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Gregorian Easter Sunday.'),
    (233, 233, 1, 'easter_offset', NULL, NULL, NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (234, 234, 1, 'fixed_date', 4, 27, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 2014, NULL, 100, 'Base legal date from 2014 onward.'),
    (235, 235, 1, 'fixed_date', 5, 5, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1945, NULL, 100, 'Base legal date.'),
    (236, 236, 1, 'easter_offset', NULL, NULL, NULL, NULL, 39, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (237, 237, 1, 'easter_offset', NULL, NULL, NULL, NULL, 49, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (238, 238, 1, 'easter_offset', NULL, NULL, NULL, NULL, 50, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Relative to Gregorian Easter Sunday.'),
    (239, 239, 1, 'fixed_date', 12, 25, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Base legal date.'),
    (240, 240, 1, 'fixed_date', 12, 26, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1926, NULL, 100, 'Base legal date.');

INSERT INTO holiday_observance_rule(
    observance_rule_id,
    holiday_id,
    applies_to_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    move_days,
    second_move_days,
    expression_language,
    expression_text,
    valid_from_year,
    valid_to_year,
    priority,
    notes
)
VALUES
    (230, 234, 234, 'previous_weekday', 'Koningsdag', '7', 1,
     NULL, NULL, NULL, NULL, 2014, NULL, 100,
     'When 27 April falls on a Sunday, King''s Day is observed on the previous weekday.');

INSERT INTO holiday_rule_source(
    holiday_id,
    rule_id,
    observance_rule_id,
    exception_id,
    source_document_id,
    role,
    notes
)
VALUES
    (230, 230, NULL, NULL, 13, 'definition', 'Netherlands New Year seed rule.'),
    (231, 231, NULL, NULL, 13, 'definition', 'Netherlands Good Friday seed rule.'),
    (232, 232, NULL, NULL, 13, 'definition', 'Netherlands First Easter Day seed rule.'),
    (233, 233, NULL, NULL, 13, 'definition', 'Netherlands Second Easter Day seed rule.'),
    (234, 234, 230, NULL, 13, 'observance', 'Netherlands King''s Day seed rule with Sunday replacement.'),
    (235, 235, NULL, NULL, 13, 'definition', 'Netherlands Liberation Day seed rule.'),
    (236, 236, NULL, NULL, 13, 'definition', 'Netherlands Ascension Day seed rule.'),
    (237, 237, NULL, NULL, 13, 'definition', 'Netherlands First Pentecost Day seed rule.'),
    (238, 238, NULL, NULL, 13, 'definition', 'Netherlands Second Pentecost Day seed rule.'),
    (239, 239, NULL, NULL, 13, 'definition', 'Netherlands First Christmas Day seed rule.'),
    (240, 240, NULL, NULL, 13, 'definition', 'Netherlands Second Christmas Day seed rule.');

.read packaging/holiday-db/mars_generated_first_class_rules.sql
.read packaging/holiday-db/mars_manual_first_class_rules.sql
