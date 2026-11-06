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

-- Uninhabited territories still need selectable country references, not fictitious towns.
-- Bouvetøya: Norwegian Polar Institute / SCAR gazetteer, 54°25.3'S, 3°21.5'E.
-- Heard Island: Australian Antarctic Division survey mark AUS347 at Atlas Cove.
-- Sources and the UTC reference-time convention for Bouvetøya are recorded in THIRD_PARTY_NOTICES.md.
insert or ignore into timezone_definition(timezone_name) values('Etc/UTC');
insert into timezone_code(timezone_name, timezone_code)
select 'Etc/UTC', coalesce(max(timezone_code), 0)+1 from timezone_code
having not exists(select 1 from timezone_code where timezone_name='Etc/UTC');
insert or ignore into timezone_canonical(timezone_name, canonical_timezone_name) values('Etc/UTC','Etc/UTC');
insert into timezone_era(timezone_name, sequence_no, gmtoff_minutes, rules_kind, format_text)
select 'Etc/UTC', 1, 0, 'none', 'UTC'
where not exists(select 1 from timezone_era where timezone_name='Etc/UTC');

create temporary table country_reference(jurisdiction_id text, latitude text, longitude text,
                                       timezone_name text, locality_name text);
insert into country_reference values
    ('BV', '-54.42166667', '3.35833333', 'Etc/UTC', 'Bouvet Island'),
    ('HM', '-53.01888867', '73.39205128', 'Indian/Kerguelen', 'Atlas Cove');

insert or ignore into jurisdiction_location_default(jurisdiction_id)
select reference.jurisdiction_id from country_reference as reference
join jurisdiction using(jurisdiction_id);
insert or ignore into jurisdiction_location_default_latitude(jurisdiction_id, latitude)
select reference.jurisdiction_id, reference.latitude from country_reference as reference
join jurisdiction_location_default using(jurisdiction_id);
insert or ignore into jurisdiction_location_default_longitude(jurisdiction_id, longitude)
select reference.jurisdiction_id, reference.longitude from country_reference as reference
join jurisdiction_location_default using(jurisdiction_id);
insert or ignore into jurisdiction_location_default_timezone(jurisdiction_id, timezone_code)
select reference.jurisdiction_id, zone.timezone_code from country_reference as reference
join jurisdiction_location_default using(jurisdiction_id)
join timezone_code as zone on zone.timezone_name=reference.timezone_name;
insert or ignore into jurisdiction_location_default_locality(jurisdiction_id, locality_name)
select reference.jurisdiction_id, reference.locality_name from country_reference as reference
join jurisdiction_location_default using(jurisdiction_id);
drop table country_reference;

-- Retire temporary inspection markers; foreign-key cascades remove their catalogue attributes.
delete from jurisdiction_town_entity where jurisdiction_town_id in (
    select jurisdiction_town_id from jurisdiction_town where jurisdiction_id='DK' and town_name in (
        'Kolindsund (former lake)', 'Tastum Sø (former lake)', 'Gårdbo Sø (former lake)',
        'Sjørring Sø (former lake)', 'Sperring Sø (former lake)'
    )
);
