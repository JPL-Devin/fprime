# AGENTS.md — AI Agent Instructions for F Prime

This file provides instructions for AI coding agents (Claude Code, Devin, GitHub Copilot, etc.) working on the F Prime repository. It describes the project structure, build system, coding standards, testing conventions, and contribution workflow.

> **Scope:** This file covers the core [fprime](https://github.com/nasa/fprime) repository. Related repositories ([fprime-tools](https://github.com/fprime-community/fprime-tools), [fprime-gds](https://github.com/fprime-community/fprime-gds), [fpp](https://github.com/nasa/fpp)) have their own conventions.

---

## Project Overview

F Prime (F') is a component-driven flight software framework developed at NASA's Jet Propulsion Laboratory. It targets embedded and spaceflight applications. The framework provides:

- A **component architecture** with typed ports for inter-component communication
- **FPP** (F Prime Prime), a modeling language that generates C++ boilerplate
- A **CMake-based build system** driven by `fprime-util`
- A **Ground Data System** (GDS) in Python for commanding and telemetry

**Key constraint:** This is flight software. Code quality, safety, and correctness are paramount. All changes must meet the standards described below.

---

## Repository Structure

```
fprime/
  Fw/           # Core framework: base classes, types, ports, serialization
  Svc/          # Reusable service components (CmdDispatcher, TlmChan, Health, etc.)
  Drv/          # Hardware abstraction and device driver components
  Os/           # Operating system abstraction layer (POSIX, Darwin, stub)
  Ref/          # Reference deployment application (example topology)
  CFDP/         # CCSDS File Delivery Protocol implementation
  Utils/        # Utility libraries
  STest/        # Structural testing library
  TestUtils/    # Shared testing utilities
  Fpp/          # FPP-specific test and support code
  FppTestProject/ # FPP autocoder integration tests
  default/      # Default configuration headers (config/)
  cmake/        # Build system: autocoders, platform support, targets
  ci/           # CI test scripts
  docs/         # Documentation and user manual
  googletest/   # GoogleTest submodule
  .github/      # CI workflows, PR templates, code review agents
```

### Component Directory Layout

Each component follows a consistent structure:

```
Svc/ComponentName/
  ComponentName.fpp          # FPP model (ports, commands, events, telemetry, parameters)
  *.fppi                     # FPP include files (events, telemetry split out)
  ComponentNameComponentImpl.hpp   # Implementation header
  ComponentNameComponentImpl.cpp   # Implementation source
  CMakeLists.txt             # Build registration
  docs/
    sdd.md                   # Software Design Document
  test/
    ut/
      ComponentNameTester.hpp      # Test harness header
      ComponentNameTester.cpp      # Test harness implementation
      ComponentNameTestMain.cpp    # GoogleTest main with TEST() macros
```

**Auto-generated files** (suffixed `Ac.hpp`, `Ac.cpp`, `GTestBase.*`, `TesterBase.*`) are build products. Never edit them manually.

### Topology (Deployment) Layout

```
Ref/Top/
  instances.fpp       # Component instance definitions
  topology.fpp        # Connection graph (port wiring)
  RefTopology.cpp     # Topology setup and teardown C++ code
  RefTopology.hpp     # Topology header
  RefTopologyDefs.hpp # Deployment-specific definitions
```

---

## Build System

### Prerequisites

- Python 3.9+, pip, virtual environments
- C/C++ compiler (GCC or Clang), CMake 3.16+
- Install tools: `pip install -Ur requirements.txt`
- Initialize submodules: `git submodule update --init --recursive`

### Core Tool: `fprime-util`

All builds go through `fprime-util`. Do **not** invoke CMake directly.

```bash
# Generate build system (from repo root or a deployment directory)
fprime-util generate

# Build all modules
fprime-util build --all

# Generate with unit tests enabled
fprime-util generate --ut

# Build all unit tests
fprime-util build --all --ut

# Run all unit tests
fprime-util check --all

# Run unit tests for a single component
cd Svc/BufferManager
fprime-util check

# Format changed C++ files (all files modified since branching off devel)
git diff --name-only devel...HEAD | fprime-util format --stdin

# Format a specific directory
fprime-util format --dirs Svc/BufferManager

# Purge build artifacts
fprime-util purge
```

### Build with Static Analysis

```bash
# clang-tidy (matches CI)
fprime-util generate --ut -DCMAKE_CXX_CLANG_TIDY=clang-tidy
fprime-util build --all --ut
```

### CMake Registration

Each component's `CMakeLists.txt` registers source files:

```cmake
set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/ComponentName.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/ComponentNameComponentImpl.cpp"
)
register_fprime_module()

# Unit tests
set(UT_SOURCE_FILES
  "${FPRIME_FRAMEWORK_PATH}/Path/To/ComponentName.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/test/ut/ComponentNameTester.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test/ut/ComponentNameTestMain.cpp"
)
register_fprime_ut()
```

---

## Coding Standards (C++)

### Language Version and Restrictions

- **C++14** compliance required. No C++17 or later features.
- **No exceptions, RTTI, STL containers, or `std::string`**. These cause implicit allocation or code bloat in embedded targets.
- **No lambdas.** Templates are allowed but should remain simple.
- **No dynamic memory after initialization.** All memory must be allocated at startup.

### Type System

- Use F Prime framework types (`FwIndexType`, `FwSizeType`, `Fw::String`, `Fw::Buffer`, etc.) instead of bare C/C++ types where appropriate.
- Use FPP modeled types for ground-facing interfaces (events, commands, telemetry, parameters).
- Prefer `Fw::String` over `char*`. Use `char*` only for string literals or external API boundaries.
- Prefer `Fw/DataStructures` types over bare C/C++ or inlined data structures where applicable.
- Use configurable `Fw*` types in preference to bare integral types.

### Null Pointers, Casts, and Memory

- Use `nullptr` only. Never `NULL` or `0` as null pointer.
- **No C-style casts or function-style casts.** Use `static_cast<>`.
- Avoid `reinterpret_cast` and `const_cast`; require justification if used.
- Prefer `constexpr`, then `const`, unless mutation is required.
- Prefer references over pointers where possible.
- Do not pass C-style arrays. Use structs containing array + length.

### Classes and Inheritance

- Avoid multiple inheritance; only acceptable for pure virtual interface inheritance.
- Mark overrides with `override`.
- Destructors should be virtual, or protected non-virtual.
- Use `explicit` constructors where appropriate. Explicitly call base class constructors.
- Follow Rule of Three or Rule of Five where ownership/lifetime is involved.
- `friend` should be used only for unit test access.
- Do not use `using namespace`.

### Assertions

- `FW_ASSERT` is for **programming errors only**. Do not use it for untrusted or external inputs (hardware, ground commands, data from off-device sources).

### Constants and Macros

- Prefer constants over `#define`. Flag complex macro usage.
- Initialize all variables.

### Style and Formatting

- **Formatting is enforced by `clang-format`** (Chromium base, 4-space indent, 120 column limit). See `.clang-format`.
- **Header guards:** Use `#ifndef FOO_HPP` / `#define FOO_HPP` style. Not `#pragma once`.
- **Line length:** 120 characters max.
- **Compiler warnings are errors** (`-Werror`). The build enables `-Wall -Wextra -Wconversion -Wdouble-promotion -Wshadow -pedantic` and `-Wold-style-cast` for C++.
- Follow the [F Prime Style Guidelines](https://github.com/nasa/fprime/wiki/F%C2%B4-Style-Guidelines).
- Python code is formatted with `black`.

---

## FPP (F Prime Modeling Language)

FPP files (`.fpp`, `.fppi`) define component interfaces, types, topologies, and more. The FPP compiler generates C++ base classes (`*Ac.hpp`, `*Ac.cpp`).

### Key Concepts

- **Components:** Active (own thread + queue), Queued (queue, no thread), or Passive (runs on caller's thread).
- **Ports:** Typed interfaces. Can be `sync input`, `async input`, `guarded input`, or `output`.
- **Special ports:** `time get port`, `event port`, `telemetry port`, `command recv port`, etc.
- **Events (EVRs):** Discrete log messages with severity levels.
- **Telemetry channels:** Periodic data points.
- **Commands:** Ground-dispatchable operations with opcodes.
- **Parameters:** Persistent configuration values.
- **Topology:** Defines component instances and their port connections.
- **`.fppi` files:** FPP include fragments, used to split large definitions (e.g., events and telemetry into separate files).

### Working with FPP

- Edit `.fpp` files to change component interfaces. The autocoder generates the `*Ac.*` files during build.
- After changing an FPP model, rebuild to regenerate the base classes, then update the implementation class if the interface changed.
- Never edit `*Ac.hpp` or `*Ac.cpp` files.

---

## Testing

### Unit Test Framework

Tests use **GoogleTest** (submodule at `googletest/`). The F Prime autocoder generates test harness base classes (`GTestBase`, `TesterBase`) from FPP models.

### Test Structure

Each component's unit tests live in `test/ut/` and consist of:

1. **`ComponentNameTester.hpp/cpp`** — Test harness class that inherits from the auto-generated `ComponentNameGTestBase`. Contains test helper methods and the component under test.
2. **`ComponentNameTestMain.cpp`** — Contains `TEST()` macros and `main()`.

```cpp
TEST(Nominal, SomeTest) {
    Svc::ComponentNameTester tester;
    tester.someTestMethod();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

### Running Tests

```bash
# All unit tests from repo root
fprime-util generate --ut
fprime-util build --all --ut
fprime-util check --all

# Single component
cd Svc/ComponentName
fprime-util check
```

### Test Expectations

- All new code must include unit tests.
- Tests should verify port behavior, command handling, event emission, telemetry updates, and error/edge cases.
- The STest library (`STest/`) provides helpers for structured scenario-based testing.

---

## Documentation

### Software Design Documents (SDDs)

Each component should have a `docs/sdd.md` with:

1. **Introduction** — What the component does
2. **Requirements** — Table of requirements with verification method
3. **Design** — Assumptions, Block Description Diagram, ports, state, port behavior, sequence diagrams, assertions
4. **Configuration** — Constants, runtime setup

When modifying a component's behavior or interface, update its SDD.

---

## CI Checks

Pull requests to `devel` trigger the following CI workflows:

| Check | What it does |
|---|---|
| **CI [ubuntu]** | Builds framework + Ref deployment, runs all unit tests, integration tests with Valgrind, and static analysis (clang-tidy) |
| **Code Format Check** | Verifies C++ formatting with `clang-format` via `fprime-util format --check` |
| **Cpplint** | Runs `cpplint` for style compliance |
| **Spell checking** | Checks spelling across the codebase |
| **Format Python** | Checks Python formatting with `black` |
| **Markdown link check** | Verifies links in Markdown files |
| **FPP tests** | Tests FPP autocoder outputs |
| **External builds** | Builds external reference applications (tutorials, LED Blinker, etc.) to catch breakage |

### Paths That Skip CI

Changes only in `docs/**`, `**.md`, `.github/actions/spelling/**`, or `.github/ISSUE_TEMPLATE/**` skip the build/test workflows.

### Before Submitting

1. **Format C++ code:**
   ```bash
   git diff --name-only devel...HEAD | fprime-util format --stdin
   ```
2. **Run unit tests locally:**
   ```bash
   fprime-util generate --ut
   fprime-util build --all --ut
   fprime-util check --all
   ```
3. **Check Python formatting** (if Python files were modified):
   ```bash
   pip install black==21.6b0
   black --check --diff ./
   ```

---

## Contribution Workflow

1. **Branch from `devel`** — The default development branch is `devel`, not `main` or `master`.
2. **One issue, one PR** — Each PR should correspond to a CCB-approved issue.
3. **Squash & Merge** — All PRs are squash-merged. Use imperative-style phrasing for the PR title (e.g., "Add buffer overflow check to FileUplink").
4. **Fill in the PR template** — Including the "Generative AI was used" field and the "AI Usage" section if applicable.
5. **Keep PRs small and focused** — Large PRs are difficult to review and may be delayed.
6. **External repo checks** — If your change breaks how F Prime is used in tutorial/reference repos, you may need to submit companion PRs to those repos on branches named `pr-<PR_NUMBER>`.

### AI Disclosure

Per the [AI Policy](AI_POLICY.md), all generative AI usage must be disclosed in PRs, issues, and discussions. Specify the tool, scope, and level of modification.

---

## Quick Reference for Common Tasks

### Adding a New Component

1. Create a directory under the appropriate module (e.g., `Svc/NewComponent/`).
2. Write the FPP model (`NewComponent.fpp`) defining ports, commands, events, telemetry.
3. Create `CMakeLists.txt` with `register_fprime_module()`.
4. Build to generate base classes, then implement the `*ComponentImpl.hpp/cpp`.
5. Write unit tests in `test/ut/`.
6. Write a `docs/sdd.md`.

### Modifying an Existing Component

1. If the interface changes, edit the `.fpp` file first.
2. Rebuild to regenerate `*Ac.*` files.
3. Update the implementation in `*ComponentImpl.cpp/hpp` to match the new interface.
4. Update or add unit tests.
5. Update `docs/sdd.md` if behavior or interfaces changed.

### Adding a Port Connection

Edit the topology `.fpp` file (e.g., `Ref/Top/topology.fpp`) to add connections between component instances.

---

## Key Files

| File | Purpose |
|---|---|
| `.clang-format` | C++ formatting rules (Chromium style, 4-space indent, 120 col) |
| `.clang-tidy` | Static analysis checks (bugprone, modernize, readability) |
| `release.clang-tidy` | Flight-code-only checks (no recursion) |
| `CPPLINT.cfg` | Cpplint configuration and filters |
| `.pre-commit-config.yaml` | Pre-commit hooks (black for Python, clang-format for C++) |
| `requirements.txt` | Python dependencies (pinned versions) |
| `settings.ini` | F Prime project settings (framework path) |
| `CMakePresets.json` | CMake presets (Release, Debug, Unit Test) |
| `CONTRIBUTING.md` | Full contribution guide |
| `AI_POLICY.md` | Generative AI usage policy |
| `GOVERNANCE.md` | Project governance and CCB process |

---

## What NOT to Do

- Do not edit auto-generated files (`*Ac.hpp`, `*Ac.cpp`, `GTestBase.*`, `TesterBase.*`).
- Do not use STL, exceptions, RTTI, `std::string`, or dynamic allocation after init.
- Do not use `FW_ASSERT` for external/untrusted inputs.
- Do not use `NULL`, `0` as null pointer, C-style casts, or `#pragma once`.
- Do not invoke CMake directly; use `fprime-util`.
- Do not push directly to `devel` or `release/*` branches.
- Do not submit PRs without an associated approved issue (the CCB may need to review first).
- Do not modify tests solely to make them pass. Flag incorrect tests instead.
