#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PAGE_SIZE 16000
#define PAGE_HEADER_SIZE 10
#define PAGE_CELL_PTR_SIZE 8

// todo: add magic number
typedef struct {
    uint32_t next_page_id;
    uint16_t checksum;
    uint16_t cell_count;
    uint16_t free_space_end;
} PageHeader;

typedef struct {
    uint32_t size;
    uint16_t offset;
    uint8_t deleted;
    uint8_t overflown;
} CellPointer;

typedef struct {
    PageHeader* hdr;    // page header
    CellPointer* ptrs;  // first cell ptr
    char* pdata;        // page data
} Page;

Page* load(char* page_data) {
    Page* p = malloc(sizeof(Page));
    p->hdr = (PageHeader*)page_data;
    p->ptrs = (CellPointer*)(page_data + PAGE_HEADER_SIZE);
    p->pdata = page_data;
    return p;
}

Page* alloc() {
    char* page_data = calloc(1, PAGE_SIZE);
    Page* p = calloc(1, sizeof(Page));
    p->hdr = (PageHeader*)page_data;
    p->ptrs = (CellPointer*)(page_data + PAGE_HEADER_SIZE);
    p->pdata = page_data;
    p->hdr->free_space_end = PAGE_SIZE;
    return p;
}

static inline CellPointer* get_cell_ptr(Page* page, uint16_t offset) {
    assert(page->hdr->cell_count > offset);
    return page->ptrs + offset;
}

static inline uint16_t get_free_space(Page* page) {
    return page->hdr->free_space_end - (PAGE_HEADER_SIZE + (PAGE_CELL_PTR_SIZE * page->hdr->cell_count));
}

static inline char* get_data_start(Page* page) {
    return page->pdata + page->hdr->free_space_end;
}

uint16_t get(Page* page, char** data, uint16_t cell_offset) {
    if (cell_offset >= page->hdr->cell_count) {
        printf("get: out of bounds\n");
        return 0;
    }
    CellPointer* cell_ptr = page->ptrs + cell_offset;
    if (cell_ptr->deleted == 1) {
        printf("get: not found\n");
        return 0;
    }

    *data = page->pdata + cell_ptr->offset;
    return cell_ptr->size;
}

uint32_t insert(Page* page, char* record, uint16_t record_size) {
    // compute required space (cell ptr size + record size) 
    // and check if there is enough space within page for new record
    uint16_t req_space = record_size + PAGE_CELL_PTR_SIZE;
    uint16_t free_space = get_free_space(page);
    if (free_space < req_space) {
        printf("not enough free space, free: %d, required: %d!\n", free_space, req_space);
        return 0;
    }

    // update pointer and header
    CellPointer* ptr = page->ptrs + page->hdr->cell_count;
    ptr->offset = page->hdr->free_space_end - record_size;
    ptr->size = record_size;
    ptr->deleted = 0;
    ptr->overflown = 0;

    PageHeader* hdr = page->hdr;
    hdr->cell_count++;
    hdr->free_space_end = ptr->offset;

    // copy record data into page
    char* cell = page->pdata + ptr->offset;
    memcpy(cell, record, record_size);
    return page->hdr->cell_count;
}

int update(Page* page, uint16_t cell_offset, char* record, uint16_t record_size) {
    if (cell_offset > page->hdr->cell_count) {
        printf("update: offset %d is out of bounds (%d)\n", cell_offset, page->hdr->cell_count);
        return 1;
    }

    CellPointer* cell_ptr = get_cell_ptr(page, cell_offset);
    if (cell_ptr->deleted != 0) {
        printf("update: record %d is deleted\n", cell_offset);
        return 1;
    }

    // we have three scenarios:
    // 1. new record has same size as old one
    //    - we dont have to do anything in this case but to overwrite the data
    // 2. new record is smaller then old one
    //    - write the record
    //    - shift/memmove the data to its left to the right and update pointers
    // 3. new record is larger then old one
    //    - check if there is enough space
    //    - if there is enough space move data to its left to the right and update pointers
    //    - write the record at the end

    if (record_size == cell_ptr->size) {
        char* cell = page->pdata + cell_ptr->offset;
        memcpy(cell, record, record_size);
        return 0;
    }
    else {
        if (record_size > cell_ptr->size) {
            uint16_t cell_overflow = record_size - cell_ptr->size;
            if (cell_overflow > get_free_space(page)) {
                printf("update: not enough space in page\n");
                return 1;
            }
        }

        // move other cells and pointers
        char* src = get_data_start(page);
        char* dest = src + cell_ptr->size;
        uint16_t n = cell_ptr->offset - page->hdr->free_space_end;
        memmove(dest, src, n);

        for (uint16_t i = 0; i < page->hdr->cell_count; i++) {
            CellPointer* ptr = get_cell_ptr(page, i);
            if (ptr->deleted == 0 && ptr->offset < cell_ptr->offset) {
                ptr->offset += cell_ptr->size;
            }
        }
        page->hdr->free_space_end += cell_ptr->size;
        //

        // update cell
        cell_ptr->offset = page->hdr->free_space_end - record_size;
        cell_ptr->size = record_size;
        page->hdr->free_space_end = cell_ptr->offset;
        char* cell_src = page->pdata + cell_ptr->offset;
        memcpy(cell_src, record, record_size);
        return 0;
    }
}

