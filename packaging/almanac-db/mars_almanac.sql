BEGIN TRANSACTION;

PRAGMA foreign_keys = ON;

DROP TABLE IF EXISTS almanac_nutation_term;
DROP TABLE IF EXISTS almanac_nutation_model;
DROP TABLE IF EXISTS almanac_orbital_elements_model;
DROP TABLE IF EXISTS almanac_lunar_correction_model;
DROP TABLE IF EXISTS almanac_lunar_fundamentals_model;
DROP TABLE IF EXISTS almanac_lunar_latitude_term;
DROP TABLE IF EXISTS almanac_lunar_longitude_radius_term;
DROP TABLE IF EXISTS almanac_lunar_fundamental_coeff;
DROP TABLE IF EXISTS almanac_keplerian_model;
DROP TABLE IF EXISTS almanac_frame_rotation_segment;
DROP TABLE IF EXISTS almanac_frame_rotation_series;
DROP TABLE IF EXISTS almanac_chebyshev_position_segment;
DROP TABLE IF EXISTS almanac_chebyshev_position_series;
DROP TABLE IF EXISTS almanac_frame;
DROP TABLE IF EXISTS almanac_body_ref;
DROP TABLE IF EXISTS almanac_fixed_equatorial_model;
DROP TABLE IF EXISTS almanac_body;

CREATE TABLE almanac_body (
    body_id INTEGER PRIMARY KEY CHECK (body_id >= 0 AND body_id <= 68),
    body_kind TEXT NOT NULL CHECK (body_kind IN ('star', 'planet', 'sun', 'moon')),
    model_kind TEXT NOT NULL CHECK (model_kind IN ('fixed_equatorial', 'chebyshev_position', 'lunar_chebyshev')),
    brightness_model TEXT NOT NULL DEFAULT 'none'
        CHECK (brightness_model IN ('none', 'catalogued', 'sun_distance', 'planetary_phase', 'lunar_phase')),
    magnitude_coeff_blob BLOB NOT NULL,
    sort_order INTEGER NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
);

CREATE TABLE almanac_fixed_equatorial_model (
    body_id INTEGER PRIMARY KEY NOT NULL REFERENCES almanac_body(body_id) ON DELETE CASCADE,
    coefficient_blob BLOB NOT NULL
);

CREATE TABLE almanac_body_ref (
    body_ref_id INTEGER PRIMARY KEY
);

CREATE TABLE almanac_frame (
    frame_id INTEGER PRIMARY KEY,
    frame_code TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);

CREATE TABLE almanac_chebyshev_position_series (
    series_id INTEGER PRIMARY KEY,
    body_ref_id INTEGER NOT NULL REFERENCES almanac_body_ref(body_ref_id),
    center_ref_id INTEGER NOT NULL REFERENCES almanac_body_ref(body_ref_id),
    frame_id INTEGER NOT NULL REFERENCES almanac_frame(frame_id),
    start_jd REAL NOT NULL,
    end_jd REAL NOT NULL,
    segment_span_days REAL NOT NULL CHECK (segment_span_days > 0.0),
    segment_count INTEGER NOT NULL CHECK (segment_count > 0),
    degree INTEGER NOT NULL CHECK (degree >= 1 AND degree <= 32),
    component_count INTEGER NOT NULL DEFAULT 3 CHECK (component_count = 3),
    CHECK (end_jd > start_jd)
);

CREATE TABLE almanac_chebyshev_position_segment (
    series_id INTEGER NOT NULL
        REFERENCES almanac_chebyshev_position_series(series_id)
        ON DELETE CASCADE,
    segment_index INTEGER NOT NULL CHECK (segment_index >= 0),
    coefficient_blob BLOB NOT NULL,
    PRIMARY KEY (series_id, segment_index)
);

CREATE TABLE almanac_frame_rotation_series (
    series_id INTEGER PRIMARY KEY,
    source_frame_id INTEGER NOT NULL REFERENCES almanac_frame(frame_id),
    target_frame_id INTEGER NOT NULL REFERENCES almanac_frame(frame_id),
    start_jd REAL NOT NULL,
    end_jd REAL NOT NULL,
    segment_span_days REAL NOT NULL CHECK (segment_span_days > 0.0),
    segment_count INTEGER NOT NULL CHECK (segment_count > 0),
    degree INTEGER NOT NULL CHECK (degree >= 1 AND degree <= 32),
    component_count INTEGER NOT NULL DEFAULT 9 CHECK (component_count = 9),
    CHECK (end_jd > start_jd)
);

CREATE TABLE almanac_frame_rotation_segment (
    series_id INTEGER NOT NULL
        REFERENCES almanac_frame_rotation_series(series_id)
        ON DELETE CASCADE,
    segment_index INTEGER NOT NULL CHECK (segment_index >= 0),
    coefficient_blob BLOB NOT NULL,
    PRIMARY KEY (series_id, segment_index)
);

