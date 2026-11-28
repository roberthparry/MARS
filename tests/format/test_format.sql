-- Typed interpolation inputs and independent text/error expectations; assertions and ownership stay in C.
CREATE TABLE IF NOT EXISTS format_cases (
    name TEXT PRIMARY KEY,
    operation TEXT NOT NULL,
    input TEXT NOT NULL,
    expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 31 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS format_cases_operation ON format_cases(operation,name);

INSERT INTO test_case_runner(module,source,parent,table_name,operation) VALUES
    ('format.render','tests/format/test_format.c','tests.test_format_render','format_cases','render'),
    ('format.errors','tests/format/test_format.c','tests.test_format_errors','format_cases','error'),
    ('format.stream','tests/format/test_format.c','tests.test_format_streams','format_cases','render'),
    ('format.stream_errors','tests/format/test_format.c','tests.test_format_stream_errors','format_cases','error')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name,operation=excluded.operation;

INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('literal_empty','render','{"pattern":"","args":[]}',''),
    ('literal_percent_braces','render','{"pattern":"100% {{ready}}","args":[]}','100% {ready}'),
    ('inserted_text_is_data','render','{"pattern":"{}","args":[{"type":"text","value":"{missing}: 100%"}]}','{missing}: 100%'),
    ('utf8_binding_name','render','{"pattern":"{κόσμος}","args":[{"type":"int","name":"κόσμος","value":"7"}]}','7'),
    ('named_native_fixed','render','{"pattern":"{name}: x={x:.3f}","args":[{"type":"text","name":"name","value":"Ada"},{"type":"qfloat","name":"x","value":"1.25"}]}','Ada: x=1.250'),
    ('automatic_positions','render','{"pattern":"{} + {}","args":[{"type":"int","value":"4"},{"type":"int","value":"7"}]}','4 + 7'),
    ('indexed_repeat','render','{"pattern":"{1}/{0}/{1}","args":[{"type":"int","value":"4"},{"type":"int","value":"7"}]}','7/4/7'),
    ('named_with_automatic','render','{"pattern":"{name}: {}","args":[{"type":"int","value":"42"},{"type":"text","name":"name","value":"Ada"}]}','Ada: 42'),
    ('named_with_indexed','render','{"pattern":"{name}: {0}","args":[{"type":"int","value":"42"},{"type":"text","name":"name","value":"Ada"}]}','Ada: 42'),
    ('duplicate_names_positional','render','{"pattern":"{} {}","args":[{"type":"int","name":"x","value":"1"},{"type":"int","name":"x","value":"2"}]}','1 2'),
    ('signed_minimum','render','{"pattern":"{}","args":[{"type":"int","value":"-9223372036854775808"}]}','-9223372036854775808'),
    ('unsigned_maximum','render','{"pattern":"{}","args":[{"type":"uint","value":"18446744073709551615"}]}','18446744073709551615'),
    ('signed_padding','render','{"pattern":"{:+06d}","args":[{"type":"int","value":"42"}]}','+00042'),
    ('negative_padding','render','{"pattern":"{:06d}","args":[{"type":"int","value":"-42"}]}','-00042'),
    ('hex_prefix_padding','render','{"pattern":"{:+#010x}","args":[{"type":"uint","value":"255"}]}','+0x00000ff'),
    ('upper_hex','render','{"pattern":"{:#X}","args":[{"type":"int","value":"255"}]}','0XFF'),
    ('binary_prefix','render','{"pattern":"{:#b}","args":[{"type":"int","value":"5"}]}','0b101'),
    ('octal_prefix','render','{"pattern":"{:#o}","args":[{"type":"int","value":"15"}]}','0o17'),
    ('unicode_scalar','render','{"pattern":"{:c}","args":[{"type":"int","value":"937"}]}','Ω'),
    ('bool_default','render','{"pattern":"{} {}","args":[{"type":"bool","value":true},{"type":"bool","value":false}]}','true false'),
    ('bool_numeric','render','{"pattern":"{:04d}","args":[{"type":"bool","value":true}]}','0001'),
    ('float_fixed','render','{"pattern":"{:.3f}","args":[{"type":"float","value":"1.25"}]}','1.250'),
    ('double_scientific','render','{"pattern":"{:.2e}","args":[{"type":"double","value":"1.25"}]}','1.25e+00'),
    ('double_general','render','{"pattern":"{:.3g}","args":[{"type":"double","value":"1.25"}]}','1.25'),
    ('long_double_fixed','render','{"pattern":"{:.4f}","args":[{"type":"longdouble","value":"1.25"}]}','1.2500'),
    ('double_zero_signed','render','{"pattern":"{:+.2f}","args":[{"type":"double","value":"-0"}]}','-0.00'),
    ('float_alternate','render','{"pattern":"{:#.0f}","args":[{"type":"double","value":"2"}]}','2.'),
    ('float_nonfinite','render','{"pattern":"{:+F}","args":[{"type":"double","value":"inf"}]}','+INF'),
    ('null_cstring','render','{"pattern":"{}","args":[{"type":"text","value":null}]}','(null)'),
    ('null_string','render','{"pattern":"{}","args":[{"type":"string","value":null}]}','(null)'),
    ('null_pointer','render','{"pattern":"{}","args":[{"type":"pointer"}]}','0x0'),
    ('center_fill','render','{"pattern":"{:*^7s}","args":[{"type":"text","value":"abc"}]}','**abc**'),
    ('odd_center_fill','render','{"pattern":"{:_^6s}","args":[{"type":"text","value":"abc"}]}','_abc__'),
    ('unicode_precision','render','{"pattern":"{:>4.1s}","args":[{"type":"string","value":"é🙂"}]}','   é'),
    ('view_unicode','render','{"pattern":"{:.1s}","args":[{"type":"view","value":"Ω🙂"}]}','Ω'),
    ('rune_combining','render','{"pattern":"{}","args":[{"type":"rune","value":"éx"}]}','é'),
    ('string_precision_zero','render','{"pattern":"[{:.0s}]","args":[{"type":"text","value":"text"}]}','[]'),
    ('native_complex_fixed','render','{"pattern":"{:.2f}","args":[{"type":"complex","value":"(1.25,-2.5)"}]}','1.25 - 2.50i'),
    ('native_complex_unit','render','{"pattern":"{}","args":[{"type":"complex","value":"-i"}]}','-i'),
    ('native_qfloat_infinity','render','{"pattern":"{:+F}","args":[{"type":"qfloat","hi":"inf","lo":"0"}]}','+INF'),
    ('native_qfloat_low_infinity','render','{"pattern":"{:f}","args":[{"type":"qfloat","hi":"0","lo":"-inf"}]}','-inf'),
    ('native_qfloat_same_infinities','render','{"pattern":"{:e}","args":[{"type":"qfloat","hi":"inf","lo":"inf"}]}','inf'),
    ('native_qfloat_opposite_infinities','render','{"pattern":"{:g}","args":[{"type":"qfloat","hi":"inf","lo":"-inf"}]}','nan'),
    ('native_qfloat_nan','render','{"pattern":"{:f}","args":[{"type":"qfloat","hi":"nan","lo":"0"}]}','nan'),
    ('native_qfloat_negative_nan','render','{"pattern":"{:F}","args":[{"type":"qfloat","hi":"-nan","lo":"0"}]}','-NAN'),
    ('native_complex_sign','render','{"pattern":"{:+}","args":[{"type":"complex","value":"(1,2)"}]}','+1 + 2i'),
    ('native_exact_number','render','{"pattern":"{}","args":[{"type":"number","value":"1234567890123456789012345678901234567890"}]}','1234567890123456789012345678901234567890'),
    ('native_fraction_precision','render','{"pattern":"{:.3n}","args":[{"type":"number","value":"5/6"}]}','⅚'),
    ('native_number_scientific','render','{"pattern":"{:.3N}","args":[{"type":"number","value":"1.25"}]}','1.250E+00'),
    ('error_open_brace','error','{"pattern":"{","args":[]}','syntax'),
    ('error_close_brace','error','{"pattern":"}","args":[]}','syntax'),
    ('error_nested','error','{"pattern":"{0:{1}}","args":[{"type":"int","value":"1"},{"type":"int","value":"2"}]}','syntax'),
    ('error_mixed_indexing','error','{"pattern":"{} {0}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('error_mixed_reverse','error','{"pattern":"{0} {}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('error_missing_named','error','{"pattern":"{absent}","args":[]}','missing'),
    ('error_missing_position','error','{"pattern":"{1}","args":[{"type":"int","value":"1"}]}','missing'),
    ('error_missing_automatic','error','{"pattern":"{}","args":[]}','missing'),
    ('error_duplicate_named','error','{"pattern":"{x}","args":[{"type":"int","name":"x","value":"1"},{"type":"int","name":"x","value":"2"}]}','ambiguous'),
    ('error_unknown_presentation','error','{"pattern":"{:unknown}","args":[{"type":"int","value":"1"}]}','type'),
    ('error_string_numeric','error','{"pattern":"{:f}","args":[{"type":"text","value":"text"}]}','type'),
    ('error_integer_float','error','{"pattern":"{:.2f}","args":[{"type":"int","value":"1"}]}','type'),
    ('error_integer_precision','error','{"pattern":"{:.2d}","args":[{"type":"int","value":"1"}]}','type'),
    ('error_string_sign','error','{"pattern":"{:+s}","args":[{"type":"text","value":"text"}]}','type'),
    ('error_string_zero','error','{"pattern":"{:05s}","args":[{"type":"text","value":"text"}]}','type'),
    ('error_integer_alternate','error','{"pattern":"{:#d}","args":[{"type":"int","value":"1"}]}','type'),
    ('error_number_presentation','error','{"pattern":"{:f}","args":[{"type":"number","value":"1/3"}]}','type'),
    ('error_precision_missing','error','{"pattern":"{:.f}","args":[{"type":"double","value":"1"}]}','syntax'),
    ('error_precision_negative','error','{"pattern":"{:.-1f}","args":[{"type":"double","value":"1"}]}','syntax'),
    ('error_precision_range','error','{"pattern":"{:.10001f}","args":[{"type":"double","value":"1"}]}','range'),
    ('error_width_range','error','{"pattern":"{:2147483648}","args":[{"type":"int","value":"1"}]}','range'),
    ('error_index_overflow','error','{"pattern":"{99999999999999999999999999999}","args":[]}','range'),
    ('error_nonascii_fill','error','{"pattern":"{:🙂>8}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('error_surrogate_character','error','{"pattern":"{:c}","args":[{"type":"int","value":"55296"}]}','argument'),
    ('error_large_character','error','{"pattern":"{:c}","args":[{"type":"uint","value":"1114112"}]}','argument'),
    ('error_negative_character','error','{"pattern":"{:c}","args":[{"type":"int","value":"-1"}]}','argument'),
    ('error_encoded_padding_overflow','error','{"pattern":"{:2147483647}","args":[{"type":"text","value":"Ω"}]}','range')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

-- Exact binary-component decimal references independently evaluated using 2304-bit MPFR, ties to even.
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_000','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_001','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_002','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_003','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_004','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_005','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_006','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.00000000000000000086736173798840355')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_007','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.00000000000000000086736173798840355e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_008','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.0000000000000000008673617379884035')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_009','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.000000000000000000867361737988403547205962240695953369140625000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_010','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.000000000000000000867361737988403547205962240695953369140625000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_011','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x1p-60"}]}','1.000000000000000000867361737988403547205962240695953369140625')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_012','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_013','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','1e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_014','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_015','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','1.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_016','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','1.000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_017','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_018','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','0.99999999999999999913263826201159645')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_019','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','9.99999999999999999132638262011596453e-01')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_020','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','0.99999999999999999913263826201159645')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_021','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','0.999999999999999999132638262011596452794037759304046630859375000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_022','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','9.999999999999999991326382620115964527940377593040466308593750000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e-01')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_023','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p-60"}]}','0.999999999999999999132638262011596452794037759304046630859375')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_024','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_025','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_026','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_027','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.500000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_028','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.500000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_029','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_030','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.50000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_031','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.50000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_032','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_033','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_034','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_035','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1.4p+1","lo":"0x0p+0"}]}','2.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_036','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','4')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_037','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','4e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_038','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','4')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_039','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.500000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_040','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.500000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_041','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_042','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.50000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_043','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.50000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_044','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_045','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_046','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_047','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1.cp+1","lo":"0x0p+0"}]}','3.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_048','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_049','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','2e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_050','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','2e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_051','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_052','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','1.797693e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_053','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','1.79769e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_054','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.00000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_055','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','1.79769313486231570814527423731704357e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_056','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','1.7976931348623157081452742373170436e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_057','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_058','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','1.797693134862315708145274237317043567980705675258449965989174768031572607800285387605895586327668781715404589535143824642343213268894641827684675467035375169860499105765512820762454900903893289440758685084551339423045832369032229481658085593321233482747978262041447231687381771809192998812504040261841248583680000000000000000e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_059','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"0x0p+0"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_060','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_061','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','5e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_062','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','5e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_063','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','0.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_064','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','4.940656e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_065','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','4.94066e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_066','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','0.00000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_067','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','4.94065645841246544176568792868221372e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_068','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','4.9406564584124654417656879286822137e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_069','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_070','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','4.940656458412465441765687928682213723650598026143247644255856825006755072702087518652998363616359923797965646954457177309266567103559397963987747960107818781263007131903114045278458171678489821036887186360569987307230500063874091535649843873124733972731696151400317153853980741262385655911710266585566867681870395603106249319e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_071','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x0.0000000000001p-1022","lo":"0x0p+0"}]}','4.94065645841246544176568792868221372365059802614324764425585682500675507270208751865299836361635992379796564695445717730926656710355939796398774796010781878126300713190311404527845817167848982103688718636056998730723050006387409153564984387312473397273169615140031715385398074126238565591171026658556686768187039560310624932e-324')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_072','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_073','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_074','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_075','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_076','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1.000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_077','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_078','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1.00000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_079','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1.00000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_080','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_081','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_082','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_083','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"0x0.0000000000001p-1022"}]}','1')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_084','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_085','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','2e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_086','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','2e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_087','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_088','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','1.797693e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_089','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','1.79769e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_090','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.00000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_091','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','1.79769313486231570814527423731704357e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_092','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','1.7976931348623157081452742373170436e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_093','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858367.999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999995')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_094','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','1.797693134862315708145274237317043567980705675258449965989174768031572607800285387605895586327668781715404589535143824642343213268894641827684675467035375169860499105765512820762454900903893289440758685084551339423045832369032229481658085593321233482747978262041447231687381771809192998812504040261841248583680000000000000000e+308')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_095','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1.fffffffffffffp+1023","lo":"-0x0.0000000000001p-1022"}]}','179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_096','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_097','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_098','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_099','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_100','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0.000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_101','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_102','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0.00000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_103','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0.00000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_104','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_105','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_106','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_107','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"-0x0p+0","lo":"-0x0p+0"}]}','-0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_108','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_109','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_110','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_111','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0.000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_112','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0.000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_113','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_114','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0.00000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_115','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0.00000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_116','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_117','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_118','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_119','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1p+0","lo":"-0x1p+0"}]}','0')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_120','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_121','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_122','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_123','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.500000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_124','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.500000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_125','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_126','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.50000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_127','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.50000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_128','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_129','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_130','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_131','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x0p+0","lo":"-0x1.4p+1"}]}','-2.5')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_132','render','{"pattern":"{:.0f}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','10')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_133','render','{"pattern":"{:.0e}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','1e+01')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_134','render','{"pattern":"{:.0g}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','1e+01')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_135','render','{"pattern":"{:.6f}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.999000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_136','render','{"pattern":"{:.6e}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.999000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_137','render','{"pattern":"{:.6g}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.999')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_138','render','{"pattern":"{:.35f}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.99900000000000055422333389287814498')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_139','render','{"pattern":"{:.35e}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.99900000000000055422333389287814498e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_140','render','{"pattern":"{:.35g}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.999000000000000554223333892878145')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_141','render','{"pattern":"{:.324f}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.999000000000000554223333892878144979476928710937500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_142','render','{"pattern":"{:.324e}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.999000000000000554223333892878144979476928710937500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('decimal_reference_143','render','{"pattern":"{:.324g}","args":[{"type":"qfloat","hi":"0x1.3ff7ced916873p+3","lo":"0x0p+0"}]}','9.9990000000000005542233338928781449794769287109375')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('excel_unambiguous_date','render','{"pattern":"{:yyyy-MM-dd hh:mm:ss}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','2026-02-10 13:05:06'),
    ('excel_month_after_hour','render','{"pattern":"{:hh:MM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','13:02'),
    ('excel_month_before_second','render','{"pattern":"{:MM:ss}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','02:06'),
    ('excel_minute_in_date','render','{"pattern":"{:yyyy-mm-dd}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','2026-05-10'),
    ('excel_month_plain','render','{"pattern":"{:M}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','2'),
    ('excel_minute_plain','render','{"pattern":"{:m}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','5'),
    ('excel_month_short','render','{"pattern":"{:MMM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','Feb'),
    ('excel_month_long','render','{"pattern":"{:MMMM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','February'),
    ('excel_month_initial','render','{"pattern":"{:MMMMM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','F'),
    ('excel_literal_words','render','{"pattern":"{:do \"day of month\" MM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','10th day of month 02'),
    ('excel_literal_words_superscript','render','{"pattern":"{:do^ \"day of month\" MM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','10ᵗʰ day of month 02'),
    ('excel_error_explicit__month_0_','error','{"pattern":"{:excel:[month:0]}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','syntax'),
    ('excel_error_explicit__month_6_','error','{"pattern":"{:excel:[month:6]}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','syntax'),
    ('excel_error_explicit__minute_3_','error','{"pattern":"{:excel:[minute:3]}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','syntax'),
    ('excel_error_explicit__month_two_','error','{"pattern":"{:excel:[month:two]}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','syntax')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
-- MARS ordinal extensions cover every day, including the teen suffix exceptions.
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('ordinal_ordinary_01','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-01"}]}','1st'),
    ('ordinal_superscript_01','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-01"}]}','1ˢᵗ'),
    ('ordinal_ordinary_02','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-02"}]}','2nd'),
    ('ordinal_superscript_02','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-02"}]}','2ⁿᵈ'),
    ('ordinal_ordinary_03','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-03"}]}','3rd'),
    ('ordinal_superscript_03','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-03"}]}','3ʳᵈ'),
    ('ordinal_ordinary_04','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-04"}]}','4th'),
    ('ordinal_superscript_04','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-04"}]}','4ᵗʰ'),
    ('ordinal_ordinary_05','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-05"}]}','5th'),
    ('ordinal_superscript_05','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-05"}]}','5ᵗʰ'),
    ('ordinal_ordinary_06','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-06"}]}','6th'),
    ('ordinal_superscript_06','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-06"}]}','6ᵗʰ'),
    ('ordinal_ordinary_07','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-07"}]}','7th'),
    ('ordinal_superscript_07','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-07"}]}','7ᵗʰ'),
    ('ordinal_ordinary_08','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-08"}]}','8th'),
    ('ordinal_superscript_08','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-08"}]}','8ᵗʰ'),
    ('ordinal_ordinary_09','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-09"}]}','9th'),
    ('ordinal_superscript_09','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-09"}]}','9ᵗʰ'),
    ('ordinal_ordinary_10','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-10"}]}','10th'),
    ('ordinal_superscript_10','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-10"}]}','10ᵗʰ'),
    ('ordinal_ordinary_11','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-11"}]}','11th'),
    ('ordinal_superscript_11','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-11"}]}','11ᵗʰ'),
    ('ordinal_ordinary_12','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-12"}]}','12th'),
    ('ordinal_superscript_12','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-12"}]}','12ᵗʰ'),
    ('ordinal_ordinary_13','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-13"}]}','13th'),
    ('ordinal_superscript_13','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-13"}]}','13ᵗʰ'),
    ('ordinal_ordinary_14','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-14"}]}','14th'),
    ('ordinal_superscript_14','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-14"}]}','14ᵗʰ'),
    ('ordinal_ordinary_15','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-15"}]}','15th'),
    ('ordinal_superscript_15','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-15"}]}','15ᵗʰ'),
    ('ordinal_ordinary_16','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-16"}]}','16th'),
    ('ordinal_superscript_16','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-16"}]}','16ᵗʰ'),
    ('ordinal_ordinary_17','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-17"}]}','17th'),
    ('ordinal_superscript_17','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-17"}]}','17ᵗʰ'),
    ('ordinal_ordinary_18','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-18"}]}','18th'),
    ('ordinal_superscript_18','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-18"}]}','18ᵗʰ'),
    ('ordinal_ordinary_19','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-19"}]}','19th'),
    ('ordinal_superscript_19','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-19"}]}','19ᵗʰ'),
    ('ordinal_ordinary_20','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-20"}]}','20th'),
    ('ordinal_superscript_20','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-20"}]}','20ᵗʰ'),
    ('ordinal_ordinary_21','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-21"}]}','21st'),
    ('ordinal_superscript_21','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-21"}]}','21ˢᵗ'),
    ('ordinal_ordinary_22','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-22"}]}','22nd'),
    ('ordinal_superscript_22','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-22"}]}','22ⁿᵈ'),
    ('ordinal_ordinary_23','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-23"}]}','23rd'),
    ('ordinal_superscript_23','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-23"}]}','23ʳᵈ'),
    ('ordinal_ordinary_24','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-24"}]}','24th'),
    ('ordinal_superscript_24','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-24"}]}','24ᵗʰ'),
    ('ordinal_ordinary_25','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-25"}]}','25th'),
    ('ordinal_superscript_25','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-25"}]}','25ᵗʰ'),
    ('ordinal_ordinary_26','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-26"}]}','26th'),
    ('ordinal_superscript_26','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-26"}]}','26ᵗʰ'),
    ('ordinal_ordinary_27','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-27"}]}','27th'),
    ('ordinal_superscript_27','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-27"}]}','27ᵗʰ'),
    ('ordinal_ordinary_28','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-28"}]}','28th'),
    ('ordinal_superscript_28','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-28"}]}','28ᵗʰ'),
    ('ordinal_ordinary_29','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-29"}]}','29th'),
    ('ordinal_superscript_29','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-29"}]}','29ᵗʰ'),
    ('ordinal_ordinary_30','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-30"}]}','30th'),
    ('ordinal_superscript_30','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-30"}]}','30ᵗʰ'),
    ('ordinal_ordinary_31','render','{"pattern":"{:do}","args":[{"type":"datetime","value":"2026-01-31"}]}','31st'),
    ('ordinal_superscript_31','render','{"pattern":"{:do^}","args":[{"type":"datetime","value":"2026-01-31"}]}','31ˢᵗ')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('excel_currency_unicode_literal','render','{"pattern":"{:excel:€0.00}","args":[{"type":"double","value":"1.25"}]}','€1.25')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('excel_date_unicode_literal','render','{"pattern":"{:excel:yyyy年mm月dd日}","args":[{"type":"datetime","value":"2026-06-10"}]}','2026年06月10日')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('excel_fraction_integral','render','{"pattern":"{:excel:# ?/?}","args":[{"type":"double","value":"2"}]}','2    '),
    ('excel_fraction_round_integral','render','{"pattern":"{:excel:# ?/?}","args":[{"type":"double","value":"1.9999"}]}','2    '),
    ('excel_fraction_mandatory','render','{"pattern":"{:excel:0 0/0}","args":[{"type":"double","value":"2"}]}','2 0/1'),
    ('excel_fraction_three_places','render','{"pattern":"{:excel:# ???/???}","args":[{"type":"number","value":"7/13"}]}','   7/ 13'),
    ('excel_fraction_half','render','{"pattern":"{:excel:# ?/?}","args":[{"type":"number","value":"1/2"}]}',' 1/2'),
    ('excel_serial_leap_round','render','{"pattern":"{:excel:yyyy-mm-dd hh:mm:ss}","args":[{"type":"double","value":"59.999999"}]}','1900-02-29 00:00:00'),
    ('excel_serial_fiction_round','render','{"pattern":"{:excel:yyyy-mm-dd hh:mm:ss}","args":[{"type":"double","value":"60.999999"}]}','1900-03-01 00:00:00'),
    ('excel_date_leap','render','{"pattern":"{:excel:dddd, dd mmmm yyyy}","args":[{"type":"datetime","value":"2024-02-29"}]}','Thursday, 29 February 2024'),
    ('excel_date_minute_hour','render','{"pattern":"{:excel:hh:mm}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','13:05'),
    ('excel_date_fractions_nine','render','{"pattern":"{:excel:ss.000000000}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','06.125000000'),
    ('excel_date_fraction_carry','render','{"pattern":"{:excel:hh:mm:ss.00}","args":[{"type":"datetime","value":"2026-06-10T13:59:59.999"}]}','14:00:00.00'),
    ('excel_colour_blue','render','{"pattern":"{:excel:[Blue]0}","args":[{"type":"int","value":"2"}]}','[34m2[0m'),
    ('excel_colour_text','render','{"pattern":"{:excel:0;0;0;[Green]\"hello \"@}","args":[{"type":"string","value":"Ada"}]}','[32mhello Ada[0m'),
    ('excel_repeat_suffix','render','{"pattern":"{:>8:excel:0*-}","args":[{"type":"int","value":"12"}]}','12------'),
    ('excel_repeat_unicode','render','{"pattern":"{:>8:excel:*🙂0}","args":[{"type":"int","value":"12"}]}','🙂🙂🙂🙂🙂🙂12'),
    ('excel_repeat_coloured','render','{"pattern":"{:>8:excel:[Red]*-0}","args":[{"type":"int","value":"12"}]}','[31m------12[0m'),
    ('excel_mpfr_precise','render','{"pattern":"{:excel:0.000000000000000000000000000000000000000000000000000000000000}","args":[{"type":"number","value":"1.000000000000000000000000000000000000000000000000000000000001","digits":"100"}]}','1.000000000000000000000000000000000000000000000000000000000001'),
    ('excel_mpfr_half_away','render','{"pattern":"{:excel:0.00}","args":[{"type":"number","value":"0.125","digits":"100"}]}','0.13'),
    ('excel_mpfr_general','render','{"pattern":"{:excel:General}","args":[{"type":"number","value":"1.25","digits":"100"}]}','1.25'),
    ('excel_condition_less_equal','render','{"pattern":"{:excel:[<=2]\"yes\";\"no\"}","args":[{"type":"int","value":"2"}]}','yes'),
    ('excel_condition_exponent','render','{"pattern":"{:excel:[>=1E3]\"large\";\"small\"}","args":[{"type":"int","value":"1000"}]}','large'),
    ('excel_condition_exact_small','render','{"pattern":"{:excel:[<0.000000000000000000000000000001]\"small\";\"other\"}","args":[{"type":"number","value":"1/10000000000000000000000000000000"}]}','small'),
    ('excel_boolean','render','{"pattern":"{:excel:0}","args":[{"type":"bool","value":true}]}','1'),
    ('excel_number_real_complex','render','{"pattern":"{:excel:0.00}","args":[{"type":"number","value":"1.25+0i"}]}','1.25'),
    ('excel_notation','render','{"pattern":"{:latex}","args":[{"type":"TeX","value":"1/2"}]}','\frac{1}{2}'),
    ('excel_expression_native','render','{"pattern":"{:unbound}","args":[{"type":"expression","value":"x^2 + 1"}]}','x² + 1'),
    ('excel_equation_native','render','{"pattern":"{:unbound}","args":[{"type":"equation","value":"x^2 = 4"}]}','x² = 4')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
-- Excel custom codes use independent expected values; the C fixture retains ownership and assertions.
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('excel_fixed','render','{"pattern":"{:excel:0.00}","args":[{"type":"double","value":"1234.5"}]}','1234.50'),
    ('excel_grouping','render','{"pattern":"{:excel:#,##0.00}","args":[{"type":"double","value":"1234.5"}]}','1,234.50'),
    ('excel_integer_exact','render','{"pattern":"{:excel:#,##0}","args":[{"type":"number","value":"123456789012345678901234567890"}]}','123,456,789,012,345,678,901,234,567,890'),
    ('excel_unsigned_exact','render','{"pattern":"{:excel:0}","args":[{"type":"uint","value":"18446744073709551615"}]}','18446744073709551615'),
    ('excel_int_min','render','{"pattern":"{:excel:0}","args":[{"type":"int","value":"-9223372036854775808"}]}','-9223372036854775808'),
    ('excel_mandatory','render','{"pattern":"{:excel:000000}","args":[{"type":"int","value":"42"}]}','000042'),
    ('excel_optional','render','{"pattern":"{:excel:#}","args":[{"type":"int","value":"0"}]}',''),
    ('excel_optional_space','render','{"pattern":"{:excel:???}","args":[{"type":"int","value":"0"}]}','   '),
    ('excel_fractional_optional','render','{"pattern":"{:excel:0.0#}","args":[{"type":"double","value":"1.5"}]}','1.5'),
    ('excel_fractional_space','render','{"pattern":"{:excel:0.0?}","args":[{"type":"double","value":"1.5"}]}','1.5 '),
    ('excel_half_away','render','{"pattern":"{:excel:0}","args":[{"type":"double","value":"2.5"}]}','3'),
    ('excel_half_negative','render','{"pattern":"{:excel:0}","args":[{"type":"double","value":"-2.5"}]}','-3'),
    ('excel_round_carry','render','{"pattern":"{:excel:0.00}","args":[{"type":"double","value":"9.999"}]}','10.00'),
    ('excel_percent','render','{"pattern":"{:excel:0.0%}","args":[{"type":"double","value":"0.125"}]}','12.5%'),
    ('excel_repeated_percent','render','{"pattern":"{:excel:0%%}","args":[{"type":"double","value":"0.125"}]}','1250%%'),
    ('excel_scale_thousand','render','{"pattern":"{:excel:0.00,}","args":[{"type":"double","value":"1234.5"}]}','1.23'),
    ('excel_scale_million','render','{"pattern":"{:excel:#,##0,,}","args":[{"type":"double","value":"1234567890"}]}','1,235'),
    ('excel_scale_before_decimal','render','{"pattern":"{:excel:#,##0,.0}","args":[{"type":"double","value":"1234567"}]}','1,234.6'),
    ('excel_scientific','render','{"pattern":"{:excel:0.00E+00}","args":[{"type":"double","value":"1234.5"}]}','1.23E+03'),
    ('excel_scientific_minus','render','{"pattern":"{:excel:0.00E-00}","args":[{"type":"double","value":"1234.5"}]}','1.23E03'),
    ('excel_small_scientific','render','{"pattern":"{:excel:0.00e+00}","args":[{"type":"double","value":"0.00125"}]}','1.25e-03'),
    ('excel_engineering','render','{"pattern":"{:excel:##0.0E+0}","args":[{"type":"double","value":"12345"}]}','12.3E+3'),
    ('excel_scientific_carry','render','{"pattern":"{:excel:0.0E+00}","args":[{"type":"double","value":"9.999"}]}','1.0E+01'),
    ('excel_mixed_fraction','render','{"pattern":"{:excel:# ?/?}","args":[{"type":"double","value":"1.25"}]}','1 1/4'),
    ('excel_improper_fraction','render','{"pattern":"{:excel:??/??}","args":[{"type":"double","value":"1.25"}]}',' 5/ 4'),
    ('excel_fixed_fraction','render','{"pattern":"{:excel:# ?/8}","args":[{"type":"double","value":"1.25"}]}','1 2/8'),
    ('excel_two_digit_fraction','render','{"pattern":"{:excel:# ??/??}","args":[{"type":"number","value":"1/3"}]}','  1/ 3'),
    ('excel_qfloat_precision','render','{"pattern":"{:excel:0.000000000000000000000000000000}","args":[{"type":"qfloat","value":"1.000000000000000000000000000001"}]}','1.000000000000000000000000000001'),
    ('excel_rational_precision','render','{"pattern":"{:excel:0.00000000000000000000}","args":[{"type":"number","value":"1/3"}]}','0.33333333333333333333'),
    ('excel_rational_half','render','{"pattern":"{:excel:0.0}","args":[{"type":"number","value":"1/20"}]}','0.1'),
    ('excel_positive_section','render','{"pattern":"{:excel:+0.00;(0.00);\"zero\"}","args":[{"type":"double","value":"1.25"}]}','+1.25'),
    ('excel_negative_section','render','{"pattern":"{:excel:+0.00;(0.00);\"zero\"}","args":[{"type":"double","value":"-1.25"}]}','(1.25)'),
    ('excel_zero_section','render','{"pattern":"{:excel:+0.00;(0.00);\"zero\"}","args":[{"type":"double","value":"0"}]}','zero'),
    ('excel_negative_zero_section','render','{"pattern":"{:excel:+0.00;(0.00);\"zero\"}","args":[{"type":"double","value":"-0"}]}','zero'),
    ('excel_negative_zero_single','render','{"pattern":"{:excel:0.00}","args":[{"type":"double","value":"-0"}]}','-0.00'),
    ('excel_two_section_zero','render','{"pattern":"{:excel:0.0;(0.0)}","args":[{"type":"double","value":"-0"}]}','0.0'),
    ('excel_empty_section','render','{"pattern":"{:excel:0;;}","args":[{"type":"double","value":"-1"}]}',''),
    ('excel_hidden_number','render','{"pattern":"{:excel:;;;}","args":[{"type":"double","value":"1"}]}',''),
    ('excel_text_section','render','{"pattern":"{:excel:0;0;0;\"name: \"@}","args":[{"type":"string","value":"Ada"}]}','name: Ada'),
    ('excel_text_only','render','{"pattern":"{:excel:\"[\"@\"]\"}","args":[{"type":"text","value":"Ada"}]}','[Ada]'),
    ('excel_text_no_section','render','{"pattern":"{:excel:0.00}","args":[{"type":"text","value":"Ada"}]}','Ada'),
    ('excel_hidden_text','render','{"pattern":"{:excel:;;;}","args":[{"type":"string","value":"Ada"}]}',''),
    ('excel_condition_lt','render','{"pattern":"{:excel:[<10]\"small\";[>=10]0.0}","args":[{"type":"double","value":"2"}]}','small'),
    ('excel_condition_ge','render','{"pattern":"{:excel:[<10]\"small\";[>=10]0.0}","args":[{"type":"double","value":"12"}]}','12.0'),
    ('excel_condition_ne','render','{"pattern":"{:excel:[<>0]0.00;\"zero\"}","args":[{"type":"double","value":"0"}]}','zero'),
    ('excel_condition_exact','render','{"pattern":"{:excel:[>9007199254740992]\"large\";\"other\"}","args":[{"type":"number","value":"9007199254740993"}]}','large'),
    ('excel_condition_negative','render','{"pattern":"{:excel:[<-2]0.0;[>2]0.0;\"middle\"}","args":[{"type":"double","value":"-3"}]}','-3.0'),
    ('excel_condition_fallback','render','{"pattern":"{:excel:[<-2]0.0;[>2]0.0;\"middle\"}","args":[{"type":"double","value":"0"}]}','middle'),
    ('excel_condition_no_match','render','{"pattern":"{:excel:[<0]0}","args":[{"type":"double","value":"1"}]}','########'),
    ('excel_colour_red','render','{"pattern":"{:excel:[Red]0.00}","args":[{"type":"double","value":"1.25"}]}','[31m1.25[0m'),
    ('excel_colour_negative','render','{"pattern":"{:excel:0.00;[Red](0.00)}","args":[{"type":"double","value":"-1.25"}]}','[31m(1.25)[0m'),
    ('excel_colour_palette','render','{"pattern":"{:excel:[Color3]0}","args":[{"type":"int","value":"2"}]}','[38;2;255;0;0m2[0m'),
    ('excel_colour_width','render','{"pattern":"{:>10:excel:[Red]0.00}","args":[{"type":"double","value":"1.25"}]}','      [31m1.25[0m'),
    ('excel_repeat_width','render','{"pattern":"{:>10:excel:*-0.00}","args":[{"type":"double","value":"1.25"}]}','------1.25'),
    ('excel_repeat_no_width','render','{"pattern":"{:excel:*-0.00}","args":[{"type":"double","value":"1.25"}]}','1.25'),
    ('excel_spacing','render','{"pattern":"{:excel:0.00_)}","args":[{"type":"double","value":"1.25"}]}','1.25 '),
    ('excel_currency','render','{"pattern":"{:excel:[$£-809]#,##0.00}","args":[{"type":"double","value":"1234.5"}]}','£1,234.50'),
    ('excel_quoted_literal','render','{"pattern":"{:excel:\"value: \"0.0\" kg\"}","args":[{"type":"double","value":"1.5"}]}','value: 1.5 kg'),
    ('excel_quoted_braces','render','{"pattern":"{:excel:\"{x}=\"0}","args":[{"type":"int","value":"2"}]}','{x}=2'),
    ('excel_escaped_braces','render','{"pattern":"{:excel:\\{0\\}}","args":[{"type":"int","value":"2"}]}','{2}'),
    ('excel_escaped_semicolon','render','{"pattern":"{:excel:0\\;0}","args":[{"type":"int","value":"12"}]}','1;2'),
    ('excel_general','render','{"pattern":"{:excel:General}","args":[{"type":"double","value":"1.25"}]}','1.25'),
    ('excel_general_integer','render','{"pattern":"{:excel:General}","args":[{"type":"number","value":"123456789012345678901234567890"}]}','1.2345678901234567890123456789E+29'),
    ('excel_date_iso','render','{"pattern":"{:excel:yyyy-mm-dd hh:mm:ss.000}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','2026-06-10 13:05:06.125'),
    ('excel_date_names','render','{"pattern":"{:excel:dddd, d mmmm yyyy}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','Wednesday, 10 June 2026'),
    ('excel_date_abbreviated','render','{"pattern":"{:excel:ddd, dd mmm yy}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','Wed, 10 Jun 26'),
    ('excel_date_month_initial','render','{"pattern":"{:excel:mmmmm}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','J'),
    ('excel_date_twelve','render','{"pattern":"{:excel:h:mm:ss AM/PM}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','1:05:06 PM'),
    ('excel_date_period_short','render','{"pattern":"{:excel:hh:mm A/P}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','01:05 P'),
    ('excel_date_period_lower','render','{"pattern":"{:excel:hh:mm am/pm}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','01:05 pm'),
    ('excel_date_minute_context','render','{"pattern":"{:excel:mm:ss}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','05:06'),
    ('excel_date_month_context','render','{"pattern":"{:excel:mm-dd}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','06-10'),
    ('excel_date_single_day','render','{"pattern":"{:excel:d}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','10'),
    ('excel_date_single_month','render','{"pattern":"{:excel:m}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','6'),
    ('excel_date_single_second','render','{"pattern":"{:excel:s}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','6'),
    ('excel_date_long_date','render','{"pattern":"{:excel:[$-F800]}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','Wednesday, June 10, 2026'),
    ('excel_date_long_time','render','{"pattern":"{:excel:[$-F400]}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','13:05:06'),
    ('excel_date_native_iso','render','{"pattern":"{:iso}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','2026-06-10T13:05:06'),
    ('excel_date_midnight','render','{"pattern":"{:excel:h:mm AM/PM}","args":[{"type":"datetime","value":"2026-06-10T00:05:00"}]}','12:05 AM'),
    ('excel_date_round_day','render','{"pattern":"{:excel:yyyy-mm-dd hh:mm:ss}","args":[{"type":"datetime","value":"2026-12-31T23:59:59.75"}]}','2027-01-01 00:00:00'),
    ('excel_serial_epoch','render','{"pattern":"{:excel:yyyy-mm-dd}","args":[{"type":"int","value":"1"}]}','1900-01-01'),
    ('excel_serial_leap','render','{"pattern":"{:excel:yyyy-mm-dd}","args":[{"type":"int","value":"60"}]}','1900-02-29'),
    ('excel_serial_after_leap','render','{"pattern":"{:excel:yyyy-mm-dd}","args":[{"type":"int","value":"61"}]}','1900-03-01'),
    ('excel_serial_zero','render','{"pattern":"{:excel:yyyy-mm-dd}","args":[{"type":"int","value":"0"}]}','1900-01-00'),
    ('excel_serial_weekday','render','{"pattern":"{:excel:dddd}","args":[{"type":"int","value":"1"}]}','Sunday'),
    ('excel_serial_negative','render','{"pattern":"{:excel:yyyy-mm-dd}","args":[{"type":"int","value":"-1"}]}','########'),
    ('excel_elapsed_hours','render','{"pattern":"{:excel:[h]:mm:ss}","args":[{"type":"double","value":"1.5"}]}','36:00:00'),
    ('excel_elapsed_minutes','render','{"pattern":"{:excel:[m]:ss}","args":[{"type":"double","value":"1.5"}]}','2160:00'),
    ('excel_elapsed_seconds','render','{"pattern":"{:excel:[s].000}","args":[{"type":"double","value":"0.5"}]}','43200.000'),
    ('excel_complex_real','render','{"pattern":"{:excel:0.00}","args":[{"type":"complex","value":"(1.25,0)"}]}','1.25'),
    ('excel_error_fifth_section','error','{"pattern":"{:excel:0;0;0;@;0}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('excel_error_bad_condition','error','{"pattern":"{:excel:[<oops]0}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('excel_error_bad_colour','error','{"pattern":"{:excel:[Color57]0}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('excel_error_unclosed_bracket','error','{"pattern":"{:excel:[Red0}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('excel_error_bad_escape','error','{"pattern":"{:excel:0\\}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('excel_error_complex_nonreal','error','{"pattern":"{:excel:0.00}","args":[{"type":"complex","value":"(1,2)"}]}','type'),
    ('excel_error_pointer','error','{"pattern":"{:excel:0.00}","args":[{"type":"pointer","value":null}]}','type'),
    ('excel_error_excess_decimals','error','{"pattern":"{:excel:0.0.0}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('excel_error_bad_scientific','error','{"pattern":"{:excel:0E+}","args":[{"type":"int","value":"1"}]}','syntax'),
    ('excel_error_unsupported_calendar','error','{"pattern":"{:excel:[~japanese]yyyy}","args":[{"type":"datetime","value":"2026-06-10T13:05:06.125"}]}','syntax')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('native_text_precision_zeroes','render','{"pattern":"{:.00s}","args":[{"type":"text","value":"🙂"}]}',''),
    ('native_text_precision_width','render','{"pattern":"{:>4.000s}","args":[{"type":"string","value":"Ada"}]}','    '),
    ('native_month_two','render','{"pattern":"{:MM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','02'),
    ('native_minute_two','render','{"pattern":"{:mm}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','05'),
    ('native_lowercase_m_is_minute','render','{"pattern":"{:yyyy-mm-dd}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','2026-05-10'),
    ('native_case_sensitive_datetime','render','{"pattern":"{:yyyy-MM-dd hh:mm:ss}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','2026-02-10 13:05:06'),
    ('native_lower_period','render','{"pattern":"{:h:mm am/pm}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','1:05 pm'),
    ('native_upper_period','render','{"pattern":"{:h:mm AM/PM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','1:05 PM'),
    ('excel_uppercase_context_minutes','render','{"pattern":"{:excel:yyyy-mm-dd hh:MM:ss}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','2026-02-10 13:05:06')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('native_adjacent_lower_period','render','{"pattern":"{:h:mm:ssam/pm}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','1:05:06pm'),
    ('native_adjacent_upper_period','render','{"pattern":"{:h:mm:ssAM/PM}","args":[{"type":"datetime","value":"2026-02-10T13:05:06"}]}','1:05:06PM'),
    ('native_adjacent_midnight','render','{"pattern":"{:h:mm:ssam/pm}","args":[{"type":"datetime","value":"2026-02-10T00:05:06"}]}','12:05:06am')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
INSERT INTO format_cases(name,operation,input,expected) VALUES ('native_coloured_string_width','render','{"pattern":"{:>10s}","args":[{"type":"text","value":"\u001b[31m1.25\u001b[0m"}]}','      [31m1.25[0m')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

-- Every slot in the minimal perfect colour dictionary is exercised, including mixed case and non-members.
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('excel_colour_hash_black','render','{"pattern":"{:excel:[Black]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[30m2'||char(27)||'[0m'),
    ('excel_colour_hash_blue','render','{"pattern":"{:excel:[Blue]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[34m2'||char(27)||'[0m'),
    ('excel_colour_hash_cyan','render','{"pattern":"{:excel:[Cyan]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[36m2'||char(27)||'[0m'),
    ('excel_colour_hash_green','render','{"pattern":"{:excel:[Green]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[32m2'||char(27)||'[0m'),
    ('excel_colour_hash_magenta','render','{"pattern":"{:excel:[Magenta]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[35m2'||char(27)||'[0m'),
    ('excel_colour_hash_red','render','{"pattern":"{:excel:[Red]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[31m2'||char(27)||'[0m'),
    ('excel_colour_hash_white','render','{"pattern":"{:excel:[White]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[37m2'||char(27)||'[0m'),
    ('excel_colour_hash_yellow','render','{"pattern":"{:excel:[Yellow]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[33m2'||char(27)||'[0m'),
    ('excel_colour_hash_mixed','render','{"pattern":"{:excel:[mAgEnTa]0}","args":[{"type":"int","value":"2"}]}',char(27)||'[35m2'||char(27)||'[0m'),
    ('excel_colour_hash_nonmember','error','{"pattern":"{:excel:[Redd]0}","args":[{"type":"int","value":"2"}]}','syntax')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

-- Native datetime presentation names occupy every slot in the module-owned minimal perfect hash.
INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('lingua_default','render','{"pattern":"{dt}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','10 juin 2026'),
    ('lingua_explicit_default','render','{"pattern":"{dt:lingua}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','10 juin 2026'),
    ('lingua_french','render','{"pattern":"{dt:fr_FR}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','10 juin 2026'),
    ('lingua_french_first','render','{"pattern":"{dt:fr_FR}","args":[{"name":"dt","type":"datetime","value":"2026-06-01T00:00:00"}]}','1ᵉʳ juin 2026'),
    ('lingua_spanish','render','{"pattern":"{dt:es_ES}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','10 de junio de 2026'),
    ('lingua_welsh','render','{"pattern":"{dt:cy_GB}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','10 Mehefin 2026'),
    ('lingua_greek','render','{"pattern":"{dt:el_GR}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','10 Ιουνίου 2026'),
    ('lingua_russian','render','{"pattern":"{dt:ru_RU}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','10 июня 2026 г.'),
    ('lingua_japanese','render','{"pattern":"{dt:ja_JP}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','2026年6月10日'),
    ('lingua_hebrew_new_year','render','{"pattern":"{dt:he_IL}","args":[{"name":"dt","type":"datetime","value":"2024-10-03T00:00:00"}]}','1 בתשרי 5785'),
    ('lingua_hebrew_adar_first','render','{"pattern":"{dt:he_IL}","args":[{"name":"dt","type":"datetime","value":"2024-02-10T00:00:00"}]}','1 באדר א׳ 5784'),
    ('lingua_hebrew_adar_second','render','{"pattern":"{dt:he_IL}","args":[{"name":"dt","type":"datetime","value":"2024-03-11T00:00:00"}]}','1 באדר ב׳ 5784'),
    ('lingua_hebrew_adar_common','render','{"pattern":"{dt:he_IL}","args":[{"name":"dt","type":"datetime","value":"2023-02-22T00:00:00"}]}','1 באדר 5783'),
    ('lingua_arabic_new_year','render','{"pattern":"{dt:ar_SA}","args":[{"name":"dt","type":"datetime","value":"2025-06-27T00:00:00"}]}','١ محرم ١٤٤٧'),
    ('lingua_uk_ordinal','render','{"pattern":"{dt:en_GB}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T00:00:00"}]}','10ᵗʰ June 2026'),
    ('lingua_uk_eleventh','render','{"pattern":"{dt:en_GB}","args":[{"name":"dt","type":"datetime","value":"2026-06-11T00:00:00"}]}','11ᵗʰ June 2026'),
    ('lingua_uk_twenty_first','render','{"pattern":"{dt:en_GB}","args":[{"name":"dt","type":"datetime","value":"2026-06-21T00:00:00"}]}','21ˢᵗ June 2026'),
    ('lingua_unknown_locale','error','{"pattern":"{dt:zz_ZZ}","args":[{"name":"dt","type":"datetime","value":"2026-06-10T13:05:06"}]}','type')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('native_datetime_hash_month','render','{"pattern":"{:M}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','2'),
    ('native_datetime_hash_date','render','{"pattern":"{:date}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','2026-02-10'),
    ('native_datetime_hash_minute','render','{"pattern":"{:m}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','5'),
    ('native_datetime_hash_hour','render','{"pattern":"{:h}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','13'),
    ('native_datetime_hash_second','render','{"pattern":"{:s}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','6'),
    ('native_datetime_hash_day','render','{"pattern":"{:d}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','10'),
    ('native_datetime_hash_datetime','render','{"pattern":"{:datetime}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','2026-02-10T13:05:06'),
    ('native_datetime_hash_iso','render','{"pattern":"{:iso}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','2026-02-10T13:05:06'),
    ('native_datetime_hash_year','render','{"pattern":"{:y}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','26'),
    ('native_datetime_hash_time','render','{"pattern":"{:time}","args":[{"type":"datetime","value":"2026-02-10T13:05:06.125"}]}','13:05:06')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('native_repeated_year_one_field','render','{"pattern":"Chinese New Year {dt:yyyy: yyyy-MM-dd}","args":[{"name":"dt","type":"datetime","value":"2020-01-25T00:00:00"}]}','Chinese New Year 2020: 2020-01-25')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;

INSERT INTO format_cases(name,operation,input,expected) VALUES
    ('calendar_christian','render','{"pattern":"{dt:christian}","args":[{"name":"dt","type":"datetime","value":"2026-06-21T00:00:00"}]}','Gregorian 2026-06-21; Julian 2026-06-08'),
    ('calendar_chinese','render','{"pattern":"{dt:chinese}","args":[{"name":"dt","type":"datetime","value":"2026-02-17T00:00:00"}]}','Year 4724 (Horse), month 1, day 1'),
    ('calendar_hindu','render','{"pattern":"{dt:hindu}","args":[{"name":"dt","type":"datetime","value":"2026-06-21T00:00:00"}]}','Vikram Samvat 2083, Ashadha Shukla 7, lunar day 7'),
    ('calendar_buddhist','render','{"pattern":"{dt:buddhist}","args":[{"name":"dt","type":"datetime","value":"2026-06-21T00:00:00"}]}','B.E. 2569-06-21 (Thai solar)'),
    ('calendar_muslim','render','{"pattern":"{dt:muslim}","args":[{"name":"dt","type":"datetime","value":"2026-02-17T00:00:00"}]}','29 Sha''ban 1447 AH'),
    ('calendar_jewish','render','{"pattern":"{dt:jewish}","args":[{"name":"dt","type":"datetime","value":"2024-10-03T00:00:00"}]}','1 Tishrei 5785 AM'),
    ('calendar_cherokee','render','{"pattern":"{dt:cherokee}","args":[{"name":"dt","type":"datetime","value":"2026-06-21T00:00:00"}]}','Cherokee civil Green Corn Moon, day 21, year 2026'),
    ('calendar_mayan','render','{"pattern":"{dt:mayan}","args":[{"name":"dt","type":"datetime","value":"2026-06-21T00:00:00"}]}','Long Count 13.0.13.12.10; Tzolk''in 7 Ok; Haab 3 Sek'),
    ('calendar_aztec','render','{"pattern":"{dt:aztec}","args":[{"name":"dt","type":"datetime","value":"2026-06-21T00:00:00"}]}','Tonalpohualli 5 Mazatl; Xiuhpohualli day 19 of Etzalcualiztli; year 1 Tochtli'),
    ('calendar_ethiopian','render','{"pattern":"{dt:ethiopian}","args":[{"name":"dt","type":"datetime","value":"2023-09-11T00:00:00"}]}','1 Meskerem 2016 EC'),
    ('calendar_unknown_name','error','{"pattern":"{:mayan_extra}","args":[{"type":"datetime","value":"2026-06-21T00:00:00"}]}','type')
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation,input=excluded.input,expected=excluded.expected;
