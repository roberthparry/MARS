-- SQLCipher jurisdiction database source script.
-- The installer supplies the chosen pragma key before reading this file.
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
--     expects expression_text to be a single sql select that returns one
--     holiday date in YYYY-MM-DD form for the bound rule year/context.
--   - Targeted subdivision coverage is loaded alongside the worldwide country
--     set so UI clients can expose first-class jurisdiction choices such as
--     AU-NSW and AU-VIC instead of only the national AU calendar.

pragma foreign_keys = on;

drop view if exists jurisdiction;
drop view if exists calendar_system;
drop view if exists source_document;
drop view if exists timezone_era;
drop view if exists timezone_transition_rule;
drop view if exists holiday_definition;
drop view if exists holiday_name;
drop view if exists holiday_rule;
drop view if exists holiday_observance_rule;
drop view if exists jurisdiction_weekend_rule;
drop view if exists holiday_exception;
drop view if exists holiday_rule_source;
drop view if exists holiday_instance;
drop view if exists jurisdiction_town;
drop table if exists jurisdiction_location_default_notes;
drop table if exists jurisdiction_location_default_locality;
drop table if exists jurisdiction_location_default_timezone;
drop table if exists jurisdiction_location_default_longitude;
drop table if exists jurisdiction_location_default_latitude;
drop table if exists jurisdiction_location_default;
drop table if exists jurisdiction_default_town;
drop table if exists jurisdiction_town_timezone;
drop table if exists jurisdiction_town_elevation;
drop table if exists jurisdiction_town_longitude;
drop table if exists jurisdiction_town_latitude;
drop table if exists jurisdiction_town_name;
drop table if exists jurisdiction_town_jurisdiction_id;
drop table if exists jurisdiction_town_entity;
drop table if exists timezone_notes;
drop table if exists timezone_canonical;
drop table if exists timezone_code;
drop table if exists timezone_definition;
drop table if exists jurisdiction_entity;
drop table if exists jurisdiction_parent_jurisdiction_id;
drop table if exists jurisdiction_jurisdiction_type;
drop table if exists jurisdiction_iso_country_code;
drop table if exists jurisdiction_iso_subdivision_code;
drop table if exists jurisdiction_cldr_region_code;
drop table if exists jurisdiction_name;
drop table if exists jurisdiction_valid_from_year;
drop table if exists jurisdiction_valid_to_year;
drop table if exists jurisdiction_notes;
drop table if exists calendar_system_entity;
drop table if exists calendar_system_cldr_rscale;
drop table if exists calendar_system_name;
drop table if exists calendar_system_family;
drop table if exists calendar_system_notes;
drop table if exists source_document_entity;
drop table if exists source_document_jurisdiction_id;
drop table if exists source_document_source_type;
drop table if exists source_document_citation;
drop table if exists source_document_source_url;
drop table if exists source_document_published_on;
drop table if exists source_document_accessed_on;
drop table if exists source_document_notes;
drop table if exists timezone_era_entity;
drop table if exists timezone_era_timezone_name;
drop table if exists timezone_era_sequence_no;
drop table if exists timezone_era_gmtoff_minutes;
drop table if exists timezone_era_rules_kind;
drop table if exists timezone_era_fixed_save_minutes;
drop table if exists timezone_era_rule_name;
drop table if exists timezone_era_format_text;
drop table if exists timezone_era_until_year;
drop table if exists timezone_era_until_month;
drop table if exists timezone_era_until_day_kind;
drop table if exists timezone_era_until_day_value;
drop table if exists timezone_era_until_weekday;
drop table if exists timezone_era_until_seconds;
drop table if exists timezone_era_until_suffix;
drop table if exists timezone_transition_rule_entity;
drop table if exists timezone_transition_rule_rule_name;
drop table if exists timezone_transition_rule_from_year;
drop table if exists timezone_transition_rule_to_year;
drop table if exists timezone_transition_rule_in_month;
drop table if exists timezone_transition_rule_on_kind;
drop table if exists timezone_transition_rule_on_day;
drop table if exists timezone_transition_rule_on_weekday;
drop table if exists timezone_transition_rule_at_seconds;
drop table if exists timezone_transition_rule_at_suffix;
drop table if exists timezone_transition_rule_save_minutes;
drop table if exists timezone_transition_rule_letters;
drop table if exists holiday_definition_entity;
drop table if exists holiday_definition_jurisdiction_id;
drop table if exists holiday_definition_holiday_key;
drop table if exists holiday_definition_default_name;
drop table if exists holiday_definition_holiday_class;
drop table if exists holiday_definition_scope;
drop table if exists holiday_definition_calendar_system_id;
drop table if exists holiday_definition_valid_from_year;
drop table if exists holiday_definition_valid_to_year;
drop table if exists holiday_definition_is_active;
drop table if exists holiday_definition_notes;
drop table if exists holiday_name_entity;
drop table if exists holiday_name_holiday_id;
drop table if exists holiday_name_locale;
drop table if exists holiday_name_localized_name;
drop table if exists holiday_name_is_primary;
drop table if exists holiday_rule_entity;
drop table if exists holiday_rule_holiday_id;
drop table if exists holiday_rule_sequence_no;
drop table if exists holiday_rule_rule_kind;
drop table if exists holiday_rule_month;
drop table if exists holiday_rule_day;
drop table if exists holiday_rule_weekday;
drop table if exists holiday_rule_ordinal;
drop table if exists holiday_rule_offset_days;
drop table if exists holiday_rule_anchor_holiday_key;
drop table if exists holiday_rule_rrule_text;
drop table if exists holiday_rule_expression_language;
drop table if exists holiday_rule_expression_text;
drop table if exists holiday_rule_holiday_date;
drop table if exists holiday_rule_valid_from_year;
drop table if exists holiday_rule_valid_to_year;
drop table if exists holiday_rule_priority;
drop table if exists holiday_rule_notes;
drop table if exists holiday_observance_rule_entity;
drop table if exists holiday_observance_rule_holiday_id;
drop table if exists holiday_observance_rule_holiday_rule_id;
drop table if exists holiday_observance_rule_observed_rule_kind;
drop table if exists holiday_observance_rule_observed_name;
drop table if exists holiday_observance_rule_weekend_mask;
drop table if exists holiday_observance_rule_suppress_original;
drop table if exists holiday_observance_rule_move_days;
drop table if exists holiday_observance_rule_second_move_days;
drop table if exists holiday_observance_rule_expression_language;
drop table if exists holiday_observance_rule_expression_text;
drop table if exists holiday_observance_rule_valid_from_year;
drop table if exists holiday_observance_rule_valid_to_year;
drop table if exists holiday_observance_rule_priority;
drop table if exists holiday_observance_rule_notes;
drop table if exists jurisdiction_weekend_rule_entity;
drop table if exists jurisdiction_weekend_rule_jurisdiction_id;
drop table if exists jurisdiction_weekend_rule_weekend_mask;
drop table if exists jurisdiction_weekend_rule_valid_from_year;
drop table if exists jurisdiction_weekend_rule_valid_to_year;
drop table if exists jurisdiction_weekend_rule_source_document_id;
drop table if exists jurisdiction_weekend_rule_notes;
drop table if exists holiday_exception_entity;
drop table if exists holiday_exception_jurisdiction_id;
drop table if exists holiday_exception_holiday_id;
drop table if exists holiday_exception_holiday_rule_id;
drop table if exists holiday_exception_holiday_date;
drop table if exists holiday_exception_action;
drop table if exists holiday_exception_name;
drop table if exists holiday_exception_replacement_holiday_key;
drop table if exists holiday_exception_expression_language;
drop table if exists holiday_exception_expression_text;
drop table if exists holiday_exception_valid_from_year;
drop table if exists holiday_exception_valid_to_year;
drop table if exists holiday_exception_priority;
drop table if exists holiday_exception_source_document_id;
drop table if exists holiday_exception_notes;
drop table if exists holiday_rule_source_entity;
drop table if exists holiday_rule_source_holiday_id;
drop table if exists holiday_rule_source_holiday_rule_id;
drop table if exists holiday_rule_source_holiday_observance_rule_id;
drop table if exists holiday_rule_source_holiday_exception_id;
drop table if exists holiday_rule_source_source_document_id;
drop table if exists holiday_rule_source_role;
drop table if exists holiday_rule_source_notes;
drop table if exists holiday_instance_entity;
drop table if exists holiday_instance_jurisdiction_id;
drop table if exists holiday_instance_holiday_id;
drop table if exists holiday_instance_holiday_date;
drop table if exists holiday_instance_holiday_name;
drop table if exists holiday_instance_holiday_class;
drop table if exists holiday_instance_language;
drop table if exists holiday_instance_source_document_id;

