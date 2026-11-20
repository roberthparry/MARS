-- Native three-dimensional axis layout inputs; ownership and SVG checks remain in C.
CREATE TABLE IF NOT EXISTS plot_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 64 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS plot_cases_operation ON plot_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'plot.occlusion', 'tests/plot/test_plot.c', 'tests.test_plot_occlusion', 'plot_cases', 'occlusion')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO plot_cases(name, operation, input, expected) VALUES
    ('occlusion_empty', 'occlusion', '[[[0,0,0.2],[10,0,0.2],[10,10,0.2],[0,10,0.2]],[]]', '100'),
    ('occlusion_front', 'occlusion',
     '[[[0,0,0.2],[10,0,0.2],[10,10,0.2],[0,10,0.2]],[[[0,0,0.4],[10,0,0.4],[10,10,0.4]],[[0,0,0.4],[10,10,0.4],[0,10,0.4]]]]', '0'),
    ('occlusion_behind', 'occlusion',
     '[[[0,0,0.2],[10,0,0.2],[10,10,0.2],[0,10,0.2]],[[[0,0,0.1],[10,0,0.1],[10,10,0.1]],[[0,0,0.1],[10,10,0.1],[0,10,0.1]]]]', '100'),
    ('occlusion_overlap', 'occlusion',
     '[[[0,0,0.2],[10,0,0.2],[10,10,0.2],[0,10,0.2]],[[[0,0,0.4],[5,0,0.4],[5,10,0.4]],[[0,0,0.4],[5,10,0.4],[0,10,0.4]],[[0,0,0.4],[5,0,0.4],[5,10,0.4]]]]', '50'),
    ('occlusion_depth_crossing', 'occlusion',
     '[[[0,0,0.2],[10,0,0.2],[10,10,0.2],[0,10,0.2]],[[[0,0,0.1],[10,0,0.3],[10,10,0.3]],[[0,0,0.1],[10,10,0.3],[0,10,0.1]]]]', '50'),
    ('occlusion_hole', 'occlusion',
     '[[[0,0,0.2],[10,0,0.2],[10,10,0.2],[0,10,0.2]],[[[0,0,0.4],[3,0,0.4],[3,10,0.4]],[[0,0,0.4],[3,10,0.4],[0,10,0.4]],[[7,0,0.4],[10,0,0.4],[10,10,0.4]],[[7,0,0.4],[10,10,0.4],[7,10,0.4]]]]', '40');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'plot.surface_origin', 'tests/plot/test_plot.c', 'tests.test_plot_surface_origin', 'plot_cases', 'surface_origin')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO plot_cases(name, operation, input, expected) VALUES
    ('surface_origin_symmetric', 'surface_origin', '["-2","2","-3","3","-4","4"]', '0.5 0.5 0.5'),
    ('surface_origin_asymmetric', 'surface_origin', '["-2","6","-3","9","-4","12"]', '0.25 0.25 0.25'),
    ('surface_origin_positive', 'surface_origin', '["1","3","2","6","4","8"]', '-0.5 -0.5 -1'),
    ('surface_origin_negative', 'surface_origin', '["-3","-1","-6","-2","-8","-4"]', '1.5 1.5 2'),
    ('surface_origin_mixed', 'surface_origin', '["-2","6","2","6","-8","-4"]', '0.25 -0.5 2'),
    ('surface_origin_extreme', 'surface_origin',
     '["-1e400","3e400","-1e400","3e400","-1e400","3e400"]', '0.25 0.25 0.25');

-- Preserve case identities and selections when the frame presentation changes.
UPDATE plot_cases SET expected='outside_connected' WHERE operation='surface_origin';

INSERT INTO plot_cases(name, operation, input, expected) VALUES
    ('surface_origin_centred_symmetric', 'surface_origin', '["-2","2","-3","3","-4","4"]',
     'origin 0.5 0.5 0.5'),
    ('surface_origin_centred_asymmetric', 'surface_origin', '["-1","3","-3","5","-4","4"]',
     'origin 0.25 0.375 0.5'),
    ('surface_origin_centred_extreme', 'surface_origin', '["-1e400","3e400","-1e400","3e400","-1e400","3e400"]',
     'origin 0.25 0.25 0.25'),
    ('surface_origin_centred_torus', 'surface_origin', '["-16","16","-16","16","-4","4"]',
     'origin 0.5 0.5 0.5')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'plot.perspective', 'tests/plot/test_plot.c', 'tests.test_plot_perspective', 'plot_cases', 'perspective')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO plot_cases(name, operation, input, expected) VALUES
    ('perspective_near', 'perspective', '[1,1,-1]', '[133.33333333333333,133.33333333333333,1.6666666666666667]'),
    ('perspective_centre', 'perspective', '[1,1,0]', '[100,100,1.25]'),
    ('perspective_far', 'perspective', '[1,1,1]', '[80,80,1]'),
    ('perspective_tick_first', 'perspective', '[-0.5,0,-0.5]', '[-57.14285714285714,0,1.4285714285714286]'),
    ('perspective_tick_last', 'perspective', '[0.5,0,0.5]', '[44.44444444444444,0,1.1111111111111112]');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'plot.axis_legibility', 'tests/plot/test_plot.c', 'tests.test_plot_axis_legibility', 'plot_cases', 'axis_legibility')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Face-on pinhole boxes: every scale must span 170 pixels and joined lines need 90 pixels of perpendicular clearance.
