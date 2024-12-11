# Telemetry

Constellation comes with a builtin system for gathering telemetry to provide online monitoring during data taking.
A satellite can send register and send metrics for this purpose, which consist of a name, a unit and a metric type.

## Metric Types

Four types of metrics can be defined, that define how the metric should be handled on receiving a new value:

- `LAST_VALUE`: display the last value
- `ACCUMULATE`: display the sum of all received values
- `AVERAGE`: display an average of the received values
- `RATE`: display the rate of the value

```{note}
Currently, the `Influx` satellite cannot add this information to the database nor handles it locally. Using `LAST_VALUE` and
handling accumulation, averages and rates locally is currently recommended.
```

## Sending Metrics

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Metrics have to be registered before use. For satellite this can be done via:

```c++
register_metric("NAME", "unit", MetricsValue::LAST_VALUE);
```

A metric can then be send using the `STAT` macros:

```c++
STAT("NAME", get_metric_value());
```

There are also macros available to only send metrics every nth iteration or at most every t seconds. The macros are described
in detail in the [developer guide](../../framework_reference/cxx/core/metrics).

Metrics that are evaluated regularly from a lambda can also be registered:

```c++
register_timed_metric("NAME", "unit", MetricsValue::LAST_VALUE, 10s,
                      [this]() -> std::optional<double> {
    return allowed_to_get_metric() ? std::optional(get_metric_value()) : std::nullopt;
});
```

The lambda can return either the value directly, or an optional, where the empty value means that the metric can currently
not be sent. To check if the satellite is in a given state the `getState()` method can be used, or the following helper:

```c++
register_timed_metric("NAME", "unit", MetricsValue::LAST_VALUE, 10s,
                      {CSCP::State::ORBIT, CSCP::State::RUN},
                      [this]() { return get_metric_value(); }
);
```

Note that timed metrics can still be triggered manually if desired.

:::
:::{tab-item} Python
:sync: python

TODO

:::
::::
