#include <Interpreters/ClusterProxy/DescribeStreamFactory.h>
#include <Interpreters/InterpreterDescribeQuery.h>
#include <DataStreams/MaterializingBlockInputStream.h>
#include <DataStreams/BlockExtraInfoInputStream.h>
#include <DataStreams/RemoteBlockInputStream.h>

namespace DB
{

namespace
{

BlockExtraInfo toBlockExtraInfo(const Cluster::Address & address)
{
    BlockExtraInfo block_extra_info;
    block_extra_info.host = address.host_name;
    block_extra_info.resolved_address = address.resolved_address.toString();
    block_extra_info.port = address.port;
    block_extra_info.user = address.user;
    block_extra_info.is_valid = true;
    return block_extra_info;
}

}

namespace ClusterProxy
{

BlockInputStreamPtr DescribeStreamFactory::create(
        const Cluster::ShardInfo & shard_info,
        const String & query, const ASTPtr & query_ast, const Context & context,
        const ThrottlerPtr & throttler)
{
    if (shard_info.isLocal())
    {
        InterpreterDescribeQuery interpreter{query_ast, context};
        BlockInputStreamPtr stream = interpreter.execute().in;

        /** Materialization is needed, since from remote servers the constants come materialized.
         * If you do not do this, different types (Const and non-Const) columns will be produced in different threads,
         * And this is not allowed, since all code is based on the assumption that in the block stream all types are the same.
         */
        BlockInputStreamPtr materialized_stream = std::make_shared<MaterializingBlockInputStream>(stream);
        return std::make_shared<BlockExtraInfoInputStream>(materialized_stream, toBlockExtraInfo(shard_info.local_addresses[0]));
    }
    else
    {
        auto stream = std::make_shared<RemoteBlockInputStream>(
                shard_info.pool, query, &context.getSettingsRef(), context, throttler);
        stream->setPoolMode(PoolMode::GET_ALL);
        stream->appendExtraInfo();
        return stream;
    }
}

}
}
