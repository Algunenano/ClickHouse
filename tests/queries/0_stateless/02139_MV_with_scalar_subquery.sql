-- https://github.com/ClickHouse/ClickHouse/issues/9587#issuecomment-944431385

CREATE TABLE source (query Int32, a Int32) ENGINE=MergeTree() ORDER BY tuple();
CREATE TABLE source_null AS source ENGINE=Null;
CREATE TABLE dest_a (query Int32, count UInt32, min Int32, max Int32, count_subquery Int32, min_subquery Int32, max_subquery Int32) ENGINE=MergeTree() ORDER BY tuple();

-- CREATE MATERIALIZED VIEW mv_null TO source_null AS SELECT * FROM source;
CREATE MATERIALIZED VIEW mv_a to dest_a AS
SELECT
     query,
    (SELECT count() FROM (SELECT * FROM system.numbers LIMIT 1000) _a) AS count,
    (SELECT min(a) FROM source) AS min,
    (SELECT max(a)  FROM source) AS max,
    (SELECT count() FROM source COMPLETE) AS count_subquery,
    (SELECT min(a) FROM source COMPLETE) AS min_subquery,
    (SELECT max(a) FROM source COMPLETE) AS max_subquery
FROM source
--WHERE a > (SELECT max(query) FROM source COMPLETE)
GROUP BY query, count_subquery, min_subquery, max_subquery;


INSERT INTO source SELECT 1, number FROM numbers(2000) SETTINGS min_insert_block_size_rows=1500, max_insert_block_size=1500;
INSERT INTO source SELECT 2, number FROM numbers(2000, 2000) SETTINGS min_insert_block_size_rows=1500, max_insert_block_size=1500;

SELECT count() FROM source;
SELECT count() FROM dest_a;
SELECT * from dest_a ORDER BY query, count DESC;
