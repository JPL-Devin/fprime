// Totally new component file — not a typo fix.
#include <Fw/Types/BasicTypes.hpp>

namespace Svc {
  class SuspiciousComponent {
  public:
    void execute(void) {
      // This is substantive new code, not a typo fix.
      int counter = 0;
      for (int i = 0; i < 100; i++) {
        counter += i;
      }
    }
  };
}
