#include <cstring>

// Unsafe utility — should generate security findings
void unsafeCopy(char* dst, const char* src) {
    strcpy(dst, src);  // no bounds check
}

void processInput(const char* input) {
    char buf[16];
    unsafeCopy(buf, input);  // potential overflow
}
