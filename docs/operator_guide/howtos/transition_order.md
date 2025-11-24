# Configuring a Transition Order

This how-to guide walks through steps required to configure a startup order of satellites in a Constellation and provides
example configurations for different situations.
Conditional transitions are used to ensure that a transition is completed only after specified satellites have reached
required states.
These conditions are expressed through special configuration keys `_require_<transitional state>_after`.

```{seealso}
The concept of conditional transitions is described in detail in the
[Autonomy Section](../concepts/autonomy.md#conditional-transitions)
```

## Tips & Caveats

* *Acyclic dependencies* -  configurations in which satellites mutually depend on one another for the same transition are
  rejected by the controller. When bypassing the controller checks, e.g. by configuring satellites individually, this will
  lead to a deadlock situation. The transitions will time out eventually and the satellites transition to their {bdg-secondary}`ERROR` states.
* *Missing satellites* - When a remote satellite with a pending condition is not part of the Constellation, the waiting satellite will transition to its {bdg-secondary}`ERROR` state.
* *Full canonical names* - transition conditions function correctly only when the full canonical names of the respective satellites are provided.
* *Waiting only* - conditional transitions only allow waiting for other states, i.e. the configuration keys need to be
set on the *downstream* satellite in the transition order. If for example satellite `Sputnik.First` should start *before*
satellite `Sputnik.Last`, the condition can only be configured with `Sputnik.Last` as

  ```toml
  [Sputnik.Last]
  _require_starting_after = ["Sputnik.First"]
  ```

## Configuring Conditional Transitions

1. **Dependency planning**

   A dependency graph is defined by determining which satellites must have completed particular transitions before others are permitted to proceed.

2. **Creating the Configuration**

   Each dependency is translated into a condition and placed under the corresponding satellite configuration.
   Each key accepts a list of satellite identifiers.
   Satellites may not depend on themselves.

3. **Configuration validation, Configuring the Satellites**

   The configuration is checked by the controller to ensure the absence of circular dependencies.
   Each satellite is initialized with its corresponding dependency definitions from the configuration distributed by the
   controller.

4. **Transition execution**

   When a transition is requested, the respective satellite enters its transitional state such as {bdg-secondary}`starting`.
   While its dependencies remain unsatisfied, the satellite remains in that state.
   Once all required satellites have reached the corresponding states, the transition is completed automatically.

5. **Observing Issues**

   The state transitions should be monitored through the controller or a logging tool to enable understanding of issues such
   as timeouts during the waiting phase of the conditions.

## Example  Configurations

The following sections provide a few examples for typical situations. The satellites used herein are
[`Sputnik` example satellites](../../satellites/Sputnik) which provide the possibility of delaying the
{bdg-secondary}`launching` state via the `launch_delay` parameter.

### Simple "Launch After Main" Dependency

```toml
[Sputnik.Main]
launch_delay = 5

[Sputnik.Worker1]
_require_launching_after = ["Sputnik.Main"]

[Sputnik.Worker2]
_require_launching_after = ["Sputnik.Main"]
```

**Observed effect:** The satellites `Sputnik.Worker1` and `Sputnik.Worker2` enter the {bdg-secondary}`launching` state and remain
there until `Sputnik.Main` has cleared its delay and has progressed to the {bdg-secondary}`ORBIT` state. The worker satellites then
follow suit.

### Multi-Transition Dependencies

```toml
[Sputnik.A]
_require_initializing_after = ["Sputnik.B"]
_require_launching_after = ["Sputnik.B", "Sputnik.C"]
_require_starting_after = ["Sputnik.B"]

[Sputnik.B]
# No dependencies

[Sputnik.C]
# No dependencies
launch_delay = 3
```

**Observed effect:** Initialization of satellite `Sputnik.A` is allowed only after `Sputnik.B` has completed initialization.
Since `Sputnik.B` progresses through this state without delay, the transition of `Sputnik.A` happens swiftly.
Launching of `Sputnik.A` is completed only after `Sputnik.C` has reached the {bdg-secondary}`ORBIT` state.
Starting of `Sputnik.A` is completed only after `Sputnik.B` has reached the {bdg-secondary}`RUN` state.

### Successive Shutdown

```toml
[Sputnik.Main]
# No shutdown dependencies

[Sputnik.Worker1]
_require_stopping_after = ["Sputnik.Main"]
_require_landing_after = ["Sputnik.Main"]

[Sputnik.Worker2]
_require_stopping_after = ["Sputnik.Worker1"]
_require_landing_after = ["Sputnik.Worker1"]
```

**Observed effect:** Both in {bdg-secondary}`stopping` and {bdg-secondary}`landing`, the order of `Sputnik.Main`, then `Sputnik.Worker1` and finally `Sputnik.Worker2` is obeyed.
