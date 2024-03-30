#include <stdio.h>
#include <string.h>
#include <assert.h>

static int binCollFunc(
    int nKey1, const void* pKey1,
    int nKey2, const void* pKey2
) {
    int rc, n;
    n = nKey1 < nKey2 ? nKey1 : nKey2;
    /* EVIDENCE-OF: R-65033-28449 The built-in BINARY collation compares
    ** strings byte by byte using the memcmp() function from the standard C
    ** library. */
    assert(pKey1 && pKey2);
    rc = memcmp(pKey1, pKey2, n);
    if (rc == 0) {
        rc = nKey1 - nKey2;
    }
    return rc;
}


int main() {
    char* a = "aaa";
    char* b = "aaaBasd";
    printf("('%s' == '%s') = %d\n", a, b, binCollFunc(strlen(a), a, strlen(b), b));
    return 0;
}
