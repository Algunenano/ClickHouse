#include <Common/JemallocJITArena.h>

#if USE_JEMALLOC

#include <Common/Exception.h>
#include <Common/Jemalloc.h>
#include <Common/ProfileEvents.h>
#include <Common/Stopwatch.h>

#include <fmt/format.h>
#include <jemalloc/jemalloc.h>
#include <string>

namespace ProfileEvents
{
    extern const Event MemoryAllocatorPurge;
    extern const Event MemoryAllocatorPurgeTimeMicroseconds;
}

namespace DB
{
namespace ErrorCodes
{
    extern const int CANNOT_ALLOCATE_MEMORY;
}
}

namespace DB::JemallocJITArena
{

namespace
{

unsigned createArena()
{
    unsigned arena_index = 0;
    size_t arena_index_size = sizeof(arena_index);
    int err = je_mallctl("arenas.create", &arena_index, &arena_index_size, nullptr, 0);
    if (err)
        throw DB::Exception(DB::ErrorCodes::CANNOT_ALLOCATE_MEMORY, "JemallocJITArena: Failed to create jemalloc arena, error: {}", err);
    return arena_index;
}

}

unsigned getArenaIndex()
{
    static unsigned index = createArena();
    return index;
}

bool isEnabled()
{
    /// The arena only has callers when JIT is built in. Without USE_EMBEDDED_COMPILER, nothing
    /// allocates here; reporting `enabled` would create a useless empty arena and expose a
    /// permanently-zero metric. Pair the runtime jemalloc check with the compile-time JIT check.
#if USE_EMBEDDED_COMPILER
    return true;
#else
    return false;
#endif
}

void purge()
{
    static Jemalloc::MibCache<unsigned> purge_mib(fmt::format("arena.{}.purge", getArenaIndex()).c_str());

    Stopwatch watch;
    purge_mib.run();
    ProfileEvents::increment(ProfileEvents::MemoryAllocatorPurge);
    ProfileEvents::increment(ProfileEvents::MemoryAllocatorPurgeTimeMicroseconds, watch.elapsedMicroseconds());
}

}

#else

namespace DB::JemallocJITArena
{

unsigned getArenaIndex() { return 0; }
bool isEnabled() { return false; }
void purge() {}

}

#endif
