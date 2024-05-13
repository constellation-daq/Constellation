# Start a Satellite & Control It

## Starting a Satellite

To start a satellite, you need to provide three things:

- Type of satellite. This corresponds to the class name of the satellite implementation
- Group. The name of the constellation group this satellite should be a part of
- Name for the satellite. This name is a user-chosen name and should be unique within the group.

Example

```sh
./build/cxx/constellation/exec/satellite -t prototype -g myLabPlanet -n TheFirstSatellite
```

## Controlling your Satellite

tba
