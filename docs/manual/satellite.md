# The Constellation Satellite

The central components of a Constellation network are satellites. A satellite is a program for controlling an instrument and
is built around a finite state machine. It is the only component in a Constellation which partakes in all Constellation
protocols. In the following, the main features of the Constellation Satellite are described.

## The Finite State Machine

The finite state machine (FSM) controls the behavior of the satellite and guarantees that the system is always in a defined state.
The FSM has been designed carefully to strike the balance between robustness, features, and simplicity.
State transitions are either initiated by a controller sending transition commands through CSCP for regular operation, or by
internal state changes in case of errors.
In the following, the different states and their transitions are introduced, starting from regular operations and extending
into the different failure modes.

### Normal Operation - Steady States

In regular operation, i.e. without unexpected incidents in the Constellation, the satellite FSM will transition between four
steady states. Steady here indicates that the satellite will remain in this state until either a transition is initiated by
a controller, or a failure mode is activated. The simplified state diagram for normal operation mode can be drawn like this:

```plantuml
@startuml
hide empty description

State NEW : Satellite started
State INIT : Satellite initialized
State ORBIT : Satellite powered
State RUN : Satellite taking data

NEW -right[#blue,bold]-> INIT : initialize
INIT -right[#blue,bold]-> ORBIT : launch
ORBIT -left[#blue,bold]-> INIT : land
ORBIT -[#blue,bold]-> RUN : start
ORBIT -[#blue,bold]-> ORBIT : reconfigure
RUN -left[#blue,bold]-> ORBIT : stop
@enduml
```

The individual states of the satellite correspond to well-defined states of the attached instrument hardware as well as the
satellite communication with the rest of the Constellation:

* The `NEW` state is the initial state of any satellite.

  It indicates that the satellite has just been started and that no connection with a controller or other satellites in the Constellation has been made.

* The `INIT` state indicates that the satellite has been initialized.

  Initialization comprises the reception of the configuration from a controller and the start of heartbeat publication through
  the CHP protocol. At this point, the satellite as been made aware of other satellites in the Constellation.
  A first connection to the instrument hardware may be made at this point, using the configuration provided by the controller.

* The `ORBIT` state signals that the satellite is ready for data taking.

  When the satellite enters this state, all instrument hardware has been configured, is powered up and is ready for entering a
  measurement run.

* The `RUN` state represents the data acquisition mode of the satellite.

  In this state, the instrument is active and queried for data, the satellite handles measurement data, i.e. obtains data from
  the instrument and sends it across the Constellation, or receives data and stores it to file.

### Operating the Instrument - The `RUN` State

The `RUN` state is special in that this is where the operation of the satellite takes place, data is collected and passed on and statistical metrics are distributed.
Constellation provides two different ways for satellite implementations to interact with the `RUN` state:

* An inheritance of the `run_sequence` function is the simplest method of implementing instrument code. The function is
  called repeatedly by the satellite until a transition out of the RUN state is requested either by a controller or by a failure mode.

  ```plantuml
  @startuml
  hide empty description

  State RUN {
      State start <<entryPoint>>
      State stop <<exitPoint>>
      State c <<choice>>
      State run_sequence
      start -right-> c
      c -[dotted]down-> run_sequence : run
      c -[dotted]right-> stop
      run_sequence -up[dotted]-> c : loop
  }

  @enduml
  ```

* An inheritance of the `run_loop` function provides some more freedom in implementing device code. The method is just called
  once upon entering the `RUN` state, and should exit as soon as a state change is requested either by a controller or by a
  failure mode. In contrast to an inheritance of the `run_sequence` method, this function requires the implemented code to
  implement this behavior.

  ```plantuml
  @startuml
  hide empty description

  State RUN {
      State start <<entryPoint>>
      State stop <<exitPoint>>
      State run_loop
      start -right[dotted]-> run_loop
      run_loop -right[dotted]-> stop
  }

  @enduml
  ```

### Changing States - Transitions

Instrument code of the individual satellites is executed in so-called transitional states. They differ from steady states in
that they are entered by a state transition initiated through CSCP or a failure mode, but exited automatically upon completion
of the action. Such a transition diagram is shown below:

```plantuml
@startuml
hide empty description

State NEW : Satellite started
State initializing #lightblue
State INIT : Satellite initialized

NEW -[#blue,bold]right-> initializing : initialize
initializing -[dotted]right-> INIT
@enduml
```

In this example, the transition "initialize" is triggered by a command sent by the controller. The satellite enters the
"initializing" state and works through the instrument initialization code. Upon success, the satellite automatically
transitions into the "INIT" state and remains there, awaiting further transition commands.

In this scheme, actions controlling and setting up the instrument hardware directly correspond to transitional states. The
following transitional states are defined in the Constellation FSM:

* The `initializing` state
* The `launching` state
* The `landing` state
* The `staring` state
* The `stopping` state

In addition, the optional `reconfiguring` transitional state enables quick configuration updates of satellites in `ORBIT` state
without having to pass through the `INIT` state. A typical example for reconfiguration is a high-voltage power supply unit,
which is slowly ramped up to its target voltage in the `launching` state. Between runs, the applied voltage is supposed to be
changed by a few volts - and instead of the time-consuming operation ramping down via the `landing` transition and ramping up again,
the voltage is ramped directly from its current value to the target value in the `reconfigure` transitional state:

```plantuml
@startuml
hide empty description

State INIT : Satellite initialized
State launching #lightblue
State landing #lightblue
State ORBIT : Satellite orbiting
State reconfiguring #lightblue##[dotted]

INIT -right[#blue,bold]-> launching : launch
launching -down[dotted]-> ORBIT
ORBIT -left[#blue,bold]-> landing : land
landing -up[dotted]-> INIT

ORBIT -[#blue,bold]down-> reconfiguring : reconfigure
reconfiguring -[dotted]up-> ORBIT
@enduml
```

This transition needs to be specifically implemented in individual satellites in order to make this transition available in the FSM.

### Failure Modes & Safe State

Satellites operate autonomously in the Constellation, and no central controller instance is required to run. Controllers are
only required to initiate state transitions out of steady satellite states as described above.
This situation implies that a mechanism should be in place to deal with unexpected events occurring in the operation of a single
satellite, but also within the entire Constellation. For this purpose, the satellite FSM knows two additional steady states:

The `ERROR` state is entered whenever an unexpected event occurs within the instrument control or the data transfer. This
state can only be left by a manual intervention via a controller by resetting the satellite back into its `INIT` state.

The `SAFE` state on the other hand, is entered by the satellite when detecting an issue with *another* satellite in the
Constellation. This awareness of the Constellation status is achieved with the help of CHP. Each satellite in the
Constellation transmits its current state as a heartbeat via the CHP protocol at regular intervals. This allows other
satellites to react if the communicated status contains the `ERROR` state - or if the heartbeat is absent for a defined
period of time.
The `SAFE` state resembles that of an uncrewed spacecraft, where all non-essential systems are shut down and only essential
functions such as communication and attitude control are active. For a Constellation satellite this could encompass powering
down instruments or switching off voltages. Also this state can only be left by a manual intervention via a controller, and
the satellite will transfer back to its `INIT` state.

The main difference between the two failure states is the possible statement about the condition of the respective satellite.
The `SAFE` state is achieved via a controlled shutdown of components and is a well-defined procedure, while the `ERROR` state
is entered, for example, through a lack of control or communication with the instrument and therefore does not allow any
statement to be made about the condition of attached hardware.