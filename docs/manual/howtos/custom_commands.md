# Adding Custom Satellite Commands

This how-to guide will walk through the process of adding and registering custom commands for a satellite in C++.
This functionality can be used to expose additional functionality of the devices that is handled by the satellite to
controllers in the Constellation network.

## The Command Registry

The satellite command registry provides a facility where the satellite can register arbitrary commands and expose them
to any controller connecting to the satellite. These commands are defined per individual satellite type by registering them
in the respective constructor.

Controllers can query for commands using the `get_commands` request which returns a list of available commands and FSM
transitions along with their descriptions.

Custom commands cannot overwrite the standard commands provided by Constellation satellites and therefore must not reuse
their names.

## Adding The Command

In this example, the following command is added to the satellite `MySatellite`:

```cpp
int MySatellite::get_channel_reading(int channel) {
  auto value = device_->read_channel(channel);
  return value / 10;
}
```

In the constructor of `MySatellite`, the command is registered with the command registry as follows:

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

```text
get_channel_reading:  This command reads the current device value from the channel number provided as argument. Since this
                      will reset the corresponding channel, this can only be done before the run has started.
                      This command requires 1 arguments.
                      This command can only be called in the following states: NEW, INIT, ORBIT
```

### Command arguments and return values

The command registry can handle commands with any number of arguments and can also provide return values from the called
functions back to the controller. Arguments do not have to be specifically denominated when registering the command. The
command registry instead takes this information directly from the function declaration of the command to be called.

```{note}
All parameters and return values of functions in the command registry are encoded as configuration values and must therefore
be able to be converted to and from these. A detailed information of available types is available in the configuration
documentation.
```

The arguments have to be provided as a list of configuration values in the command payload sent to the satellite. Similarly,
return values from the called functions are converted to a configuration value and are returned in the message payload to
the calling controller.

```{warning}
It is discouraged to implement commands that change the configuration of the instrument or device since these changes take
direct effect and are not reflected in the satellite configuration.
```

### Allowed FSM states

Commands may change the internal state of the satellite e.g. by altering the setting of an attached device. It may therefore
be important for some commands to not run when the satellite state machine is in a given state.

A typical example would be a high-voltage power supply, and a custom command that allows reading the current compliance.
While reading this limit when the satellite is in its `ORBIT` or `RUN` state will produce the correct result, reading the
value in the `NEW` or `INIT` states where the power supply is not fully configured yet may yield wrong values.

The command registry allows limiting the states in which each of the commands can be executed and will report an invalid
command otherwise.
