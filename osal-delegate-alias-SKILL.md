---
name: osal-delegate-alias
description: "Make an F Prime OSAL abstraction (Os::X) selectable at compile time. Split X into an interface header, a thin aggregate public header, and the link-time delegate; add a per-class config header (config/OsDelegateX.hpp) that both aliases Os::X and names the defining header via an OS_X_HEADER macro. Default keeps the link-time delegate (Os::DelegateX); a platform overrides the config to alias Os::X directly to a concrete impl (no virtual dispatch). Applies to RawTime, Task, File, Mutex, Queue, Console, Directory, FileSystem, CountingSemaphore, Condition, Cpu, Memory. Interactive: stops after each phase for the user to build."
disable-model-invocation: true
---



## FIRST Look at  osal-delegate-alias-SKILL.md for whether the class being converted is stand alone or intertwined 

# OSAL X: compile-time-selectable implementation

Goal: let a platform pick the `Os::X` implementation at **compile time** while the
default still selects it at **link time** (the existing delegate mechanism).

The mechanism is a per-class config header, `config/OsDelegateX.hpp`, that does
two things:

1. Defines the alias `namespace Os { using X = <chosen type>; }`.
2. Defines a macro `#define OS_X_HEADER <path>` naming the header that provides
   the complete definition of `<chosen type>`.

- **Default** (link-time delegate): alias `X = Os::DelegateX`, and
  `OS_X_HEADER = <Os/DelegateX.hpp>`.
- **Platform override** (compile-time): alias `X = <ConcreteX>` and
  `OS_X_HEADER = "<path to concrete header>"`.

`RawTime` is done; use it as the reference. Do the rest by replicating it.

## Mode

At each **CHECKPOINT** , verify the following commands work. If any fail, investigate (and keep a record of) why
fprime-util generate -f && fprime-util build 
fprime-util generate -f --ut && fprime-util check  
cd TestDeploymentsProject/Ref && fprime-util generate -f && fprime-util build  


When the reference (`RawTime`) and this document disagree, defer to the actual `RawTime`
files; if still unsure, ask.



## Reference file set (RawTime, in `lib/fprime`)

- `Os/RawTimeInterface.hpp` — new; holds `RawTimeHandle` + `RawTimeInterface`.
- `Os/RawTime.hpp` — rewritten; thin aggregate (interface + `OS_RAW_TIME_HEADER`).
- `Os/DelegateRawTime.hpp` — renamed from old `Os/RawTime.hpp`'s delegate class.
- `Os/DelegateRawTime.cpp` — renamed from old `Os/RawTime.cpp`.
- `default/config/OsDelegateRawTime.hpp` — new config header (alias + macro).
- `Os/CMakeLists.txt`, `default/config/CMakeLists.txt` — build wiring.
- Platform overrides live in this repo's `config-scythe/` (see Step 6):
  `config-scythe/linux-gcc/OsDelegateRawTime.hpp` (link-time delegate),
  `config-scythe/vorago/OsDelegateRawTime.hpp` (compile-time concrete alias),
  and `config-scythe/CMakeLists.txt` selecting between them. The concrete impl
  header/source live in the platform repo (`lib/fprime-vorago`).

## Rules

- Do **not** change `XInterface`'s primitives, the `XInterface::getDelegate(...)`
  factory, or the `XHandleStorage` typedef. Concrete `DefaultXImpl.cpp` files
  need no edits.
- Every virtual-method signature keeps referencing the `X` alias (e.g.
  `const Os::X& other`); only a forward declaration of the aliased type is needed
  because interfaces reference it solely through references to an incomplete type.
- The config header contains **only** forward declarations, the `using` alias,
  and the `OS_X_HEADER` macro. It must **not** include any `Os/` header — it is
  included before those headers are complete, so including them forms a cycle.
- Include guard in the config header is `CONFIG_OS_DELEGATEX_HPP`; the platform
  override reuses the exact same guard so it shadows the base cleanly.

## Step 1 — Split the old `Os/X.hpp` into three headers

The old `Os/X.hpp` held both `XInterface` (+`XHandle`) and the delegate class
`class X final`. Split it:

1. **`Os/XInterface.hpp`** (new): move `XHandle` and `XInterface` here. At the
   top include `#include "config/OsDelegateX.hpp"` (for the `X` alias used in
   signatures) plus the primitives it already needed (basic types, `Os/Os.hpp`,
   `Fw/...`). Keep helper methods (see Step 3) declared here as **non-pure**
   virtuals.
2. **`Os/DelegateX.hpp`** (renamed from old `Os/X.hpp`): `#include
   "Os/XInterface.hpp"`; rename `class X final` -> `class DelegateX final :
   public XInterface`, renaming only its special members (class decl, ctor, dtor,
   copy-ctor, copy-assign). Keep every method signature referencing the `X`
   alias. New include guard `OS_DELEGATEX_HPP_`.
3. **`Os/X.hpp`** (rewritten): a thin aggregate that pulls the full public API:

       #include "Os/XInterface.hpp"
       #include OS_X_HEADER

   Include order is load-bearing: interface first (defines `XInterface`), then
   `OS_X_HEADER` (the complete aliased type). Document this in the header (see
   `Os/RawTime.hpp` for the canonical comment).

