-- Backfill both the core catalogue and worldwide supplement on older installations.
-- Preserve existing town IDs, memberships, coordinates, heights and selected defaults.
.read packaging/jurisdiction-db/town_catalogue_seed.sql
.read packaging/jurisdiction-db/mars_jurisdiction_world_towns.sql

CREATE TEMPORARY TABLE town_catalogue_missing AS
SELECT seed.*
FROM jurisdiction_town_seed AS seed
JOIN jurisdiction AS country ON country.jurisdiction_id=seed.jurisdiction_id
JOIN timezone_code AS zone ON zone.timezone_code=seed.timezone_code
WHERE NOT EXISTS (
    SELECT 1
    FROM jurisdiction_town_name AS name
    JOIN jurisdiction_town_jurisdiction_id AS membership USING(jurisdiction_town_id)
    WHERE name.town_name=seed.town_name
      AND substr(membership.jurisdiction_id,1,2)=substr(seed.jurisdiction_id,1,2)
);
CREATE INDEX town_catalogue_missing_name ON town_catalogue_missing(jurisdiction_id,town_name);

INSERT INTO jurisdiction_town(jurisdiction_id,town_name)
SELECT jurisdiction_id,town_name FROM town_catalogue_missing ORDER BY rowid;

INSERT INTO jurisdiction_town_latitude(jurisdiction_town_id,latitude)
SELECT town.jurisdiction_town_id,seed.latitude
FROM town_catalogue_missing AS seed
JOIN jurisdiction_town AS town USING(jurisdiction_id,town_name);

INSERT INTO jurisdiction_town_longitude(jurisdiction_town_id,longitude)
SELECT town.jurisdiction_town_id,seed.longitude
FROM town_catalogue_missing AS seed
JOIN jurisdiction_town AS town USING(jurisdiction_id,town_name);

INSERT INTO jurisdiction_town_elevation(jurisdiction_town_id,elevation_metres)
SELECT town.jurisdiction_town_id,seed.elevation_metres
FROM town_catalogue_missing AS seed
JOIN jurisdiction_town AS town USING(jurisdiction_id,town_name)
WHERE seed.elevation_metres<>'-9999';

INSERT INTO jurisdiction_town_timezone(jurisdiction_town_id,timezone_code)
SELECT town.jurisdiction_town_id,seed.timezone_code
FROM town_catalogue_missing AS seed
JOIN jurisdiction_town AS town USING(jurisdiction_id,town_name);

DROP TABLE town_catalogue_missing;
DROP TABLE jurisdiction_town_seed;
