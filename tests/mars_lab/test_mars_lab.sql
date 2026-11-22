-- Native Lab form-reference work budgets. Timed calculations must not run while restoring controls.
CREATE TABLE IF NOT EXISTS mars_lab_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS mars_lab_cases_operation ON mars_lab_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.progression_plots', 'tools/mars_lab/tests/test_lab_evaluation_extra.c',
    'tests.test_lab_extra_progression_plots', 'mars_lab_cases', 'progression_plot')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('progression_plot_sine_literal', 'progression_plot', '{"expression":"@Z_(k=1)^100 sin(kx)","action":"plot","wrt":"x","precision":40}', '20'),
    ('progression_plot_cosine_literal', 'progression_plot', '{"expression":"@Z_(k=1)^100 cos(kx)","action":"plot","wrt":"x","precision":40}', '20'),
    ('progression_plot_sine_bound', 'progression_plot', '{"expression":"{@Z_(k=1)^n sin(kx)|n=100}","action":"plot","wrt":"x","precision":40}', '20')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('progression_plot_sine_500', 'progression_plot', '{"expression":"@Z_(k=1)^500 sin(kx)","action":"plot","wrt":"x","precision":97}', '20'),
    ('progression_plot_cosine_500', 'progression_plot', '{"expression":"@Z_(k=1)^500 cos(kx)","action":"plot","wrt":"x","precision":97}', '20'),
    ('progression_plot_bound_500', 'progression_plot', '{"expression":"{@Z_(k=1)^n sin(kx)|n=500}","action":"plot","wrt":"x","precision":97}', '20')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('progression_plot_sine_1000', 'progression_plot', '{"expression":"@Z_(k=1)^1000 sin(kx)","action":"plot","wrt":"x","precision":97}', '20'),
    ('progression_plot_ellipsis_1000', 'progression_plot', '{"expression":"sin(x)+sin(2x)+sin(3x)+...+sin(1000x)","action":"plot","wrt":"x","precision":97}', '20'),
    ('progression_plot_cosine_1000', 'progression_plot', '{"expression":"@Z_(k=1)^1000 cos(kx)","action":"plot","wrt":"x","precision":97}', '20'),
    ('progression_plot_bound_1000', 'progression_plot', '{"expression":"{@Z_(k=1)^n sin(kx)|n=1000}","action":"plot","wrt":"x","precision":97}', '20'),
    ('progression_plot_closed_1000', 'progression_plot', '{"expression":"sin(500*x)*sin(1001*x/2)/sin(x/2)","action":"plot","wrt":"x","precision":97}', '20'),
    ('progression_plot_renamed_1000', 'progression_plot', '{"expression":"@Z_(k=1)^1000 sin(kt)","action":"plot","wrt":"t","precision":97}', '20')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.plot_resolution', 'tools/mars_lab/tests/test_lab_evaluation_extra.c',
    'tests.test_lab_extra_plot_resolution', 'mars_lab_cases', 'plot_resolution')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('plot_resolution_sine_sum', 'plot_resolution', '{"expression":"@Z_(k=1)^1000 sin(kx)","action":"plot","wrt":"x","plot_lower":"-10","plot_upper":"10","precision":97}', 'Plot sampling budget exhausted'),
    ('plot_resolution_cosine', 'plot_resolution', '{"expression":"cos(1000*x)","action":"plot","wrt":"x","plot_lower":"-10","plot_upper":"10","precision":97}', 'Plot sampling budget exhausted')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.progression_samples', 'tools/mars_lab/tests/test_lab_evaluation_extra.c',
    'tests.test_lab_extra_progression_samples', 'mars_lab_cases', 'progression_sample')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO mars_lab_cases(name, operation, input, expected, precision) VALUES
    ('progression_sample_sine_zero_double', 'progression_sample', '{"expression":"@Z_(k=1)^500 sin(kx)","point":"0"}', '0', 53),
    ('progression_sample_sine_zero_precise', 'progression_sample', '{"expression":"@Z_(k=1)^500 sin(kx)","point":"0"}', '0', 384),
    ('progression_sample_cosine_zero_double', 'progression_sample', '{"expression":"@Z_(k=1)^500 cos(kx)","point":"0"}', '500', 53),
    ('progression_sample_cosine_zero_precise', 'progression_sample', '{"expression":"@Z_(k=1)^500 cos(kx)","point":"0"}', '500', 384),
    ('progression_sample_sinh_zero', 'progression_sample', '{"expression":"@Z_(k=1)^500 sinh(kx)","point":"0"}', '0', 384),
    ('progression_sample_cosh_zero', 'progression_sample', '{"expression":"@Z_(k=1)^500 cosh(kx)","point":"0"}', '500', 384),
    ('progression_sample_exp_zero', 'progression_sample', '{"expression":"@Z_(k=1)^500 exp(kx)","point":"0"}', '500', 384),
    ('progression_sample_tiny_sine', 'progression_sample', '{"expression":"@Z_(k=1)^500 sin(kx)","point":"1e-60"}', '1.2525e-55', 384),
    ('progression_sample_empty', 'progression_sample', '{"expression":"@Z_(k=1)^0 sin(kx)","point":"1"}', '0', 384),
    ('progression_sample_negative_upper', 'progression_sample', '{"expression":"@Z_(k=1)^(-1) sin(kx)","point":"1"}', '0', 384),
    ('progression_sample_noninteger_upper', 'progression_sample', '{"expression":"@Z_(k=1)^(3/2) sin(kx)","point":"1"}', 'NaN', 384),
    ('progression_sample_coordinate_upper', 'progression_sample', '{"expression":"{@Z_(k=1)^n sin(kx)|x=1}","axis":"n","point":"3/2"}', 'NaN', 384)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.equation_pairs', 'tools/mars_lab/tests/test_lab_evaluation_extra.c',
    'tests.test_lab_extra_equation', 'mars_lab_cases', 'equation_pair')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_pair_scaled_ellipsoid', 'equation_pair', '{"equation":"x^2/9+y^2/16+z^2/25=1","precision":40}', '{"pairs":"3"}'),
    ('equation_pair_scaled_closed_root', 'equation_pair', '{"equation":"x^2/9=2","precision":40}', '{"pairs":"1"}'),
    ('equation_pair_shifted_ellipse', 'equation_pair', '{"equation":"(x-2)^2/9+y^2/16=1","precision":40}', '{"pairs":"2"}'),
    ('equation_pair_exact_roots', 'equation_pair', '{"equation":"x^2=4","precision":40}', '{"pairs":"1"}'),
    ('equation_pair_decimal_tapered', 'equation_pair', '{"equation":"x^2/784+(y^2+z^2)/484*(1+0.01*x)=1","precision":101}', '{"pairs":"3","max_TeX":"1200","required":"\\sqrt{\\frac","forbidden":"0.008099173553719"}')