CREATE TABLE almanac_nutation_term (
    term_id INTEGER PRIMARY KEY,
    multiplier_L INTEGER NOT NULL DEFAULT 0,
    multiplier_Lprime INTEGER NOT NULL DEFAULT 0,
    multiplier_omega INTEGER NOT NULL DEFAULT 0,
    sin_coeff_arcsec REAL NOT NULL DEFAULT 0.0,
    cos_coeff_arcsec REAL NOT NULL DEFAULT 0.0,
    sort_order INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE almanac_nutation_model (
    model_id INTEGER PRIMARY KEY,
    coefficient_blob BLOB NOT NULL,
    sort_order INTEGER NOT NULL
);

CREATE INDEX idx_almanac_chebyshev_position_series_lookup
    ON almanac_chebyshev_position_series(body_ref_id, frame_id, start_jd, end_jd);

CREATE INDEX idx_almanac_frame_rotation_series_lookup
    ON almanac_frame_rotation_series(source_frame_id, target_frame_id, start_jd, end_jd);

INSERT INTO almanac_frame (frame_id, frame_code, display_name)
VALUES
    (1, 'ECLIPJ2000', 'J2000 ecliptic frame'),
    (2, 'TRUE_EQUATOR_DATE', 'True equator and equinox of date');

INSERT INTO almanac_body_ref (body_ref_id)
VALUES
    (1),
    (2),
    (3),
    (4),
    (5),
    (6),
    (7),
    (8),
    (9);

INSERT INTO almanac_body (body_id, body_kind, model_kind, brightness_model, magnitude_coeff_blob, sort_order, enabled)
VALUES
    (1, 'sun', 'chebyshev_position', 'sun_distance', X'3d0ad7a370bd3ac00000000000000000000000000000000000000000000000000000000000000000', 10, 1),
    (2, 'moon', 'lunar_chebyshev', 'lunar_phase', X'f6285c8fc27529c039b4c876be9f9a3f0000000000000000000000000000000095d626e80b2e313e', 15, 1),
    (3, 'planet', 'chebyshev_position', 'planetary_phase', X'e17a14ae47e1dabfdbf97e6abc74a33f540262122ee431bf8dedb5a0f7c6c03e0000000000000000', 20, 1),
    (4, 'planet', 'chebyshev_position', 'planetary_phase', X'9a999999999911c092cb7f48bf7d4d3f8eb1135e82532f3f6b4eb91d75cfa5be0000000000000000', 30, 1),
    (5, 'planet', 'chebyshev_position', 'planetary_phase', X'52b81e85eb51f8bffca9f1d24d62903f000000000000000000000000000000000000000000000000', 40, 1),
    (6, 'planet', 'chebyshev_position', 'planetary_phase', X'cdcccccccccc22c07b14ae47e17a743f000000000000000000000000000000000000000000000000', 50, 1),
    (7, 'planet', 'chebyshev_position', 'planetary_phase', X'c3f5285c8fc221c0ba490c022b87a63f000000000000000000000000000000000000000000000000', 60, 1),
    (10, 'star', 'fixed_equatorial', 'catalogued', X'0ad7a3703d0a07400000000000000000000000000000000000000000000000000000000000000000', 100, 1),
    (11, 'star', 'fixed_equatorial', 'catalogued', X'713d0ad7a370dd3f0000000000000000000000000000000000000000000000000000000000000000', 110, 1),
    (12, 'star', 'fixed_equatorial', 'catalogued', X'52b81e85eb51e83f0000000000000000000000000000000000000000000000000000000000000000', 120, 1),
    (13, 'star', 'fixed_equatorial', 'catalogued', X'000000000000f83f0000000000000000000000000000000000000000000000000000000000000000', 130, 1),
    (14, 'star', 'fixed_equatorial', 'catalogued', X'5c8fc2f5285cfb3f0000000000000000000000000000000000000000000000000000000000000000', 140, 1),
    (15, 'star', 'fixed_equatorial', 'catalogued', X'85eb51b81e85eb3f0000000000000000000000000000000000000000000000000000000000000000', 150, 1),
    (16, 'star', 'fixed_equatorial', 'catalogued', X'52b81e85eb51fc3f0000000000000000000000000000000000000000000000000000000000000000', 160, 1),
    (17, 'star', 'fixed_equatorial', 'catalogued', X'c3f5285c8fc2fd3f0000000000000000000000000000000000000000000000000000000000000000', 170, 1),
    (18, 'star', 'fixed_equatorial', 'catalogued', X'0ad7a3703d0afb3f0000000000000000000000000000000000000000000000000000000000000000', 180, 1),
    (19, 'star', 'fixed_equatorial', 'catalogued', X'85eb51b81e85ff3f0000000000000000000000000000000000000000000000000000000000000000', 190, 1),
    (20, 'star', 'fixed_equatorial', 'catalogued', X'ec51b81e85eb01400000000000000000000000000000000000000000000000000000000000000000', 200, 1),
    (21, 'star', 'fixed_equatorial', 'catalogued', X'7b14ae47e17a00400000000000000000000000000000000000000000000000000000000000000000', 210, 1),
    (22, 'star', 'fixed_equatorial', 'catalogued', X'52b81e85eb51e83f0000000000000000000000000000000000000000000000000000000000000000', 220, 1),
    (23, 'star', 'fixed_equatorial', 'catalogued', X'0ad7a3703d0a03400000000000000000000000000000000000000000000000000000000000000000', 230, 1),
    (24, 'star', 'fixed_equatorial', 'catalogued', X'1f85eb51b81eed3f0000000000000000000000000000000000000000000000000000000000000000', 240, 1),
    (25, 'star', 'fixed_equatorial', 'catalogued', X'9a9999999999a9bf0000000000000000000000000000000000000000000000000000000000000000', 250, 1),
    (26, 'star', 'fixed_equatorial', 'catalogued', X'14ae47e17a14fe3f0000000000000000000000000000000000000000000000000000000000000000', 260, 1),
    (27, 'star', 'fixed_equatorial', 'catalogued', X'c3f5285c8fc2fd3f0000000000000000000000000000000000000000000000000000000000000000', 270, 1),
    (28, 'star', 'fixed_equatorial', 'catalogued', X'3d0ad7a3703dfa3f0000000000000000000000000000000000000000000000000000000000000000', 280, 1),
    (29, 'star', 'fixed_equatorial', 'catalogued', X'e17a14ae47e1da3f0000000000000000000000000000000000000000000000000000000000000000', 290, 1),
    (30, 'star', 'fixed_equatorial', 'catalogued', X'ae47e17a14aee7bf0000000000000000000000000000000000000000000000000000000000000000', 300, 1),
    (31, 'star', 'fixed_equatorial', 'catalogued', X'7b14ae47e17ab43f0000000000000000000000000000000000000000000000000000000000000000', 310, 1),
    (32, 'star', 'fixed_equatorial', 'catalogued', X'000000000000f43f0000000000000000000000000000000000000000000000000000000000000000', 320, 1),
    (33, 'star', 'fixed_equatorial', 'catalogued', X'0ad7a3703d0a01400000000000000000000000000000000000000000000000000000000000000000', 330, 1),
    (34, 'star', 'fixed_equatorial', 'catalogued', X'14ae47e17a1400400000000000000000000000000000000000000000000000000000000000000000', 340, 1),
    (35, 'star', 'fixed_equatorial', 'catalogued', X'a4703d0ad7a3fc3f0000000000000000000000000000000000000000000000000000000000000000', 350, 1),
    (36, 'star', 'fixed_equatorial', 'catalogued', X'666666666666fa3f0000000000000000000000000000000000000000000000000000000000000000', 360, 1),
    (37, 'star', 'fixed_equatorial', 'catalogued', X'd7a3703d0ad701400000000000000000000000000000000000000000000000000000000000000000', 370, 1),
    (38, 'star', 'fixed_equatorial', 'catalogued', X'1f85eb51b81e03400000000000000000000000000000000000000000000000000000000000000000', 380, 1),
    (39, 'star', 'fixed_equatorial', 'catalogued', X'8fc2f5285c8ff23f0000000000000000000000000000000000000000000000000000000000000000', 390, 1),
    (40, 'star', 'fixed_equatorial', 'catalogued', X'3d0ad7a3703dfa3f0000000000000000000000000000000000000000000000000000000000000000', 400, 1),
    (41, 'star', 'fixed_equatorial', 'catalogued', X'a4703d0ad7a304400000000000000000000000000000000000000000000000000000000000000000', 410, 1),
    (42, 'star', 'fixed_equatorial', 'catalogued', X'8fc2f5285c8fe23f0000000000000000000000000000000000000000000000000000000000000000', 420, 1),
    (43, 'star', 'fixed_equatorial', 'catalogued', X'14ae47e17a1400400000000000000000000000000000000000000000000000000000000000000000', 430, 1),
    (44, 'star', 'fixed_equatorial', 'catalogued', X'f6285c8fc2f5fc3f0000000000000000000000000000000000000000000000000000000000000000', 440, 1),
    (45, 'star', 'fixed_equatorial', 'catalogued', X'a4703d0ad7a300400000000000000000000000000000000000000000000000000000000000000000', 450, 1),
    (46, 'star', 'fixed_equatorial', 'catalogued', X'd7a3703d0ad703400000000000000000000000000000000000000000000000000000000000000000', 460, 1),
    (47, 'star', 'fixed_equatorial', 'catalogued', X'3d0ad7a3703d04400000000000000000000000000000000000000000000000000000000000000000', 470, 1),
    (48, 'star', 'fixed_equatorial', 'catalogued', X'66666666666600400000000000000000000000000000000000000000000000000000000000000000', 480, 1),
    (49, 'star', 'fixed_equatorial', 'catalogued', X'0ad7a3703d0afb3f0000000000000000000000000000000000000000000000000000000000000000', 490, 1),
    (50, 'star', 'fixed_equatorial', 'catalogued', X'a4703d0ad7a3fc3f0000000000000000000000000000000000000000000000000000000000000000', 500, 1),
    (51, 'star', 'fixed_equatorial', 'catalogued', X'8fc2f5285c8f00400000000000000000000000000000000000000000000000000000000000000000', 510, 1),
    (52, 'star', 'fixed_equatorial', 'catalogued', X'b81e85eb51b8fe3f0000000000000000000000000000000000000000000000000000000000000000', 520, 1),
    (53, 'star', 'fixed_equatorial', 'catalogued', X'295c8fc2f52800400000000000000000000000000000000000000000000000000000000000000000', 530, 1),
    (54, 'star', 'fixed_equatorial', 'catalogued', X'3d0ad7a3703df23f0000000000000000000000000000000000000000000000000000000000000000', 540, 1),
    (55, 'star', 'fixed_equatorial', 'catalogued', X'ae47e17a14aed73f0000000000000000000000000000000000000000000000000000000000000000', 550, 1),
    (56, 'star', 'fixed_equatorial', 'catalogued', X'8fc2f5285c8f00400000000000000000000000000000000000000000000000000000000000000000', 560, 1),
    (57, 'star', 'fixed_equatorial', 'catalogued', X'666666666666f63f0000000000000000000000000000000000000000000000000000000000000000', 570, 1),
    (58, 'star', 'fixed_equatorial', 'catalogued', X'a4703d0ad7a3c03f0000000000000000000000000000000000000000000000000000000000000000', 580, 1),
    (59, 'star', 'fixed_equatorial', 'catalogued', X'7b14ae47e17a843f0000000000000000000000000000000000000000000000000000000000000000', 590, 1),
    (60, 'star', 'fixed_equatorial', 'catalogued', X'5c8fc2f5285c03400000000000000000000000000000000000000000000000000000000000000000', 600, 1),
    (61, 'star', 'fixed_equatorial', 'catalogued', X'd7a3703d0ad701400000000000000000000000000000000000000000000000000000000000000000', 610, 1),
    (62, 'star', 'fixed_equatorial', 'catalogued', X'14ae47e17a14fa3f0000000000000000000000000000000000000000000000000000000000000000', 620, 1),
    (63, 'star', 'fixed_equatorial', 'catalogued', X'5c8fc2f5285cf7bf0000000000000000000000000000000000000000000000000000000000000000', 630, 1),
    (64, 'star', 'fixed_equatorial', 'catalogued', X'0ad7a3703d0aef3f0000000000000000000000000000000000000000000000000000000000000000', 640, 1),
    (65, 'star', 'fixed_equatorial', 'catalogued', X'ae47e17a14ae01400000000000000000000000000000000000000000000000000000000000000000', 650, 1),
    (66, 'star', 'fixed_equatorial', 'catalogued', X'b81e85eb51b89e3f0000000000000000000000000000000000000000000000000000000000000000', 660, 1),
    (67, 'star', 'fixed_equatorial', 'catalogued', X'00000000000006400000000000000000000000000000000000000000000000000000000000000000', 670, 1),
    (68, 'star', 'fixed_equatorial', 'catalogued', X'ae47e17a14ae15400000000000000000000000000000000000000000000000000000000000000000', 680, 1);

INSERT INTO almanac_nutation_term
    (term_id, multiplier_L, multiplier_Lprime, multiplier_omega, sin_coeff_arcsec, cos_coeff_arcsec, sort_order)
VALUES
    (1, 0, 0, 1, -17.20,  9.20, 10),
    (2, 2, 0, 0,  -1.32,  0.57, 20),
    (3, 0, 2, 0,  -0.23,  0.10, 30),
    (4, 0, 0, 2,   0.21, -0.09, 40);

-- Generated by tools/generate_almanac_nutation_sql.py
-- Max sampled dpsi residual: 1.781709 arcsec
-- Max sampled deps residual: 0.769844 arcsec
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (1, X'cf040e4a9f4707bfa8b9c7c4b2e50abf50fb71eff06ee43eca1a924aeb5a8abed6668d68cea5813e7a66bbefb7efb5be3b7d36bcec45953e46bd1fb2fbd9a3be70f9a3c473f8fdbe9a3bf3a98c56f73e910e78a1421ad73ec97d5cb21db8b0bebe6eebe464b99cbe7d6b429b677689bef90f086af71f96be1c8f09ceabdb7cbe', 10);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (2, X'd69a12fd2b9708bfaf3bb087085c073f7173ae422661e33ef5b2645143f9c9be67fc3e89f0b2873ee8b17998e1beb4be140280538f02933edb2844a2d4bca3bec2d22d2d7a07fc3e4a98f0067c05f83e0dd6579e72dcd7be764abfd20411aebea56fb52f08378ebe4fe5d88f5ac086beb42941735f6694be278f97bcc8b779be', 20);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (3, X'9bdb9061bdce0c3fab1143197b4e033f306fa9f9b5ace5bef31c50d81d3ac8be81461099193a9d3e8a0ff1bee085b4bebe75be8014f9933e2cab06f31026a2be302b6876a628f73e50736853dfa7fcbeade21b9c102dd4bec61ed270e6a3a23e061aef2576588fbefd83dcdc8a198bbe49ddb3fde10b95be8c090c99aaaf7fbe', 30);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (4, X'f635c6ef1e57023f6be886af335d0ebf5298c002f32adebeb59be726b534723e38e905ba15de9f3e0177e602bdc7b5befde47d4cdc30963ef4d78be728e3a3bea89a851492e800bf4c2630b9f5cef2bef7fe7773497bda3ed3c63049b3a6963ef4b1bbe385b79dbe4c4159576c218cbef1d87d8db9d095be28224b2d17047ebe', 40);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (5, X'4822f8417da210bf9d97e85665bffbbee92db9879035ec3ef822b5c0f2eeb0bebc2980fec65a733e2358319e2c91b5bea487b0b43be6933e3c76efc34031a4be7b5d29f5d7c6eebe596d3e90269e003fee15d02b660ec53e143910bab31eb5be5c5bded4fb3e97be5fc95342f52386be33fbe2b9531b95bef3e5ccf22a407abe', 50);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (6, X'efee7ccf1553f4be0a73e25199b10e3fa3e282f2289cd03e717b2880bae4ccbea3ffd43ff203923e66067208029bb4be69d7c6634f87943edd3fdf9dc445a2beef7ca6c5f275023fd7f66536058ae33ed219901f2e3cdebeb13e111fbcb2a0be78eb649fed928bbe8c6a37c3e67989beaa91972ecd6394be8a480980a97a7dbe', 60);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (7, X'e4a758b612cb113f2ed9428a1e7ae53ecc9369f86bc9ebbe463bfbf36383c2be82ee84ca8076a23e8023b6bf4254b5be6f62989de5b4963e525f5c34d935a4bec54decb8d2efda3eac69b2f6e0d101bf6a76fa44e93bbebe5d62e10d4d87a93e27666139f0f792be0913eab68cc88cbe5f998302da2c95bed8ac81c2f6497fbe', 70);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (8, X'8fa5f01c67f8d03e6c8445519b9011bfb079f8e01507a3be53b0c3c572479d3e3beb504f0c64943ee4433f8bc218b6be50b561a488f0933ec83985c57219a4be675d99b9108903bf2c87c80130c6c2be3e65ce40dd1ddf3ee193aa6fb85c84beab953fa3d1c79ebe1238015ef8f888be148a8bf97dcd95be95581891ca7d7cbe', 80);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (9, X'298551fc02fe11bf64d2dd6211d9ca3efa1d30d07794ed3eb9f9117e3681c0be3cef1bff2924803e75a7ecc5e9acb4be0c6d6fcb3f0a953e04315eaaf352a2be518246089d6ac33e5b304eb578d4013f3e3ef64305d8afbedddb85eba3ceb5be3d525653d95d93bea4027d8aa98587be9032eb33d5ed94be5386afd80a467bbe', 90);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (10, X'761613eb4195e93e8995eff3be840f3f56de1dd1b46bc0be861fb1a2abe9ccbe67687ae2634f993ee3edd1de65a0b4be418208ec9824973e4fe66d672d91a3be5bd0ac51b4f2023f32dae48f2ff5d9be4aba98429d15dfbe1146a1f829f378bef1b96b5aa13e8bbeaeeb01b577f28bbe5d23fc78c95c94be914493c471607fbe', 100);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (11, X'679925ae1662113fc8eb67ce5184f4be5ba75c1fa307ecbe6fff4dd49ec1b5bec77dfa62949ba13eb6dac5ff4388b5be8f9bd9effa3b943e95b045fbb7bca3bef9277c308a85e6beb150eef79b8801bf756a066db854bc3ee5a0a56e05f2ab3e529af647810898be283c479434ea8abe751e4a28e30796beef066f2fb34f7ebe', 110);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (12, X'36d19d1cb326fdbeb5c0c620951310bfea23bc31f530da3e5190a05011a4923ed231a7481c9b8d3ed057b34cba47b5be8250df4b74a3953e20137fdcb881a2be2470202100e901bfc46e6b70021ced3ee61be16f4746dc3e2c704a98e877a8befaf472cef71c9dbe9c535526460488be555670060ba095be15ece1e517b67abe', 120);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (13, X'9b0abc6710f00ebf2c4d393f2349003f21026e5673e4e83ebba7ea42d7f9c6beb9160e1b87ce893e9768ef05f377b4be0d6dc687450b973e0bfe50379e74a3be605fbd5aa48df33e3738ec5da363fe3ea15cf8cb117dd1be0020961e8182b2be27b62a2cb03f90be23415547d2b28abe304e48b5786b94bef36fef43f8cd7ebe', 130);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (14, X'b7fb548948a1053f25bc22b963a6093ff409b30dc5b9dfbee13f47837889cbbe417af1fd661e9d3e31926389e978b5be9e769d261cba953e63161a0248aca3be3d25c1fa11cffe3e306065deef7cf5bee08b7aaed7eed9bee52d81331cd5953e9306a0273d5d90be90d298d0de078abec2a2585e65d795be50afacc19cd47ebe', 140);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (15, X'6769534ee5630a3f878f6aa3202908bf90f47e678fc5e5bea1dbcf00971e93be5a7217e71383a03e3975b6810655b5be3d7e0f452ef9943efed509068429a3be655ffe23a1f5fabe4577f241d9dafabee951e4cf806cd43e9bd61e309774a43eecfc5289f28d9abe1ed7d969a1ab8abe9f79d0e62f9995be45173a9f9fb27abe', 150);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (16, X'c6268e5767010cbf1388443baa4006bf463b4f642c58e83eb3706f26c1a99dbe7bc2eb348bd4813e7f28ef8d9ebfb4becc377ff829c0953ed92346346c29a3be90db9b132ad0f8beb65042f90c08fc3e732e846f13a4d23e77985ba0122fb3be5a3f803f9b939abe2eaf39579df889bef2fbf74f720095bea8fce819c2f47fbe', 160);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (17, X'8d850fa9965903bfbaeae8445dfd0a3f75ca14693ccbde3e510a565ceaabcbbe11b6a6571815923ec8858047663eb5be29ac18be1e54963eef9444452708a3be9ce2358ec232003f28a291813fcbf23eccd62054b61fdbbe4967d9e5e6f8a9be7d902d5e043a8fbe0559eeac17008abe1eddc51a62f895be6ede8693d1447ebe', 170);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (18, X'dfeda94e802c103fe2b1c158a36dfc3ee1024a914dc3e8be8f3e0cf60c0dc6bee1f15a82694a9e3e79ee07e0be99b4be91a37d431212953e429fc9deb46da3be2b530ac5ce15f13e5ec80719501c00bf9dc2a0ad4c44cfbe48905db125a1a63e7fbbcfc5472291bec48fadde46d58abef59fc75ca1d494be30c5084afc727bbe', 180);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (19, X'2a92d6321c76f83eac32a765298f10bfdafdfa3310c8d3bea682323b25e5963ef07aadee77ae9c3e5b3827ef39bfb4be1039ed9b5967943ea85d99d9ca50a2beba6439267e6f02bf0c7e9438ab42e9bed2c4e3ab423cdd3e8df67794a5b2823e2d5ae4c49e199ebeab181d8b0a9c8bbee03d36f2175d95be1dcbb217bf8280be', 190);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (20, X'f9fb35f5bba511bf383fb545085aefbe340e1dc96f9eed3e9d3c7c6fba76b7bed916257c424d7d3e1d0ef92a5693b5be2bc76f583204973e25662782c54da3be92527104490de1be931abd0ffd8d013f5a46291af814b33e5a48823f8469b6becd0c88cf2a4b96bec76dd4e9b2938abe666efc5d273296be4f4231ce084e7ebe', 200);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (21, X'27c47eaac379ddbef23fd42a2ada0f3fa9719128922cbb3e3e418a83813ecdbe49d95ad192a7933ea9f37cce467bb4be63a5c2982a03953e07f37bc9a99ca3be9e0c823c8725033f6566e81b9c3bcb3e3d43ae902459dfbeaf84e996273896be441569d716918dbeea510e1834de89bebf9554784c4994bea1d96226dfa77cbe', 210);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (22, X'91e5d834f91c123fb4d69a398910b8bec55a19fa9cb3ecbe8156fcd3b627bebeb8322c681c6ca23e27f236975b9bb4bee3ac55586d85943ecebdd2999d2fa2be1671129bd7cb99be9ba866f87c2f02bf2711c46d79819cbeed1646796f12ab3ebe694c9318ef93be2522160573028cbe42f4deb5b26695be62a83da7614580be', 220);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (23, X'd2d35beee48ee2be4d8b0e5bf67211bf22d6ea0939a5c23e2ee8e60ce8f19d3ea1c530b96212923ee8fddafcf5cab5bee1465d114ad4963e86cfef68ba8da3bec4a8f1bc6b6703bf63cff587a4e1d13e4ee55b9e2ee3de3ea6503dcfe2f49bbe5a97dba928849ebe47a13bcdb8928dbe935d5fee89c795be5a9d972cc69e7fbe', 230);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (24, X'8621250e137511bff3d4115e44fcee3ee6e43e389d5cec3ef3b59b0c9d76c3be9d969ea1e2aa813e9a51576129b8b4be7487347caae3933ebc2c1b55debfa3be653eaf974afbe23e5d9c91f8b739013f326263066a61c3bece8cb84564f5b4be7d24c06984e492be2dd33ce34a2388beddc5ae78dab894be7573d63bca9b7dbe', 240);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (25, X'b7b3e21d918bf93eca2e3ae607040e3fb9eacde97708d2befc094ef34b2accbe53f9dcd2ad7b993ec6d91b5ae454b4be5cbce4cd97a8953eec2575ca43a4a1be2443daec0603023fa880713b9480e9bebc683750a4abddbe72f426bd3664743efef71041b6d28abe1d43086e017e8bbe1577d661bcf194be353cdc5225ed7ebe', 250);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (26, X'1a3c9f252629103f5fc0dbf97f2600bf99dff885f54ceabe2910e31d2cc6acbe3b21566a8bf0a13ed78d69f53eceb4be08330da204c2963e4f321216836fa3bebaf556b7aff3f1be5fbc88b8705d00bf57c6ce7a8c82c93e3e92187db5bda83ec717259044ca98be2335b110dde48fbeb72a21aff3e794be20e9534401d880be', 260);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (27, X'cfe889304f7b04bf9733b8579dff0cbfb0584822fe17e23ee97a3ec4c441523ed7a0395dd451883e71f9fdb969aeb5be2d6d2d66eb24943edfa06718fdb3a3be42e66df2272600bfecc628c51b7cf43eae25ec466721d93e3c992bcfe915afbe922f1399ac639cbebf2d670af35688be88d2d80f6e8095bea54035229fbf7ebe', 270);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (28, X'c8db4208d00c0bbf995c10cc4927053fda4b4977ec87e53ec1c1b792b0eac8be436979181f8a893e401dea0b0c5eb4beaca7cd0460a4963eb104e382c302a2be318783294e56f93e0339ae0d8771fa3ef39c1864f8c4d5bef234199b9f72b0be18abc885302b8dbe1244183525098bbebd1145dbba9294be265b969cf4f17cbe', 280);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (29, X'0bcedcc3f4960a3f7c8a748db4bb053f34bd6e06aecbe3be90c218a40077c9beed50b3c2dc12a03e1c0faf95f27db4bef45ad5c5684c973ef6c46bc25cb4a3be68738220c50cfa3e485afe0abc70fabe1c26a9f50c5cd6be8ccd59bdc1319d3e85b399f055c68fbe1d55da0144558ebeea237aad721f94be3831af2c3ca381be', 290);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (30, X'8d6d2334395a053f6d6d5445a77f0cbfc0255e7e4999e1be62ef178b1dc25bbe2a8125213102a03ea26876f8c320b6bed47f28351369943e05df9a48f69fa3bee85c7bf39bbcffbe6e6e09d9abd2f5be4297ebae4d9ad83e0a6f2f8707119d3e9e75cc1ed7089cbe3113426ebf818cbe6232061fcf1396bee501158eeb3780be', 300);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (31, X'8ef13676d5c50fbf2459e97bc1ea00bf0b0bd1682b30eb3eacb5238037dbaabed501003012787a3ecdff023243b0b4bec85e3407e6d0963e9f530d0036aaa2bee8e6e9c652cff2bedfa0f0c209b9ff3ec147f6cd171ecb3efb00440586ddb4be2dd32487352598bebfed1b9fb0fd89bed5cc1ab29aab94be5ab46beb5aeb7bbe', 310);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (32, X'989c38281af0fabe05a9a417ffba0d3fe1180656c8d1d53e39a916b9822cccbe93885aa90e51953e0300b497ab4bb4be23675c4c52cc963e2c774c5d7e24a3be907f78f631d6013f447c91bd7befe93e9e3f41e3326cddbea106d3084150a5be7f091ef9ba798bbef9705fab84668cbe65cdaed257db93be0f2b48563f9081be', 320);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (33, X'8b4555e90369113f88be99cc7215f13e20d325afe1f6eabeed507ec50cf1c3be1242c0fbdee3a03e181b2d09756fb5be534539f1dfe4953ef56aaedc5c32a3be7376f414d9d7e43eba1043d09d6301bf45b5d208adebc4bea4755ae687a8a83ed0b6c647268892be6b54fa81cd388ebeb947a9e76fec95be202c6a54537680be', 330);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (34, X'17c7b2caeab2e63e7de17e99f36211bf4a5cf716a43cc1befd724cb9c7909e3e90158de62eb5983e49f5c6a3cadab4be10d500ef3ff6953e0945f8e554f0a2befb0e119a9d5403bfbb94d46990f2d7be13cc7ed684d5de3e33b211b7d0e373be49de5e44df8b9ebe5c6a9199e94e8bbe5df609d85aef94be28b1cd7579e87cbe', 340);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (35, X'a8c9f764520c12bf6e3c2ff8224bc9be0da420113406ee3ef9c0e597b1a8bcbeb034ef0d57d1843e52d08dd88f72b4bec92c9346045d973e946a5328dd97a2be8b55d0b4060ab6be5d4a8dda7de3013f79b8e3dce53d8fbe18947637c37cb6be94d3a63eeefc92bec45c8806b5ad8abe48255ca07b5894be24e904b4ddd080be', 350);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (36, X'c3d9ef442880d73e71b3f8cce7ed0f3f3ec4f4a795b1a7be9aec41d387a9cdbe603f3b491f5e983eaa19c7d8523fb5bec796e8f51b23983e2f7bcdd2f756a3be749afd8f4631033ffdd8518d82d8c8be20cf3aade667dfbe39db8c3935b189beb1057eed9ff88dbe079d6e5c37d18dbe134465bdae4d95bee95c106d803e80be', 360);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (37, X'fd22f0cbf1cf113f9c0670302e00ecbed478df72d8a4ecbe899df82f271bb8be6ecece412933a23e48090664a3deb4be96e5fde7f0b2943e20f133b137b8a3be5abbfac45244debe09e33e3482ee01bf300dff7a0d09b03edd400e13bd5bab3e928433ac04f395be9863e9bd48c48bbe3109a1aefbee94bedee71a7f100f7ebe', 370);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (38, X'bf7a1e537d76f6be2fe18c7ae8b610bf2917844761d9d43e1d785e86ff7a9a3ebd79d7dd21df903eab2cbdb34a0ab5be1f3442e908e3963eb2e0c45d2e48a2be10a12da9fa9802bf45a63057f34ee63e32c390d5ff86dd3e3b49cb363833a5be5af0eddb5be49cbead1b212795528cbee8af6218811c95be5da6ee849fbd80be', 380);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (39, X'f4df8e0ced5110bf3e57ffab15f2fa3e53f87a52d15eea3e05f9c6bae72fc6be54cca49442538d3e8d3ccc429cf5b4be4ca829b45603993e6c0da28ea053a3be4b8f7f810439f03e640c8e6e4906003f78dc34ed28cecdbe5c0ce8886315b4bee0424976a7ae91beba83430bb3a68cbe8be08156aa0d95be9c17bbc96bc77fbe', 390);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (40, X'c7d556da51bd023fd147d55761760b3f464baa76d94edbbe9aebb1403070cbbec97441c30a4a9a3e00184ab9eda7b4be68e20e1bd276943e72aa9639c9ada3bee55a1aa04f74003f244c86c6329ef2bef9e7507ceb7edbbe992a398ad134903e27b6837457cf8cbee6ff78fe0aec8abe32cca2d7db8494bec164a49c721c7fbe', 400);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (41, X'757fc000fbbf0c3fe053086d429205bf0a4f2f4b3892e7bec7168fce75b39ebe7cad3f890d27a13e3257e60f8977b4be1f0bb54751ab953ebb416699d4afa1bec8a22ea33e0bf8be13c58588fd30fdbe4ed50e502e06d23e92e5743fa8cfa53ee6e70bb3e0ec99bed3f5ee3022978ebe3674901eda4295be8d7ed6a35d9880be', 410);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (42, X'1b16b56156a909bffdde3cc7fcd208bf0423dc91c778e63eee8e79bd387890beb01d59c1b8c68b3e436fbf177015b5bed1dcaf598725993e32005d060609a3be13be907af3a9fbbefb4811b769a1f93e4dd938fe6b12d53e211f20a465cbb2bec5f76513340f9bbe91b03264fb928cbe4986f11e495695be3c11ead0b16380be', 420);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (43, X'1d6f493ea23b06bfd36da07eda3c093ff38320e515b7e13ec1b7246796a4cabe33bb9a10b1718f3e89951f8aa09eb4bed8fdf66499fb953eaec9e426707ca3bed490b5827539fe3ef41a58b748a2f53e7f5e30a9e87dd9be2fc7c8c6672dacbe3af04c977e6f8dbe5de2ce44dbe98abedfc208390b7994becbda20394fd57fbe', 430);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (44, X'e2917be843a70e3fe158c2bd5506013f29ff8539544be7bec21dda4b0544c7be489d50d440859f3e8b136858001bb4be89b5a4efc722963ea78a1b00d7cda1bea397e049f664f43e2102bca2a280febe1a504340f90ad2bed4670a5e8f48a33e55e7fd5f316490be165609e412f38cbeb2b6117dbbc994be9284695f65797fbe', 440);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (45, X'519f0635d61eff3e7690cf4065d40fbfef3a51140657d9be4a0a0ff83f9b923e91f69f644dd29f3e1e9d5d538039b5be8a8786962774973e3e0483e82a27a3bec8b5af997bb601bfd30e0b643400f0be1affb66f81dddb3e26eeab433d598d3e86aaa201b3c69cbea708b0c981c18fbe25c99d32bb2d95be4cfdfa9e198d81be', 450);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (46, X'468c6506683511bf0f5392374928f6beafc59133b32ced3e6eb3258c703db4be7b20241f1741803ec41a3a59260eb5bed3343edfbca5963e575a3e5ab286a3be56e5656dbc63e8beae5985d66d20013faa54746d119dbf3e5812f1d08404b6beafa90578925b97befecfa08e298e8abe7fef02d3283795be2cafb523ea3180be', 460);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (47, X'fdb14848bb9cecbef61c1369e5740f3fdb2e45fc630cc83eb6e17e58d0acccbe0fec37387441953e37f8d8fa0315b4be7426a42c1829973e28996a7679d6a1beb8bec97e0bde023fc46ab02ffb07db3efe655cbd3cdddebe356134cc130b9fbec8eb9aa552918abe76c1439d50c88bbecbf9b472383c94be4e8a8e4d27f17dbe', 470);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (48, X'31859ee1470e123f40b4187c265bd43ed4d739a28257ecbe69bf6145ce5dc0be3628801626bca13e24fa08ca1639b4be13c438d97f30963e724d6d070bcca2beaa8bb0067c15cb3ec0c9de3ca01502bff55c56d20961b3be8a635a6ec0fea93e924ab21c49cc92beb286ef27c26190bee570dca5895894be25740bcdd76b82be', 480);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (49, X'4b306ee88426c1beb97c98ec9c9c11bf8543908938c3a93efb177b5b46499e3e13152efcda2f973e654f34280e72b5beb8575751f864963e5fff9d990b0ea3be944c8ea0c98e03bf6cc57cc8fdd7aa3e666bbbb0e635df3e9da81b42a01995be7261e6f5913b9fbe62fdaad17ab08cbe5801bc770dcb95be9c671a247fdf80be', 490);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (50, X'0002827ad2d711bf7e418c860b6fe23e00de2f9b973bed3eb30fbe88f74ac1be1bd4f503c27e843e16546de00e49b4beb846aa4bcc74983e73d0290adc39a2bec2ce4753a320d73efdfd0fc1b59b013f23dae3cb20e7babee4555f724cefb5be0ad57d2a7ea891be2df18851f13b8bbea9916153237694be447f4ae63e3e7dbe', 500);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (51, X'b97f732064eef23ef97fb7f0d9020f3f4ed677ba2093c9be2d33f2cc1089ccbef10818006a129a3ecf83f2b219deb3be1901b9655d12973eab9cdc0bd786a2bea928f046af97023fc4a99e84c406e3be3f43ceb9d679debeca14f39fd99363be776cb53dc9918bbe3b6ef52e8d7c8fbe390794efd0a193be34b14fc49be282be', 510);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (52, X'8b149678b4e7103f87324a548d32fabefacdb91c6d6bebbe2ada42d3bf03b2beaba4bdb1ba32a33e5207ea60405ab5be92e15c3b2f31963e342ae662b922a3bebc821ca905f6ecbe45675a6b8b1401bf08f8352d3d95c33eecfab50dc60fa93e9512a69754bc97be7649ccad9d918fbec8a32dfbe1cb95bee08295fa6d7481be', 520);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (53, X'5152affd667401bf8a5ddecccfd40ebf37cad4979745df3e4b17313aeea08b3e8185e7e9f4de893ea81228645df5b4be32f260542fe1973e0a51637d09e7a2bec55e2a6c032801bfba06d8b8f969f13e122c2a103ceeda3edd2c43a741f7abbed354daa7e4369cbe67bb388c2a318cbe872b6be9d5cc94beb396db48d91f7ebe', 530);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (54, X'04d82778d3450dbf257e61ff62b7023f288b1f960e80e73e7140781c5adcc7bebb7b2523ff4c903ebc4aa22613f9b3bef15ed125ab28973e671a87f38057a2befb41b5d37863f63ec867dd17039bfc3e297d42bb7c88d3bedff4a63d5153b2beeee4e077384d8fbe0250a93e90908dbe9b18a8b4abea93be74c306908e8382be', 540);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (55, X'2f29d2c5df1a083f563e682799f8073fb8f01cc0add5e1beebb35a51b138cabe6ed11df7dc739e3ec1f18d5811f5b4beb8d800ecd291973e4fd6abe752fca2be0a85d2bf2fb0fc3e66cf6ff959f5f7be78080843f15dd8be8f7c385df04a973e66bbc8f6b0bd8dbe5f61096a8c1090be3450f998a63195be8d1c84f6ca6881be', 550);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (56, X'0b464a23e52a083f3dc47c2486560abf312c7dde37e8e3be305e712429d081bee10460095c0ba03e04210127cb95b4be0ac4ca3505cf953ee06d30ebe201a3be6f7a5e146355fdbecff15d00b79ef8be24a0901ab18dd63e1a70f0465743a13e0f910c1442bf9bbe5c8cceb496ab8ebe1dc2ac48b7c294be8a75a103454e80be', 560);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (57, X'c745e4fa3cf50dbf6a6af4602ed103bfa6883a4037e8e93ef150a1238547a2bef81945172004893eda744321d469b4be9ce96ae524b8973e285acd210bcca1be0870b73dca13f6be5f9acf6140dffd3e1960097c4d5ad03e484743e2c1c8b4bea99bc0228b4f98be2904654b00b88bbeb1f813b060a294bee64c3121089781be', 570);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (58, X'3c3cb7345eab00bfbbfc55c56a750c3fb22a0310ddcfda3e6559a378b1ddcbbe8127d9fc5800943e55c59869e0a5b4be9e06238a6ee5993e5fbc9c59e703a3be2591809ab60b013f30cdb1e5a815f03e5533cd2d8a51dcbe740aff3e7286a8beb4584e0dda858cbe8306e8d1403490be6142c5f2abb894bec1fbfbc1a33281be', 580);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (59, X'f73755c97dda103f30c5308cd23ef73ef57818d478fae9be73d16f98df30c5beeb4f776e0364a03e18c72830ad88b4bec663fab8cafa943ec1e86b4bf9a5a3be73f730adf607ec3e86c2e24aaacc00bf753b784f096dcabea92267d596a4a63e0321547ff70492be086dcc4683958dbe3e8fa9b8167894be5f3b3368e23381be', 590);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (60, X'a23eadac2c69f23ef7b9fd59c60911bf4fc49d1d30faccbebbd1d49386929f3e3cab319515d29c3eba8e923fcbf0b4be18f0ef8ef03b973e5305192649a7a1beb4b35b5715f102bf402146f0a326e3bec1ce4247fa27de3edb624436289e503efef8a030ef779cbeb8fc72c44f8a8ebe2b80986b7f0f95be302b4f0d892181be', 600);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (61, X'9db2365e2cf011bf381cd54bffa6e3be8291e127f816ee3ee28082a15f53babe17f189525cf5853eb0301f4f0ba5b4be5efc216b74979a3e9b692f579e71a3bea57a528652add4be430374ad51c6013f9f723493daefa03e6ee0dad0f2ffb6bebe15e93edc2295be368e8c3496558fbe08c09ba8df7294bed81ad875025a81be', 610);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (62, X'fba5b7815c74b2beba4da532f60a103ffb790604c1d5a03edbf45423565ccdbe67672f95002c963e94db84ff0fd7b4be982ba0233fca943e559703c1318fa3bed5d90b63a940033f656bd4635bda933ed2ceef25877fdfbe471f562b0ad893beff12725604448cbefeb1b07117148cbe9a568186537594be27647332146481be', 620);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (63, X'728beaaf4815123f2e77808eb862ddbe9bdadfbb71cdecbe17b04a308868babe5e6a5a25d2eba13e8528ec120948b4be57976018b6d9963e52611cdfdf97a1bed9f183464046cebe6818d63ff02802bf2bdf7d22db10903e52b7b12bbedeaa3e2a9e1d231ebe93be5226de6a454190be45dfe575c5bf94becc73c000eba780be', 630);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (64, X'ebef8ede0624efbe7f2fa0700c3311bf52acfb0130fdcd3ea5e7aae13e0d9e3eba44e62d1ca6963e7eb94584ec89b4beb420bc28f49a993e1af8ee32a1e3a2be977a37fffa1a03bfc0ea42f46a78de3ee4cacdeb2b74de3e218ecdd7b9e6a2be0a6a0206a4109ebe0c3dc9e230968fbe85d0108ee65c94beb2a57f74cd2182be', 640);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (65, X'9fb63f04330511bf89563d0c9112f53efa254ccbac9eeb3e21c636944f68c4bedd8fa564ee79883ee689b37f2ce2b4bea6780e0bf407973e00823d578803a3be0e82e97e3170e93e44adfa56e1b8003fe793063b566ec8becb6b3a39f8dfb4be6f504c0f70e490be72e687564af98bbeba067f3ff3e694be9f44b767586381be', 650);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (66, X'd0470407e958ff3e25b72c1aa5000d3f397515969084d6be799944069acecbbe315c1d27ed91993e9057ad7b60f2b3be83e50cbd0089973e1e006db877faa1be851e8286685c013f87ab6b722433efbef0aa5965a6afdcbe841ffce20c6b823e7267adc179158cbed6dda80b72a68ebe6375239a610794be27229e7de1127fbe', 660);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (67, X'ab2c2e8191d40e3f8771495c92ca02bf216a8c05d21ee9bea100617a47f2a4bea812ca85c61ea43eafaaeee96941b4bed7c324e65ea6973edcd377730a96a2beaa8880efabe9f4be455578243e3fffbe30fcac9fff9bce3e3fd16b272c12a63eeed7be9a607198bece2800f90e8d90be35a6a840c84e94be4d0d3b4b613e83be', 670);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (68, X'18ad98a3cc0a07bfa1f86ba633300bbf71884a3ff96de43ed29d2f6f367378bea2bc17d4e82e8b3e221faf031893b5bebe7b1ce43f65993e98b01286ca12a3bec002301ddf3efebee9eb572d2e00f73e80dbbfa75359d73e927f17697a35b1bed13ceb3716169bbed2198c7b85b68dbe7071e449ec7195be032542a2ae5581be', 680);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (69, X'dec3ede69fef08bfe3cacde9c13b073f9ee1d87c18cce33e5735183993b5c9be07204451ca798f3eb696d81adffbb3bee92d1b3b00c8973e36dcc30b3aa2a2beca5ad445f1c5fb3e55d6a3a47c40f83eaba5eaab1090d7be97aeadef09dfafbe40493b4ca59a8dbed019064de5cb8cbe7eddcf2e809493bea7b99d89fb3f7ebe', 690);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (70, X'87e9ed3f43ad0c3f98b6531a7fac033f7122385a5082e5be68c461a794dcc7be991198476089a03ee4e5be12cfd3b3be07b396cc8d56963eb2eda270d500a2be112a0a1a977df73e85cf42f16880fcbe07dfe850656cd4beb98f82de6f2aa03e95e8c884e3178dbe1fde2e37221490be1a5b6d5e7afb93be8f140cb87e7683be', 700);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (71, X'729d6ebf90c1023fe968f0b4643d0ebf8d9bd7f35a87debeace858af20c57e3e816030c9f1f9a03ea1b81cf24f3fb5be27aeef89b9b2993e028fdb61a1c8a2bec8fc192f76ce00bfda8b1924f931f3beed42e9bcfd53da3e912f850aef50953e6cf56663b6099dbef23a0ba6e9dc90bef8d1ae79e09095beb8c79763cddd81be', 710);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (72, X'e4ec0cd2259c10bf3dfe3208796afcbe3e6362298a4eec3ef65fc738a8d3aebe467bb129c855863e4b9b7ce6033bb4bec6061eece7cd983e8b249541d6aea2be59af3d9c5985efbe33bdd3958284003f5a74bd118de6c53edf639b6d6528b6be317c8841d4e596be235171e0a3848bbe755922928b0994be31d26aea11537fbe', 720);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (73, X'766ce4553c19f5be7c74f7d3bfc00e3f887156d65d6ed13e96a96d0d180dccbe9a28f46815d5943ef746c5636d9ab3be963966562228973e2f1ab9ee0a73a1be74a8575ed667023fc6c02c29c020e43e0daf775af031debed99b07e338e0a2be3ab7fe49c86989be82a2c84f12bd8fbe4a582848f6d293be7b9f62ad8e2e83be', 730);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (74, X'341fc88300d2113fdfa7f0463713e73e37fa1a5a35c3ebbed5cae60f6b62c2be85fac6eabbcea23e4cfdaf319469b4bec9e69e6b3e58993e57ff9ffe71b1a2be6277e6e4499adc3ed845724b3bd001bf0654f740824dbfbe6212540706f5a73e2742ae959d4e93be37bbd6dda73191bed084f856180495be95b833161f4182be', 740);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (75, X'a0b6270f3a33d43ecf3538fccf9511bfae91987ad82aa7be3940d67be326a23e20f6ae1ea8aa9a3efe7ccabf9e05b5beb48d0d769e3b983ecaf896ef04f0a2befed6d0603d8703bfb4a8df627477c6be90d8d2127c1ddf3e645aad62dbaa8bbecb7cd34fc4b49dbebeca488239e88dbeba2844fd25eb94be26bacc29e9e680be', 750);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (76, X'3ad4e8cd0e0e12bf61cd926d2ac0c53e38bd50331fe6ed3e8ed8dad7379dbfbeef0c486c5ddd853e712ad86315deb3be451694ae20c3983e6f7818419d9fa1beb0f182d24525c03e46c8b0a516d2013f993aa37fe218adbe772043c1b49db6bebfb3fab0a2a492bec8de00bbf1ba8ebed619422fe83594be6e68bf83dc5482be', 760);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (77, X'0cac598a1123e83e18f9c18558b40f3fd14a5b34a0fdbebe372f8c8593e2ccbe6bd65690778c9a3e31b2f8857036b4be006277bb300f993e9c0dfba6cdcea2bedd84b6ee67fe023f8161ec5027c3d8be5833ee20f918dfbef09faf1654c586be227b347defaf8bbec084a08f07be90be9fd1b45dfe1c94be844519784a7a82be', 770);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (78, X'5d5c76d1967f113f27a34f98cbc6f3be1b868d58be23ecbec0319a56b142b4beb69c0da1d39ea23e215b25f952a1b4be51823c9af94a963e357e775b38cea2bea3a3bb41c1bce5becaa00dda619e01bf63e0d7494feeba3ebbc50967ae64aa3e9d7733f1989696bea7c641f63b3b90be86967118e52e95bea560ac871e3782be', 780);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (79, X'd84cd1c4567cfcbe2a5505d6a83010bfbbe6f0fca301da3ec17d78bf671a963e592ba3a53ec3923ececf66aae62eb4beb67832ef6054993e9401c668125aa1bec11cf32fe6fd01bfb2eef9cf5444ec3e9ab40e342286dc3ed455160dc3c2a9bebd120bbcbbeb9cbeaee054311a9c8ebea0d247c964b994be2b1222645c8e81be', 790);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (80, X'2a2a49c2d73d0fbfde588c83be12003fb49818652228e93eb68d31a9346ac6bec05d149e7104903e510134418713b4be36a883134a499a3e887d0fef97c6a2be3262a225bf34f33e334e30d1fa84fe3e8dd3aef79e33d1bed3a1cd36e0f0b3be01be5ae4b0fd8ebe1c57ec74148790be435d6f296bf993be6a5acdb132a782be', 800);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (81, X'd47bce620f65053fe59664d6c0fd093f06bba9490155dfbefa9d80968ee6cabeee0f66d895a39b3ee83bc5ae305eb4be20d87c52b3dc953e3136e90dc7baa2be2fc54581d511ff3ec8a5866f4a3df5be2bd41c114218dabec2c2a2f0ddf6923e03ce7200f0998ebec9afb302a60490be742efaa76dab94beee5c3b9cd3e282be', 810);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (82, X'1e63bf702fc10a3f2efb5de20eef07bf0d170c2eadfde5bee0d621cb677c91bebd48cdb34de1a23ec9f2a3734e3db4be4ea290712d35983ed21fbe5f4ca3a1beaa9a776a37a5fabe16abdea51434fbbe5497e0fb5b45d43e2875e5ec953ca23e91e7036d50ec99beb5831288016d90be661cc3362bbe94be0af71af8cc6a81be', 820);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (83, X'fd4ceb4e88d60bbfa285f60c4e9606bf64ade8fcd756e83e7862efcaf55f97be61576a4190f9893e59a55faf9d84b4be7829a9d9e4e39a3ef1697f79a6d7a2be7aa75c234d25f9be0a013d4f24b9fb3ed897f87e01ecd23e45b14f8a721fb4be4c9708a6271d99be2d1b33df278390be1b800303a93f94be566bb3e5ae1f83be', 830);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (84, X'0f3939108fbd03bf858ca14065e90a3f2248d6a2c671df3e298c434c5c8ecbbe99886507fbcc913e1957284b21c0b4be74c3ffa18372963e4e0ecdc1950ba3be9319a0e67319003fa9d2b2612912f33ef698e3556ddfdabea96e73ef9194abbe0c5349271f0a8ebe2b53870c3d418fbebba5733fb78094be0dcc4b5ed4c082be', 840);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (85, X'3b2ebce9d923103f88f1d008ad39fd3e7bab9aa0f1b1e8be993f3416c4b9c5beb906fc6c74b2a03ec348cadd54e9b3bea1e9c2e26566973e9d4eafad9f14a2be8f35e3a82379f13ed9c0ee42cc1000bf7bdd3850a6adcfbed802eec0d3d7a33e9a304e8c9dc48ebe4d7dd854a28390be182003d3e10394bef42463465f4881be', 850);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (86, X'60448c8a1055f93e29db2ea91a8910bfce2f6920141ed4bee4850dd7122c9a3eeb93d4d0d9a59f3e2b9f53cf203fb4be61b6949fbe56993e0f90479dfc2aa2be3b12bcfaf35d02bf09f71dc7ab22eabeeeefad8c5524dd3e6a5d3078ac407d3efc84c10eab009dbea5342f19346f91be5b8449578f3894be7cb38016addc83be', 860);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (87, X'7d6de51d8ca911bf9651aff1be67f0bee45230c67bbded3ed45a3ae4b2b6b6beed85199719da863ed51f26efc0f3b4be9e6942f061a9983e9c218dcb65bfa2be66f58c65cad7e1be1af8f178fc7d013fde14814febe4b43e34d0a726ae4db7be0b155e4a22b895be4b43e5c6dda48ebea6d2dea9970195be56e6c4d65e9f82be', 870);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (88, X'e051da3f9354e0be783ad95bf1f80f3ff390a655cbbbbd3e109038b5e88accbe87dd75ffd803953e6c9186e133b2b3be07640c3287fb973ef0286c15d34da2bea4d0a7225922033f85e1b70878c5cd3e71d2ec876345dfbe005002bce2839abeddafd3acd92d89beac21895a41c88fbea93cdea4fb5d93bec2201a948b8080be', 880);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (89, X'9bf6ea22962e123fa3e9a1b5643aa7be01776c5f61a6ecbed32d4a93ccdabdbeaeb595a3f0b9a33eb4bb5dcf7b99b3be3b153729c72a983ef0d741da72bea1be3da5dfc0c71c603e6cf1ad40c53702bf82545f2dfd36a0beb69426e87a59a93e12716d8ebe4a93beedabc5baf05b91bec8b6a141a1c793be62e77ea53c6f84be', 890);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (90, X'70f2b51490ffe0be6b9d05b2408411bfd24c9f1012cfc13e8a408449fadea03ebfb05d2ad84e993ecc43507b747fb5bebab777f8b3c39a3e08fb48aecdb4a2beccb90ad7af6e03bf2ef988210310d03ec36a769818ffde3ec1327441a0199fbe37f47560053c9dbed73825e97ba290be82b6ffae983095be11b0922abeb182be', 900);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (91, X'150eddfa8f8f11bf347c951ba9ceed3e5b56201f7ba7ec3e9e67d01e67dfc2be2deec310d7a5843e68bab4094b0db4befaa45c4b0d6a983ed0e550086033a3be7eeebb144431e23e6cc068b50f42013fd4bd91ac7b9cc2becee6df24f7a5b5bee3ac58909e6891be0960853d625f8ebee718f07bb17993beaeced31bf69180be', 910);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (92, X'92844c8193eaf83e94f7c20f28430e3f0b05b50eaf67d1be3a749f14b00dccbe3cedd449afd29c3e6cbacf4c83a3b3beaf3c52613acc973e8d153b4e6f5ca1be849127501f19023f84db54db17ede8be4b4fde49a8b0ddbea11e96b197c73a3e61f6540bf96e89be59520a3ce93190be4a40456e456e93be63235a51dbbe83be', 920);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (93, X'3b0db5810050103f5460022343afffbeeec4aa9af15ceabe9b0b634e0b9cabbe24cef578f49fa43e910eeb6bc3d6b4bef51cc01bff6a9b3ef9119c032ebda2be79f289d64d94f1be0325c136c87b00bf9a5bd1221ae2c83e9b225b77e94ca73ec1c0fe06e94297beddd707e06d8692be14ee863214bd94bee400266e454283be', 930);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (94, X'446d7bb7c53404bf73baf1122a480dbfcab6df36ba04e23e977b360a6154753eabc08a1888f48e3e5299fbe2e477b4bed5429a8921ba983ed754ee88ba14a3becbb7b24c474400bf5f624974011cf43efbc15e26266fd93e0929abce7a02b0bed99df35484019cbe25d6f1ebc7018ebedd850d63effd93be3c438790889581be', 940);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (95, X'6c973fd743650bbf643d8a740702053fa7b5111192e2e53eacce2cd1e527c8bea8048f51b2bf903e67cba98ad783b3be3a1d4ba82a7e983e2a01272dbac8a0be7c8b3d3ba508f93eecac68b200a5fa3e8b2a25e19182d5beac9473cc12b0b1be3c8abff569598bbe33f6106369998fbe6c9d0bb6cbeb93bed94f9c7609c682be', 950);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (96, X'386a8f6b926c0a3fefb943bd6d1b063faf4fe4ecef8ae3bee42fd5defc01c9bece7103b9162da13e0fdef78077ccb3beac5da9e7ddb49b3e09edb5917876a2be3f68c9c08f5efa3e44442bb8443bfabe948b275c3093d6be56416a6cdc3a9a3e1f6b27f2ac408ebe0604a3a3384392be9a4344388fc993be2debd360676983be', 960);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (97, X'444e69cb45c2053f8af9c4c991530cbf0b20bcf92ecce1bef691c2bd18d0533e73bc10d02509a33eeb8357d344a9b4be5d6b819a6789983e1f0064d70dbca2be15251bff977bffbe1110e8394c34f6beece724602772d83e928b452a02329a3eb66660cc1ee89bbe523e44cf464590be33d170f7dfca94bec2e4cbe685e682be', 970);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (98, X'd7a6d9f71bad0fbfc7552188954101bff521b17a0f41eb3e29dbfbd4f1c2a6bea391f5c1370e853ee3042a662c1cb4be4ed342218a619a3e8e9d84dd193fa1bee90c267f8330f3befda89f96c87bff3ec3b29cc332c5cb3e9024bd292ab4b5be957dc925abd096beccde10b0ccb28fbee67850b5b06e94bef396604c16ef81be', 980);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (99, X'916013a933c0fbbe561bcc6036ba0d3fc266e19df48ed63e8b4c50bf56eecbbe9911053a5cc1963eae0be5856b89b3be281ed82c40579a3ec4b597602281a2beb669bf744dc4013f6b5da38ec18bea3e06e808d90d48ddbe95e919931625a7be471dd6fe9a078cbe8d17cfba136791beae829921413593be9656f8e2008b83be', 990);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (100, X'bd1e3034d36a113fcac8b8e08ae8f13eef069951b8daeabef217ddde5288c3bef33e61538c86a33e29a900763585b4beec26c97e1779983e155809dd0381a2bef525aa8bccaae53ea20a508e215d01bf6dc566f3c092c5be8ecb4640aea7a63e9d592d769cca91beb8163301dcd290be467fe996693495bee72e00ff44b383be', 1000);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (101, X'339ec2b7dd72e83e143c34d8e56311bffd69905521f8c1beca697fc629dfa13ea01914dcf4a69b3e7532babc7e31b4be3b77b39830a69a3ec3b0145bf589a1bea7e0d314704c03bfce9fe1fc1bc8d9be4cec80a091d2de3e5716a098f4ce7abe83f6db91835e9dbe58c1fbd19cff90be8dc2852f9e7f94befe3c7eb5f18a81be', 1010);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (102, X'fcc4702ec91a12bfa03265df47e1cebef617129b0628ee3e0966db561bb0bbbe8fc5b74704db8a3e3dae4645f689b3be03bb54d583db993ecc1cf9f20d3aa2beebc68b137fbcbcbe3f493e70e1da013fc3ff66d7501f83bef2fd0099b4b4b7be61d4217de07793be5ce2b68a7b9490beb0663432508493be87e9df91d72a84be', 1020);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (103, X'704ec4d47070d43ef3e2054d480f103f994536b737cba2be7933197b04dcccbee89909aa4e38993ea108d02fb13bb4be5716425fe483983ee0b540e297eea1be1ac2466dd536033f8338350bb132c6be46bbf8375675dfbebc40a2dc1c8490be5a017fd586aa8bbe53bc8612ec1991be69e2c7a9a70195befb76c934a0c483be', 1030);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (104, X'9a010e2580ea113fe80b22d0b26aeabe9a3b646d27a5ecbe7b8104245d55b7be4ea1a29975aaa33ea2455af2f287b3be5539d1348dcb993ee4b54749b9e9a1be2860b6626a87dcbe7d4890c97e0002bfa495a6e44a14ae3e5ac13eac56a5a93ee9f39bb2808395be6cd55203497d91be20a62a678a1c94be757d57622efd81be', 1040);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (105, X'09e98b08d2c3f5bec69f7a228dcf10bff1f671ee8b60d43e8920d66b3445a03e7841a0361d84963e7044c10bb335b4beb78d9d1b72069a3e225957fe8fc4a1be5200240a09ab02bf3eaef91f6262e53e1bb4c0148ea8dd3e35066d9d3b60a7be1e139d7737359cbeedb152598d1191beb08f9777512e94be011e09e489ad84be', 1050);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (106, X'74ea71dcfa7510bf251a47d59a70fa3e554441ecfc9dea3e282b75f738a8c5be6fd159b94cab893ee6aa682fbb6fb4be4651983fbc4a993eeb40919e7478a2bee84b9aab9db1ef3e77c8be4f6317003f37a8464b3c2ccdbeb2912fbb8ec3b4be9ae4194dd4c790beaaadd4eb153891bedbbaa9ae9aac94beca9c231d6b8a83be', 1060);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (107, X'48364712bb76023fe7fd85fab0c60b3f0ceacb6348ccdabebeaf18c17b14cbbee790f0ae306e9d3ebcb9d0e2817ab3be4a4a4301906e983e22a7d2bdad59a2bed170658bb893003f0a8de9de785df2bebe3e2d644394dbbef1c823511c74853efde0281c38f48bbe8f9bf66df3c590be64806524d26093be8b8c97ce739b82be', 1070);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (108, X'7d06cf39a41b0d3fa1978780724c05bf718084d493b0e7bee93ace31d23a9abe0a4c210711dba33efa3f48d74d07b4be1fd9eb588451993ec4fc15ef8945a1be0bc841a372b5f7be450867b46182fdbebe7c1161acb8d13e93e31592c90da43e28076f959e5398be323d3b9ff05592bee42cdb18946f94be7101d9cf25da84be', 1080);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (109, X'ef50a590397509bfe7e71f818f2a09bfc379a524ba5de63edee97554bf448dbe817057a7ad498c3e3d2d91634291b4be4bf4159056d39a3e80d51d80de77a2beba63741618f6fbbe23c9154e394cf93ef62d8368db70d53e783b90ff955cb3be455071f73abf9abe93601a1a026991beef6eed852c9094beec0e858410c783be', 1090);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (110, X'0ba74ea4e9a106bffb712c04eb26093f38cfa421faf8e13e1b131f298ee1c9be8a7d04d84837913e4c6529ff049bb3be8f88f5b878d3973ed106e2e26785a2be7ea85e7afbfafd3e11fa835176e1f53e6803dac29c3fd9be163bc5764ddfaebece839c279fdc8abe6decf62a2d1790beced98dc0ff2993beb7ceefd6e47b82be', 1100);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (111, X'98458173f28f0e3fb0eecdc6fd6e013faa7221cef304e7be9220f41ea5c1c6bedc0c71b6984fa13e52bbbc7b954ab3be8e9681a3172f993e20c07b3fe2ada0bed106a8978cc5f43ed968c63c4f5cfebeb514576df344d2be8eddd49b3578a13eaaf8a56546e68dbe78896018d32392be212bbf893bc193be258dbad0f9a084be', 1110);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (112, X'56c08a62daf8ff3e40daab7d3ebf0fbfed87ff8a93ddd9bed8cb4ae177be923eb675bd045160a23ed17638c50386b4be83de11d18b949b3e22d9a5e73578a2bed0a617cfb49d01bfbb6318990471f0be86ea0f0feecddb3ea6aaac26c975863e3c691dc096739cbe021d1eb739ab92bebe2ccc818b5394bee9407d92546e84be', 1120);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (113, X'4dacc702ae3411bfae3017d140dff6be7cffed131c30ed3eca1ca79f0f39b2bebafb17058f64823e89d16bcd5b69b4beac0f13f0a7e9983e1770455a53fba2be993c2314eb34e9be1bc663c7470b013f3c9b25c05490c03e51bbfccc16dfb6be14d3b9cc0d4295bee3d82463a5cc8fbe472c63b15dbb93be404b96bac3b782be', 1130);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (114, X'6e881d4b9c39eebee9aabdeb0d850f3f13dc208293c6c93e083a61d0f073ccbe6e927822197e973edc8245a53763b3beed045b613e78993e22646bcfebdfa0be00aaa32015d7023f79d1d9c5dc52dc3efb74e0f15cb2debe4a7aa1358c71a1be8ba81eae3dd688be9fa04341a34491be97c9c4434f0193beb5d6d3c0479e83be', 1140);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (115, X'b59bb837fc19123f77e3b601477cd73e7e82b8390644ecbe819992d63b60c0beda18a833fa44a53ef60330919500b4bec87425399fb29b3e608be7cfe8aba2be7116f01e2d95ce3e33dee269411a02bf03ee531daea7b4beff49484c9274a73e55f55893ecd591bed5ab0edc2d2993be9643cbe53eb093beacc75416862385be', 1150);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (116, X'c05d06a773ddb4bed592b6e45aa811bf3bc5ad557e09a63e5abd8d7a22a8a13ef9971fb9af8d983efff1bca56ddfb4bef28dac83d56c993eebc60bcbc3c3a2be2805a49bb29003bf5cc2e034cb4b983e70dd83995447df3e053b675b766a96bea7a2d5ea7c9c9dbe6076cd08ca3491be14232689526494bee7fef4c771ba83be', 1160);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (117, X'c194fb190bef11bf36dcef415d26e13e1106a2390675ed3ecd016ad672dcc0be44aaba8584a38a3e521e1f921e75b3be8b91e6d130239a3ec244ec27a5fba0bedb1c9ba7d682d53e0ba0b9f8799d013f98f84a7ead0bb9be350bfbffd3fcb6be6e164e69992591be2102d4aa8d4690bec737613b367493be2454900e2e7582be', 1170);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (118, X'9e688cd89a3df23e9ac60e3acb3e0f3f99dbd76cea0bc8bedbb51ab4e6facbbe5165a8c9ae8d9e3e99898138ad40b3be90e7a0dae5d99b3e4e8f1d8dca0da2be9e688d28a8a7023f72cb45672466e2be888713785d90debe05aa5db416cb7bbe1eeacf5f885788bebd7763c594ac92be9c40f8964ac892be69c6ddd4943385be', 1180);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (119, X'c6e56001bd0b113fbf99778e5a7ff9bed9966debcf71ebbecfd28f1c77a1b1be60cb193945bba43e60d5eb355a58b4be5c69557e83e9993e6ad88d758a4ea2bee4d170636e21ecbebd8e2525742d01bf328fd7a6b30ec33eff81b7912c24a83e4d10fc5d965f97bea6bc0c68e23592bec56624d399b394be61db1b00e68384be', 1190);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (120, X'd11614ec9f2601bf5b953aba41140fbfe1651bb9e9ddde3e41cbfd21de3d943eb6264bbd7650923e61a47429c823b4be08312337b7159b3e13056c8f9258a1be3783e413844201bf50746eda09fdf03ec13c74692a30db3ec9732e2230e8adbe6fc37832620a9bbe62248cfa628e90be67b06a53cf1f94be140e45db264082be', 1200);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (121, X'355b53ca7f980dbff800dbb88b84023f68c113d97ce3e73e3925e3de2227c7bebe65ecf5836e923e2463829c8b1ab3becac23c7ac88c9b3e091268c43fe9a1be3f54c3ae6e0cf63eefb9d7955dcdfc3e1768c1baeb41d3befa39627a9d1bb3be50fdbdbbc0db8cbefb57cbfa3edd91be9a5cf736edbe92be80af0d887bd084be', 1210);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (122, X'5386f5bb7ae6073f0193b0a53151083fb0016aba4b82e1be5fea4b737d30cabefedf0ac7aee2a13e28da36ca3f3fb4be1f0fcc80f16b9b3eee3c9d850337a2be1cfed7ae38fffc3e337ac090c5b7f7be8a8926827c85d8be7c352b8db39d933e27b978481ce38dbec7a0ed758de391be4d56fda8278694be555f6bacb6ae84be', 1220);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (123, X'65da0690e490083f616b0e2b501e0abf33b61b8f8e1ce4bed01fb933b5146fbe7910e3bdcd93a23e8351f0310f27b4be8445611ba2709a3e624d62d469fca1bebde124725f0bfdbe573f9a6fe2fcf8be42992234fb52d63e3cc91141b5939f3e02baa76d05d699be63868e6c4afd91be50d1318c9c1a94be48231898c18e82be', 1230);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (124, X'15c179eb11d10dbf1d9a1d1d3a2e04bfacfae9c8bde8e93eabeac8df2736a0be8efdb60169098c3e0df2bc100362b3be2028ae8f0c909a3e914ddc32158fa1be47c5b081b06ff6be3eb9f7f46797fd3edd5fb61b45b6d03eb25e113a9d74b5bee529c02d816098be0a9e7732ce4691bea086652b037793be4b7ccd4c0a2985be', 1240);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (125, X'405c1166c01401bf5a9a6c25956e0c3f7e791dfe4498db3e85f493e87b4ecbbe571ccf76d5df973e69b198670debb3be1c07085cb0169c3e40a214ea3c93a1bea0e08d9b40f4003f9e61ec490f62f03ee369d1457f26dcbe21afb8300bc3aabec04d3905cb658bbe9c3c52f2e8f691be07d3da1fa47994be55b21e015d4e84be', 1250);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (126, X'3f0144dce7d7103f7ea95480e621f83eb6d5a076e7c8e9be39c02df8fc6bc4bede27543dd610a23e052acd6a2861b3be61a76650c81d9a3eb8d3da52bb3ca2be175fa0dc9fdcec3ef0e9a64c96c000bf38880db754ffcabe8e604c66ea21a53e0e000fbb019b90bec45487b9d50992beb00f9f2f1e7993be86c2a199144783be', 1260);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (127, X'f1e3bab8be49f33e419f9dde320611bfdc8bbb8afe19cebecef7c3e21b54a13e2c21e5cf4690a03e32be7e890d90b3bed8a5d552dab4993eb65c775703b8a0be45ada38991e202bf7243c6328819e4be0e650e4e4d1ede3e9a8f4f57a96b62be16c3eaf804a89cbedf5104b92d1392be9cfeb3e1430994be42009bb55b5a85be', 1270);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (128, X'ff957d4269f911bf41e0ad3aea10e5be1a212501c22cee3e585c4cfc7b68b8be6ce18b3a7b77883e3777541af41eb4bec33d02609a9e9c3e0779adfddeeba1be85f7929de46cd6be88fd008d69ba013fd76529d6c5b1a33e112279ce14e9b7bef72af8d35a1b94bedd62d4feb03d92beff5dab18138194be29b2f4cfea5c84be', 1280);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (129, X'93a75fc11103bfbeaee270fb8b1f103f05e80fc706f7a73e84415f7025acccbe2e2ebacababe993e78fcf571594ab3be242ff44472c7993e98700d63f142a2beeadf60239b42033f092b1e237ed3a43e1ddecfc8966fdfbe0cb14672e89297be553a691515468bbe3dda6c43e76391be480f4015a30293bebbba4e8788fd83be', 1290);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (130, X'84d00b83a82a123f13386d3fae25dabebf0dddc8abc8ecbe141e66b833cfb9be6b1ab5aba414a53ec6d66f65f66fb3bec6748b3f14e6993e5bc7aef7c1a5a0be9ccf2906cbbbcabe404b3009ba3602bfe3af1e0d1abd853e2fc8912e169ba83ed55960cc4df392be303276a7f89292be8bbdcec42a2294bece7c5c810ae084be', 1300);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (131, X'e036297f739bedbed04c112b9c4811bf5693fb7388d5cc3e5087367a7e6fa13efb776df2ee2c963e7f8e9bc15e37b4be890e754cff429c3e69b638688615a2be246d25001b2803bfb27380d4509adc3ebc27842f898dde3e5567d7e3edeca3be60e14b3e5d0e9dbe25b7fb10076093bed100f68ec70794beb91ea0a1eb1885be', 1310);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (132, X'a45d5b06ad2511bf78cf0fc20086f43e9cb15a0d6ae0eb3eb9ae31481fbec3befb8266f122608d3e7b53c08e518db3be22f32806bac9983ef00c8bbfd843a2be8a279ed9f1a3e83e18c21fbf50c4003f750f8d312ca7c7bed1e82ad1f8f1b5be7af65bc996db90be1e4abfe401a790bef16ac7c9317593be382d2823c06384be', 1320);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (133, X'4e8f46aa3ebffe3e279515d2be4e0d3f9ccb332e1cbdd5beffab12519e18cbbea2c8251d59959e3ec05c13222a0db3be25a679a7c3ea9a3e09ab6646d043a0be619029c36876013fe708a72d62a2eebe8faa230c50ccdcbee2b40340042a713edec6fbd0b66f88be23767225566d92bee03daa31888793be81ebf9d79b1384be', 1330);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (134, X'f73ecacb23290f3f010b6a5fd87b02bf0fe7f5018c4ae9be7807c892a116a4be65a128bd4eb3a43ef04b378f2556b3be0546ac66c2b29b3ea5a77716b5e1a1be3a9a92fd3688f4be11180e37c587ffbe180a8119701ace3e54bcb48ed890a43ebc4bd4c25d0f98be28e838c6615594be4d3d390df85393be319447291c3d86be', 1340);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (135, X'eeaf1ec723cf06bf5e3724138e7c0bbf503800d9742fe43e2710cf98b1a04a3e13fc1d64ee07903e741d5048e68eb4be31e2c671f489993ed47b090a6142a2be6f9e8771c587febeb01c0a80af9ef63e235a56362ca8d73efb60fc554924b2be17f259891a1e9abe067973f9f0f690beedffab71ff2794be4b2db8f5a2ca84be', 1350);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (136, X'd7a73fe8c14d09bf9b931376181b073f0d3da3ceed3be43e083dee4e62efc8be96ce4bac7910923e77d997ddf600b3beb6aac0e388859b3e9441e06beab9a0becbb8f2a4767cfb3edf537ca61c7df83e3241c47ca747d7bedd3df967b4efb0bece54a48f4f478abe728c8ceb4f2792bef18b235d4df492be845b229cb35083be', 1360);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (137, X'ff09e20268880c3f60d84b88750d043fc89fb4bac93fe5be9ef3913e41dac7becd303fe8cd1ba33e66b667183819b3be8a5241bc50db9b3e196942597307a2be42d967068fdaf73e93c2f8690b55fcbe211b3c0465a0d4be4b7a9d0655099b3e08adfd58fbec8cbecbd7c91f748b93be26534df563b392bed16b1fc9a00c87be', 1370);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (138, X'e579e2938b2c033fffe68b4222170ebf8ae04b552330dfbe2f4ed6d3b82d893ec19eae94a1f5a13ebcaae239e4d9b4bed235e23aec3f9a3e76626651fc10a2be2962d3c9dfb100bf23020a9cfb9ef3befe3dfcb80628da3ea7d9cb1a0692923e7ffea47a6d029bbe59159b2dd33393be4647d7a6a99d94beef0d7d30ab7485be', 1380);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (139, X'21ea82bb659410bf01edb493cc2dfdbe60731c3b5b58ec3e481c1f830eaeacbec91044d98381873ebadc966d864cb3bedbdabbcc43a19b3e62648a8b236aa1bec019b6a58c28f0bea496ca427869003f4ea18e828ecfc63e937f99c147dfb6beb5653943ae1e96be93d40bc0b85d91be96d68ca9c20293bee39bb18354fb82be', 1390);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (140, X'e604fd40c5eff5be22e1740077c80e3f2c7d7b4cb94dd23e662081d7fe99cbbec2659b68a88c9a3e4c4d99eb48e8b2be9df14744826f9b3e2770756d0d60a1be840b2bf81b59023f7d0c749e4bc0e43e19659c672812debe86878630a0c1a5be7bacac3fcd8087be8f9bc9193cb292bee154c0a7416992bec0b020c743d486be', 1400);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (141, X'd57107da8fd8113fea0c4f4268cbe83e3b88f86d33a4ebbe10580fa65d07c2be29e1d45d99dba33e54fe527bf603b4bea6efe6904dca9b3ef5a6449b31ada1be28bd8ec2085fde3e95db955b05cd01bf39323a7ffe40c0be9c93d30fb39fa63e16f7335706c591be6af6c9eed30994be49390b17525d94be773a224f4aaa85be', 1410);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (142, X'6608df639e9bd73ecda9558ada9b11bf7896db9a73f6acbeaa71c784fccea33e27c8169f9d1d9d3ec8d0950d0fb2b3beb5c212d931ef9a3ea149959c0cbaa1be55b9f07d6c8203bfe947062bac5fcabee9919919193adf3e7842fac42f0591be674bc56e77fe9cbe1bf30900c7ed91bee3e06495ac6a93be848da74ca1a883be', 1420);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (143, X'3a27b144bc1f12bf147b4f6ed773c03eceb698233b24ee3eafaa78cd2c88bdbe12102b4e2b9d8d3ea90a1fe5990bb3bea645501e1d2e9c3e536114a097eca0be128fa25a1635b93e909411488fcf013f90d83a7091f4a9be7281b99de69bb7be29116febe4a890be57cc6f1cadeb91be48ce11f781c092becf802daa96d585be', 1430);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (144, X'66690c2f15ace63e31a57c9f2be40f3f93197f02882abbbec3e8a1b665cfccbede9911e45cbb9e3e2de9152cc8d3b3beedbab2840eb69d3ee7e8b79241e6a1be44258b025a0a033f1c3e700a6d61d7be73488e908817dfbe96cbe6b9fb068dbec5370d30de158bbed0ba6d8081a893be27d04ed5ed9c93be520d0ebd488885be', 1440);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (145, X'5f4c7b34a39c113f516179ec7404f3be9c0bfa2a4e47ecbe880c759f93b2b3bee73069cff8e5a43ead3b2c81d4bfb3bebf2ec6d31cae993e2a243bb17652a2bef2f4eac04adde4be54453ee40fb501bf767fc8acf8d3b93e9d430b7c5948a83e017bcae22bfd94be438ae9c6537f92bed32cb1451e9893be257a42a39d7b84be', 1450);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (146, X'4fff92450cc7fbbe17a112a4e64d10bfe45e9edfb8acd93e849e09de4aaf9c3e6e9b3b7c385f943ef697a9781291b3bebd2372ff010b9c3eb53c7fe5f0aaa0beb30e1482781502bf9672baa6795feb3e2be71a227bbadc3ef79e357298c6aabef17a1f812c2e9bbee3c2be6ab89292bedfdf1530b27e93be9153914f8b8c85be', 1460);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (147, X'34b189b4d28b0fbfa9b31ee14ca5ff3ee744683fa189e93ec0d5bf98c62dc6beaec5ce77acdb943ee9d4c26ad57cb3be62b71b36a2539e3e4383250c03cda1be15b5e55ae4d6f23ea0d9534bb7affe3eda60746f42d1d0bec63f36aa64eab4bedddfe0696fd58ebec8d6329bedf692be0096603e144993be32bc5701072885be', 1470);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (148, X'4e98c5cb5723053f79e4f1db93580a3ff6c6c82f1bb6debee4f445469035cabe741083d5cb02a03ebff94c7e376db3be5e3ea0d47389993e1c191166d72ba2bed73a1589ed56ff3ed0652103e1f7f4bebcaf98e27844dabeac07d7647efa8d3eae8d0d116a9b8abe4196c6a9944592be1a9591202b4093bea3ffd4d2fe1f85be', 1480);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (149, X'827182b6b0240b3f9f11c261a2ab07bf1a27014d8b2ae6bea4b6e4cb92098dbe31282a7506a2a33e7282451fcc1cb3beb33476e090f39a3e21dbeb4f453ba0bed88cea22664ffabe947622e0e78cfbbe36a0f0e97114d43ec2151913a02ca13e2d3ff998b43299be02ac0dcc2d9693be5efc273be1c293bea53f4259bb4285be', 1490);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (150, X'bd384a274fa90bbf8f98db3b88ed06bfc0e1c89b343de83edf8b9bed23e990bef3839c86310b923e313affe5589eb3be9ba9ff99845c9e3e942d7431df7ca1bee56092837f7ef9bee210c6ff4766fb3e795799c98e47d33e09004805d71cb5bebfdb86db5b7998be2f957ed58edc92bec514fc3a929393bec4dd39c6e29585be', 1500);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (151, X'ca9ae7b72a2304bff4e43d0710dc0a3fc67039443b3ae03eab0667e8b489cabe7e85d316e5a2953ebf5dccf38958b3be6177264e5e0c9b3e3f898a1c72f2a1be699bfe22e5f6ff3ece498677f55ff33e375e9e37e4abdabe06fccf1f45fdacbe7fbca4b209af8abe5c9dea52c33492be52601cbff01493bebae802f6dc6c85be', 1510);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (152, X'bc9263115d1a103f95859aa00012fe3e42170594457fe8be9e2387d56389c5beff9b2a3ffecda23ebb5f7d5795d4b2beafc07bab833e9b3e39f3a99a097ca0bedbc6c35f3de3f13ea5ca2a40290000bf812653a8240fd0bef65e3b4352eba13ed8bf3553886a8ebe13b352c4f4d692becd6bb118bb5993be163d0f42026284be', 1520);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (153, X'3508421e3a36fa3e72804d49f47e10bfb3a094bebccdd4be25c4e90e8fe89f3e5e63e87aaf90a13ec2b8ad342fbcb3bee116de0a67969c3e4806f9a6ec73a1beb51f2967b34b02bfaa0160351e13ebbe82fe64b4ba00dd3e13676dda046e6f3e10f2d2023d789bbe45d2c00f086794be77ab0fc9de8393bef7fdd8b1f2bd86be', 1530);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (154, X'f56793e99caa11bf779cab45e427f1be3cdda554c6eeed3e2b672cfebf3bb5be5ce0ec5aebbf8b3e9f3c450a39aeb3be486c9ab38ac39b3ef49641861ee8a1beb05224e05cb7e2bed7d7c6252b6e013f9e2bad011098b63ea903054f04cab7be6fc464bf614995be9ed2713828e591be97dd1bb2d5b893be1231baf0fd9285be', 1540);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (155, X'e8572fd79902e2be2e87e4b7350b103f0bcb6d043a99c03ea4ae02e9f2f7cbbe4d84d4fd49a89a3e179b3f1a66b9b2be1a2185aaf33e9c3e8d75d74a8987a0be429c3d42581e033fd41c68c4f03bd03eb85517243529dfbe8ee8853a0fe19fbe7af2d2db641787be7293700d9b6492be20077dc124b292be3c88f4ee59c083be', 1550);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (156, X'549141a6503e123fec3232d077ba843e8c183b02d7a3ecbe7fc12e634d93bcbeb7d088ce3652a43e0feedbd67dbcb2be14f0e939cc109b3e4d9e02f9dc0ba1be85bfe586238d9e3e3a1489672e3f02bf11a858e3ad23a3becaa2b9cc8cc7a73e0ca063e0df1092beab1be8899ed594bef39d137409d092be1ff4d82e279d87be', 1560);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (157, X'47ea08edc7b3debef22788cf469411bf5b9e6ed8da90c03ee8b67ed148c6a23e33a95810699b9b3e9ddbb4727423b4be06c0e720aece9b3e8b073eaf8978a1be12e67d92b57503bfb479eafaa23fcc3e364ba5116f1fdf3e0209e98d1cbba0be128b4edcc65a9dbecb1dd46040e992be70c1ae02174494be4fa11c53e21986be', 1570);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (158, X'4c736bcd9daa11bf25d12bf67fabec3e5b68702670f4ec3eca95ee1a5da1c1beb8d149c7ace78d3e191f13f334dfb2be9fd18910bd5c9d3e48db4ed863faa0be629dba8e3753e13eaee20d126548013fedd7f929ded4c1bed82f3c3061d7b6be9906c33a5f2b8fbe11fa01b4de3f92becb38364f0cc092be04cacd603fa483be', 1580);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (159, X'fd22075dc23df83ef3b5833a0b8a0e3f0a88bd802192d0be426ebdbfa583cbbecc66e4c369cf9f3e76da84a1a46eb2bed96173a2b3c19b3efdcd272a79baa0be13b985994e2e023f28803f91c34ae8beb5c3b79f19c7ddbe3196ede554d06cbe3b4dba273fd088bea405be2193fa93bee9b27d618f2692bee8306ac746f487be', 1590);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (160, X'98013706bc74103fec401c0b3bfffebe19d84bcea5a1eabefb1673256b9caabec6d2f41597c7a53e0364458a3e02b4bedea4cfe4a2c09b3ec2142bd0807fa1be73593f9e0d2cf1be5a16ff31269c00bf12701e72214cc83edee61ee56d4ca53e28081e46d28096becd382a408e9894becd9a08c1c73494be54a3e37ab19686be', 1600);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (161, X'b2971b3b6be803bfcb7fcdd0a78d0dbf7f4c95e375dae13e09e005fd16a38b3e4875544275af903e354796a1d07cb3bea239aba702c19c3e25ad2a270b98a1be6280d99fef6500bf8514b1080bb3f33e0ab44ba7bab1d93eee3df489fc9ab0bede70ae3be67a9abe4d4acdfa358692be656466e62a1993be92416921a34284be', 1610);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (162, X'212530e56abf0bbfb7eaa392a4d2043f68d896d1ec59e63e92f99f6785b9c7bec467ec0a47fc953eeb9368602685b2bee53cf61c70f69b3ec536dfc69e68a0beac728e2c1cb5f83edad226f592dafa3edf6c2a42c92dd5beb8bbd2f00be6b2be785f63bb64f48abef2f16c63bb0893be7d20c3d9b26392be5be70275256287be', 1620);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (163, X'4f5ee4c61e380a3f2de9c06cbc7d063fcdfe9eabbf5ee3bee36610bef4a2c8beed4b2015fc1ea23e642242934b6eb3be5cd2cc950b219d3eea28acf7b969a1bebcc09e7e59b4fa3e955f14bd1104fabe13da739483cbd6be431c97f9d29c953e830a5c8f1ef08abe1f479bf92ef994bec935fc436b7493beac87032bc99386be', 1630);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (164, X'6a6f03b4a22d063f40f076e4921e0cbf604b543efc1be2be591b0f980d7a773ed8113d71ec85a23e7f63d1de193eb3be46c46cb340c09a3eaf70c93deb8fa1be23e4ecf76c35ffbe48060f63519bf6beb77ee1efdb4ad83eac8998387b44983e910a60d95ef39abe2bc5591e59a393be1812cfa6ca3c93beecda28aaed9d85be', 1640);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (165, X'84850b96df900fbf8f81c15ada9f01bf07659da0bc46eb3eaf255658f830a3bea428482f1b61903e2e46548cb00db3be4e9818edf3d09c3e5be3508b5f1aa0bec3ce1b7de795f3be9b77786c6237ff3ef822e9f519a2cc3e01d2cd7b63c9b6bede242dab21a695be1b68e7be234592be19c29c827f0c93be002ecd7c4f4386be', 1650);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (166, X'ace84c5fef98fcbed13132f111b80d3f4d664d00d57cd73e949f6413666ccbbe396294efccdf993ed0f7d518ea0db3bef589143792119f3ecf8b734fe97fa1bea74f8e8a40af013ff7bf8e12c432eb3ee2102b653122ddbe067fa9306e2da9be4a1e0e8b553289be970ac77905ef94beec0e1954c8c692be1844b248af7f86be', 1660);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (167, X'9f9697584569113f2defa29974c9f23ec3382259e0d8eabea3ce2edbd94dc3be0b8bb71d48a1a33e1f6874826444b3be103382ce39e9993e9c89c664cf0ba2be32decfa3a692e63ee4dbce8dfe5401bf5493c5aa4604c6bedd5a1c1d5bcda43ead4b77d5a1e290be3e7ef805c74d93be350cbd8e9c2a93be5cb56bde569e86be', 1670);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (168, X'32a5fb22e840ea3e46498f30056311bf59ce20f9f445c3be65cccc040424a53e6d3c55d2f5959f3e58bfff6e5d91b3becd5a5d6f95969c3e80927f99431da0be833c51d4524303bf199ebd5858c9dbbe46f3d4b1d4d2de3e4d342a90ba2f84be153dd4a625089bbe3889501583c693bea583bec1c47293bee7bca12dcabf85be', 1680);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (169, X'2d552856ec2512bf10f67dd82d8dd2bed51a0594cd6dee3ee84a5d7e64f6babe4d9e280a40b9903e31c6a409dcf2b2be2e11e961584d9f3e6757665bf2c1a1be73b5a5ea67fbc1bebae4455ecdd3013f533460275efb62be7ed14b21c16eb8bef003ff42c8de92beabfc7c2ca91094be580f37b33a8692be5bb02727f4a486be', 1690);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (170, X'69573278431fd13e0ad465ab5a24103fa7417c15369b9abec2de984de682ccbe44255f57a52d9c3e6bd952b3ac82b3be934e171171139a3e15199bb5cfd3a1be879c629a2d3d033fec192803085fc3bea4882d4aff67dfbe4b09292ccc0095be292073e94fd288bed147e349ddd192be04e30bfbcd2a93be8ccc778425cf86be', 1700);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (171, X'baaee0045f04123fd4e41ffd4db4e8be21631fc675b2ecbe671adf65fb18b6be7ff04e74c850a43e6591361befcfb2be3bd000e62d169c3e48a23617c72ea0be8a755de91fb4dabebbdc5730ac1102bf3d0b36e45fc9ab3e849db4c8bc28a83e34b8591a892d93be6c1757a63fbe94bed239f5a90f2393be537b6302905b85be', 1710);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (172, X'c7a15d63f4fcf4be87e40a4bf2e910bf34bbaef85deed33e08f3c8f9d8e5a13e43309b0d95e89a3e4fa9e3f8d503b3be508252fde2559e3e68ccc8582620a1bea7ffeda0c2bc02bf6f826b0ae56ee43e93f7ebf445e4dd3e20e012b1d1bba8be4852c8c8b1e19bbea4ebcaaeb0ed93bef80a789a04a192beaf5dd2023a4787be', 1720);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (173, X'3008d4cd489910bf6af6c5430eeef93e9886945fcffdea3eefcff641819ac4be1981e11a4895913e9b64d999427ab3be12d3a952577f9c3ea9ab4da3d85aa1be1353cff6aadeee3e06c52889cf29003f76022de39572ccbee6acf37ca8a7b5beb3c579cf459c8dbecb686061fde692be806355141c6493be530a2d059a9d86be', 1730);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (174, X'fe0b54b13e2b023f128ab8f6ed1b0c3f04ba807c2020dabef79064be4da8cabe69d38bfdf7559f3ee11f4709a288b2be17bb002b816d9c3eff81f3d0bcbca0be4fb14b72cfb3003f7ab9fe97110ef2bea298ed6a2ca5dbbed2d05889535a7f3e92544f944fce89be4bcf2e53289e93bed232badc495d92be64747030a87684be', 1740);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (175, X'fa07bb5c72770d3f4042f9520e0005bf8c70a3c019ece7be3fba292ecd5697beb7e576ea4c56a63e0c3150f592e5b2be183f8b1fca589c3e6062a4b177b9a0be825777c04357f7be589a94f3a4d7fdbe48fd0530427ad13e75f7a3bbd6f8a13ee132e85dce1c97beca4ab98d09e094bed077ae3962ca92bef6883f024e3c88be', 1750);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (176, X'6b6d1f1dc13509bfc993c175f07f09bf9ec4c8110f5de63ed097bacd34dd7cbe4b3191acf7b4913e802a9771bbfab3be76c3987c88dc9e3eef9c294af669a1be6f2012e6f24bfcbeccf7028acdf4f83e64bcceaa04c3d53ee1a50c0824adb3be838461fee91399bec44ab056179a93be83bd2ac8c5b593be0fd919456c7686be', 1760);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (177, X'eef76a026c0807bf5f59ff73e708093f1cbe2802ed67e23e0879d5ac3478c9bed5528e152d71953ea606db42ab95b2beef95764496649c3e589e9b8f6245a1bee9b2302cbab6fd3ea464315dbd29f63ec2cf4b1155ecd8be7aaba1140968b0becb34383d17c289be3b1a22f57eab92bea35e30fa22e291be97bc190f284284be', 1770);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (178, X'64db451a726f0e3f6ecaad49aadc013f93b3971268d5e6be2dcbd2006420c6beb468d5e16319a33e9a813b131360b2be5d7e56dfb0229b3e3da5bca8c814a0be9a4c1fd84a28f53e3f3d32a51e32febe3b7d3600a98fd2be56e614f2558f9e3e49a498497c738abe95d6c61cab9994bec35e01a73d8492be7637a908554c88be', 1780);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (179, X'2d31d34f5273003fc6f8be316c9f0fbfd32c85fb3e58dabe4b88b46644e2953ed5fc0ec94847a33e6dde651194a2b3bed6a9d7156e109f3e07d39cfd9926a1be25bfcd6c638401bf09e6c41d39e2f0beb7f847386db2db3e397571253431843e6b6612ee10e29bbef2436d12536095be968001046ebb93be3ecd66d908f986be', 1790);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (180, X'6929bbed9a3011bf62bfa5886199f7be929b7839ad49ed3ec6374a052103b0befdc88947654c8f3ec9ee97ae66e3b2be4154092725989d3eb88c256f7545a1bede8396695615eabe2ef86392c2f3003ffe2874bc1c7cc13ec9d0f7aa09e7b7be082563f99e4c94be74878d08582d92bee8978657395c92be14f3308197e484be', 1800);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (181, X'd49e805224f2efbe91efae3b599b0f3f44217e60cda0cb3e162505e8067fcbbe3344ca3833fe993e5446c29d5c1bb2be6a80aee487029c3eacd3f9c9c13d9fbe16884a62fdca023fcf1615e93bc0dd3e8385a13942a4debedbf40c44353da3bee4d27cc4f75886be3d41d10c464294beeaaf96158a3792be25bb0020bbc987be', 1810);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (182, X'a780521e6523123ffa21ca80c513db3eef85cd98e147ecbecbaa08f65e36c0be9695e0c783e1a53efa4405ac15ddb2beaed5a5da715b9e3e1e7f5d24741da1be5151ba483728d13e0cd8fcdeb31b02bfc678fe792ad1b5be68fec948eeb6a53e902e082af5f091beebd128067eaf95bedca1f368623093be125aa3d3776e87be', 1820);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (183, X'b36fd2b019449abe0ff1d7dbf2af11bfda6490909b6ea13e268b35d47f55a63e7eefc8bf7ae19d3e58e757f0bca9b3be3ed019476e619d3e5408a517a05ca1bedaaab7fe229403bfed5fda6c56297fbed19d7485d64adf3eb7bbc23ed13199be3a47cea14d0a9cbe6758daf2877693be5d6c2692215593bebfc5a8b5401486be', 1830);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (184, X'4a1cc3019e0312bf570b572ee18fdf3edf1edf11b9ceed3e4633c4317d22c0be8d522c865256903e24200937b450b2be199e601f289a9d3edabbde3de0b39fbe49bfaaa87fafd33e5524397d84a0013fa4039a7eb559b7beb24a1c989dbab7bebbe67f8ddc7e90bed68e45e34b9793bef991e1f44a7692bec2158cdeecda86be', 1840);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (185, X'ff48c09bf072f13ec1da76b3327a0f3f65a5e4912f05c7be25d8c7609eb2cbbedb2f25ec8422a03e90d59687688eb2be90d6e4e9a2e99d3ef810b27cf516a1be193b987297b7023f5d5d70aa2dbae1bec3e3df879d9ddebe232d941b31a388be4a1e18a556a087be452e679fe64095bef7ae3d58c64592be8e677fa23fb487be', 1850);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (186, X'b09e811ef92e113f776f5f862baef8beacfe6171b694ebbec3336bd40dd0afbeb3d590f35833a53e66bf41982e31b3be42f40b9fecaf9b3efebb0420dd13a1beb12997f25848ebbe9044cd79834701bf29615c10c051c23ecdde16c78b0fa73e9a8d8ec56ee495befb39518d72d494be951ccb5436a993be285383f6165e87be', 1860);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (187, X'079ba22630cd00bfe95eb2e437560fbf7b7fdaef6797de3ea77fa2b37c13993e10b68f5625f9963ec44ee69843beb2be0475c71d0e649e3e7721221b4b8e9fbe2fc8f95acb5e01bfdf80ec6f448af03e1ec7c0ba0884db3e7007529c354dafbea9b396d615a99abe5199f0bba06e93be1e7c41e5ebfa92be806d9afc4a1e86be', 1870);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (188, X'3e7aa33e07f20dbf54ac96004b4e023ffc2726cc2735e83ed2567c8ea849c6be06e749a4ae3a953e7ce479340362b2be10ad591ad6209f3ed3b081f3b307a1be29ea0800dfaaf53eca09b19a7cfcfc3ed60f042112f8d2be88a7e12d3075b4befe40e4d7f15b8abe6ca38ffde61295be8d1e61cd500e92be76a1f28422e587be', 1880);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (189, X'ac581b81a6a6073f80f5d48f80b5083f200f7d733150e1bec33e71c88e94c9be55cb3b945ae5a03e7c8a5fc2f4f3b2be08b0db879b219b3e633c05a67909a1beb14a45e0694ffd3ecd0d0fb4e971f7bec28693faa2b1d8be37f8b0baff01913e314eee3e673b8cbe6161ca65919294bed23396369f2d93beb723d6c0560888be', 1890);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (190, X'119e5f51bbf9083faa28940470de09bfbe1637643267e4bed88a9c41fd20fe3d7d1e8dcd99fda43ea9ca856550e1b2bebcc6de24063d9d3e730307dba91fa0be8e77538aaabafcbe8b009aff4864f9be5d4bd3df0c27d63eb5bafd319e449b3ea597c48f93a898be39a800b725bf94be32d435dcb11c93be505511eaff2686be', 1900);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (191, X'6ac0762dbba50dbfed68707a668e04bf713ee84e4dede93e30c37dfe33e098bec1528ead741a913e99a7d1225ac5b2be1fcbb7a073a19f3ee7e4cdb77807a1be6781f2a200d6f6be2e9cc562c049fd3e85cd1e281e0dd13e5516d4360138b6be695e23e34ad796beb2bd94868dd694bea3c67454c85c92be4e8584e1f93088be', 1910);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (192, X'9684357a4c8601bf5e61a6cb9c5e0c3fc9ac16e57c47dc3e0d3ac9e13c1dcbbee0d6f5d622ef973e3c184b1b4143b3be11b3990322c79b3ef4eef5846a32a1be2bb252506ada003fe8e84791b9b6f03e6c7c254403e2dbbe51eedfc5cf8eacbe90467c4a1a358abe88c276c99e3d94bec903c34df0f192bef2b362fc26ec87be', 1920);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (193, X'd0bec23452d0103fdcc9f4183e0ef93e53fe74c143bae9be50a86dd003cac3be8067e1fe5e41a33e28aa3dd9eb68b2be26c978f615319c3ed53ac57ed48ba0be2cee8c0c1ebced3e5729cd4012b500bfc6a38789ab86cbbebd2ecbf129aaa23ede7d47630da98cbe3095c44f9e0195bed9e14140426c92bea2c126b96d3286be', 1930);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (194, X'49ab5fe26a41f43e564d9650360111bfd4274b71cffbcebe71b1b89db432a33ec571a3ccd6eba13e29fb1e66ad99b2beabc5c20679269e3eb58fa1436646a0beeb4d12a1a5d302bfe38bd8b34213e5be3dcf8aa12d14de3ec65b4500cbfb6fbe9c7ff1c0ebaa9bbedd29e2bc797e95be7729bd2fbe7a92be7aa30f55cfb388be', 1940);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (195, X'105c940c9f0012bf0d73efa5559fe6be8672b491aa4eee3ed56445cddf1fb7be150afd733e8b903e6c96209b4469b3bee64775f1dc369e3e0d177b1064f2a0be6a6fb643ba44d8beffb29da282ad013fb8e93f78a5aaa73e557e4dd72fdab8bea94046f7931093be1a26f582a20694be1161ce6a424d93be01fb97307aa087be', 1950);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (196, X'df1cf480b889c6beca0df8312933103fc69dfc708097ad3eefabea30d3d6cbbe117188338b2e9a3e6c3e55451b3db2be5bbbc360b1879c3e9c8e4147ffeea0be9e68a5f25742033f4b73e5deaa60b03e98b64923e05ddfbee29dbffc47249bbef334fae20d8b86be6bbe236d5d4d94bebc7c9f6358a991be1b1c2a21ffac85be', 1960);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (197, X'f7794ad7e13f123f264b8fa5a0a7d6be8024a96148caecbe9e8685cdb061b9bea5383d5aa777a63e361acfd8aa1fb2be1327647c34b79c3e26ed5c55ebba9fbecf7da07e4be8c6bee7ffde9df44202bfd14bef4b29f07a3ef8a22d67fcc4a63edafa69e9811792bef74ded24fe8595be12962de2123f92be9a9c09ae1a1989be', 1970);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (198, X'207f08a936e8ebbec0b5f4ab305d11bf06b9e50c32e2cb3e240ac4856f64a43e6a0bc857e7729c3ecde18bde4de2b3bef409a2e23527a03eee72f6da71fca0becac8f682103603bf3a2f70b809a1da3edf88e9ee63b1de3e0d9742dc613aa5be533ff3e195359bbe92a6a7e8294c95be95334cdca35693be62378ba95aa987be', 1980);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (199, X'a5678749744511bf75c117b1e4e3f33e8f9887896931ec3e569544be1420c3be32399a53fad68f3e024ae5471c93b2bef65d694a96b59c3e4451a6b81fb2a1bed0dd0bcd8ec4e73e4b14e0860bd3003feaa8099cecc4c6be9f95d6195991b6be1e8ad93afede8ebe7dcea381418093be7a29bcf2cdb191be2930afdc56e485be', 1990);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (200, X'f9de5511d80ffe3e0dab7415779b0d3f3a4fa69b6f1cd5be3391367d2bc7cabe3250786245f1a03e0fa4e2df8a1fb2bee62159b5ec739c3e3c424d3f60ec9ebef13d1694bb91013f355d70dd67feedbe8ab8a89171d7dcbe15b249b353a841be01a2740d9dd285be150522c33a9094be00dad4192ef091bea208938f674b88be', 2000);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (201, X'53eb0fe8a9830f3f1756cae9ab2402bfe633f8640b63e9be860974f6be55a2bee8624e68e5c9a63e8a0afff6bc15b3beadfcf763c044a03e716869eb45f4a0be5179e2edfe22f4bef6715bab1ad0ffbe95a6caf79c77cd3e48f2c71f9a7ca33eb69d07753e4a96becaadc4fd1e1497be250564d95ac592be44a846666f5688be', 2010);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (202, X'3741badf3f8506bf2967ad5666ce0bbfb88511def314e43ef09a8f1d2a59793e5271b7a87e3a943ef74c464a8806b3be290f5ff4057c9d3e9ab29f9e626ba1bee7d754daf4d3febe65dad1a90b3bf63e13eacc199108d83ea36b22de00a2b2be543a0309c3a099be1272450e913b93be0bb426b4584d92be5b14062cd4d986be', 2020);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (203, X'a3ac7b8e82b209bf3955a6a8e5f7063f22fdbb8ef5a1e43e0379b0933cf1c7be73be37f83e67953e09900d51d3f7b1be04dd2616257a9d3e871c2ad2cf199ebe9301d5556029fb3ead3fbe4acebff83eee91a2b3c700d7bef2661bbc260ab2beac9ff106e31f87be1a243e63613e94be32221b70c83492be0b1230250d2087be', 2030);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (204, X'ed8ab3648a5d0c3faf07c523aa7a043f72896a9ae2ffe4be9982a601e759c7bee9934233befca33e017b8bf38216b2be8c2944f05022a03ea6a80b8cfac4a0bec0caa8eeee3af83e803b3e9a741cfcbe494e701843dbd4be5c0563ed4b35983e7197b6b14a708bbe6acd7854308c96be94cd92ee7bd191be073d7f8e37a288be', 2040);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (205, X'2e459056b1a1033f0c9dbb63bbe80dbfbd07f1dbf2b6dfbee62c7925c234913ebf6341636848a53e8b5d75fb4e55b3beca372b824fc79d3e5b526e1f24f4a0bedc499c23939200bf9db24e34720ef4be0688872744ffd93e4338a6535abf8f3e270eff79d15a9abe02bc7b9edfb094bec9e691eb1a3a93be9577e428bdf287be', 2050);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (206, X'6c52e9cbf78810bff6d52998c3f0fdbe930822692970ec3eb471da05fe2ca8be4220c809f32b8d3e31b421693a7bb2bee6ffb05b1a6d9f3eec472d55886e9fbee6a449092b9af0beb47bce372d4d003ff83b0a6e219cc73ed6da922f3580b7be0d879c45018994be5e3db99a332d94be4edab815517f92be0cdb1cdfcd5b86be', 2060);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (207, X'f8d3536c93d8f6be775ae95ee0cd0e3fe70897ef8012d33e91203ba7783bcbbe37cbfab54c539c3e80aef6487fdab1be48077f85daa29e3ed230c64f029da0bedc8145757548023f457713970078e53eb70536667debddbea5bb8525afc0a7bef9ba5f3f45bf87beb097994b108395be00e7e691334791be57cbea6896bc88be', 2070);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (208, X'f90d3b5a5add113f462e0aa798acea3e36d007398189ebbec69526580a71c1be878034306f46a63ecab4b72fed0bb3be0f7a2e4006eb9d3ec84efdff2d9da0be7e8b5807ff1ae03e8c6a6518f3c701bf803e2a5f4dffc0be3670e4f85ff0a43ee4aa52fe647e90bef0f73cd58a9295befb55078cdaa093beb900eb33179f88be', 2080);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (209, X'89c4055c2679db3ea936d58f559f11bf633d6fa8b54bb0be2e7d5be167b3a63e6bea83b453bb9f3ee2c3981f228db2be628f692318af9f3e9d8830b15803a0becdce9ede0e7e03bf6aafbcb12869cebee4bf57c62c46df3ece9ad3874a4192be9e418643f8ed9bbeeefaa264001595befece9f78318e92bee1e04f6e553986be', 2090);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (210, X'44beba69173212bf1b658264a438b53e933c24d1464cee3e9323ba17cbe1bbbebf66769e1165923eb663ca56fee4b1be7006ed46ed809e3ee4b556d5fc40a0bef75b68022692b13ed4f0aea971cb013fdc16143a88a5a6be257224205ddfb8bee2489d97fb9f90bed9e90459fac694be228f92f922ab91be4188d6498b2c89be', 2100);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (211, X'45e003f1ab02e53ecf9d7034820f103fd98a7f03fe66b8be2f66ee6d92ebcbbe824bb846d9d29e3e2e07865926a9b2bef686dcec91f89d3ec246f206bf26a0be82c075271414033f933083c89cdbd5be44eb7edd3228dfbe05c6556e3f7491be6fc17a19329d88be2c293a568bd295bedbeb4c38c94093be05f84bb9b89d88be', 2110);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (212, X'6efca454f3bb113f768ed8663525f2bee3ecb667e358ecbeb8ab9f01f3b9b2be39cf7a03fe81a63e42a32f0cd004b2be5c1f4e9656719e3e0694860d2d66a0be8f46a5c54bece3bed082f864b3cb01bf2c11a864c9b8b83eb9fb9a3fe28ba63e411647c5ad8494be0f86f048209d95be53557bdaa16d92bed0a1aebbeef986be', 2120);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (213, X'4870dbe3e403fbbe954a8520c66910bf9bd70c6fbd2ed93e83510dab0a9fa23e8d2ebf7fe888993e87b1da17a89fb2bece36a7254c0d9f3e0689db2036b49fbed4617f502b2f02bf1ba38f83c265ea3ea6079f163fe7dc3eaf6c331dffa1acbe7aa15f71f3e599be90f881c09d4995be0ba09d910e6b92be0e59821ddd5689be', 2130);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (214, X'409a442945df0fbf368afc486c1cff3e43070becc7d1e93ea7dc249438a5c5be0d8b29ebb4d4923e75f0980941b5b2bea15453b123769e3e4e7d1b845ba4a0be90da7840846ef23e7eaf044725dffe3eb0df6b798a74d0bef314ad15d38bb5bec7a27429a0878dbe3adb9d9c62c895be31800496f0bf92bee2f6d33fda7088be', 2140);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (215, X'4b0b07f72ad8043ff7e7745911b70a3fbeed0ea67a3bdebeb783679961a2c9bed28a58e47680a13e8ea3257f97f6b1be160005166ad49c3e96ee0887a4b4a0be5bd5a78b4da1ff3e013f7b54d6aff4be8b4f3f88a066dabea7128eccdf2a833eb3565ab8d3d688beb446ddeaeb0895be13d2b210a6dc91bed641a192d3c087be', 2150);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (216, X'a07a37ded78e0b3fc50cee810e5f07bf9c35e1450b4fe6beee4d7e508a6081be3350e13d7fc4a53e30d39f06be5eb2bed2388de7376f9e3e92952ada0b969ebe9119aa3e9ff6f9be81adec34aeeafbbec4cb6bbd5bc9d33e5abb6f158ed19f3e0a3e664bb35a97be88ed3b04869296be81118910afbb92be0179e8c9924389be', 2160);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (217, X'0496a02a3a740bbf672fd249724f07bf6f1210b7af1de83eb2278184e9bc8bbe94ab77877736933e277c9282d9c9b2be38ad7b79a6ff9f3ee6c8c9972998a0bedd8ba85b59dbf9be23faefc84a0ffb3efed56efaa9b6d33e6bcf3e78f6c2b5be937182f1fc3198beaabd5bd17cb495be0dd91d3cd79792beb5689275c7b288be', 2170);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (218, X'fe899cd9819504bf18a33e903eca0a3f5ef7a52e6e83e03e2bf1b487bf84c9becb8e5720b44c963e840ac9b8811ab2bebbc229d1c55c9c3e8fc9847303e0a0beb717e3da1ab5ff3ef01590f64caff33e0081c304ba6fdabeb91c5ad5086cafbec627d6d9d30687bee2c36adab68f94be3e3c53b3339091be995c2c5800be87be', 2180);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (219, X'a00bd339ac0f103f27743b7af700ff3eb8c2fa63e63ce8be6094e04f83f8c4be29c086d8b0eba33e3afa6a2008aab1be798fa94cd3069e3e7139e3f1cdb99dbe6527b328a452f23e38fe398197dbffbe25701ee8d04bd0be3e9636513042a03e275c5e0b01ba8bbef8f1d598975396be07e82211dd1792be10d116e565fb88be', 2190);
INSERT INTO almanac_nutation_model (model_id, coefficient_blob, sort_order) VALUES (220, X'e381348f4f5cfb3e9535b25ef96f10bf17d03dcb3e63d6be72302b0f8b308e3e9c552eba126c9b3e7530e187821eb7be096d2e01c6f8933ed87fc966bd0ca7bed8c7030e713102bfa49551b3b80eecbe6f66d10a4ef7dc3e7169b7e78d41723e50b2a06d74c596be9d100668fe9793be49ab5504ceca90bed901d34975e687be', 2200);

INSERT INTO almanac_fixed_equatorial_model (body_id, coefficient_blob)
VALUES
    (10, X'6fd575a8a6c40740365b79c9ff2644c052b81e85eb714ac07b14ae47e1fa3540'),
    (11, X'7afeb4519d0efa3f6d1d1cec4d9e4cc00000000000c055401f85eb51b81e43c0'),
    (12, X'395fecbdf8e228409b215514af8c4fc00ad7a3703dea41c0b81e85eb51b82dc0'),
    (13, X'2f14b01d8ce81b4043e6caa0daf83cc0ec51b81e85eb094048e17a14ae47f53f'),
    (14, X'2b6c06b82023364035272f32017b47c05c8fc2f528ac5f40d7a3703d0a6f62c0'),
    (15, X'b8567bd80b65124093c9a99d618230409a99999999b94f40ae47e17a149e67c0'),
    (16, X'000341800ccd29406c79e57adbfa4b400ad7a3703dfa5b407b14ae47e17a20c0'),
    (17, X'185dde1cae952b4041f50f2219a848407b14ae47e14a5ec052b81e85ebd12dc0'),
    (18, X'6bd3d85e0b6a16406f0ed76a0f3bf3bf0ad7a3703d0af73ff6285c8fc2f5e8bf'),
    (19, X'410e4a9869eb22408c4aea04345121c0f6285c8fc2752ec08fc2f5285c2f4140'),
    (20, X'47acc5a700282f4017f19d98f5b63a4017d9cef753bb5d402fdd240681ed55c0'),
    (21, X'b9a81611c5e4c13f698b6b7c26173d401f85eb51b82e6140ae47e17a146e64c0'),
    (22, X'13ba4be2acd833402bdcf29194bc2140a4703d0ad7c18040713d0ad7a3147840'),
    (23, X'45f5d6c05609dc3f2600ff942a2745c09a99999999216d40cdcccccccc4476c0'),
    (24, X'98da5207797d30401ec6a4bf976e3ac0b81e85eb513828c0cdcccccccc4c37c0'),
    (25, X'2f34d769a4852c40fe7e315bb22e3340c3f5285c8f1591c00ad7a3703d409fc0'),
    (26, X'd05fe811a3cf30403b6d8d08c64151c03d0ad7a370fd314014ae47e17a943fc0'),
    (27, X'4ad3a0681ec02040eaea8ec536c14dc085eb51b81e8539c08fc2f5285c0f3640'),
    (28, X'6c96cb46e7ac15409a40118b18661940b81e85eb513820c0c3f5285c8fc229c0'),
    (29, X'f3e49a0299ad17400a849d62d5a01d400ad7a3703d8a3b409a99999999992640'),
    (30, X'2fa52e19c7981940240d6e6b0b594ac0ae47e17a14ee33403d0ad7a3703d3740'),
    (31, X'22e010aad41c1540e659492bbeff46400000000000d052400ad7a3703dae7ac0'),
    (32, X'6bd784b4c6b034408c65fa25e2a3464014ae47e17a1400409a9999999999fd3f'),
    (33, X'e76d6c76a4a2274091d442c9e4242d407b14ae47e11a7fc07b14ae47e1aa5cc0'),
    (34, X'bdc799266c3fe73f9109f83592fc31c09a99999999116d403d0ad7a370fd3f40'),
    (35, X'b4226aa2cf1f2640609335ea21e04e40ec51b81e85c360c09a999999995941c0'),
    (36, X'c0b0fcf9b6c01540c9e369f9819b3c40c3f5285c8fc23640c3f5285c8fb265c0'),
    (37, X'7ade8d0585f131409692e52494be4940f6285c8fc2f520c00ad7a3703dca36c0'),
    (38, X'56babbce86bc35404d69fd2d01c02340ec51b81e85eb3a40295c8fc2f528dc3f'),
    (39, X'0f7ee200faf53640745c8dec4a9f3dc033333333338f74403d0ad7a3709564c0'),
    (40, X'24ed461ff3092940b43d7ac37d8e4cc07b14ae47e13a3c40e17a14ae479170c0'),
    (41, X'992d5915e1862840a6b8aaecbb8a31c0ec51b81e85d363c05c8fc2f528dc3540'),
    (42, X'ccf09f6ea0202c40b6a1629cbf2f4ec0c3f5285c8fa240c0295c8fc2f52837c0'),
    (43, X'1f10e84cdaf40040ec14ab06617637409a99999999916740c3f5285c8f8262c0'),
    (44, X'9babe6392267324059c2da183b3141c0f6285c8fc2b543c0cdcccccccc0c5fc0'),
    (45, X'715af0a2afb02d406b990cc7f3895240ae47e17a144e40c0d7a3703d0ad72640'),
    (46, X'c50089265014374064e597c118692e403333333333334e406666666666a644c0'),
    (47, X'c0d02346cf4d0840f6b3588ae45b104052b81e85ebd124c066666666663653c0'),
    (48, X'd93f4f0306392c40effe78af5a2f42c00ad7a3703d4480c014ae47e17a3080c0'),
    (49, X'e8a1b60da37022407e1b62bce66d51c0d7a3703d0a8f63c0cdcccccccc3c5b40'),
    (50, X'b45bcb64383e0b4041800c1d3bee48400000000000c037407b14ae47e13a3ac0'),
    (51, X'd3a3a99ecceb324035f0a31af64b3ac048e17a14ae472e40d7a3703d0ab74ac0'),
    (52, X'2159c0046e6d3440eeceda6d175e4cc09a99999999991b40e17a14ae478155c0'),
    (53, X'fe800706103e044013656f29e75056403d0ad7a3703d46403333333333b327c0'),
    (54, X'74620fed63051f401a3048fab4063c4066666666669483c06666666666e646c0'),
    (55, X'fd3383f8c09e1e4044f8174163e614401f85eb51b85486c033333333333390c0'),
    (56, X'8e5bcccf0d95314093196f2bbd1e294014ae47e17a045b400ad7a3703db26bc0'),
    (57, X'6d8e739b7047244032cb9e0436ef27408fc2f5285c176fc05c8fc2f5285c1640'),
    (58, X'7a8b87f71cf81440ff428f183d6720c0f6285c8fc2f5f43f000000000000e03f'),
    (59, X'55c2137afd512d40e50b5a48c06a4ec00000000080beacc01f85eb51b89a7d40'),
    (60, X'c93846b2472c31406f48a30227732fc0713d0ad7a31044407b14ae47e1ca5840'),
    (61, X'87e0b88c9b9ae53fa0185932c7444c40e3a59bc420904840b81e85eb51983fc0'),
    (62, X'f60ce198658f31401c45d61a4a8d42c08fc2f5285c0f21c0cdcccccccccc3ec0'),
    (63, X'01f8a75489021b407024d06053b730c0ae47e17a141081c0e17a14ae471c93c0'),
    (64, X'f14a92e7fad62a4065726a67985226c0cdcccccccc2c45c0ec51b81e85ab3ec0'),
    (65, X'6556ef703b442240a0c552245fb745c0c3f5285c8f0238c00ad7a3703d0a2b40'),
    (66, X'abd0402c9b9d3240139ed0eb4f644340ae47e17a141e694048e17a14aee37140'),
    (67, X'f69507e929b22d4099b9c0e5b10a30c0ec51b81e856b5ac09a999999991951c0'),
    (68, X'a5bc564277253540f0a7c64b373d56c0a69bc420b0523a40fca9f1d24de21240');

COMMIT;