Rename the `.cpp`: `Os/X.cpp` -> `Os/DelegateX.cpp`; rename each `X::` qualifier
on delegate members to `DelegateX::`; leave `XInterface::` definitions as-is; fix
the self-include to `#include <Os/DelegateX.hpp>` and add `#include
<Os/X.hpp>` where the alias is needed.

## Step 2 — Add the default config header

Create `default/config/OsDelegateX.hpp`:

    #ifndef CONFIG_OS_DELEGATEX_HPP
    #define CONFIG_OS_DELEGATEX_HPP
    namespace Os {
    class DelegateX;
    using X = DelegateX;
    }  // namespace Os
    #define OS_X_HEADER <Os/DelegateX.hpp>
    #endif  // CONFIG_OS_DELEGATEX_HPP

Register it in `default/config/CMakeLists.txt` under `register_fprime_config(...
HEADERS ...)` (alphabetical with the other config headers).

## Step 3 — Delegate is pure pass-through; helpers live in the interface

- Every `DelegateX` method (including derived helpers like `getDiffUsec`,
  `operator==`) is a pass-through to `m_delegate` (with the existing
  `FW_ASSERT(&m_delegate == reinterpret_cast<...>(&m_handle_storage[0]))` guard).
- Implement the helper methods (`getDiffUsec`, `operator==`, ...) as **non-pure
  virtuals on `XInterface`**, built from the interface primitives. Concrete impls
  inherit them; they do not reimplement them. (For `RawTime`, `operator==` was
  promoted from a delegate-only method to an `XInterface` virtual.)

## Step 4 — Fix CMake registration for X

`add_named_os_module(X ...)` assumes one matching `X.cpp` + `X.hpp`. That no
longer holds (thin `X.hpp` has no `.cpp`; the code lives in `DelegateX.{hpp,cpp}`
+ `XInterface.hpp`). Replace that line in `Os/CMakeLists.txt` with an explicit
registration:

    register_fprime_module(
          "Os_X"
        REQUIRES_IMPLEMENTATIONS
          "Os_X"
        SOURCES
          "${CMAKE_CURRENT_LIST_DIR}/DelegateX.cpp"
        HEADERS
          "${CMAKE_CURRENT_LIST_DIR}/X.hpp"
          "${CMAKE_CURRENT_LIST_DIR}/DelegateX.hpp"
        DEPENDS
          Fw_Time
          Fw_Types
          <extra deps the old add_named_os_module line passed, e.g. Fw_Buffer>
    )
    fprime_target_dependencies(Os PUBLIC "Os_X")

(`XInterface.hpp` is pulled in transitively; the reference did not list it under
HEADERS.)

## Step 5 — Fix code that reaches through the handle

### The failure

Any `XInterface` subclass (incl. test delegates) that inspects **another** `Os::X`
object via `getHandle()` + `reinterpret_cast<SomeConcreteHandle*>` assumes it
knows the concrete type behind `Os::X`. That assumption is exactly what this
change breaks: once `Os::X` can alias a *different* concrete impl, the cast reads
the wrong handle layout (UB / wrong results). Grep for `getHandle()` and
`reinterpret_cast<...Handle` across `Os/`, its tests, and each platform impl, and
audit every hit that casts an object it did not itself create.

### What worked for RawTime (do not assume it generalizes)

`Svc/OsTime/test/RawTimeTester/RawTimeTester.hpp::getTimeInterval` was rewritten
to serialize `other` into a `Fw::SerialBuffer` (sized
`Os::RawTimeInterface::SERIALIZED_SIZE`) and deserialize a `Fw::Time`, instead of
casting `other.getHandle()`. That works **only because** `RawTime`'s public
interface fully exposes the state the caller needed (serialize/deserialize round-
trips the time), and both sides agree on the serialized form.

That is a `RawTime`-specific fact, not a rule. For a different class the
serialized/public surface may not carry the information the handle-reaching code
was after, so serialize/deserialize may be impossible or lossy.

### How to fix it for an arbitrary class

For each handle-reaching site, work through this in order and pick the first that
applies:

1. **Prefer the public interface.** Can the needed value be obtained via existing
   `XInterface` virtuals (compare/serialize/query methods) without touching the
   handle? If yes, use them (the RawTime case).
2. **Add a public accessor to `XInterface`.** If the information is legitimately
   part of the abstraction but not yet exposed, add a small non-pure/virtual
   accessor to `XInterface` (Step 3 style) and route through it. Prefer this over
   any cast.
3. **Only cast when the concrete type is guaranteed.** A `reinterpret_cast` of
   `getHandle()` is acceptable **only** in code that is itself the concrete impl
   (i.e. `this` and `other` are known to be the same concrete type) — never in
   generic/delegate/test code that runs against a configurable `Os::X`. If a test
   genuinely needs impl internals, have it construct and hold the concrete type
   directly (not through the `Os::X` alias) so the layout is known.
4. **If none of the above work, STOP and ask.** Report the site, what state it
   needs, and why the public API can't supply it. Do not weaken or delete the
   test to make it compile.

Note: the old plan's broad "audit every `getDelegate` caller" was **not** the
issue for RawTime — the getDelegate factory itself is unchanged. The real audit
target is handle-reaching casts (above). Do that narrow audit first; widen only
if a build/behavior failure points elsewhere.

## CHECKPOINT — build the default path

After modifying each class and verifying the build commands succeed, STOP 
