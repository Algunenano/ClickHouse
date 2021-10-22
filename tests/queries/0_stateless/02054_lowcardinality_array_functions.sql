-- https://github.com/ClickHouse/ClickHouse/issues/30357
CREATE TABLE strings (col Array(String)) engine = Memory AS SELECT ['hi'] as col;
CREATE TABLE lc_strings (col Array(LowCardinality(String))) engine = Memory AS SELECT ['hi'] as col;
CREATE TABLE lc_nullable_strings (col Array(LowCardinality(Nullable(String)))) engine = Memory AS SELECT ['hi', null] as col;

SELECT 'toLowConst', toTypeName(arrayMap(y -> toLowCardinality(y), ['hi']));
SELECT 'toLowConst', arrayMap(y -> toLowCardinality(y), ['hi']);
SELECT 'toLowCol', toTypeName(arrayMap(y -> toLowCardinality(y), col)) FROM strings;
SELECT 'toLowCol', arrayMap(y -> toLowCardinality(y), col) FROM strings;

SELECT 'toLowToNullConst', toTypeName(arrayMap(y -> toLowCardinality(toNullable(y)), ['hi']));
SELECT 'toLowToNullConst', arrayMap(y -> toLowCardinality(toNullable(y)), ['hi']);
SELECT 'toLowToNullCol', toTypeName(arrayMap(y -> toLowCardinality(toNullable(y)), col)) FROM strings;
SELECT 'toLowToNullCol', arrayMap(y -> toLowCardinality(toNullable(y)), col) FROM strings;

SELECT 'fromLowConst', toTypeName(arrayMap(y -> y, ['hi']::Array(LowCardinality(String))));
SELECT 'fromLowConst', arrayMap(y -> y, ['hi']::Array(LowCardinality(String)));
SELECT 'fromLowCol', toTypeName(arrayMap(y -> y, col)) from lc_strings;
SELECT 'fromLowCol', arrayMap(y -> y, col) from lc_strings;

SELECT 'fromLowNullConst', toTypeName(arrayMap(y -> y, ['hi', NULL]::Array(LowCardinality(Nullable(String)))));
SELECT 'fromLowNullConst', arrayMap(y -> y, ['hi', NULL]::Array(LowCardinality(Nullable(String))));
SELECT 'fromLowNullCol', toTypeName(arrayMap(y -> y, col)) FROM lc_nullable_strings;
SELECT 'fromLowNullCol', arrayMap(y -> y, col) FROM lc_nullable_strings;

SELECT 'concatFromLowConst', toTypeName(arrayMap(y -> concat(y, ' there'), ['hi']::Array(LowCardinality(String))));
SELECT 'concatFromLowConst', arrayMap(y -> concat(y, ' there'), ['hi']::Array(LowCardinality(String)));
SELECT 'concatFromLowCol', toTypeName(arrayMap(y -> concat(y, ' there'), col)) FROM lc_strings;
SELECT 'concatFromLowCol', arrayMap(y -> concat(y, ' there'), col) FROM lc_strings;

SELECT 'concatFromLowNullConst', toTypeName(arrayMap(y -> concat(y, ' there'), ['hi', null]::Array(LowCardinality(Nullable(String)))));
SELECT 'concatFromLowNullConst', arrayMap(y -> concat(y, ' there'), ['hi', null]::Array(LowCardinality(Nullable(String))));
SELECT 'concatFromLowNullCol', toTypeName(arrayMap(y -> concat(y, ' there'), col)) FROM lc_nullable_strings;
SELECT 'concatFromLowNullCol', arrayMap(y -> concat(y, ' there'), col) FROM lc_nullable_strings;


