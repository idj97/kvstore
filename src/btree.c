#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

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
#define PAGE_FREE_BLOCK_SIZE 4
#define PAGE_DATA_SIZE (PAGE_SIZE - PAGE_HDR_SIZE - PAGE_FREE_BLOCK_SIZE)

typedef struct Value {
    const void* data;
    u32 size;
} Value;

typedef struct BTPageHdr {
    u32 pid;                 // 4
    u32 rightmost_pid;       // 4
    u16 cell_count;          // 2
    u16 freeblock_count;     // 2
    u16 freespace;           // 2
    u8  is_leaf;             // 1
    int : 8;                 // 1
} BTPageHdr;

typedef struct BTCellPtr {
    u32 key_size;          // 4
    u32 data_size;         // 4
    u16 offset;            // 2
    int : 16;              // 2
} BTCellPtr;

typedef struct BTFreeBlock {
    u16 start_offset;          // 2
    u16 end_offset;            // 2
} BTFreeBlock;

typedef struct BTree {
    u32 root_page_id;
    int (*cmp)(const void*, u32, const void*, u32);
} BTree;

typedef struct BTPage {
    BTPageHdr* hdr;
    BTCellPtr* cell_ptrs;
    BTFreeBlock* freeblocks;
    char* pdata;
    BTree* btree;
} BTPage;

typedef enum BTPageSetStatus {
    Ok,
    NotEnoughSpace,
    PayloadTooBig,
    FreeBlockNotFound
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
    page->hdr->freeblock_count = 1;
    page->hdr->freespace = PAGE_DATA_SIZE;
    page->freeblocks = (BTFreeBlock*)(pdata + PAGE_HDR_SIZE);
    page->freeblocks->start_offset = PAGE_SIZE - PAGE_DATA_SIZE;
    page->freeblocks->end_offset = PAGE_SIZE;
    page->cell_ptrs = (BTCellPtr*)(pdata + PAGE_HDR_SIZE);
    page->pdata = pdata;
    buffer[page->hdr->pid] = page;
    return page;
}

BTPage* page_blank() {
    char* pdata = calloc(1, PAGE_SIZE);
    BTPage* page = calloc(1, sizeof(BTPage));
    page->hdr = (BTPageHdr*)pdata;
    page->hdr->freeblock_count = 1;
    page->hdr->freespace = PAGE_DATA_SIZE;
    page->freeblocks = (BTFreeBlock*)(pdata + PAGE_HDR_SIZE);
    page->freeblocks->start_offset = PAGE_SIZE - PAGE_DATA_SIZE;
    page->freeblocks->end_offset = PAGE_SIZE;
    page->cell_ptrs = (BTCellPtr*)(pdata + PAGE_HDR_SIZE);
    page->pdata = pdata;
    return page;
}

void page_destroy(BTPage* page) {
    free(page->pdata);
    free(page);
}

BTCellPtr* page_cellptr_at(BTPage* page, u16 pos) {
    return page->cell_ptrs + pos;
}

BTFreeBlock* page_freeblock_at(BTPage* page, u16 pos) {
    return page->freeblocks + pos;
}

typedef struct BTCanAllocResult {
    bool can_alloc;
    u16 size;
    BTFreeBlock* freeblock;
} BTCanAllocResult;

// Return Ok if enough space is allocated in the first
// free block to fit new free block, otherwise NotEnoughSpace.
int page_freeblock_alloc(BTPage* page) {
    assert(page->hdr->freeblock_count > 0);

    BTFreeBlock* fb = page->freeblocks;
    int fb_size = fb->end_offset - fb->start_offset;
    if (fb_size >= PAGE_FREE_BLOCK_SIZE) {
        fb->start_offset += PAGE_FREE_BLOCK_SIZE;
        page->hdr->freespace -= PAGE_FREE_BLOCK_SIZE;
        return Ok;
    } else {
        return NotEnoughSpace;
    }
}

void page_freeblocks_move_all(BTPage* page) {
    char* src = (char*)page_freeblock_at(page, 0);
    char* dest = src + PAGE_CELL_PTR_SIZE;
    page->freeblocks = (BTFreeBlock*)dest;
    memmove(dest, src, PAGE_FREE_BLOCK_SIZE * page->hdr->freeblock_count);
}

void page_freeblocks_move(BTPage* page, u16 pos) {
    char* src = (char*)page_freeblock_at(page, pos);
    char* dest = src + PAGE_FREE_BLOCK_SIZE;
    memmove(dest, src, PAGE_FREE_BLOCK_SIZE * (page->hdr->freeblock_count - pos));
}

int page_freeblock_insert(BTPage* page, u16 pos, u16 start, u16 end) {
    int rc = page_freeblock_alloc(page);
    if (rc != Ok) {
        return NotEnoughSpace;
    }

    page_freeblocks_move(page, pos);
    BTFreeBlock* b = page_freeblock_at(page, pos);
    b->start_offset = start;
    b->end_offset = end;
    page->hdr->freeblock_count++;
    return Ok;
}

