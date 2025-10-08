# Environment Variables

For configuration parameters such as access keys or tokens it can be beneficial to not store them directly in configuration
files, which might be under version control or shared to and accessible by a wide range of people.
For this purpose, Constellation supports placeholders for environment variables, which are read at run-time.

## Placeholders in Configuration Files

The syntax of placeholders is based on the familiar format of environment variables in the shell, i.e. a variable name
prefixed with a dollar sign. Since environment variables are always string representations, the usage of placeholders is
reserved for string-type configuration keys.

A configuration value can contain multiple and different environment variable placeholders.
They are placed directly in the value of the respective configuration key, for example:

```toml
file_path = "/home/${USER}/data/"
```

Environment variables can either be present on the machine where the satellite requiring the parameter runs, or only on the
node running a controller of the Constellation. Hence, both concepts are available as described in the following.

## Controller-Side Variables

*Controller-side variables* can be placed in configuration files using the syntax `_${VARIABLE}`. These placeholders will
be resolved on the controller side, i.e. before encoding and sending the configuration to satellites during the
{bdg-secondary}`initializing` or {bdg-secondary}`reconfiguring` state transitions. The resolution is performed on the
node the controller runs on. When parsing configuration keys on the controller side, an error is displayed when a
referenced environment variable cannot be found.

```{warning}
Controller-side environment variables will be substituted by the controller and subsequently sent to the respective
satellites in clear text. Also retrieving the configuration from a satellite will contain the substituted values. They
should therefore not be used for secrets.
```

## Satellite-Side Variables

*Satellite-side variables* are denoted using the syntax `${VARIABLE}`. They will be resolved on the satellite side, i.e.
only after the satellite has received the configuration and when it accesses the respective configuration key. The
resolution is performed on the node the satellite runs on. Substituted satellite-side variables will not be added to the
configuration and are therefore not accessible outside the satellite node, neither through the `get_config` command nor
through the run metadata. They are therefore ideal for storing secrets in satellite configurations that should not be
accessible to everyone.

A typical usage of a satellite-side environment variable is a secret not to be shared in the Constellation, such as the
access token portion of a [Mattermost web hook](../howtos/setup_mattermost_logger.md). On the node running the `Mattermost`
satellite, an environment variable is defined:

```sh
export MM_HOOK_TOKEN="9om7nhes7p859e1qrxi5dgykzr"
```

In the Constellation configuration file, this environment variable is now referenced:

```toml
[Mattermost.Logger]
webhook_url = "https://yourmattermost.com/hooks/${MM_HOOK_TOKEN}"
```

During the {bdg-secondary}`initializing` stage, the `Mattermost` satellite will access the `webhook_url` key. At this
moment, all environment variable placeholders in the value are resolved, and the `${MM_HOOK_TOKEN}` environment variable
will be substituted, resulting in the final value provided to the satellite being e.g.
`https://yourmattermost.com/hooks/9om7nhes7p859e1qrxi5dgykzr`.


## Default Values
