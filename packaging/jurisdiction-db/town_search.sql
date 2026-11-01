-- Worldwide name search; authoritative town and area names remain in their normalised tables.
CREATE VIRTUAL TABLE IF NOT EXISTS jurisdiction_town_search USING fts5(
    town_name,
    tokenize='unicode61 remove_diacritics 2',
    prefix='2 3 4'
);

DELETE FROM jurisdiction_town_search;
INSERT INTO jurisdiction_town_search(rowid, town_name)
SELECT jurisdiction_town_id, town_name FROM jurisdiction_town_name;

CREATE TRIGGER IF NOT EXISTS jurisdiction_town_search_insert
AFTER INSERT ON jurisdiction_town_name BEGIN
    INSERT OR REPLACE INTO jurisdiction_town_search(rowid, town_name)
    VALUES (new.jurisdiction_town_id, new.town_name);
END;

CREATE TRIGGER IF NOT EXISTS jurisdiction_town_search_update
AFTER UPDATE ON jurisdiction_town_name BEGIN
    DELETE FROM jurisdiction_town_search WHERE rowid=old.jurisdiction_town_id;
    INSERT INTO jurisdiction_town_search(rowid, town_name)
    VALUES (new.jurisdiction_town_id, new.town_name);
END;

CREATE TRIGGER IF NOT EXISTS jurisdiction_town_search_delete
AFTER DELETE ON jurisdiction_town_name BEGIN
    DELETE FROM jurisdiction_town_search WHERE rowid=old.jurisdiction_town_id;
END;

-- Stable integer keys provide direct FTS maintenance even when catalogue names or rowids change.
CREATE TABLE IF NOT EXISTS jurisdiction_area_search_key(
    id INTEGER PRIMARY KEY,
    jurisdiction_id TEXT NOT NULL UNIQUE
);
CREATE VIRTUAL TABLE IF NOT EXISTS jurisdiction_area_search USING fts5(
    name,
    jurisdiction_id UNINDEXED,
    tokenize='unicode61 remove_diacritics 2',
    prefix='2 3 4'
);
INSERT OR IGNORE INTO jurisdiction_area_search_key(jurisdiction_id)
SELECT jurisdiction_id FROM jurisdiction_name;
DELETE FROM jurisdiction_area_search;
INSERT INTO jurisdiction_area_search(rowid, name, jurisdiction_id)
SELECT key.id, names.name, names.jurisdiction_id
FROM jurisdiction_name AS names
JOIN jurisdiction_area_search_key AS key USING (jurisdiction_id);

CREATE TRIGGER IF NOT EXISTS jurisdiction_area_search_insert
AFTER INSERT ON jurisdiction_name BEGIN
    INSERT OR IGNORE INTO jurisdiction_area_search_key(jurisdiction_id) VALUES (new.jurisdiction_id);
    INSERT OR REPLACE INTO jurisdiction_area_search(rowid, name, jurisdiction_id)
    SELECT id, new.name, new.jurisdiction_id
    FROM jurisdiction_area_search_key WHERE jurisdiction_id=new.jurisdiction_id;
END;

CREATE TRIGGER IF NOT EXISTS jurisdiction_area_search_update
AFTER UPDATE ON jurisdiction_name BEGIN
    DELETE FROM jurisdiction_area_search
    WHERE rowid=(SELECT id FROM jurisdiction_area_search_key WHERE jurisdiction_id=old.jurisdiction_id);
    UPDATE jurisdiction_area_search_key SET jurisdiction_id=new.jurisdiction_id
    WHERE jurisdiction_id=old.jurisdiction_id;
    INSERT INTO jurisdiction_area_search(rowid, name, jurisdiction_id)
    SELECT id, new.name, new.jurisdiction_id
    FROM jurisdiction_area_search_key WHERE jurisdiction_id=new.jurisdiction_id;
END;

CREATE TRIGGER IF NOT EXISTS jurisdiction_area_search_delete
AFTER DELETE ON jurisdiction_name BEGIN
    DELETE FROM jurisdiction_area_search
    WHERE rowid=(SELECT id FROM jurisdiction_area_search_key WHERE jurisdiction_id=old.jurisdiction_id);
    DELETE FROM jurisdiction_area_search_key WHERE jurisdiction_id=old.jurisdiction_id;
