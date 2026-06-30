#pragma once

#include <Processors/IProcessor.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace DB
{

/// Growable open-addressing UInt64 set (0 handled separately), used once per partition at finalize.
struct RadixU64Set
{
    std::vector<UInt64> slots;
    size_t mask = 0;
    size_t count = 0;
    bool has_zero = false;

    explicit RadixU64Set(size_t cap_pow2) { slots.assign(cap_pow2, 0); mask = cap_pow2 - 1; }

    inline void insert(UInt64 k, UInt64 h)
    {
        if (k == 0) { has_zero = true; return; }
        size_t i = h & mask;
        while (true)
        {
            UInt64 s = slots[i];
            if (s == k) return;
            if (s == 0) { slots[i] = k; ++count; return; }
            i = (i + 1) & mask;
        }
    }
    size_t size() const { return count + (has_zero ? 1 : 0); }
};

/// Chain of const-size blocks: append fills the current block; a full block triggers a new one.
/// No realloc-copy on growth (unlike a single growable array) and skew-safe (unlike a fixed array).
struct RadixBlockList
{
    static constexpr size_t BLOCK = 8192;
    std::vector<std::vector<UInt64>> blocks;

    inline void push(UInt64 k)
    {
        if (blocks.empty() || blocks.back().size() == BLOCK)
        {
            blocks.emplace_back();
            blocks.back().reserve(BLOCK);
        }
        blocks.back().push_back(k);
    }
};

/// PROTOTYPE (buffer + chained const-size blocks): route keys to per-stream, per-partition block
/// chains, then build one hash set per partition (cache-resident, no merge) and sum the sizes.
struct RadixUniqExactData
{
    static constexpr size_t NUM_PARTITIONS = 256;

    explicit RadixUniqExactData(size_t num_streams_)
        : num_streams(num_streams_), buffers(num_streams_)
    {
    }

    const size_t num_streams;
    std::vector<std::array<RadixBlockList, NUM_PARTITIONS>> buffers;
};

using RadixUniqExactDataPtr = std::shared_ptr<RadixUniqExactData>;

class RadixRouteTransform : public IProcessor
{
public:
    RadixRouteTransform(SharedHeader header, RadixUniqExactDataPtr data_, size_t stream_index_, size_t key_pos_);
    String getName() const override { return "RadixRouteTransform"; }
    Status prepare() override;
    void work() override;

private:
    RadixUniqExactDataPtr data;
    const size_t stream_index;
    const size_t key_pos;
    Chunk current_chunk;
    bool has_chunk = false;
};

class RadixFinalizeTransform : public IProcessor
{
public:
    RadixFinalizeTransform(SharedHeader input_header, SharedHeader output_header, RadixUniqExactDataPtr data_, size_t max_threads_);
    String getName() const override { return "RadixFinalizeTransform"; }
    Status prepare() override;
    void work() override;

private:
    RadixUniqExactDataPtr data;
    const size_t max_threads;
    bool computed = false;
    bool pushed = false;
    UInt64 result = 0;
    Chunk result_chunk;
};

}
