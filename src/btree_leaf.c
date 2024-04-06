#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define PAGE_SIZE 100
#define PAGE_HDR_SIZE 16
#define PAGE_CELL_PTR_SIZE 8


typedef struct BTPageHeader {
    uint32_t pid;            // 4
    uint32_t rightmost_pid;  // 4
    uint16_t cell_count;     // 2
    uint16_t free_space_end; // 2
    uint8_t is_leaf;         // 1
    int padding : 24;        // 3
} BTPageHeader;


typedef struct CellPointer {
    uint16_t offset;    // 2
    uint16_t key_size;  // 2
    uint16_t data_size; // 2
    int padding : 16;   // 2
} CellPointer;


typedef struct BTPage {
    BTPageHeader* hdr;
    CellPointer* cell_ptrs;
    char* pdata;
} BTPage;


uint32_t page_counter = 0;
BTPage* buffer[1000];

int binary_collation(
    char* key1,
    uint16_t key1_size,
    char* key2,
    uint16_t key2_size
) {
    // thx sqlite
    int rc, n;
    n = key1_size < key2_size ? key1_size : key2_size;
    assert(key1 && key2);
    rc = memcmp(key1, key2, n);
    if (rc == 0) {
        rc = key1_size - key2_size;
    }
    return rc;
}


BTPage* alloc() {
    char* pdata = calloc(1, PAGE_SIZE);
    BTPage* page = calloc(1, sizeof(BTPage));
    page->hdr = (BTPageHeader*)pdata;

    printf("=======\n");
    printf("page_counter before: %d\n", page_counter);
    printf("=======\n");
    page->hdr->pid = page_counter++;


    printf("=======\n");
    printf("page_counter after: %d\n", page_counter);
    printf("=======\n");

    page->hdr->free_space_end = PAGE_SIZE;
    page->cell_ptrs = (CellPointer*)(pdata + PAGE_HDR_SIZE);
    page->pdata = pdata;
    buffer[page->hdr->pid] = page;
    return page;
}


BTPage* blank_page() {
    char* pdata = calloc(1, PAGE_SIZE);
    BTPage* page = calloc(1, sizeof(BTPage));
    page->hdr = (BTPageHeader*)pdata;
    page->hdr->free_space_end = PAGE_SIZE;
    page->cell_ptrs = (CellPointer*)(pdata + PAGE_HDR_SIZE);
    page->pdata = pdata;
    return page;
}


void dealloc_page(BTPage* page) {
    free(page->pdata);
    free(page);
}


CellPointer* _get_cell_ptr(BTPage* page, uint16_t offset) {
    // assert(page->hdr->cell_count > offset);
    return page->cell_ptrs + offset;
}


uint16_t _get_free_space(BTPage* page) {
    return page->hdr->free_space_end - (PAGE_HDR_SIZE + (PAGE_CELL_PTR_SIZE * page->hdr->cell_count));
}


CellPointer* _get_leftmost_ptr(BTPage* page) {
    return &page->cell_ptrs[0];
}


