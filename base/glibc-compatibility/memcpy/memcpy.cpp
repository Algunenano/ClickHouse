#include "memcpy.h"

#define SZ_USE_SKYLAKE 1
#define SZ_USE_HASWELL 1
#include "/mnt/ch/ClickHouse/contrib/StringZilla/include/stringzilla/memory.h"

__attribute__((target("sse,sse2,sse3,ssse3,sse4.2,popcnt,avx,avx2,avx512f,avx512bw,avx512vl,avx512vbmi,avx512vbmi2")))
void * memcpyImpl(void * __restrict dst_, const void * __restrict src_, size_t size)
{
    void * ret = dst_;
    sz_copy_haswell(static_cast<sz_ptr_t>(dst_), static_cast<sz_cptr_t>(src_), size);
    return ret;
}

__attribute__((no_sanitize("coverage")))
extern "C" void * memcpy(void * __restrict dst, const void * __restrict src, size_t size)
{
    if (size < 65536)
        return inline_memcpy(dst, src, size);
    return memcpyImpl(dst, src, size);
}
