-- Unrestricted arithmetic reuses the catalogue's dense 1,024-bit and 512-bit integers and 521-bit prime.
-- Division of integer operands uses their known 128-bit common divisor, retaining an integer result.
-- Rational operands are a/p and b/a; parsing and canonical reduction happen before timing.
WITH operands AS (
    SELECT substr(g.arguments, 1, instr(g.arguments, ',') - 1) AS a,
           trim(substr(g.arguments, instr(g.arguments, ',') + 1)) AS b,
           p.arguments AS p,
           '340282366920938463463374607431768211454' AS common_divisor
    FROM benchmark_operation AS g
    JOIN benchmark_operation AS p ON p.name = 'is_prime'
    WHERE g.name = 'gcd'
), arithmetic(operation, arity, power) AS (
    VALUES ('abs', 1, 0), ('add', 2, 0), ('div', 2, 0), ('inv', 1, 0), ('mul', 2, 0),
           ('neg', 1, 0), ('pow', 2, 1), ('pow_int', 2, 1), ('sqr', 1, 0), ('sub', 2, 0)
), profiles(backend) AS (
    VALUES ('exact-integer'), ('exact-fraction')
), inputs AS (
    SELECT operation, backend, arity, power,
           CASE WHEN backend = 'exact-fraction' THEN a || '/' || p ELSE a END AS first,
           CASE WHEN backend = 'exact-fraction' THEN b || '/' || a
                WHEN operation = 'div' THEN common_divisor ELSE b END AS second
    FROM operands CROSS JOIN arithmetic CROSS JOIN profiles
    WHERE backend = 'exact-fraction' OR operation <> 'inv'
)
INSERT INTO benchmark_workload(operation, backend, arguments)
SELECT operation, backend,
       CASE WHEN operation = 'abs' THEN '-' ELSE '' END || first ||
       CASE WHEN power THEN ', 5' WHEN arity = 2 THEN ', ' || second ELSE '' END
FROM inputs
WHERE true
ON CONFLICT(operation, backend) DO UPDATE SET
    arguments_display = CASE WHEN benchmark_workload.arguments = excluded.arguments
                             THEN benchmark_workload.arguments_display END,
    arguments = excluded.arguments;
