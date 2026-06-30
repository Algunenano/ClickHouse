#include <Processors/Transforms/RadixUniqExactTransform.h>

#include <Columns/ColumnsNumber.h>

#include <thread>

namespace DB
{

static inline UInt64 murmur64(UInt64 x)
{
    x ^= x >> 32; x *= 0xd6e8feb86659fd93ULL;
    x ^= x >> 32; x *= 0xd6e8feb86659fd93ULL;
    x ^= x >> 32;
    return x;
}

static inline size_t nextPow2(size_t n) { size_t c = 1; while (c < n) c <<= 1; return c; }

RadixRouteTransform::RadixRouteTransform(SharedHeader header, RadixUniqExactDataPtr data_, size_t stream_index_, size_t key_pos_)
    : IProcessor(InputPorts{header}, OutputPorts{header})
    , data(std::move(data_)), stream_index(stream_index_), key_pos(key_pos_)
{
}

IProcessor::Status RadixRouteTransform::prepare()
{
    auto & input = inputs.front();
    auto & output = outputs.front();

    if (output.isFinished()) { input.close(); return Status::Finished; }
    if (has_chunk) return Status::Ready;

    if (input.isFinished()) { output.finish(); return Status::Finished; }

    input.setNeeded();
    if (input.hasData()) { current_chunk = input.pull(true); has_chunk = true; return Status::Ready; }
    return Status::NeedData;
}

void RadixRouteTransform::work()
{
    /// Key is a 64-bit integer; read raw bits, signedness is irrelevant for distinctness.
    const auto & column = *current_chunk.getColumns()[key_pos];
    const size_t n = column.size();
    const UInt64 * vals = reinterpret_cast<const UInt64 *>(column.getRawData().data());
    auto & bufs = data->buffers[stream_index];
    for (size_t i = 0; i < n; ++i)
    {
        UInt64 k = vals[i];
        bufs[murmur64(k) >> (64 - 8)].push(k);
    }
    has_chunk = false;
    current_chunk.clear();
}

RadixFinalizeTransform::RadixFinalizeTransform(SharedHeader input_header, SharedHeader output_header, RadixUniqExactDataPtr data_, size_t max_threads_)
    : IProcessor(InputPorts(data_->num_streams, input_header), OutputPorts{output_header})
    , data(std::move(data_)), max_threads(std::max<size_t>(max_threads_, 1))
{
}

IProcessor::Status RadixFinalizeTransform::prepare()
{
    auto & output = outputs.front();

    if (output.isFinished())
    {
        for (auto & in : inputs) in.close();
        return Status::Finished;
    }

    if (computed)
    {
        if (!pushed)
        {
            if (!output.canPush()) return Status::PortFull;
            output.push(std::move(result_chunk));
            pushed = true;
        }
        output.finish();
        return Status::Finished;
    }

    bool all_finished = true;
    for (auto & in : inputs)
    {
        if (!in.isFinished())
        {
            in.setNeeded();
            if (in.hasData()) in.pull(true); /// routes never push real data; drain just in case
            all_finished = false;
        }
    }
    if (!all_finished) return Status::NeedData;
    return Status::Ready;
}

void RadixFinalizeTransform::work()
{
    std::atomic<int> next{0};
    std::atomic<size_t> total{0};
    const size_t threads = std::min(max_threads, RadixUniqExactData::NUM_PARTITIONS);

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (size_t t = 0; t < threads; ++t)
    {
        pool.emplace_back([&]
        {
            int p;
            while ((p = next.fetch_add(1)) < static_cast<int>(RadixUniqExactData::NUM_PARTITIONS))
            {
                size_t rows = 0;
                for (size_t s = 0; s < data->num_streams; ++s)
                    for (const auto & block : data->buffers[s][p].blocks)
                        rows += block.size();
                if (rows == 0) continue;

                RadixU64Set set(nextPow2(static_cast<size_t>(rows / 0.4) + 16));
                for (size_t s = 0; s < data->num_streams; ++s)
                    for (const auto & block : data->buffers[s][p].blocks)
                        for (UInt64 k : block)
                            set.insert(k, murmur64(k));
                total.fetch_add(set.size());
            }
        });
    }
    for (auto & th : pool) th.join();

    result = total.load();

    auto col = ColumnUInt64::create();
    col->insert(result);
    result_chunk.setColumns(Columns{std::move(col)}, 1);
    computed = true;
}

}
