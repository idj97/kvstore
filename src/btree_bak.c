#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define PAGE_SIZE 200
#define PAGE_HDR_SIZE 12
#define PAGE_CELL_PTR_SIZE 8



typedef struct {
    uint32_t rightmost_pid;  // 4
    uint16_t cell_count;     // 2
    uint16_t free_space_end; // 2
    uint8_t is_leaf;         // 1
    int padding : 24;        // 3
} BTPageHeader;


typedef struct {
    uint16_t offset;    // 2
    uint16_t key_size;  // 2
    uint16_t data_size; // 2
    int padding : 16;   // 2
} CellPointer;


typedef struct {
    BTPageHeader* hdr;
    CellPointer* cell_ptrs;
    char* pdata;
} BTPage;



BTPage* buffer[1000];
uint32_t page_counter;


int binary_collation(
    char* a,
    uint32_t a_size,
    char* b,
    uint32_t b_size
) {
    uint32_t sz;
    if (a_size >= b_size) {
        sz = b_size;
    } else {
        sz = a_size;
    }

    int rc = memcmp(a, b, sz);
    if (rc == 0) {
        // in case of memcmp(aaa,aaaB,3) -> -1
        //         or memcmp(aaaB,aaa,3) ->  1
        rc = a_size - b_size;
    }
    return rc;
}


BTPage* alloc() {
    char* pdata = calloc(1, PAGE_SIZE);
    BTPage* page = calloc(1, sizeof(BTPage));
    page->hdr = (BTPageHeader*)pdata;
    page->hdr->free_space_end = PAGE_SIZE;
    page->cell_ptrs = (CellPointer*)(pdata + PAGE_HDR_SIZE);
    page->pdata = pdata;
    return page;
}


CellPointer* _get_cell_ptr(BTPage* page, uint16_t offset) {
    // assert(page->hdr->cell_count > offset);
    return page->cell_ptrs + offset;
}


uint16_t _get_free_space(BTPage* page) {
    return page->hdr->free_space_end - (PAGE_HDR_SIZE + (PAGE_CELL_PTR_SIZE * page->hdr->cell_count));
}


