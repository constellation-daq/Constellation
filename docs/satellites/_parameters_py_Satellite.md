<!-- markdownlint-disable MD041 -->
### Parameters inherited from `Satellite`

#### Autonomous Operation

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_role` | Role name | Role this satellite should take in the Constellation. Accepted values are `ESSENTIAL`, `DYNAMIC`, `TRANSIENT` and `NONE`. | `DYNAMIC` |

#### Transition Orchestration

Parameters to control [autonomous transition orchestration](../operator_guide/concepts/autonomy.md) of satellites:

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_conditional_transition_timeout` | Unsigned integer | Timeout in seconds to wait for all conditions to be satisfied for conditional transitioning | `30` |
| `_require_initializing_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `initializing` state | - |
| `_require_launching_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `launching` state | - |
| `_require_landing_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `landing` state | - |
| `_require_starting_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `starting` state | - |
| `_require_stopping_after` | List of strings | List of canonical names of remote satellites for conditional transitioning in `stopping` state | - |
