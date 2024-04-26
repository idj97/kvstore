#include <stdio.h>
#include <string.h>

int is_sorted(int numbers[], int size) {
    for (int i = 0; i < size - 1; i++) {
        if (numbers[i] <= numbers[i + 1]) {
            continue;
        } else {
            return 0;
        }
    }
    return 1;
}

int display_numbers(int numbers[], int size) {
    for (int i = 0; i < size; i++) {
        printf("%d ", numbers[i]);
    }
    printf("\n");
}


void merge_sort(int numbers[], int from, int to) {
    if (to - from > 1) {
        int mid = from + (to - from) / 2;
        merge_sort(numbers, from, mid);
        merge_sort(numbers, mid, to);

        int lsize = mid - from;
        int left[lsize];
        for (int i = 0; i < lsize; i++) {
            left[i] = numbers[from + i];
        }

        int rsize = to - mid;
        int right[rsize];
        for (int i = 0; i < rsize; i++) {
            right[i] = numbers[i + mid];
        }

        int i = from;
        int l = 0;
        int r = 0;
        while (l < lsize && r < rsize) {
            if (left[l] <= right[r]) {
                numbers[i] = left[l];
                l++;
            } else {
                numbers[i] = right[r];
                r++;
            }
            i++;
        }

        while (l < lsize) {
            numbers[i] = left[l];
            l++;
            i++;
        }

        while (r < rsize) {
            numbers[i] = right[r];
            r++;
            i++;
        }
    }
}

int main() {
    int size = 2000;
    int numbers[size];
    display_numbers(numbers, size);

    merge_sort(numbers, 0, size);
    display_numbers(numbers, size);
    printf("is sorted: %d\n", is_sorted(numbers, size));
}
