<!-- markdownlint-disable MD041 -->
### Parameters inherited from `Satellite`

#### Autonomous Operation

Parameters to control [autonomy](../operator_guide/concepts/autonomy.md#satellite-autonomy--roles) in the `_autonomy` section:

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `role` | Role name | Role this satellite should take in the Constellation. Accepted values are `ESSENTIAL`, `DYNAMIC`, `TRANSIENT` and `NONE`. | `DYNAMIC` |
| `max_heartbeat_interval` | Unsigned integer | Maximum interval in seconds between heartbeats to be sent to other Constellation components. The time between heartbeat is adjusted automatically according to the number of nodes in the Constellation but will never exceed this value. | `30` |

#### Conditional Transitions

Parameters to control [conditional transitions](../operator_guide/concepts/autonomy.md#conditional-transitions) in the `_conditions` section:

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `require_initializing_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `initializing` state | - |
| `require_launching_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `launching` state | - |
| `require_landing_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `landing` state | - |
| `require_starting_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `starting` state | - |
| `require_stopping_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `stopping` state | - |
| `transition_timeout` | Unsigned integer | Timeout in seconds to wait for all conditions to be satisfied for conditional transitioning | `30` |
