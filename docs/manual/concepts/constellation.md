# Constellation Concepts

This section of the manual provides an overview of the concepts and design principles of the Constellation framework.

Constellation is a network-distributed supervisory control and data acquisition framework (SCADA) geared towards flexibility
and easy integration of new sensors, detectors and other devices. The components of the framework run on a single computer or
multiple machines across a local IP-based network, and allow to distribute commands, record data and collect monitoring and
logging information from its different constituents.

## Naming Things

In software, "naming things" is often among the most difficult tasks as names both need to be "speaking" (describe what they
refer to and what it does) and easy to remember (and type).

The name "Constellation" is borrowed from space flight, where satellite constellations operate with relatively large autonomy, transmitting
data between them, and communicating with a ground control center which issues commands and polices the entire system.
Consequently, vocabulary from space flight appears at different places in the framework and - hopefully - helps in clarifying
functionality and intention. Examples are 'satellite', 'payload', 'safe mode', or 'launch'.

Other vocabulary used throughout the framework and this manual originate from data acquisition systems in nuclear and particle
physics:

* A "run" designates a self-contained measurement or series of measurements. In Constellation, this corresponds to one transition of the
  finite state machine into the 'RUN' state and out of it.
* ...

## Design Approach & Architecture Goals

* make use of modern frameworks for message queuing
* implement a solid finite state machine.
* ...


## Components of a Constellation

The Constellation framework knows three different types of components; Satellites, controllers and listeners. Each of them
have a different purpose and can or cannot partake in interactions. The components are described in the subsequent sections.

### Satellite

- has finite state machine
- listens to transition commands from controller(s)
- broadcasts its current state via heartbeat channel
- goes into the `safe state` if heartbeat from other satellite goes missing
- possibility to ignore missing heartbeats from other satellites via their importance from configuration list

### Controller

- is stateless
- sends commands to satellite fsms to initiate transitions (remote procedure calls)
- can go offline without affecting constellation run
- only a controller can reset the safe state of satellites
- distributes the configuration & importance list

### Listener

- is stateless
- passive component of the constellation, can only consume information, receive info
- examples could be Grafana dashboard, logger display, alarm recipient?


## The Finite State Machine

- needs robust but keep simplicity in mind
- not too many states & transitions
- describe each state, its supposed state for the attached hardware
- provide examples (HV power supply, ramping)
- mention heartbeating, safe mode

- importance?

### States

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
