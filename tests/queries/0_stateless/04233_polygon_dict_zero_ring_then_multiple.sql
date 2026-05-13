-- Regression: when a single insert block contains a `[[]]` (zero-ring) polygon followed by two or
-- more polygons that each have rings, the second non-empty polygon's points used to be folded into
-- the first non-empty polygon as an inner ring, because `current_polygon` was never advanced past
-- the leading empties so `atLastRingOfPolygon` looked at the empty polygon's ring range.

DROP DICTIONARY IF EXISTS polygon_dict_zero_ring_multi;
DROP TABLE IF EXISTS polygon_dict_zero_ring_multi_src SYNC;

CREATE TABLE polygon_dict_zero_ring_multi_src
(polygon Array(Array(Array(Tuple(Float64, Float64)))), city_id UInt32) ENGINE = Memory;

-- Single INSERT forces all three rows into one block.
INSERT INTO polygon_dict_zero_ring_multi_src VALUES
    ([[]], 100),
    ([[[(0,0),(1,0),(1,1),(0,1),(0,0)]]], 200),
    ([[[(2,0),(3,0),(3,1),(2,1),(2,0)]]], 300);

CREATE DICTIONARY polygon_dict_zero_ring_multi
(polygon Array(Array(Array(Tuple(Float64, Float64)))), city_id UInt32 DEFAULT 0)
PRIMARY KEY polygon
SOURCE(CLICKHOUSE(TABLE 'polygon_dict_zero_ring_multi_src'))
LIFETIME(0)
LAYOUT(POLYGON_INDEX_CELL());

SYSTEM RELOAD DICTIONARY polygon_dict_zero_ring_multi;

-- Expected: 200 (point inside the first unit square at the origin).
SELECT dictGet('polygon_dict_zero_ring_multi', 'city_id', tuple(0.5, 0.5));
-- Expected: 300 (point inside the second unit square at x=2-3). The bug returns 0 because the
-- second polygon was attached to the first as an inner ring, so the point is not covered.
SELECT dictGet('polygon_dict_zero_ring_multi', 'city_id', tuple(2.5, 0.5));

DROP DICTIONARY polygon_dict_zero_ring_multi;
DROP TABLE polygon_dict_zero_ring_multi_src SYNC;

-- Additional shapes to cover related corner cases of the zero-ring / mixed-polygon layout:
--   * multiple leading empties before the first non-empty
--   * empties interleaved with non-empties
--   * an empty polygon inside a multi-polygon row
DROP DICTIONARY IF EXISTS polygon_dict_mixed_empties;
DROP TABLE IF EXISTS polygon_dict_mixed_empties_src SYNC;

CREATE TABLE polygon_dict_mixed_empties_src
(polygon Array(Array(Array(Tuple(Float64, Float64)))), city_id UInt32) ENGINE = Memory;

INSERT INTO polygon_dict_mixed_empties_src VALUES
    ([[]], 100),                                                    -- empty
    ([[]], 110),                                                    -- empty
    ([[[(0,0),(1,0),(1,1),(0,1),(0,0)]]], 200),                     -- unit square
    ([[]], 210),                                                    -- empty
    ([[[(2,0),(3,0),(3,1),(2,1),(2,0)]]], 300),                     -- unit square at x=2
    ([[[(4,0),(5,0),(5,1),(4,1),(4,0)]],                            -- unit square at x=4 ...
      [],                                                           -- ... + empty polygon ...
      [[(6,0),(7,0),(7,1),(6,1),(6,0)]]], 400);                     -- ... + unit square at x=6

CREATE DICTIONARY polygon_dict_mixed_empties
(polygon Array(Array(Array(Tuple(Float64, Float64)))), city_id UInt32 DEFAULT 0)
PRIMARY KEY polygon
SOURCE(CLICKHOUSE(TABLE 'polygon_dict_mixed_empties_src'))
LIFETIME(0)
LAYOUT(POLYGON_INDEX_CELL());

SYSTEM RELOAD DICTIONARY polygon_dict_mixed_empties;

-- Each lookup should map to the city_id of its enclosing polygon's source row.
SELECT dictGet('polygon_dict_mixed_empties', 'city_id', tuple(0.5, 0.5));   -- 200
SELECT dictGet('polygon_dict_mixed_empties', 'city_id', tuple(2.5, 0.5));   -- 300
SELECT dictGet('polygon_dict_mixed_empties', 'city_id', tuple(4.5, 0.5));   -- 400
SELECT dictGet('polygon_dict_mixed_empties', 'city_id', tuple(6.5, 0.5));   -- 400
SELECT dictGet('polygon_dict_mixed_empties', 'city_id', tuple(10.0, 10.0)); -- 0 (default)

DROP DICTIONARY polygon_dict_mixed_empties;
DROP TABLE polygon_dict_mixed_empties_src SYNC;
