#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t

#ifdef TEST
#define PAGE_SIZE 128
#else
#define PAGE_SIZE 512
#endif

#define PAGE_HDR_SIZE 16
#define PAGE_CELL_PTR_SIZE 12

typedef struct Value {
    const void* data;
    u32 size;
} Value;

typedef struct BTPageHdr {
    u32 pid;                 // 4
    u32 rightmost_pid;       // 4
    u16 cell_count;          // 2
    u16 freespace_end;       // 2
    u8  is_leaf;             // 1
    int : 24;                // 3
} BTPageHdr;

typedef struct BTCellPtr {
    u32 key_size;          // 4
    u32 data_size;         // 4
    u16 offset;            // 2
    int : 16;              // 2
} BTCellPtr;

typedef struct BTree {
    u32 root_page_id;
    int (*cmp)(const void*, u32, const void*, u32);
} BTree;

typedef struct BTPage {
    BTPageHdr* hdr;
    BTCellPtr* cell_ptrs;
    char* pdata;
    BTree* btree;
} BTPage;

typedef enum BTPageSetStatus {
    Ok,
    NotEnoughSpace,
    PayloadTooBig
} BTPageSetStatus;

typedef struct BTPageSplitResult {
    int status;
    BTPage* page;
    BTPage* new_page;
} BTPageSplitResult;

u32 page_counter = 0;
BTPage* buffer[100];

int compare_integers(const void* a, u32 a_sz, const void* b, u32 b_sz) {
    return *(int*)a - *(int*)b;
}

BTPage* page_new(BTree* btree) {
    char* pdata = calloc(1, PAGE_SIZE);
    BTPage* page = calloc(1, sizeof(BTPage));
    page->hdr = (BTPageHdr*)pdata;
    page->btree = btree;
    page->hdr->pid = page_counter++;
    page->hdr->freespace_end = PAGE_SIZE;
    page->cell_ptrs = (BTCellPtr*)(pdata + PAGE_HDR_SIZE);
    page->pdata = pdata;
    buffer[page->hdr->pid] = page;
    return page;
}

BTPage* page_blank() {
    char* pdata = calloc(1, PAGE_SIZE);
    BTPage* page = calloc(1, sizeof(BTPage));
    page->hdr = (BTPageHdr*)pdata;
    page->hdr->freespace_end = PAGE_SIZE;
    page->cell_ptrs = (BTCellPtr*)(pdata + PAGE_HDR_SIZE);
    page->pdata = pdata;
    return page;
}

void page_destroy(BTPage* page) {
    free(page->pdata);
    free(page);
}

u16 page_freespace(BTPage* page) {
    return page->hdr->freespace_end - (PAGE_HDR_SIZE + (PAGE_CELL_PTR_SIZE * page->hdr->cell_count));
}

BTCellPtr* page_cellptr_at(BTPage* page, u16 pos) {
    return page->cell_ptrs + pos;
}

Value page_key_at(BTPage* page, u16 pos) {
    BTCellPtr* cellptr = page_cellptr_at(page, pos);
    Value v = {
        .size = cellptr->key_size,
        .data = page->pdata + cellptr->offset
    };
    return v;
}

Value page_data_at(BTPage* page, u16 pos) {
    BTCellPtr* cellptr = page_cellptr_at(page, pos);
    Value v = {
        .size = cellptr->data_size,
        .data = page->pdata + cellptr->offset + cellptr->key_size
    };
    return v;
}