ON CONFLICT(name) DO UPDATE SET expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.history_previews', 'tools/mars_lab/tests/test_lab_presentation.c',
    'tests.test_lab_presentation_history', 'mars_lab_cases', 'history_preview')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('history_authored_addition', 'history_preview', '{"action":"history","text":"2+3"}', '{"display":"2+3","contains":"2 + 3","excludes":"5"}'),
    ('history_equation', 'history_preview', '{"action":"history","text":"x^2=2"}', '{"display":"x^2=2","contains":"x^{2}"}'),
    ('history_decimal_binding', 'history_preview', '{"action":"history","text":"{x*exp(x)=2 | x=0.852605501372345678901234567890123456789}"}', '{"display":"{x*exp(x)=2 | x=0.8526055013723456...}","contains":"\\ldots{}","excludes":"901234567890123456789"}'),
    ('history_long_integer', 'history_preview', '{"action":"history","text":"123456789012345678901234567890+x"}', '{"display":"123456789012345678...+x","contains":"\\ldots{}","excludes":"123456789012345678901234567890"}'),
    ('history_scientific', 'history_preview', '{"action":"history","text":"1.234567890123456789012345678901e-40*x"}', '{"display":"1.2345678901234567...e-40*x","contains":"10^{-40}"}'),
    ('history_unset_binding', 'history_preview', '{"action":"history","text":"{x+C | x=?, C=3}"}', '{"display":"{x+C | x=?, C=3}","contains":"?"}'),
    ('history_factorial', 'history_preview', '{"action":"history","text":"factorial(500)"}', '{"display":"factorial(500)","contains":"\\operatorname{factorial}(500)","excludes":"122013682599"}'),
    ('history_calculus', 'history_preview', '{"action":"history","text":"integral(sin(x),x)"}', '{"display":"integral(sin(x),x)","contains":"\\int"}'),
    ('history_factor_request', 'history_preview', '{"action":"history","text":"factors(2^512*3^256*5^128)"}', '{"display":"factor(2^512*3^256*5^128)","contains":"\\operatorname{factor}(2^{512}","excludes":"a_{0}"}'),
    ('history_factor_bound', 'history_preview', '{"action":"history","text":"{ factors(n) | n=2^512*3^256*5^128 }"}', '{"display":"factor(n)","contains":"\\operatorname{factor}(n)","excludes":"a_{0}"}'),
    ('history_factor_parentheses', 'history_preview', '{"action":"history","text":"(( factors (84) ))"}', '{"display":"factor(84)","contains":"\\operatorname{factor}(84)"}'),
    ('history_factor_label', 'history_preview', '{"action":"history","text":"factor(84)"}', '{"display":"factor(84)","contains":"\\operatorname{factor}(84)"}'),
    ('history_factor_digits', 'history_preview', '{"action":"history","text":"factors(123456789012345678901234567890123456789012345678901234567890)"}', '{"display":"factor(123456789012345678...)","contains":"\\operatorname{factor}(123456789012345678901234567890123456789012345678901234567890)","fit":true}'),
    ('history_factor_nested', 'history_preview', '{"action":"history","text":"factors(84)+x"}', '{"display":"factors(84)+x","contains":"factors","excludes":"a_{0}"}'),
    ('history_quoted_markup', 'history_preview', '{"action":"history","text":"<img src=x onerror=alert(1)>"}', '{"display":"<img src=x onerror=alert(1)>","literal":true}'),
    ('history_incomplete', 'history_preview', '{"action":"history","text":"sin("}', '{"display":"sin(","literal":true}'),
    ('history_long_identifier', 'history_preview', '{"action":"history","text":"x123456789012345678901234567890+1"}', '{"display":"x123456789012345678901234567890+1","contains":"x"}')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation, input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.calculation_times', 'tools/mars_lab/tests/test_lab_presentation.c',
    'tests.test_lab_presentation_calculation_time', 'mars_lab_cases', 'calculation_time')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('calculation_time_zero', 'calculation_time', '{"calculation_time_ns":"0"}', '0 ns'),
    ('calculation_time_nanoseconds', 'calculation_time', '{"calculation_time_ns":"999"}', '999 ns'),
    ('calculation_time_microsecond', 'calculation_time', '{"calculation_time_ns":"1000"}', '1.0 µs'),
    ('calculation_time_microseconds', 'calculation_time', '{"calculation_time_ns":"12345"}', '12.3 µs'),
    ('calculation_time_millisecond', 'calculation_time', '{"calculation_time_ns":"1000000"}', '1.0 ms'),
    ('calculation_time_second', 'calculation_time', '{"calculation_time_ns":"1000000000"}', '1.00 s'),
    ('calculation_time_seconds', 'calculation_time', '{"calculation_time_ns":"1234567890"}', '1.23 s'),
    ('calculation_time_absent', 'calculation_time', '{}', ''),
    ('calculation_time_negative', 'calculation_time', '{"calculation_time_ns":"-1"}', ''),
    ('calculation_time_nonfinite', 'calculation_time', '{"calculation_time_ns":"NaN"}', ''),
    ('calculation_time_overflow', 'calculation_time', '{"calculation_time_ns":"18446744073709551616"}', ''),
    ('calculation_time_trailing_junk', 'calculation_time', '{"calculation_time_ns":"12 ns"}', '');

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

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.equation_parameters', 'tools/mars_lab/tests/test_lab_equation_parameters.c',
    'tests.test_lab_equation_parameters', 'mars_lab_cases', 'equation_parameters')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_radical_cubic_display', 'equation_parameters',
     '{"equation":"x^3-3*sqrt(3)*x^2-3*x+sqrt(3)=0","precision":70}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"x &= \\sqrt{3} +","display_excludes":"\\sqrt[3]{","bindings":{"x":"variable"}}')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_inverse_curve_parameters', 'equation_parameters',
     '{"expression":"{x=-(2*atanh(sqrt(C+2*exp(y))/sqrt(C)))/sqrt(C)+D | x=?, y=?; C=?, D=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{"C":"-1","D":"1"},"plot_marker":"data-series"}'),
    ('equation_line_parameters', 'equation_parameters',
     '{"expression":"{y=a*x+b | x=?, y=?; a=?, b=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{"a":"1","b":"1"},"plot_marker":"data-series"}'),
    ('equation_named_coordinates_parameters', 'equation_parameters',
     '{"expression":"{v=a*u+b | u=?, v=?; a=?, b=?}","wrt":"u","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{"a":"1","b":"1"},"plot_marker":"data-series"}'),
    ('equation_fixed_constant_parameter', 'equation_parameters',
     '{"expression":"{y=a*x+b | x=?, y=?; a=3, b=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{"b":"1"},"plot_marker":"data-series"}'),
    ('equation_exact_constant_narrow_parameter', 'equation_parameters',
     '{"expression":"{y=C+a*x | x=?, y=?; C=exp(1), a=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32,"plot_lower":"-1e-70","plot_upper":"1e-70","plot_scale":"manual","plot_y_lower":"exp(1)-2e-70","plot_y_upper":"exp(1)+2e-70"}',
     '{"status":"200","parameters":{"a":"1"},"plot_marker":"data-series"}'),
    ('equation_three_coordinate_parameter', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=r^2 | x=?, y=?, z=?; r=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{"r":"1"},"plot_marker":"data-surface"}'),
    ('equation_parameter_slider_position', 'equation_parameters',
     '{"expression":"{y=a*x+b | x=?, y=?; a=?, b=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32,"plot_slices":"a\t-2\t2\t1\t250\nb\t-2\t2\t1\t-1"}',
     '{"status":"200","parameters":{"a":"-1","b":"1"},"plot_marker":"data-series"}'),
    ('equation_parameter_invalid_limits', 'equation_parameters',
     '{"expression":"{y=a*x | x=?, y=?; a=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32,"plot_slices":"a\t1\t0\t1\t500"}',
     '{"status":"422","parameters":{},"error_contains":"Invalid slice"}'),
    ('equation_four_coordinates_notice', 'equation_parameters',
     '{"expression":"{x+y+z+w=a | x=?, y=?, z=?, w=?; a=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"422","parameters":{},"error_contains":"mark them as constant parameters"}'),
    ('equation_display_y_of_x', 'equation_parameters',
     '{"equation":"x=2*y+1","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"y &=","display_excludes":"x &=","horizontal":"x"}'),
    ('equation_display_x_of_t', 'equation_parameters',
     '{"equation":"t=2*x+1","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"x &=","display_excludes":"t &=","horizontal":"t"}'),
    ('equation_display_all_dependent_branches', 'equation_parameters',
     '{"equation":"x=y^2","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"y &=","display_excludes":"x &=","horizontal":"x"}');

INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_unwrapped_inverse_display', 'equation_parameters',
     '{"equation":"x=-(2*atanh(sqrt(C+2*exp(y))/sqrt(C)))/sqrt(C)+D","precision":40}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"y &=","display_excludes":"D &=","solutions_contains":"where","horizontal":"x","bindings":{"x":"variable","y":"variable","C":"constant","D":"constant"}}'),
    ('equation_unwrapped_inverse_plot', 'equation_parameters',
     '{"expression":"x=-(2*atanh(sqrt(C+2*exp(y))/sqrt(C)))/sqrt(C)+D","wrt":"x","action":"plot","plot_source_mode":"equation","precision":40}',
     '{"status":"200","parameters":{"C":"-1","D":"1"},"plot_marker":"data-series","bindings":{"x":"variable","y":"variable","C":"constant","D":"constant"}}');

-- Retain the complete native domain and branch restrictions in the dependent-coordinate TeX card.
UPDATE mars_lab_cases SET expected =
    '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"y &=","display_excludes":"\\operatorname{sgn}","display_fragments":["\\text{provided that}","C\\ne 0","\\operatorname{arg}","\\text{or}","\\mathfrak{Im}","\\le \\frac{\\pi}{2}"],"solutions_contains":"where","horizontal":"x","bindings":{"x":"variable","y":"variable","C":"constant","D":"constant"}}'
