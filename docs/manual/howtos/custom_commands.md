# Adding Custom Satellite Commands

This how-to guide will walk through the process of adding and registering custom commands for a satellite in C++.
This functionality can be used to expose additional functionality of the devices controlled by the satellite to controllers
in the Constellation network.

## The Command Registry

The satellite command registry provides a facility where the satellite can register arbitrary commands and expose them
to any controller connecting to the satellite. These commands are defined per individual satellite type by registering them
in the respective constructor.

Controllers can query for commands using the `get_commands` request.

Custom commands cannot overwrite the standard commands provided by each satellite and therefore must not reuse their names.

## Adding The Command

In this example, the following command is added to the satellite `MySatellite`:

```cpp
int MySatellite::get_channel_reading(int channel) {
  auto value = device_->read_channel(channel);
  return value / 10;
}
```

Int he constructor of `MySatellite`, the command is registered with the command registry as follows:

```cpp
MySatellite::MySatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
    register_command("get_channel_reading",
                     "This command reads the current device value from the channel number provided as argument. Since this"
                     "will reset the corresponding channel, this can only be done before the run has started.",
                     {State::NEW, State::INIT, State::ORBIT},
                     &MySatellite::get_channel_reading,
                     this);
}
```

The arguments here are, in this order, the name of the command, its description, the allowed states this command can be
called in, the pointer to the command function and the pointer to this satellite instance. The individual parts of the
registration process are discussed below in detail.

### Name and description

The name of the command is the handle with which it will be called from a controller. It should be short and descriptive and
only contain alphanumeric characters and underscores. The description should comprehensively describe the command, its
required arguments and the return value.

In addition to this information, the number of required arguments as well as the allowed states are automatically appended
to the description reported by the satellite e.g. through its `get_commands` response. For the example command registered
above, the output could look like this:

```sh
get_channel_reading:  This command reads the current device value from the channel number provided as argument. Since this
                      will reset the corresponding channel, this can only be done before the run has started.
                      This command requires 1 arguments.
                      This command can only be called in the following states: NEW, INIT, ORBIT
```

### Command arguments and return values

The command registry can handle commands with any number of arguments and can also provide return values from the called
functions back to the controller. Arguments do not have to be specifically denominated when registering the command. The
command registry instead takes this information directly from the function declaration.

```{note}
All parameters and return values of functions in the command registry are coded as `std::string` values and must therefore
be able to be converted to these.
```

The arguments have to be provided as a list of strings in the command payload sent to the satellite.

If more complex data structures should be passed back and forth, specific `to_string` and `from_string` methods might have
to be provided by the satellite in order to code them into and from `std::string`. It should however be investigated if in
these cases custom commands are the right approach to solving the issue at hand.

### Allowed FSM states

Commands may change the internal state of the satellite e.g. by altering the setting of an attached device. It may therefore
be important for some commands to not run when the satellite state machine is in a given state.

A typical example would be a high-voltage power supply, and a custom command that allows changing the current limit. While
changing this limit while the satellite is in its `INIT` state may be unproblematic, changing it in `ORBIT` or `RUN` state
where the voltage output is switched on may lead to the power supply tripping.

The command registry allows limiting the states in which each of the commands can be executed and will report an invalid
command otherwise.
