#include <Interpreters/ClusterProxy/ShardWithLocalReplicaBlockInputStream.h>
#include <Interpreters/InterpreterSelectQuery.h>
#include <DataStreams/MaterializingBlockInputStream.h>

namespace DB
{
namespace ClusterProxy
{

void ShardWithLocalReplicaBlockInputStream::readPrefix()
{
    InterpreterSelectQuery interpreter{query_ast, context, to_stage};
    BlockInputStreamPtr stream = interpreter.execute().in;

    /// Materialization is needed, since from remote servers the constants come materialized.
    /// If you do not do this, different types (Const and non-Const) columns will be produced in different threads,
    /// And this is not allowed, since all code is based on the assumption that in the block stream all types are the same.
    auto materialized_stream = std::make_unique<MaterializingBlockInputStream>(stream);

    std::lock_guard<std::mutex> lock(cancel_mutex);

    if (isCancelled())
        return;

    impl = std::move(materialized_stream);

    if (progress_callback)
        impl->setProgressCallback(progress_callback);
    if (process_list_elem)
        impl->setProcessListElement(process_list_elem);

    std::stringstream log_str;
    log_str << "Executing locally. Query pipeline:\n";
    impl->dumpTree(log_str);
    LOG_DEBUG(log, log_str.str());
}

void ShardWithLocalReplicaBlockInputStream::cancel()
{
    std::lock_guard<std::mutex> lock(cancel_mutex);

    IProfilingBlockInputStream::cancel();
    if (impl)
        impl->cancel();
}

Block ShardWithLocalReplicaBlockInputStream::readImpl()
{
    Block res;
    if (isCancelled())
        return res;
    return impl->read();
}

}
}