END;

-- Named geographic places resolve to existing reference towns rather than becoming fictitious towns or regions.
CREATE TABLE IF NOT EXISTS jurisdiction_place_alias(
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    context TEXT NOT NULL DEFAULT '',
    jurisdiction_town_id INTEGER NOT NULL REFERENCES jurisdiction_town_entity(jurisdiction_town_id) ON DELETE CASCADE,
    UNIQUE(name, context, jurisdiction_town_id)
);
CREATE INDEX IF NOT EXISTS idx_jurisdiction_place_alias_town ON jurisdiction_place_alias(jurisdiction_town_id);
CREATE VIRTUAL TABLE IF NOT EXISTS jurisdiction_place_search USING fts5(
    name,
    search_text,
    jurisdiction_town_id UNINDEXED,
    tokenize='unicode61 remove_diacritics 2',
    prefix='2 3 4'
);
DELETE FROM jurisdiction_place_search;
INSERT INTO jurisdiction_place_search(rowid, name, search_text, jurisdiction_town_id)
SELECT id, name, name||' '||context, jurisdiction_town_id FROM jurisdiction_place_alias;

CREATE TRIGGER IF NOT EXISTS jurisdiction_place_search_insert
AFTER INSERT ON jurisdiction_place_alias BEGIN
    INSERT INTO jurisdiction_place_search(rowid, name, search_text, jurisdiction_town_id)
    VALUES (new.id, new.name, new.name||' '||new.context, new.jurisdiction_town_id);
END;

CREATE TRIGGER IF NOT EXISTS jurisdiction_place_search_update
AFTER UPDATE ON jurisdiction_place_alias BEGIN
    DELETE FROM jurisdiction_place_search WHERE rowid=old.id;
    INSERT INTO jurisdiction_place_search(rowid, name, search_text, jurisdiction_town_id)
    VALUES (new.id, new.name, new.name||' '||new.context, new.jurisdiction_town_id);
END;

CREATE TRIGGER IF NOT EXISTS jurisdiction_place_search_delete
AFTER DELETE ON jurisdiction_place_alias BEGIN
    DELETE FROM jurisdiction_place_search WHERE rowid=old.id;
END;

-- Island/town associations: official Azores guide, https://www.visitazores.com/storage/media/2022/03/feel-alive-en.pdf
-- Coordinates identify existing catalogue references; no new town, height, time zone or jurisdiction is invented.
-- UK references use coordinates already recorded in the packaged GeoNames town catalogue.
WITH seeds(name, context, code, latitude, longitude) AS (
    VALUES ('Pico', 'Azores, Portugal', 'PT-20', 38.53642, -28.5266),
           ('Faial', 'Azores, Portugal', 'PT-20', 38.5333, -28.6333),
           ('Terceira', 'Azores, Portugal', 'PT-20', 38.6590, -27.2208),
           ('São Miguel', 'Azores, Portugal', 'PT-20', 37.7412, -25.6756),
           ('Azores', 'Portugal', 'PT-20', 37.7412, -25.6756),
           ('Cornwall', 'England, Britain (UK)', 'GB-ENG', 50.41557, -5.07319),
           ('Isle of Wight', 'Isle of White, England, Britain (UK)', 'GB-ENG', 50.70146, -1.29124)
)
INSERT OR IGNORE INTO jurisdiction_place_alias(name, context, jurisdiction_town_id)
SELECT seed.name, seed.context, town.jurisdiction_town_id
FROM seeds AS seed
JOIN jurisdiction_town_jurisdiction_id AS town ON town.jurisdiction_id=seed.code
JOIN jurisdiction_town_latitude AS lat USING (jurisdiction_town_id)
JOIN jurisdiction_town_longitude AS lon USING (jurisdiction_town_id)
WHERE abs(CAST(lat.latitude AS REAL)-seed.latitude)<=0.000001
  AND abs(CAST(lon.longitude AS REAL)-seed.longitude)<=0.000001;
