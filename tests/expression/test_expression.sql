-- expression regression inputs and expectations. Checking logic remains in the module's C tests.
CREATE TABLE IF NOT EXISTS expression_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS expression_cases_operation ON expression_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.differentiability', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'differentiability')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('differentiability_lambert', 'differentiability', 'lambert_S(-1,exp(2i*x),1,1,-1,1000)', 'yes'),
    ('differentiability_nested_lambert', 'differentiability', '-4*lambert_S(-1,exp(2i*x),1,1,-1,1000)', 'yes'),
    ('differentiability_lerch', 'differentiability', 'lerch_phi(x,2,1)', 'yes'),
    ('differentiability_lommel', 'differentiability', 'lommel_S(1,0,x)', 'yes'),
    ('differentiability_appell_f1', 'differentiability', 'appell_F1(1,2,3,4,x,y)', 'yes'),
    ('differentiability_appell_f2', 'differentiability', 'appell_F2(1,2,3,4,5,x,y)', 'yes'),
    ('differentiability_appell_f3', 'differentiability', 'appell_F3(1,2,3,4,5,x,y)', 'yes'),
    ('differentiability_appell_f4', 'differentiability', 'appell_F4(1,2,3,4,x,y)', 'yes'),
    ('differentiability_nested_value_only', 'differentiability', 'lambert_S(-1,exp(2i*x),gcd(x,2),1,-1,4)', 'no'),
    ('differentiability_nested_pack_value_only', 'differentiability', 'appell_F1(1,gcd(x,2),3,4,x,y)', 'no'),
    ('differentiability_gcd', 'differentiability', 'gcd(x,2)', 'no')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.quotient_recognition', 'tests/expression/test_expr_lambert.c',
    'tests.test_expr_lambert.test_quotient_recognition', 'expression_cases', 'quotient_recognition')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO expression_cases(name, operation, input, expected, precision) VALUES
    ('quotient_recognition_tan', 'quotient_recognition', 'sum(k,1,4,k*(tan(k*x)^2+1))', '["x","0.1",true]', 320),
    ('quotient_recognition_authored_notation', 'quotient_recognition', '@Z_(k=1)^4 k(tan^2(kx)+1)',
     '["x","0.1",true]', 320),
    ('quotient_recognition_expanded', 'quotient_recognition', 'sum(k,1,4,k*tan(k*x)^2+k)', '["x","0.1",true]', 320),
    ('quotient_recognition_negative', 'quotient_recognition', '-sum(k,1,4,k*(1+tan(k*x)^2))', '["x","0.1",true]', 320),
    ('quotient_recognition_second', 'quotient_recognition', 'sum(k,1,4,2*k^2*(tan(k*x)+tan(k*x)^3))',
     '["x","0.1",true]', 320),
    ('quotient_recognition_third', 'quotient_recognition', 'sum(k,1,4,2*k^3*(1+4*tan(k*x)^2+3*tan(k*x)^4))',
     '["x","0.1",true]', 320),
    ('quotient_recognition_fourth', 'quotient_recognition', 'sum(k,1,4,8*k^4*(2*tan(k*x)+5*tan(k*x)^3+3*tan(k*x)^5))',
     '["x","0.1",true]', 320),
    ('quotient_recognition_sixth', 'quotient_recognition',
     'sum(k,1,3,k^6*(272*tan(k*x)+1232*tan(k*x)^3+1680*tan(k*x)^5+720*tan(k*x)^7))',
     '["x","0.1",true]', 320),
    ('quotient_recognition_cot', 'quotient_recognition', 'sum(k,1,4,-k*(1+cot(k*x)^2))', '["x","0.1",true]', 320),
    ('quotient_recognition_tanh', 'quotient_recognition', 'sum(k,1,4,k*(1-tanh(k*x)^2))', '["x","0.1",true]', 320),
    ('quotient_recognition_coth', 'quotient_recognition', 'sum(k,1,4,k*(1-coth(k*x)^2))', '["x","0.1",true]', 320),
    ('quotient_recognition_affine', 'quotient_recognition', 'sum(j,1,4,2*j*(tan(j*(2*t+1))^2+1))',
     '["t","0.1",true]', 320),
    ('quotient_recognition_polynomial_weight', 'quotient_recognition',
     '{sum(k,1,4,(a*k+b)*(1+tan(k*x)^2))|a=2;b=3}', '["x","0.1",true]', 320),
    ('quotient_recognition_wrong_constant', 'quotient_recognition', 'sum(k,1,4,k*(tan(k*x)^2-1))',
     '["x","0.1",false]', 320),
    ('quotient_recognition_mixed_phases', 'quotient_recognition', 'sum(k,1,4,k*(tan(k*x)^2+tan(2*k*x)^2+1))',
     '["x","0.1",false]', 320),
    ('quotient_recognition_nonlinear_index', 'quotient_recognition', 'sum(k,1,4,k*(tan(k^2*x)^2+1))',
     '["x","0.1",false]', 320),
    ('quotient_recognition_nonpolynomial_weight', 'quotient_recognition', 'sum(k,1,4,exp(k)*(tan(k*x)^2+1))',
     '["x","0.1",false]', 320),
    ('quotient_recognition_different_lower', 'quotient_recognition', 'sum(k,2,4,k*(tan(k*x)^2+1))',
     '["x","0.1",false]', 320)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES
    ('expression.lambert_value', 'tests/expression/test_expr_lambert.c',
     'tests.test_expr_lambert.test_lambert_values', 'expression_cases', 'lambert_value'),
    ('expression.lambert_calculus', 'tests/expression/test_expr_lambert.c',
     'tests.test_expr_lambert.test_lambert_calculus', 'expression_cases', 'lambert_calculus'),
    ('expression.lambert_notation', 'tests/expression/test_expr_lambert.c',
     'tests.test_expr_lambert.test_lambert_notation', 'expression_cases', 'lambert_notation'),
    ('expression.lambert_integral_limits', 'tests/expression/test_expr_lambert.c',
     'tests.test_expr_lambert.test_lambert_integral_limits', 'expression_cases', 'lambert_integral_limits'),
    ('expression.lambert_qpartial', 'tests/expression/test_expr_lambert.c',
     'tests.test_expr_lambert.test_lambert_qpartials', 'expression_cases', 'lambert_qpartial'),
    ('expression.lambert_real', 'tests/expression/test_expr_lambert.c',
     'tests.test_expr_lambert.test_lambert_real', 'expression_cases', 'lambert_real')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO expression_cases(name,operation,input,expected,precision) VALUES
    ('lambert_real_tangent','lambert_real','{-4*lambert_S(-1,exp(2i*x),1,1,-1,4)|x=1/10}',
     '["{sum(k,1,4,k/cos(k*x)^2)|x=1/10}",true]',320),
    ('lambert_real_cotangent','lambert_real','{4*lambert_S(1,exp(2i*x),1,1,-1,4)|x=1/10}',
     '["{-sum(k,1,4,k/sin(k*x)^2)|x=1/10}",true]',320),
    ('lambert_real_quarter_phase','lambert_real','{lambert_S(i,exp(i*x),1,2,-1,4)|x=1/10}',
     '["{-sum(k,1,4,k^2/(4*sin(pi/4+k*x/2)^2))|x=1/10}",true]',320),
    ('lambert_real_second_tangent','lambert_real','{-8i*lambert_S(-1,exp(2i*x),1,2,-2,4)|x=1}',
     '["{2*sum(k,1,4,k^2*tan(k*x)/cos(k*x)^2)|x=1}",true]',320),
    ('lambert_real_third_tangent','lambert_real','{16*lambert_S(-1,exp(2i*x),1,3,-3,4)|x=1/10}',
     '["{2*sum(k,1,4,k^3*(1+3*tan(k*x)^2)/cos(k*x)^2)|x=1/10}",true]',320),
    ('lambert_imaginary_even_order','lambert_real','{lambert_S(-1,exp(2i*x),1,2,-2,4)|x=1/10}',
     '["{i*sum(k,1,4,k^2*tan(k*x)/cos(k*x)^2)/4|x=1/10}",false,true]',320),
    ('lambert_imaginary_fourth_order','lambert_real','{lambert_S(-1,exp(2i*x),1,4,-4,4)|x=1/10}',
     '["{-i*sum(k,1,4,k^4*(2*tan(k*x)+3*tan(k*x)^3)/cos(k*x)^2)/4|x=1/10}",false,true]',320),
    ('lambert_even_order_preserves_complex','lambert_real',
     '{lambert_S(-1,exp(2i*x),1,2,-2,4)|x=(1+i)/10}',
     '["{sum(k,1,4,k^2*(-exp(2i*k*x))*(1-exp(2i*k*x))/(1+exp(2i*k*x))^3)|x=(1+i)/10}",false,false]',320),
    ('lambert_preserves_complex','lambert_real','{-4*lambert_S(-1,exp(2i*x),1,1,-1,4)|x=(1+i)/10}',
     '["{-4*sum(k,1,4,k*(-exp(2i*k*x))/(1+exp(2i*k*x))^2)|x=(1+i)/10}",false]',320),
    ('lambert_finite_unit','lambert_value','["-1","1","1","1","-1","4"]','-5/2',320),
    ('lambert_constant_long','lambert_value','["-1","1","1","1","-1","1000"]','-125125',1024),
    ('lambert_constant_zero_weight','lambert_value','["0.5","1","0.25","0","0","1000"]','1000',512),
    ('lambert_constant_cubic_order','lambert_value','["0.5","1","0.25","1","-3","37"]','35113/2',512),
    ('lambert_constant_complex','lambert_value','["-1+i/8","1","1+i/8","1","-1","1000"]',
     '(1000*(1+i/8)+499500)*(-1+i/8)/(2-i/8)^2',1024),
    ('lambert_constant_positive_order','lambert_value','["0.5","1","1","1","1","1000"]',
     '500500*ln(2)',512),
    ('lambert_constant_positive_complex','lambert_value','["0.25+i/8","1","1+i/8","0","1","1000"]',
     '-1000*ln(0.75-i/8)',512),
    ('lambert_recurrence_small_phase','lambert_value','["1","exp(1e-12i)","1","0","-1","96"]',
     'rational',512),
    ('lambert_recurrence_complex','lambert_value','["0.25+i/8","0.999+0.0002i","1.25+i/8","3","-1","65"]',
     'rational',512),
    ('lambert_recurrence_nonunit','lambert_value','["0.25+i/8","1+0.0001i","1+i/8","2","-1","65"]',
     'rational',1024),
    ('lambert_recurrence_midblock_pole','lambert_value',
     '["(1-1e-5)*exp(-1.6i)","exp(0.1i)","1","1","-1","65"]','rational',512),
    ('lambert_recurrence_high_weight','lambert_value',
     '["0.5","exp(0.02i)","1","64","-1","65"]','rational',512),
    ('lambert_rational_zero_order','lambert_value','["0.5","0.75","0","0","0","7"]',
     'sum(j,0,6,polylog(0,0.5*0.75^j))',512),
    ('lambert_rational_weight_zero','lambert_value','["-1","0.75","1.25","0","-1","9"]',
     'rational',512),
    ('lambert_rational_high_order','lambert_value','["0.125","0.5","1.25","4","-32","5"]',
     'rational',512),
    ('lambert_rational_high_weight','lambert_value','["-0.25","0.5","1","64","-3","5"]',
     'rational',512),
    ('lambert_rational_complex_coordinate','lambert_value','["0.25+0.125i","0.5-0.25i","1.25+0.125i","3","-4","5"]',
     'rational',1024),
    ('lambert_rational_negative_base','lambert_value','["0.25","-0.5","1","3","-2","7"]',
     'rational',512),
    ('lambert_rational_principal_branch','lambert_value','["0.25","-0.5","0.5","2","-1","5"]',
     'rational',512),
    ('lambert_rational_unit_circle','lambert_value','["-1","exp(0.002i)","1","1","-1","1000"]',
     '-sum(k,1,1000,k/(4*cos(0.001*k)^2))',512),
    ('lambert_rational_near_pole','lambert_value','["1","0.999","1","2","-3","5"]',
     'rational',512),
    ('lambert_rational_longer_tail','lambert_value','["0.5","0.875","1","1","-1","inf"]','dual',1024),
    ('lambert_geometric_finite_real','lambert_value','["0.25","0.5","0.25","0","-4","256"]','rational',512),
    ('lambert_geometric_finite_complex','lambert_value',
     '["0.25+0.125i","0.5-0.125i","1.25+0.125i","1","-3","256"]','rational',512),
    ('lambert_geometric_infinite_real','lambert_value','["0.5","0.5","1","0","-4","inf"]','dual',512),
    ('lambert_geometric_infinite_complex','lambert_value',
     '["0.5+0.125i","0.5-0.125i","1","1","-3","inf"]','dual',512),
    ('lambert_finite_zero','lambert_value','["1","0.5","1","0","0","0"]','0',320),
    ('lambert_finite_weight','lambert_value','["0.5","0.25","1","2","0","4"]',
     'sum(k,1,4,k^2*(0.5*0.25^k)/(1-0.5*0.25^k))',320),
    ('lambert_finite_negative_order','lambert_value','["0.5","0.25","1","1","-3","4"]',
     'sum(k,1,4,k*(0.5*0.25^k)*(1+4*(0.5*0.25^k)+(0.5*0.25^k)^2)/(1-0.5*0.25^k)^4)',320),
    ('lambert_finite_positive_order','lambert_value','["0.5","0.25","1","2","1","4"]',
     '-sum(k,1,4,k^2*ln(1-0.5*0.25^k))',320),
    ('lambert_finite_complex','lambert_value','["0.5+0.125i","0.25+0.0625i","1","1","-1","4"]',
     'sum(k,1,4,k*((0.5+0.125i)*(0.25+0.0625i)^k)/(1-(0.5+0.125i)*(0.25+0.0625i)^k)^2)',320),
    ('lambert_infinite_real','lambert_value','["0.5","0.25","1","1","-1","inf"]','dual',320),
    ('lambert_infinite_complex','lambert_value','["0.5+0.125i","0.25+0.0625i","1","2","1","inf"]','dual',320),
    ('lambert_infinite_precision','lambert_value','["0.5","0.25","1","2","-2","inf"]','dual',1024),
    ('lambert_notation_integral','lambert_notation','-8i*lambert_S(-1,exp(2i*x),1,2,-2,1000)+C',
     '[280,"-8"," + C"]',320),
    ('lambert_notation_narrow_integral','lambert_notation','-8i*lambert_S(-1,exp(2i*x),1,2,-2,1000)+C',
     '[1,"-8"," + C"]',320),
    ('lambert_notation_subtraction','lambert_notation','C-lambert_S(-1,exp(2i*x),1,2,-2,1000)',
     '[1,"C - "]',320),
    ('lambert_notation_multiple','lambert_notation',
     'lambert_S(-1,exp(2i*x),1,2,-2,1000)+lambert_S(0.5,0.25,1,1,0,8)+C',
     '[1," + C"]',320),
    ('lambert_integral_linear_base','lambert_integral_limits','lambert_S(-1,exp(2i*x),1,3,-3,8)','true',320),
    ('lambert_integral_nonlinear_base','lambert_integral_limits','lambert_S(-1,exp(x^2),1,3,-3,8)','false',320),
    ('lambert_integral_mixed_arguments','lambert_integral_limits','lambert_S(exp(x),exp(2*x),1,3,-3,8)','false',320),
    ('lambert_integral_varying_weight','lambert_integral_limits','lambert_S(-1,exp(2*x),1,x,-3,8)','false',320),
    ('lambert_integral_varying_order','lambert_integral_limits','lambert_S(-1,exp(2*x),1,3,x,8)','false',320),
    ('lambert_integral_varying_count','lambert_integral_limits','lambert_S(-1,exp(2*x),1,3,-3,x)','false',320),
    ('lambert_integral_zero_weight','lambert_integral_limits','lambert_S(-1,exp(2*x),1,0,-3,8)','false',320),
    ('lambert_integral_unknown_rate','lambert_integral_limits','lambert_S(-1,exp(c*x),1,3,-3,8)','false',320),
    ('lambert_derivative_a','lambert_calculus','lambert_S(x,0.25,1,1,0,8)',
     '["0.5","sum(k,1,8,k*(x*0.25^k)/(1-x*0.25^k))",1]',320),
    ('lambert_derivative_q_second','lambert_calculus','lambert_S(0.5,x,1,1,-1,8)',
     '["0.25","sum(k,1,8,k*(0.5*x^k)/(1-0.5*x^k)^2)",2]',320),
    ('lambert_derivative_z_second','lambert_calculus','lambert_S(0.5,0.25,x,2,-1,8)',
     '["1.25","sum(k,0,7,(k+x)^2*(0.5*0.25^(k+x))/(1-0.5*0.25^(k+x))^2)",2]',320),
    ('lambert_derivative_z_weight_zero','lambert_calculus','lambert_S(0.5,0.25,x,0,0,8)',
     '["1.25","sum(k,0,7,(0.5*0.25^(k+x))/(1-0.5*0.25^(k+x)))",1]',320),
    ('lambert_derivative_chain','lambert_calculus','lambert_S(x/2,exp(-x),1+x,2,0,8)',
     '["0.25","sum(k,0,7,(k+1+x)^2*(x/2*exp(-x*(k+1+x)))/(1-x/2*exp(-x*(k+1+x))))",2]',320),
    ('lambert_integral_z','lambert_calculus','lambert_S(0.5,0.25,x,2,-1,8)',
     '["1.25","sum(k,0,7,(k+x)^2*(0.5*0.25^(k+x))/(1-0.5*0.25^(k+x))^2)",-1]',320),
    ('lambert_integral_unit','lambert_calculus','lambert_S(-1,1,x,2,-1,8)',
     '["1.25","-sum(k,0,7,(k+x)^2)/4",-1]',320),
    ('lambert_integral_exponential_base','lambert_calculus','lambert_S(0.5,exp(-2*x+1),1,2,-1,8)',
     '["1","sum(k,1,8,k^2*(0.5*exp((-2*x+1)*k))/(1-0.5*exp((-2*x+1)*k))^2)",-1]',320),
    ('lambert_integral_complex_base','lambert_calculus','lambert_S(-1,exp(2i*x),1,3,-3,8)',
     '["0.1","sum(k,1,8,k^3*(1+4*tan(k*x)^2+3*tan(k*x)^4))/8",-1]',320),
    ('lambert_integral_fractional_offset','lambert_calculus','lambert_S(0.5,exp(2i*x),1/2,2,0,4)',
     '["2","sum(k,0,3,(k+1/2)^2*(0.5*exp(2i*x)^(k+1/2))/(1-0.5*exp(2i*x)^(k+1/2)))",-1]',320),
    ('lambert_integral_bound_base_1000','lambert_calculus',
     '{16*lambert_S(-1,exp(2i*x),1,3,-3,1000)|x=1}',
     '["1","2*sum(k,1,1000,k^3*(1+4*tan(k*x)^2+3*tan(k*x)^4))",-1]',320),
    ('lambert_integral_weight_one','lambert_calculus','lambert_S(-1,exp(2i*x),1,1,-1,8)',
     '["0.1","-sum(k,1,8,k*(1+tan(k*x)^2))/4",-1]',320),
    ('lambert_integral_exponential_amplitude','lambert_calculus','lambert_S(exp(-2*x+1),0.25,1,0,0,8)',
     '["1","sum(k,1,8,(exp(-2*x+1)*0.25^k)/(1-exp(-2*x+1)*0.25^k))",-1]',320),
    ('lambert_integral_complex_amplitude','lambert_calculus','lambert_S(exp((1+i)*x-1),0.25,1,2,-1,8)',
     '["0.1","sum(k,1,8,k^2*(exp((1+i)*x-1)*0.25^k)/(1-exp((1+i)*x-1)*0.25^k)^2)",-1]',320),
    ('lambert_repeated_tangent','lambert_calculus','sum(k,1,13,tan(k*x))',
     '["0.1","sum(k,1,13,tan(k*x))",3]',320),
    ('lambert_qpartial_second','lambert_qpartial','qdigamma(x,9/4)-qdigamma(x,5/4)',
     '["0.25","-ln(x)*x^(5/4)/(1-x^(5/4))",2]',320),
    ('lambert_qpartial_complex','lambert_qpartial','qdigamma(x,9/4)-qdigamma(x,5/4)',
     '["0.25+0.0625i","-ln(x)*x^(5/4)/(1-x^(5/4))",2]',320),
    ('lambert_qpartial_reciprocal','lambert_qpartial','qdigamma(x,9/4)-qdigamma(x,5/4)',
     '["2","-ln(x)*x^(5/4)/(1-x^(5/4))",2]',320),
    ('lambert_qpartial_chain','lambert_qpartial','qdigamma(exp(-x),9/4)-qdigamma(exp(-x),5/4)',
     '["1.25","x*exp(-5*x/4)/(1-exp(-5*x/4))",2]',320),
    ('lambert_qpartial_mixed','lambert_qpartial','qdigamma(exp(-x),x+2)-qdigamma(exp(-x),x+1)',
     '["1.25","x*exp(-x*(x+1))/(1-exp(-x*(x+1)))",2]',320),
    ('lambert_qpartial_unit','lambert_qpartial','qdigamma(x,9/4)-qdigamma(x,5/4)',
     '["1","-7/24",2]',320)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.quotient_derivative', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_from_string_quotient_progression_derivatives', 'expression_cases',
    'quotient_derivative')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected, precision) VALUES
    ('quotient_derivative_tan', 'quotient_derivative', '@Z_(k=1)^4 tan(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_cot', 'quotient_derivative', '@Z_(k=1)^4 cot(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_sec', 'quotient_derivative', '@Z_(k=1)^4 sec(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_cosec', 'quotient_derivative', '@Z_(k=1)^4 cosec(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_tanh', 'quotient_derivative', '@Z_(k=1)^4 tanh(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_coth', 'quotient_derivative', '@Z_(k=1)^4 coth(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_sech', 'quotient_derivative', '@Z_(k=1)^4 sech(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_cosech', 'quotient_derivative', '@Z_(k=1)^4 cosech(kx)', '["x","0.1"]', 320),
    ('quotient_derivative_tan_zero', 'quotient_derivative', '@Z_(k=1)^1000 tan(kx)', '["x","0"]', 320),
    ('quotient_derivative_tan_large', 'quotient_derivative', '@Z_(k=1)^1000 tan(kx)', '["x","0.01"]', 320),
    ('quotient_derivative_tan_complex', 'quotient_derivative', '@Z_(k=1)^4 tan(kx)', '["x","0.1+0.03i"]', 320),
    ('quotient_derivative_tan_scaled', 'quotient_derivative', '@Z_(j=1)^13 tan(j*(2t+1))', '["t","0.1"]', 320)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.atan_progression', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_from_string_atan_progression', 'expression_cases', 'atan_progression')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected, precision) VALUES
    ('atan_progression_positive', 'atan_progression', '@Z_(k=1)^1000 atan(kx)', '["x","1","1000"]', 320),
    ('atan_progression_negative', 'atan_progression', '@Z_(k=1)^1000 atan(kx)', '["x","-1","1000"]', 320),
    ('atan_progression_small', 'atan_progression', '@Z_(k=1)^1000 atan(kx)', '["x","1e-8","1000"]', 320),
    ('atan_progression_small_negative', 'atan_progression', '@Z_(k=1)^1000 atan(kx)', '["x","-1e-8","1000"]', 320),
    ('atan_progression_large', 'atan_progression', '@Z_(k=1)^1000 atan(kx)', '["x","1e8","1000"]', 320),
    ('atan_progression_large_negative', 'atan_progression', '@Z_(k=1)^1000 atan(kx)', '["x","-1e8","1000"]', 320),
    ('atan_progression_zero', 'atan_progression', '@Z_(k=1)^1000 atan(kx)', '["x","0","1000"]', 320),
    ('atan_progression_renamed', 'atan_progression', '@Z_(j=1)^13 atan(jt)', '["t","0.3","13"]', 320),
    ('atan_progression_complex', 'atan_progression', '@Z_(k=1)^4 atan(kx)', '["x","1+i/3","4"]', 320),
    ('atan_progression_complex_negative', 'atan_progression', '@Z_(k=1)^4 atan(kx)', '["x","-1-i/3","4"]', 320)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;

INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('fold_sinh_zero', 'test_from_string_functions_check_parse_simplified_expr_4', 'sinh(0)', '0'),
    ('fold_cosh_zero', 'test_from_string_functions_check_parse_simplified_expr_4', 'cosh(0)', '1'),
    ('fold_tanh_zero', 'test_from_string_functions_check_parse_simplified_expr_4', 'tanh(0)', '0'),
    ('fold_sech_zero', 'test_from_string_functions_check_parse_simplified_expr_4', 'sech(0)', '1'),
    ('fold_sec_zero', 'test_from_string_functions_check_parse_simplified_expr_4', 'sec(0)', '1'),
    ('fold_asinh_zero', 'test_from_string_functions_check_parse_simplified_expr_4', 'asinh(0)', '0'),
    ('fold_atanh_zero', 'test_from_string_functions_check_parse_simplified_expr_4', 'atanh(0)', '0'),
    ('fold_sinh_nonzero_symbolic', 'test_from_string_functions_check_parse_simplified_expr_4', 'sinh(1)', 'sinh(1)'),
    ('fold_cosh_nonzero_symbolic', 'test_from_string_functions_check_parse_simplified_expr_4', 'cosh(1)', 'cosh(1)'),
    ('fold_coth_zero_remains_singular', 'test_from_string_functions_check_parse_simplified_expr_4', 'coth(0)', 'coth(0)'),
    ('fold_sinh_bound_variable_preserved', 'test_from_string_functions_check_parse_simplified_expr_4',
     '{sinh(c)|c=0}', '{ sinh(c) | c = 0 }')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.growth_progression_sampling', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_growth_progression_sampling', 'expression_cases', 'growth_sample')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('growth_sample_sinh', 'growth_sample', '@Z_(k=1)^1000 sinh(kx)',
     '["x","-1/500","1/500","-1/500","1/500",false]'),
    ('growth_sample_cosh', 'growth_sample', '@Z_(k=1)^1000 cosh(kx)',
     '["x","-1/500","1/500","-1/500","1/500",false]'),
    ('growth_sample_exp', 'growth_sample', '@Z_(k=1)^1000 exp(kx)',
     '["x","-1/500","1/500","-1/500","1/500",false]'),
    ('growth_sample_affine', 'growth_sample', '@Z_(j=1)^1000 sinh(j*(2t+1))',
     '["t","-501/1000","-499/1000","-501/1000","-499/1000",false]'),
    ('growth_sample_negative_slope', 'growth_sample', '@Z_(k=1)^1000 cosh(k*(-3x+2))',
     '["x","2/3-1/1500","2/3+1/1500","2/3-1/1500","2/3+1/1500",false]'),
    ('growth_sample_bound_parameters', 'growth_sample', '{@Z_(k=1)^n exp(k*(a*t+b))|t=2,n=1000; a=2,b=1}',
     '["t","-501/1000","-499/1000","-501/1000","-499/1000",false]'),
    ('growth_sample_variable_upper', 'growth_sample', '{@Z_(k=1)^n sinh(kx)|n=1000,x=1}',
     '["n",null,null,"1","2",false]'),
    ('growth_sample_variable_lower', 'growth_sample', '{@Z_(k=x)^1000 sinh(kx)|x=1}',
     '["x",null,null,"1","2",false]'),
    ('growth_sample_noninteger', 'growth_sample', '@Z_(k=1)^(3/2) sinh(kx)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('growth_sample_empty', 'growth_sample', '@Z_(k=1)^0 cosh(kx)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('growth_sample_nonlinear', 'growth_sample', '@Z_(k=1)^1000 sinh(k*x^2)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('growth_sample_circular_unchanged', 'growth_sample', '@Z_(k=1)^1000 sin(kx)',
     '["x",null,null,"-0.1","0.1",false]')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.circular_progression_sampling', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_circular_progression_sampling', 'expression_cases', 'circular_sample')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('circular_sample_tangent', 'circular_sample', '@Z_(k=1)^1000 tan(kx)',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",true]'),
    ('circular_sample_secant', 'circular_sample', '@Z_(k=1)^1000 sec(kx)',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",true]'),
    ('circular_sample_cotangent', 'circular_sample', '@Z_(k=1)^1000 cot(kx)',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",false]'),
    ('circular_sample_cosecant', 'circular_sample', '@Z_(k=1)^1000 cosec(kx)',
     '["x","-pi/4000","pi/4000","pi/8000","pi/4000",true]'),
    ('circular_sample_cotangent_endpoint', 'circular_sample', '@Z_(k=1)^1000 cot(kx)',
     '["x","-pi/4000","pi/4000","0","pi/4000",false]'),
    ('circular_sample_tangent_pole', 'circular_sample', '@Z_(k=1)^1000 tan(kx)',
     '["x","-pi/4000","pi/4000","0","pi/2000",false]'),
    ('circular_sample_tangent_wide', 'circular_sample', '@Z_(k=1)^1000 tan(kx)',
     '["x","-pi/4000","pi/4000","-10","10",false]'),
    ('circular_sample_renamed_scaled', 'circular_sample', '@Z_(j=1)^1000 tan(2jt)',
     '["t","-pi/8000","pi/8000","-pi/8000","pi/8000",true]'),
    ('circular_sample_affine', 'circular_sample', '@Z_(k=1)^1000 tan(k*(2x+1))',
     '["x","-1/2-pi/8000","-1/2+pi/8000","-0.5001","-0.4999",true]'),
    ('circular_sample_negative_scale', 'circular_sample', '@Z_(k=1)^1000 tan(k*(-3x+2))',
     '["x","2/3-pi/12000","2/3+pi/12000","2/3-pi/24000","2/3+pi/24000",true]'),
    ('circular_sample_bound_parameters', 'circular_sample', '{@Z_(k=1)^n tan(k*(a*t+b))|t=2,n=1000; a=2,b=1}',
     '["t","-1/2-pi/8000","-1/2+pi/8000","-0.5001","-0.4999",true]'),
    ('circular_sample_variable_upper', 'circular_sample', '{@Z_(k=1)^n tan(kx)|n=1000,x=1}',
     '["n",null,null,"1","2",false]'),
    ('circular_sample_variable_lower', 'circular_sample', '{@Z_(k=x)^1000 tan(kx)|x=1}',
     '["x",null,null,"1","2",false]'),
    ('circular_sample_noninteger', 'circular_sample', '@Z_(k=1)^(3/2) tan(kx)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('circular_sample_empty', 'circular_sample', '@Z_(k=1)^0 tan(kx)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('circular_sample_nonlinear', 'circular_sample', '@Z_(k=1)^1000 tan(k*x^2)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('circular_sample_sine_unchanged', 'circular_sample', '@Z_(k=1)^1000 sin(kx)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('circular_sample_lambert_tangent', 'circular_sample', '-4*lambert_S(-1,exp(2i*x),1,1,-1,1000)',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",true]'),
    ('circular_sample_lambert_second', 'circular_sample', '-8i*lambert_S(-1,exp(2i*x),1,2,-2,1000)',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",true]'),
    ('circular_sample_lambert_third', 'circular_sample', '16*lambert_S(-1,exp(2i*x),1,3,-3,1000)',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",true]'),
    ('circular_sample_lambert_fourth', 'circular_sample', '32i*lambert_S(-1,exp(2i*x),1,4,-4,1000)',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",true]'),
    ('circular_sample_lambert_positive_order', 'circular_sample', 'lambert_S(-1,exp(2i*x),1,1,1,1000)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('circular_sample_lambert_affine', 'circular_sample', '-8*lambert_S(-1,exp(2i*(2t+1)),1,1,-1,13)',
     '["t","-1/2-pi/104","-1/2+pi/104","-1/2-pi/104","-1/2+pi/104",true]'),
    ('circular_sample_lambert_secant', 'circular_sample',
     'lambert_S(i,exp(i*x),1,1,-1,10)-lambert_S(-i,exp(i*x),1,1,-1,10)',
     '["x","-pi/40","pi/40","-pi/40","pi/40",true]'),
    ('circular_sample_lambert_cosecant', 'circular_sample',
     'lambert_S(-1,exp(i*x),1,1,-1,10)-lambert_S(1,exp(i*x),1,1,-1,10)',
     '["x","-pi/20","pi/20","pi/1000","pi/100",true]'),
    ('circular_sample_lambert_mixed_base', 'circular_sample',
     'lambert_S(i,exp(i*x),1,1,-1,10)-lambert_S(-i,exp(2i*x),1,1,-1,10)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('circular_sample_lambert_complex_amplitude', 'circular_sample', 'lambert_S(0.5,exp(2i*x),1,1,-1,10)',
     '["x",null,null,"-0.1","0.1",false]'),
    ('circular_sample_q_identity', 'circular_sample',
     '1000*i+(qdigamma(exp(4*i*x),1)-qdigamma(exp(4*i*x),1001)-qdigamma(exp(2*i*x),1)+qdigamma(exp(2*i*x),1001))/x',
     '["x","-pi/4000","pi/4000","-pi/4000","pi/4000",true]'),
    ('circular_sample_q_wrong_coefficient', 'circular_sample',
     '1001*i+(qdigamma(exp(4*i*x),1)-qdigamma(exp(4*i*x),1001)-qdigamma(exp(2*i*x),1)+qdigamma(exp(2*i*x),1001))/x',
     '["x",null,null,"-pi/4000","pi/4000",false]'),
    ('circular_sample_q_wrong_endpoint', 'circular_sample',
     '1000*i+(qdigamma(exp(4*i*x),1)-qdigamma(exp(4*i*x),1002)-qdigamma(exp(2*i*x),1)+qdigamma(exp(2*i*x),1001))/x',
     '["x",null,null,"-pi/4000","pi/4000",false]')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.binding_square_domain', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_binding_square_domain_ownership',
    'expression_cases', 'binding_square_domain')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('binding_square_complex_positive_real', 'binding_square_domain', '3+4i', 'unchanged'),
    ('binding_square_complex_negative_real', 'binding_square_domain', '-3+4i', 'unchanged'),
    ('binding_square_positive_real', 'binding_square_domain', '3', '3'),
    ('binding_square_negative_real', 'binding_square_domain', '-3', '3')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.negative_sums', 'tests/expression/test_expr_to_string.c',
    'tests.test_expr_t_to_string.test_to_string_all.test_to_string_negative_sums',
    'expression_cases', 'negative_sum')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('negative_sum_polynomial', 'negative_sum', '@Z_(k=1)^4 -k^2*sin(k*pi/2)',
     '["-Σ_(k=1)^4 k²", "-sum(k, 1, 4,", "-\\sum_{k=1}^{4}"]'),
    ('negative_sum_divisor', 'negative_sum', '@Z_(k=1)^4 k/(-2)',
     '["-Σ_(k=1)^4", "-sum(k, 1, 4,", "-\\sum_{k=1}^{4}"]'),
    ('negative_sum_product', 'negative_sum', '3*(@Z_(k=1)^4 -k^2)',
     '["(-Σ_(k=1)^4", "-sum(k, 1, 4,", "-\\sum_{k=1}^{4}"]'),
    ('negative_sum_power', 'negative_sum', '(@Z_(k=1)^4 -k^2)^2',
     '["(-Σ_(k=1)^4", "(-sum(k, 1, 4,", "-\\sum_{k=1}^{4}"]'),
    ('negative_sum_subtraction', 'negative_sum', '1-(@Z_(k=1)^4 -k^2)',
     '["Σ_(k=1)^4", "sum(k, 1, 4,", "1 + \\sum_{k=1}^{4}"]'),
    ('negative_sum_even_signs', 'negative_sum', '@Z_(k=1)^4 (-k)*(-k)',
     '["Σ_(k=1)^4", "sum(k, 1, 4,", "\\sum_{k=1}^{4}"]'),
    ('negative_sum_infinite', 'negative_sum', '@Z_(k=1) -1/2^k',
     '["-Σ_(k=1)", "-sum(k, 1, @inf,", "-\\sum_{k=1}^{\\infty}"]'),
    ('negative_product_stays_inside', 'negative_sum', '@P_(k=1)^4 -k',
     '["Π_(k=1)^4 -k", "product(k, 1, 4, -k)", "\\prod_{k=1}^{4}-k"]')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.rational_progression', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_from_string_rational_progressions', 'expression_cases', 'rational_progression')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('rational_wrong_constant', 'rational_progression', 'sum(k,1,4,k^3/(2+k^2*x^2)^2)', 'false'),
    ('rational_linear_denominator_term', 'rational_progression', 'sum(k,1,4,k^3/(1+k+k^2*x^2)^2)', 'false'),
    ('rational_cubic_denominator', 'rational_progression', 'sum(k,1,4,k^3/(1+k^3*x^2)^2)', 'false'),
    ('rational_improper_numerator', 'rational_progression', 'sum(k,1,4,k^4/(1+k^2*x^2)^2)', 'false'),
    ('rational_fractional_power', 'rational_progression', 'sum(k,1,4,k/(1+k^2*x^2)^(3/2))', 'false'),
    ('rational_power_ceiling', 'rational_progression', 'sum(k,1,4,k/(1+k^2*x^2)^9)', 'false'),
    ('rational_nonpolynomial_weight', 'rational_progression', 'sum(k,1,4,sin(k)/(1+k^2*x^2)^2)', 'false'),
    ('rational_other_lower_bound', 'rational_progression', 'sum(k,2,4,k^3/(1+k^2*x^2)^2)', 'false'),
    ('rational_symbolic_weight', 'rational_progression', '{sum(j,1,n,(a+b*j^2)/(1+j^2*t^2)^3)|t=1;n=4;a=2;b=3}', 'true')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('series_rational_squared_reuse', 'function_series_endpoint', '{-sum(k,1,4,2*k^3*x/(1+k^2*x^2)^2)|x=1}',
     '["-\\sum_{k=1}^{4}","-2*(1/4+8/25+27/100+64/289)"," = ",true,null,true]'),
    ('series_rational_squared_thousand', 'function_series_endpoint', '-@Z_(k=1)^1000 2k^3*x/(k^2*x^2+1)^2',
     '["-\\sum_{k=1}^{1000}",null,"\\psi",false,null,true]'),
    ('series_rational_squared_affine', 'function_series_endpoint', '{sum(j,1,4,3*j^3/(1+j^2*(2*t+1)^2)^2)|t=0}',
     '["\\sum_{j=1}^{4}","3*(1/4+8/25+27/100+64/289)"," = ",true,null,true]'),
    ('series_rational_squared_complex', 'function_series_endpoint', '{sum(k,1,4,k^3/(1+k^2*x^2)^2)|x=1+i/3}',
     '["\\sum_{k=1}^{4}","{sum(k,1,4,k^3/(1+k^2*x^2)^2)|x=1+i/3}"," = ",true,null,true]'),
    ('series_rational_squared_zero', 'function_series_endpoint', '{-sum(k,1,1000,2*k^3*x/(1+k^2*x^2)^2)|x=0}',
     '["-\\sum_{k=1}^{1000}","0"," = ",false,null,true]'),
    ('series_rational_cubed_reuse', 'function_series_endpoint', '{sum(k,1,4,2*k^3*(3*k^2*x^2-1)/(1+k^2*x^2)^3)|x=1}',
     '["\\sum_{k=1}^{4}","{sum(k,1,4,2*k^3*(3*k^2-1)/(1+k^2)^3)}"," = ",true,null,true]'),
    ('series_rational_cubed_collected', 'function_series_endpoint', '{sum(k,1,1000,(2*k^2*x^2-8*k^2*x^2+2)*k^3/(1+k^2*x^2)^3)|x=1}',
     '["\\sum_{k=1}^{1000}","{sum(k,1,1000,2*k^3*(1-3*k^2)/(1+k^2)^3)}"," = ",true,null,true,[3,"x^{7}","x^{8}","1000 + 1","2k^{2}x^{2} - 8k^{2}x^{2}"]]'),
    ('series_rational_cubed_affine', 'function_series_endpoint', '{sum(j,1,4,2*j^3*(3*j^2*(2*t+1)^2-1)/(1+j^2*(2*t+1)^2)^3)|t=0}',
     '["\\sum_{j=1}^{4}","{sum(j,1,4,2*j^3*(3*j^2-1)/(1+j^2)^3)}"," = ",true,null,true,[3,"5 + 1"]]'),
    ('series_rational_cubed_complex', 'function_series_endpoint', '{sum(k,1,4,2*k^3*(3*k^2*x^2-1)/(1+k^2*x^2)^3)|x=1+i/3}',
     '["\\sum_{k=1}^{4}","{sum(k,1,4,2*k^3*(3*k^2*(1+i/3)^2-1)/(1+k^2*(1+i/3)^2)^3)}"," = ",true,null,true,[3,"x^{7}","x^{8}"]]'),
    ('series_rational_fourth_collected', 'function_series_endpoint', '{sum(k,1,4,24*k^5*x*(1-k^2*x^2)/(1+k^2*x^2)^4)|x=1}',
     '["\\sum_{k=1}^{4}","{sum(k,1,4,24*k^5*(1-k^2)/(1+k^2)^4)}"," = ",true,null,true,[4,"x^{9}","x^{10}","x^{11}"]]'),
    ('series_rational_mixed_numerator', 'function_series_endpoint', '{sum(k,1,4,(1+2*k^3)/(1+k^2*x^2)^4)|x=1}',
     '["\\sum_{k=1}^{4}","{sum(k,1,4,(1+2*k^3)/(1+k^2)^4)}"," = ",true,null,true]'),
    ('series_rational_even_numerator', 'function_series_endpoint', '{sum(k,1,4,k^2/(1+k^2*x^2)^2)|x=1}',
     '["\\sum_{k=1}^{4}","1/4+4/25+9/100+16/289"," = ",true,null,true]'),
    ('series_rational_eighth_power', 'function_series_endpoint', '{sum(k,1,2,k^15/(1+k^2*x^2)^8)|x=1}',
     '["\\sum_{k=1}^{2}","1/256+32768/390625"," = ",true,null,true]'),
    ('series_rational_atan_reuse', 'function_series_endpoint', '@Z_(k=1)^1000 k/(1+k^2*x^2)',
     '["\\sum_{k=1}^{1000}",null," = ",false,"(digamma(1001+i/x)-digamma(1+i/x)+digamma(1001-i/x)-digamma(1-i/x))/(2*x^2)"]'),
    ('series_rational_atan_bound', 'function_series_endpoint', '{sum(k,1,4,k/(1+k^2*x^2))|x=1}',
     '["\\sum_{k=1}^{4}","122/85"," = ",true]'),
    ('series_rational_atan_negative', 'function_series_endpoint', '{-sum(j,1,4,j/(1+j^2*t^2))|t=1}',
     '["-\\sum_{j=1}^{4}","-122/85"," = ",true]'),
    ('series_rational_atan_affine', 'function_series_endpoint', '{sum(j,1,4,2*j/(1+j^2*(2*t+1)^2))|t=0}',
     '["\\sum_{j=1}^{4}","244/85"," = ",true]'),
    ('series_rational_atan_zero', 'function_series_endpoint', '{sum(j,1,1000,j/(1+j^2*t^2))|t=0}',
     '["\\sum_{j=1}^{1000}","500500"," = ",false]'),
    ('series_simplified_linear_cosine_1000', 'function_series_endpoint', '@Z_(k=1)^1000 k*cos(kx)',
     '["\\sum_{k=1}^{1000}",null," = ",false,"(1001*cos(1000*x)-1000*cos(1001*x)-1)/(4*sin(x/2)^2)"]'),
    ('series_simplified_linear_cosine_small', 'function_series_endpoint', '@Z_(k=1)^4 k*cos(kx)',
     '["\\sum_{k=1}^{4}",null," = ",false,"(5*cos(4*x)-4*cos(5*x)-1)/(4*sin(x/2)^2)"]'),
    ('series_simplified_linear_cosine_renamed', 'function_series_endpoint', '@Z_(j=1)^7 j*cos(jt)',
     '["\\sum_{j=1}^{7}",null," = ",false,"(8*cos(7*t)-7*cos(8*t)-1)/(4*sin(t/2)^2)"]'),
    ('series_simplified_linear_sine', 'function_series_endpoint', '@Z_(k=1)^4 k*sin(kx)',
     '["\\sum_{k=1}^{4}",null," = ",false,"(5*sin(4*x)-4*sin(5*x))/(4*sin(x/2)^2)"]'),
    ('series_simplified_linear_sinh', 'function_series_endpoint', '@Z_(k=1)^4 k*sinh(kx)',
     '["\\sum_{k=1}^{4}",null," = ",false,"(4*sinh(5*x)-5*sinh(4*x))/(4*sinh(x/2)^2)"]'),
    ('series_simplified_linear_cosh', 'function_series_endpoint', '@Z_(k=1)^4 k*cosh(kx)',
     '["\\sum_{k=1}^{4}",null," = ",false,"(1-5*cosh(4*x)+4*cosh(5*x))/(4*sinh(x/2)^2)"]'),
    ('series_weighted_second_derivative', 'function_series_endpoint', '{Dxx(@Z_(k=1)^4 sin(kx))|x=pi/2}',
     '["\\sum_{k=1}^{4}","8"," = ",true]'),
    ('series_weighted_square_sine_unbound', 'function_series_endpoint', '@Z_(k=1)^100 -k^2*sin(kx)',
     '["\\sum_{k=1}^{100}",null," = "]'),
    ('series_weighted_square_sine', 'function_series_endpoint', '{@Z_(k=1)^4 -k^2*sin(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","8"," = ",true]'),
    ('series_weighted_square_cosine', 'function_series_endpoint', '{@Z_(k=1)^4 k^2*cos(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","12"," = ",true]'),
    ('series_weighted_linear_sine', 'function_series_endpoint', '{@Z_(k=1)^4 k*sin(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","-2"," = ",true]'),
    ('series_weighted_linear_cosine', 'function_series_endpoint', '{@Z_(k=1)^4 k*cos(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","2"," = ",true]'),
    ('series_weighted_cubic_sine', 'function_series_endpoint', '{@Z_(k=1)^4 k^3*sin(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","-26"," = ",true]'),
    ('series_weighted_quartic_cosine', 'function_series_endpoint', '{@Z_(k=1)^4 k^4*cos(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","240"," = ",true]'),
    ('series_weighted_quintic_sine', 'function_series_endpoint', '{@Z_(k=1)^4 k^5*sin(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","-242"," = ",true]'),
    ('series_weighted_sextic_cosine', 'function_series_endpoint', '{@Z_(k=1)^4 k^6*cos(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","4032"," = ",true]'),
    ('series_weighted_quintic_exponential', 'function_series_endpoint', '{@Z_(k=1)^4 k^5*exp(kx)|x=ln(2)}',
     '["\\sum_{k=1}^{4}","18458"," = ",true]'),
    ('series_weighted_sextic_sinh', 'function_series_endpoint', '{@Z_(k=1)^4 k^6*sinh(kx)|x=ln(2)}',
     '["\\sum_{k=1}^{4}","570099/16"," = ",true]'),
    ('series_weighted_sextic_cosh', 'function_series_endpoint', '{@Z_(k=1)^4 k^6*cosh(kx)|x=ln(2)}',
     '["\\sum_{k=1}^{4}","575917/16"," = ",true]'),
    ('series_weighted_shifted_polynomial', 'function_series_endpoint', '{@Z_(k=1)^4 (k+1)^2*sin(kx)|x=pi/2}',
     '["\\sum_{k=1}^{4}","-12"," = ",true,null,true]'),
    ('series_weighted_symbolic_coefficients', 'function_series_endpoint',
     '{@Z_(k=1)^4 (a*k^2+b*k+c)*sin(kx)|x=pi/2; a=2,b=3,c=4}',
     '["\\sum_{k=1}^{4}","-22"," = ",true,null,true]'),
    ('series_weighted_sinh', 'function_series_endpoint', '{@Z_(k=1)^4 k^2*sinh(kx)|x=ln(2)}',
     '["\\sum_{k=1}^{4}","2739/16"," = ",true]'),
    ('series_weighted_cosh', 'function_series_endpoint', '{@Z_(k=1)^4 k^2*cosh(kx)|x=ln(2)}',
     '["\\sum_{k=1}^{4}","2797/16"," = ",true]'),
    ('series_weighted_exponential', 'function_series_endpoint', '{@Z_(k=1)^4 k^2*exp(kx)|x=ln(2)}',
     '["\\sum_{k=1}^{4}","346"," = ",true]'),
    ('series_weighted_renamed_scaled', 'function_series_endpoint', '{@Z_(j=1)^4 j*cos(2jt)|t=pi/4}',
     '["\\sum_{j=1}^{4}","2"," = ",true]')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.function_series_endpoints', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_from_string_function_series_endpoints',
    'expression_cases', 'function_series_endpoint')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
WITH functions(function) AS (VALUES ('sin'), ('cos'), ('sinh'), ('cosh'), ('exp')),
     powers(degree, second, third) AS (VALUES (1, 2, 3), (2, 4, 9), (3, 8, 27), (4, 16, 81))
INSERT INTO expression_cases(name, operation, input, expected)
SELECT 'series_moment_complex_' || function || '_' || degree, 'function_series_endpoint',
       '{@Z_(k=1)^3 k^' || degree || '*' || function || '(kx)|x=1+i}',
       json_array('\sum_{k=1}^{3}', function || '(1+i)+' || second || '*' || function || '(2+2i)+' ||
                  third || '*' || function || '(3+3i)', ' = ', json('true'))
FROM functions CROSS JOIN powers
WHERE true
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('series_explicit_sine_unbound', 'function_series_endpoint', '@Z_(k=1)^100 sin(kx)',
     '["\\sum_{k=1}^{100}\\sin(k\\mkern-2mu x)",null,"\\sin(50\\mkern-2mu x)"]'),
    ('series_explicit_sine_value', 'function_series_endpoint', '{@Z_(k=1)^100 sin(kx) | x=pi/6}',
     '["\\sum_{k=1}^{100}\\sin(k\\mkern-2mu x)","3/2+sqrt(3)","\\sin(50\\mkern-2mu x)"]'),
    ('series_explicit_sine_zero', 'function_series_endpoint', '{@Z_(k=1)^100 sin(kx) | x=0}',
     '["\\sum_{k=1}^{100}\\sin(k\\mkern-2mu x)","0","\\sin(50\\mkern-2mu x)"]'),
    ('series_explicit_cosine_unbound', 'function_series_endpoint', '@Z_(k=1)^100 cos(kx)',
     '["\\sum_{k=1}^{100}\\cos(k\\mkern-2mu x)",null,"\\sin(50\\mkern-2mu x)"]'),
    ('series_literal_sine_unbound', 'function_series_endpoint', 'sin(x)+sin(2x)+sin(3x)+...+sin(100x)',
     '["\\sum_{k=1}^{100}\\sin(k\\mkern-2mu x)",null]'),
    ('series_literal_sine_value', 'function_series_endpoint', '{sin(x)+sin(2x)+sin(3x)+...+sin(100x) | x=pi/6}',
     '["\\sum_{k=1}^{100}\\sin(k\\mkern-2mu x)","3/2+sqrt(3)"]'),
    ('series_literal_cosine_value', 'function_series_endpoint', '{cos(x)+cos(2x)+cos(3x)+…+cos(100x) | x=pi/6}',
     '["\\sum_{k=1}^{100}\\cos(k\\mkern-2mu x)","sqrt(3)/2"]'),
    ('series_literal_sinh_value', 'function_series_endpoint', '{sinh(x)+sinh(2x)+sinh(3x)+...+sinh(6x) | x=ln(2)}',
     '["\\sum_{k=1}^{6}\\sinh(k\\mkern-2mu x)","8001/128"]'),
    ('series_literal_cosh_value', 'function_series_endpoint', '{cosh(x)+cosh(2x)+cosh(3x)+...+cosh(6x) | x=ln(2)}',
     '["\\sum_{k=1}^{6}\\cosh(k\\mkern-2mu x)","8127/128"]'),
    ('series_literal_sine_zero', 'function_series_endpoint', '{sin(x)+sin(2x)+sin(3x)+...+sin(100x) | x=0}',
     '["\\sum_{k=1}^{100}\\sin(k\\mkern-2mu x)","0"]'),
    ('series_literal_cosine_zero', 'function_series_endpoint', '{cos(x)+cos(2x)+cos(3x)+...+cos(100x) | x=0}',
     '["\\sum_{k=1}^{100}\\cos(k\\mkern-2mu x)","100"]'),
    ('series_literal_unit_angles', 'function_series_endpoint', 'sin(1)+sin(2)+sin(3)+...+sin(6)',
     '["\\sum_{k=1}^{6}\\sin(k)","sin(3)*sin(7/2)/sin(1/2)"]'),
    ('series_literal_exp', 'function_series_endpoint', '{exp(x)+exp(2x)+exp(3x)+...+exp(6x) | x=ln(2)}',
     '["\\sum_{k=1}^{6}","126"]'),
    ('series_literal_erf', 'function_series_endpoint', '{erf(x)+erf(2x)+erf(3x)+...+erf(4x) | x=0}',
     '["\\sum_{k=1}^{4}","0"]'),
    ('series_literal_weighted_sine', 'function_series_endpoint', '{sin(x)+sin(2x)/2+sin(3x)/3+...+sin(4x)/4 | x=pi/2}',
     '["\\sum_{k=1}^{4}","2/3"]'),
    ('series_literal_weighted_cosine', 'function_series_endpoint', '{cos(x)+cos(2x)/2+cos(3x)/3+...+cos(4x)/4 | x=pi/2}',
     '["\\sum_{k=1}^{4}","-1/4"]'),
    ('series_literal_weighted_sinh', 'function_series_endpoint', '{sinh(x)+sinh(2x)/2+sinh(3x)/3+...+sinh(4x)/4 | x=ln(2)}',
     '["\\sum_{k=1}^{4}","639/128"]'),
    ('series_literal_weighted_cosh', 'function_series_endpoint', '{cosh(x)+cosh(2x)/2+cosh(3x)/3+...+cosh(4x)/4 | x=ln(2)}',
     '["\\sum_{k=1}^{4}","2179/384"]'),
    ('series_literal_skipped_index', 'function_series_endpoint', 'sin(x)+sin(2x)+sin(4x)+...+sin(100x)', 'null'),
    ('series_literal_mixed_function', 'function_series_endpoint', 'sin(x)+cos(2x)+sin(3x)+...+sin(100x)', 'null'),
    ('series_literal_fractional_endpoint', 'function_series_endpoint', 'sin(x)+sin(2x)+sin(3x)+...+sin(100.5x)', 'null'),
    ('series_literal_overlapping_endpoint', 'function_series_endpoint', 'sin(x)+sin(2x)+sin(3x)+...+sin(3x)', 'null'),
    ('series_literal_negative_endpoint', 'function_series_endpoint', 'sin(x)+sin(2x)+sin(3x)+...+sin(-100x)', 'null'),
    ('series_literal_non_linear_endpoint', 'function_series_endpoint', 'sin(x)+sin(2x)+sin(3x)+...+sin(100x^2)', 'null'),
    ('series_literal_weight_mismatch', 'function_series_endpoint', 'sin(x)+sin(2x)/2+sin(3x)/3+...+sin(100x)/99', 'null')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.shift_format', 'tests/expression/test_expr_to_string.c',
    'tests.test_expr_t_to_string.test_to_string_all.test_to_string_symmetric_shifts_TeX',
    'expression_cases', 'shift_format')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('shift_digamma_quotient', 'shift_format',
     '{digamma(i/x+1001)+digamma(1001-i/x)+digamma(i/x+1)+digamma(1-i/x)|x=1+i}',
     '{"plain":["1001 + i/x","1001 - i/x","1 + i/x","1 - i/x"],"function":["1001 + i/x","1001 - i/x","1 + i/x","1 - i/x"],"TeX":["1001 + \\frac{i}{x}","1001 - \\frac{i}{x}","1 + \\frac{i}{x}","1 - \\frac{i}{x}"]}'),
    ('shift_trigamma_quotient', 'shift_format',
     '{trigamma(i/t+7)+trigamma(7-i/t)|t=1+i}',
     '{"plain":["7 + i/t","7 - i/t"],"function":["7 + i/t","7 - i/t"],"TeX":["7 + \\frac{i}{t}","7 - \\frac{i}{t}"]}'),
    ('shift_different_functions', 'shift_format', '{sin(i/t+a)+cos(b-i/t)|a=2;b=3;t=1+i}',
     '{"plain":["a + i/t","b - i/t"],"function":["a + i/t","b - i/t"],"TeX":["a + \\frac{i}{t}","b - \\frac{i}{t}"]}'),
    ('shift_composite_divisor', 'shift_format', '{sin(i/(x+y)+c)+cos(c-i/(x+y))|x=1+i;y=2;c=3}',
     '{"plain":["c + i/(x + y)","c - i/(x + y)"],"function":["v1 = x + y","c + i/v1","c - i/v1"],"TeX":["c + \\frac{i}{x + y}","c - \\frac{i}{x + y}"]}'),
    ('shift_negative_centre', 'shift_format', '{sin(i/t-c)+cos(-c-i/t)|t=1+i;c=2}',
     '{"beautify":true,"plain":["-c + i/t","-c - i/t"],"function":["-c + i/t","-c - i/t"],"TeX":["-c + \\frac{i}{t}","-c - \\frac{i}{t}"]}'),
    ('shift_product_unchanged', 'shift_format', '{sin(i*t+c)+cos(c-i*t)|t=1+i;c=2}',
     '{"plain":["c + i·t","c - i·t"],"function":["c + i.t","c - i.t"],"TeX":["c + i\\mkern-2mu t","c - i\\mkern-2mu t"]}'),
    ('shift_positive_first', 'shift_format', '{-sin(t)+cos(t)|t=0.3}',
     '{"beautify":true,"plain":["cos(t) - sin(t)"],"function":["cos(t) - sin(t)"],"TeX":["\\cos(t) - \\sin(t)"]}'),
    ('shift_positive_numerator', 'shift_format', '{(-a+b)/x|a=2;b=5;x=1+i}',
     '{"beautify":true,"plain":["(b - a)/x"],"function":["(b - a)/x"],"TeX":["\\frac{b - a}{x}"]}'),
    ('shift_positive_imaginary_coefficient', 'shift_format', '{(-i*a+i*b)/x|a=2;b=5;x=1+i}',
     '{"beautify":true,"plain":["b·i - a·i"],"function":["b.i - a.i"],"TeX":["\\frac{b"]}')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.shared_centres', 'tests/expression/test_expr_to_string.c',
    'tests.test_expr_t_to_string.test_to_string_all.test_to_string_shared_centres',
    'expression_cases', 'shared_centre')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('shared_centre_radical', 'shared_centre', '["sqrt(3)-2*cos(pi/18)","4*cos(pi/18)+sqrt(3)"]',
     '{"plain":["√(3) - 2·cos(π/18)","√(3) + 4·cos(π/18)"],"TeX":["\\sqrt{3} - 2\\mkern-2mu \\cos(\\frac{\\pi}{18})","\\sqrt{3} + 4\\mkern-2mu \\cos(\\frac{\\pi}{18})"]}'),
    ('shared_centre_functions', 'shared_centre', '["sin(t)+c","c-sin(t)","exp(t)+c"]',
     '{"plain":["c + sin(t)","c - sin(t)","c + exp(t)"],"TeX":["c + \\sin(t)","c - \\sin(t)","c + e^{t}"]}'),
    ('shared_centre_negative', 'shared_centre', '["sin(t)-sqrt(5)","-sqrt(5)-cos(t)"]',
     '{"plain":["-√(5) + sin(t)","-√(5) - cos(t)"],"TeX":["-\\sqrt{5} + \\sin(t)","-\\sqrt{5} - \\cos(t)"]}'),
    ('shared_centre_multiple', 'shared_centre', '["sin(t)+a+b","b+cos(t)+a"]',
     '{"plain":["a + b + sin(t)","a + b + cos(t)"],"TeX":["a + b + \\sin(t)","a + b + \\cos(t)"]}'),
    ('shared_centre_opposite_sign_control', 'shared_centre', '["sin(t)+c","cos(t)-c"]',
     '{"plain":["sin(t) + c","cos(t) - c"],"TeX":["\\sin(t) + c","\\cos(t) - c"]}'),
    ('shared_centre_duplicate', 'shared_centre', '["sin(t)+c+c","c+cos(t)"]',
     '{"plain":["c + sin(t) + c","c + cos(t)"],"TeX":["c + \\sin(t) + c","c + \\cos(t)"]}')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.unit_coefficients_TeX', 'tests/expression/test_expr_to_string.c',
    'tests.test_expr_t_to_string.test_to_string_all.test_to_string_unit_coefficients_TeX',
    'expression_cases', 'unit_coefficient_TeX')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('unit_conjugate_difference', 'unit_coefficient_TeX', 'B-conj(B)', 'B - \overline{B}'),
    ('unit_reversed_conjugate', 'unit_coefficient_TeX', 'conj(B)-B', '-B + \overline{B}'),
    ('unit_conjugate_integer_control', 'unit_coefficient_TeX', '21*B-conj(B)', '21\mkern-2mu B - \overline{B}'),
    ('unit_conjugate_weight_control', 'unit_coefficient_TeX', 'B-3*conj(B)', 'B - 3\mkern-2mu \overline{B}'),
    ('unit_conjugate_standalone_integer', 'unit_coefficient_TeX', 'B-1-conj(B)', 'B - \overline{B} - 1'),
    ('unit_projection_positive', 'unit_coefficient_TeX', 'B/2+conj(B)/2', '\mathfrak{Re}\mathopen{}\left\{B\right\}'),
    ('unit_projection_negative', 'unit_coefficient_TeX', '-B/2-conj(B)/2', '-\mathfrak{Re}\mathopen{}\left\{B\right\}'),
    ('unit_common_conjugate_positive', 'unit_coefficient_TeX', '(1/2)*B+(1/2)*conj(B)', '\mathfrak{Re}\mathopen{}\left\{B\right\}'),
    ('unit_common_conjugate_negative', 'unit_coefficient_TeX', '(-1/2)*B+(-1/2)*conj(B)', '-\mathfrak{Re}\mathopen{}\left\{B\right\}'),
    ('unit_projection_mixed_sum', 'unit_coefficient_TeX', 'A-Re(B)+conj(C)', 'A - \mathfrak{Re}\mathopen{}\left\{B\right\} + \overline{C}')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.exact_decimal_literals', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_from_string_exact_decimal_literals', 'expression_cases', 'exact_decimal_literal')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('exact_decimal_coefficient', 'exact_decimal_literal', '0.01/484', '1/48400'),
    ('exact_decimal_power', 'exact_decimal_literal', '(0.01)^2', '1/10000'),
    ('exact_decimal_scientific', 'exact_decimal_literal', '7.25e-3', '29/4000'),
    ('exact_decimal_integer', 'exact_decimal_literal', '1.2e3', '1200'),
    ('exact_decimal_leading_point', 'exact_decimal_literal', '.5', '1/2'),
    ('exact_decimal_binding', 'exact_decimal_literal', '{0.01*x | x=8}', '2/25'),
    ('exact_decimal_extreme_exponent', 'exact_decimal_literal', '1e99999999999999999999', 'inf')
ON CONFLICT(name) DO UPDATE SET expected=excluded.expected;

-- Mathematical letter case is significant in the canonical special-function names.
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('named_appell_F1_direct', 'numeric_callable', 'appell_F1(1,1,1,2,0,0)', '1'),
    ('named_bessel_I_direct', 'numeric_callable', 'bessel_I(0,0)', '1'),
    ('named_bessel_J_direct', 'numeric_callable', 'bessel_J(0,0)', '1'),
    ('named_bessel_K_direct', 'numeric_callable', 'bessel_K(0,1)', 'besselk(0,1)'),
    ('named_bessel_Y_direct', 'numeric_callable', 'bessel_Y(0,1)', 'bessely(0,1)'),
    ('named_lambert_W0_direct', 'numeric_callable', 'lambert_W0(0)', '0'),
    ('named_lambert_Wm1_direct', 'numeric_callable', 'lambert_Wm1(-0.2)', 'lambertwm1(-0.2)'),
    ('named_lambert_Wn_direct', 'numeric_callable', 'lambert_Wn(0,0.7)', 'lambertwn(0,0.7)'),
    ('named_lauricella_F_direct', 'numeric_callable', 'lauricella_F(1,1,1,2,0)', '1'),
    ('named_struve_H_direct', 'numeric_callable', 'struve_H(0,0)', '0'),
    ('named_struve_L_direct', 'numeric_callable', 'struve_L(0,0)', '0'),
    ('named_lommel_S_direct', 'numeric_callable', 'lommel_S(1,0,1)', 'lommels(1,0,1)'),
    ('named_appell_F1_binding', 'numeric_callable', '{x|x=appell_F1(1,1,1,2,0,0)}', '1'),
    ('named_bessel_I_binding', 'numeric_callable', '{x|x=bessel_I(0,0)}', '1'),
    ('named_bessel_J_binding', 'numeric_callable', '{x|x=bessel_J(0,0)}', '1'),
    ('named_bessel_K_binding', 'numeric_callable', '{x|x=bessel_K(0,1)}', 'besselk(0,1)'),
    ('named_bessel_Y_binding', 'numeric_callable', '{x|x=bessel_Y(0,1)}', 'bessely(0,1)'),
    ('named_lambert_W0_binding', 'numeric_callable', '{x|x=lambert_W0(0)}', '0'),
    ('named_lambert_Wm1_binding', 'numeric_callable', '{x|x=lambert_Wm1(-0.2)}', 'lambertwm1(-0.2)'),
    ('named_lambert_Wn_binding', 'numeric_callable', '{x|x=lambert_Wn(0,0.7)}', 'lambertwn(0,0.7)'),
    ('named_lauricella_F_binding', 'numeric_callable', '{x|x=lauricella_F(1,1,1,2,0)}', '1'),
    ('named_struve_H_binding', 'numeric_callable', '{x|x=struve_H(0,0)}', '0'),
    ('named_struve_L_binding', 'numeric_callable', '{x|x=struve_L(0,0)}', '0'),
    ('named_lommel_S_binding', 'numeric_callable', '{x|x=lommel_S(1,0,1)}', 'lommels(1,0,1)')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

-- Canonical polynomial names work in expressions and in binding values, with the same real/complex conventions.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('poly_chebyshev_Tn_direct', 'numeric_callable', 'chebyshev_Tn(3,2)', '26'),
    ('poly_chebyshev_Un_direct', 'numeric_callable', 'chebyshev_Un(3,2)', '56'),
    ('poly_hermite_Hn_direct', 'numeric_callable', 'hermite_Hn(3,2)', '40'),
    ('poly_chebyshev_Tn_binding', 'numeric_callable', '{x|x=chebyshev_Tn(3,2)}', '26'),
    ('poly_chebyshev_Un_binding', 'numeric_callable', '{x|x=chebyshev_Un(3,2)}', '56'),
    ('poly_hermite_Hn_binding', 'numeric_callable', '{x|x=hermite_Hn(3,2)}', '40'),
    ('poly_chebyshev_Tn_complex', 'numeric_callable', 'chebyshev_Tn(3,i)', '-7i'),
    ('poly_chebyshev_Un_complex', 'numeric_callable', 'chebyshev_Un(3,i)', '-12i'),
    ('poly_hermite_Hn_complex', 'numeric_callable', 'hermite_Hn(3,i)', '-20i');

-- Numeric callable coverage includes exact-width operands, aliases and preserved binding function calls.
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.integral_exp_root_primitive', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'integral_exp_root_primitive')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('exp_root_primitive_symbolic_positive', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)+C)|y=1;C=1}', 'formal'),
    ('exp_root_primitive_symbolic_negative', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)+C)|y=1;C=-1}', 'formal'),
    ('exp_root_primitive_positive', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)+1)|y=1}', 'derivative'),
    ('exp_root_primitive_parameter_rate', 'integral_exp_root_primitive', '{1/sqrt(2exp(b*y)+1)|y=1;b=2}', 'formal'),
    ('exp_root_primitive_zero', 'integral_exp_root_primitive', '{1/sqrt(2exp(y))|y=1}', 'derivative'),
    ('exp_root_primitive_negative_literal', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)-1)|y=1}', 'derivative'),
    ('exp_root_primitive_scaled_rate', 'integral_exp_root_primitive', '{3/sqrt(4exp(2y+1)+2)|y=1}', 'derivative')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT INTO expression_cases(name, operation, input, expected, precision) VALUES
    ('exp_root_small_positive_tail', 'integral_exp_root', '{ @S 1/sqrt(A*exp(y)+C) dy | y=1; A=1e80; C=1 }',
     '2*(atanh(1/sqrt(1e80+1))-atanh(1/sqrt(1e80*exp(1)+1)))', 53),
    ('exp_root_small_negative_tail', 'integral_exp_root', '{ @S 1/sqrt(A*exp(y)+C) dy | y=1; A=1e80; C=-1 }',
     '2*(atan(1/sqrt(1e80-1))-atan(1/sqrt(1e80*exp(1)-1)))', 53)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.integral_exp_root', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'integral_exp_root')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('exp_root_zero_offset', 'integral_exp_root', '{ @S 1/sqrt(2e^y+C) dy | y=1; C=0 }',
     'sqrt(2)*(1-exp(-1/2))'),
    ('exp_root_positive_offset', 'integral_exp_root', '{ @S 1/sqrt(2e^y+C) dy | y=1; C=1 }',
     'ln((sqrt(2*exp(1)+1)-1)/(sqrt(2*exp(1)+1)+1))-ln((sqrt(3)-1)/(sqrt(3)+1))'),
    ('exp_root_negative_offset', 'integral_exp_root', '{ @S 1/sqrt(2e^y+C) dy | y=1; C=-1 }',
     '2*(atan(sqrt(2*exp(1)-1))-atan(1))'),
    ('exp_root_reversed_interval', 'integral_exp_root', '@S_1^0 1/sqrt(2e^y+1) dy',
     'ln((sqrt(3)-1)/(sqrt(3)+1))-ln((sqrt(2*exp(1)+1)-1)/(sqrt(2*exp(1)+1)+1))'),
    ('exp_root_shifted_scaled', 'integral_exp_root', '@S_0^1 3/sqrt(4exp(2y+1)+2) dy',
     '3/sqrt(2)*(atanh(sqrt(2)/sqrt(4*exp(1)+2))-atanh(sqrt(2)/sqrt(4exp(3)+2)))'),
    ('exp_root_constant_base', 'integral_exp_root', '@S_0^1 1/sqrt(3*2^y) dy',
     '2/(sqrt(3)*ln(2))*(1-1/sqrt(2))'),
    ('exp_root_negative_amplitude', 'integral_exp_root', '@S_0^1 1/sqrt(10-exp(y)) dy',
     '(ln((sqrt(10)-sqrt(10-exp(1)))/(sqrt(10)+sqrt(10-exp(1))))-ln((sqrt(10)-3)/(sqrt(10)+3)))/sqrt(10)'),
    ('exp_root_negative_rate', 'integral_exp_root', '@S_0^1 1/sqrt(2exp(-y)) dy',
     'sqrt(2)*(exp(1/2)-1)'),
    ('exp_root_integrable_endpoint', 'integral_exp_root', '@S_0^1 1/sqrt(2exp(y)-2) dy',
     'sqrt(2)*atan(sqrt(exp(1)-1))'),
    ('exp_root_improper_zero_offset', 'integral_exp_root', '@S_0^inf 1/sqrt(2exp(y)) dy', 'sqrt(2)'),
    ('exp_root_improper_negative_offset', 'integral_exp_root', '@S_0^inf 1/sqrt(2exp(y)-1) dy', '@pi/2')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
