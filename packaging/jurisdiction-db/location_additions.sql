-- Named island reference points; source attribution is in THIRD_PARTY_NOTICES.md.
-- Shared by fresh installations and managed updates; retain existing town IDs and defaults.
create temporary table location_addition(town_name text, latitude text, longitude text, elevation text);
insert into location_addition values
    ('Robben Island', '-33.8064', '18.3688', '17'),
    ('Seal Island', '-34.1375', '18.5825', null),
    ('Duiker Island', '-34.05861', '18.32694', null);

insert into jurisdiction_town(jurisdiction_id, town_name)
select 'ZA', a.town_name from location_addition a
where exists(select 1 from jurisdiction where jurisdiction_id='ZA')
  and not exists(select 1 from jurisdiction_town t where t.jurisdiction_id='ZA' and t.town_name=a.town_name);

insert or ignore into jurisdiction_town_latitude(jurisdiction_town_id, latitude)
select t.jurisdiction_town_id, a.latitude from location_addition a
join jurisdiction_town t on t.jurisdiction_id='ZA' and t.town_name=a.town_name;
insert or ignore into jurisdiction_town_longitude(jurisdiction_town_id, longitude)
select t.jurisdiction_town_id, a.longitude from location_addition a
join jurisdiction_town t on t.jurisdiction_id='ZA' and t.town_name=a.town_name;
insert or ignore into jurisdiction_town_elevation(jurisdiction_town_id, elevation_metres)
select t.jurisdiction_town_id, a.elevation from location_addition a
join jurisdiction_town t on t.jurisdiction_id='ZA' and t.town_name=a.town_name
where a.elevation is not null;
insert or ignore into jurisdiction_town_timezone(jurisdiction_town_id, timezone_code)
select t.jurisdiction_town_id, z.timezone_code from location_addition a
join jurisdiction_town t on t.jurisdiction_id='ZA' and t.town_name=a.town_name
join timezone_code z on z.timezone_name='Africa/Johannesburg';
drop table location_addition;