WHERE name = 'equation_unwrapped_inverse_display';
INSERT INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_display_inverse_conditions_renamed', 'equation_parameters',
     '{"equation":"t=-(2*atanh(sqrt(A+2*exp(x))/sqrt(A)))/sqrt(A)+B","precision":40}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"x &=","display_excludes":"\\operatorname{sgn}","display_fragments":["\\text{provided that}","A\\ne 0","\\operatorname{arg}","\\text{or}","\\le \\frac{\\pi}{2}"],"solutions_contains":"where","horizontal":"t"}'),
    ('equation_display_square_root_conditions', 'equation_parameters',
     '{"equation":"x=sqrt(y)","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"y &=","display_excludes":"\\operatorname{sgn}","display_fragments":["\\text{provided that}","\\operatorname{arg}","\\text{or}"],"solutions_contains":"where","horizontal":"x"}'),
    ('equation_display_logarithm_conditions', 'equation_parameters',
     '{"equation":"a+2*ln(y)=x","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"y &=","display_excludes":"Conditions in Solutions","display_fragments":["\\text{provided that}","\\mathfrak{Im}","\\pi"],"solutions_contains":"where","horizontal":"x"}'),
    ('equation_display_reciprocal_conditions', 'equation_parameters',
     '{"equation":"x=atanh(y)/a","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"display_contains":"y &=","display_excludes":"Conditions in Solutions","display_fragments":["\\text{provided that}","a\\ne 0"],"solutions_contains":"where","horizontal":"x"}')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation, input=excluded.input, expected=excluded.expected;

