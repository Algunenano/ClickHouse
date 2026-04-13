-- Test that numeric literals are parsed as NumberLiteral to preserve precision.
-- Before this fix, numbers > UInt64 max were parsed as Float64, silently losing precision.
-- Decimal-point literals (3.14) were also parsed as Float64, losing precision for Decimal targets.

-- Big unsigned integer type inference
SELECT toTypeName(100000000000000000000000);

-- Big negative integer type inference
SELECT toTypeName(-100000000000000000000000);

-- Decimal-point literal default type (backward compat: Float64)
SELECT toTypeName(3.14);

-- Precision preservation: these two values differ by 500
SELECT 100000000000000000000500 - 100000000000000000000000;

-- The original bug: BETWEEN with UInt128 values
DROP TABLE IF EXISTS test_number_literal;
CREATE TABLE test_number_literal (action_id UInt128) ENGINE = MergeTree ORDER BY action_id;
INSERT INTO test_number_literal SELECT number + 100000000000000000000000 FROM numbers(1000);

SELECT count() FROM test_number_literal
WHERE action_id BETWEEN 100000000000000000000000 AND 100000000000000000000500;

SELECT count() FROM test_number_literal
WHERE action_id >= 100000000000000000000000 AND action_id <= 100000000000000000000500;

DROP TABLE test_number_literal;

-- Big literals in array context
SELECT toTypeName([100000000000000000000000, 100000000000000000000001]);

-- Value preservation
SELECT 100000000000000000000000;
SELECT -100000000000000000000000;

-- Decimal precision: implicit cast from literal to Decimal column preserves precision
DROP TABLE IF EXISTS test_decimal_literal;
CREATE TABLE test_decimal_literal (d Decimal128(18)) ENGINE = MergeTree ORDER BY d;
INSERT INTO test_decimal_literal VALUES ('1.123456789012345678');
SELECT * FROM test_decimal_literal WHERE d = 1.123456789012345678;
SELECT count() FROM test_decimal_literal WHERE d = 1.123456789012345678;
DROP TABLE test_decimal_literal;

-- String target: big integer literal cast to String
SELECT CAST(100000000000000000000000, 'String');
