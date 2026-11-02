-- Normalise the shared core and worldwide town seeds for fresh installations.
.read packaging/jurisdiction-db/town_catalogue_seed.sql

create temp table jurisdiction_town_original_default as
select jurisdiction_id, min(town_name) as town_name from jurisdiction_town_seed group by jurisdiction_id;

-- Worldwide supplement: preserve original fallback choices before appending new rows.
.read packaging/jurisdiction-db/mars_jurisdiction_world_towns.sql
create index jurisdiction_town_seed_name on jurisdiction_town_seed(jurisdiction_id, town_name);

-- Island regions need their own town lists and defaults, not the mainland capital.
-- Retain the country entries for existing selections and country-wide searches.
with island_region(jurisdiction_id, timezone_name, default_town) as (
    values ('PT-20', 'Atlantic/Azores', 'Ponta Delgada'),
           ('PT-30', 'Atlantic/Madeira', 'Funchal')
)
insert into jurisdiction_town_seed
select region.jurisdiction_id, town.town_name, town.latitude, town.longitude,
       town.elevation_metres, town.timezone_code,
       case when town.town_name = region.default_town then 'Y' else 'N' end
from jurisdiction_town_seed as town
join timezone_code as zone on zone.timezone_code = town.timezone_code
join island_region as region on region.timezone_name = zone.timezone_name
where town.jurisdiction_id = 'PT';

create temp table jurisdiction_town_totality_seed(
    jurisdiction_id,
    town_name,
    totality_seed_priority
);

insert into jurisdiction_town_totality_seed(
    jurisdiction_id,
    town_name,
    totality_seed_priority
)
values
    ('ES', 'Oviedo', 0),
    ('ES', 'Madrid', 1),
    ('ES', 'Santander', 2),
    ('ES', 'Bilbao', 3),
    ('ES', 'Burgos', 4),
    ('ES', 'Zaragoza', 5),
    ('ES', 'Valencia', 6),
    ('GI', 'Gibraltar', 7),
    ('MA', 'Tangier', 8),
    ('MA', 'Rabat', 9),
    ('TN', 'Tunis', 10),
    ('LY', 'Tripoli', 11),
    ('EG', 'Alexandria', 12),
    ('EG', 'Cairo', 13),
    ('IS', 'Reykjavík', 14),
    ('GL', 'Nuuk', 15),
    ('PT', 'Lisbon', 16),
    ('FR', 'Paris', 17);

insert into jurisdiction_town(jurisdiction_id, town_name)
select jurisdiction_id, town_name
from jurisdiction_town_seed
order by rowid;

insert into jurisdiction_town_latitude(jurisdiction_town_id, latitude)
select town.jurisdiction_town_id, seed.latitude
from jurisdiction_town_seed as seed
join jurisdiction_town as town
  on town.jurisdiction_id = seed.jurisdiction_id
 and town.town_name = seed.town_name;

insert into jurisdiction_town_longitude(jurisdiction_town_id, longitude)
select town.jurisdiction_town_id, seed.longitude
from jurisdiction_town_seed as seed
join jurisdiction_town as town
  on town.jurisdiction_id = seed.jurisdiction_id
 and town.town_name = seed.town_name;

insert into jurisdiction_town_elevation(jurisdiction_town_id, elevation_metres)
select town.jurisdiction_town_id, seed.elevation_metres
from jurisdiction_town_seed as seed
join jurisdiction_town as town
  on town.jurisdiction_id = seed.jurisdiction_id
 and town.town_name = seed.town_name
-- Preserve missing DEM provenance in the seed, but do not store it as an elevation.
where seed.elevation_metres <> '-9999';

insert into jurisdiction_town_timezone(jurisdiction_town_id, timezone_code)
select town.jurisdiction_town_id, seed.timezone_code
from jurisdiction_town_seed as seed
join jurisdiction_town as town
  on town.jurisdiction_id = seed.jurisdiction_id
 and town.town_name = seed.town_name;

insert into jurisdiction_town_totality_seed_priority(jurisdiction_town_id, totality_seed_priority)
select town.jurisdiction_town_id, seed.totality_seed_priority
from jurisdiction_town_totality_seed as seed
join jurisdiction_town as town
  on town.jurisdiction_id = seed.jurisdiction_id
 and town.town_name = seed.town_name;

insert or ignore into jurisdiction_default_town(jurisdiction_id, jurisdiction_town_id)
select seed.jurisdiction_id, town.jurisdiction_town_id
from jurisdiction_town_seed as seed
join jurisdiction_town as town
  on town.jurisdiction_id = seed.jurisdiction_id
 and town.town_name = seed.town_name
where seed.is_default = 'Y';

insert or ignore into jurisdiction_default_town(jurisdiction_id, jurisdiction_town_id)
select seed.jurisdiction_id, town.jurisdiction_town_id
from jurisdiction_town_seed as seed
join jurisdiction_town as town
  on town.jurisdiction_id = seed.jurisdiction_id
 and town.town_name = seed.town_name
join jurisdiction_location_default_locality as locality
  on locality.jurisdiction_id = seed.jurisdiction_id
 and locality.locality_name = seed.town_name;

insert or ignore into jurisdiction_default_town(jurisdiction_id, jurisdiction_town_id)
select town.jurisdiction_id, town.jurisdiction_town_id
from jurisdiction_town as town
join jurisdiction_town_original_default as original
  on original.jurisdiction_id = town.jurisdiction_id and original.town_name = town.town_name;

insert or ignore into jurisdiction_default_town(jurisdiction_id, jurisdiction_town_id)
select town.jurisdiction_id, MIN(town.jurisdiction_town_id)
from jurisdiction_town as town
group by town.jurisdiction_id;

drop table jurisdiction_town_totality_seed;
drop table jurisdiction_town_original_default;
drop table jurisdiction_town_seed;
