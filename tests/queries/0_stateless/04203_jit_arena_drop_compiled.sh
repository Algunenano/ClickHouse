#!/usr/bin/env bash
# Tags: no-parallel, no-fasttest, use_jemalloc
# no-parallel: this test issues `SYSTEM DROP COMPILED EXPRESSION CACHE`, which is process-wide.
#              Running in parallel with other JIT-using tests would flap their assertions and ours.
# no-fasttest: requires USE_EMBEDDED_COMPILER, which the fast-test image disables.
# use_jemalloc: this test asserts on `jemalloc.jit_arena.*` async metrics, which are only
#               registered when the build has jemalloc.
#
# Test that JIT compilation populates the dedicated jemalloc JIT arena and the compiled-expression
# cache. The drop itself is exercised at the end but its post-conditions are not asserted; see the
# comment on the drop call below for why.

CUR_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CUR_DIR"/../shell_config.sh

# Force-evict any pre-existing compiled-expression cache content so the count
# we measure is solely from the queries we run below. (Other JIT-using tests
# must not run in parallel with this one — see no-parallel tag.)
$CLICKHOUSE_CLIENT -q "SYSTEM DROP COMPILED EXPRESSION CACHE"

# Trigger a JIT compile so the JIT arena is created and the cache is non-empty.
$CLICKHOUSE_CLIENT -q "
SELECT sum(number * 7), avg(number / 3.0), max(number * 11)
FROM numbers(1000)
SETTINGS compile_aggregate_expressions = 1, compile_expressions = 1,
         min_count_to_compile_aggregate_expression = 0, min_count_to_compile_expression = 0
FORMAT Null
"

$CLICKHOUSE_CLIENT -q "SYSTEM RELOAD ASYNCHRONOUS METRICS"

# After the compile: the JIT arena must hold some bytes, and the cache must have at least one entry.
echo "arena_active_after_compile $($CLICKHOUSE_CLIENT -q "
    SELECT toUInt8(value > 0) FROM system.asynchronous_metrics
    WHERE metric = 'jemalloc.jit_arena.active_bytes'")"

echo "cache_count_after_compile $($CLICKHOUSE_CLIENT -q "
    SELECT toUInt8(value > 0) FROM system.metrics
    WHERE name = 'CompiledExpressionCacheCount'")"

# Drop the cache. We do NOT assert here that `CompiledExpressionCacheCount` returns to zero,
# nor that `jemalloc.jit_arena.active_bytes` returns to its pre-compile value:
#  - The arena footprint is dominated by the CHJIT singleton's persistent state (`TargetMachine`,
#    `Subtarget`, `LLVMContext`-uniqued types/constants), held alive via `shared_ptr<CHJIT>` by
#    every cache entry that compiled against it. A concurrent in-flight compile that lands a
#    fresh holder between `cache->clear()` and the singleton-slot reset will pin the old
#    instance for that entry's life.
#  - The cache count is similarly racy and the race is pre-existing, not introduced by this PR.
#    The static `min_count_to_compile_*` counters in `Aggregator.cpp` / `ExpressionJIT.cpp` are
#    not reset on drop, so any aggregation whose description has crossed the threshold before
#    the drop will JIT-compile and re-populate the cache on first reuse — including from
#    server-internal queries running between our drop and our metric read.
# We still issue the drop here so that the JIT arena gets purged before the test exits, which
# keeps the test environment tidy for subsequent runs.
$CLICKHOUSE_CLIENT -q "SYSTEM DROP COMPILED EXPRESSION CACHE"
