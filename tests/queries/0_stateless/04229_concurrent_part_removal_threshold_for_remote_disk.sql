-- Smoke test for the new MergeTree setting `concurrent_part_removal_threshold_for_remote_disk`.
-- The behavioral path (parallel removal triggered earlier on a remote disk) is exercised by
-- existing CI jobs that run on object storage backends. Here we just verify the setting is
-- defined, has the expected default, and can be applied to a table.

-- Default value
SELECT name, value, type, default
FROM system.merge_tree_settings
WHERE name = 'concurrent_part_removal_threshold_for_remote_disk';

-- Set on a table and verify CREATE/DROP work end-to-end
DROP TABLE IF EXISTS t_04229;

CREATE TABLE t_04229 (x UInt64)
ENGINE = MergeTree
ORDER BY x
SETTINGS concurrent_part_removal_threshold_for_remote_disk = 8;

INSERT INTO t_04229 SELECT number FROM numbers(10);

-- Setting is visible in system.tables
SELECT extract(create_table_query, 'concurrent_part_removal_threshold_for_remote_disk\\s*=\\s*\\d+')
FROM system.tables
WHERE database = currentDatabase() AND name = 't_04229';

DROP TABLE t_04229;
