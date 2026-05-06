-- Regression: a `[[]]` (zero-ring) polygon followed in the same insert block by a non-empty polygon
-- used to fold the second polygon's points into the empty entry, so a `dictGet` against a point
-- inside the second polygon returned the *first* row's attribute value.

DROP DICTIONARY IF EXISTS polygon_dict_zero_ring;
DROP TABLE IF EXISTS polygon_dict_zero_ring_src SYNC;

CREATE TABLE polygon_dict_zero_ring_src
(polygon Array(Array(Array(Tuple(Float64, Float64)))), city_id UInt32) ENGINE = Memory;

-- Single INSERT forces both rows into one block, exercising the multi-row path.
INSERT INTO polygon_dict_zero_ring_src VALUES ([[]], 100), ([[[(0,0),(1,0),(1,1),(0,1),(0,0)]]], 200);

CREATE DICTIONARY polygon_dict_zero_ring
(polygon Array(Array(Array(Tuple(Float64, Float64)))), city_id UInt32 DEFAULT 0)
PRIMARY KEY polygon
SOURCE(CLICKHOUSE(TABLE 'polygon_dict_zero_ring_src'))
LIFETIME(0)
LAYOUT(POLYGON_INDEX_CELL());

SYSTEM RELOAD DICTIONARY polygon_dict_zero_ring;

-- Expected 200: (0.5, 0.5) is inside the unit square (row with city_id=200).
SELECT dictGet('polygon_dict_zero_ring', 'city_id', tuple(0.5, 0.5));

DROP DICTIONARY polygon_dict_zero_ring;
DROP TABLE polygon_dict_zero_ring_src SYNC;
