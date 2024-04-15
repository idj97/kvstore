#include <stdio.h>
#include <inttypes.h>

uint32_t cnt = 0;

void alloc() {
    uint32_t before = cnt++;
    printf("before: %d, after: %d\n", before, cnt);
}

int main() {
    for (int i = 0; i < 10; i++) {
        alloc();
    }
}