-- TeX test vectors; all assertions remain in the native C case callbacks.
CREATE TABLE IF NOT EXISTS TeX_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS TeX_cases_operation ON TeX_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_calculus_shorthand_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_calculus_shorthand', 'TeX_cases', 'test_TeX_calculus_shorthand_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_calculus_shorthand. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6cb7f5fc3c248cd8', 'test_TeX_calculus_shorthand_cases', '@F{exp(-x^2)}', '\mathcal{F}_{x\to k}\{e^{-x^{2}}\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_836ac096c7e0808a', 'test_TeX_calculus_shorthand_cases', 'Fourier(exp(-x^2),x,k)', '\mathcal{F}_{x\to k}\{e^{-x^{2}}\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_73b8dd3d036d120b', 'test_TeX_calculus_shorthand_cases', 'ℱ{exp(-x^2)}', '\mathcal{F}_{x\to k}\{e^{-x^{2}}\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_ca7a7c1beeffbd9d', 'test_TeX_calculus_shorthand_cases', 'sin(@F{exp(-x^2)})', '\sin(\mathcal{F}_{x\to k}\{e^{-x^{2}}\})');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_56706c827e076076', 'test_TeX_calculus_shorthand_cases', '@Finv{exp(-k^2)}', '\mathcal{F}^{-1}_{k\to x}\{e^{-k^{2}}\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_27dda9b83308d960', 'test_TeX_calculus_shorthand_cases', '@L{sin(t)}', '\mathcal{L}_{t\to s}\{\sin(t)\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_a32b25b0dd4c392c', 'test_TeX_calculus_shorthand_cases', '@Linv{1/(s+1)}', '\mathcal{L}^{-1}_{s\to t}\{\frac{1}{s + 1}\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_d084a5f0b75ad564', 'test_TeX_calculus_shorthand_cases', '@P_(k=1)^3 k', '\prod_{k=1}^{3}k');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_e13ee8dd2072053a', 'test_TeX_calculus_shorthand_cases', 'lambertw0(x)', 'W_{0}(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_37442b2819f42a76', 'test_TeX_calculus_shorthand_cases', 'BesselJ(2,x)', 'J_{2}\left(x\right)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8db1a8e007daff05', 'test_TeX_calculus_shorthand_cases', 'normalpdf(x)', '\operatorname{normal\_pdf}(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8e884735ee17ad9f', 'test_TeX_calculus_shorthand_cases', 'gamma(x)', '\Gamma(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_ed87aa131c6cd004', 'test_TeX_calculus_shorthand_cases', 'conj(x)', '\overline{x}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_3ca9a59d9e674005', 'test_TeX_calculus_shorthand_cases', 'Re(x)', '\mathfrak{Re}\mathopen{}\left\{x\right\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8344f36e62e9ef1c', 'test_TeX_calculus_shorthand_cases', 'Im(x)', '\mathfrak{Im}\mathopen{}\left\{x\right\}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6b74781dd3f29dcc', 'test_TeX_calculus_shorthand_cases', 'zeta(x)', '\zeta(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_fa7ebba455d54fd5', 'test_TeX_calculus_shorthand_cases', 'erf(x)', '\operatorname{erf}(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_05f9ed2a078d9600', 'test_TeX_calculus_shorthand_cases', 'sinh(x)', '\sinh(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_67af3f291c7ca468', 'test_TeX_calculus_shorthand_cases', '@inf', '\infty');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_aa60dbc6d6b70299', 'test_TeX_calculus_shorthand_cases', '@infinity', '\infty');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_ed3971f8f06f6559', 'test_TeX_calculus_shorthand_cases', '-@inf', '-\infty');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6eac39d81a011051', 'test_TeX_calculus_shorthand_cases', '∞', '\infty');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_79fb59ed8b19e967', 'test_TeX_calculus_shorthand_cases', '@eulermascheroni', '\gamma');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c4bb7efef6e2c0ca', 'test_TeX_calculus_shorthand_cases', '@gamma', '\gamma');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_a34071ab2a3fda88', 'test_TeX_calculus_shorthand_cases', 'γ', '\gamma');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_43bb40d64f9c8f12', 'test_TeX_calculus_shorthand_cases', '@pi', '\pi');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_b3c18b294709d444', 'test_TeX_calculus_shorthand_cases', '@phi', '\phi');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_92e6ef2934f087e9', 'test_TeX_calculus_shorthand_cases', '@tau', '\tau');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_fa2899cb6d8e0c26', 'test_TeX_calculus_shorthand_cases', 'e', 'e');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_fa28a5cb6d8e208a', 'test_TeX_calculus_shorthand_cases', 'i', 'i');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_987d13c92c5d9380', 'test_TeX_calculus_shorthand_cases', '@S_0^(@inf) exp(-x) dx', '\int_{0}^{\infty} e^{-x}\, dx');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6791b45b5e2f0c32', 'test_TeX_calculus_shorthand_cases', '@S x^2 dx', '\int x^{2}\, dx');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_47c5faa26aa237cc', 'test_TeX_calculus_shorthand_cases', '@S_0^1 x^2 dx', '\int_{0}^{1} x^{2}\, dx');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_e6e21f390ff8d83e', 'test_TeX_calculus_shorthand_cases', '@Z_(k=1)^3 k^2', '\sum_{k=1}^{3}k^{2}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_e51f415f91886956', 'test_TeX_calculus_shorthand_cases', 'd/dx(sin(x))', '\frac{\partial}{\partial x}\left[\sin(x)\right]');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6d6c2413a4faede2', 'test_TeX_calculus_shorthand_cases', 'd^2/dx^2(sin(x))', '\frac{\partial^{2}}{\partial x^{2}}\left[\sin(x)\right]');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_581d0a4449fe62d2', 'test_TeX_calculus_shorthand_cases', 'd^2/dxy(x^2*y^3)', '\frac{\partial^{2}}{\partial y\,\partial x}\left[x^{2}\mkern-2mu y^{3}\right]');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_f7554755a7fbb363', 'test_TeX_calculus_shorthand_cases', 'Dxy(x^2*y^3)', '\frac{\partial^{2}}{\partial y\,\partial x}\left[x^{2}\mkern-2mu y^{3}\right]');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c6c73e3222c5dcbf', 'test_TeX_calculus_shorthand_cases', 'Dxyx(x^3*y^2)', '\frac{\partial^{3}}{\partial x\,\partial y\,\partial x}\left[x^{3}\mkern-2mu y^{2}\right]');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_567989fb87981811', 'test_TeX_calculus_shorthand_cases', 'D[x^2y](x^3*y^2)', '\frac{\partial^{3}}{\partial y\,\partial x^{2}}\left[x^{3}\mkern-2mu y^{2}\right]');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_calculus_shorthand_invalid', 'tests/TeX/test_TeX.c', 'tests.test_TeX_calculus_shorthand', 'TeX_cases', 'test_TeX_calculus_shorthand_invalid')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_calculus_shorthand. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_749e9bb07c0306d6', 'test_TeX_calculus_shorthand_invalid', 'd^2/dx^3(x)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c26f21bb6cd14525', 'test_TeX_calculus_shorthand_invalid', 'd^0/dx(x)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_947ec1a311c49ab0', 'test_TeX_calculus_shorthand_invalid', 'd^2/dxyx(x)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_74fc9dc60ae3bf96', 'test_TeX_calculus_shorthand_invalid', 'd^2/dx^s(x)', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_expression_style_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_expression_style', 'TeX_cases', 'test_TeX_expression_style_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_expression_style. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_552d755d6927e3a4', 'test_TeX_expression_style_cases', '{sqrt(x^2+c)/(x+1) | x=2; c=4}', '\frac{\sqrt{x^{2} + c}}{x + 1}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_328edc20fd2a492d', 'test_TeX_expression_style_cases', '{x²+c | x=[1,2,3]; c=4}', 'x^{2} + c');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_98fa96d052eb0ee5', 'test_TeX_expression_style_cases', '{[radius]^2 | [radius]=5}', '\mathit{radius}^{2}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_99ac50e627238aab', 'test_TeX_expression_style_cases', '{sin²(x)+cos²(x) | x=?}', '1');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_3da7dcec54fcca2a', 'test_TeX_expression_style_cases', '{x_0+@alpha | x_0=?; @alpha=2}', 'x_{0} + \alpha');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_e89199ed506a27bd', 'test_TeX_expression_style_cases', '{120/s^6 where (Re(s)>0) | s=2}', '\frac{120}{s^{6}}\quad \left( \mathfrak{Re}(s) > 0 \right)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_4de2c665507e94c8', 'test_TeX_expression_style_cases', '@F{exp(-x^2)}', '\sqrt{\pi}\mkern-2mu e^{-\frac{1}{4}\mkern-2mu k^{2}}\quad \left( k\in \mathbb{R} \right)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_84f391758b57d63c', 'test_TeX_expression_style_cases', '@S_0^1 x^2 dx', '\int_{0}^{1} x^{2}\, dx');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_relation_input_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_relation_input', 'TeX_cases', 'test_TeX_relation_input_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_relation_input. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_19f265ef9f6c63ed', 'test_TeX_relation_input_cases', 'x < y', 'x < y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c14f3aedcb580998', 'test_TeX_relation_input_cases', 'x<=y', 'x \le y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_5843c31ab34a2c50', 'test_TeX_relation_input_cases', 'x >= y', 'x \ge y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_b9d4c76b10ed2e6b', 'test_TeX_relation_input_cases', 'x>y', 'x > y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_77044ad01756714f', 'test_TeX_relation_input_cases', 'x != y', 'x \ne y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_dbd74335349afd83', 'test_TeX_relation_input_cases', 'x == y', 'x = y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_23758eefa50d7b02', 'test_TeX_relation_input_cases', 'x = y', 'x = y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c3cb331c440a2c7e', 'test_TeX_relation_input_cases', 'x ~~= y', 'x \approx y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_44c938fde18f6705', 'test_TeX_relation_input_cases', 'x ~== y', 'x \cong y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_733f04a09757ddf2', 'test_TeX_relation_input_cases', 'x ≤ y', 'x \le y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6a95dba092706a9d', 'test_TeX_relation_input_cases', 'x ≥ y', 'x \ge y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_517258a0847040c6', 'test_TeX_relation_input_cases', 'x ≠ y', 'x \ne y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_abedc0a148b05d0e', 'test_TeX_relation_input_cases', 'x ≈ y', 'x \approx y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_809dfba13027c3fd', 'test_TeX_relation_input_cases', 'x ≅ y', 'x \cong y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_fc6039a75390832e', 'test_TeX_relation_input_cases', '0 < x <= 1', '0 < x \le 1');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_97056f16c8fa11b4', 'test_TeX_relation_input_cases', 'a>=b>c!=d==e~~=f~==g', 'a \ge b > c \ne d = e \approx f \cong g');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_38dc861a6fe77277', 'test_TeX_relation_input_cases', 'x=y=z', 'x = y = z');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_247be9e9cf7e73b6', 'test_TeX_relation_input_cases', '{x <= y | x = 3, y = 4}', 'x \le y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_5862f3f3a2b00c36', 'test_TeX_relation_input_cases', 'sin(x)~~=x', '\sin(x) \approx x');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c204faad735bd739', 'test_TeX_relation_input_cases', '@Z_(k=1)^3 k == 6', '\sum_{k=1}^{3}k = 6');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_52e075fd6e649640', 'test_TeX_relation_input_cases', '{x | x != 0}', 'x\quad \left( x \ne 0 \right)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_b81d6a39a6735fac', 'test_TeX_relation_input_cases', '{x | x = 1}', 'x');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_b563d68901c777b0', 'test_TeX_relation_input_cases', '(1,2;3,4) == (5,6;7,8)', '\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix} = \begin{bmatrix}5 & 6 \\[10pt] 7 & 8\end{bmatrix}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_relation_input_invalid_invalid', 'tests/TeX/test_TeX.c', 'tests.test_TeX_relation_input_invalid', 'TeX_cases', 'test_TeX_relation_input_invalid_invalid')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_relation_input_invalid. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_80f3d36a48cf4288', 'test_TeX_relation_input_invalid_invalid', 'x <', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_681c058e7b8dedb4', 'test_TeX_relation_input_invalid_invalid', '<= y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_a17af3a0c6924940', 'test_TeX_relation_input_invalid_invalid', 'x < <= y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_49021f9de084831a', 'test_TeX_relation_input_invalid_invalid', 'x === y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9542b8460a0ca796', 'test_TeX_relation_input_invalid_invalid', 'x ~= y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9196eb97a5a81d70', 'test_TeX_relation_input_invalid_invalid', 'x ~=== y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_5226fe2d81e45079', 'test_TeX_relation_input_invalid_invalid', 'x ~~== y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_cff65b37b48c423c', 'test_TeX_relation_input_invalid_invalid', 'x < = y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8b45115b92953201', 'test_TeX_relation_input_invalid_invalid', 'x >< y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_ed51f699b7f6c966', 'test_TeX_relation_input_invalid_invalid', 'x !=', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6b530f34d699490c', 'test_TeX_relation_input_invalid_invalid', 'x ≅', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_958834839f5425ee', 'test_TeX_relation_input_invalid_invalid', 'x <= y >', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_7218607abc11f5a0', 'test_TeX_relation_input_invalid_invalid', 'x) < y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_5212fd4c0bb808cb', 'test_TeX_relation_input_invalid_invalid', 'x < (y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_3ce836d756fe34b4', 'test_TeX_relation_input_invalid_invalid', 'sin(x<y)', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_matrix_input_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_matrix_input', 'TeX_cases', 'test_TeX_matrix_input_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_matrix_input. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_ad90d99d8898ea51', 'test_TeX_matrix_input_cases', '(1, 2; 3, 4)^x', '{\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_ad908f9d88986c93', 'test_TeX_matrix_input_cases', '(1, 2; 3, 4)^2', '{\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{2}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_cd0925b77a908200', 'test_TeX_matrix_input_cases', ' (1 2; 3 4) ^ (x+1) ', '{\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x + 1}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_bea79a0a001e7d86', 'test_TeX_matrix_input_cases', '(x, 1; 0, y)^(x+1)', '{\begin{bmatrix}x & 1 \\[10pt] 0 & y\end{bmatrix}}^{x + 1}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_aa4f888af48637a3', 'test_TeX_matrix_input_cases', '(1,2)', '\begin{bmatrix}1 & 2\end{bmatrix}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_fe2f5ec06539ba82', 'test_TeX_matrix_input_cases', '(sin(x), root(8,3); 3,4)^(@alpha+1)', '{\begin{bmatrix}\sin(x) & 2 \\[10pt] 3 & 4\end{bmatrix}}^{\alpha + 1}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_78e15e2bf0dbd518', 'test_TeX_matrix_input_cases', '(x+1)^2', '\left(x + 1\right)^{2}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_matrix_input_invalid', 'tests/TeX/test_TeX.c', 'tests.test_TeX_matrix_input', 'TeX_cases', 'test_TeX_matrix_input_invalid')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_matrix_input. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_eb798c4ae3086ad3', 'test_TeX_matrix_input_invalid', '(1,2;3)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_27fe543fc7560c08', 'test_TeX_matrix_input_invalid', '(1,2;3,4', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_0227faa451cb96d7', 'test_TeX_matrix_input_invalid', '(1,2;3,4)^', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9a32adfa56b0cb29', 'test_TeX_matrix_input_invalid', '(1,2;3,4)^x+1', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_47bf06066630e31e', 'test_TeX_matrix_input_invalid', '(1,2;3,4)^(x)+1', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9a49fdfa56c431fa', 'test_TeX_matrix_input_invalid', '(1,2;3,4)^x y', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_4106686b81f54cc5', 'test_TeX_matrix_input_invalid', '(1,2;3,4)junk', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_bound_problems_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_bound_problems', 'TeX_cases', 'test_TeX_bound_problems_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_bound_problems. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_b06ab1089b769416', 'test_TeX_bound_problems_cases', '{ (1, 2; 3, 4)^x | x = 3 }', '{\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_3f2088d0ff7edfd1', 'test_TeX_bound_problems_cases', '{(x,1;0,x)^x | x=3}', '{\begin{bmatrix}x & 1 \\[10pt] 0 & x\end{bmatrix}}^{x}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c72fbbfe292cc517', 'test_TeX_bound_problems_cases', '{(1,2;3,4)^(x+1) | x=3}', '{\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x + 1}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_121a8d1d2c595fde', 'test_TeX_bound_problems_cases', '{x | x=3}', 'x');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8d501b9b583b62eb', 'test_TeX_bound_problems_cases', '{ y'''' + 3y'' + 40y | y''(0) = 1, y(0) = 0 }', 'y'''' + 3\mkern-2mu y'' + 40\mkern-2mu y = 0,\qquad y''\left(0\right) = 1,\qquad y\left(0\right) = 0');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_dd9921e756ad5b62', 'test_TeX_bound_problems_cases', '{ Dx(y) = x*y | x = ?;; y(0) = 1 }', '\frac{d y}{d x} = x\mkern-2mu y,\qquad \left. y\right|_{x=0} = 1');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_d2ead18f212c7e50', 'test_TeX_bound_problems_cases', '{ Dx(u)+Dy(u)=0 | x=?, y=?;; u(x,0)=sin(x) }', '\frac{\partial u}{\partial x} + \frac{\partial u}{\partial y} = 0,\qquad \left. u\right|_{x=x,\,y=0} = \sin(x)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_bound_problems_invalid', 'tests/TeX/test_TeX.c', 'tests.test_TeX_bound_problems', 'TeX_cases', 'test_TeX_bound_problems_invalid')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_bound_problems. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8d4fc8de1bb8ef0b', 'test_TeX_bound_problems_invalid', '{(1,2;3,4)^x | x=}', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_199bebdac9ce72df', 'test_TeX_bound_problems_invalid', '{y''''+y|y''(0)=1,y(0)=}', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_4932f0eeecbc3364', 'test_TeX_bound_problems_invalid', '{(1,2;3,4)^x | x=3} trailing', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8d4f86de1bb87ee5', 'test_TeX_bound_problems_invalid', '{(1,2;3,4)^x | x=3', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_prime_input_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_prime_input', 'TeX_cases', 'test_TeX_prime_input_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_prime_input. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_a459d78497d16ef1', 'test_TeX_prime_input_cases', 'y''''+y=0', 'y'''' + y = 0');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_1ab686a8230cbcbe', 'test_TeX_prime_input_cases', '{y''''''''+y | y''''''(0)=1, y(0)=0}', 'y^{(4)} + y = 0,\qquad y''''''\left(0\right) = 1,\qquad y\left(0\right) = 0');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_39305b067028050f', 'test_TeX_prime_input_cases', '{z''''+z | z''(1)=2, z(1)=0}', 'z'''' + z = 0,\qquad z''\left(1\right) = 2,\qquad z\left(1\right) = 0');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_107caa99b7a07fe0', 'test_TeX_prime_input_cases', '{ y''''+y=0 | t=?;; y''(0)=1 }', 'y'''' + y = 0,\qquad y''\left(0\right) = 1');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_d1f99c04531c0627', 'test_TeX_prime_input_cases', '{Dx(y)+y=0 | x=?;; y''(0)=1}', '\frac{d y}{d x} + y = 0,\qquad \left. \frac{d y}{d x}\right|_{x=0} = 1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_native_text_array_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_native_text_array', 'TeX_cases', 'test_TeX_native_text_array_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_native_text_array. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_939f9ff19d3df57e', 'test_TeX_native_text_array_cases', 'r_(n) = root(1+x_(n)^2,3)/sqrt(1+@lambda^2)', 'r_{n} = \frac{\sqrt[3]{x_{n}^{2} + 1}}{\sqrt{\lambda^{2} + 1}}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_53ee506d209e0546', 'test_TeX_native_text_array_cases', 'P(x) = 1/(1+x^2)*(1,x;x,x^2)', 'P\left(x\right) = \frac{1}{x^{2} + 1}\mkern-5mu \begin{bmatrix}1 & x \\[10pt] x & x^{2}\end{bmatrix}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_3be3ee2e9b341593', 'test_TeX_native_text_array_cases', 'i*h*Dt(@psi) = -h^2/(2*m)*Dxx(@psi)+V*@psi', 'i\mkern-2mu h\mkern-2mu \frac{\partial}{\partial t}\left[\psi\right] = -\frac{h^{2}}{2\mkern-2mu m}\mkern-2mu \frac{\partial^{2}}{\partial x^{2}}\left[\psi\right] + V\mkern-2mu \psi');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_a77d3e3ff5cc94c7', 'test_TeX_native_text_array_cases', '{x=y|x=3;y=4}', 'x = y');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_0f4a2ed6d760fb1e', 'test_TeX_native_text_array_cases', '2*(1,2;3,4)', '2\mkern-5mu \begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6547c2f33f9ba746', 'test_TeX_native_text_array_cases', '(1,2;3,4)*2', '2\mkern-5mu \begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_d1469a592aab2329', 'test_TeX_native_text_array_cases', '-2*(1,2)', '-2\mkern-5mu \begin{bmatrix}1 & 2\end{bmatrix}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_14732c91c8474571', 'test_TeX_native_text_array_cases', '(1,2)=(3,4)', '\begin{bmatrix}1 & 2\end{bmatrix} = \begin{bmatrix}3 & 4\end{bmatrix}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c3437f3d2a4c9c03', 'test_TeX_native_text_array_cases', 'r_n', 'r_{n}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_606ed1d3a3a204e2', 'test_TeX_native_text_array_cases', 'x_(n+1)', 'x_{n + 1}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_native_text_array_invalid', 'tests/TeX/test_TeX.c', 'tests.test_TeX_native_text_array', 'TeX_cases', 'test_TeX_native_text_array_invalid')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_native_text_array. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_80810f48c81c958d', 'test_TeX_native_text_array_invalid', 'x=', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_7f9d4848c75afe23', 'test_TeX_native_text_array_invalid', '=x', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_5af64352bf1c0600', 'test_TeX_native_text_array_invalid', '2*(1,2;3)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_09fab95a4d92ed80', 'test_TeX_native_text_array_invalid', '(1,2)*', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_4c6c2b5323ed781c', 'test_TeX_native_text_array_invalid', 'x_()', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_510645060368b255', 'test_TeX_native_text_array_invalid', 'x_(n+1', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c6cee817efd95456', 'test_TeX_native_text_array_invalid', '1/(1+x^2)+2*(1,2)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_96c23b85abfe3280', 'test_TeX_native_text_array_invalid', '(1,2)*(3,4)', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_native_index_roundtrip_inputs', 'tests/TeX/test_TeX.c', 'tests.test_TeX_native_index_roundtrip', 'TeX_cases', 'test_TeX_native_index_roundtrip_inputs')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_native_index_roundtrip. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_1c403f6553a51fb9', 'test_TeX_native_index_roundtrip_inputs', 'x_(n+1)^2', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_151cdf8fe0599095', 'test_TeX_native_index_roundtrip_inputs', 'r_n+x_(n)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_2260ac7f1732ee8d', 'test_TeX_native_index_roundtrip_inputs', '{x_(n)|n=3}', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_d31f5e60fce31a3a', 'test_TeX_native_index_roundtrip_inputs', '@S_0^1 x^2 dx', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_real_interval_domains_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_real_interval_domains', 'TeX_cases', 'test_TeX_real_interval_domains_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_real_interval_domains. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_d2214ce45769403c', 'test_TeX_real_interval_domains_cases', '{x | x=?; x ∈ ℝ; Re(x)>0; Re(pi)>x}', 'x\quad (0 < x < \pi)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_f33ac7820f07e0aa', 'test_TeX_real_interval_domains_cases', '{x | x=?; Re(pi)>x; Re(x)>0; x ∈ ℝ}', 'x\quad (0 < x < \pi)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9e0d9351f1fbe79f', 'test_TeX_real_interval_domains_cases', '{x | x=?; x ∈ ℝ; Re(x)>-1; Re(2)>x}', 'x\quad (-1 < x < 2)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_05781ec3b13bb084', 'test_TeX_real_interval_domains_cases', '{x | x=?; Re(x)>0}', 'x\quad (\mathfrak{Re}(x)>0)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_485507ec12c448f0', 'test_TeX_real_interval_domains_cases', '{x | x=?; x ≠ 0; x ∈ ℝ; Re(x)>-1; Re(1)>x}', 'x\quad (x\ne 0,\;-1 < x < 1)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_49d6fab1b21585ff', 'test_TeX_real_interval_domains_cases', '{x | x=?; x ∈ ℝ}', 'x\quad (x\in\mathbb{R})');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_1c3acd19e4ffd1e6', 'test_TeX_real_interval_domains_cases', '{x | x=?; x ∈ ℝ; Re(x)>0}', 'x\quad (x>0)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_31e36633964153f7', 'test_TeX_real_interval_domains_cases', '{x | x=?, a=?; a ∈ ℝ; x ∈ ℝ; Re(a)>0; Re(x)>a; Re(1)>x}', 'x\quad (a>0,\;x>a,\;1>x)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_native_aggregate_contexts_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_native_aggregate_contexts', 'TeX_cases', 'test_TeX_native_aggregate_contexts_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_native_aggregate_contexts. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_22ba77e42ef3e109', 'test_TeX_native_aggregate_contexts_cases', '@Z_(k=1)^n k^2', '\sum_{k=1}^{n}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_7836fc8027926fcb', 'test_TeX_native_aggregate_contexts_cases', '@P_(k=1)^n k', '\prod_{k=1}^{n}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_d6ff6c974e5505cc', 'test_TeX_native_aggregate_contexts_cases', '@L(f(t),t,s)', '\mathcal{L}_{t\to s}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_69fd17cf4ff47376', 'test_TeX_native_aggregate_contexts_cases', '@F(f(t),t,w)', '\mathcal{F}_{t\to w}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6db0b015313d4156', 'test_TeX_native_aggregate_contexts_cases', 'sin(x)', '\sin(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_6056ce839fcc001f', 'test_TeX_native_aggregate_contexts_cases', 'gamma(x)', '\Gamma(x)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_69016d840f122940', 'test_TeX_native_aggregate_contexts_cases', 'f''''(x)', 'f''''\left(x\right)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9856709ccfe272f1', 'test_TeX_native_aggregate_contexts_cases', '{1/s^2 | s = ?; Re(s)>0}', '\mathfrak{Re}(s) > 0');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c5b55e71c526e3fb', 'test_TeX_native_aggregate_contexts_cases', 'LerchPhi(z,s,a)', '\Phi\left(z, s, a\right)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_fb105dc79f93fe6c', 'test_TeX_native_aggregate_contexts_cases', 'LommelS(mu,nu,x)', 's_{');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_239a2156891235a0', 'test_TeX_native_aggregate_contexts_cases', 'appell_f1(a,b1,b2,c,x,y)', '\operatorname{F}_{1}\left(');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_36caf3340a929547', 'test_TeX_native_aggregate_contexts_cases', 'lauricella_f(2,a,b1,b2,c,x,y)', 'F_D^{');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_527ff8afc117bd66', 'test_TeX_native_aggregate_contexts_cases', 'hypergeometric_pFq(1,1,a,b,x)', '{}_{1}F_{1}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9dd41d6053973bb2', 'test_TeX_native_aggregate_contexts_cases', '@S x^2 dx', '\int x^{2}\, dx');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_7a72c1f12d6007a2', 'test_TeX_native_aggregate_contexts_cases', 'x+x^2+1', 'x^{2} + x + 1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_quantum_input_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_quantum_input', 'TeX_cases', 'test_TeX_quantum_input_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_quantum_input. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_85feebbd14ac42bd', 'test_TeX_quantum_input_cases', 'hat(H)', '\widehat{H}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_2a7211b93734af13', 'test_TeX_quantum_input_cases', 'H_op', '\widehat{H}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_16d81b92c435a247', 'test_TeX_quantum_input_cases', 'p_op^2', '\widehat{p}^{2}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_63c7cb409cd531f7', 'test_TeX_quantum_input_cases', 'hat(A+B)', '\widehat{A + B}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_8c43e7c18d132d16', 'test_TeX_quantum_input_cases', 'B_op*A_op', '\widehat{B}\mkern-2mu \widehat{A}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_52c060347578e5ef', 'test_TeX_quantum_input_cases', 'dagger(A)*B_op', '{A}^{\dagger}\mkern-2mu \widehat{B}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c1a3ef85bde3cf2f', 'test_TeX_quantum_input_cases', 'hat(x)*0', '\widehat{x}\mkern-2mu 0');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_498dee87be7189d9', 'test_TeX_quantum_input_cases', 'sqrt(A_op^2)', '\sqrt{\widehat{A}^{2}}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_a67bee97b0d612db', 'test_TeX_quantum_input_cases', 'hat(0)+sin(0)', '\widehat{0} + \sin(0)');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_08446e7799d5977a', 'test_TeX_quantum_input_cases', 'dagger(p_op)', '{\widehat{p}}^{\dagger}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_f81c29b58d8dce4b', 'test_TeX_quantum_input_cases', 'dagger(A*B)', '{\left(A\mkern-2mu B\right)}^{\dagger}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_290108c7538d214e', 'test_TeX_quantum_input_cases', 'bra(@phi)', '\left\langle \phi \right|');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_f02549b7eb282b28', 'test_TeX_quantum_input_cases', 'ket(@psi)', '\left| \psi \right\rangle');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_90097eb60546a1d0', 'test_TeX_quantum_input_cases', 'braket(@phi,@psi)', '\left\langle \phi \middle| \psi \right\rangle');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_932e760a2cc2cb64', 'test_TeX_quantum_input_cases', 'ketbra(@psi,@phi)', '\left| \psi \right\rangle\!\left\langle \phi \right|');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_b9dc2b15fc3d77ea', 'test_TeX_quantum_input_cases', 'matrix_element(@psi,p_op,@psi)', '\left\langle \psi \middle| \widehat{p} \middle| \psi \right\rangle');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_e39c1e9229a70b8f', 'test_TeX_quantum_input_cases', 'expect(p)', '\left\langle p \right\rangle');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_1dc68781b6708b6d', 'test_TeX_quantum_input_cases', 'expect(p)=matrix_element(@psi,p_op,@psi)', '\left\langle p \right\rangle = \left\langle \psi \middle| \widehat{p} \middle| \psi \right\rangle');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_fd6ba2085b6ba201', 'test_TeX_quantum_input_cases', 'hat (H)', '\widehat{H}');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_ba3cccbda59a8b4e', 'test_TeX_quantum_input_cases', 'dagger(dagger(A))', '{\left({A}^{\dagger}\right)}^{\dagger}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'TeX.test_TeX_quantum_input_errors_cases', 'tests/TeX/test_TeX.c', 'tests.test_TeX_quantum_input_errors', 'TeX_cases', 'test_TeX_quantum_input_errors_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/TeX/test_TeX.c: test_TeX_quantum_input_errors. Existing checks are retained in C.
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_dd1a29077ff64a9f', 'test_TeX_quantum_input_errors_cases', 'hat()', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_929c1a83b67abb90', 'test_TeX_quantum_input_errors_cases', 'hat(A,B)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_bb1fb12096adbc24', 'test_TeX_quantum_input_errors_cases', 'bra(x,y)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_015c1683abfe3ea9', 'test_TeX_quantum_input_errors_cases', 'ket(x,y)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c000ae1eadcbc813', 'test_TeX_quantum_input_errors_cases', 'dagger(A,B)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_9ed2d33e1c02307d', 'test_TeX_quantum_input_errors_cases', 'braket(x)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_785c7cd03e088a90', 'test_TeX_quantum_input_errors_cases', 'ketbra(x,y,z)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_cd411e26f79c4d75', 'test_TeX_quantum_input_errors_cases', 'matrix_element(x,A)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_5fc60ec26632b896', 'test_TeX_quantum_input_errors_cases', 'matrix_element(x,A,y,z)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_c6248ccaf33f9738', 'test_TeX_quantum_input_errors_cases', 'expect(x,y)', '');
INSERT OR IGNORE INTO TeX_cases(name, operation, input, expected) VALUES ('case_dd19f8077ff5f75c', 'test_TeX_quantum_input_errors_cases', 'hat(x', '');
