-- Configuration-plan inputs. C performs the selection and leaf-count assertions.
CREATE TABLE IF NOT EXISTS test_config_plan_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;

INSERT OR IGNORE INTO test_config_plan_cases(name, operation, input, expected) VALUES
    ('disabled_leaf_and_group', 'global', '{"source":"tests/example/test_example.c","configuration":{"tests/example/test_example.c":{"tests":{"enabled":true,"active":true,"off":false,"hidden":{"enabled":false,"leaf":true}},"readme_examples":{"shown":true,"hidden":false}}}}', '2'),
    ('disabled_entire_module', 'global', '{"source":"tests/example/test_example.c","configuration":{"tests/example/test_example.c":{"tests":{"enabled":false,"active":true}}}}', '0'),
    ('nested_group_is_not_a_case', 'global', '{"source":"tests/example/test_example.c","configuration":{"tests/example/test_example.c":{"tests":{"outer":{"enabled":true,"inner":{"a":true,"b":true}}}}}}', '2'),
    ('cross_file_group', 'global', '{"source":"tests/example/test_example.c","configuration":{"tests/example/test_example.c":{"tests":{"batch":true}},"tests/example/test_extra.c":{"tests":{"batch":{"one":true,"two":false}}}}}', '1'),
    ('disabled_cross_file_group', 'global', '{"source":"tests/example/test_example.c","configuration":{"tests/example/test_example.c":{"tests":{"batch":false}},"tests/example/test_extra.c":{"tests":{"batch":{"one":true}}}}}', '0'),
    ('tool_namespaces_are_distinct', 'global', '{"source":"tools/one/tests/test_one.c","configuration":{"tools/one/tests/test_one.c":{"tests":{"a":true}},"tools/two/tests/test_two.c":{"tests":{"b":true}},"tests/example/test_example.c":{"tests":{"c":true}}}}', '1'),
    ('missing_module', 'global', '{"source":"tests/example/test_example.c","configuration":{"tests/other/test_other.c":{"tests":{"a":true}}}}', 'unknown'),
    ('wiped_configuration', 'global', '{"source":"tests/example/test_example.c","configuration":{}}', 'unknown'),
    ('local_disabled_group', 'local', '{"source":"tests/example/test_example.c","configuration":{"group":{"enabled":false,"a":true},"b":true,"c":false}}', '1'),
    ('profile_overrides_json', 'override', '{"source":"tests/example/test_example.c","configuration":{"tests/example/test_example.c":{"tests":{"enabled":false,"wanted":false}}}}', '1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'test_test_config', 'tests/test_test_harness/test_test_harness.c', 'test_config_progress_plans',
    'test_config_plan_cases', NULL)
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent, table_name=excluded.table_name,
    operation=excluded.operation;
