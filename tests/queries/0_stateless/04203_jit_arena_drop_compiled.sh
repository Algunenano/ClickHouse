#!/usr/bin/env bash
# Tags: no-parallel, no-fasttest, use_jemalloc
# no-parallel: this test issues `SYSTEM DROP COMPILED EXPRESSION CACHE`, which is process-wide.
#              Running in parallel with other JIT-using tests would flap their assertions and ours.
# no-fasttest: requires USE_EMBEDDED_COMPILER, which the fast-test image disables.
# use_jemalloc: this test asserts on `jemalloc.jit_arena.*` async metrics, which are only
#               registered when the build has jemalloc.
#
# Test that JIT compilation populates the dedicated jemalloc JIT arena and that
# `SYSTEM DROP COMPILED EXPRESSION CACHE` removes cached compiled functions.

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

# Drop the cache. We assert the cache count goes to zero, but NOT that
# `jemalloc.jit_arena.active_bytes` returns to its pre-compile value, because the bulk of the
# arena footprint is the CHJIT singleton's persistent state (TargetMachine, Subtarget,
# LLVMContext-uniqued types/constants). That state is held alive via `shared_ptr<CHJIT>` by
# every cache entry that compiled against it, so it only goes away once the last entry releases.
# Background system queries that JIT-compile (for system_log inserts, etc.) can land a fresh
# entry between `cache->clear()` and the singleton-slot reset, pinning the old instance for the
# duration of that entry's life. Asserting on arena bytes here would therefore be flaky.
$CLICKHOUSE_CLIENT -q "SYSTEM DROP COMPILED EXPRESSION CACHE"

echo "cache_count_after_drop $($CLICKHOUSE_CLIENT -q "
    SELECT value FROM system.metrics WHERE name = 'CompiledExpressionCacheCount'")"
