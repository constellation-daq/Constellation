# About Constellation

Constellation is an autonomous control and data acquisition system for small-scale experiments and experimental setup with volatile and dynamic constituents such as testbeam environments or laboratory test stands. Constellation aims to provide a flexible framework that is easy to use and requires minimal effort for the integration of new devices.

```{image} logo/logo_desy.png
:alt: DESY Logo
:width: 100px
:align: left
```

Constellation is hosted at and mainly developed by [Deutsches Elektronen-Synchrotron DESY](https://www.desy.de/), a Research Center of the Helmholtz Association.

A list of all authors and contributors can be found in the [AUTHORS.md file](https://gitlab.desy.de/constellation/constellation/-/blob/main/AUTHORS.md) in the software repository.

The development of Constellation is coordinated through the *EDDA (Exchange on & Development of Data Acquisitions)* Collaboration. Events, workshops and new releases are announced via the [EDDA mailing list](https://lists.desy.de/sympa/info/edda).

## Software License

The software is released as open source software under the terms of the [European Union Public License 1.2](https://gitlab.desy.de/constellation/constellation/-/blob/main/LICENSE). The authors wish to emphasize the interoperability of the license, and hereby declare their intention to choose this license so that:

- all modifications and extensions of the Constellation framework must be made available under the same or a compatible license, such as the LGPL.
- software that links parts of the Constellation framework or the Constellation libraries, or connects to components of the Constellation framework via a network, can be implemented under any license, including proprietary licenses.

More information in the intent of the European Union Public License can be found on the [website of the European Commission](https://interoperable-europe.ec.europa.eu/collection/eupl/introduction-eupl-licence).

This website and the documentation are distributed under the [Creative Commons CC-BY-4.0](https://creativecommons.org/licenses/by/4.0/) license.

## Citing Constellation

Constellation is distributed freely and openly under the EUPL license, but the authors kindly ask to cite the reference paper
and the Zenodo record in scientific publications:

- The reference paper is published in NIM A and can be found at [10.1016/j.nima.2026.171279](https://doi.org/10.1016/j.nima.2026.171279).
  Please cite this paper for all works using Constellation, for example as:

  > S. Spannagel et al., “Constellation: The autonomous control and data acquisition system for dynamic experimental setups”, Nucl. Instr. Meth. A 1085 (2026) 171279, doi:10.1016/j.nima.2026.171279, arXiv:2601.06494.

- The versioned Zenodo records can be found at [10.5281/zenodo.15688357](https://doi.org/10.5281/zenodo.15688357).
  Please cite the version used for the published work. For example, the version 0.6.1 should be cited as:

  > S. Lachnit, H. Perrey & S. Spannagel (2025). Constellation (0.6.1). DOI: [10.5281/zenodo.17294234](https://doi.org/10.5281/zenodo.17294234).

:::{dropdown} BibTex Citations
:icon: book

```bibtex
@article{constellation,
  title = {Constellation: The autonomous control and data acquisition system for dynamic experimental setups},
  journal = {Nucl. Instr. Meth. A},
  volume = {1085},
  pages = {171279},
  year = {2026},
  issn = {0168-9002},
  doi = {10.1016/j.nima.2026.171279},
  author = {Simon Spannagel and Stephan Lachnit and Hanno Perrey and Justus Braach and Lene Kristian Bryngemark and Erika Garutti and Adrian Herkert and Finn King and Christoph Krieger and David Leppla-Weber and Linus Ros and Sara {Ruiz Daza} and Murtaza Safdari and Luis G. Sarmiento and Annika Vauth and Håkan Wennlöf},
  eprint={2601.06494},
  archivePrefix={arXiv},
  primaryClass={physics.ins-det},
}

@software{lachnit_2025_17294234,
  author       = {Stephan Lachnit and Hanno Perrey and Simon Spannagel},
  title        = {Constellation},
  year         = 2025,
  publisher    = {Zenodo},
  version      = {0.6.1},
  doi          = {10.5281/zenodo.17294234},
}

```

:::
