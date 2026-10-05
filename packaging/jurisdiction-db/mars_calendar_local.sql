-- Location-specific calendar; provisioned by install-jurisdiction-db.
-- Querying needs SQLite 3.35+ with built-in maths, but no MARS functions.
-- The installer fills the location and daily policy tables for 2015 through
-- the installation year plus seven. Reinstall to refresh policy and coverage.

drop view if exists calendar_local;
create table if not exists calendar_local_settings (
    singleton integer primary key check (singleton = 1),
    location text not null,
    jurisdiction text not null,
    timezone text not null,
    latitude real not null check (latitude between -90 and 90),
    longitude real not null check (longitude between -180 and 180),
    first_year integer not null,
    last_year integer not null check (last_year >= first_year),
    locale text
);
create table if not exists calendar_local_days (
    calendar_date text primary key,
    holiday_name text,
    is_weekend integer not null check (is_weekend in (0, 1)),
    utc_offset_hours real not null
);

create view calendar_local as
with
settings as materialized (
    select settings.*, coalesce(settings.locale, local_language.locale, language.locale) as display_locale,
           coalesce(local_language.locale, language.locale) as regional_locale
    from calendar_local_settings as settings
    join calendar_territory_locale as language on language.territory = substr(settings.jurisdiction, 1, 2)
    left join calendar_territory_locale as local_language on local_language.territory = settings.jurisdiction
    where singleton = 1
),
calendar_policy as (
    select *, case when is_weekend = 1 or holiday_name is not null then 0 else 1 end as is_workday
    from calendar_local_days
),
dates as (
    select calendar_date from calendar_policy
),
month_names as materialized (
    select names.* from calendar_locale_month as names
    join settings on names.locale = settings.display_locale
),
weekday_names as materialized (
    select names.* from calendar_locale_weekday as names
    join settings on names.locale = settings.display_locale
),
regional_month_names as materialized (
    select names.* from calendar_locale_month as names
    join settings on names.locale = settings.regional_locale
),
calendar_parts as (
    select calendar_date,
           cast(strftime('%Y', calendar_date) as integer) as year_no,
           cast(strftime('%m', calendar_date) as integer) as month_no,
           cast(strftime('%d', calendar_date) as integer) as day_no,
           (cast(strftime('%w', calendar_date) as integer) + 6) % 7 as weekday_no
    from dates
),
calendar_dates as materialized (
    select p.*,
           case when day_no in (1, 21, 31) then 'ˢᵗ'
                when day_no in (2, 22) then 'ⁿᵈ'
                when day_no in (3, 23) then 'ʳᵈ'
                else 'ᵗʰ' end as uk_day_suffix,
           date(calendar_date, 'start of month', '+1 month', '-1 day') as month_end,
           date(calendar_date, printf('-%d days', weekday_no)) as week_start,
           date(calendar_date, printf('%+d days', 6 - weekday_no)) as week_end,
           year_no + (month_no > 3) as fiscal_year_end,
           (month_no + 8) % 12 + 1 as fiscal_month,
           policy.holiday_name, policy.is_weekend, policy.utc_offset_hours, policy.is_workday,
           -- Calculate before outer filtering, without restarting at year boundaries.
           sum(policy.is_workday) over (
               order by p.calendar_date rows between unbounded preceding and current row
           ) as working_day_no
    from calendar_parts as p
    join calendar_policy as policy using (calendar_date)
),
-- Civil Hijri calendar: Friday epoch (JDN 1948440), 30-year leap cycle.
-- Restrict conversion to Arabic display; policy and civil date keys stay Gregorian.
hijri_days as (
    select c.calendar_date, cast(julianday(c.calendar_date) + 0.5 as integer) - 1948440 as days_from_epoch
    from calendar_dates as c cross join settings as s
    where s.display_locale = 'ar' or substr(s.display_locale, 1, 3) = 'ar_'
),
hijri_years as (
    select *, (30 * days_from_epoch + 10646) / 10631 as year_no from hijri_days
),
hijri_ordinals as (
    select *, days_from_epoch - (year_no - 1) * 354 - (3 + 11 * year_no) / 30 as day_of_year
    from hijri_years
),
hijri_months as (
    select *, min(12, 2 * day_of_year / 59 + 1) as month_no from hijri_ordinals
),
hijri_dates as materialized (
    select calendar_date, year_no, month_no, day_of_year - (59 * (month_no - 1) + 1) / 2 + 1 as day_no
    from hijri_months
),
hijri_month_names(month_no, full_name) as (
    values (1, 'محرم'), (2, 'صفر'), (3, 'ربيع الأول'), (4, 'ربيع الآخر'),
           (5, 'جمادى الأولى'), (6, 'جمادى الآخرة'), (7, 'رجب'), (8, 'شعبان'),
           (9, 'رمضان'), (10, 'شوال'), (11, 'ذو القعدة'), (12, 'ذو الحجة')
),
-- Fixed Hebrew calendar, including both exceptional new-year postponements.
-- Calculate starts once per candidate year, not separately for every date.
hebrew_days as materialized (
    select c.calendar_date, c.year_no + 3760 as estimate,
           cast(julianday(c.calendar_date) + 0.5 as integer) as jdn
    from calendar_dates as c cross join settings as s
    where s.display_locale = 'he' or substr(s.display_locale, 1, 3) = 'he_'
),
hebrew_offsets(offset) as (values (-1), (0), (1), (2), (3)),
hebrew_candidates as (
    select distinct estimate + offset as year_no from hebrew_days cross join hebrew_offsets
),
hebrew_molads as (
    select year_no, (235 * year_no - 234) / 19 as months_elapsed from hebrew_candidates
),
hebrew_elapsed as (
    select year_no, 29 * months_elapsed + (12084 + 13753 * months_elapsed) / 25920 as days_elapsed
    from hebrew_molads
),
hebrew_weekday_delay as materialized (
    select year_no, days_elapsed + ((3 * (days_elapsed + 1)) % 7 < 3) as days_elapsed
    from hebrew_elapsed
),
hebrew_new_years as materialized (
    select y.year_no, 347998 + y.days_elapsed
           + case when following.days_elapsed - y.days_elapsed = 356 then 2
                  when y.days_elapsed - previous.days_elapsed = 382 then 1 else 0 end as start_jdn
    from hebrew_weekday_delay as y
    join hebrew_weekday_delay as previous on previous.year_no = y.year_no - 1
    join hebrew_weekday_delay as following on following.year_no = y.year_no + 1
),
hebrew_years as materialized (
    select y.year_no, y.start_jdn, following.start_jdn - y.start_jdn as year_length,
           (7 * y.year_no + 1) % 19 < 7 as is_leap
    from hebrew_new_years as y
    join hebrew_new_years as following on following.year_no = y.year_no + 1
),
-- Stable month slots: slot 6 (Adar I) is absent in a common year.
-- Hebrew spellings follow Unicode CLDR (Unicode-3.0); see THIRD_PARTY_NOTICES.md.
hebrew_month_names(month_no, full_name, ordinary_length) as (
    values (1, 'תשרי', 30), (2, 'חשוון', 29), (3, 'כסלו', 30), (4, 'טבת', 29),
           (5, 'שבט', 30), (6, 'אדר א׳', 30), (7, 'אדר', 29), (8, 'ניסן', 30),
           (9, 'אייר', 29), (10, 'סיוון', 30), (11, 'תמוז', 29), (12, 'אב', 30), (13, 'אלול', 29)
),
hebrew_month_lengths as (
    select y.*, m.month_no,
           case when m.month_no = 7 and y.is_leap then 'אדר ב׳' else m.full_name end as full_name,
           case when m.month_no = 2 then 29 + (y.year_length % 10 = 5)
                when m.month_no = 3 then 30 - (y.year_length % 10 = 3)
                else m.ordinary_length end as month_length
    from hebrew_years as y cross join hebrew_month_names as m
    where m.month_no <> 6 or y.is_leap
),
hebrew_months as materialized (
    select *, start_jdn + sum(month_length) over (
        partition by year_no order by month_no rows unbounded preceding
    ) - month_length as month_start
    from hebrew_month_lengths
),
hebrew_dates as materialized (
    select d.calendar_date, m.year_no, m.month_no, m.full_name, d.jdn - m.month_start + 1 as day_no
    from hebrew_days as d
    -- Indexed year lookup; at most two years of twelve/thirteen months per date.
    join hebrew_months as m on m.year_no in (d.estimate, d.estimate + 1)
        and d.jdn >= m.month_start and d.jdn < m.month_start + m.month_length
),
-- Evaluate the original solar model at noon UTC for each civil date.
-- Materialised stages bound expression expansion and share intermediate values.
solar_epoch as materialized (
    select c.calendar_date, c.utc_offset_hours, s.latitude, s.longitude,
           (julianday(c.calendar_date) + 0.5 - 2451545.0) / 36525.0 as t
    from calendar_dates as c cross join settings as s
),
solar_orbit as materialized (
    select *,
           mod(280.46646 + t * (36000.76983 + t * 0.0003032), 360.0) as mean_longitude,
           357.52911 + t * (35999.05029 - 0.0001537 * t) as mean_anomaly,
           0.016708634 - t * (0.000042037 + 0.0000001267 * t) as eccentricity,
           23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0
               as mean_obliquity
    from solar_epoch
),
solar_angles as materialized (
    select *,
           mean_longitude
               + sin(radians(mean_anomaly)) * (1.914602 - t * (0.004817 + 0.000014 * t))
               + sin(radians(2 * mean_anomaly)) * (0.019993 - 0.000101 * t)
               + sin(radians(3 * mean_anomaly)) * 0.000289
               - 0.00569 - 0.00478 * sin(radians(125.04 - 1934.136 * t)) as apparent_longitude,
           mean_obliquity + 0.00256 * cos(radians(125.04 - 1934.136 * t)) as obliquity
    from solar_orbit
),
solar_declination as materialized (
    select *,
           asin(sin(radians(obliquity)) * sin(radians(apparent_longitude))) as declination,
           tan(radians(obliquity / 2.0)) * tan(radians(obliquity / 2.0)) as y
    from solar_angles
),
solar_circumstances as materialized (
    select calendar_date,
           720.0 - 4.0 * longitude + 60.0 * utc_offset_hours
               - 4.0 * degrees(
                   y * sin(2.0 * radians(mean_longitude))
                   - 2.0 * eccentricity * sin(radians(mean_anomaly))
                   + 4.0 * eccentricity * y * sin(radians(mean_anomaly)) * cos(2.0 * radians(mean_longitude))
                   - 0.5 * y * y * sin(4.0 * radians(mean_longitude))
                   - 1.25 * eccentricity * eccentricity * sin(2.0 * radians(mean_anomaly))
               ) as solar_noon_minutes,
           degrees(acos(
               cos(radians(90.833)) / (cos(radians(latitude)) * cos(declination))
               - tan(radians(latitude)) * tan(declination)
           )) as hour_angle
    from solar_declination
)
select
    c.calendar_date as FullDateAlternateKey,
    c.calendar_date || ' 23:59:59' as "FullDate and End Time",
    c.year_no as "Year",
    c.month_no as "Month",
    c.day_no as "Day",
    cast(strftime('%d', c.month_end) as integer) as "Days in Month",
    date(c.calendar_date, 'start of month') || ' 00:00:00' as "MC Dates",
    c.month_end || ' 23:59:59' as "ME Dates",
    month_name.short_name || ' ' || printf('%04d', c.year_no) as "ME Dates Text",
    c.week_start as "WC Dates",
    c.week_end as "WE Dates",
    date(c.week_start, '+4 days') as "WEDates (Friday)",
    c.week_end || ' 23:59:59' as "WE Dates plus End Time",
    strftime('%d', c.week_end) || ' ' || week_month.short_name || ' ' || strftime('%Y', c.week_end) as "WE Dates Text",
    c.weekday_no as "Day of Week",
    weekday_name.day_name as "Day Name",
    month_label.full_name as "Month Name",
    c.fiscal_year_end as "Fiscal Year End",
    c.fiscal_month as "Fiscal Month",
    (c.fiscal_month - 1) / 3 + 1 as "Fiscal Quarter No",
    'Qtr ' || ((c.fiscal_month - 1) / 3 + 1) as "Fiscal Qtr",
    printf('%04d Q%d', c.fiscal_year_end, (c.fiscal_month - 1) / 3 + 1) as [Fiscal Quarter Name],
    printf('%02d-%02d', (c.fiscal_year_end - 1) % 100, c.fiscal_year_end % 100) as "Fiscal Years",
    c.holiday_name as "Public Holiday",
    case when c.is_weekend = 1 then 'Weekend'
         when c.holiday_name is not null then 'Public Holiday' end as "Non working day Type",
    c.is_workday as "Workday Type",
    c.working_day_no as [Working Day No],
    case when c.is_workday = 1 then 'WorkDay' else 'NonWorkDay' end as "Day Type",
    case when solar.hour_angle is not null then
        time('00:00:00', printf('%+d minutes', cast(round(solar.solar_noon_minutes - 4.0 * solar.hour_angle) as integer)))
    end as "Sunrise",
    case when solar.hour_angle is not null then
        time('00:00:00', printf('%+d minutes', cast(round(solar.solar_noon_minutes + 4.0 * solar.hour_angle) as integer)))
    end as "Sunset",
    cast(100.0 - abs(
        (julianday(c.calendar_date) - julianday('2000-01-06')) / 29.53
        - floor((julianday(c.calendar_date) - julianday('2000-01-06')) / 29.53) - 0.5
    ) * 200.0 + 0.5 as integer) as "Moon Phase %",
    c.day_no || c.uk_day_suffix
        || ' ' || uk_month.full_name || ' ' || printf('%04d', c.year_no) as [Date UK],
    month_name.short_name as "Month Name Abbrev",
    weekday_name.short_name as "Day Name Abbrev",
    (select group_concat(
        case when month_name.locale = 'ar' or substr(month_name.locale, 1, 3) = 'ar_' then
            replace(replace(replace(replace(replace(
            replace(replace(replace(replace(replace(part,
                '0', '٠'), '1', '١'), '2', '٢'), '3', '٣'), '4', '٤'),
                '5', '٥'), '6', '٦'), '7', '٧'), '8', '٨'), '9', '٩')
        else part end, '') from (
        select case p.field
                   when 'literal' then p.literal
                   when 'year' then printf('%04d', coalesce(hijri.year_no, hebrew.year_no, c.year_no))
                   when 'month' then cast(coalesce(hijri.month_no, hebrew.month_no, c.month_no) as text)
                   when 'month_short' then coalesce(hijri_month.full_name, hebrew.full_name, month_name.short_name)
                   when 'month_full' then coalesce(hijri_month.full_name, hebrew.full_name, month_name.full_name)
                   when 'day' then cast(coalesce(hijri.day_no, hebrew.day_no, c.day_no) as text)
                   when 'day_padded' then printf('%02d', coalesce(hijri.day_no, hebrew.day_no, c.day_no))
                   when 'first_day_suffix' then case when coalesce(hijri.day_no, hebrew.day_no, c.day_no) = 1
                                                    then p.literal else '' end
               end || case when p.field in ('day', 'day_padded')
                                and (s.jurisdiction = 'GB' or s.jurisdiction glob 'GB-*')
                                and (s.display_locale = 'en' or s.display_locale glob 'en_*')
                           then c.uk_day_suffix else '' end as part
        from calendar_date_pattern_part as p
        where p.pattern_id = date_pattern.pattern_id
        order by p.position
    )) as [Date Lingua],
    -- Regional civil dates use the jurisdiction default, not the chosen display language.
    (select group_concat(
        case when regional_month.locale = 'ar' or substr(regional_month.locale, 1, 3) = 'ar_' then
            replace(replace(replace(replace(replace(
            replace(replace(replace(replace(replace(part,
                '0', '٠'), '1', '١'), '2', '٢'), '3', '٣'), '4', '٤'),
                '5', '٥'), '6', '٦'), '7', '٧'), '8', '٨'), '9', '٩')
        else part end, '') from (
        select case p.field
                   when 'literal' then p.literal
                   when 'year' then printf('%04d', c.year_no)
                   when 'month' then cast(c.month_no as text)
                   when 'month_short' then regional_month.short_name
                   when 'month_full' then regional_month.full_name
                   when 'day' then cast(c.day_no as text)
                   when 'day_padded' then printf('%02d', c.day_no)
                   when 'first_day_suffix' then case when c.day_no = 1 then p.literal else '' end
               end as part
        from calendar_date_pattern_part as p
        where p.pattern_id = regional_pattern.pattern_id
        order by p.position
    )) as [Date Regional]
from calendar_dates as c
cross join settings as s
left join hijri_dates as hijri on hijri.calendar_date = c.calendar_date
left join hijri_month_names as hijri_month on hijri_month.month_no = hijri.month_no
left join hebrew_dates as hebrew on hebrew.calendar_date = c.calendar_date
join month_names as month_name on month_name.month_no = c.month_no
join calendar_locale_month_standalone as month_label
    on month_label.locale = month_name.locale and month_label.month_no = c.month_no
join calendar_locale_date_pattern as date_pattern on date_pattern.locale = month_name.locale
join regional_month_names as regional_month on regional_month.month_no = c.month_no
join calendar_locale_date_pattern as regional_pattern on regional_pattern.locale = regional_month.locale
join month_names as week_month on week_month.month_no = cast(strftime('%m', c.week_end) as integer)
join weekday_names as weekday_name on weekday_name.weekday_no = c.weekday_no
join calendar_locale_month as uk_month on uk_month.locale = 'en_GB' and uk_month.month_no = c.month_no
join solar_circumstances as solar on solar.calendar_date = c.calendar_date;
