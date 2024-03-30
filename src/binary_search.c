#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#define N 100

int binary_search(int* arr, int n, int key) {
    int lo = 0;
    int hi = n;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int mid_key = arr[mid];
        if (mid_key == key) {
            return mid;
        }
        else if (mid_key < key) {
            lo = mid + 1;
        }
        else {
            hi = mid - 1;
        }
    }

    return -1;
}

int* generate_numbers() {
    srand(time(NULL));
    int* nums = malloc(sizeof(int) * N);
    for (int i = 0; i < N; i++) {
        nums[i] = rand();
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N - i; j++) {
            if (nums[j] > nums[j + 1]) {
                int t = nums[j];
                nums[j] = nums[j + 1];
                nums[j + 1] = t;
            }
        }
    }
    return nums;
}

int main() {
    int* nums = generate_numbers();
    for (int i = 0; i < N; i++) {
        int key = nums[i];
        int pos = binary_search(nums, N, key);
        printf("%d pos is %d, found pos is %d\n", key, i, pos);
        assert(pos == i);
    }
    return 0;
}