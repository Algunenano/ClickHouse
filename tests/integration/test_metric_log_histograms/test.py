import pytest

from helpers.cluster import ClickHouseCluster

cluster = ClickHouseCluster(__file__)

node_off = cluster.add_instance(
    "node_off",
    user_configs=["config/flatten_nested_off.xml"],
    stay_alive=True,
)
node_on = cluster.add_instance(
    "node_on",
    user_configs=["config/flatten_nested_on.xml"],
    stay_alive=True,
)


@pytest.fixture(scope="module")
def start_cluster():
    try:
        cluster.start()
        yield cluster
    finally:
        cluster.shutdown()


@pytest.mark.parametrize("node", ["node_off", "node_on"])
def test_histograms_column_is_flattened(start_cluster, node):
    instance = cluster.instances[node]
    instance.query("SYSTEM FLUSH LOGS metric_log")

    columns = instance.query(
        """
        SELECT name, type
        FROM system.columns
        WHERE database = 'system' AND table = 'metric_log' AND name LIKE 'histograms%'
        ORDER BY name
        """
    ).strip()

    expected = (
        "histograms.count\tArray(UInt64)\n"
        "histograms.histogram\tArray(Map(Float64, UInt64))\n"
        "histograms.labels\tArray(Map(LowCardinality(String), LowCardinality(String)))\n"
        "histograms.metric\tArray(LowCardinality(String))\n"
        "histograms.sum\tArray(Float64)"
    )
    assert columns == expected

    zero_count_rows = instance.query(
        """
        SELECT countIf(arrayExists(c -> c = 0, histograms.count))
        FROM system.metric_log
        WHERE event_time >= now() - 600
        """
    ).strip()
    assert zero_count_rows == "0"
