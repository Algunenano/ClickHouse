#pragma once

#include "config.h"

namespace DB::JemallocJITArena
{

/// Returns the jemalloc arena index dedicated to LLVM/JIT allocations.
/// Creates the arena on first call (thread-safe via Meyers singleton).
/// Returns 0 (meaning "use default arena selection") if jemalloc is not available.
///
/// LLVM allocates via global `operator new`, so we cannot route its allocations
/// through a custom allocator template parameter the way we do for `JemallocCacheAllocator`.
/// Instead, callers temporarily switch the calling thread's preferred arena (via
/// `mallctl("thread.arena", ...)`) using `ScopedJemallocThreadArena` for the duration
/// of any block that calls into LLVM. See `ScopedJemallocThreadArena` in `Common/Jemalloc.h`.
unsigned getArenaIndex();

/// Whether the dedicated JIT arena is available (jemalloc compiled in).
bool isEnabled();

/// Purge dirty pages only in the JIT arena, returning memory to the OS.
/// No-op if jemalloc is not available.
void purge();

}