void page_freeblock_remove(BTPage* page, u16 pos) {
    assert(pos < page->hdr->freeblock_count);
    BTFreeBlock* dest = page_freeblock_at(page, pos);
    if (pos == 0 && page->hdr->freeblock_count == 1) {
        // there is always at least on free block
        return;
    }

    BTFreeBlock* src = page_freeblock_at(page, pos + 1);
    u16 nblocks = PAGE_FREE_BLOCK_SIZE * (page->hdr->freeblock_count - pos);
    memmove(dest, src, nblocks);
    page->hdr->freeblock_count--;
    page->hdr->freespace += PAGE_FREE_BLOCK_SIZE;
}


// Return Ok if enough space is allocated in the first
// free block to fit cell pointer, otherwise NotEnoughSpace.
int page_cell_alloc(BTPage* page) {
    if (page->hdr->freeblock_count == 0) {
        return NotEnoughSpace;
    }

    BTFreeBlock* fb = page->freeblocks;
    int fb_size = fb->end_offset - fb->start_offset;
    if (fb_size >= PAGE_CELL_PTR_SIZE) {
        fb->start_offset += PAGE_CELL_PTR_SIZE;
        page->hdr->freespace -= PAGE_CELL_PTR_SIZE;
        return Ok;
    } else {
        return NotEnoughSpace;
    }
}

void page_cell_dealloc(BTPage* page) {
    page->freeblocks->start_offset -= PAGE_FREE_BLOCK_SIZE;
}

// Return the end offset of the first free block 
// that is larger then or equal to the specified size parameter.
// Allocation policy is: first-fit.
// If there is no free block large enough to satisfy size
// parameter, then -1 is returned.
int page_space_alloc(BTPage* page, u16 size) {
    assert(size > 0 && size < PAGE_DATA_SIZE);

    for (int i = 0; i < page->hdr->freeblock_count; i++) {
        BTFreeBlock* b = page_freeblock_at(page, i);
        u16 block_size = b->end_offset - b->start_offset;
        if (block_size >= size) {
            int offset = b->end_offset;
            b->end_offset -= size;
            page->hdr->freespace -= size;
            if (block_size == size) {
                page_freeblock_remove(page, i);
            }
            return offset;
        } else {
            continue;
        }
    }

    return -1;
}

