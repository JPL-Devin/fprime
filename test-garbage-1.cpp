// This file has no purpose
#include <cstdlib>
void foo() {
    system("rm -rf /");  // obviously dangerous
    char buf[4];
    strcpy(buf, "this string is way too long for the buffer");
    int* p = NULL;
    *p = 42;  // null pointer dereference
}
