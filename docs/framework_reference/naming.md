# Naming Conventions & Case Sensitivity
<!-- markdownlint-disable MD024 -->

This page documents the naming conventions for both the user-facing interface and the internal C++ and Python implementations.

## User Input is Case-Insensitive

All user input should be matched against keys case-insensitively. This concerns configuration keys, but also satellite names
and types, commands etc. The goal of this is to minimize the possibility of long bug-hunts for misconfiguration.

## Public Names

* Satellite types should be `PascalCase`. Case is preserved to make them more easily readable.
* Command names should be `snake_case` for quicker typing and easy tab completion.
* Commands starting with an underscore shall be treated as hidden commands and not advertised e.g. through the `get_commands` call.
* Configuration parameters should best be defined in `snake_case`, i.e. all lower-case with underscores separating words.
* Metric names should be `UPPER_SNAKE_CASE`, i.e. all upper-case with underscores separating words, e.g. `EVENT_RATE`.

## Code Naming Conventions

The code base of Constellation follows concise rules on naming schemes and coding conventions. This allows to maintain a
high code quality and ensures maintainability over a longer period of time. The following naming conventions should be
adhered to when writing code which eventually should be merged into the main repository.

Adherence to these conventions is enforced by the continuous integration as well as the `pre-commit` hooks described in the
[Contributing](contributing.md) guide.

### C++ Conventions

#### Namespaces

The `constellation` namespace should be used for all classes which are part of the framework, nested namespaces may be
defined for subsystems (e.g. `constellation::config`, `constellation::log`). It is encouraged to make use of
`using namespace constellation;` directive in implementation files only for this namespace. Especially the namespace `std`
should always be referred to directly at the function to be called, e.g. `std::string test`.

#### File Names

Header files use the `.hpp` extension and implementation files use the `.cpp` extension. Template implementation files use
the `.ipp` extension. File names are `PascalCase` and match the primary class they define, e.g. `HeartbeatManager.hpp` /
`HeartbeatManager.cpp`. Header guards `#pragma once` should be used throughout.

#### Class Names & Declaration Order

Class names are typeset in `PascalCase`, starting with a capital letter, e.g. `class HeartbeatManager`. Every class must
provide Doxygen documentation for the class itself as well as for all public and protected member functions.

Within a class declaration, sections should appear in this order:

1. `public` — types, constructors/destructor, public member functions
2. `protected` — types, constructors, protected member functions
3. `private` — types, private member functions, member variables

All member variables should be kept together at the end of the `private` section, private member functions may be placed in a
separate `private` section above the member variables.

#### Member Functions

Public member function names are typeset as `camelCase` without underscores, e.g. `getCanonicalName()`. Private member
functions follow `snake_case` using lower-case names, separating individual words with an underscore, e.g. `update_config()`.
This allows to visually distinguish between public and restricted access when reading and writing code.

Public member functions that retrieve a value should be prefixed with `get` and made `const` wherever the object state is not
modified, e.g. `std::string getName() const`. Functions that modify state should be prefixed with `set`, e.g.
`void setInterval(std::chrono::milliseconds)`. Boolean queries should use an `is` or `has` prefix, e.g.
`bool isConnected() const`.

Virtual functions that override a base class method must be annotated with `override`. The annotation `final` should be used
on a class or virtual function only when inheritance or further overriding is explicitly prohibited. The `[[nodiscard]]`
annotation should be added to functions whose return value should not silently discard.

#### Member Variables

Member variables of classes should always be `private` and accessed only via respective public member functions. This
allows to change the class implementation and its internal members without requiring to rewrite code which accesses them.
Member names should be typeset `snake_case`, i.e. in lower-case letters. A trailing underscore is used to mark them as
member variables, e.g. `std::string host_name_`. This sets them apart from local variables declared within a function.

#### Enumerations

Scoped enumerations (`enum class`) are preferred over unscoped `enum`. The enum type name is set as `PascalCase` and
enumerators are `UPPER_SNAKE_CASE`, e.g.:

```cpp
enum class ServiceStatus : std::uint8_t {
    DISCOVERED,
    DEPARTED,
    DEAD,
};
```

#### Comments and Documentation

All public and protected APIs must have Doxygen comments, the starred comment block style `/** */` is preferred over triple
slash notation `///`. Implementation-internal comments should use `//`.
It should be kept in mind that comments should explain *why* a certain implementation has been chosen, not *what* the code
in question does, which should be evident from the code itself.


### Python Conventions

The Python code is expected to follow the guidelines of [PEP 8](https://peps.python.org/pep-0008/) for indentation, line
length, imports, and overall layout. The line length limit is 125 characters. Public modules, classes, functions and methods
should be documented with docstrings following [PEP 257](https://peps.python.org/pep-0257/).

#### File Names

Module file names should be typeset in `snake_case`, e.g. `heartbeat_manager.py`. The name should reflect the main class of
the module file.

#### Class Names & Declaration Order

Class names should be typeset in `PascalCase`, e.g. `class HeartbeatManager`. Every public class must be documented with a
docstring describing its functionality.

Within a class, elements should be stored according to the following order:

1. `__init__` and similar methods
2. Public methods and public properties
3. Internal methods

### Functions, Variables and Attributes

Function names and variables should use `snake_case` naming. A single leading underscore should be added to indicates
internal methods, e.g. `_parse_string(...)` or `self._socket`. Dunder methods (`__init__`, `__repr__`, `__enter__`, etc.)
follow standard Python conventions.

### Type Hints

Type hints are required for all public APIs. The built-in syntax should be used, e.g. `list[str]`, `dict[str, int]`.
Optional values should be marked with `Type | None`.
Internal methods and helpers should carry type hints where sensible.

### Imports

Absolute imports are preferred over relative imports and should be grouped into standard library, third-party package and
local module imports, in this order.

Groups should be separated by a blank line. Wildcard imports should be avoided.