INSERT OR IGNORE INTO plot_cases(name, operation, input, expected) VALUES
    ('axis_legibility_0', 'axis_legibility', '[[709.131590697,740.000000000],[190.868409303,740.000000000],[797.591201255,721.759403134],[102.408798745,721.759403134],[698.002710994,238.637698196],[201.997289006,238.637698196],[777.856642512,60.000000000],[122.143357488,60.000000000]]', '[170,90]'),
    ('axis_legibility_1', 'axis_legibility', '[[698.002710994,561.362301804],[201.997289006,561.362301804],[777.856642512,740.000000000],[122.143357488,740.000000000],[709.131590697,60.000000000],[190.868409303,60.000000000],[797.591201255,78.240596866],[102.408798745,78.240596866]]', '[170,90]'),
    ('axis_legibility_2', 'axis_legibility', '[[102.408798745,721.759403134],[797.591201255,721.759403134],[190.868409303,740.000000000],[709.131590697,740.000000000],[122.143357488,60.000000000],[777.856642512,60.000000000],[201.997289006,238.637698196],[698.002710994,238.637698196]]', '[170,90]'),
    ('axis_legibility_3', 'axis_legibility', '[[122.143357488,740.000000000],[777.856642512,740.000000000],[201.997289006,561.362301804],[698.002710994,561.362301804],[102.408798745,78.240596866],[797.591201255,78.240596866],[190.868409303,60.000000000],[709.131590697,60.000000000]]', '[170,90]');

-- The same near-cardinal views with unequal coordinate spans, matching the distorted ellipsoid example.
INSERT OR IGNORE INTO plot_cases(name, operation, input, expected) VALUES
    ('axis_legibility_4', 'axis_legibility', '[[648.683781346,581.036289305],[151.316218654,581.036289305],[725.000000000,561.623206300],[75.000000000,561.623206300],[638.796427453,198.691305530],[161.203572547,198.691305530],[708.316581856,68.963710695],[91.683418144,68.963710695]]', '[120,90]'),
    ('axis_legibility_5', 'axis_legibility', '[[638.796427453,451.308694470],[161.203572547,451.308694470],[708.316581856,581.036289305],[91.683418144,581.036289305],[648.683781346,68.963710695],[151.316218654,68.963710695],[725.000000000,88.376793700],[75.000000000,88.376793700]]', '[120,90]');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'plot.tick_density', 'tests/plot/test_plot.c', 'tests.test_plot_tick_density', 'plot_cases', 'tick_density')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'plot.axis_stability', 'tests/plot/test_plot.c', 'tests.test_plot_axis_stability', 'plot_cases', 'axis_stability')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Rock the camera by 0.02 degrees around each pose; a readable frame must keep its reference edges.
INSERT OR IGNORE INTO plot_cases(name, operation, input, expected) VALUES
    ('axis_stability_north', 'axis_stability', '[0,30,0]', '0'),
    ('axis_stability_east', 'axis_stability', '[90,30,0]', '0'),
    ('axis_stability_south', 'axis_stability', '[180,30,0]', '0'),
    ('axis_stability_west', 'axis_stability', '[270,30,0]', '0');

-- Inputs are [zero index, [[screen x, screen y, perspective size], ...]]; expected values are stroke bit masks.
INSERT OR IGNORE INTO plot_cases(name, operation, input, expected) VALUES
    ('tick_density_long', 'tick_density', '[2,[[0,0,1],[25,0,1],[50,0,1],[75,0,1],[100,0,1]]]', '31'),
    ('tick_density_short_readable', 'tick_density', '[2,[[0,0,1],[10,0,1],[20,0,1],[30,0,1],[40,0,1]]]', '31'),
    ('tick_density_reverse', 'tick_density', '[2,[[40,0,1],[30,0,1],[20,0,1],[10,0,1],[0,0,1]]]', '31'),
    ('tick_density_crowded', 'tick_density', '[2,[[0,0,1],[4,0,1],[8,0,1],[12,0,1],[16,0,1]]]', '21'),
    ('tick_density_perspective', 'tick_density', '[2,[[0,0,1],[12,0,1.5],[24,0,2],[36,0,2.5],[48,0,3]]]', '21'),
    ('tick_density_collapsed', 'tick_density', '[2,[[0,0,1],[1,0,1],[2,0,1],[3,0,1],[4,0,1]]]', '4'),
    ('tick_density_without_zero', 'tick_density', '[5,[[0,0,1],[4,0,1],[8,0,1],[12,0,1],[16,0,1]]]', '21');
