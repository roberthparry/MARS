-- string test vectors; all assertions remain in the native C case callbacks.
CREATE TABLE IF NOT EXISTS string_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS string_cases_operation ON string_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'string.test_string_unquote_shell_words_cases', 'tests/string/test_string.c', 'tests.test_string_unquote_shell_words', 'string_cases', 'test_string_unquote_shell_words_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/string/test_string.c: test_string_unquote_shell_words. Existing checks are retained in C.
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_9b67c43f493ca875', 'test_string_unquote_shell_words_cases', '', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_c2264a4d532200b6', 'test_string_unquote_shell_words_cases', ' 	 ', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_9327e58cd49bd903', 'test_string_unquote_shell_words_cases', '''''', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_9338ea8cd4aa54b5', 'test_string_unquote_shell_words_cases', '""', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_4dd0ef0f32427caa', 'test_string_unquote_shell_words_cases', ' 	# comment', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_8e7cfda1228a3854', 'test_string_unquote_shell_words_cases', 'literal', 'literal');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_7c3edd7e038f53dd', 'test_string_unquote_shell_words_cases', '  literal	 ', 'literal');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_93c9d84d119eb5b6', 'test_string_unquote_shell_words_cases', '''two words''', 'two words');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_01bc1f2f1849e35a', 'test_string_unquote_shell_words_cases', '"two words"', 'two words');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_34dbcd8f7bce1e43', 'test_string_unquote_shell_words_cases', 'one\ two', 'one two');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_c23c263ae9075b6a', 'test_string_unquote_shell_words_cases', 'one\	two', 'one	two');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_a1b2e167ac100181', 'test_string_unquote_shell_words_cases', '''pa''"''"''ss word''', 'pa''ss word');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_85d52728aba217e3', 'test_string_unquote_shell_words_cases', '''pa''\''''ss''', 'pa''ss');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_7a43379fbeff4a72', 'test_string_unquote_shell_words_cases', 'un''quo''"ted"', 'unquoted');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_a707c719e928f34d', 'test_string_unquote_shell_words_cases', 'a''''b""c', 'abc');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_8ed9994c13ffc1fd', 'test_string_unquote_shell_words_cases', 'a#b', 'a#b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_058036254f94acf7', 'test_string_unquote_shell_words_cases', '''a''#b', 'a#b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_9aa8fc44c73a6f41', 'test_string_unquote_shell_words_cases', 'a\#b', 'a#b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_5cf852d99d8aa767', 'test_string_unquote_shell_words_cases', '''a # b'' # trailing comment', 'a # b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_0aca7ff94a8cf783', 'test_string_unquote_shell_words_cases', 'a 	# unmatched '' and dangling \', 'a');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_e87b48f1ea1616ae', 'test_string_unquote_shell_words_cases', '\''\"\\', '''"\');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_a2586024866a7244', 'test_string_unquote_shell_words_cases', '''a\b''', 'a\b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_94bade35de186420', 'test_string_unquote_shell_words_cases', '"a\\b"', 'a\b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_9e472c34c17cbeca', 'test_string_unquote_shell_words_cases', '"a\"b"', 'a"b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_4c475d3646865003', 'test_string_unquote_shell_words_cases', '"a\qb"', 'a\qb');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_c9a46b34da10b185', 'test_string_unquote_shell_words_cases', '"a\''b"', 'a\''b');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_c6f6423cc03e1333', 'test_string_unquote_shell_words_cases', '"\$HOME\`id\`"', '\$HOME\`id\`');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_99f4da44c6a160f8', 'test_string_unquote_shell_words_cases', 'a\nb', 'anb');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_90dfd0d3226f0da4', 'test_string_unquote_shell_words_cases', '''$HOME $(id) `id` ~ * ; | &''', '$HOME $(id) `id` ~ * ; | &');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_cac7ab5532490510', 'test_string_unquote_shell_words_cases', '$HOME', '$HOME');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_da945e369994c177', 'test_string_unquote_shell_words_cases', '$(id)', '$(id)');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_4ca84cf9d163ac9d', 'test_string_unquote_shell_words_cases', '~/*.env', '~/*.env');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_e5448a081976cdf7', 'test_string_unquote_shell_words_cases', '''é''"λ"😀', 'éλ😀');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_4c4a3042e88ca044', 'test_string_unquote_shell_words_cases', 'a b', 'a b');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'string.test_string_unquote_shell_invalid_cases', 'tests/string/test_string.c', 'tests.test_string_unquote_shell_invalid', 'string_cases', 'test_string_unquote_shell_invalid_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/string/test_string.c: test_string_unquote_shell_invalid. Existing checks are retained in C.
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_26076bbee56ce713', 'test_string_unquote_shell_invalid_cases', 'one two', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_01e241516011f7ef', 'test_string_unquote_shell_invalid_cases', '''one'' two', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_1f3084d01803a063', 'test_string_unquote_shell_invalid_cases', 'one "two"', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_13787fe1da7deff9', 'test_string_unquote_shell_invalid_cases', ''''' ''''', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_583c067d67c0c8ba', 'test_string_unquote_shell_invalid_cases', 'one	two', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_4e5eefb402ef1cf5', 'test_string_unquote_shell_invalid_cases', 'one # comment
', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_c197de0fd9e47ed2', 'test_string_unquote_shell_invalid_cases', '''unterminated', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_26b5b73f5496f625', 'test_string_unquote_shell_invalid_cases', '"unterminated', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_e59384dc893eb749', 'test_string_unquote_shell_invalid_cases', 'trailing\', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_3f515894d8bf4089', 'test_string_unquote_shell_invalid_cases', '"trailing\', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_0b13e4b99ae258ca', 'test_string_unquote_shell_invalid_cases', 'a
b', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_0b10feb99ae04f21', 'test_string_unquote_shell_invalid_cases', 'ab', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_109f3408803fbc60', 'test_string_unquote_shell_invalid_cases', '''a
b''', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_a44d1237ad4a3359', 'test_string_unquote_shell_invalid_cases', '"ab"', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_056065613a6e17a2', 'test_string_unquote_shell_invalid_cases', 'a\
b', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_055d3f613a6ba139', 'test_string_unquote_shell_invalid_cases', 'a\b', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_38dece2ff990e7c0', 'test_string_unquote_shell_invalid_cases', '"a\
b"', '');
INSERT OR IGNORE INTO string_cases(name, operation, input, expected) VALUES ('case_c61d7cd6ecccfb60', 'test_string_unquote_shell_invalid_cases', '# comment', '');
