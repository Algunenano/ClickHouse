#pragma once

#include <DataStreams/IProfilingBlockInputStream.h>
#include <Parsers/IAST.h>
#include <Core/QualifiedTableName.h>
#include <Interpreters/Context.h>
#include <Core/QueryProcessingStage.h>

#include <common/logger_useful.h>

#include <memory>
#include <mutex>

namespace DB
{
namespace ClusterProxy
{

class ShardWithLocalReplicaBlockInputStream : public IProfilingBlockInputStream
{
public:
    ShardWithLocalReplicaBlockInputStream(
            String query_, ASTPtr query_ast_, QualifiedTableName main_table_, Context context_,
            QueryProcessingStage::Enum to_stage_)
        : query_ast(std::move(query_ast_)), main_table(std::move(main_table_)), context(std::move(context_))
        , to_stage(to_stage_)
    {
    }

    void readPrefix() override;

    void cancel() override;

protected:

    Block readImpl() override;

    String getName() const override { return "ShardWithLocalReplica"; }

    String getID() const override
    {
        std::stringstream res;
        res << "ShardWithLocalReplica(" << this << ")";
        return res.str();
    }

private:
    Logger * log = &Logger::get("ShardWithLocalReplicaBlockInputStream");

    String query;
    ASTPtr query_ast;
    QualifiedTableName main_table;
    Context context;
    QueryProcessingStage::Enum to_stage;

    // ConnectionPoolWithFailoverPtr connection_pool;

    std::mutex cancel_mutex;
    std::unique_ptr<IProfilingBlockInputStream> impl;
};

}
}
