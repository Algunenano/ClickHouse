-- https://github.com/ClickHouse/ClickHouse/issues/101540
-- JIT-compiled `intExp2` must clamp shifts `>= 64` to `UINT64_MAX`, matching the non-JIT path.

SELECT 'non-jit';
SELECT intExp2(toInt32(number + 63)) FROM numbers(5) ORDER BY number SETTINGS compile_expressions = 0;

SELECT 'jit';
SELECT intExp2(toInt32(number + 63)) FROM numbers(5) ORDER BY number SETTINGS compile_expressions = 1, min_count_to_compile_expression = 0;
