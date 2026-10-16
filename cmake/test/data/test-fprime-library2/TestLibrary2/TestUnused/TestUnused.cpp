// Library module that no project target depends on: must not be built by `all` (issue #4436)
#include <Fw/FPrimeBasicTypes.h>
namespace TestLibrary2 {
U32 unused() {
    return 42;
}
}  // namespace TestLibrary2
