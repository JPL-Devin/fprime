
# Targets

The build system supplies the ability to register extra targets that allow the user to register new
functionality to the build system. For example, the user may wish to create a build target that
counts the total number of files in the system.  This would be the place to register such a target
and have it available to the build system.

Targets are applied both at the global scope and per-module scope. Thus each target can provide a set of build targets (one per registered module) and a global build target. For example, a counting target might provide a `count` global target to count all files, and a `<MODULE>_count` to count the files of a given module.

## Built-In Targets

The CMake system supplies several targets that are useful for all projects and thus are included
as part of the CMake system. These targets are defined in [cmake/target](https://github.com/nasa/fprime/tree/devel/cmake/target).

**`all` and included code.** `fprime-util build` at the project root and `fprime-util build --all` build CMake's `all`
target. `all` contains the project's own modules, deployments and unit tests and whatever they depend on; framework and
library modules included through `fprime_setup_included_code()` that nothing in the project uses are registered
`EXCLUDE_FROM_ALL` and are built only on demand (`fprime-util build` in their directory, `fprime-util build --target
<module>`, or as a dependency). Set `FPRIME_BUILD_ALL_INCLUDED_CODE=ON` to build every included module. Unit tests
enabled by `FPRIME_ENABLE_FRAMEWORK_UTS` / `FPRIME_ENABLE_<LIBRARY>_UTS` are never excluded, so `fprime-util check --all`
runs every registered test. The framework (or a library) building from its own directory is treated as project code;
`settings.ini`'s `project_root` has no effect on what `all` builds.


## Adding Custom Targets

See the [Customization Guide](cmake-customization.md) for a description of adding custom targets.
