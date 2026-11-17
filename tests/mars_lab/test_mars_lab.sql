-- Native Lab form-reference work budgets. Timed calculations must not run while restoring controls.
CREATE TABLE IF NOT EXISTS mars_lab_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS mars_lab_cases_operation ON mars_lab_cases(operation, name);

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