UPDATE mars_lab_cases SET expected = json_set(expected, '$.display_min_condition_rows', '4')
WHERE name IN ('equation_unwrapped_inverse_display', 'equation_display_inverse_conditions_renamed');

UPDATE mars_lab_cases SET expected = json_set(expected, '$.display_fragments',
    json_insert(json_extract(expected, '$.display_fragments'), '$[#]', 'u &=', '$[#]', 'v &=',
        '$[#]', '\operatorname{arg}\left(v\right)', '$[#]', '\left\{u\right\}'))
WHERE name IN ('equation_unwrapped_inverse_display', 'equation_display_inverse_conditions_renamed');

-- Fixing any coordinate, including the selected first one, changes the plot dimension without changing its role.
INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_torus_equatorial_circles', 'equation_parameters',
     '{"expression":"{(x^2+y^2+z^2+91)^2=400*(x^2+y^2) | x=?, y=?, z=0}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["x","y"],"title_excludes":"z","contour_radii":[7,13]}'),
    ('equation_nested_circles', 'equation_parameters',
     '{"expression":"(x^2+y^2-1)*(x^2+y^2-9)=0","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["x","y"],"contour_radii":[1,3]}'),
    ('equation_nested_circles_translated', 'equation_parameters',
     '{"expression":"((x-4)^2+(y+2)^2-1)*((x-4)^2+(y+2)^2-9)=0","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["x","y"],"contour_centre":[4,-2],"contour_radii":[1,3]}'),
    ('equation_torus_manual_inner_circle', 'equation_parameters',
     '{"expression":"{(x^2+y^2+z^2+91)^2=400*(x^2+y^2) | x=?, y=?, z=0}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32,"plot_lower":"-8","plot_upper":"8","plot_scale":"manual","plot_y_lower":"-8","plot_y_upper":"8"}',
     '{"status":"200","parameters":{},"axes":["x","y"],"contour_radii":[7]}');

INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_fixed_first_coordinate', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=4 | x=1, y=?, z=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","plot_excludes":"data-surface","axes":["y","z"],"bindings":{"x":"variable","y":"variable","z":"variable"},"values":{"x":"1"}}'),
    ('equation_fixed_second_coordinate', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=4 | x=?, y=1, z=?}","wrt":"y","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","plot_excludes":"data-surface","axes":["x","z"],"bindings":{"x":"variable","y":"variable","z":"variable"},"values":{"y":"1"}}'),
    ('equation_fixed_third_coordinate', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=4 | x=?, y=?, z=1}","wrt":"z","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","plot_excludes":"data-surface","axes":["x","y"],"bindings":{"x":"variable","y":"variable","z":"variable"},"values":{"z":"1"}}'),
    ('equation_fixed_coordinate_axis_choice', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=4 | x=1, y=?, z=?}","wrt":"z","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","plot_excludes":"data-surface","axes":["z","y"]}'),
    ('equation_fixed_named_coordinate', 'equation_parameters',
     '{"expression":"{u^2+v^2+w^2=4 | u=1, v=?, w=?}","wrt":"u","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","plot_excludes":"data-surface","axes":["v","w"]}'),
    ('equation_fixed_tapered_ellipsoid', 'equation_parameters',
     '{"expression":"{x^2/784+(y^2+z^2)/484*(1+0.01*x)=1 | x=1, y=?, z=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","plot_excludes":"data-surface","axes":["y","z"],"values":{"x":"1"}}'),
    ('equation_cleared_coordinate_surface', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=4 | x=?, y=?, z=?}","wrt":"x","action":"plot","plot_source_mode":"equation","plot_quality":"preview","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-surface","axes":["x","y","z"]}');

