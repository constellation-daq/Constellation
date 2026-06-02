---
html_theme.sidebar_secondary.remove: true
sd_hide_title: true
---
<!-- markdownlint-disable -->

<script src="_static/js/hero.js"></script>

<!-- CSS overrides on the homepage only -->
<style>
  .bd-container .bd-page-width {
    max-width: none;
  }
  .bd-main .bd-content .bd-article-container {
    max-width: none;
    padding: 0px;
  }
  .bd-main .bd-content .bd-article-container .bd-article {
    padding: 0;
  }
</style>

# Constellation

<div class="hero">
  <div class="hero-title-row">
    <img
      src="_static/logo.svg"
      alt="Constellation Logo"
      class="hero-logo"
    />
    <div class="hero-title">
      <span
        class="typewrite"
        data-period="1000"
        data-type='[
          "Data Acquisition",
          "Instrument Control",
          "Autonomous Operation",
          "Test Beam Campaigns",
          "Dynamic Lab Setups"
        ]'>
      </span>
    </div>
  </div>
  <div class="hero-title-row">
    <div class="hero-subtitle">
      Constellation is a modern, distributed control and data acquisition framework tailored to highly dynamic environments such as laboratory and beamline setups.
    </div>
  </div>
  <div class="hero-title-row">
    <div class="homepage-button-container">
      <div class="homepage-button-container-row">
        <a href="./operator_guide/index.html" class="homepage-button primary-button">Get Started →</a>
        <a href="./operator_guide/concepts/constellation.html" class="homepage-button secondary-button">Concepts</a>
      </div>
      <div class="homepage-button-container-row">
        <a href="./application_development/index.html" class="homepage-button-link">See Application Developer Guide →</a>
      </div>
      <div class="homepage-button-container-row">
        <a href="./framework_reference/index.html" class="homepage-button-link">See Framework Reference →</a>
      </div>
    </div>
  </div>
  </div>

<div class="homepage-content">
  <div class="homepage-container">

<p class="eyebrow">Why Constellation?</p>

## The Instrument Integration Challenge

Coordinated operation of instruments is crucial to modern scientific experiments.
The individual components require configuration, synchronization and monitoring throughout the experiment - tasks realized by control and data acquisition software frameworks.

The complexity of established frameworks necessitates engineering effort for integration and operation that is unattainable for smaller experiments, such as laboratory or beamline experimental setups.
These projects require more flexible and lightweight solutions that can be adapted to changing experimental conditions quickly and by the instrument experts themselves.

<p class="eyebrow">Rethinking Flexibility</p>

## An Autonomous Control and Data Acquisition Framework

Enter *Constellation*, a flexible, network-distributed control and data acquisition framework that enables rapid integration of new devices and allows scientists to connect new instruments with minimal added effort and with the choice between an implementation in C++ or Python.

The framework is designed with flexibility in mind and targets applications ranging from laboratory test stands up to small and mid-sized experiments with several dozens of connected instruments and large data volumes.

::::{grid} 1 2 2 3
:gutter: 3

:::{grid-item-card} {octicon}`zap;1em;sd-text-info` Fast Integration
Finite state machine and satellite interfaces are designed for fast and easy integration of devices.
:::
:::{grid-item-card} {octicon}`repo-forked;1em;sd-text-info` Autonomous
Constellation operates without a central server, satellites exchange heartbeats to keep in touch.
:::
:::{grid-item-card} {octicon}`broadcast;1em;sd-text-info` Flexible
Automatic network discovery of satellites make it easy to add and remove satellites on the fly.
:::
:::{grid-item-card} {octicon}`tools;1em;sd-text-info` Robust
Constellation is based on widely adopted networking libraries such as [ZMQ](https://zeromq.org/) and [MsgPack](https://msgpack.org/).
:::
:::{grid-item-card} {octicon}`code;1em;sd-text-info` Python & C++ APIs
Integrate new components easily using modern APIs for both rapid prototyping and high-performance systems.
:::
:::{grid-item-card} {octicon}`graph;1em;sd-text-info` Telemetry & Monitoring
Collect metrics, monitor health, and visualize telemetry data in real time across the entire experiment.
:::
::::


## Key Concepts in Constellation


:::::{grid} 1 2 2 2
:gutter: 3

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 2 2 2

:::{image} _static/undraw_online_connection_6778.svg
:::

::::

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 10 10 10

:::{div} key-features-text
<strong>Autonomously Operating Satellites</strong><br/>
Satellites are the main actors in a constellation and control instruments or provide functionality.
Constellation is built around the concept of autonomous operation, i.e. satellites run without a central server or user interface connected.
The satellite nodes use heartbeats to exchange information and to take action - such as entering a safe mode when identifying errors, or orchestrating launch sequences.

<a href="./operator_guide/concepts/constellation.html" class="homepage-button-link">Learn about the System Architecture →</a>

:::
::::

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 2 2 2

:::{image} _static/undraw_set_preferences_kwia.svg
:::

::::


::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 10 10 10

:::{div} key-features-text
<strong>Independent User Interfaces</strong><br/>
Constellation user interfaces for controlling or monitoring satellites can be started and closed at any time.
They are stateless and will reconnect to the running Constellation and its satellites upon start, displaying the latest state of the network.
Satellites are independent of these interfaces and continue to run and exchange heartbeat information.

<a href="./operator_guide/concepts/controller.html" class="homepage-button-link">More on Controller Interfaces →</a>

:::
::::

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 2 2 2

:::{image} _static/undraw_knowledge_re_5v9l.svg
:::

::::

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 10 10 10

:::{div} key-features-text
<strong>Extensive Documentation</strong><br/>
Scientific software is often only as good as its documentation.
This is why Constellation is extensively documented and provides a comprehensive user guide featuring descriptions of the basic concepts of the framework,
how-to guides for specific tasks as well as a set of tutorials which ease starting to use it. In addition, in-depth developer documentation
guides novel contributors.

<a href="./operator_guide/index.html" class="homepage-button-link">Start with the Operators Guide →</a>

:::
::::

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 2 2 2

:::{image} _static/undraw_open_source_-1-qxw.svg
:::

::::

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 10 10 10

:::{div} key-features-text
<strong>Free & Open Source Software</strong><br/>
Constellation is entirely free and open source software. The code is released under the [EUPL1.2](https://opensource.org/licenses/EUPL-1.2) license,
and its documentation and this website are licensed under [Creative Commons CC-BY-4.0](https://creativecommons.org/licenses/by/4.0/).

<a href="./about.html" class="homepage-button-link">Information on citation & license intent →</a>

:::
::::

:::::

```{toctree}
:maxdepth: 2
:hidden:

news
About <about>
🛰️ Satellites <satellites/index>
📑 Operate <operator_guide/index>
🧩 Integrate <application_development/index>
🔧 Contribute <framework_reference/index>
```


</div>
</div>
