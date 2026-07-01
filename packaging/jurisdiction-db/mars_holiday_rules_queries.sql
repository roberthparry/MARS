-- SQLCipher jurisdiction database query helpers.
-- Open the target database first, then run:
--   pragma key = 'your chosen key';

pragma foreign_keys = on;

.tables

select
    jurisdiction_id,
    parent_jurisdiction_id,
    jurisdiction_type,
    iso_country_code,
    iso_subdivision_code,
    name
from jurisdiction
order by jurisdiction_id;

select
    jurisdiction_weekend_rule_id,
    jurisdiction_id,
    weekend_mask,
    valid_from_year,
    valid_to_year,
    notes
from jurisdiction_weekend_rule
order by jurisdiction_id, valid_from_year, valid_to_year, jurisdiction_weekend_rule_id;

select
    calendar_system_id,
    cldr_rscale,
    family,
    name
from calendar_system
order by calendar_system_id;

select
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    calendar_system_id
from holiday_definition
where jurisdiction_id = 'GB-ENG'
order by holiday_id;

select
    holiday_rule_id,
    holiday_id,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    holiday_date,
    priority
from holiday_rule
order by holiday_id, priority, sequence_no;

select
    holiday_observance_rule_id,
    holiday_id,
    observed_rule_kind,
    observed_name,
    suppress_original,
    priority
from holiday_observance_rule
order by holiday_id, priority;

select
    holiday_exception_id,
    jurisdiction_id,
    holiday_date,
    action,
    name,
    replacement_holiday_key
from holiday_exception
order by holiday_date, priority;

select
    source_document_id,
    source_type,
    citation,
    source_url
from source_document
order by source_document_id;

select
    jurisdiction_id,
    COUNT(*) as holiday_count
from holiday_instance
group by jurisdiction_id
order by holiday_count desc, jurisdiction_id
limit 20;

select
    MIN(holiday_date) as first_holiday_date,
    MAX(holiday_date) as last_holiday_date,
    COUNT(*) as holiday_instance_count
from holiday_instance;

select
    jurisdiction_id,
    MIN(holiday_date) as first_holiday_date,
    MAX(holiday_date) as last_holiday_date,
    COUNT(*) as holiday_count
from holiday_instance
group by jurisdiction_id
order by first_holiday_date desc, jurisdiction_id
limit 20;

select
    jurisdiction_id,
    holiday_date,
    holiday_name,
    holiday_class
from holiday_instance
where jurisdiction_id in ('US', 'JP', 'IN', 'CN', 'BR', 'ZA')
order by jurisdiction_id, holiday_date, holiday_name
limit 120;

select
    jurisdiction_id,
    COUNT(*) as holiday_count
from holiday_instance
where jurisdiction_id in ('AU', 'AU-ACT', 'AU-NSW', 'AU-NT', 'AU-QLD', 'AU-SA', 'AU-TAS', 'AU-VIC', 'AU-WA')
  and holiday_date between '2026-01-01' and '2026-12-31'
group by jurisdiction_id
order by jurisdiction_id;

select
    jurisdiction_id,
    holiday_date,
    holiday_name,
    holiday_class
from holiday_instance
where jurisdiction_id in ('AU', 'AU-ACT', 'AU-NSW', 'AU-NT', 'AU-QLD', 'AU-SA', 'AU-TAS', 'AU-VIC', 'AU-WA')
  and holiday_date between '2026-01-01' and '2026-12-31'
order by jurisdiction_id, holiday_date, holiday_name;

select
    jurisdiction_id,
    COUNT(*) as holiday_count
from holiday_instance
where jurisdiction_id in (
    'US', 'US-AL', 'US-AK', 'US-AS', 'US-AZ', 'US-AR', 'US-CA', 'US-CO', 'US-CT',
    'US-DE', 'US-DC', 'US-FL', 'US-GA', 'US-GU', 'US-HI', 'US-ID', 'US-IL', 'US-IN',
    'US-IA', 'US-KS', 'US-KY', 'US-LA', 'US-ME', 'US-MD', 'US-MA', 'US-MI', 'US-MN',
    'US-MS', 'US-MO', 'US-MT', 'US-NE', 'US-NV', 'US-NH', 'US-NJ', 'US-NM', 'US-NY',
    'US-NC', 'US-ND', 'US-MP', 'US-OH', 'US-OK', 'US-OR', 'US-PA', 'US-PR', 'US-RI',
    'US-SC', 'US-SD', 'US-TN', 'US-TX', 'US-UM', 'US-UT', 'US-VT', 'US-VI', 'US-VA',
    'US-WA', 'US-WV', 'US-WI', 'US-WY'
)
  and holiday_date between '2026-01-01' and '2026-12-31'
group by jurisdiction_id
order by jurisdiction_id;

select
    jurisdiction_id,
    holiday_date,
    holiday_name,
    holiday_class
from holiday_instance
where jurisdiction_id in ('US', 'US-CA', 'US-NY', 'US-TX', 'US-FL', 'US-DC', 'US-PR')
  and holiday_date between '2026-01-01' and '2026-12-31'
order by jurisdiction_id, holiday_date, holiday_name;