-- Captions describe the sampled slice, retaining exact values and the worksheet's binding roles.
INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_caption_fixed_egg', 'equation_parameters',
     '{"expression":"{x^2/784+(y^2+z^2)/484*(1+0.01*x)=1 | x=?, y=?, z=1}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["x","y"],"title_contains":"3025","title_excludes":"z","title_TeX_contains":"784 - x^{2}","title_TeX_excludes":"\\frac","bindings":{"x":"variable","y":"variable","z":"variable"},"values":{"z":"1"}}'),
    ('equation_caption_exact_radical', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=4 | x=?, y=?, z=sqrt(2)}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["x","y"],"title_contains":"x² + y² = 2","title_excludes":"z","title_TeX_contains":"x^{2} + y^{2} &= 2","title_TeX_excludes":"z"}'),
    ('equation_caption_exact_fraction', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=4 | x=?, y=?, z=1/3}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["x","y"],"title_excludes":"z","title_TeX_contains":"y^{2} &= \\frac{35}{9}","title_TeX_excludes":"z"}'),
    ('equation_caption_fixed_named', 'equation_parameters',
     '{"expression":"{u^2+v^2+w^2=4 | u=1, v=?, w=?}","wrt":"u","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["v","w"],"title_contains":"v² + w² = 3","title_excludes":"u","title_TeX_contains":"v^{2} + w^{2} &= 3"}'),
    ('equation_caption_mesh_parameter', 'equation_parameters',
     '{"expression":"{x^2+y^2+z^2=r^2 | x=?, y=?, z=?; r=2}","wrt":"x","action":"plot","plot_source_mode":"equation","plot_quality":"preview","precision":32}',
     '{"status":"200","parameters":{},"axes":["x","y","z"],"title_contains":"x² + y² + z² = 4","title_excludes":"r","title_TeX_contains":"z^{2} &= 4","bindings":{"r":"constant"},"values":{"r":"2"}}'),
    ('equation_caption_curve_parameter', 'equation_parameters',
     '{"expression":"{x^2+a=4 | x=?; a=1}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["x"],"title_contains":"x² = 3","title_excludes":"a","title_TeX_contains":"x^{2} &= 3","bindings":{"a":"constant"}}'),
    ('equation_caption_reduced_circle', 'equation_parameters',
     '{"expression":"{x^2/784+(y^2+z^2)/484*(1+0.01*x)=1 | x=1, y=?, z=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"axes":["y","z"],"title_contains":"y² + z²","title_excludes":"x","title_TeX_contains":"y^{2} + z^{2} &= \\frac{2368575}{4949}","title_TeX_excludes":"3025","values":{"x":"1"}}'),
    ('equation_caption_selected_parameter', 'equation_parameters',
     '{"expression":"{y=a*x+b | x=?, y=?; a=?, b=?}","wrt":"x","action":"plot","plot_source_mode":"equation","precision":32,"plot_slices":"a\t-2\t2\t1\t250\nb\t-2\t2\t1\t-1"}',
     '{"status":"200","parameters":{"a":"-1","b":"1"},"title_contains":"y = 1 - x","title_excludes":"b - x","title_TeX_contains":"y &= 1 - x","bindings":{"a":"constant","b":"constant"}}')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation, input=excluded.input, expected=excluded.expected;

