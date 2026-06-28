-- SQLCipher jurisdiction database query helpers.
-- Open the target database first, then run:
--   PRAGMA key = 'your chosen key';

PRAGMA foreign_keys = ON;

.tables

SELECT
    jurisdiction_id,
    parent_jurisdiction_id,
    jurisdiction_type,
    iso_country_code,
    iso_subdivision_code,
    name
FROM jurisdiction
ORDER BY jurisdiction_id;

SELECT
    weekend_rule_id,
    jurisdiction_id,
    weekend_mask,
    valid_from_year,
    valid_to_year,
    notes
FROM jurisdiction_weekend_rule
ORDER BY jurisdiction_id, valid_from_year, valid_to_year, weekend_rule_id;

SELECT
    calendar_system_id,
    cldr_rscale,
    family,
    name
FROM calendar_system
ORDER BY calendar_system_id;

SELECT
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    calendar_system_id
FROM holiday_definition
WHERE jurisdiction_id = 'GB-ENG'
ORDER BY holiday_id;

SELECT
    rule_id,
    holiday_id,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    holiday_date,
    priority
FROM holiday_rule
ORDER BY holiday_id, priority, sequence_no;

SELECT
    observance_rule_id,
    holiday_id,
    observed_rule_kind,
    observed_name,
    suppress_original,
    priority
FROM holiday_observance_rule
ORDER BY holiday_id, priority;

SELECT
    exception_id,
    jurisdiction_id,
    holiday_date,
    action,
    name,
    replacement_holiday_key
FROM holiday_exception
ORDER BY holiday_date, priority;

SELECT
    source_document_id,
    source_type,
    citation,
    source_url
FROM source_document
ORDER BY source_document_id;

SELECT
    jurisdiction_id,
    COUNT(*) AS holiday_count
FROM holiday_instance
GROUP BY jurisdiction_id
ORDER BY holiday_count DESC, jurisdiction_id
LIMIT 20;

SELECT
    MIN(holiday_date) AS first_holiday_date,
    MAX(holiday_date) AS last_holiday_date,
    COUNT(*) AS holiday_instance_count
FROM holiday_instance;

SELECT
    jurisdiction_id,
    MIN(holiday_date) AS first_holiday_date,
    MAX(holiday_date) AS last_holiday_date,
    COUNT(*) AS holiday_count
FROM holiday_instance
GROUP BY jurisdiction_id
ORDER BY first_holiday_date DESC, jurisdiction_id
LIMIT 20;

SELECT
    jurisdiction_id,
    holiday_date,
    holiday_name,
    holiday_class
FROM holiday_instance
WHERE jurisdiction_id IN ('US', 'JP', 'IN', 'CN', 'BR', 'ZA')
ORDER BY jurisdiction_id, holiday_date, holiday_name
LIMIT 120;

SELECT
    jurisdiction_id,
    COUNT(*) AS holiday_count
FROM holiday_instance
WHERE jurisdiction_id IN ('AU', 'AU-ACT', 'AU-NSW', 'AU-NT', 'AU-QLD', 'AU-SA', 'AU-TAS', 'AU-VIC', 'AU-WA')
  AND holiday_date BETWEEN '2026-01-01' AND '2026-12-31'
GROUP BY jurisdiction_id
ORDER BY jurisdiction_id;

SELECT
    jurisdiction_id,
    holiday_date,
    holiday_name,
    holiday_class
FROM holiday_instance
WHERE jurisdiction_id IN ('AU', 'AU-ACT', 'AU-NSW', 'AU-NT', 'AU-QLD', 'AU-SA', 'AU-TAS', 'AU-VIC', 'AU-WA')
  AND holiday_date BETWEEN '2026-01-01' AND '2026-12-31'
ORDER BY jurisdiction_id, holiday_date, holiday_name;

SELECT
    jurisdiction_id,
    COUNT(*) AS holiday_count
FROM holiday_instance
WHERE jurisdiction_id IN (
    'US', 'US-AL', 'US-AK', 'US-AS', 'US-AZ', 'US-AR', 'US-CA', 'US-CO', 'US-CT',
    'US-DE', 'US-DC', 'US-FL', 'US-GA', 'US-GU', 'US-HI', 'US-ID', 'US-IL', 'US-IN',
    'US-IA', 'US-KS', 'US-KY', 'US-LA', 'US-ME', 'US-MD', 'US-MA', 'US-MI', 'US-MN',
    'US-MS', 'US-MO', 'US-MT', 'US-NE', 'US-NV', 'US-NH', 'US-NJ', 'US-NM', 'US-NY',
    'US-NC', 'US-ND', 'US-MP', 'US-OH', 'US-OK', 'US-OR', 'US-PA', 'US-PR', 'US-RI',
    'US-SC', 'US-SD', 'US-TN', 'US-TX', 'US-UM', 'US-UT', 'US-VT', 'US-VI', 'US-VA',
    'US-WA', 'US-WV', 'US-WI', 'US-WY'
)
  AND holiday_date BETWEEN '2026-01-01' AND '2026-12-31'
GROUP BY jurisdiction_id
ORDER BY jurisdiction_id;

SELECT
    jurisdiction_id,
    holiday_date,
    holiday_name,
    holiday_class
FROM holiday_instance
WHERE jurisdiction_id IN ('US', 'US-CA', 'US-NY', 'US-TX', 'US-FL', 'US-DC', 'US-PR')
  AND holiday_date BETWEEN '2026-01-01' AND '2026-12-31'
ORDER BY jurisdiction_id, holiday_date, holiday_name;