uint16_t _find_insertion_point(
    BTPage* page,
    char* key,
    uint16_t key_size
) {
    if (page->hdr->cell_count == 0) {
        return 0;
    }

    CellPointer* cell;
    char* cell_key;
    uint16_t lo = 0;
    uint16_t hi = page->hdr->cell_count;
    uint16_t mid;

    while (lo < hi) {
        mid = (lo + hi) / 2;
        cell = _get_cell_ptr(page, mid);
        cell_key = &page->pdata[cell->offset];

        int cmp_res = binary_collation(key, key_size, cell_key, cell->key_size);

        if (cmp_res == 1) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}


int _find_cell_bs(BTPage* page, char* key, uint16_t key_size, uint16_t* res) {
    if (page->hdr->cell_count == 0) {
        return 1;
    }

    CellPointer* mid_cell;
    char* mid_cell_key;
    uint16_t lo = 0;
    uint16_t hi = page->hdr->cell_count;
    uint16_t mid;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        mid_cell = _get_cell_ptr(page, mid);
        mid_cell_key = &page->pdata[mid_cell->offset];

        int rcmp = binary_collation(key, key_size, mid_cell_key, mid_cell->key_size);
        if (rcmp == 0) {
            *res = mid;
            return 0;
        } else if (rcmp > 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return 1;
}


void _move_cells(BTPage* page, uint16_t source_offset) {
    CellPointer* src = _get_cell_ptr(page, source_offset);
    CellPointer* dest = src + 1;
    memmove(dest, src, PAGE_CELL_PTR_SIZE * (page->hdr->cell_count - source_offset));
}


void print(BTPage* page) {
    CellPointer* ptr;
    for (uint16_t i = 0; i < page->hdr->cell_count; i++) {
        ptr = _get_cell_ptr(page, i);
        uint32_t key = *(uint32_t*)(page->pdata + ptr->offset);
        char* data = page->pdata + ptr->offset + ptr->key_size;
        printf("%d - key: %d, data: %s\n", i, key, data);
    }
}


int set(
    BTPage* page,
    char* key,
    uint16_t key_size,
    char* data,
    uint16_t data_size
) {
    // Make sure that there is enough space for new entry
    uint16_t required_space = key_size + data_size + PAGE_CELL_PTR_SIZE;
    uint16_t free_space = _get_free_space(page);
    if (free_space < required_space) {
        printf("not enough free space, have: %d required: %d\n", free_space, required_space);
        return 1;
    }

    // Find insertion point in cell_ptrs
    uint16_t insertion_point = 0;
    if (page->hdr->cell_count > 0) {
        insertion_point = _find_insertion_point(page, key, key_size);

        CellPointer* cell = _get_cell_ptr(page, insertion_point);
        char* cell_key = &page->pdata[cell->offset];

        if (binary_collation(key, key_size, cell_key, cell->key_size) == 0) {
            printf("key already exists, todo: overwrite?\n");
            return 1;
        }

        // Move cells
        if (insertion_point < page->hdr->cell_count) {
            _move_cells(page, insertion_point);
        }
    }

    // insert data
    uint16_t data_offset = page->hdr->free_space_end - data_size;
    uint16_t key_offset = data_offset - key_size;
    memcpy(page->pdata + data_offset, data, data_size);
    memcpy(page->pdata + key_offset, key, key_size);

    // Insert cell
    CellPointer* cell = _get_cell_ptr(page, insertion_point);
    cell->key_size = key_size;
    cell->data_size = data_size;
    cell->offset = key_offset;
    page->hdr->cell_count++;
    page->hdr->free_space_end = cell->offset;
    return 0;
}


int get(BTPage* page, char* key, uint16_t key_size, char** data, uint16_t* data_size) {
    uint16_t cell_offset;
    if (_find_cell_bs(page, key, key_size, &cell_offset) == 0) {
        CellPointer* cell = _get_cell_ptr(page, cell_offset);
        *data_size = cell->data_size;
        *data = page->pdata + cell->offset + cell->key_size;
        return 0;
    }
    return 1;
}


char cmp(char* a, uint16_t a_size, char* b, uint16_t b_size) {
    uint32_t ia = *(uint32_t*)a;
    uint32_t ib = *(uint32_t*)b;
    if (ia > ib) {
        return 1;
    } else if (ia < ib) {
        return -1;
    } else {
        return 0;
    }
}

// Crumbs
/////////////////////////////////////////////////

typedef struct {
    uint8_t n;
    BTPage* crumbs[32];
} BTCrumbs;

BTCrumbs* btcrumbs_new() {
    BTCrumbs* crumbs = malloc(sizeof(BTCrumbs));
    crumbs->n = 0;
    return crumbs;
}

void btcrumbs_push(BTCrumbs* crumbs, BTPage* page) {
    assert(crumbs->n < 31);
    uint8_t pos = crumbs->n++;
    crumbs->crumbs[pos] = page;
}

BTPage* btcrumbs_pop(BTCrumbs* crumbs) {
    assert(crumbs->n > 0);
    uint8_t pos = crumbs->n--;
    return crumbs->crumbs[pos];
}

void btcrumbs_destroy(BTCrumbs* crumbs) {
    free(crumbs);
}

// BTREE top
//////////////////////////////////////////////////

typedef struct {
    uint32_t root_page_id;
} BTree;


BTree* btree_new(char (*cmp)(char*, uint16_t, char*, uint16_t)) {
    BTPage* root_page = alloc(cmp);
    root_page->hdr->is_leaf = 1;

    uint32_t root_page_id = page_counter++;
    buffer[root_page_id] = root_page;

    BTree* btree = malloc(sizeof(BTree));
    btree->root_page_id = root_page_id;
    return btree;
}

void _btree_find_leaf(BTree* btree, BTCrumbs** btcrumbs, char* key, uint16_t key_size) {
    BTPage* curr = buffer[btree->root_page_id];

    BTCrumbs* crumbs = btcrumbs_new();
    btcrumbs_push(crumbs, curr);

    while (1) {
        if (curr->hdr->is_leaf) {
            *btcrumbs = crumbs;
            return;
        } else {
            printf("todo: finding leafs through internal nodes not implemented.\n");
            assert(1 == 2);
        }
    }
}


void btree_insert(BTree* btree, char* key, uint16_t key_size, char* data, uint16_t data_size) {
    BTCrumbs* crumbs;
    _btree_find_leaf(btree, &crumbs, key, key_size);

    BTPage* leaf = btcrumbs_pop(crumbs);

}


// TEST
//////////////////////////////////////////////////

int main() {
    FILE* f = fopen("btree_leaf.data", "w");
    BTPage* page = alloc(&cmp);

    uint32_t key1 = 123;
    char* data1 = "bbbb";
    uint32_t key2 = 20;
    char* data2 = "cccccccc";
    uint32_t key3 = 50;
    char* data3 = "aaa";
    uint32_t key4 = 0;
    char* data4 = "kkkkkk";

    set(page, (char*)&key3, 4, data3, strlen(data3) + 1);
    set(page, (char*)&key1, 4, data1, strlen(data1) + 1);
    set(page, (char*)&key2, 4, data2, strlen(data2) + 1);
    set(page, (char*)&key3, 4, data3, strlen(data3) + 1);
    set(page, (char*)&key3, 4, data3, strlen(data3) + 1);
    set(page, (char*)&key4, 4, data4, strlen(data4) + 1);
    set(page, (char*)&key3, 4, data3, strlen(data3) + 1);

    fwrite(page->pdata, 1, PAGE_SIZE, f);
    fflush(f);
    fclose(f);

    print(page);

    // char* data;
    // uint16_t data_size;
    // int r;
    // r = get(page, (char*)&key1, 4, &data, &data_size);
    // printf("%d | %d - %s\n", r, key1, data);
    // r = get(page, (char*)&key2, 4, &data, &data_size);
    // printf("%d | %d - %s\n", r, key2, data);

    free(page);
}
