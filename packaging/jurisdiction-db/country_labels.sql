-- Correct the political name without replacing jurisdictions, towns or saved references.
-- Great Britain excludes Northern Ireland. The Crown Dependencies keep their own
-- ISO jurisdictions; the geographical British Isles are not a political parent.
UPDATE jurisdiction_name SET name = 'United Kingdom' WHERE jurisdiction_id = 'GB';
