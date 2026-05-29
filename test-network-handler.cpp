#include <cstring>
#include <cstdio>

struct Packet {
    char header[8];
    char payload[256];
};

void handlePacket(const char* raw, size_t len) {
    Packet pkt;
    // No validation of len — could overflow both fields
    memcpy(&pkt, raw, len);
    printf("Header: %.8s\n", pkt.header);
}