-- This operation stores working bits; the small-tail cases deliberately exercise double precision.
UPDATE expression_cases SET precision=256 WHERE operation='integral_exp_root' AND precision<>53;
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.numeric_callables', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'numeric_callable')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.numeric_rendering', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'numeric_render')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('powmod_small', 'numeric_callable', 'powmod(4,13,497)', '445'),
    ('powmod_constant_integral', 'numeric_callable', '@S_0^1 powmod(2,3,5) dx', '3'),
    ('powmod_constant_derivative', 'numeric_callable', 'd/dx(powmod(2,3,5)*x)', '3'),
    ('gcd_constant_derivative', 'numeric_callable', 'd/dx(gcd(12,18)*x)', '6'),
    ('powmod_original_1024_bit_operands', 'numeric_callable', 'powmod(97128534091520311412078115857277804694847051212062325796074342369690095336188259720191897149915705664060543226969142780725822337496727884659619483452400511633617514882293623272276764857474830401626415841035971458657442133370777607803464071101158385124186229995073045427448918227959893082282919863982307657776,12498846048367045926370716602609982839721641429222959125376462582674869403443996972648628699047686839582654905384579341977328653591547736951147466435189662,6864797660130609714981900799081393217269435300143305409394463459185543183397656052122559640661454554977296311391480858037121987999716643812574028291115057151)', '2426708632600750408239409365812084203045954767825752904850432903124881973671736995843975761420957307038639263776889805180044277291952781094657794927246068639'),
    ('powmod_huge_exponent', 'numeric_callable', 'powmod(2,2^512,17)', '1'),
    ('powmod_negative_base', 'numeric_callable', 'powmod(-2,5,13)', '7'),
    ('powmod_zero_exponent', 'numeric_callable', 'powmod(9,0,7)', '1'),
    ('powmod_one_modulus', 'numeric_callable', 'powmod(9,0,1)', '0'),
    ('powmod_zero_modulus', 'numeric_callable', 'powmod(2,3,0)', 'NAN'),
    ('powmod_negative_exponent', 'numeric_callable', 'powmod(2,-1,7)', 'NAN'),
    ('powmod_fraction', 'numeric_callable', 'powmod(2,1/2,7)', 'NAN'),
    ('integer_bit_length', 'numeric_callable', 'bit_length(2^1024)', '1025'),
    ('integer_bit_length_zero', 'numeric_callable', 'bit_length(0)', '0'),
    ('integer_bit_length_negative', 'numeric_callable', 'bit_length(-8)', '4'),
    ('integer_bit_length_fraction', 'numeric_callable', 'bit_length(1/2)', 'NAN'),
    ('integer_set_bit', 'numeric_callable', 'set_bit(0,701)', '2^701'),
    ('integer_clear_bit', 'numeric_callable', 'clear_bit(2^701+5,701)', '5'),
    ('integer_test_bit', 'numeric_callable', 'test_bit(2^701,701)', '1'),
    ('integer_test_bit_absent', 'numeric_callable', 'test_bit(2^701,700)', '0'),
    ('integer_negative_index', 'numeric_callable', 'set_bit(0,-1)', 'NAN'),
    ('integer_fractional_index', 'numeric_callable', 'clear_bit(0,1/2)', 'NAN'),
    ('integer_index_overflow', 'numeric_callable', 'test_bit(0,2^128)', 'NAN'),
    ('integer_prove_prime', 'numeric_callable', 'prove_prime(97)', '1'),
    ('integer_prove_composite', 'numeric_callable', 'prove_prime(91)', '0'),
    ('integer_divmod_quotient', 'numeric_callable', 'divmod(17,5,0)', '3'),
    ('integer_divmod_remainder', 'numeric_callable', 'divmod(17,5,1)', '2'),
    ('integer_divmod_negative', 'numeric_callable', 'divmod(-17,5,0)', '-3'),
    ('integer_divmod_negative_remainder', 'numeric_callable', 'divmod(-17,5,1)', '-2'),
    ('integer_divmod_zero', 'numeric_callable', 'divmod(17,0,0)', 'NAN'),
    ('integer_divmod_selector', 'numeric_callable', 'divmod(17,5,2)', 'NAN'),
    ('integer_gcdext_gcd', 'numeric_callable', 'gcdext(30,21,0)', '3'),
    ('integer_gcdext_identity', 'numeric_callable', '30*gcdext(30,21,1)+21*gcdext(30,21,2)', '3'),
    ('integer_gcdext_selector', 'numeric_callable', 'gcdext(30,21,3)', 'NAN'),
    ('alias_bit_and', 'numeric_callable', 'bit_and(12,10)', '8'),
    ('alias_bit_or', 'numeric_callable', 'bit_or(12,10)', '14'),
    ('alias_bit_xor', 'numeric_callable', 'bit_xor(12,10)', '6'),
    ('alias_bit_not', 'numeric_callable', 'bit_not(12)', 'not(12)'),
    ('alias_arithmetic_add', 'numeric_callable', 'add(2,3)', '5'),
    ('alias_arithmetic_sub', 'numeric_callable', 'sub(2,3)', '-1'),
    ('alias_arithmetic_mul', 'numeric_callable', 'mul(2,3)', '6'),
    ('alias_arithmetic_div', 'numeric_callable', 'div(2,3)', '2/3'),
    ('alias_arithmetic_neg', 'numeric_callable', 'neg(2)', '-2'),
    ('alias_add_long', 'numeric_callable', 'add_long(2,3)', '5'),
    ('alias_mul_long', 'numeric_callable', 'mul_long(2,3)', '6'),
    ('alias_pow_int', 'numeric_callable', 'pow_int(2,13)', '8192'),
    ('alias_inverse', 'numeric_callable', 'inv(2)', '1/2'),
    ('alias_square', 'numeric_callable', 'sqr(13)', '169'),
    ('alias_pow10', 'numeric_callable', 'pow10(3)', '1000'),
    ('alias_ldexp', 'numeric_callable', 'ldexp(3,5)', '96'),
    ('alias_mul_pow10', 'numeric_callable', 'mul_pow10(3,-2)', '3/100'),
    ('alias_arg', 'numeric_callable', 'arg(1+i)', '@pi/4'),
    ('alias_polylog1', 'numeric_callable', 'polylog1(0)', '0'),
    ('alias_tetragamma', 'numeric_callable', 'tetragamma(1)', 'polygamma(2,1)'),
    ('alias_sincos_sine', 'numeric_callable', 'sincos(0,0)', '0'),
    ('alias_sincos_cosine', 'numeric_callable', 'sincos(0,1)', '1'),
    ('alias_sinhcosh_sinh', 'numeric_callable', 'sinhcosh(0,0)', '0'),
    ('alias_sinhcosh_cosh', 'numeric_callable', 'sinhcosh(0,1)', '1'),
    ('clausen_explicit_order', 'numeric_callable', 'clausen(1,1)', 'Cl(1,1)'),
    ('clausen_legacy_one', 'numeric_callable', 'clausen(1)', 'clausen2(1)'),
    ('powmod_binding', 'numeric_callable', '{x | x=powmod(4,13,497)}', '445'),
    ('bit_length_binding', 'numeric_callable', '{x | x=bit_length(2^1024)}', '1025'),
    ('alias_binding_inverse', 'numeric_callable', '{x | x=inv(2)}', '1/2'),
    ('alias_binding_pow10', 'numeric_callable', '{x | x=pow10(3)}', '1000'),
    ('alias_binding_sinhcosh', 'numeric_callable', '{x | x=sinhcosh(0,1)}', '1'),
    ('clausen_binding_order', 'numeric_callable', '{x | x=clausen(1,1)}', 'Cl(1,1)'),
    ('powmod_finite_sum', 'numeric_callable', '@Z_(k=1)^3 powmod(2,k,7)', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('symbolic_powmod', 'numeric_render', '{powmod(a,b,c) | a=4,b=13,c=497}', 'powmod('),
    ('symbolic_bit_edit', 'numeric_render', '{set_bit(a,b) | a=0,b=701}', 'set_bit('),
    ('symbolic_divmod', 'numeric_render', '{divmod(a,b,c) | a=17,b=5,c=1}', 'divmod('),
    ('symbolic_gcdext', 'numeric_render', '{gcdext(a,b,c) | a=30,b=21,c=2}', 'gcdext('),
    ('symbolic_binding_powmod', 'numeric_render', '{x | x=powmod(4,13,497)}', 'powmod(');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.real_sample', 'tests/expression/test_expression.c', 'tests.test_expr_real_sample',
    'expression_cases', 'real_sample')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('sample_nested_acos_root_power', 'real_sample', '{acos(sin(2-x*y)-10^(-root(x^2+y^2,5))) | x=0.37,y=-0.29}', 'accept'),
    ('sample_positive_variable_power', 'real_sample', '{x^y | x=0.37,y=-0.29}', 'accept'),
    ('sample_positive_integer_power', 'real_sample', '{x^3 | x=0.37}', 'accept'),
    ('sample_positive_fractional_power', 'real_sample', '{x^(1/5) | x=0.37}', 'accept'),
    ('sample_positive_negative_power', 'real_sample', '{x^(-3) | x=0.37}', 'accept'),
    ('sample_positive_fifth_root', 'real_sample', '{root(x,5) | x=0.37}', 'accept'),
    ('sample_positive_square_root', 'real_sample', '{sqrt(x) | x=0.37}', 'accept'),
    ('sample_positive_cube_root', 'real_sample', '{cubrt(x) | x=0.37}', 'accept'),
    ('sample_positive_large_root', 'real_sample', '{root(x,2147483647) | x=0.37}', 'accept'),
    ('sample_negative_integer_power_fallback', 'real_sample', '{x^3 | x=-0.37}', 'fallback'),
    ('sample_negative_principal_root_fallback', 'real_sample', '{root(x,5) | x=-0.37}', 'fallback'),
    ('sample_zero_principal_root_fallback', 'real_sample', '{root(x,5) | x=0}', 'fallback'),
    ('sample_negative_cube_root_fallback', 'real_sample', '{cubrt(x) | x=-0.37}', 'fallback'),
    ('sample_variable_root_order_fallback', 'real_sample', '{root(x,y) | x=0.37,y=5}', 'fallback'),
    ('sample_noninteger_root_order_fallback', 'real_sample', '{root(x,2.5) | x=0.37}', 'fallback'),
    ('sample_unsupported_root_order_fallback', 'real_sample', '{root(x,2147483648) | x=0.37}', 'fallback'),
    ('sample_outside_acos_domain_fallback', 'real_sample', '{acos(1+x) | x=0.37}', 'fallback'),
    ('sample_root_cancellation_fallback', 'real_sample', '{sqrt(x)-sqrt(x+1e-30) | x=1}', 'fallback'),
    ('sample_power_overflow_fallback', 'real_sample', '{10^x | x=400}', 'fallback'),
    ('sample_power_underflow_fallback', 'real_sample', '{10^x | x=-400}', 'fallback');

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('sample_tangent', 'real_sample', '{tan(x) | x=0.37}', 'accept'),
    ('sample_cotangent', 'real_sample', '{cot(x) | x=0.37}', 'accept'),
    ('sample_secant', 'real_sample', '{sec(x) | x=0.37}', 'accept'),
    ('sample_cosecant', 'real_sample', '{cosec(x) | x=0.37}', 'accept'),
    ('sample_tangent_pole_fallback', 'real_sample', '{tan(x) | x=pi/2}', 'fallback'),
    ('sample_cotangent_pole_fallback', 'real_sample', '{cot(x) | x=0}', 'fallback'),
    ('sample_sum_tangent_1000', 'real_sample', '{sum(k,1,1000,tan(k*x)) | x=0.01}', 'accept'),
    ('sample_sum_tangent_error_estimate', 'real_sample', '{sum(k,1,1000,tan(k*x)) | x=0.1}', 'estimate'),
    ('sample_sum_tangent_derivative', 'real_sample', '{sum(k,1,1000,k*(1+tan(k*x)^2)) | x=0.01}', 'accept'),
    ('sample_sum_shifted_index', 'real_sample', '{2+sum(j,3,27,sec(j*x)) | x=0.01}', 'accept'),
    ('sample_sum_negative_index', 'real_sample', '{sum(j,-3,-1,tan(j*x)) | x=0.03}', 'accept'),
    ('sample_sum_dependent_bound_fallback', 'real_sample', '{sum(k,1,x,tan(k*x)) | x=3}', 'fallback'),
    ('sample_sum_nested_fallback', 'real_sample', '{sum(k,1,3,sum(j,1,2,tan(j*k*x))) | x=0.03}', 'fallback'),
    ('sample_sum_work_limit_fallback', 'real_sample', '{sum(k,1,4097,tan(k*x)) | x=0.01}', 'fallback'),
    ('sample_sum_cancellation_fallback', 'real_sample', '{sum(k,-3,3,tan(k*x)) | x=0.03}', 'fallback');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.real_sum_derivative', 'tests/expression/test_expression.c', 'tests.test_expr_real_sample',
    'expression_cases', 'real_sum_derivative')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('sample_sum_tangent_first', 'real_sum_derivative', '{sum(k,1,1000,tan(k*x)) | x=0.01}', 'accept'),
    ('sample_sum_tangent_second', 'real_sum_derivative', '{sum(k,1,1000,k*(1+tan(k*x)^2)) | x=0.01}', 'accept'),
    ('sample_sum_secant_derivative', 'real_sum_derivative', '{sum(k,1,20,sec(k*(2*x+1))) | x=0.01}', 'accept');

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('exact_addition', 'evaluate', '1/3+1/6', '1/2');

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('trigonometric_identity', 'evaluate', 'sin(0)^2+cos(0)^2', '1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression', 'tests/expression/test_expression.c', 'tests.test_expression_database_cases', 'expression_cases', 'evaluate')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_prime_shorthand_cases', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace_prime_shorthand', 'expression_cases', 'test_from_string_laplace_prime_shorthand_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_from_string.c: test_from_string_laplace_prime_shorthand. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_561b746a48f13517', 'test_from_string_laplace_prime_shorthand_cases', '@L{y''''+4y''+5y-50t}', '@L{y''''(t)+4*y''(t)+5*y(t)-50*t}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f9d738f8ac1f9c71', 'test_from_string_laplace_prime_shorthand_cases', '@L{5y+4y''+y''''-50t}', '@L{5*y(t)+4*y''(t)+y''''(t)-50*t}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6349cf811ab3d0ce', 'test_from_string_laplace_prime_shorthand_cases', '@L(y''''+a*y''+b*y,x,p)', '@L(y''''(x)+a*y''(x)+b*y(x),x,p)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f08e164be0389ec3', 'test_from_string_laplace_prime_shorthand_cases', '@L(y''+@L(z'',x,p),t,s)', '@L(y''(t)+@L(z''(x),x,p),t,s)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_58fd07c0c4e77e22', 'test_from_string_laplace_prime_shorthand_cases', 'y+@L{y''+y}', 'y+@L{y''(t)+y(t)}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_88407bb6bd0a2270', 'test_from_string_laplace_prime_shorthand_cases', '@L{y''+y}+@L{y*t}', '@L{y''(t)+y(t)}+@L{y*t}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_function_uses_lowercase_builtin_names_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_function_uses_lowercase_builtin_names', 'expression_cases', 'test_to_string_function_uses_lowercase_builtin_names_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_function_uses_lowercase_builtin_names. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_38ab0e19563d3440', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Li1(x) }', 'return li1(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8688c04bfc29ac4b', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Li2(x) }', 'return li2(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8a0d188016724f74', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Ei(x) }', 'return ei(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2b10a19dbcc207ac', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ E1(x) }', 'return e1(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_40ba6c2f0ad7a5e8', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ normal_pdf(x) }', 'return normalpdf(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f3806b70020cea63', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ normal_cdf(x) }', 'return normalcdf(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a4478a4ebb0c37f2', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ normal_logpdf(x) }', 'return normallogpdf(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_de7346d5f7acee30', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_lower(a, x) }', 'return gammainclower(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2068cbbc69f78f65', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_upper(a, x) }', 'return gammaincupper(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2519facc25db032d', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_P(a, x) }', 'return gammaincp(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_05e0f8e93c1c9002', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_Q(a, x) }', 'return gammaincq(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2b6db995106e6eb4', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ is_prime(n) }', 'return isprime(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b00b436980d3690b', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ next_prime(n) }', 'return nextprime(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_53b743a27c19b5cb', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ prev_prime(n) }', 'return prevprime(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b6b027871c72b98e', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ AND(a, b) }', 'return and(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2072076407ce127a', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ OR(a, b) }', 'return or(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_06aa96995b412dc6', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ XOR(a, b) }', 'return xor(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_c5878f1926491b44', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ NOT(a) }', 'return not(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ae5d81ba7c1bd92a', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ SHL(a, b) }', 'return shl(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_196bb95d68666078', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ SHR(a, b) }', 'return shr(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e59f3806830cc134', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselJ(n, x) }', 'return besselj(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_dcec89f7b82b6bfd', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselY(n, x) }', 'return bessely(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_597a802907b56437', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselK(n, x) }', 'return besselk(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_68c2a39d23dce7cd', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselI(n, x) }', 'return besseli(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_63be85e777c932ce', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ bessel_I(n, x) }', 'return besseli(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_fb16fa830d81068d', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ besseli(n, x) }', 'return besseli(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_dc094e68f032d2a5', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ StruveH(n, x) }', 'return struveh(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_8f7d671c94fb4584', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struve_H(n, x) }', 'return struveh(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6c862ab491091a65', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struveh(n, x) }', 'return struveh(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_be86657e82588c59', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ StruveL(n, x) }', 'return struvel(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_7bb6f7b390a07568', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struve_L(n, x) }', 'return struvel(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_49af4a2058eb6519', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struvel(n, x) }', 'return struvel(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_be3027136363ae4a', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ harmonic_poly(n, x) }', 'return harmonicpoly(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5da5ce73765ed956', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ legendre_chi(s, x) }', 'return legendrechi(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_791f224d13ef44c7', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Cl2(x) }', 'return clausen2(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5f55276621be44cf', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Cl(n, x) }', 'return cl(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_866fc5d1549ff17c', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ LommelS(mu, nu, x) }', 'return lommels(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ee04f4be47578edd', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ LerchPhi(z, s, a) }', 'return lerchphi(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b011db8ab98c325e', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ qdigamma(q, z) }', 'return qdigamma(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_4a2b596e3b6a0d3d', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ W(x) }', 'return w(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_da4989c5c3123c7f', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ W0(x) }', 'return w0(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_736acd8aa9275507', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ W-1(x) }', 'return wm1(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f33d0d5e5bb9fc81', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Wn(n, x) }', 'return wn(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_9ca4f291410444e8', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ appell_F1(a, b1, b2, c, x, y) }', 'return appellf1(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_1d01b335bc0d20d5', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ lauricella_F(2, a, b1, b2, c, x, y) }', 'return lauricellaf(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_db2f3559f8284cc0', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ hypergeometric_pFq(1, 1, a, b, x) }', 'return hypergeometricpfq(');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_exact_rational_powers_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_exact_rational_powers', 'expression_cases', 'test_to_string_exact_rational_powers_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_exact_rational_powers. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9ee1785baa57e94b', 'test_to_string_exact_rational_powers_cases', '4^(3/2)', '8');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9568ab84cd706d6c', 'test_to_string_exact_rational_powers_cases', '9^(3/2)', '27');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f1e4ec666181bd62', 'test_to_string_exact_rational_powers_cases', '36^(3/2)', '216');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3569ee3cd88d5c79', 'test_to_string_exact_rational_powers_cases', '9^(-3/2)', '1/27');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_bb230502d8394220', 'test_to_string_exact_rational_powers_cases', '(9/4)^(3/2)', '27/8');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_680a73b835913657', 'test_to_string_exact_rational_powers_cases', '8^(2/3)', '4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a2c09251f5cf811a', 'test_to_string_exact_rational_powers_cases', '625^(3/4)', '125');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6798733feb9d266a', 'test_to_string_exact_rational_powers_cases', '(169/144)^(3/2)', '2197/1728');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d487135081b7333b', 'test_to_string_exact_rational_powers_cases', '2/5*(4*9^(3/2)+4^(3/2))', '232/5');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_indexed_principal_roots_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_indexed_principal_roots', 'expression_cases', 'test_to_string_indexed_principal_roots_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_indexed_principal_roots. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_11c85ba7bfc178f3', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x) | x=2}', '³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d8a6137b87f9c1b7', 'test_to_string_indexed_principal_roots_cases', '{root(x,4) | x=2}', '⁴√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_7596dc7aa7bc4750', 'test_to_string_indexed_principal_roots_cases', '{root(x,12) | x=2}', '¹²√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9601067f63f23cab', 'test_to_string_indexed_principal_roots_cases', '{²√(x) | x=2}', '√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_778854900b7ee3af', 'test_to_string_indexed_principal_roots_cases', '{root(x,2147483647) | x=2}', '²¹⁴⁷⁴⁸³⁶⁴⁷√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9ae0f61816919cca', 'test_to_string_indexed_principal_roots_cases', '{³√(⁴√(x)) | x=2}', '³√(⁴√x)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3f7485b871d25c07', 'test_to_string_indexed_principal_roots_cases', '{root(x,n) | x=2; n=4}', 'root(x, n)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_12a856cf69c1e5a7', 'test_to_string_indexed_principal_roots_cases', '{2*cubrt(x) | x=2}', '2·³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_57e01464a62575fb', 'test_to_string_indexed_principal_roots_cases', '{2*root(x,4) | x=2}', '2·⁴√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_0176a6999476c923', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x+1) | x=2}', '³√(x + 1)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_185519a361780104', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x^2) | x=-2}', '³√(x²)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_40b73aa342826a63', 'test_to_string_indexed_principal_roots_cases', '{³√x+1 | x=2}', '³√x + 1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_12248668e1407dc4', 'test_to_string_indexed_principal_roots_cases', '{³√x*y | x=2; y=3}', 'y·³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_51ae9caffd1cf271', 'test_to_string_indexed_principal_roots_cases', '{cubrt([long name]) | [long name]=2}', '³√([long name])');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_19e801b074628419', 'test_to_string_indexed_principal_roots_cases', '{cubrt(@alpha) | @alpha=2}', '³√α');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3df828c80dbe6f63', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x_1) | x_1=2}', '³√(x₁)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_493898b430f98d2e', 'test_to_string_indexed_principal_roots_cases', '{x*cubrt(2) | x=3}', '³√(2)·x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_110db59658fd9fce', 'test_to_string_indexed_principal_roots_cases', '{x*root(2,4) | x=3}', '⁴√(2)·x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_1f3fc9d95c9119af', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x)^2 | x=2}', '³√(x)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_aabaea26f5cdd40c', 'test_to_string_indexed_principal_roots_cases', '{1/root(x,12) | x=2}', '¹²√(x)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9908d0812f6d0541', 'test_to_string_indexed_principal_roots_cases', '{³√(x) | x=-8}', '³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a1c8c13e4da1a2ec', 'test_to_string_indexed_principal_roots_cases', '{⁴√(x) | x=-16}', '⁴√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e6bdd6dba307f2f3', 'test_to_string_indexed_principal_roots_cases', '{¹²√(x) | x=3+4i}', '¹²√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b7ef147a9af6feea', 'test_to_string_indexed_principal_roots_cases', '{x | x=³√π}', '³√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d93025d630c408c4', 'test_to_string_indexed_principal_roots_cases', '{x | x=⁴√π}', '⁴√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a022397754447b75', 'test_to_string_indexed_principal_roots_cases', '{x | x=√π}', '√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_38d038f8cf8c73e2', 'test_to_string_indexed_principal_roots_cases', '{x | x=³√π*2}', '³√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3e896b6fa2e6f53d', 'test_to_string_indexed_principal_roots_cases', '{x | x=(⁴√π)^2}', '⁴√');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_13f7693660c784d7', 'test_to_string_indexed_principal_roots_cases', '{x | x=(³√π)^2}', '³√(π)²');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e5001a7f8d166954', 'test_to_string_indexed_principal_roots_cases', '{x | x=³√(2)}', '³√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_29c35527c089214a', 'test_to_string_indexed_principal_roots_cases', '{x | x=⁴√(2)}', '⁴√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_0d0cd1ab9e0ab7dc', 'test_to_string_indexed_principal_roots_cases', '{x | x=2*³√(2)}', '·³√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6f0a9bc6cdd9eaee', 'test_to_string_indexed_principal_roots_cases', '{x | x=2*¹²√(2)}', '·¹²√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d3c50d3d523d3807', 'test_to_string_indexed_principal_roots_cases', '{x^(1/3) | x=-8}', '^⅓');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_complex_projection_TeX_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_complex_projection_TeX', 'expression_cases', 'test_to_string_complex_projection_TeX_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_complex_projection_TeX. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_49f6d920532ccfdf', 'test_to_string_complex_projection_TeX_cases', 'Re(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_47916602a946385f', 'test_to_string_complex_projection_TeX_cases', 'realpart(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_0238206a690af8ba', 'test_to_string_complex_projection_TeX_cases', 'real_part(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_175e420fcd9ee260', 'test_to_string_complex_projection_TeX_cases', 'ℜ(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_66e64dd2c44f6db6', 'test_to_string_complex_projection_TeX_cases', 'Im(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2f4a512b378d6d31', 'test_to_string_complex_projection_TeX_cases', 'imagpart(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_93edd597041c00fc', 'test_to_string_complex_projection_TeX_cases', 'imag_part(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f632b7e61d7d22c1', 'test_to_string_complex_projection_TeX_cases', 'ℑ(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_46b3f557656dd27d', 'test_to_string_complex_projection_TeX_cases', 'Re(x+y)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_193aa283bffb5e44', 'test_to_string_complex_projection_TeX_cases', 'Im(x+y)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b6814a47ad2e49b3', 'test_to_string_complex_projection_TeX_cases', 'Re(z)^2', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3d0c2572bafd03fe', 'test_to_string_complex_projection_TeX_cases', 'Im(z)^2', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a3390875dd55dca9', 'test_to_string_complex_projection_TeX_cases', '8*Re(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ca938f9b4208396c', 'test_to_string_complex_projection_TeX_cases', '8*Im(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_947dd4cf4f9d1a65', 'test_to_string_complex_projection_TeX_cases', '8*Re(z)^2', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5b8ee97b506c2a0c', 'test_to_string_complex_projection_TeX_cases', '8*Im(z)^2', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b80b9c47ae7d6475', 'test_to_string_complex_projection_TeX_cases', 'Re(z)*x', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3c0a6372ba227784', 'test_to_string_complex_projection_TeX_cases', 'Im(z)*x', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e26b761befdd6a1c', 'test_to_string_complex_projection_TeX_cases', '(1/2)*Re(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ae7ea7e0443cc4a9', 'test_to_string_complex_projection_TeX_cases', '(1/2)*Im(z)', 'Im');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_integrals_outside_fractions_TeX_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_integrals_outside_fractions_TeX', 'expression_cases', 'test_to_string_integrals_outside_fractions_TeX_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_integrals_outside_fractions_TeX. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_74cb15ff239a2878', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)/exp(x)', 'e^{-x}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8fe176945abe4b35', 'test_to_string_integrals_outside_fractions_TeX_cases', '-integral(exp(cosh(t)),t,0,x)/exp(x)', '-e^{-x}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e3f93138b96a265f', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)/c', '\frac{1}{c}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_825879a5d304c356', 'test_to_string_integrals_outside_fractions_TeX_cases', '(1+integral(exp(cosh(t)),t,0,x))/c', '\frac{1}{c}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d501756fe399b039', 'test_to_string_integrals_outside_fractions_TeX_cases', '1/integral(exp(cosh(t)),t,0,x)', '\left(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_dd5d64982e8a051f', 'test_to_string_integrals_outside_fractions_TeX_cases', '1/exp(integral(exp(cosh(t)),t,0,x))', 'e^{-');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a176a43b48fd2d63', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)^(-2)', '\left(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_7811b04c2979fda3', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)^(-1/2)', '\left(');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_indexed_principal_roots_invalid', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_indexed_principal_roots', 'expression_cases', 'test_to_string_indexed_principal_roots_invalid')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_indexed_principal_roots. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_03a3a063f760a8fd', 'test_to_string_indexed_principal_roots_invalid', '⁰√(2)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_262c685820f34215', 'test_to_string_indexed_principal_roots_invalid', '¹√(2)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5b0dc18c5e658907', 'test_to_string_indexed_principal_roots_invalid', '⁻³√(2)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8fb9a71860a75c5a', 'test_to_string_indexed_principal_roots_invalid', '³√', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b64276ea9aaabd2d', 'test_to_string_indexed_principal_roots_invalid', '³√()', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b6425bea9aaa8f4c', 'test_to_string_indexed_principal_roots_invalid', '³√(2', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_48b2bae6eb589b20', 'test_to_string_indexed_principal_roots_invalid', '²¹⁴⁷⁴⁸³⁶⁴⁸√(2)', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_compact_product_quotient_TeX_sources', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_compact_product_quotient_TeX', 'expression_cases', 'test_to_string_compact_product_quotient_TeX_sources')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_compact_product_quotient_TeX. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3b5a24a32a1074f7', 'test_to_string_compact_product_quotient_TeX_sources', 'a*(x+1)/(x^2+1)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a9c3b3d0b151ef65', 'test_to_string_compact_product_quotient_TeX_sources', 'b*(t-2)/(t^2+3)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_97a5cdeb7f1c68c7', 'test_to_string_compact_product_quotient_TeX_sources', '(x+1)/(2*(x^2+1))', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_95ff7451793c5299', 'test_to_string_compact_product_quotient_TeX_sources', 'c*(2*c*x+1)/(2*c*x*(c*x+1)+1)', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_inverse_rational_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_inverse_rational', 'expression_cases', 'test_from_string_inverse_rational_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('inverse of Equation-mode rational output', 'test_from_string_inverse_rational_check_parse_num_1', '{@Linv{-5*(-s-10/s^2-3)/(s*(s+4)+5)} | t=0}', '5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('expanded rational denominator', 'test_from_string_inverse_rational_check_parse_num_1', '{@Linv{(5*s^3+15*s^2+50)/(s^4+4*s^3+5*s^2)} | t=0}', '5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('repeated quadratic inverse', 'test_from_string_inverse_rational_check_parse_num_1', '{2*@Linv{1/(s^2+1)^2}/(sin(t)-t*cos(t)) | t=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('distinct quadratic inverse', 'test_from_string_inverse_rational_check_parse_num_1', '{3*@Linv{1/((s^2+1)*(s^2+4))}/(sin(t)-sin(2*t)/2) | t=1}', '1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero hyperbolic tangent', 'test_from_string_laplace_check_parse_num_1', '{@L(tanh(c*t)) | c=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero tangent without target', 'test_from_string_laplace_check_parse_num_1', '{@L(tan(c*t)) | c=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero tangent no half-plane restriction', 'test_from_string_laplace_check_parse_num_1', '{@L(tan(c*t)) | s=-2; c=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace logarithm', 'test_from_string_laplace_check_parse_num_1', '{@Linv{-(ln(s)+γ)/s} | t=1}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace logarithmic coefficient', 'test_from_string_laplace_check_parse_num_1', '{@Linv{-(ln(s)+γ)/s}/ln(2) | t=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace braced logarithm', 'test_from_string_laplace_check_parse_num_1', '{@L{ln(t)}/@eulermascheroni | s=1}', '-1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace braced inverse', 'test_from_string_laplace_check_parse_num_1', '{@Linv{1/s^2} | t=2}', '2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace pole', 'test_from_string_laplace_check_parse_num_1', '{@Linv(1/s) | t=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace repeated pole', 'test_from_string_laplace_check_parse_num_1', '{InverseLaplace(1/s^3) | t=2}', '2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace Unicode', 'test_from_string_laplace_check_parse_num_1', '{ℒ⁻¹(s/(s^2+1)) | t=0}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace shifted cosine', 'test_from_string_laplace_check_parse_num_1', '{@Linv((s-a)/((s-a)^2+1)) | t=0; a=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace polynomial', 'test_from_string_laplace_check_parse_num_1', '{@L(t^2+3*t+2) | s=2}', '2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace Bessel zero', 'test_from_string_laplace_check_parse_num_1', '{@L(J_0(t))*sqrt(5) | s=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Bessel ASCII alias', 'test_from_string_laplace_check_parse_num_1', 'J0(0)', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Bessel subscript alias', 'test_from_string_laplace_check_parse_num_1', 'J₀(0)', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic power', 'test_from_string_laplace_check_parse_num_1', '{@L(t^n,t) | s=2; n=3}', '3/8');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace fractional power', 'test_from_string_laplace_check_parse_num_2', '{@L(t^(-1/2)) | s=1}', 'sqrt(pi)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace square root', 'test_from_string_laplace_check_parse_num_2', '{2*@L(sqrt(t))/sqrt(pi) | s=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace cube root', 'test_from_string_laplace_check_parse_num_2', '{@L(cubrt(t))/gamma(4/3) | s=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace fourth root', 'test_from_string_laplace_check_parse_num_2', '{@L(root(t,4))/gamma(5/4) | s=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace reciprocal root', 'test_from_string_laplace_check_parse_num_2', '{@L(1/sqrt(t)) | s=1}', 'sqrt(pi)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Readable transform domain', 'test_from_string_laplace_check_parse_num_2', '{120/s^6 where (Re(s)>0) | s=2}', '15/8');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Binding domain condition', 'test_from_string_laplace_check_parse_num_2', '{120/s^6 | s=2; Re(s)>0}', '15/8');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Binding condition rendering', 'test_from_string_laplace_check_parse_expr_3', '{120/s^6 where (Re(s)>0) | s=2}', '{ 120/s⁶ | s = 2; Re(s) > 0 }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Multiple domain conditions', 'test_from_string_laplace_check_parse_num_4', '{gamma(n+1)/s^(n+1) where (Re(s) > 0; Re(n) > -1) | s=2; n=5}', '15/8');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace exponential', 'test_from_string_laplace_check_parse_num_5', '{@L(exp(-t)) | s=2}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic exponential notation', 'test_from_string_laplace_check_parse_num_5', '{@L(e^(a*t)) | s=2; a=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace negative rate half-plane', 'test_from_string_laplace_check_parse_num_5', '{@L(exp(a*t)) | s=-1; a=-2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace complex rate', 'test_from_string_laplace_check_parse_num_5', '{@L(exp(a*t)) | s=2+i; a=1+i}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Real-part bound round trip', 'test_from_string_laplace_check_parse_num_5', '{1/(s-a) | s=2+i; a=1+i; Re(s)>Re(a)}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace sine', 'test_from_string_laplace_check_parse_num_5', '{ℒ(sin(t)) | s=2}', '1/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace inferred time with Greek frequency', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(@omega*t)) | s=2; @omega=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace inferred time with free parameter', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(x*t)) | s=2; x=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace explicit source overrides time', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(x*t),x) | s=2; t=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace complementary error functions', 'test_from_string_laplace_check_parse_num_5', '{@L(erf(t))+@L(erfc(t)) | s=2}', '1/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace opposite error function rates', 'test_from_string_laplace_check_parse_num_5', '{@L(erf(t))+@L(erf(-t)) | s=2}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero error function rate', 'test_from_string_laplace_check_parse_num_5', '{@L(erf(at)) | s=2; a=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace time-weighted symbolic sine', 'test_from_string_laplace_check_parse_num_5', '{@L(tsin(at)) | s=2; a=1}', '4/25');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace time-weighted cosine', 'test_from_string_laplace_check_parse_num_5', '{@L(tcos(at)) | s=2; a=1}', '3/25');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace squared-time cosine', 'test_from_string_laplace_check_parse_num_5', '{@L(t^2*cos(at),t) | s=2; a=1}', '4/125');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace time-weighted exponential', 'test_from_string_laplace_check_parse_num_5', '{@L(t*exp(at)) | s=2; a=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace hyperbolic sine', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(at)) | s=2; a=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace hyperbolic cosine', 'test_from_string_laplace_check_parse_num_5', '{@L(cosh(at)) | s=2; a=1}', '2/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace imaginary hyperbolic rate', 'test_from_string_laplace_check_parse_num_5', '{@L(cosh(at)) | s=2; a=i}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace bound sine power', 'test_from_string_laplace_check_parse_num_5', '{@L(sin^n(t)) | s=1; n=5}', '3/13');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace even sine power', 'test_from_string_laplace_check_parse_num_5', '{ℒ(sin(t)^2) | s=1}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace even cosine power', 'test_from_string_laplace_check_parse_num_5', '{Laplace(cos(t)^4,t,p) | p=1}', '41/85');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace odd cosine power', 'test_from_string_laplace_check_parse_num_5', '{@L(cos(t)^3) | s=1}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic cosine sum', 'test_from_string_laplace_check_parse_num_5', '{@L(cos(t)^n,t) | s=1, n=3}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic sine sum', 'test_from_string_laplace_check_parse_num_5', '{@L(sin(t)^n,t) | s=1, n=5}', '3/13');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Integer domain satisfied', 'test_from_string_laplace_check_parse_num_5', '{1/s | s=2; n=5; nonnegative_integer(n)}', '1/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace negative frequency', 'test_from_string_laplace_check_parse_num_5', '{@L(sin(-2*t)^3) | s=1}', '-48/185');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace explicit target', 'test_from_string_laplace_check_parse_num_5', '{Laplace(t,t,p) | p=2}', '1/4');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_expr_6', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_expr_6')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace canonical call', 'test_from_string_laplace_check_parse_expr_6', '@L(f(t),t,p)', '{ ℒ(f(t), t, p) | p = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_TeX_7', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_TeX_7')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace convergence', 'test_from_string_laplace_check_parse_TeX_7', '@L(t)', '\left\{ \frac{1}{s^{2}}\quad (\mathfrak{Re}(s)>0) \;\middle|\; s = ? \right\}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('w followed by call parentheses selects principal Lambert W', 'test_from_string_function_hash_check_parse_expr_1', 'w(x)', '{ W(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('w0 followed by call parentheses selects Lambert W zero branch', 'test_from_string_function_hash_check_parse_expr_1', 'w0(x)', '{ W₀(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('wm1 followed by call parentheses selects Lambert W minus-one branch', 'test_from_string_function_hash_check_parse_expr_1', 'wm1(x)', '{ W₋₁(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('wn followed by call parentheses selects a named Lambert W branch', 'test_from_string_function_hash_check_parse_expr_1', 'wn(n,x)', '{ Wₙ(n, x) | x = ?; n = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_2', 'zeta(3)', '1.202056903159594285399738161511449990764986292340498881792271555');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zetap is registered in the perfect hash', 'test_from_string_function_hash_check_parse_expr_3', 'zetap(x)', '{ ζ''(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zetap bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_3', '{ zetap(x) | x = 3 }', '{ ζ''(x) | x = 3 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta-prime symbol selects the Riemann derivative', 'test_from_string_function_hash_check_parse_expr_3', 'ζ''(x)', '{ ζ''(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta-prime symbol selects the Hurwitz derivative', 'test_from_string_function_hash_check_parse_expr_3', 'ζ''(s, a)', '{ ζ''(s, a) | s = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('the zeta symbol is a perfect-hash alias', 'test_from_string_function_hash_check_parse_expr_3', 'ζ(x)', '{ ζ(x) | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('the zeta symbol evaluates through its alias', 'test_from_string_function_hash_check_parse_num_4', 'ζ(3)', '1.202056903159594285399738161511449990764986292340498881792271555');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('two-argument zeta selects Hurwitz zeta', 'test_from_string_function_hash_check_parse_num_4', 'zeta(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('two-argument zeta symbol selects Hurwitz zeta', 'test_from_string_function_hash_check_parse_num_4', 'ζ(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zetah is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'zetah(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta2 is a perfect-hash alias', 'test_from_string_function_hash_check_parse_num_4', 'zeta2(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zatahp is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'zatahp(2.5, 101)', '-0.003491619656530338106744558404347153037971242923601833749838331612');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('qdigamma is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'qdigamma(1/2,1)', '-0.4205290343560457797847369304069241');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('q-digamma symbol is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'ψq(1/2,1)', '-0.4205290343560457797847369304069241');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('qdigamma bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_5', '{ qdigamma(q,z) | q=1/2; z=1 }', '{ ψq(q, z) | q = ¹⁄₂; z = 1 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('q-digamma symbol bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_5', '{ ψq(q,z) | q=1/2; z=1 }', '{ ψq(q, z) | q = ¹⁄₂; z = 1 }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_6', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_6')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta2p is a perfect-hash alias', 'test_from_string_function_hash_check_parse_num_6', 'zeta2p(2.5, 101)', '-0.003491619656530338106744558404347153037971242923601833749838331612');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_7', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_7')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('infinite product is a built-in function', 'test_from_string_function_hash_check_parse_expr_7', 'product(k,1,inf,1-1/k^2)', 'Π_(k=1)^∞ (1 - 1/k²)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_8', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_8')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('harmonic_poly is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_8', 'harmonic_poly(4, 1/2)', '131/192');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_9', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_9')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('harmonic_poly bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_9', '{ harmonic_poly(n,x) | x=1/2; n=4 }', '{ Hn(n, x) | x = ¹⁄₂; n = 4 }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_10', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_10')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Lerch phi evaluates through its polylogarithm reduction', 'test_from_string_function_hash_check_parse_num_10', 'lerch_phi(0.5,2,1)', '1.164481052930025011805312640319360217488396949612252850868694096');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('LerchPhi is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_10', 'LerchPhi(0.5,2,1)', '1.164481052930025011805312640319360217488396949612252850868694096');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('capital phi is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_10', 'Φ(0.5,2,1)', '1.164481052930025011805312640319360217488396949612252850868694096');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_TeX_11', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_TeX_11')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Lerch phi renders as capital phi', 'test_from_string_function_hash_check_parse_TeX_11', 'lerch_phi(1/2,2,1)', '\Phi\left(\frac{1}{2}, 2, 1\right)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('q-digamma renders its base as a subscript', 'test_from_string_function_hash_check_parse_TeX_11', 'qdigamma(1/2,1)', '\psi_{\frac{1}{2}}\left(1\right)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_conjugation_check_parse_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_conjugation', 'expression_cases', 'test_from_string_conjugation_check_parse_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conj alias parses', 'test_from_string_conjugation_check_parse_expr_1', 'conj(1 + 2i)', 'conj(1 + 2i)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conjugate alias canonicalises', 'test_from_string_conjugation_check_parse_expr_1', 'conjugate(1 + 2i)', 'conj(1 + 2i)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('postfix conjugate parses', 'test_from_string_conjugation_check_parse_expr_1', '(1 + 2i)^*', 'conj(1 + 2i)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_conjugation_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_conjugation', 'expression_cases', 'test_from_string_conjugation_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conj alias evaluates', 'test_from_string_conjugation_check_parse_num_2', 'conj(1 + 2i)', '1 - 2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conjugate alias evaluates', 'test_from_string_conjugation_check_parse_num_2', 'conjugate(1 + 2i)', '1 - 2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('postfix conjugate evaluates', 'test_from_string_conjugation_check_parse_num_2', '(1 + 2i)^*', '1 - 2i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_conjugation_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_conjugation', 'expression_cases', 'test_from_string_conjugation_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('symbolic modulus bars parse', 'test_from_string_conjugation_check_parse_expr_3', '|z|', '{ |z| | z = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('whitespace separates implicitly multiplied factors', 'test_from_string_arithmetic_check_parse_num_1', '44 i', '44i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('power binds before following division', 'test_from_string_arithmetic_check_parse_expr_2', '{ x^2/6 | x = NAN }', '{ x²/6 | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unit fractional exponent shorthand remains an exponent', 'test_from_string_arithmetic_check_parse_expr_2', '{ x^1/3 | x = NAN }', '{ x^⅓ | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('parenthesised rational remains an exponent', 'test_from_string_arithmetic_check_parse_expr_2', '{ x^(2/6) | x = NAN }', '{ x^⅓ | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_num_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_num_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('decimal binding stays exact through cancellation-sensitive cube', 'test_from_string_arithmetic_check_parse_num_3', '{ (5x)^3 | x = 128064.000000000120974 }', '262537412640768744007706072064702817249456224221302405921303/1000000000000000000000000000000000000000000');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_simplified_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_simplified_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('parenthesised exponent can itself be powered', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^(ix)² | x = NAN; a = NAN }', '{ a^(2ix) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('ASCII parenthesised exponent can itself be powered', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^(ix)^2 | x = NAN; a = NAN }', '{ a^(2ix) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unparenthesised chained powers are left associative', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^x^2 | x = NAN; a = NAN }', '{ a^(2x) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('explicit exponent power stays inside exponent', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^((ix)^2) | x = NAN; a = NAN }', '{ a^(-x²) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('power of symbolic power folds integer exponent', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ (a^(-x))² | x = NAN; a = NAN }', '{ a^(-2x) | x = ?; a = ? }');
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('literal_zero_positive_power', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '0^2', '0'),
    ('literal_zero_binary_power', 'test_from_string_arithmetic_check_parse_simplified_expr_4', 'pow(0,2)', '0'),
    ('literal_zero_in_polynomial', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '(0^4+1)*k-k', '0'),
    ('bound_zero_remains_symbolic', 'test_from_string_arithmetic_check_parse_simplified_expr_4',
     '{x^2|x=0}', '{ x² | x = 0 }')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_TeX_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_TeX_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('cubrt uses an indexed radical in TeX', 'test_from_string_functions_check_parse_TeX_1', 'cubrt(x)', '\left\{ \sqrt[3]{x} \;\middle|\; x = ? \right\}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('root uses its order as the TeX radical index', 'test_from_string_functions_check_parse_TeX_1', 'root(x, n)', '\left\{ \sqrt[n]{x} \;\middle|\; x = ?; n = ? \right\}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('10! lowers to gamma(11)', 'test_from_string_functions_check_parse_num_2', '{ 10! }', '3628800');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('x! lowers to gamma(x+1)', 'test_from_string_functions_check_parse_num_2', '{ x! | x = 5 }', '120');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('exp(pi*i/2) = i', 'test_from_string_functions_check_parse_num_2', '{ exp(@pi*i/2) }', 'i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('1/pi stays symbolic', 'test_from_string_functions_check_parse_expr_3', '{ 1/pi }', '1/π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pi/2 stays symbolic', 'test_from_string_functions_check_parse_expr_3', '{ pi/2 }', 'π/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('i^i stays symbolic', 'test_from_string_functions_check_parse_expr_3', '{ i^i }', 'i^i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('i^(1+i) parses', 'test_from_string_functions_check_parse_expr_3', '{ i^(1+i) }', 'i^(1 + i)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_simplified_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_simplified_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W0 identity keeps unbound argument', 'test_from_string_functions_check_parse_simplified_expr_4', '{ W_0(x)*exp(W_0(x)) }', '{ x | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W0 identity resolves bound pi', 'test_from_string_functions_check_parse_simplified_expr_4', '{ W_0(x)*exp(W_0(x)) | x = pi }', 'π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W-1 identity resolves bound e', 'test_from_string_functions_check_parse_simplified_expr_4', '{ W-1(x)*exp(W-1(x)) | x = e }', 'e');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_expr_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_expr_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W identity recognises e power', 'test_from_string_functions_check_parse_expr_5', '{ W(2e^2) }', '2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_expr_6', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_expr_6')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('plain Greek aliases use the perfect hash', 'test_from_string_functions_check_parse_expr_6', '{ cos(theta) + i*sin(theta) }', '{ cos(θ) + i·sin(θ) | θ = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_simplified_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_simplified_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('trigamma at positive infinity is zero', 'test_from_string_special_functions_check_parse_simplified_expr_1', 'trigamma(inf)', '0');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('polygamma expression uses standard symbol', 'test_from_string_special_functions_check_parse_expr_2', '{ polygamma(2, x) | x = ? }', '{ ψ⁽²⁾(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('symbolic polygamma order uses the base psi symbol', 'test_from_string_special_functions_check_parse_expr_2', '{ polygamma(n, x) | n = ?; x = ? }', '{ ψ(n, x) | n = ?; x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('base psi symbol round-trips symbolic polygamma', 'test_from_string_special_functions_check_parse_expr_2', '{ ψ(n, x) | n = ?; x = ? }', '{ ψ(n, x) | n = ?; x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W(x) infers x binding', 'test_from_string_special_functions_check_parse_expr_3', '{ W(x) }', '{ W(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W_0(x) infers x binding', 'test_from_string_special_functions_check_parse_expr_3', '{ W_0(x) }', '{ W₀(x) | x = ? }');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('lambert_wn parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ lambert_Wn(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Wn parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ Wn(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W_n parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ W_n(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Wₙ parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ Wₙ(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('cdf alias keeps user spelling', 'test_from_string_special_functions_check_parse_expr_4', '{ cdf(1.96) - cdf(-1.96) }', 'cdf(1.96) - cdf(-1.96)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pdf and logpdf aliases keep user spelling', 'test_from_string_special_functions_check_parse_expr_4', '{ pdf(0) + logpdf(0) }', 'pdf(0) + logpdf(0)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_exact_value_functions_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_exact_value_functions', 'expression_cases', 'test_from_string_exact_value_functions_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factorial(10) = 10!', 'test_from_string_exact_value_functions_check_parse_num_1', '{ factorial(n) | n = 10 }', '3628800');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('fibonacci(50)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ fibonacci(n) | n = 50 }', '12586269025');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('partition(5)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ partition(n) | n = 5 }', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('isqrt(200)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ isqrt(n) | n = 200 }', '14');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('gcd(84, 30)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ gcd(a, b) | a = 84, b = 30 }', '6');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('lcm(-21, 6)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ lcm(a, b) | a = -21, b = 6 }', '42');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('mod(29, 5)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ mod(a, b) | a = 29, b = 5 }', '4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('modinv(3, 11)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ modinv(a, b) | a = 3, b = 11 }', '4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('is_prime(97)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ is_prime(n) | n = 97 }', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('is_prime(221)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ is_prime(n) | n = 221 }', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('next_prime(14)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ next_prime(n) | n = 14 }', '17');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('prev_prime(14)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ prev_prime(n) | n = 14 }', '13');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('AND(13, 11)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ AND(a, b) | a = 13, b = 11 }', '9');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('OR(13, 11)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ OR(a, b) | a = 13, b = 11 }', '15');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('XOR(13, 8)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ XOR(a, b) | a = 13, b = 8 }', '5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('NOT(8)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ NOT(a) | a = 8 }', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('SHL(7, 3)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ SHL(a, n) | a = 7, n = 3 }', '56');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('SHR(56, 3)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ SHR(a, n) | a = 56, n = 3 }', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factors(360)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ a₀³·a₁²·a₂ | ; a₀ = 2, a₁ = 3, a₂ = 5 }', '360');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_prime_factor_products', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_from_string_exact_value_functions', 'expression_cases',
    'test_prime_factor_products')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_360', 'test_prime_factor_products', 'factors(360)', '{ a₀³·a₁²·a₂ | ; a₀ = 2, a₁ = 3, a₂ = 5 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_benchmark_1215_bits', 'test_prime_factor_products', 'factors(547721190191546363398678620411634487909744841314598615206873278092099143398360125227595479954148677571112600596738260011818279723370813377533578833565803706508131569062299976033041302189833215742992653063887339218309467131548996199881113600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)', '{ a₀⁵¹²·a₁²⁵⁶·a₂¹²⁸ | ; a₀ = 2, a₁ = 3, a₂ = 5 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_power_2048_bits', 'test_prime_factor_products', 'factors(2^2048)', '{ a₀²⁰⁴⁸ | ; a₀ = 2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_bound_power_2048_bits', 'test_prime_factor_products', '{ factors(n) | n = 2^2048 }', '{ a₀²⁰⁴⁸ | ; a₀ = 2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_identity', 'test_prime_factor_products', 'factors(1)', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_zero_formal', 'test_prime_factor_products', 'factors(0)', 'factors(0)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_negative_formal', 'test_prime_factor_products', 'factors(-8)', 'factors(-8)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_fraction_formal', 'test_prime_factor_products', 'factors(3/2)', 'factors(³⁄₂)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure const rational', 'test_from_string_number_literals_check_parse_num_1', '{ [half] = 1/2 }', '1/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('rational atom expression', 'test_from_string_number_literals_check_parse_num_1', '{ 1/2 + 1/4 }', '3/4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unicode rational atom expression', 'test_from_string_number_literals_check_parse_num_1', '{ ½ + ¼ }', '3/4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('stacked unicode rational atom', 'test_from_string_number_literals_check_parse_num_1', '{ ³⁵⁵⁄₁₁₃ }', '355/113');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('rational atom expression simplifies display', 'test_from_string_number_literals_check_parse_expr_2', '{ 231/2310 }', '⅒');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('stacked unicode rational atom simplifies display', 'test_from_string_number_literals_check_parse_expr_2', '{ ²³¹⁄₂₃₁₀ }', '⅒');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unparenthesised radical constant round-trips', 'test_from_string_number_literals_check_parse_expr_2', '{ √3/2 }', '√(3)/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unparenthesised radical supports implicit multiplication', 'test_from_string_number_literals_check_parse_expr_2', '{ 4√3/2 }', '2·√(3)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_TeX_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_TeX_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('stacked unicode rational atom renders TeX', 'test_from_string_number_literals_check_parse_TeX_3', '{ ²³¹⁄₂₃₁₀ }', '\frac{1}{10}');
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('negative_imaginary_product_TeX', 'test_from_string_number_literals_check_parse_TeX_3',
     '-i/2*x', '\left\{ -\frac{1}{2}\mkern-2mu i\mkern-2mu x \;\middle|\; x = ? \right\}'),
    ('negative_imaginary_unit_product_TeX', 'test_from_string_number_literals_check_parse_TeX_3',
     '-i*x', '\left\{ -i\mkern-2mu x \;\middle|\; x = ? \right\}')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('NAN is a numeric placeholder, not a variable name', 'test_from_string_number_literals_check_parse_expr_4', '{ NAN }', 'NAN');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('lowercase nan is a numeric placeholder', 'test_from_string_number_literals_check_parse_expr_4', '{ nan }', 'NAN');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('repeated unary signs simplify', 'test_from_string_number_literals_check_parse_expr_4', '{ --2x | x = ? }', '{ 2x | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_num_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_num_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure imaginary coefficient atom', 'test_from_string_number_literals_check_parse_num_5', '{ 3/2i }', '3/2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure const rational complex', 'test_from_string_number_literals_check_parse_num_5', '{ [z] = 1/2 - 3/2i }', '1/2 - 3/2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding rational complex', 'test_from_string_number_literals_check_parse_num_5', '{ z | z = 1/2 - 3/2i }', '1/2 - 3/2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding parenthesized imag coeff', 'test_from_string_number_literals_check_parse_num_5', '{ z | z = 1/2 + (2)i }', '1/2 + 2i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_deriv_check_parse_simplified_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_deriv', 'expression_cases', 'test_from_string_deriv_check_parse_simplified_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dx(...) parses as expression derivative', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dx(exp(x^2)) | x = ? }', '{ 2x·exp(x²) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dx(...) infers only its derivative variable', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dx(exp(x^2)) }', '{ 2x·exp(x²) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dxx(...) parses as repeated expression derivative', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dxx(x^3) | x = ? }', '{ 6x | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dxy(...) parses as mixed expression derivative', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dxy(x^2*y^3) | x = ?, y = ? }', '{ 6xy² | x = ?, y = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Leibniz first derivative uses the native derivative parser', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ d/dx(x^3) }', '{ 3x² | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Leibniz second derivative uses matching orders', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ d^2/dx^2(x^3) }', '{ 6x | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Leibniz mixed derivative matches Dxy', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ d^2/dxy(x^2*y^3) | x = ?, y = ? }', '{ 6xy² | x = ?, y = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_deriv_check_parse_simplified_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_deriv', 'expression_cases', 'test_from_string_deriv_check_parse_simplified_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('D[x^5](...) parses as compact high-order derivative', 'test_from_string_deriv_check_parse_simplified_expr_2', '{ D[x^5](x^7) }', '{ 2520x² | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('D[x^2y^3](...) preserves mixed derivative order', 'test_from_string_deriv_check_parse_simplified_expr_2', '{ D[x^2y^3](x^3*y^4) }', '{ 144xy | x = ?, y = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('canonical superscript derivative input round trips', 'test_from_string_deriv_check_parse_simplified_expr_2', '{ Dx²(x^3) }', '{ 6x | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves symbolic pi/2', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', '{ e^(sin(x)) | x = pi/2 }', '{ e^sin(x) | x = π/2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves symbolic 3/2*pi', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', '{ e^(sin(x)) | x = 3/2*pi }', '{ e^sin(x) | x = ³⁄₂π }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves symbolic pi^2/2', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', '{ x | x = (pi^2)/2 }', 'π²/2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value round-trips Unicode powers', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_2', '{ x | x = (pi²)/2 }', 'π²/2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure numeric expression preserves full mathematical tree', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ phi - 1/2(1+sqrt(5)) }', 'φ - ½·(1 + √(5))');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves full mathematical tree', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ x | x = 1/2(1+sqrt(5)) }', '¹⁄₂·(1 + √(5))');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value round-trips pretty multiply', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ -x + phi | x = ½·(1 + √(5)) }', '{ -x + φ | x = ½·(1 + √(5)) }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value simplifies numeric magnitude and preserves symbolic functions', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ x | x = abs(-3)+floor(pi)+ceil(phi) }', '3 + ⌊π⌋ + ⌈φ⌉');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value accepts unknown marker', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ sinh(x) | x = ? }', '{ sinh(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('user-bound e remains symbolic', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ E - M - e·sin(E) | ; M = pi/1.234, e=0.0167 }', '{ E - M - e·sin(E) | E = ?; M = π/1.234, e = 0.0167 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('constant binding simplifies Lambert e power', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ polygamma(9, x*y*c) | x = 7, y = 1; c = W(2e^2) }', '{ ψ⁽⁹⁾(cxy) | x = 7, y = 1; c = 2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('simplified constants keep bindings', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ ax + yb + zc - 8 | a = NAN, b = NAN, c = NAN; x=1, y = 2, z = 3 }', '{ ax + by + cz - 8 | a = ?, b = ?, c = ?; x = 1, y = 2, z = 3 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('constant bindings accept semicolon separators', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ ax + by + cz - 8 | a = NAN, b = NAN, c = NAN; x = 1; y = 2, z = 3 }', '{ ax + by + cz - 8 | a = ?, b = ?, c = ?; x = 1, y = 2, z = 3 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('const-only binding preserves leading semicolon', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(π·√H) | ;H = 163 }', '{ exp(π·√H) | ; H = 163 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('const-only binding accepts trailing digit subscript', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(π·√(H8)) | ; H8 = 163 }', '{ exp(π·√(H₈)) | ; H₈ = 163 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('minus after factor remains subtraction', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(π·√(H8))-(5x)^3 | H8 = 163, x = NAN }', '{ exp(π·√(H₈)) - (5x)³ | H₈ = 163, x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('absolute-value bars omit inner spaces', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(pi*sqrt(x)) - | exp(pi*sqrt(x)) | | x = NAN }', '{ exp(π·√x) - |exp(π·√x)| | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('symbolic pi quotient cancels powers', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ pi/pi^2 }', 'π/π²');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('generated derivative with NaN bindings simplifies explicitly', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ -y²z²·sin(xyz)·exp(sin(xyz)) + y²z²·cos²(xyz)·exp(sin(xyz)) | y = NAN, z = NAN, x = NAN }', '{ y²z²·exp(sin(xyz))·(cos²(xyz) - sin(xyz)) | y = ?, z = ?, x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('reparsed symbolic pi derivative simplifies explicitly', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ (-2π·exp(π·√x) + 2·π²·√x·exp(π·√x))/(2·√x)/(2·√x)² | x = 163 }', '{ ¼π·exp(π·√x)·(π·√x - 1)/x^³⁄₂ | x = 163 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('nested rational derivative numerator canonicalizes', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ (2*(x^2 + 3*x + 5)*(2*(x + 1)*(2*x + 3) - 9*x - (-x - 1)*(2*x + 3) - 3*x^2 - 15) - 6*(2*x + 3)*((-x - 1)*(x^2 + 3*x + 5) - (2*x + 3)*(3*x - (x + 1)*(2*x + 3) + x^2 + 5)))/(x^2 + 3*x + 5)^4 | x = 1 }', '{ 6·(12x² + 44x - x⁴ - 4x³ + 23)/(x² + 3x + 5)⁴ | x = 1 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('higher rational derivative numerator canonicalizes', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ (24*(x^2 + 3*x + 5)*(6*x - x^3 - 3*x^2 + 11) - 24*(2*x + 3)*(12*x^2 + 44*x - x^4 - 4*x^3 + 23))/(x^2 + 3*x + 5)^5 | x = ? }', '{ 24·(x⁵ + 5x⁴ - 20x³ - 110x² - 115x - 14)/(x² + 3x + 5)⁵ | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_unevaluated_integral_check_parse_simplified_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_unevaluated_integral', 'expression_cases', 'test_from_string_unevaluated_integral_check_parse_simplified_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unevaluated integral explicit bounds simplify structurally', 'test_from_string_unevaluated_integral_check_parse_simplified_expr_1', '{ ∫_1^3 1/t·dt }', '∫^3_1 1/t·dt');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.reciprocal_product_TeX', 'tests/expression/test_expr_to_string.c',
    'tests.test_expr_t_to_string.test_to_string_all.test_to_string_reciprocal_products_TeX',
    'expression_cases', 'reciprocal_product_TeX')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('reciprocal_root_sum_right', 'reciprocal_product_TeX', '(a*cos(t)+b*sin(t))*abs(t)^(-1/2)', '\frac{a\mkern-2mu \cos(t) + b\mkern-2mu \sin(t)}{\sqrt{\left| t \right|}}'),
    ('reciprocal_root_sum_left', 'reciprocal_product_TeX', 'abs(s)^(-1/2)*(p*cos(s)+q*sin(s))', '\frac{p\mkern-2mu \cos(s) + q\mkern-2mu \sin(s)}{\sqrt{\left| s \right|}}'),
    ('reciprocal_cube_root', 'reciprocal_product_TeX', '(p+q)*z^(-1/3)', '\frac{p + q}{\sqrt[3]{z}}'),
    ('reciprocal_square', 'reciprocal_product_TeX', 'v*x^(-2)', '\frac{v}{x^{2}}');

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('reciprocal_root_multiple_factors', 'reciprocal_product_TeX', '2*(a*cos(t)+b*sin(t))*abs(t)^(-1/2)', '}{\sqrt{\left| t \right|}}'),
    ('reciprocal_multiple_powers', 'reciprocal_product_TeX', 'a*x^(-1/2)*y^(-1/3)', '}{\sqrt{x}\mkern-2mu \sqrt[3]{y}}'),
    ('reciprocal_unit_power', 'reciprocal_product_TeX', 'a*x^(-1)', '\frac{a}{x}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('rational_scaled_quotient', 'reciprocal_product_TeX', '(3/5*x)/y', '\frac{3\mkern-2mu x}{5\mkern-2mu y}'),
    ('rational_scaled_root_quotient', 'reciprocal_product_TeX', 'sqrt((1/49)*(a-b)/(x+100))', '\sqrt{\frac{a - b}{49\mkern-2mu \left(x + 100\right)}}');
WITH modes(operation) AS (VALUES('appell_value'),('appell_dx'),('appell_dy'),('appell_primitive'),
    ('appell_reverse'),('appell_roundtrip'),('appell_parse_reject'))
INSERT INTO test_case_runner(module,source,parent,table_name,operation)
SELECT 'expression.' || operation, 'tests/expression/test_expr_appell.c', 'tests.test_expr_appell',
       'expression_cases', operation FROM modes WHERE true
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

WITH families(family,arguments) AS (VALUES
    (2,'1,1,1,1,1,0,0'),(3,'1,1,1,1,2,0,0'),(4,'1,1,1,1,0,0')),
aliases(prefix,suffix) AS (VALUES('appell_F',''),('appellf',''),('F',''),('f',''),('F_',''))
INSERT OR IGNORE INTO expression_cases(name,operation,input,expected,precision)
SELECT 'appell_alias_' || prefix || family, 'appell_value', prefix || family || '(' || arguments || ')',
       '1', 80 FROM families CROSS JOIN aliases;

INSERT OR IGNORE INTO expression_cases(name,operation,input,expected,precision) VALUES
    ('appell_alias_unicode_2','appell_value','F₂(1;1,1;1,1;0,0)','1',80),
    ('appell_alias_unicode_3','appell_value','F₃(1,1;1,1;2;0,0)','1',80),
    ('appell_alias_unicode_4','appell_value','F₄(1,1;1,1;0,0)','1',80),
    ('appell_binding_f2','appell_value','{z|z=appell_F2(1,1,1,1,1,1/16,1/32)}','32/29',80),
    ('appell_binding_f3','appell_value','{z|z=appell_F3(1,1,1,1,2,1/16,1/32)}',
        '-ln((1-1/16)*(1-1/32))/(1/16+1/32-1/512)',80),
    ('appell_binding_f4','appell_value','{z|z=appell_F4(1,1,1,1,1/16,1/32)}',
        '1/sqrt((1-1/16-1/32)^2-4/512)',80),
    ('appell_dx_f2','appell_dx','{F2(1,1,1,1,1,x,y)|x=1/16;y=1/32}','(32/29)^2',80),
    ('appell_dy_f2','appell_dy','{F2(1,1,1,1,1,x,y)|y=1/32;x=1/16}','(32/29)^2',80),
    ('appell_dx_f3','appell_dx','{F3(1,1,1,1,2,x,y)|x=1/16;y=1/32}',
        '((1/16+1/32-1/512)/(1-1/16)+ln((1-1/16)*(1-1/32))*(1-1/32))/(1/16+1/32-1/512)^2',80),
    ('appell_dy_f3','appell_dy','{F3(1,1,1,1,2,x,y)|y=1/32;x=1/16}',
        '((1/16+1/32-1/512)/(1-1/32)+ln((1-1/16)*(1-1/32))*(1-1/16))/(1/16+1/32-1/512)^2',80),
    ('appell_dx_f4','appell_dx','{F4(1,1,1,1,x,y)|x=1/16;y=1/32}',
        '(1-1/16+1/32)/((1-1/16-1/32)^2-4/512)^(3/2)',80),
    ('appell_dy_f4','appell_dy','{F4(1,1,1,1,x,y)|y=1/32;x=1/16}',
        '(1+1/16-1/32)/((1-1/16-1/32)^2-4/512)^(3/2)',80),
    ('appell_primitive_f2','appell_primitive','{F2(2,2,2,3,3,x,y)|x=1/16;y=1/32}','self',80),
    ('appell_primitive_f3','appell_primitive','{F3(2,2,2,2,3,x,y)|x=1/16;y=1/32}','self',80),
    ('appell_primitive_f4','appell_primitive','{F4(2,2,3,3,x,y)|x=1/16;y=1/32}','self',80),
    ('appell_reverse_f2','appell_reverse','{F2(1,1,1,1,1,x,y)|x=1/16;y=1/32}','(32/29)^2',80),
    ('appell_reverse_f3','appell_reverse','{F3(1,1,1,1,2,x,y)|x=1/16;y=1/32}',
        '((1/16+1/32-1/512)/(1-1/16)+ln((1-1/16)*(1-1/32))*(1-1/32))/(1/16+1/32-1/512)^2',80),
    ('appell_reverse_f4','appell_reverse','{F4(1,1,1,1,x,y)|x=1/16;y=1/32}',
        '(1-1/16+1/32)/((1-1/16-1/32)^2-4/512)^(3/2)',80),
    ('appell_roundtrip_f2','appell_roundtrip','{F2(1,1,1,1,1,x,y)|x=1/16;y=1/32}','32/29',80),
    ('appell_roundtrip_f3','appell_roundtrip','{F3(1,1,1,1,2,x,y)|x=1/16;y=1/32}',
        '-ln((1-1/16)*(1-1/32))/(1/16+1/32-1/512)',80),
    ('appell_roundtrip_f4','appell_roundtrip','{F4(1,1,1,1,x,y)|x=1/16;y=1/32}',
        '1/sqrt((1-1/16-1/32)^2-4/512)',80),
    ('appell_sum_f2','appell_value','@Z_k=0^2 F2(1,1,1,1,1,k/64,1/64)','64/63+64/62+64/61',80),
    ('appell_sum_f3','appell_value','@Z_k=0^2 F3(1,1,1,1,2,k/64,0)',
        '1-ln(1-1/64)/(1/64)-ln(1-2/64)/(2/64)',80),
    ('appell_sum_f4','appell_value','@Z_k=0^2 F4(1,1,1,1,k/64,0)','1+64/63+64/62',80),
    ('appell_arity_f2','appell_parse_reject','F2(1,1,1,1,0,0)','0',80),
    ('appell_arity_f3','appell_parse_reject','F3(1,1,1,1,2,0)','0',80),
    ('appell_arity_f4','appell_parse_reject','F4(1,1,1,1,1,0,0)','0',80);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.atan_derivative_sample', 'tests/expression/test_expression.c',
    'tests.test_expr_atan_derivative_sample', 'expression_cases', 'atan_derivative_sample')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('atan_sample_origin', 'atan_derivative_sample',
     '{(digamma(1001+i/x)-digamma(1+i/x)+digamma(1001-i/x)-digamma(1-i/x))/(2*x^2)|x=0}',
     '500500'),
    ('atan_sample_positive', 'atan_derivative_sample',
     '{(digamma(1001+i/x)-digamma(1+i/x)+digamma(1001-i/x)-digamma(1-i/x))/(2*x^2)|x=0.001}',
     '{sum(j,1,1000,j/(1+j^2*x^2))|x=0.001}'),
    ('atan_sample_negative', 'atan_derivative_sample',
     '{(digamma(1001+i/x)-digamma(1+i/x)+digamma(1001-i/x)-digamma(1-i/x))/(2*x^2)|x=-0.001}',
     '{sum(j,1,1000,j/(1+j^2*x^2))|x=-0.001}'),
    ('atan_sample_reordered', 'atan_derivative_sample',
     '{(digamma(i/x+1001)+digamma(1001-i/x)-digamma(1-i/x)-digamma(i/x+1))/(2*x^2)|x=1}',
     '{sum(j,1,1000,j/(1+j^2*x^2))|x=1}'),
    ('atan_sample_affine_origin', 'atan_derivative_sample',
     '{(digamma(26+i/(2*t+1))-digamma(1+i/(2*t+1))+digamma(26-i/(2*t+1))-digamma(1-i/(2*t+1)))/(2*t+1)^2|t=-1/2}',
     '650'),
    ('atan_sample_affine_shift', 'atan_derivative_sample',
     '{(digamma(26+i/(2*t+1))-digamma(1+i/(2*t+1))+digamma(26-i/(2*t+1))-digamma(1-i/(2*t+1)))/(2*t+1)^2|t=0.1}',
     '{sum(j,1,25,2*j/(1+j^2*(2*t+1)^2))|t=0.1}'),
    ('atan_sample_wrong_coefficient', 'atan_derivative_sample',
     '{(digamma(1001+i/x)-digamma(1+i/x)+digamma(1001-i/x)-digamma(1-i/x))/(3*x^2)|x=0}', 'reject'),
    ('atan_sample_missing_conjugate', 'atan_derivative_sample',
     '{(digamma(1001+i/x)-digamma(1+i/x))/(2*x^2)|x=1}', 'reject'),
    ('atan_sample_wrong_shift', 'atan_derivative_sample',
     '{(digamma(1001+i/x)-digamma(2+i/x)+digamma(1001-i/x)-digamma(1-i/x))/(2*x^2)|x=1}', 'reject');

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('atan_sample_complex_offset', 'atan_derivative_sample',
     '{i+(digamma(1001+i/x)-digamma(1+i/x)+digamma(1001-i/x)-digamma(1-i/x))/(2*x^2)|x=1}', 'reject'),
    ('atan_sample_negative_slope', 'atan_derivative_sample',
     '{-(digamma(26+i/(1-2*t))-digamma(1+i/(1-2*t))+digamma(26-i/(1-2*t))-digamma(1-i/(1-2*t)))/(1-2*t)^2|t=1/2}',
     '-650'),
    ('atan_sample_bound_count', 'atan_derivative_sample',
     '{(digamma(n+1+i/x)-digamma(1+i/x)+digamma(n+1-i/x)-digamma(1-i/x))/(2*x^2)|x=0,n=25}',
     '325');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.rational_progression_sampling', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_rational_progression_sampling', 'expression_cases', 'rational_sample')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('rational_sample_fourth_derivative', 'rational_sample', '{Dxxxx(sum(k,1,1000,atan(k*x)))|x=1}', '["x","-1/100","1/100"]'),
    ('rational_sample_affine', 'rational_sample', 'Dtttt(sum(j,1,25,atan(j*(2*t+1))))', '["t","-7/10","-3/10"]'),
    ('rational_sample_negative', 'rational_sample', '-sum(j,1,25,j^3*t/(1+j^2*t^2)^2)', '["t","-2/5","2/5"]'),
    ('rational_sample_fractional_bound', 'rational_sample', 'sum(k,1,3/2,k*x/(1+k^2*x^2)^2)', '["x"]'),
    ('rational_sample_coordinate_bound', 'rational_sample', 'sum(k,1,x,k/(1+k^2*x^2)^2)', '["x"]'),
    ('rational_sample_nonlinear_phase', 'rational_sample', 'sum(k,1,25,k/(1+k^2*x^4)^2)', '["x"]'),
    ('rational_sample_complex_phase', 'rational_sample', 'sum(k,1,25,k/(1+k^2*(i*x)^2)^2)', '["x"]'),
    ('rational_sample_complex_weight', 'rational_sample', 'sum(k,1,25,i*k/(1+k^2*x^2)^2)', '["x"]'),
    ('rational_sample_nonreal_weight_domain', 'rational_sample', 'sum(k,1,25,sqrt(x)*k/(1+k^2*x^2)^2)', '["x"]'),
    ('rational_sample_pole_in_weight', 'rational_sample', 'sum(k,1,25,k/(x-1)/(1+k^2*x^2)^2)', '["x"]')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
