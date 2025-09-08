# Listing Satellites in the Library

The [Satellite Library](/satellites/index) is a central listing of satellites available for use in Constellation. It aggregates satellite information
from different sources, and this page provides information on which of these sources might be most suitable for new satellite developments.

However, as these criteria are not immutable law, it makes sense to decide on a case-by-case basis.
We therefore encourage developers to contact us at an early stage to find the most suitable solution for their satellite or application in question.

## Inclusion in the Constellation Repository

Including a satellite in the main repository of Constellation means that it can be part of default installations, that it
will receive updates at the same cadence as the core framework, but it also places a higher workload on those maintaining
and developing the framework.
Therefore, the following criteria should be met for including a satellite in the main repository:

* The satellite provides functionality to the framework that is **independent of instrument control**. A typical example for this type of satellite is the *[FlightRecorder](/satellites/FlightRecorder)* which provides log storage facilities.
* Alternatively, the instrument controlled by the satellite is **commercially available**. An example of this is the *[Keithley](/satellites/Keithley)* satellite which allows to control a widespread brand of source measure units.
* Either **little code is required** for communicating with the instrument *or* the communication code is abstracted into a library provided externally.
* There is a **substantial user community** that justifies centralized maintenance and development of the satellite.

In all other cases (i.e. hardware control for a niche system, large control code base required, small user community) the satellite is probably more fittingly stored in a separate repository and developed independently of the main Constellation code.

## Listing an External Satellite in the Library

Satellites not part of the main Constellation repository can still be listed in the Satellite Library as externally available component.
To add a satellite to the library, a README file that complies with the conventions for Constellation satellite documentation must be publicly accessible and an entry has to be added to the [external_satellites.json](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/satellites/external_satellites.json) file in the main Constellation repository, containing the following keys:

```json
{
  "name": "MyInstrument",
  "readme": "https://gitlab.example.com/myinstrument/-/raw/main/source/README.md",
  "website": "https://example.com/myinstrument"
},
```

The README key needs to point to the raw Markdown-formatted file, not to a rendered HTML representation. In particular, for
GitHub this means using URLs pointing to `https://raw.githubusercontent.com/` instead of `https://github.com` and for GitLab
to use the link containing `/raw/` instead of `/blob/`. This URL can be found by clicking the "Open raw" button at the top
of the respective page.
