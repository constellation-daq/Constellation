# Implementing a satellite (Python)

This tutorial will walk through the implementation of a new satellite, written in Python, step by step.
With this basic satellite in place, the code can be extended with  the functionality for specific hardware and needs.
It is recommended to have a peek into the overall [concept of satellites](../../operator_guide/concepts/satellite.md)
in Constellation in order to get an impression of which functionality of the application could fit into which state of the
finite state machine.
There are also multiple [read-made satellites](https://gitlab.desy.de/constellation/constellation/-/tree/main/python/constellation/satellites?ref_type=heads) as well as [the example Python satellite `Mariner`](https://gitlab.desy.de/constellation/constellation/-/tree/main/python/constellation/satellites/Mariner?ref_type=heads) available in the repository, which may serve as a source of inspiration.

```{seealso}
This how-to describes the procedure of implementing a new satellite for Constellation in Python. For C++ look [here](./satellite_cxx.md)
and for the microcontroller implementation, please refer to the [MicroSat project](https://gitlab.desy.de/constellation/satellites/microsat/).
```

## The basic satellite class structure

The soon-to-be satellite will gain all its basic functionality, such as the ability to receive and react to commands, initiate state changes, and send its logging output and monitoring information via the network, by inheriting from the `Satellite` class:

```python
from constellation.core.satellite import Satellite, SatelliteArgumentParser
from constellation.core.logging import setup_cli_logging


class TutorialMachine(Satellite):
    """An in-progress Satellite implementation."""
    pass  # NOTE placeholder statement until we start the implementation!


def main(args=None):
    """In-progress tutorial Satellite."""

    # Get a dict of the parsed arguments
    parser = SatelliteArgumentParser(description=main.__doc__)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # Start satellite with remaining args
    s = TutorialMachine(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()

```

Most code so far deals with argument parsing and start-up, while the actual satellite does not implement any extra
functionality. When saving the above code to a file `tutorial.py`, this satellite can already be started and operated though!

```shell
python3 tutorial.py --help
```

The above command will list all available parameters. To run the satellite as-is, it can be started with:

```shell
python3 tutorial.py -g myconstellation
```

The satellite can now be controlled by running `Controller -g myconstellation`
from a different terminal window or by starting the graphical `MissionControl` controller. More details on operating a
Constellation are provided in the [Operator's Guide](../../operator_guide/index.md).

In its current form, the tutorial satellite will not yet do much. The following sections will guide through extending its functionality.

## Implementing the FSM Transitions

Any satellite will start in the `NEW` state and remain there until it receives a
command to initiate a state change.

In Constellation, all common actions such as device configuration and hardware
initialization are realized through so-called transitional states which are
entered by a command and exited as soon as their action is complete. The actions
attached to these transitional states are implemented by overriding methods
provided by the `Satellite` base class.

For a new satellite, the following transitional state actions **should be all be
considered for implementation**:

* `def do_initializing(self, config: Configuration)`: use the configuration provided by the `config` argument to gather and validate all parameters of the satellite and (optionally) establish a connection to the hardware using said parameters.
* `def do_launching(self)`: prepare everything for data taking. This includes all one-time actions that are needed before subsequent measurements can take place.
* `def do_starting(self, run_identifier: str)`: start a measurement using the `run_identifier` as a label.
* `def do_stopping(self)`: stop the on-going measurement but remain prepared for the next.
* `def do_landing(self)`: power down and e.g. disarm the hardware.

The following transitional state actions are optional:

* `def do_interrupting(self)`: this is the transition to the `SAFE` state and defaults to `do_stopping` (if necessary because current state is `RUN`), followed by `do_landing`. If desired, this can be overwritten with a custom action.
* `do_reconfigure`: this transition occurs from `ORBIT` (i.e. when your satellite is ready to take data) and -- if your satellite and hardware supports it -- allows to change some or all of the configuration parameters.

For the steady state action for the `RUN` state, see below.

The following code implements some basic transitions into the tutorial class:

```python
import socket

class TutorialMachine(Satellite):
    """An in-progress Satellite implementation."""

    def do_initializing(self, config: Configuration) -> str:
        """Establish a connection to the hardware via socket."""
        address = config.setdefault("ip_address", "192.168.1.2")
        port = config.setdefault("port", 56789)
        if getattr(self, "socket", None):
            # already have a connection?
            if self.socket.connected():
                self.socket.close()
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((address, port))
        return f"Established a connection to {address}:{port}"

    def do_launching(self) -> str:
        self.socket.send("PREPARE")
        return "Prepared!"

    def do_starting(self, run_identifier:str) -> str:
        self.socket.send(f"START_{run_identifier}")
        return "Prepared!"

    def do_stopping(self) -> str:
        self.socket.send(f"STOP!")
        return "Stopped."

    def do_landing(self) -> str:
        self.socket.send(f"Power down!")
        return "Stopped."
```

Note that all of the above transitions return a string which summarizes the
state change. This string will afterwards be a part of the status message of the
satellite.

This particular satellite implementation sets up a network socket to send
commands via a TCP/IP network connection to a device. In the actual satellite implementation,
this might be a USB connection, or maybe the hardware has its own Python library that communicates with it.

```{caution}
The `do_initializing` routine can be called more than once as this transition is allowed from both `NEW` and `INIT` as well as 'ERROR' and 'SAFE' states. It should therefore be carefully ensured that e.g. any already open connections are closed before establishing new ones or that the class keeps track of any steps that only needs to be performed once (e.g. loading an FPGA bit stream).
```

Note that the configuration parameters in `do_initialize` are accessed via
`config.setdefault()`. This method will return the value for the respective key
and fall back to a default value should no such key be configured.

Any options that **must** be provided can also be accessed directly
as with any dictionary, for example `config["my_important_parameter"]`. In this
case, should the key `my_important_parameter` not be found, an exception will be
raised. See the section on error handling below for what that entails.

In case a specific key is *not accessed* in the configuration during the
initialization transition, the satellite will log a warning as this might point
to a user error, such as a typo in the parameter name.

## Running and the Stop Event

In satellite's `RUN` state,  all actions are performed by the `do_run` method, which -- just as
the transitional state actions above -- must be overridden from the `Satellite` base
class. `do_run` will be called upon entering the `RUN` state (and after the
`do_starting` action has completed). It can run as long as it needs to but is expected to finish as quickly as
possible as soon as the `stop_running` Event is set. An example run loop is shown
below:

```python
def do_run(self, payload: any) -> str:
    # the stop_running Event will be set from outside the thread when it is
    # time to close down.
    while not self._state_thread_evt.is_set():
        # Do work
        ...
    return "Finished acquisition."
```

Any finalization of the measurement run should be performed in the `do_stopping` action rather than at the end of the `do_run` function, if possible.

## Error handling and shutdown

All transition methods are wrapped into a `try:`/`except:` clause and thus
prevent the satellite from crashing should any unexpected error arise. Instead,
the transition (or run) will be aborted, and the satellite will enter the
`ERROR` state. In this state, the satellite's status or its logs are a useful resource to find out the reason for the error.

To allow the satellite to fail "gracefully", that is in a controlled fashion, the class can provide a method `fail_gracefully` that handles e.g. the closure of any resources:

```python
    def fail_gracefully(self) -> str:
        """Method called when reaching 'ERROR' state."""
        if getattr(self, "socket", None):
            # already have a connection?
            if self.socket.connected():
                self.socket.close()
        return "Failed gracefully."
```

Be aware that the `ERROR` state could potentially be reached from any other state. The routine should therefore do the best it can to assist a later recovery through initialization.

If a satellite is shut down, for example by receiving a termination signal from
the OS or if you press `Ctrl`-`C` in the terminal window where the satellite
runs, the method `reentry` will be called. In most circumstances, there is no need
to implement anything satellite-specific for this method. If the satellite however requires this, it should be ensured
that `super().reentry()` is called as last step of that routine, so that the
`Satellite` base classes `reentry` methods are executed as well.

## Troubleshooting

### Satellite base functionality not working as expected

If a satellite class has been implemented, but it does not behave as expected, this could depend on one or multiple of the following

* *Other* methods than the ones described above have been implemented, but are not calling the parent class' method in your implementation, e.g. the `__init__` method. In those cases, be sure to add a `super()` call, e.g. `super().__init__(*args, **kwargs)` and adjust the signature of the implementation to support the same arguments as the parent class' method.
* The satellite *accidentally* implemented a method, an attribute or a property already existing in the parent class, thus shadowing members of the `Satellite` class and its base classes. Currently, there is no protection against this and satellite implementations need to be carefully written to not fall into this trap.

The following code can be run in a Python console (REPL) to see all members of a `Satellite` and compare that with the attributes and methods the satellite class uses:

```python
from constellation.core.satellite import Satellite
s = Satellite()
dir(s)
```

More information about a specific method can be gathered by either looking at the source code or, after having run the above
commands, calling `help(s.method_of_interest)` to learn more about a method, in this case, `method_of_interest`.
