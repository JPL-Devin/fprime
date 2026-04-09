# FPP Codegen Changes Needed for SerialBufferBase Migration

This file tracks locations where `LinearBufferBase` or `SerializeBufferBase` (its alias) could be changed to `SerialBufferBase` but cannot be done without modifying the FPP code generator. These changes are deferred until FPP is updated.

---

## Port Infrastructure (`Fw/Port/`)

### `InputPortBase::invokeSerial(LinearBufferBase&)` 
- **File:** `Fw/Port/InputPortBase.hpp:17-18`
- **Current:** `virtual SerializeStatus invokeSerial(LinearBufferBase& buffer) = 0;`
- **Proposed:** `virtual SerializeStatus invokeSerial(SerialBufferBase& buffer) = 0;`
- **Justification:** The generated code for `invokeSerial` in typed input ports only calls `deserializeFrom()` on the buffer. It does not call `getBuffAddr()` or any pointer-returning method. This means the abstract `SerialBufferBase` interface would suffice.
- **Why deferred:** This is a base class virtual method. Changing it requires updating:
  1. The FPP code generator (which generates `invokeSerial` overrides in typed port classes)
  2. `InputSerializePort::invokeSerial()` override
  3. All FPP-generated component base classes that override this method
  4. The `CompFuncPtr` typedef in `InputSerializePort.hpp`

### `InputSerializePort::invokeSerial(LinearBufferBase&)`
- **File:** `Fw/Port/InputSerializePort.hpp:19-20`, `InputSerializePort.cpp:17`
- **Current:** `SerializeStatus invokeSerial(LinearBufferBase& buffer) override;`
- **Proposed:** `SerializeStatus invokeSerial(SerialBufferBase& buffer) override;`
- **Justification:** Implementation only calls `this->m_func(this->m_comp, this->m_portNum, buffer)` — passes buffer through without calling pointer methods. However, the `CompFuncPtr` typedef on line 22-24 also uses `LinearBufferBase&`.
- **Why deferred:** Must match `InputPortBase::invokeSerial()` virtual signature and the FPP-generated callback signatures.

### `InputSerializePort::CompFuncPtr`
- **File:** `Fw/Port/InputSerializePort.hpp:22-24`
- **Current:** `typedef void (*CompFuncPtr)(Fw::PassiveComponentBase* callComp, FwIndexType portNum, LinearBufferBase& arg);`
- **Proposed:** Change `LinearBufferBase&` to `SerialBufferBase&`
- **Why deferred:** FPP generates the actual callback functions that match this typedef.

### `OutputPortBase::invokeSerial(SerializeBufferBase&)`
- **File:** `Fw/Port/OutputPortBase.hpp:15-16`, `OutputPortBase.cpp:30`
- **Current:** `SerializeStatus invokeSerial(SerializeBufferBase& buffer);`
- **Proposed:** `SerializeStatus invokeSerial(SerialBufferBase& buffer);`
- **Justification:** Implementation only calls `this->m_serPort->invokeSerial(buffer)` — just forwards the buffer. No pointer access.
- **Why deferred:** The parameter must be compatible with `InputPortBase::invokeSerial()` which is part of the FPP-generated chain.

## FPP-Generated Serial Port Handlers

### `serialIn_handler` / `serialOut_handler` signatures
- **Example:** `Svc/GenericHub/GenericHub.hpp:119-121`
  ```cpp
  void serialIn_handler(FwIndexType portNum, Fw::SerializeBufferBase& Buffer) override;
  ```
- **Note:** These handler signatures are dictated by the FPP-generated base class (`GenericHubComponentBase`). The base class declares the virtual method with `SerializeBufferBase&` (or `LinearBufferBase&`), and the user's implementation must match.
- **Justification for change:** Some implementations (like `GenericHub::serialIn_handler`) call `Buffer.getBuffAddr()` so they genuinely need `LinearBufferBase&`. Others may only use serialize API. But regardless, the signature is controlled by FPP codegen.
- **Why deferred:** Changing these requires updating the FPP code generator's serial port handler templates.

### `from_serialOut_handler` in test harnesses
- **Example:** `Svc/GenericHub/test/ut/GenericHubTester.hpp:125-127`
  ```cpp
  void from_serialOut_handler(FwIndexType portNum, Fw::SerializeBufferBase& Buffer);
  ```
- **Note:** This handler's signature must match the FPP-generated GTest base class.
- **Why deferred:** Same as above — FPP codegen controls the base class signature.

---

## Summary

| Location | Current Type | Could Be | Blocked By |
|----------|-------------|----------|------------|
| `InputPortBase::invokeSerial()` | `LinearBufferBase&` | `SerialBufferBase&` | FPP codegen generates overrides |
| `InputSerializePort::invokeSerial()` | `LinearBufferBase&` | `SerialBufferBase&` | Must match base class virtual |
| `InputSerializePort::CompFuncPtr` | `LinearBufferBase&` | `SerialBufferBase&` | FPP codegen generates callbacks |
| `OutputPortBase::invokeSerial()` | `SerializeBufferBase&` | `SerialBufferBase&` | Must be compatible with input chain |
| `*_handler` serial port handlers | `SerializeBufferBase&` | Case-by-case | FPP codegen generates base class |
| `from_*_handler` in GTest bases | `SerializeBufferBase&` | Case-by-case | FPP codegen generates GTest base |

**Total impact:** Updating FPP codegen would unlock changes to the entire serial port infrastructure and all components that use serial ports.