-- Editable bindings retain authored decimals; captions abbreviate only long display literals.
INSERT INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('equation_caption_decimal_kepler', 'equation_parameters',
     '{"expression":"{M=E-e*sin(E)|E=?;e=0.0167,M=1.047197551196597746154214461093167628065723133125035273658314}","wrt":"E","action":"plot","plot_source_mode":"equation","precision":98}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","title_contains":"0.0167","title_TeX_contains":"1.0471975511965977\\ldots{}","title_TeX_excludes":"\\frac","values":{"e":"0.0167","M":"1.047197551196597746154214461093167628065723133125035273658314"}}'),
    ('equation_caption_decimal_renamed', 'equation_parameters',
     '{"expression":"{q=t-c*sin(t)|t=?;c=0.0167,q=1.047197551196597746154214461093167628065723133125035273658314}","wrt":"t","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","title_contains":"0.0167","title_TeX_contains":"1.0471975511965977\\ldots{}","title_TeX_excludes":"\\frac","values":{"c":"0.0167","q":"1.047197551196597746154214461093167628065723133125035273658314"}}'),
    ('equation_caption_decimal_short', 'equation_parameters',
     '{"expression":"{M=E-e*sin(E)|E=?;e=0.0167,M=1.0472}","wrt":"E","action":"plot","plot_source_mode":"equation","precision":32}',
     '{"status":"200","parameters":{},"plot_marker":"data-series","title_contains":"1.0472","title_TeX_contains":"0.0167","title_TeX_excludes":"\\ldots","values":{"e":"0.0167","M":"1.0472"}}'),
    ('equation_binding_authored_decimal', 'equation_parameters',
     '{"equation":"{y=a*x|x=?,y=?;a=0.0167}","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"values":{"a":"0.0167"}}'),
    ('expression_binding_authored_decimal', 'equation_parameters',
     '{"expression":"{a*x|x=?;a=0.0167}","precision":32}',
     '{"status":"200","parameters":{},"values":{"a":"0.0167"}}'),
    ('expression_binding_decimal_half', 'equation_parameters',
     '{"expression":"{a*x|x=?;a=0.5}","precision":32}',
     '{"status":"200","parameters":{},"values":{"a":"0.5"}}'),
    ('matrix_binding_authored_decimal', 'equation_parameters',
     '{"matrix":"{(a 0;0 a)|;a=0.0167}","operation":"eval","precision":32}',
     '{"route":"/matrix-eval","status":"200","parameters":{},"values":{"a":"0.0167"}}'),
    ('equation_binding_authored_radical', 'equation_parameters',
     '{"equation":"{y=a*x|x=?,y=?;a=sqrt(2)}","precision":32}',
     '{"route":"/equation-eval","status":"200","parameters":{},"values":{"a":"√(2)"}}')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation, input=excluded.input, expected=excluded.expected;