-- TODO: There functions should all return LowCardinality types (or Array(LowCardinality(type))
-- TODO: Address Expression for function arrayFilter must return UInt8, found Nullable(UInt8) {serverError 43}
CREATE TABLE lc_numbers (col Array(LowCardinality(UInt64))) engine = Memory AS SELECT [number, number + 2] FROM system.numbers LIMIT 1 OFFSET 2;
CREATE TABLE lc_nullable_numbers (col Array(LowCardinality(Nullable(UInt64)))) engine = Memory AS SELECT [number, number +2, null] FROM system.numbers LIMIT 1 OFFSET 2;

SELECT 'filterLowConst', toTypeName(arrayFilter(x -> x % 2 == 0, [2, 4]::Array(LowCardinality(UInt64))));
SELECT 'filterLowConst', arrayFilter(x -> x % 2 == 0, [2, 4]::Array(LowCardinality(UInt64)));
SELECT 'filterLowCol', toTypeName(arrayFilter(x -> x % 2 == 0, col)) FROM lc_numbers;
SELECT 'filterLowCol', arrayFilter(x -> x % 2 == 0, col) FROM lc_numbers;

SELECT 'filterLowNullConst', toTypeName(arrayFilter(x -> x % 2 == 0, [2, 4, null]::Array(LowCardinality(Nullable(UInt64))))); -- {serverError 43}
SELECT 'filterLowNullConst', arrayFilter(x -> x % 2 == 0, [2, 4, null]::Array(LowCardinality(Nullable(UInt64)))); -- {serverError 43}
SELECT 'filterLowNullCol', toTypeName(arrayFilter(x -> x % 2 == 0, col)) FROM lc_nullable_numbers; -- {serverError 43}
SELECT 'filterLowNullCol', arrayFilter(x -> x % 2 == 0, col) FROM lc_nullable_numbers; -- {serverError 43}

SELECT 'sumLowConst', toTypeName(arraySum(x -> x + 2 == 0, [2, 4]::Array(LowCardinality(UInt64))));
SELECT 'sumLowConst', arraySum(x -> x + 2, [2, 4]::Array(LowCardinality(UInt64)));
SELECT 'sumLowCol', toTypeName(arraySum(x -> x + 2 == 0, col)) FROM lc_numbers;
SELECT 'sumLowCol', arraySum(x -> x + 2, col) FROM lc_numbers;
SELECT 'sumLowConst', toTypeName(arraySum([2, 4]::Array(LowCardinality(UInt64))));
SELECT 'sumLowConst', arraySum([2, 4]::Array(LowCardinality(UInt64)));
SELECT 'sumLowCol', toTypeName(arraySum(col)) FROM lc_numbers;
SELECT 'sumLowCol', arraySum(col) FROM lc_numbers;

SELECT 'sumLowNullConst', toTypeName(arraySum(x -> x + 2 == 0, [2, 4, null]::Array(LowCardinality(Nullable(UInt64))))); -- {serverError 43}
SELECT 'sumLowNullConst', arraySum(x -> x + 2, [2, 4, null]::Array(LowCardinality(Nullable(UInt64)))); -- {serverError 43}
SELECT 'sumLowNullCol', toTypeName(arraySum(x -> x + 2 == 0, col)) FROM lc_nullable_numbers; -- {serverError 43}
SELECT 'sumLowNullCol', arraySum(x -> x + 2, col) FROM lc_nullable_numbers; -- {serverError 43}
SELECT 'sumLowNullConst', toTypeName(arraySum([2, 4, null]::Array(LowCardinality(Nullable(UInt64))))); -- {serverError 43}
SELECT 'sumLowNullConst', arraySum([2, 4]::Array(LowCardinality(Nullable(UInt64)))); -- {serverError 43}
SELECT 'sumLowNullCol', toTypeName(arraySum(col)) FROM lc_nullable_numbers; -- {serverError 43}
SELECT 'sumLowNullCol', arraySum(col) FROM lc_nullable_numbers; -- {serverError 43}

SELECT 'reduceLowConst', toTypeName(arrayReduce('sum', [2, 4]::Array(LowCardinality(UInt64))));
SELECT 'reduceLowConst', arrayReduce('sum', [2, 4]::Array(LowCardinality(UInt64)));
SELECT 'reduceLowCol', toTypeName(arrayReduce('sum', col)) FROM lc_numbers;
SELECT 'reduceLowCol', arrayReduce('sum', col) FROM lc_numbers;

SELECT 'reduceLowNullConst', toTypeName(arrayReduce('sum', [2, 4, null]::Array(LowCardinality(Nullable(UInt64)))));
SELECT 'reduceLowNullConst', arrayReduce('sum', [2, 4, null]::Array(LowCardinality(Nullable(UInt64))));
SELECT 'reduceLowNullCol', toTypeName(arrayReduce('sum', col)) FROM lc_nullable_numbers;
SELECT 'reduceLowNullCol', arrayReduce('sum', col) FROM lc_nullable_numbers;
