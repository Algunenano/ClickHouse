-- Test that numeric literals are parsed as NumberLiteral to preserve precision.
-- Before this fix, numbers > UInt64 max were parsed as Float64, silently losing precision.
-- Decimal-point literals were also parsed as Float64, losing precision for Decimal targets.

--
-- 1. Type inference for big integers
--

-- Positive big integers
SELECT toTypeName(100000000000000000000000);
SELECT toTypeName(-100000000000000000000000);

-- Values near UInt64 max boundary (18446744073709551615)
SELECT toTypeName(18446744073709551615);  -- max UInt64, should stay UInt64
SELECT toTypeName(18446744073709551616);  -- max UInt64 + 1, should be UInt128

-- Values near Int64 min boundary (-9223372036854775808)
SELECT toTypeName(-9223372036854775808);   -- min Int64, should stay Int64
SELECT toTypeName(-9223372036854775809);   -- min Int64 - 1, should be Int128

-- Decimal-point literal default type (backward compat)
SELECT toTypeName(3.14);

--
-- 2. Precision preservation for big integer arithmetic
--

-- Adjacent values: difference should be exactly 1
SELECT 100000000000000000000001 - 100000000000000000000000;

-- Larger difference
SELECT 100000000000000000000500 - 100000000000000000000000;

-- Negative big integers
SELECT -100000000000000000000500 + 100000000000000000000000;

-- Near UInt64 max boundary
SELECT 18446744073709551616 - 18446744073709551615;

--
-- 3. BETWEEN with UInt128 column (the original bug from #74312)
--

DROP TABLE IF EXISTS test_number_literal;
CREATE TABLE test_number_literal (action_id UInt128) ENGINE = MergeTree ORDER BY action_id;
INSERT INTO test_number_literal SELECT number + 100000000000000000000000 FROM numbers(1000);

SELECT count() FROM test_number_literal
WHERE action_id BETWEEN 100000000000000000000000 AND 100000000000000000000500;

SELECT count() FROM test_number_literal
WHERE action_id >= 100000000000000000000000 AND action_id <= 100000000000000000000500;

DROP TABLE test_number_literal;

--
-- 4. Big literals in various contexts
--

-- Array
SELECT toTypeName([100000000000000000000000, 100000000000000000000001]);

-- Value preservation (positive and negative)
SELECT 100000000000000000000000;
SELECT -100000000000000000000000;

-- Equality on big literals
SELECT 100000000000000000000123 = 100000000000000000000123;
SELECT 100000000000000000000123 = 100000000000000000000124;

-- Negative big literal equality
SELECT -100000000000000000000123 = -100000000000000000000123;
SELECT -100000000000000000000123 = -100000000000000000000124;

--
-- 5. Decimal precision: deferred parsing avoids Float64 intermediate
--

DROP TABLE IF EXISTS test_decimal_literal;
CREATE TABLE test_decimal_literal (d Decimal128(18)) ENGINE = MergeTree ORDER BY d;
INSERT INTO test_decimal_literal VALUES ('1.123456789012345678');

-- Exact match: literal parsed directly as Decimal128
SELECT * FROM test_decimal_literal WHERE d = 1.123456789012345678;

-- Non-match: last digit differs (was false positive via Float64)
SELECT count() FROM test_decimal_literal WHERE d = 1.123456789012345679;

DROP TABLE test_decimal_literal;

--
-- 6. String target: big integer literal cast to String
--

SELECT CAST(100000000000000000000000, 'String');
SELECT CAST(-100000000000000000000000, 'String');

--
-- 7. Mixed arithmetic: big literal + small integer
--

SELECT 100000000000000000000000 + 1;
SELECT 100000000000000000000000 - 1;
SELECT -100000000000000000000000 + 1;
SELECT -100000000000000000000000 - 1;

--
-- 8. Comparison between NumberLiteral and Int128 column
--

DROP TABLE IF EXISTS test_int128;
CREATE TABLE test_int128 (v Int128) ENGINE = MergeTree ORDER BY v;
INSERT INTO test_int128 VALUES (-100000000000000000000000);
INSERT INTO test_int128 VALUES (100000000000000000000000);

SELECT count() FROM test_int128 WHERE v = -100000000000000000000000;
SELECT count() FROM test_int128 WHERE v = 100000000000000000000000;
SELECT count() FROM test_int128 WHERE v > -100000000000000000000001 AND v < 100000000000000000000001;

DROP TABLE test_int128;
