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


-- TODO: There functions should all return LowCardinality types
CREATE TABLE lc_numbers (col Array(LowCardinality(UInt64))) engine = Memory AS SELECT [number, number + 2] FROM system.numbers LIMIT 1 OFFSET 2;
CREATE TABLE lc_nullable_numbers (col Array(LowCardinality(Nullable(UInt64)))) engine = Memory AS SELECT [number, number +2, null] FROM system.numbers LIMIT 1 OFFSET 2;

SELECT 'filterLowConst', toTypeName(arrayFilter(x -> x % 2 == 0, [2, 4]::Array(LowCardinality(UInt64))));
SELECT 'filterLowConst', arrayFilter(x -> x % 2 == 0, [2, 4]::Array(LowCardinality(UInt64)));
SELECT 'filterLowCol', toTypeName(arrayFilter(x -> x % 2 == 0, col)) FROM lc_numbers;
SELECT 'filterLowCol', arrayFilter(x -> x % 2 == 0, col) FROM lc_numbers;

SELECT 'sumLowConst', toTypeName(arrayMap(x -> x + 2 == 0, [2, 4]::Array(LowCardinality(UInt64))));
SELECT 'sumLowConst', arrayMap(x -> x + 2, [2, 4]::Array(LowCardinality(UInt64)));
SELECT 'sumLowCol', toTypeName(arrayMap(x -> x + 2 == 0, col)) FROM lc_numbers;
SELECT 'sumLowCol', arrayMap(x -> x + 2, col) FROM lc_numbers;

SELECT 'minLowConst', toTypeName(arrayMin([2, 4]::Array(LowCardinality(UInt64))));
SELECT 'minLowConst', arrayMin([2, 4]::Array(LowCardinality(UInt64)));
SELECT 'minLowCol', toTypeName(arrayMin(col)) FROM lc_numbers;
SELECT 'minLowCol', arrayMin(col) FROM lc_numbers;
