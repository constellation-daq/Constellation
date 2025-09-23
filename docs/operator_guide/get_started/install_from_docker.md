# Installing from Docker

Constellation is available as Docker image, allowing to easily run satellites in a container. The images contains all
framework satellites. Graphical user interfaces are not available in the image.

```{hint}
It is also possible to use [podman](https://podman.io/docs/installation) instead of docker, which is easier to install.
```

A satellite can be started via:

```sh
docker run --network host -it gitlab.desy.de:5555/constellation/constellation/constellation:latest
```

```{attention}
Without `--network host` network discovery does not work.
```

In the [container registry](https://gitlab.desy.de/constellation/constellation/container_registry), two images are available:

- `constellation`: based on Ubuntu 24.04
- `constellation_ci`: based on Fedora, containing development files and tools for building external C++ satellites

The following tags are available for each images:

- `latest`: tag pointing to the last released version
- `vX.Y.Z`: tags for each release starting from version 0.6
- `nightly`: tag pointing to the last nightly build from the main branch (not recommended for production use)

```{note}
Currently, only C++ satellites are available in the Docker images.
```
