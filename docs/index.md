---
html_theme.sidebar_secondary.remove: true
sd_hide_title: true
---
<!-- markdownlint-disable -->

<!-- CSS overrides on the homepage only -->
<style>
.bd-main .bd-content .bd-article-container {
  max-width: 70rem; /* Make homepage a little wider instead of 60em */
}
/* Extra top/bottom padding to the sections */
article.bd-article section {
  padding: 3rem 0 7rem;
}
/* Override all h1 headers except for the hidden ones */
h1:not(.sd-d-none) {
  font-weight: bold;
  font-size: 48px;
  text-align: center;
  margin-bottom: 4rem;
}
/* Override all h3 headers that are not in hero */
h3:not(.hero h3) {
  font-weight: bold;
  text-align: center;
}
</style>

# Constellation: The Autonomous Control and Data Acquisition System for Dynamic Experimental Setups

::::::{grid} 1 2 2 2
:gutter: 0
:margin: 0
:padding: 0
:class-container: hero

:::::{grid-item}
:margin: 0
:padding: 0 0 0 4
:columns: 12 5 5 5

<h2 style="font-size: 60px; font-weight: bold; margin: 2rem 0 0 0;">Constellation</h2>
<h3 style="font-weight: bold; margin-top: 0;">Autonomous Control and <br/>Data Acquisition System</h3>
<p>Constellation is a control and data acquisition system for small-scale experiments and experimental setup with volatile and dynamic constituents such as testbeam environments or laboratory test stands.</p>

<div class="homepage-button-container">
  <div class="homepage-button-container-row">
      <a href="./operator_guide/index.html" class="homepage-button primary-button">Get Started</a>
      <a href="./operator_guide/concepts/constellation.html" class="homepage-button secondary-button">Concepts</a>
  </div>
  <div class="homepage-button-container-row">
      <a href="./application_development/index.html" class="homepage-button-link">See Application Developer Guide →</a>
  </div>
  <div class="homepage-button-container-row">
      <a href="./framework_reference/index.html" class="homepage-button-link">See Framework Reference →</a>
  </div>
</div>

:::::

:::::{grid-item}
:margin: 0
:padding: 5 0 0 0
:columns: 12 7 7 7

::::{grid} 1 2 2 2
:gutter: 3

:::{grid-item-card} {octicon}`repo-forked;1em;sd-text-info` Autonomous
Constellation operates without a central server, satellites exchange heartbeats to keep in touch.
:::
:::{grid-item-card} {octicon}`broadcast;1em;sd-text-info` Flexible
Automatic network discovery of satellites make it easy to add and remove satellites on the fly.
:::
:::{grid-item-card} {octicon}`zap;1em;sd-text-info` Fast Integration
The finite state machine and satellite interface are designed for fast and easy integration of devices.
:::
:::{grid-item-card} {octicon}`tools;1em;sd-text-info` Robust
Constellation is based on widely adopted networking libraries such as [ZMQ](https://zeromq.org/) and [MsgPack](https://msgpack.org/).
:::
::::

:::::

::::::

# Key Features

:::::{grid} 1 2 2 2
:gutter: 3

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 2 2 2

:::{image} _static/undraw_code_typing_re_p8b9.svg
:::

::::

::::{grid-item-card}
:shadow: none
:class-card: sd-border-0
:columns: 12 10 10 10

:::{div} key-features-text
<strong>Built on Solid Protocols</strong><br/>
All communication between Constellation components is based on protocols designed for flexible cross-language communication and serialization.
A Constellation can consist of constituents written in any language, such as the main implementations in C++ and Python.
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
Constellation works independently of connected user interfaces, which can be started and closed as needed. They are stateless and will
reconnect to the Constellation and its satellites upon start, displaying the latest state of the network.
:::
::::

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
<strong>Autonomous Operation</strong><br/>
Constellation allows autonomous operation, i.e. without a central server of user interface connected. It uses heartbeats to distribute
information between satellites and to take action - such as entering a safe mode when identifying errors, or orchestrating launch sequences.
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
<strong>Extensively Documented</strong><br/>
Constellation is extensively documented and provides a comprehensive user guide featuring descriptions of the basic concepts of the framework,
how-to guides for specific tasks as well as a set of tutorials which ease starting to use it. In addition, in-depth developer documentation
guides novel contributors.
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
and its documentation and this website are licensed under [Creative Commons CC-BY-4.0](https://creativecommons.org/licenses/by/4.0/). The source code
is available on [DESY's GitLab instance](https://gitlab.desy.de/constellation/constellation).
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
