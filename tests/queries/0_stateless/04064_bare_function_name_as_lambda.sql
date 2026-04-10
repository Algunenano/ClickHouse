-- Test passing bare function names to higher-order functions instead of lambdas.
-- https://github.com/ClickHouse/ClickHouse/issues/63498

-- Basic: arrayMap with a function name
SELECT arrayMap(negate, [1, 2, 3]);
SELECT arrayMap(toString, [1, 2, 3]);
SELECT arrayMap(toUInt64, [1.1, 2.2, 3.3]);
SELECT arrayMap(length, ['hello', 'world', '!']);

-- arrayFilter with a function name
SELECT arrayFilter(isNotNull, [1, NULL, 3, NULL, 5]);

-- arrayExists / arrayAll
SELECT arrayExists(isNull, [1, NULL, 3]);
SELECT arrayAll(isNotNull, [1, 2, 3]);

-- Multiple array arguments (binary function as lambda)
SELECT arrayMap(plus, [1, 2, 3], [10, 20, 30]);
SELECT arrayMap(multiply, [1, 2, 3], [10, 20, 30]);

-- arrayFold with a bare function name (accumulator + element)
SELECT arrayFold(plus, [1, 2, 3, 4, 5], toUInt64(0));

-- Nested higher-order functions
SELECT arrayMap(toString, arrayMap(negate, [1, 2, 3]));

-- arrayPartialSort with a bare function name (has a fixed `limit` parameter)
SELECT arrayPartialSort(negate, 2, [5, 9, 1, 3]);

-- arraySort with a bare function name
SELECT arraySort(negate, [3, 1, 4, 1, 5]);

-- Backward compatibility: column name takes priority over function name
SELECT arrayMap(x, [1, 2, 3]) FROM (SELECT 1 AS x); -- { serverError ILLEGAL_TYPE_OF_ARGUMENT }

-- Ensure non-higher-order functions don't get the transformation
-- (the default `getLambdaArgumentTypesImpl` throws, so the probe correctly rejects them)
SELECT plus(1, 2);

-- Tuple-destructuring: bare function name is NOT applied when the inner function's
-- fixed arity doesn't match the probed arity (Array(Nothing) can't detect tuples).
-- The user should write an explicit lambda: arrayMap((x, y) -> plus(x, y), [...])
SELECT arrayMap(plus, [(1, 10), (2, 20), (3, 30)]); -- { serverError UNKNOWN_IDENTIFIER }

-- Non-HOF function with a function name as argument: should not be rewritten
SELECT length(toString(123));

-- SQL UDF: unary
CREATE FUNCTION test_04064_double AS x -> x * 2;
SELECT arrayMap(test_04064_double, [1, 2, 3]);
DROP FUNCTION test_04064_double;

-- SQL UDF: binary
CREATE FUNCTION test_04064_add AS (x, y) -> x + y;
SELECT arrayMap(test_04064_add, [1, 2, 3], [10, 20, 30]);
DROP FUNCTION test_04064_add;
