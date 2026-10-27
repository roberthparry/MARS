-- Named reference points; source attribution is in THIRD_PARTY_NOTICES.md.
-- Shared by fresh installations and managed updates; retain existing town IDs and defaults.
create temporary table location_addition(jurisdiction_id text, town_name text, latitude text, longitude text, elevation text, timezone_name text);
insert into location_addition values
    ('ZA', 'Robben Island', '-33.8064', '18.3688', '17', 'Africa/Johannesburg'),
    ('ZA', 'Seal Island', '-34.1375', '18.5825', null, 'Africa/Johannesburg'),
    ('ZA', 'Duiker Island', '-34.05861', '18.32694', null, 'Africa/Johannesburg');

insert into jurisdiction_town(jurisdiction_id, town_name)
select a.jurisdiction_id, a.town_name from location_addition a
where exists(select 1 from jurisdiction where jurisdiction_id=a.jurisdiction_id)
  and not exists(select 1 from jurisdiction_town t where t.jurisdiction_id=a.jurisdiction_id and t.town_name=a.town_name);

insert or ignore into jurisdiction_town_latitude(jurisdiction_town_id, latitude)
select t.jurisdiction_town_id, a.latitude from location_addition a
join jurisdiction_town t on t.jurisdiction_id=a.jurisdiction_id and t.town_name=a.town_name;
insert or ignore into jurisdiction_town_longitude(jurisdiction_town_id, longitude)
select t.jurisdiction_town_id, a.longitude from location_addition a
join jurisdiction_town t on t.jurisdiction_id=a.jurisdiction_id and t.town_name=a.town_name;
insert or ignore into jurisdiction_town_elevation(jurisdiction_town_id, elevation_metres)
select t.jurisdiction_town_id, a.elevation from location_addition a
join jurisdiction_town t on t.jurisdiction_id=a.jurisdiction_id and t.town_name=a.town_name
where a.elevation is not null;
insert or ignore into jurisdiction_town_timezone(jurisdiction_town_id, timezone_code)
select t.jurisdiction_town_id, z.timezone_code from location_addition a
join jurisdiction_town t on t.jurisdiction_id=a.jurisdiction_id and t.town_name=a.town_name
join timezone_code z on z.timezone_name=a.timezone_name;
drop table location_addition;

-- Retire temporary inspection markers; foreign-key cascades remove their catalogue attributes.
delete from jurisdiction_town_entity where jurisdiction_town_id in (
    select jurisdiction_town_id from jurisdiction_town where jurisdiction_id='DK' and town_name in (
        'Kolindsund (former lake)', 'Tastum Sø (former lake)', 'Gårdbo Sø (former lake)',
        'Sjørring Sø (former lake)', 'Sperring Sø (former lake)'
    )
);