BTCellPtr* page_find_cellptr(BTPage* page, const void* key, u32 key_size) {
    if (page->hdr->cell_count == 0) {
        return NULL;
    }

    BTCellPtr* mid_cell;
    char* mid_cell_key;
    int lo = 0;
    int hi = page->hdr->cell_count - 1;
    int mid;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        mid_cell = page_cellptr_at(page, mid);
        mid_cell_key = &page->pdata[mid_cell->offset];

        int rcmp = page->btree->cmp(
            key, key_size,
            mid_cell_key, mid_cell->key_size
        );

        if (rcmp == 0) {
            return mid_cell;
        } else if (rcmp > 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return NULL;
}

u16 page_insertion_point(BTPage* page, const void* key, u32 key_size) {
    if (page->hdr->cell_count == 0) {
        return 0;
    }

    BTCellPtr* cell;
    char* cell_key;
    u16 lo = 0;
    u16 hi = page->hdr->cell_count;
    u16 mid;

    while (lo < hi) {
        mid = (lo + hi) / 2;
        cell = page_cellptr_at(page, mid);
        cell_key = &page->pdata[cell->offset];

        int cmp_res = page->btree->cmp(
            key, key_size,
            cell_key, cell->key_size
        );

        if (cmp_res > 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

void page_move_cells(BTPage* page, u16 from) {
    BTCellPtr* src = page_cellptr_at(page, from);
    BTCellPtr* dest = src + 1;
    memmove(dest, src, PAGE_CELL_PTR_SIZE * (page->hdr->cell_count - from));
}

int page_leaf_insert(
    BTPage* page,
    const void* key, u32 key_size,
    const void* data, u32 data_size
) {

    u16 required_space = key_size + data_size + PAGE_CELL_PTR_SIZE;
    if (required_space > (PAGE_SIZE - PAGE_HDR_SIZE) / 4) {
        return PayloadTooBig;
    }

    u16 data_offset;
    u16 key_offset;
    BTCellPtr* cellptr = page_find_cellptr(page, key, key_size);

    if (cellptr == NULL) {
        // key to insert doesn't exist in current page

        u16 freespace = page_freespace(page);
        if (freespace < required_space) {
            printf("not enough free space, have: %d required: %d\n",
                freespace, required_space
            );
            return NotEnoughSpace;
        } else {
            // okay, there is enough space for both cellptr and payload
            // find insertion point for cellptr 
            u16 ins_point = page_insertion_point(page, key, key_size);
            cellptr = page_cellptr_at(page, ins_point);

            // if new cell should be inserted somewhere within 
            // existing cells then move other cells to the right to make space 
            if (ins_point < page->hdr->cell_count) {
                page_move_cells(page, ins_point);
            }

            // calculate byte offsets for key and data payload
            data_offset = page->hdr->freespace_end - data_size;
            key_offset = data_offset - key_size;

            // update page hdr
            page->hdr->cell_count++;
            page->hdr->freespace_end = key_offset;
        }
    } else {
        // overwrite
        u16 curr_size = cellptr->key_size + cellptr->data_size;
        u16 new_size = key_size + data_size;

        if (new_size <= curr_size) {
            // new payload can fit within the space old payload takes
            // just overwrite it 
            key_offset = cellptr->offset;
            data_offset = cellptr->offset + cellptr->key_size;
        } else {
            return NotEnoughSpace;
        }
    }

    // copy payload into data section
    memcpy(page->pdata + data_offset, data, data_size);
    memcpy(page->pdata + key_offset, key, key_size);

    // update cellptr
    cellptr->key_size = key_size;
    cellptr->data_size = data_size;
    cellptr->offset = key_offset;

    return Ok;
}

int page_find_splitpoint(BTPage* page) {
    assert(page->hdr->cell_count > 0);

    int bytes_to_take = (PAGE_SIZE - PAGE_HDR_SIZE) / 2;
    int taken = 0;

    for (int i = 0; i < page->hdr->cell_count; i++) {
        BTCellPtr* cellptr = page_cellptr_at(page, i);
        int sz = PAGE_CELL_PTR_SIZE + cellptr->data_size + cellptr->key_size;
        taken += sz;
        if (taken > bytes_to_take) {
            return i;
        }
    }

    return -1;
}

BTPageSplitResult page_leaf_split(BTPage* page) {
    assert(page->hdr->is_leaf == 1);

    BTPage* left = page_blank();
    left->hdr->is_leaf = page->hdr->is_leaf;
    left->hdr->pid = page->hdr->pid;

    BTPage* right = page_new(page->btree);
    right->hdr->is_leaf = page->hdr->is_leaf;

    int splitpoint = page_find_splitpoint(page);


    // copy first half of cells and their payloads to 'left' page
    for (int i = 0; i < splitpoint; i++) {

        // get source and destination cell pointers
        BTCellPtr* src_cellptr = page_cellptr_at(page, i);
        BTCellPtr* dest_cellptr = page_cellptr_at(left, i);

        // update metadata of left page
        int payload_size = src_cellptr->key_size + src_cellptr->data_size;
        left->hdr->freespace_end -= payload_size;
        left->hdr->cell_count++;

        // update destination cell pointer metadata
        dest_cellptr->offset = left->hdr->freespace_end;
        dest_cellptr->data_size = src_cellptr->data_size;
        dest_cellptr->key_size = src_cellptr->key_size;

        // copy key and data payload
        memcpy(
            left->pdata + dest_cellptr->offset,
            page->pdata + src_cellptr->offset,
            payload_size
        );
    }

    // copy second half of cells and their payloads to 'right' page
    int right_cell_pos = 0;
    for (int i = splitpoint; i < page->hdr->cell_count; i++) {
        // get source and destination cell pointers
        BTCellPtr* src_cellptr = page_cellptr_at(page, i);
        BTCellPtr* dest_cellptr = page_cellptr_at(right, right_cell_pos);

        // update right page metadata
        int payload_size = src_cellptr->key_size + src_cellptr->data_size;
        right->hdr->freespace_end -= payload_size;
        right->hdr->cell_count++;

        // update destination cell pointer metadata
        dest_cellptr->offset = right->hdr->freespace_end;
        dest_cellptr->data_size = src_cellptr->data_size;
        dest_cellptr->key_size = src_cellptr->key_size;

        // copy data
        memcpy(
            right->pdata + dest_cellptr->offset,
            page->pdata + src_cellptr->offset,
            payload_size
        );

        // advence cellptr position of right page
        right_cell_pos++;
    }

    // here we copy the contents of left page (helper struct) into original page
    memcpy(page->pdata, left->pdata, PAGE_SIZE);
    page_destroy(left);

    return (BTPageSplitResult) {
        .status = Ok, .page = page, .new_page = right
    };
}

BTree* btree_new(int (*cmp)(const void*, u32, const void*, u32)) {
    BTree* btree = malloc(sizeof(BTree));
    BTPage* root_page = page_new(btree);
    root_page->hdr->is_leaf = 1;

    u32 root_page_id = root_page->hdr->pid;
    buffer[root_page_id] = root_page;

    btree->root_page_id = root_page_id;
    btree->cmp = cmp;
    return btree;
}

void btree_destroy(BTree* btree) {
    free(btree);
}

void reset_buffer() {
    for (int i = 0; i < 100; i++) {
        if (buffer[i] != NULL) {
            page_destroy(buffer[i]);
        }
        buffer[i] = NULL;
    }
    page_counter = 0;
}