uint16_t _bisect_left(
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

        if (cmp_res > 0) {
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
    int lo = 0;
    int hi = page->hdr->cell_count;
    int mid;
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


CellPointer* _find_cell_ptr(BTPage* page, char* key, uint16_t key_size) {
    uint16_t pos;
    if (_find_cell_bs(page, key, key_size, &pos) == 0) {
        return _get_cell_ptr(page, pos);
    }
    return NULL;
}


void _move_cells(BTPage* page, uint16_t source_offset) {
    CellPointer* src = _get_cell_ptr(page, source_offset);
    CellPointer* dest = src + 1;
    memmove(dest, src, PAGE_CELL_PTR_SIZE * (page->hdr->cell_count - source_offset));
}


void print_page(BTPage* page) {
    CellPointer* ptr;
    printf("----------------\n");
    printf("pid: %d\n", page->hdr->pid);
    printf("count: %d\n", page->hdr->cell_count);
    printf("free space: %d\n", _get_free_space(page));
    printf("used space: %d\n", PAGE_SIZE - _get_free_space(page));
    printf("is leaf: %d\n", page->hdr->is_leaf);
    if (page->hdr->is_leaf == 0) {
        printf("rightmost pid: %d\n", page->hdr->rightmost_pid);
    }
    for (uint16_t i = 0; i < page->hdr->cell_count; i++) {
        ptr = _get_cell_ptr(page, i);
        uint32_t key = *(uint32_t*)(page->pdata + ptr->offset);
        char* data = page->pdata + ptr->offset + ptr->key_size;

        if (page->hdr->is_leaf) {
            printf("%d - key: %d, data: %s\n", i, key, data);
        } else {
            printf("%d - key: %d, data: %d\n", i, key, *(int*)data);
        }
    }
}


typedef enum {
    Ok,
    NotEnoughSpace
} BTPageSetStatus;


int set(
    BTPage* page,
    char* key,
    uint16_t key_size,
    char* data,
    uint16_t data_size
) {

    int overwrite = 0;
    uint16_t data_offset;
    uint16_t key_offset;

    CellPointer* cp = _find_cell_ptr(page, key, key_size);
    if (cp != NULL) {
        overwrite = 1;
        printf("overwrite operation\n");

        uint16_t curr_size = cp->key_size + cp->data_size;
        uint16_t new_size = key_size + data_size;

        if (new_size <= curr_size) {
            printf("new data can fit in current cell\n");
            key_offset = cp->offset;
            data_offset = cp->offset + cp->key_size;
        } else {
            printf("new data cannot fit in current cell\n");
            printf("checking if defrag could help\n");
            // todo: is defragmenting worth it? should we just split page?
            //       it probably is if fragmantation is low (means that there is a lot of free space)
            uint16_t free_space = _get_free_space(page);
            if (free_space + curr_size >= new_size) {
                printf("todo: DEFRAG PAGE INSTEAD OF SPLITTING...\n");

                data_offset = page->hdr->free_space_end - data_size;
                key_offset = data_offset - key_size;
                return NotEnoughSpace;
            } else {
                printf("not enough space, split?\n");
                return NotEnoughSpace;
            }
        }
    } else {
        printf("add new operation\n");

        uint16_t required_space = key_size + data_size + PAGE_CELL_PTR_SIZE;
        uint16_t free_space = _get_free_space(page);

        if (free_space < required_space) {
            printf("not enough free space, have: %d required: %d\n", free_space, required_space);
            printf("split!!!");
            return 1;
        } else {
            uint16_t insertion_point = _bisect_left(page, key, key_size);
            printf("found insertion point\n");
            cp = _get_cell_ptr(page, insertion_point);

            if (insertion_point < page->hdr->cell_count) {
                printf("moving cells\n");
                _move_cells(page, insertion_point);
            }

            data_offset = page->hdr->free_space_end - data_size;
            key_offset = data_offset - key_size;
        }
    }

    // insert data
    printf("writting key and data...\n");
    memcpy(page->pdata + data_offset, data, data_size);
    memcpy(page->pdata + key_offset, key, key_size);

    printf("updating cell meta\n");
    // Insert cell
    // CellPointer* cp = _get_cell_ptr(page, insertion_point);
    cp->key_size = key_size;
    cp->data_size = data_size;
    cp->offset = key_offset;

    // if current page is internal page
    // then check if inserted page id is bigger then rightmost page id stored in header
    // if yes then swap
    if (page->hdr->is_leaf != 1) {
        uint32_t* pid = (uint32_t*)(page->pdata + data_offset);
        if (*pid > page->hdr->rightmost_pid) {
            uint32_t temp = *pid;
            *pid = page->hdr->rightmost_pid;
            page->hdr->rightmost_pid = temp;
        }
    }

    if (!overwrite) {
        page->hdr->cell_count++;
        page->hdr->free_space_end = cp->offset;
    }

    printf("operation done\n");
    return Ok;
}


int split_page(BTPage* page, BTPage** right_ptr) {
    // if (page->hdr->is_leaf == 0) {
    //     printf("splitting internal pages not implemented\n");
    //     assert(0);
    // }

    BTPage* left = blank_page();
    BTPage* right = alloc();
    left->hdr->is_leaf = page->hdr->is_leaf;
    left->hdr->pid = page->hdr->pid;
    right->hdr->is_leaf = page->hdr->is_leaf;
    right->hdr->rightmost_pid = page->hdr->rightmost_pid;

    int split_point = page->hdr->cell_count / 2;

    CellPointer* cp;
    int cp_offset_src = 0;
    CellPointer* dest_cp;
    int cp_offset_dest = 0;

    // copy first N/2 items to left page 
    while (cp_offset_src < split_point) {

        // get source cell pointer
        cp = _get_cell_ptr(page, cp_offset_src);

        // if this is last source cell pointer
        // just copy page id (data_part) to 'rightmost_pid' header field
        if (page->hdr->is_leaf == 0 && cp_offset_src == split_point - 1) {
            uint32_t data_offset = cp->offset + cp->key_size;
            uint32_t pid = *(uint32_t*)(page->pdata + data_offset);
            left->hdr->rightmost_pid = pid;
            break;
        }

        // get dest cell pointer
        dest_cp = _get_cell_ptr(left, cp_offset_dest);

        // update left page metadata
        int cell_size = cp->key_size + cp->data_size;
        left->hdr->pid = page->hdr->pid;
        left->hdr->free_space_end -= cell_size;
        left->hdr->cell_count++;

        // update destination cell pointer metadata
        dest_cp->offset = left->hdr->free_space_end;
        dest_cp->data_size = cp->data_size;
        dest_cp->key_size = cp->key_size;

        // copy key and data
        memcpy(left->pdata + dest_cp->offset, page->pdata + cp->offset, cell_size);

        // move to next cell pointer
        cp_offset_src++;
        cp_offset_dest++;
    }

    // reset destination offset 
    cp_offset_src = split_point;
    cp_offset_dest = 0;

    // copy last N/2 items to right page
    while (cp_offset_src < page->hdr->cell_count) {
        // get source and destination 
        // cell pointers
        cp = _get_cell_ptr(page, cp_offset_src);
        dest_cp = _get_cell_ptr(right, cp_offset_dest);

        // update right page metadata
        int cell_size = cp->key_size + cp->data_size;
        right->hdr->free_space_end -= cell_size;
        right->hdr->cell_count++;

        // update destination cp metadata
        dest_cp->offset = right->hdr->free_space_end;
        dest_cp->data_size = cp->data_size;
        dest_cp->key_size = cp->key_size;

        // copy data
        memcpy(right->pdata + dest_cp->offset, page->pdata + cp->offset, cell_size);

        // move to next cell pointer
        cp_offset_src++;
        cp_offset_dest++;
    }

    memcpy(page->pdata, left->pdata, PAGE_SIZE);
    dealloc_page(left);
    *right_ptr = right;
    return Ok;
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

// Crumbs
/////////////////////////////////////////////////

typedef struct BTCrumbs {
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
    uint8_t pos = --crumbs->n;
    return crumbs->crumbs[pos];
}

void btcrumbs_destroy(BTCrumbs* crumbs) {
    free(crumbs);
}

// BTREE top
//////////////////////////////////////////////////

typedef struct BTree {
    uint32_t root_page_id;
} BTree;


BTree* btree_new() {
    BTPage* root_page = alloc();
    root_page->hdr->is_leaf = 1;

    uint32_t root_page_id = root_page->hdr->pid;
    buffer[root_page_id] = root_page;

    BTree* btree = malloc(sizeof(BTree));
    btree->root_page_id = root_page_id;
    return btree;
}

BTCrumbs* _btree_find_leaf(BTree* btree, char* key, uint16_t key_size) {
    BTPage* curr = buffer[btree->root_page_id];

    BTCrumbs* crumbs = btcrumbs_new();
    btcrumbs_push(crumbs, curr);

    while (1) {
        if (curr->hdr->is_leaf) {
            return crumbs;
        } else {
            int cp_offset = _bisect_left(curr, key, key_size);
            uint32_t next_page;
            if (cp_offset == curr->hdr->cell_count) {
                next_page = curr->hdr->rightmost_pid;
            } else {
                CellPointer* cp = _get_cell_ptr(curr, cp_offset);
                int data_offset = cp->offset + cp->key_size;
                next_page = *(uint32_t*)(curr->pdata + data_offset);
            }
            curr = buffer[next_page];
            btcrumbs_push(crumbs, curr);
        }
    }

    printf("fatal: failed to find leaf page!!!\n");
    assert(0);
}


int btree_promote(
    BTree* btree,
    BTCrumbs* crumbs,
    BTPage* leaf,
    BTPage* sibbling,
    char* key,
    uint16_t key_size
) {

    if (crumbs->n == 0) {
        BTPage* new_root = alloc();
        new_root->hdr->is_leaf = 0;
        set(new_root, key, key_size, (char*)&leaf->hdr->pid, 4);
        new_root->hdr->rightmost_pid = sibbling->hdr->pid;
        btree->root_page_id = new_root->hdr->pid;
        return Ok;
    } else {
        BTPage* parent = btcrumbs_pop(crumbs);
        uint32_t right_pid = sibbling->hdr->pid;
        char* right_pid_data = (char*)&right_pid;
        uint16_t right_pid_data_size = 4;

        int rc = set(parent, key, key_size, right_pid_data, right_pid_data_size);
        if (rc == Ok) {
            return Ok;
        } else if (rc == NotEnoughSpace) {
            // split leaf into left and right pages
            BTPage* parent_sibbling = NULL;
            int split_rc = split_page(parent, &parent_sibbling);
            assert(split_rc == Ok);

            // get split key from right page
            CellPointer* leftmost_cp = _get_leftmost_ptr(parent_sibbling);
            char* split_key = parent_sibbling->pdata + leftmost_cp->offset;
            uint16_t split_key_size = leftmost_cp->key_size;

            // insert new item into appropriate page
            // if key is >= leftmost_key from right page then insert into right page
            // else insert into left page
            int cmp_res = binary_collation(key, key_size, split_key, split_key_size);
            if (cmp_res >= 0) {
                set(parent_sibbling, key, key_size, right_pid_data, right_pid_data_size);
                if (cmp_res == 0) {
                    split_key = key;
                    split_key_size = key_size;
                }
            } else {
                set(parent, key, key_size, right_pid_data, right_pid_data_size);
            }

            // promote split key through parents
            int promote_rc = btree_promote(btree, crumbs, parent, parent_sibbling, split_key, split_key_size);
            assert(promote_rc == Ok);
            return Ok;
        } else {
            printf("result code %d not supported\n", rc);
            assert(0);
        }
    }
}

// TODO: make sure that saved key + data is at most 25% of the total node size
//       if not, store the rest in overflow page 

// set op scenarios
// - key already used
//     - new data is same size
//     - new data is smaller
//     - new data is larger
//        - delete key
//        - insert key
// - key not used
//     - enough free space
//     - enough free space (after defrag)
//     - not enough free space

int btree_insert(BTree* btree, char* key, uint16_t key_size, char* data, uint16_t data_size) {
    BTCrumbs* crumbs = _btree_find_leaf(btree, key, key_size);

    BTPage* leaf = btcrumbs_pop(crumbs);

    int rc = set(leaf, key, key_size, data, data_size);
    if (rc == Ok) {
        return Ok;
    } else if (rc == NotEnoughSpace) {
        // split leaf into left and right pages
        BTPage* sibbling = NULL;
        int split_rc = split_page(leaf, &sibbling);
        assert(split_rc == Ok);

        // get split key from right page
        CellPointer* leftmost_cp = _get_leftmost_ptr(sibbling);
        char* split_key = sibbling->pdata + leftmost_cp->offset;
        uint16_t split_key_size = leftmost_cp->key_size;

        // insert new item into appropriate page
        // if key is >= leftmost_key from right page then insert into right page
        // else insert into left page
        int cmp_res = binary_collation(key, key_size, split_key, split_key_size);
        if (cmp_res >= 0) {
            set(sibbling, key, key_size, data, data_size);
            if (cmp_res == 0) {
                split_key = key;
                split_key_size = key_size;
            }
        } else {
            set(leaf, key, key_size, data, data_size);
        }

        // promote split key through parents
        int promote_rc = btree_promote(btree, crumbs, leaf, sibbling, split_key, split_key_size);
        assert(promote_rc == Ok);
        return Ok;
    } else {
        printf("result code %d not supported\n", rc);
        assert(0);
    }
}


// TEST
//////////////////////////////////////////////////

void flush_page(void* data) {
    FILE* f = fopen("btree_leaf.data", "w");
    fwrite(data, 1, PAGE_SIZE, f);
    fflush(f);
    fclose(f);
}

int main() {
    page_counter = 0;
    BTree* btree = btree_new();

    uint32_t key1 = 123;
    char* data1 = "bbbb";
    uint32_t key2 = 20;
    char* data2 = "cccccccc";
    uint32_t key3 = 50;
    char* data3 = "aaa";
    uint32_t key4 = 0;
    char* data4 = "kkkkkk";

    for (uint32_t i = 1; i <= 3; i++) {
        btree_insert(btree, (char*)&i, 4, data3, strlen(data3) + 1);
        flush_page(buffer[btree->root_page_id]);
    }

    printf("\n\n\n\n\n\n\n\n");

    printf("total pages: %d\n", page_counter);
    printf("root page: %d\n", btree->root_page_id);
    printf("========\n");

    while (1) {
        char line[256];
        int i;
        printf("enter page id:");
        if (fgets(line, sizeof(line), stdin)) {
            if (1 == sscanf(line, "%d", &i)) {
                /* i can be safely used */
                print_page(buffer[i]);
                printf("========\n");
            }
        }
    }
}
