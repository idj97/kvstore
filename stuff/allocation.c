#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct FreeBlock {
    int start;
    int end;
} FreeBlock;

typedef struct Command {
    int type;
    int start;
    int end;
    int size;
} Command;

int n = 1;
int page_size = 100;
FreeBlock blocks[1000];

void show() {
    printf("n blocks: %d\n", n);
    for (int i = 0; i < n; i++) {
        printf("block %d, from: %d, to: %d\n", i, blocks[i].start, blocks[i].end);
    }
    printf("========\n");
}

Command parse(char* input) {
    char* delim = " ";
    char* op = strtok(input, delim);

    Command cmd;
    if (strcmp(op, "show") == 0) {
        cmd.type = 0;
    } else if (strcmp(op, "take") == 0) {
        cmd.type = 1;
        cmd.size = atoi(strtok(NULL, delim));
    } else if (strcmp(op, "free") == 0) {
        cmd.type = 2;
        cmd.start = atoi(strtok(NULL, delim));
        cmd.end = atoi(strtok(NULL, delim));
    } else {
        cmd.type = -1;
    }

    return cmd;
}

void insert(int start, int end, int pos) {
    assert(pos >= 0 && pos <= n);
    if (pos == n) {
        blocks[pos].end = end;
        blocks[pos].start = start;
    } else {
        for (int i = n; i > pos; i--) {
            blocks[i] = blocks[i - 1];
        }
        blocks[pos].end = end;
        blocks[pos].start = start;
    }
    n++;
}

void pop(int pos) {
    for (int i = pos; i < n - 1; i++) {
        blocks[i] = blocks[i + 1];
    }
    n--;
}

void take(int size) {
    if (size < 0) {
        printf("invalid size\n");
        return;
    }

    int rc = 1;
    for (int i = 0; i < n; i++) {
        int available = blocks[i].end - blocks[i].start + 1;
        if (available == size) {
            pop(i);
            rc = 0;
            break;
        } else if (available > size) {
            blocks[i].end -= size;
            rc = 0;
            break;
        }
    }

    if (rc == 0) {
        printf("ok\n");
    } else {
        printf("no space for block of size %d\n", size);
    }
}

void ffree(int start, int end) {
    if (start < 0 || start >= end || end > page_size - 1) {
        printf("invalid params\n");
        return;
    }

    printf("freeing (%d,%d)\n", start, end);

    // if there are no free blocks
    // just create (start, end) block
    if (n == 0) {
        blocks[0].end = end;
        blocks[0].start = start;
        n++;
        printf("first freeblock (%d,%d) created\n", start, end);
        return;
    }

    // first check if (start, end) is before first freeblock
    if (blocks[0].start == end) {
        blocks[0].start = start;
        printf("extended first freeblock\n");
        return;
    } else if (blocks[0].start > end) {
        insert(start, end, 0);
        printf("new free block prepended\n");
        return;
    } else if (blocks[n - 1].end == start) {
        blocks[n - 1].end = end;
        printf("extended last freeblock\n");
        return;
    } else if (blocks[n - 1].end < start) {
        insert(start, end, n);
        printf("new free block appended\n");
        return;
    } else {
        for (int i = 0; i < n - 1; i++) {
            FreeBlock* l = &blocks[i];     // left 
            FreeBlock* r = &blocks[i + 1]; // right

            if (l->end == start && end == r->start) {
                l->end = r->end;
                pop(i + 1);
                printf("left and right merged!\n");
                return;
            } else if (l->end == start && end < r->start) {
                l->end = start;
                printf("left extended\n");
                return;
            } else if (l->end > start && end == r->start) {
                r->start = end;
                printf("right extended\n");
                return;
            } else if (l->end < start && end < r->start) {
                insert(start, end, i + 1);
                printf("inserted new free block between left and right\n");
                return;
            } else {
                continue;
            }
        }
        printf("failed to find occupied space!\n");
    }
}

int main() {
    FreeBlock block0 = { .start = 0, .end = page_size - 1 };
    blocks[0] = block0;

    // take(50);
    // ffree(70, 80);
    // show();
    // ffree(20, 30);
    // show();
    // ffree(80, 85);
    // show();
    // ffree(95, 98);
    // show();


    while (1) {
        printf("> ");
        char input[64];
        if (fgets(input, 64, stdin) != NULL) {
            input[strlen(input) - 1] = '\0';
            Command cmd = parse(input);
            switch (cmd.type)
            {
            case -1:
                printf("invalid command\n");
                break;
            case 0:
                show();
                break;
            case 1:
                take(cmd.size);
                break;
            case 2:
                ffree(cmd.start, cmd.end);
            default:
                break;
            }
        } else {
            printf("failed to read input.\n");
        }
    }
    return 0;
}
