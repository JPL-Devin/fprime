# F´ Component Author skill (canonical)

You are an F Prime component author. Your job is to create a new F´ component
end-to-end: FPP model, autocoder regeneration, C++ implementation, unit-test
scaffold, and SDD stub.

This skill is the canonical, tool-independent version. It is referenced by the
repo-root [`AGENTS.md`](../AGENTS.md) and by tool-specific wrappers under
[`../.github/agents/`](../.github/agents), [`../.cursor/rules/`](../.cursor/rules),
[`../.windsurf/rules/`](../.windsurf/rules), and [`../CLAUDE.md`](../CLAUDE.md).

## When to use

The user wants to add a brand-new F´ component (active or passive) to a
deployment in this repository or a project built on top of it. Examples:

- "Add a new `Svc::FooBar` passive component that exposes a command and a
  telemetry channel."
- "Create an active `Drv::WidgetDriver` with a rate-group input."

Do **not** use this skill when:

- The user is editing an existing component (use the regular code-review
  skill).
- The user is wiring components into a topology (different concern).
- The user wants a Drv driver tied to specific hardware (still applies, but
  pull in `docs/how-to/develop-device-driver.md` as additional context).

## What to produce

1. An FPP model file `<Component>.fpp` declaring the component, its ports,
   commands, events, telemetry channels, and parameters using **FPP modeled
   types** and **configurable `Fw*` types** wherever applicable.
2. A `CMakeLists.txt` (or appropriate `Subdirectory.cmake`) entry registering
   the component module.
3. The autocoder output produced by running:
   ```bash
   fprime-util generate
   fprime-util impl
   ```
   You do not hand-edit anything under `build-artifacts/` or
   `*ComponentAc.{hpp,cpp}`. You operate on the generated `<Component>Impl.hpp`
   and `<Component>Impl.cpp` stubs.
4. A C++ implementation in `<Component>.hpp` / `<Component>.cpp` (or the
   `*Impl` files for the F´ versioning in use) that:
   - Implements every handler declared in FPP.
   - Initializes all members in the constructor or in `init()`.
   - Transfers ownership of any `Fw::Buffer` it accepts (return it to sender
     or pass it onward in **every** branch).
   - Uses `FW_ASSERT` **only** for programming-error checks; never for
     untrusted/external input.
   - Reports failures via events/telemetry, not by throwing or aborting.
5. A unit-test scaffold under `test/ut/` using gtest + `STest` that:
   - Builds via `fprime-util check`.
   - Covers every command handler, every port input, and every event/channel.
   - Tests rate-group ticks for active components.
   - See [`unit-test-author.md`](unit-test-author.md) for detail.
6. An SDD stub at `docs/sdd.md` for the new component, following the F´ SDD
   template (purpose, requirements, design, ports, commands, telemetry,
   events, parameters, state, references).

## Procedure

1. Confirm with the user (or, if obvious from the request, state explicitly)
   the component name, namespace, kind (`active` / `passive` / `queued`),
   inputs/outputs (ports), and any commands/events/telemetry/parameters.
2. Choose the directory by convention: framework primitives go under `Fw/`,
   standard services under `Svc/`, drivers under `Drv/`, deployment-specific
   components under the deployment directory (for example `Ref/`).
3. Write the FPP model. Use the existing components nearest to the request as
   examples (for example `Svc/CmdSequencer` for an active component with rich
   state, `Svc/Health` for a periodic passive checker).
4. Add the component to its parent CMakeLists.
5. Run `fprime-util generate` then `fprime-util impl` from the deployment that
   uses the component. Move the produced `*Impl.hpp.template` and
   `*Impl.cpp.template` files into place (strip the `.template`).
6. Implement the handler bodies. Stay within the 32 review rules in
   [`code-review.md`](code-review.md). In particular:
   - No dynamic allocation after init.
   - No exceptions, RTTI, `std::string`, STL.
   - C++14 only.
   - `nullptr` only; no `NULL`/`0`.
   - No lambdas. Simple templates are OK.
   - No C-style casts; avoid `reinterpret_cast`/`const_cast`.
   - Prefer `Fw::String` over `char*`.
7. Write the unit tests using `unit-test-author.md` as the guide.
8. Run `fprime-util build` then `fprime-util check`. Fix all failures.
9. Run `fprime-util format` on the new files (or use the pre-commit hook).
10. Write `docs/sdd.md` for the new component. Cross-link it from `.nav.yml`
    if appropriate.

## Constraints

- Never edit autocoder output. If `*ComponentAc.{hpp,cpp}` looks wrong,
  the **FPP model** is wrong; fix that and regenerate.
- Do not introduce platform-specific code in framework-level components.
  Platform abstraction belongs under `Os/` or `Drv/`.
- New ground-facing interfaces (commands, events, telemetry, parameters) use
  **FPP modeled types**, not bare C/C++ types.
- Active components must be safe to stop; do not leak buffers on shutdown.
- Follow the PR & disclosure rules in [`../AGENTS.md`](../AGENTS.md#6-pr--disclosure-rules-mandatory-for-ai-assisted-contributions)
  and [`../AI_POLICY.md`](../AI_POLICY.md). Non-trivial new components need an
  approved CCB issue before merge.

## Output Format

When operating in an interactive coding tool, produce the files in this order
so the user can review each step:

1. The FPP model.
2. CMake registration.
3. The shell commands the user should run (`fprime-util generate / impl`).
4. The hand-edited `<Component>.cpp` / `<Component>.hpp` implementation.
5. The unit tests.
6. The SDD stub.
7. A short checklist of follow-ups (topology wiring, integration tests, doc
   updates).
