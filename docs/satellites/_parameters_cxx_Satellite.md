<!-- markdownlint-disable MD041 -->
### Parameters inherited from `Satellite`

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_allow_departure` | Bool | If `true`, regular departures of satellites will not cause an interrupt to SAFE mode | `true` |
| `_heartbeat_interval` | Unsigned integer | Interval in seconds between heartbeats to be sent to other Constellation components | `10` |
| `_role` | Role name | Role this satellite should take in the Constellation. Accepted values are `essential`, `dynamic` and `none` | `dynamic` |
