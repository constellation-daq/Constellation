<!-- markdownlint-disable MD041 -->
### Parameters inherited from `Satellite`

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_heartbeat_interval` | Unsigned integer | Interval in seconds between heartbeats to be sent to other Constellation components | `10` |
| `_role` | Role name | Role this satellite should take in the Constellation. Accepted values are `ESSENTIAL`, `DYNAMIC`, `TRANSIENT` and `NONE`. | `DYNAMIC` |
