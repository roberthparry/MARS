-- Native Lab form-reference work budgets. Timed calculations must not run while restoring controls.
CREATE TABLE IF NOT EXISTS mars_lab_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS mars_lab_cases_operation ON mars_lab_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.numeric_callables', 'tools/mars_lab/tests/test_lab_numeric.c',
    'tests.test_lab_numeric_callables', 'mars_lab_cases', 'numeric_callable')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('powmod_small', 'numeric_callable', 'powmod(4,13,497)', '445'),
    ('powmod_original_1024_bit_operands', 'numeric_callable', 'powmod(97128534091520311412078115857277804694847051212062325796074342369690095336188259720191897149915705664060543226969142780725822337496727884659619483452400511633617514882293623272276764857474830401626415841035971458657442133370777607803464071101158385124186229995073045427448918227959893082282919863982307657776,12498846048367045926370716602609982839721641429222959125376462582674869403443996972648628699047686839582654905384579341977328653591547736951147466435189662,6864797660130609714981900799081393217269435300143305409394463459185543183397656052122559640661454554977296311391480858037121987999716643812574028291115057151)', '2426708632600750408239409365812084203045954767825752904850432903124881973671736995843975761420957307038639263776889805180044277291952781094657794927246068639'),
    ('integer_test_bit', 'numeric_callable', 'test_bit(2^701,701)', '1'),
    ('integer_prove_prime', 'numeric_callable', 'prove_prime(97)', '1'),
    ('integer_divmod_quotient', 'numeric_callable', 'divmod(17,5,0)', '3'),
    ('integer_divmod_remainder', 'numeric_callable', 'divmod(17,5,1)', '2'),
    ('integer_gcdext_gcd', 'numeric_callable', 'gcdext(30,21,0)', '3'),
    ('alias_pow10', 'numeric_callable', 'pow10(3)', '1000'),
    ('alias_sincos_cosine', 'numeric_callable', 'sincos(0,1)', '1'),
    ('powmod_binding', 'numeric_callable', '{x | x=powmod(4,13,497)}', '445');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.integer_family_plots', 'tools/mars_lab/tests/test_lab_integer_plot.c',
    'tests.test_lab_integer_family_plots', 'mars_lab_cases', 'integer_family_plot')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('integer_plot_imaginary_unit', 'integer_family_plot', '{"expression":"i^x=2"}', '11'),
    ('integer_plot_seeded', 'integer_family_plot', '{"expression":"{i^x=2 | x=-0.44i}"}', '11'),
    ('integer_plot_negative_unit', 'integer_family_plot', '{"expression":"(-i)^x=2"}', '11'),
    ('integer_plot_negative_base', 'integer_family_plot', '{"expression":"(-2)^x=3"}', '11'),
    ('integer_plot_cartesian_base', 'integer_family_plot', '{"expression":"(1+i)^x=2"}', '11'),
    ('integer_plot_affine_exponent', 'integer_family_plot', '{"expression":"i^(2*x+1)=2"}', '11'),
    ('integer_plot_real_family', 'integer_family_plot', '{"expression":"tan(x)=0"}', '11'),
    ('integer_plot_origin_view', 'integer_family_plot',
     '{"expression":"i^x=2","plot_lower":"-1","plot_upper":"1","plot_scale":"manual","plot_y_lower":"-1","plot_y_upper":"0"}', '1'),
    ('integer_plot_distant_view', 'integer_family_plot',
     '{"expression":"i^x=2","plot_lower":"36","plot_upper":"44","plot_scale":"manual","plot_y_lower":"-1","plot_y_upper":"0"}', '3'),
    ('integer_plot_empty_view', 'integer_family_plot',
     '{"expression":"i^x=2","plot_lower":"-1","plot_upper":"1","plot_scale":"manual","plot_y_lower":"1","plot_y_upper":"2"}', '0'),
    ('integer_plot_dense_view', 'integer_family_plot',
     '{"expression":"i^x=2","plot_lower":"-1000000","plot_upper":"1000000","plot_scale":"manual","plot_y_lower":"-1","plot_y_upper":"0"}', 'bounded'),
    ('integer_plot_index_limit', 'integer_family_plot',
     '{"expression":"i^x=2","plot_lower":"4e18","plot_upper":"4.000000000000004e18","plot_scale":"manual","plot_y_lower":"-1","plot_y_upper":"0"}', 'limited');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.form_budget', 'tools/mars_lab/tests/test_lab_forms.c',
    'tests.test_lab_forms_calculation_budget', 'mars_lab_cases', 'form_budget')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('metadata_semiprime_budget', 'form_budget',
     '{"action":"integrator","text":"free x\nfree y","expression":"factors(1000000016000000063)"}', 'false'),
    ('metadata_partial_factor_budget', 'form_budget',
     '{"action":"integrator","text":"x=0..1","expression":"factors(8000000128000000504)"}', 'false'),
    ('metadata_large_smooth_integer', 'form_budget',
     '{"action":"integrator","text":"x=0..1","expression":"factors(2^512*3^256*5^128)"}', 'true');

INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES ('metadata_saved_factor_integer', 'form_budget', '{"action":"integrator","text":"x=0..1","expression":"factors(54772119019154636339867862041163448790974484131459861520687327809209943398360125227595479954148677571112600596738260011818279723370813377533578833658037065081315690622999760330413021898332157429926530638873392183094671315489619988111360000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)"}', 'false');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.mobile_tailnet', 'tools/mars_lab/tests/test_lab_mobile.c',
    'tests.test_lab_mobile_tailnet', 'mars_lab_cases', 'mobile_tailnet')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('mobile_localhost_https', 'mobile_tailnet',
     '{"host":"localhost:8765","allowed":false,
       "status":{"BackendState":"Running","Self":{"Online":true,"DNSName":"machine.example.ts.net."}},
       "serve":{"TCP":{"443":{"HTTPS":true}},"Web":{"machine.example.ts.net:443":{"Handlers":{"/":{"Proxy":"http://127.0.0.1:8765"}}}}}}',
     'https://machine.example.ts.net/'),
    ('mobile_exact_tailnet_host', 'mobile_tailnet',
     '{"host":"machine.example.ts.net:443","allowed":true,
       "status":{"BackendState":"Running","Self":{"Online":true,"DNSName":"machine.example.ts.net."}},
       "serve":{"TCP":{"443":{"HTTPS":true}},"Web":{"machine.example.ts.net:443":{"Handlers":{"/":{"Proxy":"http://127.0.0.1:8765"}}}}}}',
     'https://machine.example.ts.net/'),
    ('mobile_spoofed_tailnet_host', 'mobile_tailnet',
     '{"host":"other.example.ts.net","allowed":false,
       "status":{"BackendState":"Running","Self":{"Online":true,"DNSName":"machine.example.ts.net."}},
       "serve":{"TCP":{"443":{"HTTPS":true}},"Web":{"machine.example.ts.net:443":{"Handlers":{"/":{"Proxy":"http://127.0.0.1:8765"}}}}}}',
     'reject'),
    ('mobile_offline_node', 'mobile_tailnet',
     '{"host":"machine.example.ts.net","allowed":false,
       "status":{"BackendState":"Running","Self":{"Online":false,"DNSName":"machine.example.ts.net."}},
       "serve":{"TCP":{"443":{"HTTPS":true}},"Web":{"machine.example.ts.net:443":{"Handlers":{"/":{"Proxy":"http://127.0.0.1:8765"}}}}}}',
     'reject'),
    ('mobile_stopped_tailscale', 'mobile_tailnet',
     '{"host":"machine.example.ts.net","allowed":false,
       "status":{"BackendState":"Stopped","Self":{"Online":true,"DNSName":"machine.example.ts.net."}},
       "serve":{"TCP":{"443":{"HTTPS":true}},"Web":{"machine.example.ts.net:443":{"Handlers":{"/":{"Proxy":"http://127.0.0.1:8765"}}}}}}',
     'reject'),
    ('mobile_public_funnel_rejected', 'mobile_tailnet',
     '{"host":"machine.example.ts.net","allowed":false,
       "status":{"BackendState":"Running","Self":{"Online":true,"DNSName":"machine.example.ts.net."}},
       "serve":{"AllowFunnel":{"machine.example.ts.net:443":true},"TCP":{"443":{"HTTPS":true}},"Web":{"machine.example.ts.net:443":{"Handlers":{"/":{"Proxy":"http://127.0.0.1:8765"}}}}}}',
     'reject'),
    ('mobile_wrong_proxy_port', 'mobile_tailnet',
     '{"host":"localhost:8765","allowed":false,
       "status":{"BackendState":"Running","Self":{"Online":true,"DNSName":"machine.example.ts.net."}},
       "serve":{"TCP":{"443":{"HTTPS":true}},"Web":{"machine.example.ts.net:443":{"Handlers":{"/":{"Proxy":"http://127.0.0.1:8766"}}}}}}',
     'reject');