int delete(Page* page, uint16_t cell_offset) {
    if (cell_offset >= page->hdr->cell_count) {
        printf("delete: invalid cell offset\n");
        return 1;
    }

    // Cell pointer of record to delete
    // Once we have a cell pointer we first mark it as deleted. Deleted cell 
    // pointers are not removed from cell dictionary because other pointers
    // must not be moved because record id cannot be changed.
    // 
    // todo: memory usage optimization?
    // Its possible to reuse deleted pointer when inserting new record
    // but that requires linear scan to find it...
    CellPointer* cell_ptr = get_cell_ptr(page, cell_offset);
    if (cell_ptr->deleted) {
        // printf("record is already deleted\n");
        return 1;
    }
    cell_ptr->deleted = 1;

    // if deleted record is in the middle of records then we need 
    // to move records that are to the left of it.
    // otherwise no memmove is removed

    if (cell_ptr->offset > page->hdr->free_space_end) {
        char* src = page->pdata + page->hdr->free_space_end;
        char* dest = src + cell_ptr->size;
        uint16_t n = cell_ptr->offset - page->hdr->free_space_end;
        memmove(dest, src, n);

        // update pointer offsets of records
        // to the left of deleted record
        for (uint16_t i = 0; i < page->hdr->cell_count; i++) {
            CellPointer* ptr = get_cell_ptr(page, i);
            if (ptr->deleted == 0 && ptr->offset < cell_ptr->offset) {
                ptr->offset += cell_ptr->size;
            }
        }
    }

    // update free space size and end 
    page->hdr->free_space_end += cell_ptr->size;
    return 0;
}

int main() {
    Page* page = alloc();
    char* data = "aaaa"; // 12
    char* data2 = "bbbbbbbbb"; // 17
    char* data3 = "ccccccccccccccccc"; // 19
    char* data4 = "ffffffffffffffffffff";

    int rc;
    int cnt = 0;
    while (1) {
        // printf("inserting %d\n", cnt);
        char* d;
        if (cnt % 3 == 0) { d = data3; }
        else if (cnt % 2 == 0) { d = data2; }
        else { d = data; }

        rc = insert(page, d, strlen(d) + 1);

        // FILE* f = fopen("out.d", "w");
        // fwrite(page->pdata, PAGE_SIZE, 1, f);
        // fflush(f);
        // fclose(f);

        if (rc == 0) { break; }
        cnt++;
    }

    for (int i = 0; i < 500; i++) {
        uint16_t offset = rand() % cnt;
        char* d;

        if (offset % 4 == 0) {
            // printf("deleting %d\n", offset);
            delete(page, offset);
        }
        else {
            if (offset % 3 == 0) {
                d = data4;
            }
            else if (offset % 2 == 0) {
                d = data2;
            }
            else {
                d = data;
            }
            // printf("updating %d with %s\n", offset, d);
            update(page, offset, d, strlen(d) + 1);
        }

        // FILE* f = fopen("out.d", "w");
        // fwrite(page->pdata, PAGE_SIZE, 1, f);
        // fflush(f);
        // fclose(f);

    }


    // printf("%d\n", delete(page, 2));
    // printf("%d\n", delete(page, 2));


    // update(page, 0, data4, strlen(data4) + 1);
    // printf("%d\n", insert(page, data, strlen(data) + 1));

    // printf("%d\n", insert(page, data, strlen(data) + 1));
    // printf("%d\n", insert(page, data2, strlen(data2) + 1));
    // printf("%d\n", insert(page, data3, strlen(data3) + 1));

    // printf("%d\n", delete(page, 2));
    // printf("%d\n", delete(page, 0));
    // printf("%d\n", delete(page, 4));

    // for (uint16_t i = 0; i < 3; i++) {
    //     char* data = NULL;
    //     if (get(page, &data, i) > 0) {
    //         printf("got: %s\n", data);
    //     }
    // }
    // printf("\n");


    free(page->pdata);
    free(page);

    return 0;
}
