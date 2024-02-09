# The Constellation Satellite


## The Finite State Machine

- needs robust but keep simplicity in mind
- not too many states & transitions
- describe each state, its supposed state for the attached hardware
- provide examples (HV power supply, ramping)
- mention heartbeating, safe mode

- importance?

### States, State Changes & Expectations

- NEW
- -> loading <- unloading
- INIT
- -> launching <- landing
- ORBIT
- -> starting <- stopping <-> reconfiguring
- RUN
- SAFE
- -> recover (to INIT)
- ERROR
- -> initialize (to INIT)

```plantuml
@startuml
State NEW : Satellite started,\nno configuration received,\nawaiting connection
State Anomaly {
State ERROR : System powered down,\nmanual intervention required
State SAFE: Satellite in safe mode\nsystem powered down
}
State Operation {
State INIT : connection initialized,\nconfiguration received
State ORBIT : hardware configured\nsystem powered\nready for data acquisition
State RUN : Satellite taking data
}

'Entry/Exit'
[*] --> NEW
NEW --> [*]
NEW --> INIT : initialize
NEW -[#red,dashed]-> ERROR
ORBIT --> INIT : land
INIT --> ORBIT : launch
INIT -[#red,dashed]-> ERROR
INIT -> [*]
ORBIT -[#blue]-> ORBIT : reconfigure
ORBIT -[#red,dashed]-> ERROR
RUN --> ORBIT : stop
ORBIT --> RUN : start
ORBIT -[#red]-> SAFE : interrupt
RUN -[#red,dashed]-> ERROR
RUN -[#red]-> SAFE : interrupt
SAFE -[#indigo,bold]-> INIT : recover
SAFE -[#red,dashed]-> ERROR
SAFE --> [*]
ERROR -[#indigo,bold]-> INIT : initialize
ERROR --> [*]
@enduml
```


## Commands

- commands comprise fsm transitions and more
- getters to get additional information on satellite
- fsm states: send CSCP message with type `REQUEST` and command string `transit::<state>` (e.g. transition to initialized state: `REQUEST "transit::init`)
- obtaining additional information via `REQUEST` and command string `get::<cmd>`
- a list of available commands shall be returned with `REQUEST "get::commands`
