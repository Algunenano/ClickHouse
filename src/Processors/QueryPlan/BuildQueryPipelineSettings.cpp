#include <Processors/QueryPlan/BuildQueryPipelineSettings.h>
#include <Core/Settings.h>
#include <Interpreters/Context.h>

namespace DB
{

namespace Setting
{
    extern SettingsBool query_plan_merge_filters;
    extern SettingsMaxThreads max_threads;
    extern SettingsUInt64 aggregation_memory_efficient_merge_threads;
}

BuildQueryPipelineSettings::BuildQueryPipelineSettings(ContextPtr from)
{
    const auto & settings = from->getSettingsRef();

    actions_settings = ExpressionActionsSettings(settings, CompileExpressions::yes);
    process_list_element = from->getProcessListElement();
    progress_callback = from->getProgressCallback();

    max_threads = from->getSettingsRef()[Setting::max_threads];
    aggregation_memory_efficient_merge_threads = from->getSettingsRef()[Setting::aggregation_memory_efficient_merge_threads];

    /// Setting query_plan_merge_filters is enabled by default.
    /// But it can brake short-circuit without splitting filter step into smaller steps.
    /// So, enable and disable this optimizations together.
    enable_multiple_filters_transforms_for_and_chain = settings[Setting::query_plan_merge_filters];
}

}