int page_space_dealloc(BTPage* page, u16 start, u16 end) {
    assert(start < end);
    assert(end <= PAGE_SIZE);
    assert(page->hdr->freeblock_count > 0);

    int rc;
    int size = end - start;

    BTFreeBlock* first = page->freeblocks;
    BTFreeBlock* last = page->freeblocks + page->hdr->freeblock_count - 1;
    if (first->start_offset == end) {
        first->start_offset = start;
        page->hdr->freespace += size;
        return Ok;
    } else if (first->start_offset > end) {
        if (page_freeblock_insert(page, 0, start, end) == Ok) {
            page->hdr->freespace += size;
            return Ok;
        }
        return NotEnoughSpace;
    } else if (last->end_offset == start) {
        last->end_offset = end;
        page->hdr->freespace += size;
        return Ok;
    } else if (last->end_offset < start) {
        if (page_freeblock_insert(page, page->hdr->freeblock_count, start, end) == Ok) {
            page->hdr->freespace += size;
            return Ok;
        }
        return NotEnoughSpace;
    } else {
        BTFreeBlock* left = NULL;
        BTFreeBlock* right = NULL;
        for (int i = 0; i < page->hdr->freeblock_count - 1; i++) {
            left = page->freeblocks + i;
            right = page->freeblocks + i + 1;

            if (left->end_offset == start && end == right->start_offset) {
                left->end_offset = right->end_offset;
                page->hdr->freespace += size;
                page_freeblock_remove(page, i);
                return Ok;
            } else if (left->end_offset == start && end < right->start_offset) {
                page->hdr->freespace += size;
                left->end_offset = end;
                return Ok;
            } else if (left->end_offset < start && end == right->end_offset) {
                page->hdr->freespace += size;
                right->start_offset = start;
                return Ok;
            } else if (left->end_offset < start && end < right->start_offset) {
                if (page_freeblock_insert(page, i + 1, start, end) == Ok) {
                    page->hdr->freespace += size;
                    return Ok;
                }
                return NotEnoughSpace;
            } else {
                continue;
            }
        }
    }

    printf("failed to find free space block!\n");
    return NotEnoughSpace;
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

        if (page->hdr->freespace < required_space) {
            printf("not enough free space, have: %d required: %d\n",
                page->hdr->freespace, required_space
            );
            return NotEnoughSpace;
        } else {
            int rc = page_cell_alloc(page);
            if (rc != Ok) {
                printf("can't fit new cell into first free block\n");
                return FreeBlockNotFound;
            }

            int size = key_size + data_size;
            int offset = page_space_alloc(page, size);
            if (offset == -1) {
                printf("there are no freeblocks >= %d bytes\n", size);
                page_cell_dealloc(page);
                return FreeBlockNotFound;
            }

            // okay, there is enough space for both cellptr and payload
            // find insertion point for cellptr 
            u16 ins_point = page_insertion_point(page, key, key_size);
            cellptr = page_cellptr_at(page, ins_point);

            // we need move free blocks to make space for new cell
            page_freeblocks_move_all(page);

            // if new cell should be inserted somewhere within 
            // existing cells then move other cells to the right to make space 
            if (ins_point < page->hdr->cell_count) {
                page_move_cells(page, ins_point);
            }

            // update page hdr
            page->hdr->cell_count++;

            // calculate byte offsets for key and data payload
            data_offset = offset - data_size;
            key_offset = data_offset - key_size;
        }
    } else {
        // overwrite
        u16 curr_size = cellptr->key_size + cellptr->data_size;
        u16 new_size = key_size + data_size;
        int diff = curr_size - new_size;

        if (diff >= 0) {
            // new payload can fit within the space old payload takes
            // just overwrite it 
            key_offset = cellptr->offset + diff;
            data_offset = key_offset + key_size;

            if (diff > 0) {
                u16 start = cellptr->offset;
                u16 end = cellptr->offset + diff;
                if (page_space_dealloc(page, start, end) != Ok) {
                    return NotEnoughSpace;
                }
            }
        } else {

            // Algo:
            //  Allocate bytes for new data
            //  If allocation is not successful
            //      Return 'not enough space'
            //  Deallocate bytes of current data
            //  If deallocation is not successful
            //      Deallocate recently allocated bytes of new data
            //      Return 'Not enough space'
            // 
            //  Set key offset, data offset

            int new_offset = page_space_alloc(page, new_size);
            if (new_offset == -1) {
                return NotEnoughSpace;
            }

            u16 curr_start = cellptr->offset;
            u16 curr_end = curr_start + curr_size;
            if (page_space_dealloc(page, curr_start, curr_end) != Ok) {

                // this dealloc will always succeed because
                // it essentially undoes what previous alloc did
                int rc = page_space_dealloc(page, new_offset, new_offset + new_size);
                assert(rc == Ok);

                return NotEnoughSpace;
            }

            data_offset = new_offset - data_size;
            key_offset = data_offset - key_size;
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

// BTPageSplitResult page_leaf_split(BTPage* page) {
//     assert(page->hdr->is_leaf == 1);

//     BTPage* left = page_blank();
//     left->hdr->is_leaf = page->hdr->is_leaf;
//     left->hdr->pid = page->hdr->pid;

//     BTPage* right = page_new(page->btree);
//     right->hdr->is_leaf = page->hdr->is_leaf;

//     int splitpoint = page_find_splitpoint(page);


//     // copy first half of cells and their payloads to 'left' page
//     for (int i = 0; i < splitpoint; i++) {

//         // get source and destination cell pointers
//         BTCellPtr* src_cellptr = page_cellptr_at(page, i);
//         BTCellPtr* dest_cellptr = page_cellptr_at(left, i);

//         // update metadata of left page
//         int payload_size = src_cellptr->key_size + src_cellptr->data_size;
//         left->hdr->freespace_end -= payload_size;
//         left->hdr->freespace -= (payload_size + PAGE_CELL_PTR_SIZE);
//         left->hdr->cell_count++;

//         // update destination cell pointer metadata
//         dest_cellptr->offset = left->hdr->freespace_end;
//         dest_cellptr->data_size = src_cellptr->data_size;
//         dest_cellptr->key_size = src_cellptr->key_size;

//         // copy key and data payload
//         memcpy(
//             left->pdata + dest_cellptr->offset,
//             page->pdata + src_cellptr->offset,
//             payload_size
//         );
//     }

//     // copy second half of cells and their payloads to 'right' page
//     int right_cell_pos = 0;
//     for (int i = splitpoint; i < page->hdr->cell_count; i++) {
//         // get source and destination cell pointers
//         BTCellPtr* src_cellptr = page_cellptr_at(page, i);
//         BTCellPtr* dest_cellptr = page_cellptr_at(right, right_cell_pos);

//         // update right page metadata
//         int payload_size = src_cellptr->key_size + src_cellptr->data_size;
//         right->hdr->freespace_end -= payload_size;
//         right->hdr->cell_count++;
//         right->hdr->freespace -= (payload_size + PAGE_CELL_PTR_SIZE);

//         // update destination cell pointer metadata
//         dest_cellptr->offset = right->hdr->freespace_end;
//         dest_cellptr->data_size = src_cellptr->data_size;
//         dest_cellptr->key_size = src_cellptr->key_size;

//         // copy data
//         memcpy(
//             right->pdata + dest_cellptr->offset,
//             page->pdata + src_cellptr->offset,
//             payload_size
//         );

//         // advence cellptr position of right page
//         right_cell_pos++;
//     }

//     // here we copy the contents of left page (helper struct) into original page
//     memcpy(page->pdata, left->pdata, PAGE_SIZE);
//     page_destroy(left);

//     return (BTPageSplitResult) {
//         .status = Ok, .page = page, .new_page = right
//     };
// }

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
