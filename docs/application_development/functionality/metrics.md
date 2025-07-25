# Sending Metrics

```{seealso}
For the basic concepts behind telemetry in Constellation and the available metric types check the
[chapter in the operator’s guide](../../operator_guide/concepts/telemetry.md).
```

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Metrics have to be registered before use. For satellite this can be done via:

```c++
register_metric("NAME", "unit", MetricsValue::LAST_VALUE, "description");
```

A metric can then be send using the `STAT` macros:

```c++
STAT("NAME", get_metric_value());
```

```{seealso}
There are also macros available to only send metrics every *nth* iteration or at most every *t* seconds. These macros are
described in detail in the [framework reference](../../framework_reference/cxx/core/metrics.md).
```

Metrics that are evaluated regularly from a lambda can also be registered:

```c++
register_timed_metric("NAME", "unit", MetricType::LAST_VALUE, "description", 10s,
                      [this]() -> std::optional<double> {
    return allowed_to_get_metric() ? std::optional(get_metric_value()) : std::nullopt;
});
```

The lambda can return either the value directly, or an optional, where the empty value means that the metric can currently
not be sent. To check if the satellite is in a given state the `getState()` method can be used, or the following helper:

```c++
register_timed_metric("NAME", "unit", MetricType::LAST_VALUE, "description", 10s,
                      {CSCP::State::ORBIT, CSCP::State::RUN},
                      [this]() { return get_metric_value(); }
);
```

Note that timed metrics can still be triggered manually if desired.

:::
:::{tab-item} Python
:sync: python

There are two approaches to send metrics (e.g. readings from a temperature sensor): one is
via a function decorator and the other is via a scheduling method.

The scheduling method,`schedule_metric`, makes it easy to create metrics
programmatically at run time. This can be particularly useful in cases where e.g. the number of available channels is only
known after connecting to the instrument hardware. The method takes a metric name, the unit, a polling interval, a
metric type and a callable function as arguments. The name of the metric will always be converted to upper-case.

* Metric type: can be `LAST_VALUE`, `ACCUMULATE`, `AVERAGE`, or `RATE`.
* Polling interval: a float value in seconds, indicating how often the metric is transmitted.
* Callable function: Should return a single value (of any type), and take no arguments. If you have a callable that requires
  arguments, consider using `functools.partial` to fill in the necessary information at scheduling time. If the callback
  returns `None`, no metric will be published.

An example call is shown below:

```python
self.schedule_metric("Current", "A", MetricsType.LAST_VALUE, 10, self.device.get_current)
```

In this example, the callable `self.device.get_current` fetches the current from
a power supply, is called every 10 seconds, and the value is sent as Metric with
name `Current`, unit `A` and an indicator for any receiver(s) to only show the
last value.

Similarly, a metric to can be sent via the `schedule_metric` function decorator:

```python
@schedule_metric("A", MetricsType.LAST_VALUE, 10)
def Current(self) -> Any:
    """The current as measured by the power supply."""
    if self.device.can_read_current():
        return self.device.get_current()
    return None
```

In case of the decorator, the name of the metric is taken from the function
name. Again, if `None` is returned, no metric is sent.

In either case, the doc string of the callable or decorated function serves as a
description of the Metric and is published via the CMDP `STAT?` notification
subscription.

When using class methods for retrieving values for the metrics, then the
decorator can be a convenient way of scheduling them. But if metrics need to be created at run time, then the method is a
powerful approach to schedule an arbitrary number of metrics.

```{attention}
Note that any registered Metrics are by default retrieved in any of the Satellite's states, **except** `NEW`, `ERROR` and `initializing` where it is assumed that reliable Metrics cannot be guaranteed.

If you have a Metric that is only valid in fewer than those states, it is advisable to add a check for the current state within the function's body (e.g. `if not self.fsm.current_state_value in [SatelliteState.RUN, SatelliteState.SAFE]: ...`). Simply return `None` in this case.

Should any error occur during the retrieval of a Metric, it will be logged but the Satellite will not be affected otherwise and the Metric will be requested again at the next scheduled interval.
```

:::
::::
