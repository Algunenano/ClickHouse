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

-- arrayPartialSort with a bare function name (has a fixed `limit` parameter).
-- Only the first `limit` elements are guaranteed to be sorted; take just those
-- to keep the reference deterministic.
SELECT arraySlice(arrayPartialSort(negate, 2, [5, 9, 1, 3]), 1, 2);

-- arraySort with a bare function name
SELECT arraySort(negate, [3, 1, 4, 1, 5]);

-- Backward compatibility: column name takes priority over function name
SELECT arrayMap(x, [1, 2, 3]) FROM (SELECT 1 AS x); -- { serverError ILLEGAL_TYPE_OF_ARGUMENT }

-- Ensure non-higher-order functions don't get the transformation.
-- `plus` is not a higher-order function, so `isHigherOrderFunction` is false and
-- the rewrite is skipped. `plus(1, 2)` evaluates normally to 3.
SELECT plus(1, 2);

-- Tuple-destructuring: passing `plus` (fixed arity 2) to `arrayMap` with a single
-- array of 2-element tuples is rewritten to `arrayMap((x0, x1) -> plus(x0, x1), ...)`,
-- which `arrayMap` destructures across the tuple elements.
SELECT arrayMap(plus, [(1, 10), (2, 20), (3, 30)]);

-- A non-HOF parent rejects bare function names even if they would otherwise be valid.
-- `plus` is not a higher-order function, so `plus(negate, 1)` is not rewritten and
-- fails because `negate` cannot be resolved as a column/alias.
SELECT plus(negate, 1); -- { serverError UNKNOWN_IDENTIFIER }

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

-- Executable UDF: `test_function` is configured via `tests/config/test_function.xml`
-- and sums two UInt64 arguments. This confirms that the rewrite reaches
-- `UserDefinedExecutableFunctionFactory` and not only `FunctionFactory`/SQL UDFs.
SELECT arrayMap(test_function, [toUInt64(1), toUInt64(2), toUInt64(3)], [toUInt64(10), toUInt64(20), toUInt64(30)]);
