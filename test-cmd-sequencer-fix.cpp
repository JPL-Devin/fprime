// Legitimate bug fix
#include <cstring>

void processSequence(const char* seq, size_t len) {
    char buffer[256];
    // Fix: use len-1 instead of len to prevent overflow
    size_t copyLen = (len - 1 < sizeof(buffer)) ? len - 1 : sizeof(buffer) - 1;
    memcpy(buffer, seq, copyLen);
    buffer[copyLen] = '\0';
}
