-- Test that big integer literals are parsed correctly without Float64 precision loss.
-- Before this fix, numbers > UInt64 max were parsed as Float64, silently losing precision.

-- Basic type inference for big unsigned integers
SELECT toTypeName(100000000000000000000000);

-- Basic type inference for big negative integers
SELECT toTypeName(-100000000000000000000000);

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

-- Ensure big literals work in array context
SELECT toTypeName([100000000000000000000000, 100000000000000000000001]);

-- Ensure values are preserved exactly
SELECT 100000000000000000000000;
SELECT -100000000000000000000000;

-- Clean up
DROP TABLE test_number_literal;
