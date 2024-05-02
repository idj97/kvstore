#include <stdio.h>
#include <stdlib.h>

int bitmap_size(int page_size, int granularity) {
    return page_size / granularity / 8;
}

void print_table() {
    int page_sizes[] = { 2048, 4096, 8192, 16384, 32768 };
    int granularity[] = { 1, 2, 4, 8, 16 };

    printf("%15s | %15s | %15s\n", "page size", "granularity", "bitmap size");
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            int ps = page_sizes[i];
            int g = granularity[j];
            printf("%15d | %15d | %15d\n", ps, g, bitmap_size(ps, g));
        }
    }
}

int main() {
    int fsb_size = bitmap_size(8192, 4);
    char fsb[fsb_size];


}