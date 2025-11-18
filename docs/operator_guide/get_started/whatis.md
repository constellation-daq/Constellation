# What is Constellation?

Constellation is a flexible, network-distributed control and data acquisition framework that provides a reliable,
maintainable, and easy-to-use system that eases the implementation of new instruments.

It is built atop established open-source libraries for network communication ([ZMQ](https://zeromq.org/)) and data
serialization ([MsgPack](https://msgpack.org/)), which provide a robust foundation and significantly reduce the required
development effort. Unlike similar frameworks, Constellation does not rely on a central control server but is designed as a
decentralized network whose components operate autonomously. The well-documented and clear interface for instrument control
enables rapid integration of new devices and allows scientists to connect new instruments with minimal added effort and with
the choice between an implementation in C++ or Python. The framework is designed with flexibility in mind and targets
applications ranging from laboratory test stands up to small and mid-sized experiments with several dozens of connected
instruments and large data volumes.

## Components

The Constellation framework knows three different types of components, namely *satellites*, *controllers* and *listeners*.
Each of them has a different purpose and partakes in different communications.

### Satellites

These are the main constituents of a Constellation.
They implement the code controlling an attached instrument, realizing data receivers, and any other component that should
follow the Constellation operation synchronously.
At the core of a satellite is its [FSM](../concepts/satellite.md#the-finite-state-machine) which governs the state of the
satellite and the attached instrument. A detailed description of the satellite is provided in the
[Concepts section](../concepts/satellite.md).

### Controllers

This component represents the main user interface to a Constellation.
Controllers can send commands to satellites via the control protocol, parse and interpret configuration files, and display
the current state of the entire system. Graphical or command-line user interfaces typically are implemented as
controller-type components, providing the possibility for both direct human interaction and scripted procedures.

Controllers are stateless, i.e. they are not a satellite of the Constellation.
The main advantage of this approach is that multiple control interfaces can be active simultaneously, and that they can be
closed and reopened by the operator, or even crash, without affecting the operation of the Constellation.

### Listeners

As the name suggests, components of this type only listen to communications of other components, typically via the
monitoring protocol, and are entirely passive otherwise. Consequently, listeners are stateless and the Constellation is not
affected by them appearing or disappearing during operations.

A typical example for a listener component is a log message interface which subscribes to logging information from satellites
in the Constellation and displays them to the operator.