-- Weather failures retain actionable status without publishing raw provider or transport text.
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'mars_lab.weather_diagnostics', 'tools/mars_lab/tests/test_lab_weather.c',
    'tests.test_lab_weather_diagnostics', 'mars_lab_cases', 'weather_diagnostic')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO mars_lab_cases(name, operation, input, expected) VALUES
    ('weather_client_unavailable', 'weather_diagnostic', '{"client":false,"error":0,"status":0,"payload":{}}', 'MARS could not initialise its HTTPS client; check the installed libcurl runtime.'),
    ('weather_argument', 'weather_diagnostic', '{"client":true,"error":1,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'MARS could not prepare the weather request.'),
    ('weather_memory', 'weather_diagnostic', '{"client":true,"error":2,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'MARS could not allocate memory for the weather request.'),
    ('weather_connection', 'weather_diagnostic', '{"client":true,"error":3,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'MARS could not connect to WeatherAPI; check the network connection.'),
    ('weather_timeout', 'weather_diagnostic', '{"client":true,"error":4,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'The WeatherAPI request timed out; try again.'),
    ('weather_limit', 'weather_diagnostic', '{"client":true,"error":5,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'The WeatherAPI response exceeded MARS''s supported size limit.'),
    ('weather_cancelled', 'weather_diagnostic', '{"client":true,"error":6,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'The weather request was cancelled.'),
    ('weather_io', 'weather_diagnostic', '{"client":true,"error":7,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'MARS could not read the weather response.'),
    ('weather_protocol', 'weather_diagnostic', '{"client":true,"error":8,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'The WeatherAPI connection used an unsupported protocol.'),
    ('weather_unknown_transport', 'weather_diagnostic', '{"client":true,"error":99,"status":0,"payload":{},"detail":"fixture-weather-key https://example.invalid/private"}', 'The weather request did not complete.'),
    ('weather_dns', 'weather_diagnostic', '{"client":true,"error":3,"status":0,"payload":{},"detail":"Couldn''t resolve host name; fixture-weather-key"}', 'MARS could not resolve api.weatherapi.com; check DNS and the network connection.'),
    ('weather_tls', 'weather_diagnostic', '{"client":true,"error":3,"status":0,"payload":{},"detail":"SSL peer certificate was not OK; fixture-weather-key"}', 'MARS could not verify the HTTPS connection to WeatherAPI; check the system clock and CA certificates.'),
    ('weather_tls_word', 'weather_diagnostic', '{"client":true,"error":3,"status":0,"payload":{},"detail":"TLS handshake failed; fixture-weather-key"}', 'MARS could not verify the HTTPS connection to WeatherAPI; check the system clock and CA certificates.'),
    ('weather_missing_key', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":1002,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI did not receive an API key (error 1002).'),
    ('weather_missing_location', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":1003,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI did not receive a location (error 1003).'),
    ('weather_invalid_url', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":1005,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI rejected the request URL (error 1005).'),
    ('weather_missing_town', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":1006,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI could not identify the selected location (error 1006).'),
    ('weather_invalid_key', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2006,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI rejected the configured API key; re-enter it in weather setup (error 2006).'),
    ('weather_quota', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2007,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI''s monthly request quota has been exceeded (error 2007).'),
    ('weather_disabled_key', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2008,"message":"fixture-weather-key https://example.invalid/private"}}}', 'The configured WeatherAPI key has been disabled (error 2008).'),
    ('weather_subscription', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2009,"message":"fixture-weather-key https://example.invalid/private"}}}', 'The WeatherAPI account does not have access to this weather service (error 2009).'),
    ('weather_restriction', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2010,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI rejected this computer because of the key''s website, app or IP restrictions (error 2010).'),
    ('weather_cap', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2011,"message":"fixture-weather-key https://example.invalid/private"}}}', 'The WeatherAPI key''s monthly request cap has been reached (error 2011).'),
    ('weather_rate', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2012,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI is receiving requests too quickly; try again shortly (error 2012).'),
    ('weather_provider_internal', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":9999,"message":"fixture-weather-key https://example.invalid/private"}}}', 'WeatherAPI reported an internal service error (error 9999).'),
    ('weather_provider_unknown', 'weather_diagnostic', '{"client":true,"error":0,"status":403,"payload":{"error":{"code":2345,"message":"fixture-weather-key"}}}', 'WeatherAPI reported error 2345 (HTTP 403).'),
    ('weather_provider_error_in_success', 'weather_diagnostic', '{"client":true,"error":0,"status":200,"payload":{"error":{"code":2006,"message":"fixture-weather-key"}}}', 'WeatherAPI rejected the configured API key; re-enter it in weather setup (error 2006).'),
    ('weather_http_failure', 'weather_diagnostic', '{"client":true,"error":0,"status":503,"payload":{"message":"fixture-weather-key"}}', 'WeatherAPI returned HTTP 503; weather is unavailable.'),
    ('weather_malicious_error_code', 'weather_diagnostic', '{"client":true,"error":0,"status":401,"payload":{"error":{"code":"fixture-weather-key","message":"fixture-weather-key"}}}', 'WeatherAPI returned HTTP 401; weather is unavailable.'),
    ('weather_unreadable_success', 'weather_diagnostic', '{"client":true,"error":0,"status":200,"payload":null}', 'WeatherAPI returned a response MARS could not read.'),
    ('weather_array_success', 'weather_diagnostic', '{"client":true,"error":0,"status":200,"payload":[]}', 'WeatherAPI returned a response MARS could not read.'),
    ('weather_unclassified_provider_error', 'weather_diagnostic', '{"client":true,"error":0,"status":200,"payload":{"error":{"message":"fixture-weather-key"}}}', 'WeatherAPI returned a response MARS could not read.'),
    ('weather_no_response', 'weather_diagnostic', '{"client":true,"error":0,"status":0,"payload":null}', 'The weather request did not complete.'),
    ('weather_success', 'weather_diagnostic', '{"client":true,"error":0,"status":200,"payload":{}}', '');
