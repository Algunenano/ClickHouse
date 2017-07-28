#include <Interpreters/ClusterProxy/SelectStreamFactory.h>
#include <Interpreters/ClusterProxy/ShardWithLocalReplicaBlockInputStream.h>
#include <DataStreams/RemoteBlockInputStream.h>

namespace DB
{

namespace
{

constexpr PoolMode pool_mode = PoolMode::GET_MANY;

}

namespace ClusterProxy
{

SelectStreamFactory::SelectStreamFactory(
        QueryProcessingStage::Enum processed_stage_,
        QualifiedTableName main_table_,
        const Tables & external_tables_)
    : processed_stage{processed_stage_}
    , main_table(std::move(main_table_))
    , external_tables{external_tables_}
{
}

BlockInputStreamPtr SelectStreamFactory::createLocal(const ASTPtr & query_ast, const Context & context, const Cluster::Address & address)
{
    return std::make_shared<ShardWithLocalReplicaBlockInputStream>(query_ast, main_table, context, processed_stage);
}

BlockInputStreamPtr SelectStreamFactory::createRemote(
        const ConnectionPoolWithFailoverPtr & pool, const std::string & query,
        const Settings & settings, ThrottlerPtr throttler, const Context & context)
{
    auto stream = std::make_shared<RemoteBlockInputStream>(pool, query, &settings, context, throttler, external_tables, processed_stage);
    stream->setPoolMode(pool_mode);
    stream->setMainTable(main_table);
    return stream;
}

BlockInputStreamPtr SelectStreamFactory::createRemote(
        ConnectionPoolWithFailoverPtrs && pools, const std::string & query,
        const Settings & settings, ThrottlerPtr throttler, const Context & context)
{
    auto stream = std::make_shared<RemoteBlockInputStream>(std::move(pools), query, &settings, context, throttler, external_tables, processed_stage);
    stream->setPoolMode(pool_mode);
    stream->setMainTable(main_table);
    return stream;
}

PoolMode SelectStreamFactory::getPoolMode() const
{
    return pool_mode;
}

}

}
