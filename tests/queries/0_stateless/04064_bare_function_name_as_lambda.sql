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