create table jurisdiction_entity (
    jurisdiction_id text primary key
);
create table jurisdiction_parent_jurisdiction_id (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    parent_jurisdiction_id text not null references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table jurisdiction_jurisdiction_type (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    jurisdiction_type text not null check (jurisdiction_type in ('country','subdivision','dependency','municipality','special','supranational'))
);
create table jurisdiction_iso_country_code (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    iso_country_code text not null
);
create table jurisdiction_iso_subdivision_code (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    iso_subdivision_code text not null
);
create table jurisdiction_cldr_region_code (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    cldr_region_code text not null
);
create table jurisdiction_name (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    name text not null
);
create table jurisdiction_valid_from_year (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    valid_from_year integer not null
);
create table jurisdiction_valid_to_year (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    valid_to_year integer not null
);
create table jurisdiction_notes (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    notes text not null
);

create table calendar_system_entity (
    calendar_system_id text primary key
);
create table calendar_system_cldr_rscale (
    calendar_system_id text primary key references calendar_system_entity(calendar_system_id) on delete cascade,
    cldr_rscale text not null
);
create table calendar_system_name (
    calendar_system_id text primary key references calendar_system_entity(calendar_system_id) on delete cascade,
    name text not null
);
create table calendar_system_family (
    calendar_system_id text primary key references calendar_system_entity(calendar_system_id) on delete cascade,
    family text not null check (family in ('solar','lunar','lunisolar','other'))
);
create table calendar_system_notes (
    calendar_system_id text primary key references calendar_system_entity(calendar_system_id) on delete cascade,
    notes text not null
);

create table source_document_entity (
    source_document_id integer primary key
);
create table source_document_jurisdiction_id (
    source_document_id integer primary key references source_document_entity(source_document_id) on delete cascade,
    jurisdiction_id text not null references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table source_document_source_type (
    source_document_id integer primary key references source_document_entity(source_document_id) on delete cascade,
    source_type text not null check (source_type in ('gazette','statute','government_site','standards','internal','other'))
);
create table source_document_citation (
    source_document_id integer primary key references source_document_entity(source_document_id) on delete cascade,
    citation text not null
);
create table source_document_source_url (
    source_document_id integer primary key references source_document_entity(source_document_id) on delete cascade,
    source_url text not null
);
create table source_document_published_on (
    source_document_id integer primary key references source_document_entity(source_document_id) on delete cascade,
    published_on text not null
);
create table source_document_accessed_on (
    source_document_id integer primary key references source_document_entity(source_document_id) on delete cascade,
    accessed_on text not null
);
create table source_document_notes (
    source_document_id integer primary key references source_document_entity(source_document_id) on delete cascade,
    notes text not null
);

create table timezone_era_entity (
    timezone_era_id integer primary key
);
create table timezone_era_timezone_name (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    timezone_name text not null references timezone_definition(timezone_name) on delete cascade
);
create table timezone_era_sequence_no (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    sequence_no integer not null
);
create table timezone_era_gmtoff_minutes (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    gmtoff_minutes integer not null
);
create table timezone_era_rules_kind (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    rules_kind text not null check (rules_kind in ('none','fixed','named'))
);
create table timezone_era_fixed_save_minutes (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    fixed_save_minutes integer not null
);
create table timezone_era_rule_name (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    rule_name text not null
);
create table timezone_era_format_text (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    format_text text not null
);
create table timezone_era_until_year (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    until_year integer not null
);
create table timezone_era_until_month (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    until_month integer not null check (until_month between 1 and 12)
);
create table timezone_era_until_day_kind (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    until_day_kind text not null check (until_day_kind in ('day_of_month','last_weekday','weekday_on_or_after','weekday_on_or_before'))
);
create table timezone_era_until_day_value (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    until_day_value integer not null
);
create table timezone_era_until_weekday (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    until_weekday integer not null check (until_weekday between 1 and 7)
);
create table timezone_era_until_seconds (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    until_seconds integer not null
);
create table timezone_era_until_suffix (
    timezone_era_id integer primary key references timezone_era_entity(timezone_era_id) on delete cascade,
    until_suffix text not null check (until_suffix in ('w','s','u','g','z'))
);

create table timezone_transition_rule_entity (
    timezone_transition_rule_id integer primary key
);
create table timezone_transition_rule_rule_name (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    rule_name text not null
);
create table timezone_transition_rule_from_year (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    from_year integer not null
);
create table timezone_transition_rule_to_year (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    to_year integer not null
);
create table timezone_transition_rule_in_month (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    in_month integer not null check (in_month between 1 and 12)
);
create table timezone_transition_rule_on_kind (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    on_kind text not null check (on_kind in ('day_of_month','last_weekday','weekday_on_or_after','weekday_on_or_before'))
);
create table timezone_transition_rule_on_day (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    on_day integer not null
);
create table timezone_transition_rule_on_weekday (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    on_weekday integer not null check (on_weekday between 1 and 7)
);
create table timezone_transition_rule_at_seconds (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    at_seconds integer not null
);
create table timezone_transition_rule_at_suffix (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    at_suffix text not null check (at_suffix in ('w','s','u','g','z'))
);
create table timezone_transition_rule_save_minutes (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    save_minutes integer not null
);
create table timezone_transition_rule_letters (
    timezone_transition_rule_id integer primary key references timezone_transition_rule_entity(timezone_transition_rule_id) on delete cascade,
    letters text not null
);

create table holiday_definition_entity (
    holiday_id integer primary key
);
create table holiday_definition_jurisdiction_id (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    jurisdiction_id text not null references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table holiday_definition_holiday_key (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    holiday_key text not null
);
create table holiday_definition_default_name (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    default_name text not null
);
create table holiday_definition_holiday_class (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    holiday_class text not null check (holiday_class in ('public','bank','observance','school','religious','half_day','special'))
);
create table holiday_definition_scope (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    scope text not null check (scope in ('full_day','half_day','hours','market_close','school_only'))
);
create table holiday_definition_calendar_system_id (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    calendar_system_id text not null references calendar_system_entity(calendar_system_id)
);
create table holiday_definition_valid_from_year (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    valid_from_year integer not null
);
create table holiday_definition_valid_to_year (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    valid_to_year integer not null
);
create table holiday_definition_is_active (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    is_active text not null check (is_active in ('Y','N'))
);
create table holiday_definition_notes (
    holiday_id integer primary key references holiday_definition_entity(holiday_id) on delete cascade,
    notes text not null
);

create table holiday_name_entity (
    holiday_name_id integer primary key
);
create table holiday_name_holiday_id (
    holiday_name_id integer primary key references holiday_name_entity(holiday_name_id) on delete cascade,
    holiday_id integer not null references holiday_definition_entity(holiday_id) on delete cascade
);
create table holiday_name_locale (
    holiday_name_id integer primary key references holiday_name_entity(holiday_name_id) on delete cascade,
    locale text not null
);
create table holiday_name_localized_name (
    holiday_name_id integer primary key references holiday_name_entity(holiday_name_id) on delete cascade,
    localized_name text not null
);
create table holiday_name_is_primary (
    holiday_name_id integer primary key references holiday_name_entity(holiday_name_id) on delete cascade,
    is_primary text not null check (is_primary in ('Y','N'))
);

create table holiday_rule_entity (
    holiday_rule_id integer primary key
);
create table holiday_rule_holiday_id (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    holiday_id integer not null references holiday_definition_entity(holiday_id) on delete cascade
);
create table holiday_rule_sequence_no (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    sequence_no integer not null
);
create table holiday_rule_rule_kind (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    rule_kind text not null check (rule_kind in ('fixed_date','nth_weekday','last_weekday','weekday_after_date','weekday_before_date','relative_to_holiday','easter_offset','orthodox_easter_offset','rrule','algorithmic','one_off'))
);
create table holiday_rule_month (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    month integer not null check (month between 1 and 12)
);
create table holiday_rule_day (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    day integer not null check (day between 1 and 31)
);
create table holiday_rule_weekday (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    weekday integer not null check (weekday between 1 and 7)
);
create table holiday_rule_ordinal (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    ordinal integer not null
);
create table holiday_rule_offset_days (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    offset_days integer not null
);
create table holiday_rule_anchor_holiday_key (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    anchor_holiday_key text not null
);
create table holiday_rule_rrule_text (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    rrule_text text not null
);
create table holiday_rule_expression_language (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    expression_language text not null check (expression_language in ('mars_sql','rfc5545_rrule','json','text'))
);
create table holiday_rule_expression_text (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    expression_text text not null
);
create table holiday_rule_holiday_date (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    holiday_date text not null check (holiday_date glob '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]')
);
create table holiday_rule_valid_from_year (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    valid_from_year integer not null
);
create table holiday_rule_valid_to_year (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    valid_to_year integer not null
);
create table holiday_rule_priority (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    priority integer not null
);
create table holiday_rule_notes (
    holiday_rule_id integer primary key references holiday_rule_entity(holiday_rule_id) on delete cascade,
    notes text not null
);

create table holiday_observance_rule_entity (
    holiday_observance_rule_id integer primary key
);
create table holiday_observance_rule_holiday_id (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    holiday_id integer not null references holiday_definition_entity(holiday_id) on delete cascade
);
create table holiday_observance_rule_holiday_rule_id (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    holiday_rule_id integer not null references holiday_rule_entity(holiday_rule_id) on delete cascade
);
create table holiday_observance_rule_observed_rule_kind (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    observed_rule_kind text not null check (observed_rule_kind in ('none','next_weekday','next_monday','nearest_weekday','next_non_holiday','previous_weekday','christmas_pair','custom_expression'))
);
create table holiday_observance_rule_observed_name (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    observed_name text not null
);
create table holiday_observance_rule_weekend_mask (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    weekend_mask text not null check (weekend_mask <> '')
);
create table holiday_observance_rule_suppress_original (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    suppress_original text not null check (suppress_original in ('Y','N'))
);
create table holiday_observance_rule_move_days (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    move_days integer not null
);
create table holiday_observance_rule_second_move_days (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    second_move_days integer not null
);
create table holiday_observance_rule_expression_language (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    expression_language text not null check (expression_language in ('mars_sql','json','text'))
);
create table holiday_observance_rule_expression_text (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    expression_text text not null
);
create table holiday_observance_rule_valid_from_year (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    valid_from_year integer not null
);
create table holiday_observance_rule_valid_to_year (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    valid_to_year integer not null
);
create table holiday_observance_rule_priority (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    priority integer not null
);
create table holiday_observance_rule_notes (
    holiday_observance_rule_id integer primary key references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade,
    notes text not null
);

create table jurisdiction_weekend_rule_entity (
    jurisdiction_weekend_rule_id integer primary key
);
create table jurisdiction_weekend_rule_jurisdiction_id (
    jurisdiction_weekend_rule_id integer primary key references jurisdiction_weekend_rule_entity(jurisdiction_weekend_rule_id) on delete cascade,
    jurisdiction_id text not null references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table jurisdiction_weekend_rule_weekend_mask (
    jurisdiction_weekend_rule_id integer primary key references jurisdiction_weekend_rule_entity(jurisdiction_weekend_rule_id) on delete cascade,
    weekend_mask text not null
);
create table jurisdiction_weekend_rule_valid_from_year (
    jurisdiction_weekend_rule_id integer primary key references jurisdiction_weekend_rule_entity(jurisdiction_weekend_rule_id) on delete cascade,
    valid_from_year integer not null
);
create table jurisdiction_weekend_rule_valid_to_year (
    jurisdiction_weekend_rule_id integer primary key references jurisdiction_weekend_rule_entity(jurisdiction_weekend_rule_id) on delete cascade,
    valid_to_year integer not null
);
create table jurisdiction_weekend_rule_source_document_id (
    jurisdiction_weekend_rule_id integer primary key references jurisdiction_weekend_rule_entity(jurisdiction_weekend_rule_id) on delete cascade,
    source_document_id integer not null references source_document_entity(source_document_id) on delete set null
);
create table jurisdiction_weekend_rule_notes (
    jurisdiction_weekend_rule_id integer primary key references jurisdiction_weekend_rule_entity(jurisdiction_weekend_rule_id) on delete cascade,
    notes text not null
);

create table holiday_exception_entity (
    holiday_exception_id integer primary key
);
create table holiday_exception_jurisdiction_id (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    jurisdiction_id text not null references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table holiday_exception_holiday_id (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    holiday_id integer not null references holiday_definition_entity(holiday_id) on delete cascade
);
create table holiday_exception_holiday_rule_id (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    holiday_rule_id integer not null references holiday_rule_entity(holiday_rule_id) on delete cascade
);
create table holiday_exception_holiday_date (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    holiday_date text not null check (holiday_date glob '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]')
);
create table holiday_exception_action (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    action text not null check (action in ('add','replace','suppress','rename'))
);
create table holiday_exception_name (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    name text not null
);
create table holiday_exception_replacement_holiday_key (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    replacement_holiday_key text not null
);
create table holiday_exception_expression_language (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    expression_language text not null check (expression_language in ('mars_sql','json','text'))
);
create table holiday_exception_expression_text (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    expression_text text not null
);
create table holiday_exception_valid_from_year (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    valid_from_year integer not null
);
create table holiday_exception_valid_to_year (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    valid_to_year integer not null
);
create table holiday_exception_priority (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    priority integer not null
);
create table holiday_exception_source_document_id (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    source_document_id integer not null references source_document_entity(source_document_id) on delete set null
);
create table holiday_exception_notes (
    holiday_exception_id integer primary key references holiday_exception_entity(holiday_exception_id) on delete cascade,
    notes text not null
);

create table holiday_rule_source_entity (
    holiday_rule_source_id integer primary key
);
create table holiday_rule_source_holiday_id (
    holiday_rule_source_id integer primary key references holiday_rule_source_entity(holiday_rule_source_id) on delete cascade,
    holiday_id integer not null references holiday_definition_entity(holiday_id) on delete cascade
);
create table holiday_rule_source_holiday_rule_id (
    holiday_rule_source_id integer primary key references holiday_rule_source_entity(holiday_rule_source_id) on delete cascade,
    holiday_rule_id integer not null references holiday_rule_entity(holiday_rule_id) on delete cascade
);
create table holiday_rule_source_holiday_observance_rule_id (
    holiday_rule_source_id integer primary key references holiday_rule_source_entity(holiday_rule_source_id) on delete cascade,
    holiday_observance_rule_id integer not null references holiday_observance_rule_entity(holiday_observance_rule_id) on delete cascade
);
create table holiday_rule_source_holiday_exception_id (
    holiday_rule_source_id integer primary key references holiday_rule_source_entity(holiday_rule_source_id) on delete cascade,
    holiday_exception_id integer not null references holiday_exception_entity(holiday_exception_id) on delete cascade
);
create table holiday_rule_source_source_document_id (
    holiday_rule_source_id integer primary key references holiday_rule_source_entity(holiday_rule_source_id) on delete cascade,
    source_document_id integer not null references source_document_entity(source_document_id) on delete cascade
);
create table holiday_rule_source_role (
    holiday_rule_source_id integer primary key references holiday_rule_source_entity(holiday_rule_source_id) on delete cascade,
    role text not null check (role in ('definition','observance','exception','background'))
);
create table holiday_rule_source_notes (
    holiday_rule_source_id integer primary key references holiday_rule_source_entity(holiday_rule_source_id) on delete cascade,
    notes text not null
);

create table holiday_instance_entity (
    holiday_instance_id integer primary key
);
create table holiday_instance_jurisdiction_id (
    holiday_instance_id integer primary key references holiday_instance_entity(holiday_instance_id) on delete cascade,
    jurisdiction_id text not null references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table holiday_instance_holiday_id (
    holiday_instance_id integer primary key references holiday_instance_entity(holiday_instance_id) on delete cascade,
    holiday_id integer not null references holiday_definition_entity(holiday_id) on delete set null
);
create table holiday_instance_holiday_date (
    holiday_instance_id integer primary key references holiday_instance_entity(holiday_instance_id) on delete cascade,
    holiday_date text not null check (holiday_date glob '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]')
);
create table holiday_instance_holiday_name (
    holiday_instance_id integer primary key references holiday_instance_entity(holiday_instance_id) on delete cascade,
    holiday_name text not null
);
create table holiday_instance_holiday_class (
    holiday_instance_id integer primary key references holiday_instance_entity(holiday_instance_id) on delete cascade,
    holiday_class text not null check (holiday_class in ('public','bank','observance','school','religious','half_day','special'))
);
create table holiday_instance_language (
    holiday_instance_id integer primary key references holiday_instance_entity(holiday_instance_id) on delete cascade,
    language text not null
);
create table holiday_instance_source_document_id (
    holiday_instance_id integer primary key references holiday_instance_entity(holiday_instance_id) on delete cascade,
    source_document_id integer not null references source_document_entity(source_document_id) on delete set null
);

create table jurisdiction_location_default (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table jurisdiction_location_default_latitude (
    jurisdiction_id text primary key references jurisdiction_location_default(jurisdiction_id) on delete cascade,
    latitude text not null,
    check (latitude <> '')
);
create table jurisdiction_location_default_longitude (
    jurisdiction_id text primary key references jurisdiction_location_default(jurisdiction_id) on delete cascade,
    longitude text not null,
    check (longitude <> '')
);
create table jurisdiction_location_default_timezone (
    jurisdiction_id text primary key references jurisdiction_location_default(jurisdiction_id) on delete cascade,
    timezone_code integer not null references timezone_code(timezone_code)
);
create table jurisdiction_location_default_locality (
    jurisdiction_id text primary key references jurisdiction_location_default(jurisdiction_id) on delete cascade,
    locality_name text not null,
    check (locality_name <> '')
);
create table jurisdiction_location_default_notes (
    jurisdiction_id text primary key references jurisdiction_location_default(jurisdiction_id) on delete cascade,
    notes text not null,
    check (notes <> '')
);
create table jurisdiction_town_entity (
    jurisdiction_town_id integer primary key
);
create table jurisdiction_town_jurisdiction_id (
    jurisdiction_town_id integer primary key references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade,
    jurisdiction_id text not null references jurisdiction_entity(jurisdiction_id) on delete cascade
);
create table jurisdiction_town_name (
    jurisdiction_town_id integer primary key references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade,
    town_name text not null,
    check (town_name <> '')
);
create table jurisdiction_town_latitude (
    jurisdiction_town_id integer primary key references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade,
    latitude text not null,
    check (latitude <> '')
);
create table jurisdiction_town_longitude (
    jurisdiction_town_id integer primary key references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade,
    longitude text not null,
    check (longitude <> '')
);
create table jurisdiction_town_elevation (
    jurisdiction_town_id integer primary key references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade,
    elevation_metres text not null,
    check (elevation_metres <> '')
);
create table jurisdiction_town_timezone (
    jurisdiction_town_id integer primary key references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade,
    timezone_code integer not null references timezone_code(timezone_code)
);
create table jurisdiction_town_totality_seed_priority (
    jurisdiction_town_id integer primary key references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade,
    totality_seed_priority integer not null,
    check (totality_seed_priority >= 0)
);
create table jurisdiction_default_town (
    jurisdiction_id text primary key references jurisdiction_entity(jurisdiction_id) on delete cascade,
    jurisdiction_town_id integer not null references jurisdiction_town_entity(jurisdiction_town_id) on delete cascade
);
create trigger jurisdiction_default_town_insert_guard
before insert on jurisdiction_default_town
begin
    select raise(abort, 'default town does not belong to jurisdiction')
    where not exists (
        select 1
        from jurisdiction_town_jurisdiction_id as town_jurisdiction
        where town_jurisdiction.jurisdiction_town_id = new.jurisdiction_town_id
          and town_jurisdiction.jurisdiction_id = new.jurisdiction_id
    );
end;
create index idx_jurisdiction_town_jurisdiction_id on jurisdiction_town_jurisdiction_id(jurisdiction_id);
create index idx_jurisdiction_town_name on jurisdiction_town_name(town_name);
create index idx_jurisdiction_town_timezone_code on jurisdiction_town_timezone(timezone_code);
create table timezone_definition (
    timezone_name text primary key
);
create table timezone_code (
    timezone_name text primary key references timezone_definition(timezone_name) on delete cascade,
    timezone_code integer not null unique
);
create table timezone_canonical (
    timezone_name text primary key references timezone_definition(timezone_name) on delete cascade,
    canonical_timezone_name text not null references timezone_definition(timezone_name)
);
create table timezone_notes (
    timezone_name text primary key references timezone_definition(timezone_name) on delete cascade,
    notes text not null,
    check (notes <> '')
);

create view jurisdiction_town as
select
    e.jurisdiction_town_id as jurisdiction_town_id,
    jurisdiction_id.jurisdiction_id as jurisdiction_id,
    name.town_name as town_name
from jurisdiction_town_entity as e
left join jurisdiction_town_jurisdiction_id as jurisdiction_id
    on jurisdiction_id.jurisdiction_town_id = e.jurisdiction_town_id
left join jurisdiction_town_name as name
    on name.jurisdiction_town_id = e.jurisdiction_town_id
;
create trigger jurisdiction_town_insert
instead of insert on jurisdiction_town
begin
    select raise(abort, 'duplicate jurisdiction town')
    where exists (
        select 1
        from jurisdiction_town_jurisdiction_id as existing_jurisdiction
        join jurisdiction_town_name as existing_name
            on existing_name.jurisdiction_town_id = existing_jurisdiction.jurisdiction_town_id
        where existing_jurisdiction.jurisdiction_id = new.jurisdiction_id
          and existing_name.town_name = new.town_name
    );
    insert into jurisdiction_town_entity(jurisdiction_town_id) values (new.jurisdiction_town_id);
    insert into jurisdiction_town_jurisdiction_id(jurisdiction_town_id, jurisdiction_id)
    values (coalesce(new.jurisdiction_town_id, last_insert_rowid()), new.jurisdiction_id);
    insert into jurisdiction_town_name(jurisdiction_town_id, town_name)
    values (coalesce(new.jurisdiction_town_id, last_insert_rowid()), new.town_name);
end;

create view jurisdiction as
select
    e.jurisdiction_id as jurisdiction_id,
    parent_jurisdiction_id.parent_jurisdiction_id as parent_jurisdiction_id,
    jurisdiction_type.jurisdiction_type as jurisdiction_type,
    iso_country_code.iso_country_code as iso_country_code,
    iso_subdivision_code.iso_subdivision_code as iso_subdivision_code,
    cldr_region_code.cldr_region_code as cldr_region_code,
    name.name as name,
    valid_from_year.valid_from_year as valid_from_year,
    valid_to_year.valid_to_year as valid_to_year,
    notes.notes as notes
from jurisdiction_entity as e
left join jurisdiction_parent_jurisdiction_id as parent_jurisdiction_id on parent_jurisdiction_id.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_jurisdiction_type as jurisdiction_type on jurisdiction_type.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_iso_country_code as iso_country_code on iso_country_code.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_iso_subdivision_code as iso_subdivision_code on iso_subdivision_code.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_cldr_region_code as cldr_region_code on cldr_region_code.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_name as name on name.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_valid_from_year as valid_from_year on valid_from_year.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_valid_to_year as valid_to_year on valid_to_year.jurisdiction_id = e.jurisdiction_id
left join jurisdiction_notes as notes on notes.jurisdiction_id = e.jurisdiction_id
;
create trigger jurisdiction_insert
instead of insert on jurisdiction
begin
    insert into jurisdiction_entity(jurisdiction_id) values (new.jurisdiction_id);
    insert into jurisdiction_parent_jurisdiction_id(jurisdiction_id, parent_jurisdiction_id) select new.jurisdiction_id, new.parent_jurisdiction_id where new.parent_jurisdiction_id is not null;
    insert into jurisdiction_jurisdiction_type(jurisdiction_id, jurisdiction_type) values (new.jurisdiction_id, new.jurisdiction_type);
    insert into jurisdiction_iso_country_code(jurisdiction_id, iso_country_code) select new.jurisdiction_id, new.iso_country_code where new.iso_country_code is not null;
    insert into jurisdiction_iso_subdivision_code(jurisdiction_id, iso_subdivision_code) select new.jurisdiction_id, new.iso_subdivision_code where new.iso_subdivision_code is not null;
    insert into jurisdiction_cldr_region_code(jurisdiction_id, cldr_region_code) select new.jurisdiction_id, new.cldr_region_code where new.cldr_region_code is not null;
    insert into jurisdiction_name(jurisdiction_id, name) values (new.jurisdiction_id, new.name);
    insert into jurisdiction_valid_from_year(jurisdiction_id, valid_from_year) select new.jurisdiction_id, new.valid_from_year where new.valid_from_year is not null;
    insert into jurisdiction_valid_to_year(jurisdiction_id, valid_to_year) select new.jurisdiction_id, new.valid_to_year where new.valid_to_year is not null;
    insert into jurisdiction_notes(jurisdiction_id, notes) select new.jurisdiction_id, new.notes where new.notes is not null;
end;

create view calendar_system as
select
    e.calendar_system_id as calendar_system_id,
    cldr_rscale.cldr_rscale as cldr_rscale,
    name.name as name,
    family.family as family,
    notes.notes as notes
from calendar_system_entity as e
left join calendar_system_cldr_rscale as cldr_rscale on cldr_rscale.calendar_system_id = e.calendar_system_id
left join calendar_system_name as name on name.calendar_system_id = e.calendar_system_id
left join calendar_system_family as family on family.calendar_system_id = e.calendar_system_id
left join calendar_system_notes as notes on notes.calendar_system_id = e.calendar_system_id
;
create trigger calendar_system_insert
instead of insert on calendar_system
begin
    insert into calendar_system_entity(calendar_system_id) values (new.calendar_system_id);
    insert into calendar_system_cldr_rscale(calendar_system_id, cldr_rscale) values (new.calendar_system_id, new.cldr_rscale);
    insert into calendar_system_name(calendar_system_id, name) values (new.calendar_system_id, new.name);
    insert into calendar_system_family(calendar_system_id, family) values (new.calendar_system_id, new.family);
    insert into calendar_system_notes(calendar_system_id, notes) select new.calendar_system_id, new.notes where new.notes is not null;
end;

create view source_document as
select
    e.source_document_id as source_document_id,
    jurisdiction_id.jurisdiction_id as jurisdiction_id,
    source_type.source_type as source_type,
    citation.citation as citation,
    source_url.source_url as source_url,
    published_on.published_on as published_on,
    accessed_on.accessed_on as accessed_on,
    notes.notes as notes
from source_document_entity as e
left join source_document_jurisdiction_id as jurisdiction_id on jurisdiction_id.source_document_id = e.source_document_id
left join source_document_source_type as source_type on source_type.source_document_id = e.source_document_id
left join source_document_citation as citation on citation.source_document_id = e.source_document_id
left join source_document_source_url as source_url on source_url.source_document_id = e.source_document_id
left join source_document_published_on as published_on on published_on.source_document_id = e.source_document_id
left join source_document_accessed_on as accessed_on on accessed_on.source_document_id = e.source_document_id
left join source_document_notes as notes on notes.source_document_id = e.source_document_id
;
create trigger source_document_insert
instead of insert on source_document
begin
    insert into source_document_entity(source_document_id) values (new.source_document_id);
    insert into source_document_jurisdiction_id(source_document_id, jurisdiction_id) select coalesce(new.source_document_id, last_insert_rowid()), new.jurisdiction_id where new.jurisdiction_id is not null;
    insert into source_document_source_type(source_document_id, source_type) values (coalesce(new.source_document_id, last_insert_rowid()), new.source_type);
    insert into source_document_citation(source_document_id, citation) values (coalesce(new.source_document_id, last_insert_rowid()), new.citation);
    insert into source_document_source_url(source_document_id, source_url) select coalesce(new.source_document_id, last_insert_rowid()), new.source_url where new.source_url is not null;
    insert into source_document_published_on(source_document_id, published_on) select coalesce(new.source_document_id, last_insert_rowid()), new.published_on where new.published_on is not null;
    insert into source_document_accessed_on(source_document_id, accessed_on) select coalesce(new.source_document_id, last_insert_rowid()), new.accessed_on where new.accessed_on is not null;
    insert into source_document_notes(source_document_id, notes) select coalesce(new.source_document_id, last_insert_rowid()), new.notes where new.notes is not null;
end;

create view timezone_era as
select
    e.timezone_era_id as timezone_era_id,
    timezone_name.timezone_name as timezone_name,
    sequence_no.sequence_no as sequence_no,
    gmtoff_minutes.gmtoff_minutes as gmtoff_minutes,
    rules_kind.rules_kind as rules_kind,
    fixed_save_minutes.fixed_save_minutes as fixed_save_minutes,
    rule_name.rule_name as rule_name,
    format_text.format_text as format_text,
    until_year.until_year as until_year,
    until_month.until_month as until_month,
    until_day_kind.until_day_kind as until_day_kind,
    until_day_value.until_day_value as until_day_value,
    until_weekday.until_weekday as until_weekday,
    until_seconds.until_seconds as until_seconds,
    until_suffix.until_suffix as until_suffix
from timezone_era_entity as e
left join timezone_era_timezone_name as timezone_name on timezone_name.timezone_era_id = e.timezone_era_id
left join timezone_era_sequence_no as sequence_no on sequence_no.timezone_era_id = e.timezone_era_id
left join timezone_era_gmtoff_minutes as gmtoff_minutes on gmtoff_minutes.timezone_era_id = e.timezone_era_id
left join timezone_era_rules_kind as rules_kind on rules_kind.timezone_era_id = e.timezone_era_id
left join timezone_era_fixed_save_minutes as fixed_save_minutes on fixed_save_minutes.timezone_era_id = e.timezone_era_id
left join timezone_era_rule_name as rule_name on rule_name.timezone_era_id = e.timezone_era_id
left join timezone_era_format_text as format_text on format_text.timezone_era_id = e.timezone_era_id
left join timezone_era_until_year as until_year on until_year.timezone_era_id = e.timezone_era_id
left join timezone_era_until_month as until_month on until_month.timezone_era_id = e.timezone_era_id
left join timezone_era_until_day_kind as until_day_kind on until_day_kind.timezone_era_id = e.timezone_era_id
left join timezone_era_until_day_value as until_day_value on until_day_value.timezone_era_id = e.timezone_era_id
left join timezone_era_until_weekday as until_weekday on until_weekday.timezone_era_id = e.timezone_era_id
left join timezone_era_until_seconds as until_seconds on until_seconds.timezone_era_id = e.timezone_era_id
left join timezone_era_until_suffix as until_suffix on until_suffix.timezone_era_id = e.timezone_era_id
;
create trigger timezone_era_insert
instead of insert on timezone_era
begin
    insert into timezone_era_entity(timezone_era_id) values (new.timezone_era_id);
    insert into timezone_era_timezone_name(timezone_era_id, timezone_name) values (coalesce(new.timezone_era_id, last_insert_rowid()), new.timezone_name);
    insert into timezone_era_sequence_no(timezone_era_id, sequence_no) values (coalesce(new.timezone_era_id, last_insert_rowid()), new.sequence_no);
    insert into timezone_era_gmtoff_minutes(timezone_era_id, gmtoff_minutes) values (coalesce(new.timezone_era_id, last_insert_rowid()), new.gmtoff_minutes);
    insert into timezone_era_rules_kind(timezone_era_id, rules_kind) values (coalesce(new.timezone_era_id, last_insert_rowid()), new.rules_kind);
    insert into timezone_era_fixed_save_minutes(timezone_era_id, fixed_save_minutes) select coalesce(new.timezone_era_id, last_insert_rowid()), new.fixed_save_minutes where new.fixed_save_minutes is not null;
    insert into timezone_era_rule_name(timezone_era_id, rule_name) select coalesce(new.timezone_era_id, last_insert_rowid()), new.rule_name where new.rule_name is not null;
    insert into timezone_era_format_text(timezone_era_id, format_text) values (coalesce(new.timezone_era_id, last_insert_rowid()), new.format_text);
    insert into timezone_era_until_year(timezone_era_id, until_year) select coalesce(new.timezone_era_id, last_insert_rowid()), new.until_year where new.until_year is not null;
    insert into timezone_era_until_month(timezone_era_id, until_month) select coalesce(new.timezone_era_id, last_insert_rowid()), new.until_month where new.until_month is not null;
    insert into timezone_era_until_day_kind(timezone_era_id, until_day_kind) select coalesce(new.timezone_era_id, last_insert_rowid()), new.until_day_kind where new.until_day_kind is not null;
    insert into timezone_era_until_day_value(timezone_era_id, until_day_value) select coalesce(new.timezone_era_id, last_insert_rowid()), new.until_day_value where new.until_day_value is not null;
    insert into timezone_era_until_weekday(timezone_era_id, until_weekday) select coalesce(new.timezone_era_id, last_insert_rowid()), new.until_weekday where new.until_weekday is not null;
    insert into timezone_era_until_seconds(timezone_era_id, until_seconds) select coalesce(new.timezone_era_id, last_insert_rowid()), new.until_seconds where new.until_seconds is not null;
    insert into timezone_era_until_suffix(timezone_era_id, until_suffix) select coalesce(new.timezone_era_id, last_insert_rowid()), new.until_suffix where new.until_suffix is not null;
end;

create view timezone_transition_rule as
select
    e.timezone_transition_rule_id as timezone_transition_rule_id,
    rule_name.rule_name as rule_name,
    from_year.from_year as from_year,
    to_year.to_year as to_year,
    in_month.in_month as in_month,
    on_kind.on_kind as on_kind,
    on_day.on_day as on_day,
    on_weekday.on_weekday as on_weekday,
    at_seconds.at_seconds as at_seconds,
    at_suffix.at_suffix as at_suffix,
    save_minutes.save_minutes as save_minutes,
    letters.letters as letters
from timezone_transition_rule_entity as e
left join timezone_transition_rule_rule_name as rule_name on rule_name.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_from_year as from_year on from_year.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_to_year as to_year on to_year.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_in_month as in_month on in_month.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_on_kind as on_kind on on_kind.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_on_day as on_day on on_day.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_on_weekday as on_weekday on on_weekday.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_at_seconds as at_seconds on at_seconds.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_at_suffix as at_suffix on at_suffix.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_save_minutes as save_minutes on save_minutes.timezone_transition_rule_id = e.timezone_transition_rule_id
left join timezone_transition_rule_letters as letters on letters.timezone_transition_rule_id = e.timezone_transition_rule_id
;
create trigger timezone_transition_rule_insert
instead of insert on timezone_transition_rule
begin
    insert into timezone_transition_rule_entity(timezone_transition_rule_id) values (new.timezone_transition_rule_id);
    insert into timezone_transition_rule_rule_name(timezone_transition_rule_id, rule_name) values (coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.rule_name);
    insert into timezone_transition_rule_from_year(timezone_transition_rule_id, from_year) select coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.from_year where new.from_year is not null;
    insert into timezone_transition_rule_to_year(timezone_transition_rule_id, to_year) select coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.to_year where new.to_year is not null;
    insert into timezone_transition_rule_in_month(timezone_transition_rule_id, in_month) values (coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.in_month);
    insert into timezone_transition_rule_on_kind(timezone_transition_rule_id, on_kind) values (coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.on_kind);
    insert into timezone_transition_rule_on_day(timezone_transition_rule_id, on_day) values (coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.on_day);
    insert into timezone_transition_rule_on_weekday(timezone_transition_rule_id, on_weekday) select coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.on_weekday where new.on_weekday is not null;
    insert into timezone_transition_rule_at_seconds(timezone_transition_rule_id, at_seconds) values (coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.at_seconds);
    insert into timezone_transition_rule_at_suffix(timezone_transition_rule_id, at_suffix) values (coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.at_suffix);
    insert into timezone_transition_rule_save_minutes(timezone_transition_rule_id, save_minutes) values (coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.save_minutes);
    insert into timezone_transition_rule_letters(timezone_transition_rule_id, letters) select coalesce(new.timezone_transition_rule_id, last_insert_rowid()), new.letters where new.letters is not null;
end;

create view holiday_definition as
select
    e.holiday_id as holiday_id,
    jurisdiction_id.jurisdiction_id as jurisdiction_id,
    holiday_key.holiday_key as holiday_key,
    default_name.default_name as default_name,
    holiday_class.holiday_class as holiday_class,
    scope.scope as scope,
    calendar_system_id.calendar_system_id as calendar_system_id,
    valid_from_year.valid_from_year as valid_from_year,
    valid_to_year.valid_to_year as valid_to_year,
    is_active.is_active as is_active,
    notes.notes as notes
from holiday_definition_entity as e
left join holiday_definition_jurisdiction_id as jurisdiction_id on jurisdiction_id.holiday_id = e.holiday_id
left join holiday_definition_holiday_key as holiday_key on holiday_key.holiday_id = e.holiday_id
left join holiday_definition_default_name as default_name on default_name.holiday_id = e.holiday_id
left join holiday_definition_holiday_class as holiday_class on holiday_class.holiday_id = e.holiday_id
left join holiday_definition_scope as scope on scope.holiday_id = e.holiday_id
left join holiday_definition_calendar_system_id as calendar_system_id on calendar_system_id.holiday_id = e.holiday_id
left join holiday_definition_valid_from_year as valid_from_year on valid_from_year.holiday_id = e.holiday_id
left join holiday_definition_valid_to_year as valid_to_year on valid_to_year.holiday_id = e.holiday_id
left join holiday_definition_is_active as is_active on is_active.holiday_id = e.holiday_id
left join holiday_definition_notes as notes on notes.holiday_id = e.holiday_id
;
create trigger holiday_definition_insert
instead of insert on holiday_definition
begin
    insert into holiday_definition_entity(holiday_id) values (new.holiday_id);
    insert into holiday_definition_jurisdiction_id(holiday_id, jurisdiction_id) values (coalesce(new.holiday_id, last_insert_rowid()), new.jurisdiction_id);
    insert into holiday_definition_holiday_key(holiday_id, holiday_key) values (coalesce(new.holiday_id, last_insert_rowid()), new.holiday_key);
    insert into holiday_definition_default_name(holiday_id, default_name) values (coalesce(new.holiday_id, last_insert_rowid()), new.default_name);
    insert into holiday_definition_holiday_class(holiday_id, holiday_class) values (coalesce(new.holiday_id, last_insert_rowid()), new.holiday_class);
    insert into holiday_definition_scope(holiday_id, scope) values (coalesce(new.holiday_id, last_insert_rowid()), coalesce(new.scope, 'full_day'));
    insert into holiday_definition_calendar_system_id(holiday_id, calendar_system_id) values (coalesce(new.holiday_id, last_insert_rowid()), new.calendar_system_id);
    insert into holiday_definition_valid_from_year(holiday_id, valid_from_year) select coalesce(new.holiday_id, last_insert_rowid()), new.valid_from_year where new.valid_from_year is not null;
    insert into holiday_definition_valid_to_year(holiday_id, valid_to_year) select coalesce(new.holiday_id, last_insert_rowid()), new.valid_to_year where new.valid_to_year is not null;
    insert into holiday_definition_is_active(holiday_id, is_active) values (coalesce(new.holiday_id, last_insert_rowid()), coalesce(new.is_active, 'Y'));
    insert into holiday_definition_notes(holiday_id, notes) select coalesce(new.holiday_id, last_insert_rowid()), new.notes where new.notes is not null;
end;

create view holiday_name as
select
    e.holiday_name_id as holiday_name_id,
    holiday_id.holiday_id as holiday_id,
    locale.locale as locale,
    localized_name.localized_name as localized_name,
    is_primary.is_primary as is_primary
from holiday_name_entity as e
left join holiday_name_holiday_id as holiday_id on holiday_id.holiday_name_id = e.holiday_name_id
left join holiday_name_locale as locale on locale.holiday_name_id = e.holiday_name_id
left join holiday_name_localized_name as localized_name on localized_name.holiday_name_id = e.holiday_name_id
left join holiday_name_is_primary as is_primary on is_primary.holiday_name_id = e.holiday_name_id
;
create trigger holiday_name_insert
instead of insert on holiday_name
begin
    insert into holiday_name_entity(holiday_name_id) values (new.holiday_name_id);
    insert into holiday_name_holiday_id(holiday_name_id, holiday_id) values (coalesce(new.holiday_name_id, last_insert_rowid()), new.holiday_id);
    insert into holiday_name_locale(holiday_name_id, locale) values (coalesce(new.holiday_name_id, last_insert_rowid()), new.locale);
    insert into holiday_name_localized_name(holiday_name_id, localized_name) values (coalesce(new.holiday_name_id, last_insert_rowid()), new.localized_name);
    insert into holiday_name_is_primary(holiday_name_id, is_primary) values (coalesce(new.holiday_name_id, last_insert_rowid()), coalesce(new.is_primary, 'N'));
end;

create view holiday_rule as
select
    e.holiday_rule_id as holiday_rule_id,
    holiday_id.holiday_id as holiday_id,
    sequence_no.sequence_no as sequence_no,
    rule_kind.rule_kind as rule_kind,
    month.month as month,
    day.day as day,
    weekday.weekday as weekday,
    ordinal.ordinal as ordinal,
    offset_days.offset_days as offset_days,
    anchor_holiday_key.anchor_holiday_key as anchor_holiday_key,
    rrule_text.rrule_text as rrule_text,
    expression_language.expression_language as expression_language,
    expression_text.expression_text as expression_text,
    holiday_date.holiday_date as holiday_date,
    valid_from_year.valid_from_year as valid_from_year,
    valid_to_year.valid_to_year as valid_to_year,
    priority.priority as priority,
    notes.notes as notes
from holiday_rule_entity as e
left join holiday_rule_holiday_id as holiday_id on holiday_id.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_sequence_no as sequence_no on sequence_no.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_rule_kind as rule_kind on rule_kind.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_month as month on month.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_day as day on day.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_weekday as weekday on weekday.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_ordinal as ordinal on ordinal.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_offset_days as offset_days on offset_days.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_anchor_holiday_key as anchor_holiday_key on anchor_holiday_key.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_rrule_text as rrule_text on rrule_text.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_expression_language as expression_language on expression_language.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_expression_text as expression_text on expression_text.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_holiday_date as holiday_date on holiday_date.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_valid_from_year as valid_from_year on valid_from_year.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_valid_to_year as valid_to_year on valid_to_year.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_priority as priority on priority.holiday_rule_id = e.holiday_rule_id
left join holiday_rule_notes as notes on notes.holiday_rule_id = e.holiday_rule_id
;
create trigger holiday_rule_insert
instead of insert on holiday_rule
begin
    insert into holiday_rule_entity(holiday_rule_id) values (new.holiday_rule_id);
    insert into holiday_rule_holiday_id(holiday_rule_id, holiday_id) values (coalesce(new.holiday_rule_id, last_insert_rowid()), new.holiday_id);
    insert into holiday_rule_sequence_no(holiday_rule_id, sequence_no) values (coalesce(new.holiday_rule_id, last_insert_rowid()), coalesce(new.sequence_no, 1));
    insert into holiday_rule_rule_kind(holiday_rule_id, rule_kind) values (coalesce(new.holiday_rule_id, last_insert_rowid()), new.rule_kind);
    insert into holiday_rule_month(holiday_rule_id, month) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.month where new.month is not null;
    insert into holiday_rule_day(holiday_rule_id, day) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.day where new.day is not null;
    insert into holiday_rule_weekday(holiday_rule_id, weekday) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.weekday where new.weekday is not null;
    insert into holiday_rule_ordinal(holiday_rule_id, ordinal) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.ordinal where new.ordinal is not null;
    insert into holiday_rule_offset_days(holiday_rule_id, offset_days) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.offset_days where new.offset_days is not null;
    insert into holiday_rule_anchor_holiday_key(holiday_rule_id, anchor_holiday_key) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.anchor_holiday_key where new.anchor_holiday_key is not null;
    insert into holiday_rule_rrule_text(holiday_rule_id, rrule_text) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.rrule_text where new.rrule_text is not null;
    insert into holiday_rule_expression_language(holiday_rule_id, expression_language) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.expression_language where new.expression_language is not null;
    insert into holiday_rule_expression_text(holiday_rule_id, expression_text) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.expression_text where new.expression_text is not null;
    insert into holiday_rule_holiday_date(holiday_rule_id, holiday_date) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.holiday_date where new.holiday_date is not null;
    insert into holiday_rule_valid_from_year(holiday_rule_id, valid_from_year) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.valid_from_year where new.valid_from_year is not null;
    insert into holiday_rule_valid_to_year(holiday_rule_id, valid_to_year) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.valid_to_year where new.valid_to_year is not null;
    insert into holiday_rule_priority(holiday_rule_id, priority) values (coalesce(new.holiday_rule_id, last_insert_rowid()), coalesce(new.priority, 100));
    insert into holiday_rule_notes(holiday_rule_id, notes) select coalesce(new.holiday_rule_id, last_insert_rowid()), new.notes where new.notes is not null;
end;

create view holiday_observance_rule as
select
    e.holiday_observance_rule_id as holiday_observance_rule_id,
    holiday_id.holiday_id as holiday_id,
    holiday_rule_id.holiday_rule_id as holiday_rule_id,
    observed_rule_kind.observed_rule_kind as observed_rule_kind,
    observed_name.observed_name as observed_name,
    weekend_mask.weekend_mask as weekend_mask,
    suppress_original.suppress_original as suppress_original,
    move_days.move_days as move_days,
    second_move_days.second_move_days as second_move_days,
    expression_language.expression_language as expression_language,
    expression_text.expression_text as expression_text,
    valid_from_year.valid_from_year as valid_from_year,
    valid_to_year.valid_to_year as valid_to_year,
    priority.priority as priority,
    notes.notes as notes
from holiday_observance_rule_entity as e
left join holiday_observance_rule_holiday_id as holiday_id on holiday_id.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_holiday_rule_id as holiday_rule_id on holiday_rule_id.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_observed_rule_kind as observed_rule_kind on observed_rule_kind.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_observed_name as observed_name on observed_name.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_weekend_mask as weekend_mask on weekend_mask.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_suppress_original as suppress_original on suppress_original.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_move_days as move_days on move_days.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_second_move_days as second_move_days on second_move_days.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_expression_language as expression_language on expression_language.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_expression_text as expression_text on expression_text.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_valid_from_year as valid_from_year on valid_from_year.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_valid_to_year as valid_to_year on valid_to_year.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_priority as priority on priority.holiday_observance_rule_id = e.holiday_observance_rule_id
left join holiday_observance_rule_notes as notes on notes.holiday_observance_rule_id = e.holiday_observance_rule_id
;
create trigger holiday_observance_rule_insert
instead of insert on holiday_observance_rule
begin
    insert into holiday_observance_rule_entity(holiday_observance_rule_id) values (new.holiday_observance_rule_id);
    insert into holiday_observance_rule_holiday_id(holiday_observance_rule_id, holiday_id) values (coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.holiday_id);
    insert into holiday_observance_rule_holiday_rule_id(holiday_observance_rule_id, holiday_rule_id) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.holiday_rule_id where new.holiday_rule_id is not null;
    insert into holiday_observance_rule_observed_rule_kind(holiday_observance_rule_id, observed_rule_kind) values (coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.observed_rule_kind);
    insert into holiday_observance_rule_observed_name(holiday_observance_rule_id, observed_name) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.observed_name where new.observed_name is not null;
    insert into holiday_observance_rule_weekend_mask(holiday_observance_rule_id, weekend_mask) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.weekend_mask where new.weekend_mask is not null;
    insert into holiday_observance_rule_suppress_original(holiday_observance_rule_id, suppress_original) values (coalesce(new.holiday_observance_rule_id, last_insert_rowid()), coalesce(new.suppress_original, 'N'));
    insert into holiday_observance_rule_move_days(holiday_observance_rule_id, move_days) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.move_days where new.move_days is not null;
    insert into holiday_observance_rule_second_move_days(holiday_observance_rule_id, second_move_days) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.second_move_days where new.second_move_days is not null;
    insert into holiday_observance_rule_expression_language(holiday_observance_rule_id, expression_language) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.expression_language where new.expression_language is not null;
    insert into holiday_observance_rule_expression_text(holiday_observance_rule_id, expression_text) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.expression_text where new.expression_text is not null;
    insert into holiday_observance_rule_valid_from_year(holiday_observance_rule_id, valid_from_year) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.valid_from_year where new.valid_from_year is not null;
    insert into holiday_observance_rule_valid_to_year(holiday_observance_rule_id, valid_to_year) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.valid_to_year where new.valid_to_year is not null;
    insert into holiday_observance_rule_priority(holiday_observance_rule_id, priority) values (coalesce(new.holiday_observance_rule_id, last_insert_rowid()), coalesce(new.priority, 100));
    insert into holiday_observance_rule_notes(holiday_observance_rule_id, notes) select coalesce(new.holiday_observance_rule_id, last_insert_rowid()), new.notes where new.notes is not null;
end;

create view jurisdiction_weekend_rule as
select
    e.jurisdiction_weekend_rule_id as jurisdiction_weekend_rule_id,
    jurisdiction_id.jurisdiction_id as jurisdiction_id,
    weekend_mask.weekend_mask as weekend_mask,
    valid_from_year.valid_from_year as valid_from_year,
    valid_to_year.valid_to_year as valid_to_year,
    source_document_id.source_document_id as source_document_id,
    notes.notes as notes
from jurisdiction_weekend_rule_entity as e
left join jurisdiction_weekend_rule_jurisdiction_id as jurisdiction_id on jurisdiction_id.jurisdiction_weekend_rule_id = e.jurisdiction_weekend_rule_id
left join jurisdiction_weekend_rule_weekend_mask as weekend_mask on weekend_mask.jurisdiction_weekend_rule_id = e.jurisdiction_weekend_rule_id
left join jurisdiction_weekend_rule_valid_from_year as valid_from_year on valid_from_year.jurisdiction_weekend_rule_id = e.jurisdiction_weekend_rule_id
left join jurisdiction_weekend_rule_valid_to_year as valid_to_year on valid_to_year.jurisdiction_weekend_rule_id = e.jurisdiction_weekend_rule_id
left join jurisdiction_weekend_rule_source_document_id as source_document_id on source_document_id.jurisdiction_weekend_rule_id = e.jurisdiction_weekend_rule_id
left join jurisdiction_weekend_rule_notes as notes on notes.jurisdiction_weekend_rule_id = e.jurisdiction_weekend_rule_id
;
create trigger jurisdiction_weekend_rule_insert
instead of insert on jurisdiction_weekend_rule
begin
    insert into jurisdiction_weekend_rule_entity(jurisdiction_weekend_rule_id) values (new.jurisdiction_weekend_rule_id);
    insert into jurisdiction_weekend_rule_jurisdiction_id(jurisdiction_weekend_rule_id, jurisdiction_id) values (coalesce(new.jurisdiction_weekend_rule_id, last_insert_rowid()), new.jurisdiction_id);
    insert into jurisdiction_weekend_rule_weekend_mask(jurisdiction_weekend_rule_id, weekend_mask) values (coalesce(new.jurisdiction_weekend_rule_id, last_insert_rowid()), new.weekend_mask);
    insert into jurisdiction_weekend_rule_valid_from_year(jurisdiction_weekend_rule_id, valid_from_year) select coalesce(new.jurisdiction_weekend_rule_id, last_insert_rowid()), new.valid_from_year where new.valid_from_year is not null;
    insert into jurisdiction_weekend_rule_valid_to_year(jurisdiction_weekend_rule_id, valid_to_year) select coalesce(new.jurisdiction_weekend_rule_id, last_insert_rowid()), new.valid_to_year where new.valid_to_year is not null;
    insert into jurisdiction_weekend_rule_source_document_id(jurisdiction_weekend_rule_id, source_document_id) select coalesce(new.jurisdiction_weekend_rule_id, last_insert_rowid()), new.source_document_id where new.source_document_id is not null;
    insert into jurisdiction_weekend_rule_notes(jurisdiction_weekend_rule_id, notes) select coalesce(new.jurisdiction_weekend_rule_id, last_insert_rowid()), new.notes where new.notes is not null;
end;

create view holiday_exception as
select
    e.holiday_exception_id as holiday_exception_id,
    jurisdiction_id.jurisdiction_id as jurisdiction_id,
    holiday_id.holiday_id as holiday_id,
    holiday_rule_id.holiday_rule_id as holiday_rule_id,
    holiday_date.holiday_date as holiday_date,
    action.action as action,
    name.name as name,
    replacement_holiday_key.replacement_holiday_key as replacement_holiday_key,
    expression_language.expression_language as expression_language,
    expression_text.expression_text as expression_text,
    valid_from_year.valid_from_year as valid_from_year,
    valid_to_year.valid_to_year as valid_to_year,
    priority.priority as priority,
    source_document_id.source_document_id as source_document_id,
    notes.notes as notes
from holiday_exception_entity as e
left join holiday_exception_jurisdiction_id as jurisdiction_id on jurisdiction_id.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_holiday_id as holiday_id on holiday_id.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_holiday_rule_id as holiday_rule_id on holiday_rule_id.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_holiday_date as holiday_date on holiday_date.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_action as action on action.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_name as name on name.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_replacement_holiday_key as replacement_holiday_key on replacement_holiday_key.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_expression_language as expression_language on expression_language.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_expression_text as expression_text on expression_text.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_valid_from_year as valid_from_year on valid_from_year.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_valid_to_year as valid_to_year on valid_to_year.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_priority as priority on priority.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_source_document_id as source_document_id on source_document_id.holiday_exception_id = e.holiday_exception_id
left join holiday_exception_notes as notes on notes.holiday_exception_id = e.holiday_exception_id
;
create trigger holiday_exception_insert
instead of insert on holiday_exception
begin
    insert into holiday_exception_entity(holiday_exception_id) values (new.holiday_exception_id);
    insert into holiday_exception_jurisdiction_id(holiday_exception_id, jurisdiction_id) values (coalesce(new.holiday_exception_id, last_insert_rowid()), new.jurisdiction_id);
    insert into holiday_exception_holiday_id(holiday_exception_id, holiday_id) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.holiday_id where new.holiday_id is not null;
    insert into holiday_exception_holiday_rule_id(holiday_exception_id, holiday_rule_id) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.holiday_rule_id where new.holiday_rule_id is not null;
    insert into holiday_exception_holiday_date(holiday_exception_id, holiday_date) values (coalesce(new.holiday_exception_id, last_insert_rowid()), new.holiday_date);
    insert into holiday_exception_action(holiday_exception_id, action) values (coalesce(new.holiday_exception_id, last_insert_rowid()), new.action);
    insert into holiday_exception_name(holiday_exception_id, name) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.name where new.name is not null;
    insert into holiday_exception_replacement_holiday_key(holiday_exception_id, replacement_holiday_key) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.replacement_holiday_key where new.replacement_holiday_key is not null;
    insert into holiday_exception_expression_language(holiday_exception_id, expression_language) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.expression_language where new.expression_language is not null;
    insert into holiday_exception_expression_text(holiday_exception_id, expression_text) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.expression_text where new.expression_text is not null;
    insert into holiday_exception_valid_from_year(holiday_exception_id, valid_from_year) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.valid_from_year where new.valid_from_year is not null;
    insert into holiday_exception_valid_to_year(holiday_exception_id, valid_to_year) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.valid_to_year where new.valid_to_year is not null;
    insert into holiday_exception_priority(holiday_exception_id, priority) values (coalesce(new.holiday_exception_id, last_insert_rowid()), coalesce(new.priority, 0));
    insert into holiday_exception_source_document_id(holiday_exception_id, source_document_id) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.source_document_id where new.source_document_id is not null;
    insert into holiday_exception_notes(holiday_exception_id, notes) select coalesce(new.holiday_exception_id, last_insert_rowid()), new.notes where new.notes is not null;
end;

create view holiday_rule_source as
select
    e.holiday_rule_source_id as holiday_rule_source_id,
    holiday_id.holiday_id as holiday_id,
    holiday_rule_id.holiday_rule_id as holiday_rule_id,
    holiday_observance_rule_id.holiday_observance_rule_id as holiday_observance_rule_id,
    holiday_exception_id.holiday_exception_id as holiday_exception_id,
    source_document_id.source_document_id as source_document_id,
    role.role as role,
    notes.notes as notes
from holiday_rule_source_entity as e
left join holiday_rule_source_holiday_id as holiday_id on holiday_id.holiday_rule_source_id = e.holiday_rule_source_id
left join holiday_rule_source_holiday_rule_id as holiday_rule_id on holiday_rule_id.holiday_rule_source_id = e.holiday_rule_source_id
left join holiday_rule_source_holiday_observance_rule_id as holiday_observance_rule_id on holiday_observance_rule_id.holiday_rule_source_id = e.holiday_rule_source_id
left join holiday_rule_source_holiday_exception_id as holiday_exception_id on holiday_exception_id.holiday_rule_source_id = e.holiday_rule_source_id
left join holiday_rule_source_source_document_id as source_document_id on source_document_id.holiday_rule_source_id = e.holiday_rule_source_id
left join holiday_rule_source_role as role on role.holiday_rule_source_id = e.holiday_rule_source_id
left join holiday_rule_source_notes as notes on notes.holiday_rule_source_id = e.holiday_rule_source_id
;
create trigger holiday_rule_source_insert
instead of insert on holiday_rule_source
begin
    insert into holiday_rule_source_entity(holiday_rule_source_id) values (new.holiday_rule_source_id);
    insert into holiday_rule_source_holiday_id(holiday_rule_source_id, holiday_id) select coalesce(new.holiday_rule_source_id, last_insert_rowid()), new.holiday_id where new.holiday_id is not null;
    insert into holiday_rule_source_holiday_rule_id(holiday_rule_source_id, holiday_rule_id) select coalesce(new.holiday_rule_source_id, last_insert_rowid()), new.holiday_rule_id where new.holiday_rule_id is not null;
    insert into holiday_rule_source_holiday_observance_rule_id(holiday_rule_source_id, holiday_observance_rule_id) select coalesce(new.holiday_rule_source_id, last_insert_rowid()), new.holiday_observance_rule_id where new.holiday_observance_rule_id is not null;
    insert into holiday_rule_source_holiday_exception_id(holiday_rule_source_id, holiday_exception_id) select coalesce(new.holiday_rule_source_id, last_insert_rowid()), new.holiday_exception_id where new.holiday_exception_id is not null;
    insert into holiday_rule_source_source_document_id(holiday_rule_source_id, source_document_id) values (coalesce(new.holiday_rule_source_id, last_insert_rowid()), new.source_document_id);
    insert into holiday_rule_source_role(holiday_rule_source_id, role) values (coalesce(new.holiday_rule_source_id, last_insert_rowid()), new.role);
    insert into holiday_rule_source_notes(holiday_rule_source_id, notes) select coalesce(new.holiday_rule_source_id, last_insert_rowid()), new.notes where new.notes is not null;
end;

create view holiday_instance as
select
    e.holiday_instance_id as holiday_instance_id,
    jurisdiction_id.jurisdiction_id as jurisdiction_id,
    holiday_id.holiday_id as holiday_id,
    holiday_date.holiday_date as holiday_date,
    holiday_name.holiday_name as holiday_name,
    holiday_class.holiday_class as holiday_class,
    language.language as language,
    source_document_id.source_document_id as source_document_id
from holiday_instance_entity as e
left join holiday_instance_jurisdiction_id as jurisdiction_id on jurisdiction_id.holiday_instance_id = e.holiday_instance_id
left join holiday_instance_holiday_id as holiday_id on holiday_id.holiday_instance_id = e.holiday_instance_id
left join holiday_instance_holiday_date as holiday_date on holiday_date.holiday_instance_id = e.holiday_instance_id
left join holiday_instance_holiday_name as holiday_name on holiday_name.holiday_instance_id = e.holiday_instance_id
left join holiday_instance_holiday_class as holiday_class on holiday_class.holiday_instance_id = e.holiday_instance_id
left join holiday_instance_language as language on language.holiday_instance_id = e.holiday_instance_id
left join holiday_instance_source_document_id as source_document_id on source_document_id.holiday_instance_id = e.holiday_instance_id
;
create trigger holiday_instance_insert
instead of insert on holiday_instance
begin
    insert into holiday_instance_entity(holiday_instance_id) values (new.holiday_instance_id);
    insert into holiday_instance_jurisdiction_id(holiday_instance_id, jurisdiction_id) values (coalesce(new.holiday_instance_id, last_insert_rowid()), new.jurisdiction_id);
    insert into holiday_instance_holiday_id(holiday_instance_id, holiday_id) select coalesce(new.holiday_instance_id, last_insert_rowid()), new.holiday_id where new.holiday_id is not null;
    insert into holiday_instance_holiday_date(holiday_instance_id, holiday_date) values (coalesce(new.holiday_instance_id, last_insert_rowid()), new.holiday_date);
    insert into holiday_instance_holiday_name(holiday_instance_id, holiday_name) values (coalesce(new.holiday_instance_id, last_insert_rowid()), new.holiday_name);
    insert into holiday_instance_holiday_class(holiday_instance_id, holiday_class) values (coalesce(new.holiday_instance_id, last_insert_rowid()), new.holiday_class);
    insert into holiday_instance_language(holiday_instance_id, language) select coalesce(new.holiday_instance_id, last_insert_rowid()), new.language where new.language is not null;
    insert into holiday_instance_source_document_id(holiday_instance_id, source_document_id) select coalesce(new.holiday_instance_id, last_insert_rowid()), new.source_document_id where new.source_document_id is not null;
end;

create index jurisdiction_parent_idx on jurisdiction_parent_jurisdiction_id(parent_jurisdiction_id);
create index jurisdiction_type_idx on jurisdiction_jurisdiction_type(jurisdiction_type);
create index jurisdiction_name_idx on jurisdiction_name(name);
create index holiday_definition_jurisdiction_idx on holiday_definition_jurisdiction_id(jurisdiction_id);
create index holiday_definition_key_idx on holiday_definition_holiday_key(holiday_key);
create index holiday_rule_holiday_idx on holiday_rule_holiday_id(holiday_id);
create index holiday_rule_priority_idx on holiday_rule_priority(priority);
create index holiday_observance_holiday_idx on holiday_observance_rule_holiday_id(holiday_id);
create index jurisdiction_weekend_rule_jurisdiction_idx on jurisdiction_weekend_rule_jurisdiction_id(jurisdiction_id);
create index holiday_exception_jurisdiction_idx on holiday_exception_jurisdiction_id(jurisdiction_id);
create index holiday_exception_date_idx on holiday_exception_holiday_date(holiday_date);
create index holiday_instance_jurisdiction_idx on holiday_instance_jurisdiction_id(jurisdiction_id);
create index holiday_instance_date_idx on holiday_instance_holiday_date(holiday_date);
create index holiday_instance_name_idx on holiday_instance_holiday_name(holiday_name);
create index timezone_era_timezone_idx on timezone_era_timezone_name(timezone_name);
create index timezone_era_sequence_idx on timezone_era_sequence_no(sequence_no);
create index timezone_transition_rule_name_idx on timezone_transition_rule_rule_name(rule_name);
create index source_document_jurisdiction_idx on source_document_jurisdiction_id(jurisdiction_id);


.read packaging/jurisdiction-db/mars_country_jurisdictions.sql
.read packaging/jurisdiction-db/mars_target_subdivisions.sql

insert into calendar_system(calendar_system_id, cldr_rscale, name, family, notes)
values
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

insert into jurisdiction(
    jurisdiction_id,
    parent_jurisdiction_id,
    jurisdiction_type,
    iso_country_code,
    iso_subdivision_code,
    cldr_region_code,
    name,
    notes
)
values
    ('XK', null, 'special', 'XK', null, 'XK', 'Kosovo',
     'Special-case jurisdiction used for imported holiday datasets that expose Kosovo under XK.'),
    ('GB-ENG', 'GB', 'subdivision', 'GB', 'GB-ENG', 'GB', 'England',
     'Subdivision rule set for English bank holidays.');

.read packaging/jurisdiction-db/mars_timezone_rules.sql
.read packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql
.read packaging/jurisdiction-db/mars_jurisdiction_towns.sql

insert into source_document(
    source_document_id,
    jurisdiction_id,
    source_type,
    citation,
    source_url,
    accessed_on,
    notes
)
values
    (1, null, 'standards',
     'ISO 3166 country and subdivision codes',
     'https://www.iso.org/iso-3166-country-codes.html',
     '2026-06-23',
     'Jurisdiction coding model.'),
    (2, null, 'standards',
     'tz database iso3166.tab country list',
     'https://data.iana.org/time-zones/tzdb-2025b/iso3166.tab',
     '2026-06-23',
     'Bulk country/territory jurisdiction seed used by this fixture.'),
    (3, null, 'standards',
     'RFC 5545 iCalendar recurrence rules',
     'https://www.rfc-editor.org/rfc/rfc5545',
     '2026-06-23',
     'Recurrence-rule representation baseline.'),
    (4, null, 'standards',
     'RFC 7529 non-Gregorian recurrence rules',
     'https://www.rfc-editor.org/rfc/rfc7529',
     '2026-06-23',
     'RSCALE support for non-Gregorian holiday recurrence.'),
    (5, null, 'standards',
     'Unicode LDML / CLDR locale and calendar identifiers',
     'https://www.unicode.org/reports/tr35/',
     '2026-06-23',
     'Calendar-system and subdivision identifier model.'),
    (6, null, 'other',
     'vacanza/holidays reference implementations',
     'https://github.com/vacanza/holidays',
     '2026-06-23',
     'Used as a practical cross-country seed reference for holiday rules and observance conventions.'),
    (7, 'GB-ENG', 'government_site',
     'GOV.UK bank holidays for England and Wales, Scotland and Northern Ireland',
     'https://www.gov.uk/bank-holidays',
     '2026-08-22',
     'Official present-day dates and one-off bank holidays; historical ranges are cross-checked against the recorded calendar implementations.'),
    (8, null, 'other',
     'workalendar reference implementations',
     'https://github.com/workalendar/workalendar',
     '2026-06-23',
     'Supplementary historical backfill source for jurisdictions where the primary import starts too late.'),
    (9, null, 'internal',
     'Automatically inferred first-class holiday rules from materialized history',
     null,
     '2026-06-23',
     'Generated country-level holiday_definition and holiday_rule rows inferred from historical materialized holidays.'),
    (10, 'ZA', 'statute',
     'Public Holidays Act, 1994 (Act No. 36 of 1994) of South Africa',
     'https://www.gov.za/documents/public-holidays-act',
     '2026-08-22',
     'Primary legal basis for annual South African public holidays and Sunday-to-Monday observance.'),
    (11, 'ZA', 'government_site',
     'South African Government public holidays guidance',
     'https://www.gov.za/about-sa/public-holidays',
     '2026-08-22',
     'Used to align holiday naming and present-day public guidance for South Africa.'),
    (12, 'DK', 'statute',
     'Danish Act No. 214 of 6 March 2023 on the consequences of abolishing Great Prayer Day as a public holiday',
     'https://www.retsinformation.dk/eli/lta/2023/214',
     '2026-08-22',
     'Official basis for Great Prayer Day ending after 2023; the remaining Danish rules are cross-checked against source documents 6 and 8.'),
    (13, 'NL', 'government_site',
     'Government of the Netherlands: official public holidays in the Netherlands',
     'https://www.government.nl/faq/work/public-holidays-in-the-netherlands',
     '2026-08-22',
     'Official present-day list used to hand-model King''s Day, Liberation Day, Easter, Pentecost and the two Christmas days.'),
    (14, null, 'standards',
     'Unicode Technical Standard #35, LDML Part 4: territory-based week and weekend data',
     'https://www.unicode.org/reports/tr35/tr35-dates.html#Week_Data',
     '2026-08-22',
     'Baseline for territory weekend conventions; historical reforms and exceptional masks are cross-checked against source documents 6 and 8.'),
    (15, 'ZA', 'government_site',
     'South African Department of Sport, Arts and Culture, Evaluation Study of South Africa''s National Days, June 2024, section 3.2',
     'https://www.dsac.gov.za/sites/default/files/2026-03/FINAL%20National%20Days%20Evaluation_FINAL_REPORT_JUNE%202024_0.pdf',
     '2026-08-22',
     'Government legislative timeline used for the 1910, 1952, 1961 and post-1994 public-holiday regimes.'),
    (16, 'IE', 'statute',
     'Holidays (Employees) Act, 1961, section 8, and Holidays (Employees) Act, 1973, Schedule',
     'https://www.irishstatutebook.ie/eli/1961/act/33/section/8/enacted/en/html',
     '2026-08-22',
     'Official basis for Whit Monday and its replacement by the first Monday in June; the 1973 Schedule is at https://www.irishstatutebook.ie/eli/1973/act/25/schedule/enacted/en/html.'),
    (17, 'NL', 'government_site',
     'Royal House of the Netherlands: History of King''s Day',
     'https://www.royal-house.nl/topics/monarchy/king%E2%80%99s-day/history-of-king%E2%80%99s-day',
     '2026-08-22',
     'Official history of the 31 August and 30 April royal holidays; Sunday replacement is also recorded at https://www.koninklijkhuis.nl/vraag-en-antwoord/waar-vind-ik-antwoorden-over-de-troonswisseling-in-2013.');

insert into jurisdiction_weekend_rule(
    jurisdiction_weekend_rule_id,
    jurisdiction_id,
    weekend_mask,
    valid_from_year,
    valid_to_year,
    source_document_id,
    notes
)
values
    (1, 'GB', '6,7', null, null, 14, 'United Kingdom Saturday-Sunday weekend baseline.'),
    (2, 'GB-ENG', '6,7', null, null, 14, 'England Saturday-Sunday weekend baseline.'),
    (3, 'GB-NIR', '6,7', null, null, 14, 'Northern Ireland Saturday-Sunday weekend baseline.'),
    (4, 'GB-SCT', '6,7', null, null, 14, 'Scotland Saturday-Sunday weekend baseline.'),
    (5, 'GB-WLS', '6,7', null, null, 14, 'Wales Saturday-Sunday weekend baseline.'),
    (6, 'AU', '6,7', null, null, 14, 'Australia Saturday-Sunday weekend baseline.'),
    (7, 'NZ', '6,7', null, null, 14, 'New Zealand Saturday-Sunday weekend baseline.'),
    (8, 'IE', '6,7', null, null, 14, 'Ireland Saturday-Sunday weekend baseline.'),
    (9, 'FR', '6,7', null, null, 14, 'France Saturday-Sunday weekend baseline.'),
    (10, 'DE', '6,7', null, null, 14, 'Germany Saturday-Sunday weekend baseline.'),
    (11, 'ZA', '6,7', null, null, 14, 'South Africa Saturday-Sunday weekend baseline.'),
    (12, 'DK', '6,7', null, null, 14, 'Denmark Saturday-Sunday weekend baseline.'),
    (13, 'NL', '6,7', null, null, 14, 'Netherlands Saturday-Sunday weekend baseline.'),
    (14, 'CA', '6,7', null, null, 14, 'Canada Saturday-Sunday weekend baseline.'),
    (15, 'US', '6,7', null, null, 14, 'United States Saturday-Sunday weekend baseline.'),
    (16, 'PT', '6,7', null, null, 14, 'Portugal Saturday-Sunday weekend baseline.'),
    (17, 'IT', '6,7', null, null, 14, 'Italy Saturday-Sunday weekend baseline.'),
    (18, 'GR', '6,7', null, null, 14, 'Greece Saturday-Sunday weekend baseline.'),
    (19, 'SA', '4,5', null, 2012, 14, 'Saudi Arabia Thursday-Friday weekend before the 2013 reform.'),
    (20, 'SA', '5,6', 2013, null, 14, 'Saudi Arabia Friday-Saturday weekend since 29 June 2013.'),
    (21, 'AE', '5,6', null, 2021, 14, 'United Arab Emirates Friday-Saturday weekend before the 2022 reform.'),
    (22, 'AE', '6,7', 2022, null, 14, 'United Arab Emirates Saturday-Sunday weekend from January 2022.'),
    (23, 'OM', '5,6', null, null, 14, 'Oman Friday-Saturday weekend baseline.'),
    (24, 'BH', '5,6', null, null, 14, 'Bahrain Friday-Saturday weekend baseline.'),
    (25, 'KW', '5,6', null, null, 14, 'Kuwait Friday-Saturday weekend baseline.'),
    (26, 'QA', '5,6', null, null, 14, 'Qatar Friday-Saturday weekend baseline.'),
    (27, 'JO', '5,6', null, null, 14, 'Jordan Friday-Saturday weekend baseline.'),
    (28, 'EG', '5,6', null, null, 14, 'Egypt Friday-Saturday weekend baseline.'),
    (29, 'BN', '5,7', null, null, 14, 'Brunei non-contiguous Friday-Sunday weekend baseline.'),
    (30, 'IR', '5', null, null, 14, 'Iran weekly rest-day baseline with Friday as the ordinary weekend holiday.');

insert into holiday_definition(
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
values
    (1, 'GB-ENG', 'new_years_day', 'New Years Day', 'bank', 'full_day', 'gregory', null, null,
     'Observed on the next weekday if 1 January falls on a weekend.'),
    (2, 'GB-ENG', 'good_friday', 'Good Friday', 'bank', 'full_day', 'gregory', null, null,
     'Two days before Gregorian Easter Sunday.'),
    (3, 'GB-ENG', 'easter_monday', 'Easter Monday', 'bank', 'full_day', 'gregory', null, null,
     'One day after Gregorian Easter Sunday.'),
    (4, 'GB-ENG', 'may_day_bank_holiday', 'May Day Bank Holiday', 'bank', 'full_day', 'gregory', null, null,
     'First Monday in May, except where a one-off exception replaces it.'),
    (5, 'GB-ENG', 'spring_bank_holiday', 'Spring Bank Holiday', 'bank', 'full_day', 'gregory', null, null,
     'Last Monday in May, except where a one-off exception replaces it.'),
    (6, 'GB-ENG', 'platinum_jubilee_bank_holiday', 'Platinum Jubilee Bank Holiday', 'special', 'full_day', 'gregory', 2022, 2022,
     'Additional one-off bank holiday.'),
    (7, 'GB-ENG', 'state_funeral_qe2', 'State Funeral of Queen Elizabeth II', 'special', 'full_day', 'gregory', 2022, 2022,
     'Additional one-off bank holiday.'),
    (8, 'GB-ENG', 'coronation_king_charles_iii', 'Coronation of King Charles III', 'special', 'full_day', 'gregory', 2023, 2023,
     'Additional one-off bank holiday.'),
    (9, 'GB-ENG', 'august_bank_holiday', 'August Bank Holiday', 'bank', 'full_day', 'gregory', null, null,
     'Last Monday in August.'),
    (10, 'GB-ENG', 'christmas_day', 'Christmas Day', 'bank', 'full_day', 'gregory', null, null,
     'Christmas Day with paired substitute handling.'),
    (11, 'GB-ENG', 'boxing_day', 'Boxing Day', 'bank', 'full_day', 'gregory', null, null,
     'Boxing Day with paired substitute handling.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (1, 'en-GB', 'New Years Day', 'Y'),
    (2, 'en-GB', 'Good Friday', 'Y'),
    (3, 'en-GB', 'Easter Monday', 'Y'),
    (4, 'en-GB', 'May Day Bank Holiday', 'Y'),
    (5, 'en-GB', 'Spring Bank Holiday', 'Y'),
    (6, 'en-GB', 'Platinum Jubilee Bank Holiday', 'Y'),
    (7, 'en-GB', 'State Funeral of Queen Elizabeth II', 'Y'),
    (8, 'en-GB', 'Coronation of King Charles III', 'Y'),
    (9, 'en-GB', 'August Bank Holiday', 'Y'),
    (10, 'en-GB', 'Christmas Day', 'Y'),
    (11, 'en-GB', 'Boxing Day', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    holiday_date,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (1, 1, 1, 'fixed_date', 1, 1, null, null, null, null, null, null, 10, null),
    (2, 2, 1, 'easter_offset', null, null, null, null, -2, null, null, null, 20, null),
    (3, 3, 1, 'easter_offset', null, null, null, null, 1, null, null, null, 30, null),
    (4, 4, 1, 'nth_weekday', 5, null, 1, 1, null, null, null, null, 40, 'First Monday in May.'),
    (5, 5, 1, 'last_weekday', 5, null, 1, -1, null, null, null, null, 50, 'Last Monday in May.'),
    (6, 6, 1, 'one_off', null, null, null, null, null, '2022-06-03', 2022, 2022, 60, 'Additional bank holiday for the Platinum Jubilee.'),
    (7, 7, 1, 'one_off', null, null, null, null, null, '2022-09-19', 2022, 2022, 70, 'Additional bank holiday for the State Funeral of Queen Elizabeth II.'),
    (8, 8, 1, 'one_off', null, null, null, null, null, '2023-05-08', 2023, 2023, 80, 'Additional bank holiday for the Coronation of King Charles III.'),
    (9, 9, 1, 'last_weekday', 8, null, 1, -1, null, null, null, null, 90, 'Last Monday in August.'),
    (10, 10, 1, 'fixed_date', 12, 25, null, null, null, null, null, null, 100, null),
    (11, 11, 1, 'fixed_date', 12, 26, null, null, null, null, null, null, 110, null),
    (12, 4, 2, 'one_off', null, null, null, null, null, '2020-05-08', 2020, 2020, 5, 'VE Day exception date represented as a high-priority override candidate.'),
    (13, 5, 2, 'one_off', null, null, null, null, null, '2022-06-02', 2022, 2022, 5, 'Moved Spring Bank Holiday date for 2022.');

insert into holiday_observance_rule(
    holiday_observance_rule_id,
    holiday_id,
    holiday_rule_id,
    observed_rule_kind,
    observed_name,
    suppress_original,
    priority,
    notes
) values
    (1, 1, 1, 'next_weekday', 'Bank Holiday in Lieu of New Years Day', 'Y', 10, 'If 1 January falls on the ordinary weekend for the jurisdiction, observe on the following weekday.'),
    (2, 10, 10, 'christmas_pair', 'Bank Holiday in Lieu of Christmas Day', 'Y', 20, 'Christmas and Boxing Day substitutions interact and must be resolved together.'),
    (3, 11, 11, 'christmas_pair', 'Bank Holiday in Lieu of Boxing Day', 'Y', 30, 'Christmas and Boxing Day substitutions interact and must be resolved together.');

insert into holiday_exception(
    holiday_exception_id,
    jurisdiction_id,
    holiday_id,
    holiday_rule_id,
    holiday_date,
    action,
    name,
    replacement_holiday_key,
    valid_from_year,
    valid_to_year,
    priority,
    source_document_id,
    notes
) values
    (1, 'GB-ENG', 4, 4, '2020-05-08', 'replace', '75th anniversary of Victory in Europe (VE Day)', 'may_day_bank_holiday', 2020, 2020, 0, 7, 'May Day moved from Monday 2020-05-04 to Friday 2020-05-08.'),
    (2, 'GB-ENG', 5, 5, '2022-06-02', 'replace', 'Spring Bank Holiday', 'spring_bank_holiday', 2022, 2022, 0, 7, 'Spring Bank Holiday moved from Monday 2022-05-30 to Thursday 2022-06-02.'),
    (3, 'GB-ENG', 6, 6, '2022-06-03', 'add', 'Platinum Jubilee Bank Holiday', null, 2022, 2022, 0, 7, 'Additional bank holiday for the Platinum Jubilee.'),
    (4, 'GB-ENG', 7, 7, '2022-09-19', 'add', 'State Funeral of Queen Elizabeth II', null, 2022, 2022, 0, 7, 'Additional bank holiday for the State Funeral of Queen Elizabeth II.'),
    (5, 'GB-ENG', 8, 8, '2023-05-08', 'add', 'Coronation of King Charles III', null, 2023, 2023, 0, 7, 'Additional bank holiday for the Coronation of King Charles III.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    holiday_observance_rule_id,
    holiday_exception_id,
    source_document_id,
    role
) values
    (1, 1, 1, null, 7, 'definition'),
    (2, 2, null, null, 7, 'definition'),
    (3, 3, null, null, 7, 'definition'),
    (4, 4, null, 1, 7, 'exception'),
    (5, 5, null, 2, 7, 'exception'),
    (6, 6, null, 3, 7, 'exception'),
    (7, 7, null, 4, 7, 'exception'),
    (8, 8, null, 5, 7, 'exception'),
    (9, 9, null, null, 7, 'definition'),
    (10, 10, 2, null, 7, 'observance'),
    (11, 11, 3, null, 7, 'observance');

-- Australia national/public holiday seed rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    notes
) values
    (100, 'AU', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', null, 'Country-level seed; subdivision-specific holidays remain separate.'),
    (101, 'AU', 'australia_day', 'Australia Day', 'public', 'full_day', 'gregory', 1935, null),
    (102, 'AU', 'good_friday', 'Good Friday', 'public', 'full_day', 'gregory', null, null),
    (103, 'AU', 'easter_monday', 'Easter Monday', 'public', 'full_day', 'gregory', null, null),
    (104, 'AU', 'anzac_day', 'Anzac Day', 'public', 'full_day', 'gregory', null, null),
    (105, 'AU', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', null, 'Christmas Day with substitute handling.'),
    (106, 'AU', 'boxing_day', 'Boxing Day', 'public', 'full_day', 'gregory', null, 'Boxing Day with substitute handling.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (100, 'en-AU', 'New Year''s Day', 'Y'),
    (101, 'en-AU', 'Australia Day', 'Y'),
    (102, 'en-AU', 'Good Friday', 'Y'),
    (103, 'en-AU', 'Easter Monday', 'Y'),
    (104, 'en-AU', 'Anzac Day', 'Y'),
    (105, 'en-AU', 'Christmas Day', 'Y'),
    (106, 'en-AU', 'Boxing Day', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    offset_days,
    priority
) values
    (100, 100, 1, 'fixed_date', 1, 1, null, 100),
    (101, 101, 1, 'fixed_date', 1, 26, null, 100),
    (102, 102, 1, 'easter_offset', null, null, -2, 100),
    (103, 103, 1, 'easter_offset', null, null, 1, 100),
    (104, 104, 1, 'fixed_date', 4, 25, null, 100),
    (105, 105, 1, 'fixed_date', 12, 25, null, 100),
    (106, 106, 1, 'fixed_date', 12, 26, null, 100);

insert into holiday_observance_rule(
    holiday_observance_rule_id,
    holiday_id,
    holiday_rule_id,
    observed_rule_kind,
    observed_name,
    suppress_original,
    priority,
    notes
) values
    (100, 100, 100, 'next_weekday', 'New Year''s Day (observed)', 'Y', 100, 'Australian observed-holiday convention for New Year''s Day falling on the ordinary weekend.'),
    (101, 101, 101, 'next_weekday', 'Australia Day (observed)', 'Y', 100, 'Australian observed-holiday convention for Australia Day falling on the ordinary weekend.'),
    (102, 104, 104, 'next_weekday', 'Anzac Day (observed)', 'Y', 100, 'Seed simplification for national-level observed Anzac Day handling tied to the ordinary weekend.'),
    (103, 105, 105, 'christmas_pair', 'Christmas Day (observed)', 'Y', 100, 'Paired substitute handling for Christmas/Boxing Day.'),
    (104, 106, 106, 'christmas_pair', 'Boxing Day (observed)', 'Y', 100, 'Paired substitute handling for Christmas/Boxing Day.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    holiday_observance_rule_id,
    source_document_id,
    role
) values
    (100, 100, 100, 6, 'observance'),
    (101, 101, 101, 6, 'observance'),
    (102, 102, null, 6, 'definition'),
    (103, 103, null, 6, 'definition'),
    (104, 104, 102, 6, 'observance'),
    (105, 105, 103, 6, 'observance'),
    (106, 106, 104, 6, 'observance');

-- New Zealand national/public holiday seed rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    notes
) values
    (120, 'NZ', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', null, 'Observed using New Zealand Mondayisation conventions.'),
    (121, 'NZ', 'day_after_new_years_day', 'Day after New Year''s Day', 'public', 'full_day', 'gregory', null, 'Observed using New Zealand Mondayisation conventions.'),
    (122, 'NZ', 'waitangi_day', 'Waitangi Day', 'public', 'full_day', 'gregory', 1974, 'Observed since 2014 when falling on a weekend.'),
    (123, 'NZ', 'anzac_day', 'Anzac Day', 'public', 'full_day', 'gregory', 1921, 'Observed since 2014 when falling on a weekend.'),
    (124, 'NZ', 'good_friday', 'Good Friday', 'public', 'full_day', 'gregory', null, null),
    (125, 'NZ', 'easter_monday', 'Easter Monday', 'public', 'full_day', 'gregory', null, null),
    (126, 'NZ', 'kings_birthday', 'King''s Birthday', 'public', 'full_day', 'gregory', 1902, 'Name varies historically with the sovereign.'),
    (127, 'NZ', 'labour_day', 'Labour Day', 'public', 'full_day', 'gregory', 1899, 'Fourth Monday in October in the modern system.'),
    (128, 'NZ', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', null, 'Christmas Day with substitute handling.'),
    (129, 'NZ', 'boxing_day', 'Boxing Day', 'public', 'full_day', 'gregory', null, 'Boxing Day with substitute handling.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (120, 'en-NZ', 'New Year''s Day', 'Y'),
    (121, 'en-NZ', 'Day after New Year''s Day', 'Y'),
    (122, 'en-NZ', 'Waitangi Day', 'Y'),
    (123, 'en-NZ', 'Anzac Day', 'Y'),
    (124, 'en-NZ', 'Good Friday', 'Y'),
    (125, 'en-NZ', 'Easter Monday', 'Y'),
    (126, 'en-NZ', 'King''s Birthday', 'Y'),
    (127, 'en-NZ', 'Labour Day', 'Y'),
    (128, 'en-NZ', 'Christmas Day', 'Y'),
    (129, 'en-NZ', 'Boxing Day', 'Y');

insert into holiday_rule(
    holiday_rule_id,
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
values
    (120, 120, 1, 'fixed_date', 1, 1, null, null, null, null, 100,
     null),
    (121, 121, 1, 'fixed_date', 1, 2, null, null, null, null, 100,
     null),
    (122, 122, 1, 'fixed_date', 2, 6, null, null, null, 1974, 100,
     null),
    (123, 123, 1, 'fixed_date', 4, 25, null, null, null, 1921, 100,
     null),
    (124, 124, 1, 'easter_offset', null, null, null, null, -2, null, 100,
     null),
    (125, 125, 1, 'easter_offset', null, null, null, null, 1, null, 100,
     null),
    (126, 126, 1, 'nth_weekday', 6, null, 1, 1, null, 1902, 100,
     'First Monday in June.'),
    (127, 127, 1, 'nth_weekday', 10, null, 1, 4, null, 1910, 100,
     'Fourth Monday in October in the modern system.'),
    (128, 128, 1, 'fixed_date', 12, 25, null, null, null, null, 100,
     null),
    (129, 129, 1, 'fixed_date', 12, 26, null, null, null, null, 100,
     null);

insert into holiday_observance_rule(
    holiday_observance_rule_id,
    holiday_id,
    holiday_rule_id,
    observed_rule_kind,
    observed_name,
    suppress_original,
    valid_from_year,
    priority,
    notes
) values
    (120, 120, 120, 'next_weekday', 'New Year''s Day (observed)', 'Y', null, 100, 'If 1 January falls on the ordinary weekend, observe on the next weekday.'),
    (121, 121, 121, 'next_non_holiday', 'Day after New Year''s Day (observed)', 'Y', null, 100, 'Move to the next non-holiday weekday after an ordinary-weekend clash to avoid overlap with New Year''s Day.'),
    (122, 122, 122, 'next_monday', 'Waitangi Day (observed)', 'Y', 2014, 100, 'Mondayised from 2014.'),
    (123, 123, 123, 'next_monday', 'Anzac Day (observed)', 'Y', 2014, 100, 'Mondayised from 2014.'),
    (124, 128, 128, 'christmas_pair', 'Christmas Day (observed)', 'Y', null, 100, 'Paired substitute handling for Christmas/Boxing Day.'),
    (125, 129, 129, 'christmas_pair', 'Boxing Day (observed)', 'Y', null, 100, 'Paired substitute handling for Christmas/Boxing Day.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    holiday_observance_rule_id,
    source_document_id,
    role
) values
    (120, 120, 120, 6, 'observance'),
    (121, 121, 121, 6, 'observance'),
    (122, 122, 122, 6, 'observance'),
    (123, 123, 123, 6, 'observance'),
    (124, 124, null, 6, 'definition'),
    (125, 125, null, 6, 'definition'),
    (126, 126, null, 6, 'definition'),
    (127, 127, null, 6, 'definition'),
    (128, 128, 124, 6, 'observance'),
    (129, 129, 125, 6, 'observance');

-- Ireland national/public holiday seed rules.
insert into holiday_definition(
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
values
    (140, 'IE', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', 1974, null,
     null),
    (141, 'IE', 'saint_brigids_day', 'Saint Brigid''s Day', 'public', 'full_day', 'gregory', 2023, null,
     '1 February when Friday, otherwise first Monday from 1 February.'),
    (142, 'IE', 'saint_patricks_day', 'Saint Patrick''s Day', 'public', 'full_day', 'gregory', 1903, null,
     null),
    (143, 'IE', 'easter_monday', 'Easter Monday', 'public', 'full_day', 'gregory', 1926, null,
     null),
    (144, 'IE', 'may_day', 'May Day', 'public', 'full_day', 'gregory', 1994, null,
     'Usually first Monday in May.'),
    (145, 'IE', 'june_bank_holiday', 'June Bank Holiday', 'public', 'full_day', 'gregory', 1973, null,
     'First Monday in June.'),
    (146, 'IE', 'august_bank_holiday', 'August Bank Holiday', 'public', 'full_day', 'gregory', 1926, null,
     'First Monday in August.'),
    (147, 'IE', 'october_bank_holiday', 'October Bank Holiday', 'public', 'full_day', 'gregory', 1977, null,
     'Last Monday in October.'),
    (148, 'IE', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', 1926, null,
     null),
    (149, 'IE', 'saint_stephens_day', 'Saint Stephen''s Day', 'public', 'full_day', 'gregory', 1926, null,
     null),
    (150, 'IE', 'whit_monday', 'Whit Monday', 'public', 'full_day', 'gregory', 1926, 1972,
     'Historic holiday observed on the Monday after Pentecost before the move to the June Holiday in 1973.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (140, 'en-IE', 'New Year''s Day', 'Y'),
    (141, 'en-IE', 'Saint Brigid''s Day', 'Y'),
    (142, 'en-IE', 'Saint Patrick''s Day', 'Y'),
    (143, 'en-IE', 'Easter Monday', 'Y'),
    (144, 'en-IE', 'May Day', 'Y'),
    (145, 'en-IE', 'June Bank Holiday', 'Y'),
    (146, 'en-IE', 'August Bank Holiday', 'Y'),
    (147, 'en-IE', 'October Bank Holiday', 'Y'),
    (148, 'en-IE', 'Christmas Day', 'Y'),
    (149, 'en-IE', 'Saint Stephen''s Day', 'Y'),
    (150, 'en-IE', 'Whit Monday', 'Y');

insert into holiday_rule(
    holiday_rule_id,
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
values
    (140, 140, 1, 'fixed_date', 1, 1, null, null, null, 1974, null, 100,
     null),
    (141, 141, 1, 'weekday_after_date', 2, 1, 1, null, null, 2023, null, 100,
     'First Monday from 1 February.'),
    (142, 142, 1, 'fixed_date', 3, 17, null, null, null, 1903, null, 100,
     null),
    (143, 143, 1, 'easter_offset', null, null, null, null, 1, 1926, null, 100,
     null),
    (144, 144, 1, 'nth_weekday', 5, null, 1, 1, null, 1994, null, 100,
     'First Monday in May.'),
    (145, 145, 1, 'nth_weekday', 6, null, 1, 1, null, 1973, null, 100,
     'First Monday in June.'),
    (146, 146, 1, 'nth_weekday', 8, null, 1, 1, null, 1926, null, 100,
     'First Monday in August.'),
    (147, 147, 1, 'last_weekday', 10, null, 1, -1, null, 1977, null, 100,
     'Last Monday in October.'),
    (148, 148, 1, 'fixed_date', 12, 25, null, null, null, 1926, null, 100,
     null),
    (149, 149, 1, 'fixed_date', 12, 26, null, null, null, 1926, null, 100,
     null),
    (150, 150, 1, 'easter_offset', null, null, null, null, 50, 1926, 1972, 100,
     'Whit Monday before replacement by the June Holiday.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    source_document_id,
    role
) values
    (140, 140, 6, 'definition'),
    (141, 141, 6, 'definition'),
    (142, 142, 6, 'definition'),
    (143, 143, 6, 'definition'),
    (144, 144, 6, 'definition'),
    (145, 145, 6, 'definition'),
    (146, 146, 6, 'definition'),
    (147, 147, 6, 'definition'),
    (148, 148, 6, 'definition'),
    (149, 149, 6, 'definition'),
    (150, 150, 16, 'definition');

-- France national/public holiday seed rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    notes
) values
    (160, 'FR', 'new_years_day', 'Jour de l''an', 'public', 'full_day', 'gregory', 1811, null),
    (161, 'FR', 'easter_monday', 'Lundi de Pâques', 'public', 'full_day', 'gregory', 1886, null),
    (162, 'FR', 'labour_day', 'Fête du Travail', 'public', 'full_day', 'gregory', 1948, null),
    (163, 'FR', 'victory_day', 'Fête de la Victoire', 'public', 'full_day', 'gregory', 1982, '8 May victory commemoration.'),
    (164, 'FR', 'ascension_day', 'Ascension', 'public', 'full_day', 'gregory', null, null),
    (165, 'FR', 'whit_monday', 'Lundi de Pentecôte', 'public', 'full_day', 'gregory', 1886, 'Removed during 2005-2007 and restored from 2008.'),
    (166, 'FR', 'national_day', 'Fête nationale', 'public', 'full_day', 'gregory', 1880, 'Bastille Day.'),
    (167, 'FR', 'assumption_day', 'Assomption', 'public', 'full_day', 'gregory', null, null),
    (168, 'FR', 'all_saints_day', 'Toussaint', 'public', 'full_day', 'gregory', null, null),
    (169, 'FR', 'armistice_day', 'Armistice', 'public', 'full_day', 'gregory', 1922, '11 November armistice commemoration.'),
    (170, 'FR', 'christmas_day', 'Noël', 'public', 'full_day', 'gregory', null, null);

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (160, 'fr-FR', 'Jour de l''an', 'Y'),
    (161, 'fr-FR', 'Lundi de Pâques', 'Y'),
    (162, 'fr-FR', 'Fête du Travail', 'Y'),
    (163, 'fr-FR', 'Fête de la Victoire', 'Y'),
    (164, 'fr-FR', 'Ascension', 'Y'),
    (165, 'fr-FR', 'Lundi de Pentecôte', 'Y'),
    (166, 'fr-FR', 'Fête nationale', 'Y'),
    (167, 'fr-FR', 'Assomption', 'Y'),
    (168, 'fr-FR', 'Toussaint', 'Y'),
    (169, 'fr-FR', 'Armistice', 'Y'),
    (170, 'fr-FR', 'Noël', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    offset_days,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (160, 160, 1, 'fixed_date', 1, 1, null, 1811, null, 100, null),
    (161, 161, 1, 'easter_offset', null, null, 1, 1886, null, 100, null),
    (162, 162, 1, 'fixed_date', 5, 1, null, 1948, null, 100, null),
    (163, 163, 1, 'fixed_date', 5, 8, null, 1982, null, 100, null),
    (164, 164, 1, 'easter_offset', null, null, 39, null, null, 100, 'Ascension Thursday.'),
    (165, 165, 1, 'easter_offset', null, null, 50, 1886, 2004, 100, 'Whit Monday before temporary removal.'),
    (166, 165, 2, 'easter_offset', null, null, 50, 2008, null, 100, 'Whit Monday after restoration.'),
    (167, 166, 1, 'fixed_date', 7, 14, null, 1880, null, 100, null),
    (168, 167, 1, 'fixed_date', 8, 15, null, null, null, 100, null),
    (169, 168, 1, 'fixed_date', 11, 1, null, null, null, 100, null),
    (170, 169, 1, 'fixed_date', 11, 11, null, 1922, null, 100, null),
    (171, 170, 1, 'fixed_date', 12, 25, null, null, null, 100, null);

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    source_document_id,
    role
) values
    (160, 160, 6, 'definition'),
    (161, 161, 6, 'definition'),
    (162, 162, 6, 'definition'),
    (163, 163, 6, 'definition'),
    (164, 164, 6, 'definition'),
    (165, 165, 6, 'definition'),
    (165, 166, 6, 'definition'),
    (166, 167, 6, 'definition'),
    (167, 168, 6, 'definition'),
    (168, 169, 6, 'definition'),
    (169, 170, 6, 'definition'),
    (170, 171, 6, 'definition');

-- Germany national/public holiday seed rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year
) values
    (180, 'DE', 'new_years_day', 'Neujahr', 'public', 'full_day', 'gregory', 1991),
    (181, 'DE', 'good_friday', 'Karfreitag', 'public', 'full_day', 'gregory', 1991),
    (182, 'DE', 'easter_monday', 'Ostermontag', 'public', 'full_day', 'gregory', 1991),
    (183, 'DE', 'labour_day', 'Erster Mai', 'public', 'full_day', 'gregory', 1991),
    (184, 'DE', 'ascension_day', 'Christi Himmelfahrt', 'public', 'full_day', 'gregory', 1991),
    (185, 'DE', 'whit_monday', 'Pfingstmontag', 'public', 'full_day', 'gregory', 1991),
    (186, 'DE', 'german_unity_day', 'Tag der Deutschen Einheit', 'public', 'full_day', 'gregory', 1991),
    (187, 'DE', 'christmas_day', 'Erster Weihnachtstag', 'public', 'full_day', 'gregory', 1991),
    (188, 'DE', 'second_day_of_christmas', 'Zweiter Weihnachtstag', 'public', 'full_day', 'gregory', 1991);

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (180, 'de-DE', 'Neujahr', 'Y'),
    (181, 'de-DE', 'Karfreitag', 'Y'),
    (182, 'de-DE', 'Ostermontag', 'Y'),
    (183, 'de-DE', 'Erster Mai', 'Y'),
    (184, 'de-DE', 'Christi Himmelfahrt', 'Y'),
    (185, 'de-DE', 'Pfingstmontag', 'Y'),
    (186, 'de-DE', 'Tag der Deutschen Einheit', 'Y'),
    (187, 'de-DE', 'Erster Weihnachtstag', 'Y'),
    (188, 'de-DE', 'Zweiter Weihnachtstag', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    offset_days,
    valid_from_year,
    priority,
    notes
) values
    (180, 180, 1, 'fixed_date', 1, 1, null, 1991, 100, null),
    (181, 181, 1, 'easter_offset', null, null, -2, 1991, 100, null),
    (182, 182, 1, 'easter_offset', null, null, 1, 1991, 100, null),
    (183, 183, 1, 'fixed_date', 5, 1, null, 1991, 100, null),
    (184, 184, 1, 'easter_offset', null, null, 39, 1991, 100, 'Ascension Thursday.'),
    (185, 185, 1, 'easter_offset', null, null, 50, 1991, 100, 'Whit Monday.'),
    (186, 186, 1, 'fixed_date', 10, 3, null, 1991, 100, null),
    (187, 187, 1, 'fixed_date', 12, 25, null, 1991, 100, null),
    (188, 188, 1, 'fixed_date', 12, 26, null, 1991, 100, null);

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    source_document_id,
    role
) values
    (180, 180, 6, 'definition'),
    (181, 181, 6, 'definition'),
    (182, 182, 6, 'definition'),
    (183, 183, 6, 'definition'),
    (184, 184, 6, 'definition'),
    (185, 185, 6, 'definition'),
    (186, 186, 6, 'definition'),
    (187, 187, 6, 'definition'),
    (188, 188, 6, 'definition');

-- South Africa national/public holiday seed rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    notes
) values
    (190, 'ZA', 'new_years_day', 'New Year''s Day', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (191, 'ZA', 'human_rights_day', 'Human Rights Day', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (192, 'ZA', 'good_friday', 'Good Friday', 'public', 'full_day', 'gregory', 1910, 'Two days before Gregorian Easter Sunday.'),
    (193, 'ZA', 'family_day', 'Family Day', 'public', 'full_day', 'gregory', 1995, 'Monday after Gregorian Easter Sunday.'),
    (194, 'ZA', 'freedom_day', 'Freedom Day', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (195, 'ZA', 'workers_day', 'Workers'' Day', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (196, 'ZA', 'youth_day', 'Youth Day', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (197, 'ZA', 'national_womens_day', 'National Women''s Day', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (198, 'ZA', 'heritage_day', 'Heritage Day', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (199, 'ZA', 'day_of_reconciliation', 'Day of Reconciliation', 'public', 'full_day', 'gregory', 1995, 'Observed on Monday only when the holiday falls on a Sunday.'),
    (200, 'ZA', 'christmas_day', 'Christmas Day', 'public', 'full_day', 'gregory', 1910, 'Observed on Monday only when the holiday falls on a Sunday; special extra holidays may still be declared separately.'),
    (201, 'ZA', 'day_of_goodwill', 'Day of Goodwill', 'public', 'full_day', 'gregory', 1980, 'Observed on Monday only when the holiday falls on a Sunday; special extra holidays may still be declared separately.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (190, 'en-ZA', 'New Year''s Day', 'Y'),
    (191, 'en-ZA', 'Human Rights Day', 'Y'),
    (192, 'en-ZA', 'Good Friday', 'Y'),
    (193, 'en-ZA', 'Family Day', 'Y'),
    (194, 'en-ZA', 'Freedom Day', 'Y'),
    (195, 'en-ZA', 'Workers'' Day', 'Y'),
    (196, 'en-ZA', 'Youth Day', 'Y'),
    (197, 'en-ZA', 'National Women''s Day', 'Y'),
    (198, 'en-ZA', 'Heritage Day', 'Y'),
    (199, 'en-ZA', 'Day of Reconciliation', 'Y'),
    (200, 'en-ZA', 'Christmas Day', 'Y'),
    (201, 'en-ZA', 'Day of Goodwill', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    offset_days,
    valid_from_year,
    priority
) values
    (190, 190, 1, 'fixed_date', 1, 1, null, 1995, 100),
    (191, 191, 1, 'fixed_date', 3, 21, null, 1995, 100),
    (192, 192, 1, 'easter_offset', null, null, -2, 1910, 100),
    (193, 193, 1, 'easter_offset', null, null, 1, 1995, 100),
    (194, 194, 1, 'fixed_date', 4, 27, null, 1995, 100),
    (195, 195, 1, 'fixed_date', 5, 1, null, 1995, 100),
    (196, 196, 1, 'fixed_date', 6, 16, null, 1995, 100),
    (197, 197, 1, 'fixed_date', 8, 9, null, 1995, 100),
    (198, 198, 1, 'fixed_date', 9, 24, null, 1995, 100),
    (199, 199, 1, 'fixed_date', 12, 16, null, 1995, 100),
    (200, 200, 1, 'fixed_date', 12, 25, null, 1910, 100),
    (201, 201, 1, 'fixed_date', 12, 26, null, 1980, 100);

insert into holiday_observance_rule(
    holiday_observance_rule_id,
    holiday_id,
    holiday_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    valid_from_year,
    priority,
    notes
) values
    (190, 190, 190, 'next_monday', 'New Year''s Day (observed)', '7', 'N', 1995, 100, 'Under the Public Holidays Act, when a public holiday falls on a Sunday the following Monday is a public holiday.'),
    (191, 191, 191, 'next_monday', 'Human Rights Day (observed)', '7', 'N', 1995, 100, 'Sunday-only Monday observance.'),
    (194, 194, 194, 'next_monday', 'Freedom Day (observed)', '7', 'N', 1995, 100, 'Sunday-only Monday observance.'),
    (195, 195, 195, 'next_monday', 'Workers'' Day (observed)', '7', 'N', 1995, 100, 'Sunday-only Monday observance.'),
    (196, 196, 196, 'next_monday', 'Youth Day (observed)', '7', 'N', 1995, 100, 'Sunday-only Monday observance.'),
    (197, 197, 197, 'next_monday', 'National Women''s Day (observed)', '7', 'N', 1995, 100, 'Sunday-only Monday observance.'),
    (198, 198, 198, 'next_monday', 'Heritage Day (observed)', '7', 'N', 1995, 100, 'Sunday-only Monday observance.'),
    (199, 199, 199, 'next_monday', 'Day of Reconciliation (observed)', '7', 'N', 1995, 100, 'Sunday-only Monday observance.'),
    (200, 200, 200, 'next_monday', 'Christmas Day (observed)', '7', 'N', 1910, 100, 'Sunday-only Monday observance; special presidential decrees may add an extra day in some years.'),
    (201, 201, 201, 'next_monday', 'Day of Goodwill (observed)', '7', 'N', 1980, 100, 'Sunday-only Monday observance; special presidential decrees may add an extra day in some years.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    holiday_observance_rule_id,
    source_document_id,
    role
) values
    (190, 190, 190, 10, 'observance'),
    (191, 191, 191, 10, 'observance'),
    (192, 192, null, 10, 'definition'),
    (193, 193, null, 10, 'definition'),
    (194, 194, 194, 10, 'observance'),
    (195, 195, 195, 10, 'observance'),
    (196, 196, 196, 10, 'observance'),
    (197, 197, 197, 10, 'observance'),
    (198, 198, 198, 10, 'observance'),
    (199, 199, 199, 10, 'observance'),
    (200, 200, 200, 10, 'observance'),
    (201, 201, 201, 10, 'observance');

-- Denmark national/public holiday seed rules.
insert into holiday_definition(
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
values
    (210, 'DK', 'new_years_day', 'Nytarsdag', 'public', 'full_day', 'gregory', 1926, null,
     'Nationwide public holiday.'),
    (211, 'DK', 'maundy_thursday', 'Skartorsdag', 'public', 'full_day', 'gregory', 1926, null,
     'Thursday before Easter Sunday.'),
    (212, 'DK', 'good_friday', 'Langfredag', 'public', 'full_day', 'gregory', 1926, null,
     'Friday before Easter Sunday.'),
    (213, 'DK', 'easter_sunday', 'Paskedag', 'public', 'full_day', 'gregory', 1926, null,
     'Gregorian Easter Sunday.'),
    (214, 'DK', 'easter_monday', 'Anden paskedag', 'public', 'full_day', 'gregory', 1926, null,
     'Monday after Easter Sunday.'),
    (215, 'DK', 'great_prayer_day', 'Store bededag', 'public', 'full_day', 'gregory', 1926, 2023,
     'Great Prayer Day remained a public holiday through 2023.'),
    (216, 'DK', 'ascension_day', 'Kristi himmelfartsdag', 'public', 'full_day', 'gregory', 1926, null,
     '39 days after Easter Sunday.'),
    (217, 'DK', 'whit_sunday', 'Pinsedag', 'public', 'full_day', 'gregory', 1926, null,
     '49 days after Easter Sunday.'),
    (218, 'DK', 'whit_monday', 'Anden pinsedag', 'public', 'full_day', 'gregory', 1926, null,
     '50 days after Easter Sunday.'),
    (219, 'DK', 'christmas_day', 'Juledag', 'public', 'full_day', 'gregory', 1926, null,
     'First Christmas Day; Christmas Eve is culturally important but not modeled here as a nationwide public holiday.'),
    (220, 'DK', 'second_christmas_day', 'Anden juledag', 'public', 'full_day', 'gregory', 1926, null,
     'Second Christmas Day.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (210, 'da-DK', 'Nytarsdag', 'Y'),
    (211, 'da-DK', 'Skartorsdag', 'Y'),
    (212, 'da-DK', 'Langfredag', 'Y'),
    (213, 'da-DK', 'Paskedag', 'Y'),
    (214, 'da-DK', 'Anden paskedag', 'Y'),
    (215, 'da-DK', 'Store bededag', 'Y'),
    (216, 'da-DK', 'Kristi himmelfartsdag', 'Y'),
    (217, 'da-DK', 'Pinsedag', 'Y'),
    (218, 'da-DK', 'Anden pinsedag', 'Y'),
    (219, 'da-DK', 'Juledag', 'Y'),
    (220, 'da-DK', 'Anden juledag', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    offset_days,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (210, 210, 1, 'fixed_date', 1, 1, null, 1926, null, 100, null),
    (211, 211, 1, 'easter_offset', null, null, -3, 1926, null, 100, null),
    (212, 212, 1, 'easter_offset', null, null, -2, 1926, null, 100, null),
    (213, 213, 1, 'easter_offset', null, null, 0, 1926, null, 100, 'Gregorian Easter Sunday.'),
    (214, 214, 1, 'easter_offset', null, null, 1, 1926, null, 100, null),
    (215, 215, 1, 'easter_offset', null, null, 26, 1926, 2023, 100, null),
    (216, 216, 1, 'easter_offset', null, null, 39, 1926, null, 100, null),
    (217, 217, 1, 'easter_offset', null, null, 49, 1926, null, 100, null),
    (218, 218, 1, 'easter_offset', null, null, 50, 1926, null, 100, null),
    (219, 219, 1, 'fixed_date', 12, 25, null, 1926, null, 100, null),
    (220, 220, 1, 'fixed_date', 12, 26, null, 1926, null, 100, null);

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    source_document_id,
    role
) values
    (210, 210, 12, 'definition'),
    (211, 211, 12, 'definition'),
    (212, 212, 12, 'definition'),
    (213, 213, 12, 'definition'),
    (214, 214, 12, 'definition'),
    (215, 215, 12, 'definition'),
    (216, 216, 12, 'definition'),
    (217, 217, 12, 'definition'),
    (218, 218, 12, 'definition'),
    (219, 219, 12, 'definition'),
    (220, 220, 12, 'definition');

-- Netherlands national/public holiday seed rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    notes
) values
    (230, 'NL', 'new_years_day', 'Nieuwjaarsdag', 'public', 'full_day', 'gregory', 1926, 'Nationwide New Year holiday.'),
    (231, 'NL', 'good_friday', 'Goede Vrijdag', 'public', 'full_day', 'gregory', 1926, 'National holiday, though not universally a mandatory paid day off.'),
    (232, 'NL', 'easter_sunday', 'Eerste Paasdag', 'public', 'full_day', 'gregory', 1926, 'First Easter Day.'),
    (233, 'NL', 'easter_monday', 'Tweede Paasdag', 'public', 'full_day', 'gregory', 1926, 'Second Easter Day.'),
    (234, 'NL', 'kings_day', 'Koningsdag', 'public', 'full_day', 'gregory', 2014, '27 April, moved to the previous weekday when 27 April falls on a Sunday.'),
    (235, 'NL', 'liberation_day', 'Bevrijdingsdag', 'public', 'full_day', 'gregory', 1945, 'National holiday, but employer practice differs on whether it is a paid day off every year.'),
    (236, 'NL', 'ascension_day', 'Hemelvaartsdag', 'public', 'full_day', 'gregory', 1926, '39 days after Easter Sunday.'),
    (237, 'NL', 'whit_sunday', 'Eerste Pinksterdag', 'public', 'full_day', 'gregory', 1926, 'First Pentecost Day.'),
    (238, 'NL', 'whit_monday', 'Tweede Pinksterdag', 'public', 'full_day', 'gregory', 1926, 'Second Pentecost Day.'),
    (239, 'NL', 'christmas_day', 'Eerste Kerstdag', 'public', 'full_day', 'gregory', 1926, 'First Christmas Day; Christmas Eve is widely celebrated but not modeled here as a nationwide public holiday.'),
    (240, 'NL', 'second_christmas_day', 'Tweede Kerstdag', 'public', 'full_day', 'gregory', 1926, 'Second Christmas Day.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (230, 'nl-NL', 'Nieuwjaarsdag', 'Y'),
    (231, 'nl-NL', 'Goede Vrijdag', 'Y'),
    (232, 'nl-NL', 'Eerste Paasdag', 'Y'),
    (233, 'nl-NL', 'Tweede Paasdag', 'Y'),
    (234, 'nl-NL', 'Koningsdag', 'Y'),
    (235, 'nl-NL', 'Bevrijdingsdag', 'Y'),
    (236, 'nl-NL', 'Hemelvaartsdag', 'Y'),
    (237, 'nl-NL', 'Eerste Pinksterdag', 'Y'),
    (238, 'nl-NL', 'Tweede Pinksterdag', 'Y'),
    (239, 'nl-NL', 'Eerste Kerstdag', 'Y'),
    (240, 'nl-NL', 'Tweede Kerstdag', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    offset_days,
    valid_from_year,
    priority,
    notes
) values
    (230, 230, 1, 'fixed_date', 1, 1, null, 1926, 100, null),
    (231, 231, 1, 'easter_offset', null, null, -2, 1926, 100, null),
    (232, 232, 1, 'easter_offset', null, null, 0, 1926, 100, 'Gregorian Easter Sunday.'),
    (233, 233, 1, 'easter_offset', null, null, 1, 1926, 100, null),
    (234, 234, 1, 'fixed_date', 4, 27, null, 2014, 100, null),
    (235, 235, 1, 'fixed_date', 5, 5, null, 1945, 100, null),
    (236, 236, 1, 'easter_offset', null, null, 39, 1926, 100, null),
    (237, 237, 1, 'easter_offset', null, null, 49, 1926, 100, null),
    (238, 238, 1, 'easter_offset', null, null, 50, 1926, 100, null),
    (239, 239, 1, 'fixed_date', 12, 25, null, 1926, 100, null),
    (240, 240, 1, 'fixed_date', 12, 26, null, 1926, 100, null);

insert into holiday_observance_rule(
    holiday_observance_rule_id,
    holiday_id,
    holiday_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    valid_from_year,
    priority,
    notes
) values
    (230, 234, 234, 'previous_weekday', 'Koningsdag', '7', 'Y', 2014, 100, 'When 27 April falls on a Sunday, King''s Day is observed on the previous weekday.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    holiday_observance_rule_id,
    source_document_id,
    role
) values
    (230, 230, null, 13, 'definition'),
    (231, 231, null, 13, 'definition'),
    (232, 232, null, 13, 'definition'),
    (233, 233, null, 13, 'definition'),
    (234, 234, 230, 13, 'observance'),
    (235, 235, null, 13, 'definition'),
    (236, 236, null, 13, 'definition'),
    (237, 237, null, 13, 'definition'),
    (238, 238, null, 13, 'definition'),
    (239, 239, null, 13, 'definition'),
    (240, 240, null, 13, 'definition');

.read packaging/jurisdiction-db/mars_generated_first_class_rules.sql
.read packaging/jurisdiction-db/mars_manual_first_class_rules.sql
