# Sending Metrics

```{seealso}
The basic concepts behind metrics and telemetry in Constellation are described in the
[*Telemetry* chapter of the operator guide](../../operator_guide/concepts/telemetry.md).
```

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Metrics have to be registered before use. For satellites this can be done via:

```cpp
register_metric("NAME", "unit", "description");
```

This metric can then be sent using the `STAT` macros:

```cpp
STAT("NAME", get_metric_value());
```

```{seealso}
There are also macros available to only send metrics every *nth* iteration or at most every *t* seconds. These macros are
described in detail in the [framework reference](../../framework_reference/cxx/core/metrics.md).
```

Metrics that are evaluated regularly from a lambda can also be registered:

```cpp
register_timed_metric("NAME", "unit", "description", 10s,
                      [this]() -> std::optional<double> {
    return allowed_to_get_metric() ? std::optional(get_metric_value()) : std::nullopt;
});
```

The lambda can return either the value directly, or an optional, where the empty value means that the metric can currently
not be sent. To check if the satellite is in a given state the `getState()` method can be used, or the following helper:

```cpp
register_timed_metric("NAME", "unit", "description", 10s,
                      {CSCP::State::ORBIT, CSCP::State::RUN},
                      [this]() { return get_metric_value(); }
);
```

Note that timed metrics can still be triggered manually if desired.

:::
:::{tab-item} Python
:sync: python

Metrics have to be registered before use. For satellites this can be done via:

```python
self.register_metric("NAME", "unit", "description")
```

This metric can then be sent using the `stat` method:

```python
self.stat("NAME", self.get_metric_value());
```

Metrics that are evaluated regularly from a function can also be registered:

```python
self.register_scheduled_metric("NAME", "unit", "description", interval, self.get_metric_value)
```

The registered callback returns `None` the metric is not sent.

Alternative, a metric can also be registered via the `schedule_metric` function decorator:

```python
@schedule_metric("A", 10)
def current(self) -> float | None:
    """The current as measured by the power supply."""
    if self.device.can_read_current():
        return self.device.get_current()
    return None
```

In case of the decorator, the name of the metric is taken from the function name and the description from the doc string.
Also in this case if the function returns `None` the metric is not sent.

```{attention}
Note that any registered Metrics are by default retrieved in any of the Satellite's states, **except** `NEW`, `ERROR` and `initializing` where it is assumed that reliable Metrics cannot be guaranteed.

If you have a Metric that is only valid in fewer than those states, it is advisable to add a check for the current state within the function's body (e.g. `if not self.fsm.state in [SatelliteState.RUN, SatelliteState.SAFE]: ...`). Simply return `None` in this case.

Should any error occur during the retrieval of a Metric, it will be logged but the Satellite will not be affected otherwise and the Metric will be requested again at the next scheduled interval.
```

:::
::::
