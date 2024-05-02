#include "../lib/unity/unity.h"
#include "btree.c"

//////////////////////////////////////////////////////////////////////
// Test data 1 
const u32 td1_key1 = 1;
const size_t td1_key1_size = sizeof(u32);
const char* td1_data1 = "111111111111111111111111111111111111111";
const size_t td1_data1_size = 40;
const size_t td1_entry1_size = td1_key1_size + td1_data1_size;

const size_t td1_freespace3_size = 25;

const u32 td1_key2 = 4;
const size_t td1_key2_size = sizeof(u32);
const char* td1_data2 = "222222222222222222222222222222222222222";
const size_t td1_data2_size = 40;
const size_t td1_entry2_size = td1_key2_size + td1_data2_size;

const size_t td1_freespace2_size = 20;

const u32 td1_key3 = 8;
const size_t td1_key3_size = sizeof(u32);
const char* td1_data3 = "33333333333333333333333333333333";
const size_t td1_data3_size = 33;
const size_t td1_entry3_size = td1_key3_size + td1_data3_size;
////////////////////////////////////////////////////////////////////////



u32 get_key(BTPage* page, int pos) {
    Value v = page_key_at(page, pos);
    return *(u32*)v.data;
}

const char* get_data(BTPage* page, int pos) {
    Value v = page_data_at(page, pos);
    return v.data;
}

void display_freeblocks(BTPage* page) {
    for (int i = 0; i < page->hdr->cell_count; i++) {
        BTCellPtr* ptr = page_cellptr_at(page, i);
        int start = ptr->offset;
        int end = start + ptr->key_size + ptr->data_size;
        printf("cell %d. - start: %d, end: %d\n", i, start, end);
    }
    printf("---------------\n");

    for (int i = 0; i < page->hdr->freeblock_count; i++) {
        BTFreeBlock* fb = page_freeblock_at(page, i);
        printf("fb %d. - start: %d, end: %d\n", i, fb->start_offset, fb->end_offset);
    }
    printf("===========\n");
}

void test_page_new() {
    BTPage* page1 = page_new(NULL);
    BTPage* page2 = page_new(NULL);
    BTPage* page3 = page_new(NULL);
    TEST_ASSERT_EQUAL_INT(page3->hdr->cell_count, 0);
    TEST_ASSERT_EQUAL_INT(page3->hdr->freeblock_count, 1);
    TEST_ASSERT_EQUAL_INT(page3->freeblocks->start_offset, PAGE_SIZE - PAGE_DATA_SIZE);
    TEST_ASSERT_EQUAL_INT(page3->freeblocks->end_offset, PAGE_SIZE);
    TEST_ASSERT_EQUAL_INT(page3->hdr->is_leaf, 0);
    TEST_ASSERT_EQUAL_INT(page3->hdr->pid, 2);
    TEST_ASSERT_EQUAL_INT(page3->hdr->rightmost_pid, 0);
    TEST_ASSERT_EQUAL_INT(page3->hdr->rightmost_pid, 0);
}

void test_set_when_empty() {
    // prepare
    BTree* btree = btree_new(&compare_integers);
    BTPage* page = buffer[btree->root_page_id];

    // test
    u32 key1 = 1234;
    size_t key1_size = sizeof(u32);
    char* data1 = "hello";
    size_t data1_size = strlen(data1) + 1;

    int rc = page_leaf_set(page, &key1, key1_size, data1, data1_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);

    u32 key2 = 321;
    size_t key2_size = sizeof(u32);
    char* data2 = "olla";
    size_t data2_size = strlen(data2) + 1;
    int rc2 = page_leaf_set(page, &key2, key2_size, data2, data2_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc2);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->cell_count);

    // check first input
    TEST_ASSERT_EQUAL_INT(key1, get_key(page, 1));
    TEST_ASSERT_EQUAL_STRING(data1, get_data(page, 1));
    BTCellPtr* cell1 = page_cellptr_at(page, 1);
    size_t payload1_size = key1_size + data1_size;
    TEST_ASSERT_EQUAL_INT(PAGE_SIZE - payload1_size, cell1->offset);
    TEST_ASSERT_EQUAL_INT(key1_size, cell1->key_size);
    TEST_ASSERT_EQUAL_INT(data1_size, cell1->data_size);

    // because key 321 is smaller then 1234 we expect to
    // find cell of '321' entry at first position
    TEST_ASSERT_EQUAL_INT(key2, get_key(page, 0));
    TEST_ASSERT_EQUAL_STRING(data2, get_data(page, 0));
    BTCellPtr* cell2 = page_cellptr_at(page, 0);
    size_t payload2_size = key2_size + data2_size;
    TEST_ASSERT_EQUAL_INT(PAGE_SIZE - payload1_size - payload2_size, cell2->offset);
    TEST_ASSERT_EQUAL_INT(key2_size, cell2->key_size);
    TEST_ASSERT_EQUAL_INT(data2_size, cell2->data_size);

    TEST_ASSERT_EQUAL_INT(1, page->hdr->freeblock_count);
    BTFreeBlock* freeblock = page_freeblock_at(page, 0);
    TEST_ASSERT_EQUAL_INT(PAGE_SIZE - payload1_size - payload2_size, freeblock->end_offset);
    TEST_ASSERT_EQUAL_INT(PAGE_HDR_SIZE + PAGE_CELL_PTR_SIZE * 2 + PAGE_FREE_BLOCK_SIZE, freeblock->start_offset);

    btree_destroy(btree);
}

void verify_test_data1(BTree* btree) {
    BTPage* page = buffer[btree->root_page_id];
    TEST_ASSERT_EQUAL_STRING(td1_data1, page_data_by_key(page, &td1_key1, td1_key1_size).data);
    TEST_ASSERT_EQUAL_STRING(td1_data2, page_data_by_key(page, &td1_key2, td1_key2_size).data);
    TEST_ASSERT_EQUAL_STRING(td1_data3, page_data_by_key(page, &td1_key3, td1_key3_size).data);
}

BTree* test_data1() {
    // Test page 1:
    //   total: 236 bytes
    //   freespace: 60 bytes
    //   used: 176 bytes
    //    - extra freeblocks: 2 (8 bytes)
    //    - cells: 3 (36 bytes)
    //    - data: (132 bytes) 
    //       1. 37 bytes (4 + 33)
    //       2. 44 bytes (4 + 40)
    //       3. 44 bytes (4 + 40)
    // ---------------------------------------------
    // | f:22 | d:37 | f:20 | d:44 | f: 25 | d: 44 |
    // ---------------------------------------------
    //

    BTree* btree = btree_new(&compare_integers);
    BTPage* page = buffer[btree->root_page_id];
    int expected_freespace = page->hdr->freespace;

    int irc1 = page_leaf_set(page, &td1_key1, td1_key1_size, td1_data1, td1_data1_size);
    TEST_ASSERT_EQUAL_INT(Ok, irc1);
    expected_freespace -= (td1_entry1_size + PAGE_CELL_PTR_SIZE);
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));

    int frc1 = page_insert_freespace(page, td1_freespace3_size);
    TEST_ASSERT_EQUAL_INT(Ok, frc1);
    expected_freespace -= PAGE_FREE_BLOCK_SIZE;
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));

    int irc2 = page_leaf_set(page, &td1_key2, td1_key2_size, td1_data2, td1_data2_size);
    TEST_ASSERT_EQUAL_INT(Ok, irc2);
    expected_freespace -= (td1_entry2_size + PAGE_CELL_PTR_SIZE);
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));

    int frc2 = page_insert_freespace(page, td1_freespace2_size);
    TEST_ASSERT_EQUAL_INT(Ok, frc2);
    expected_freespace -= PAGE_FREE_BLOCK_SIZE;
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));

    int irc3 = page_leaf_set(page, &td1_key3, td1_key3_size, td1_data3, td1_data3_size);
    TEST_ASSERT_EQUAL_INT(Ok, irc3);
    expected_freespace -= (td1_entry3_size + PAGE_CELL_PTR_SIZE);
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));

    // assert data
    verify_test_data1(btree);

    // assert metadata, offsets etc
    TEST_ASSERT_EQUAL_INT(3, page->hdr->cell_count);
    TEST_ASSERT_EQUAL_INT(3, page->hdr->freeblock_count);
    TEST_ASSERT_EQUAL_INT(expected_freespace, page->hdr->freespace);

    BTCellPtr* cp1 = page_find_cellptr(page, &td1_key1, td1_key1_size);
    u32 expected_entry1_offset = PAGE_SIZE - td1_entry1_size;
    TEST_ASSERT_EQUAL_INT(expected_entry1_offset, cp1->offset);
    TEST_ASSERT_EQUAL_INT(td1_key1_size, cp1->key_size);
    TEST_ASSERT_EQUAL_INT(td1_data1_size, cp1->data_size);

    BTFreeBlock* fb3 = page_freeblock_at(page, 2);
    u32 expected_freespace3_offset = expected_entry1_offset - td1_freespace3_size;
    TEST_ASSERT_EQUAL_INT(expected_freespace3_offset, fb3->start_offset);
    TEST_ASSERT_EQUAL_INT(expected_freespace3_offset + td1_freespace3_size, fb3->end_offset);

    BTCellPtr* cp2 = page_find_cellptr(page, &td1_key2, td1_key2_size);
    u32 expected_entry2_offset = expected_freespace3_offset - td1_entry2_size;
    TEST_ASSERT_EQUAL_INT(expected_entry2_offset, cp2->offset);
    TEST_ASSERT_EQUAL_INT(td1_key2_size, cp2->key_size);
    TEST_ASSERT_EQUAL_INT(td1_data2_size, cp2->data_size);

    BTFreeBlock* fb2 = page_freeblock_at(page, 1);
    u32 expected_freespace2_offset = expected_entry2_offset - td1_freespace2_size;
    TEST_ASSERT_EQUAL_INT(expected_freespace2_offset, fb2->start_offset);
    TEST_ASSERT_EQUAL_INT(expected_freespace2_offset + td1_freespace2_size, fb2->end_offset);

    BTCellPtr* cp3 = page_find_cellptr(page, &td1_key3, td1_key3_size);
    u32 expected_entry3_offset = expected_freespace2_offset - td1_entry3_size;
    TEST_ASSERT_EQUAL_INT(expected_entry3_offset, cp3->offset);
    TEST_ASSERT_EQUAL_INT(td1_key3_size, cp3->key_size);
    TEST_ASSERT_EQUAL_INT(td1_data3_size, cp3->data_size);

    BTFreeBlock* fb1 = page_freeblock_at(page, 0);
    u32 expected_freespace1_offset = expected_entry3_offset - 22;
    TEST_ASSERT_EQUAL_INT(expected_freespace1_offset, fb1->start_offset);
    TEST_ASSERT_EQUAL_INT(expected_freespace2_offset + td1_freespace2_size, fb2->end_offset);
    TEST_ASSERT_EQUAL_INT(22, fb1->end_offset - fb1->start_offset);

    return btree;
}

//////////////////////////////////////////////////////////////////////
// Test data 2
const size_t td2_freespace3_size = 15;

const u32 td2_key1 = 1;
const size_t td2_key1_size = sizeof(u32);
const char* td2_data1 = "1111111111111111111111111111111111111111111111111";
const size_t td2_data1_size = 50;
const size_t td2_entry1_size = td2_key1_size + td2_data1_size;

const size_t td2_freespace2_size = 5;

const u32 td2_key2 = 4;
const size_t td2_key2_size = sizeof(u32);
const char* td2_data2 = "2222222222222222222222222222222222222222222222222";
const size_t td2_data2_size = 50;
const size_t td2_entry2_size = td2_key2_size + td2_data2_size;

const size_t td2_freespace1_size = 10;

const u32 td2_key3 = 8;
const size_t td2_key3_size = sizeof(u32);
const char* td2_data3 = "333333333333333333333333333333333333333333333";
const size_t td2_data3_size = 46;
const size_t td2_entry3_size = td2_key3_size + td2_data3_size;

void verify_test_data2(BTree* btree) {
    BTPage* page = buffer[btree->root_page_id];
    TEST_ASSERT_EQUAL_STRING(td2_data1, page_data_by_key(page, &td2_key1, td2_key1_size).data);
    TEST_ASSERT_EQUAL_STRING(td2_data2, page_data_by_key(page, &td2_key2, td2_key2_size).data);
    TEST_ASSERT_EQUAL_STRING(td2_data3, page_data_by_key(page, &td2_key3, td2_key3_size).data);
}

BTree* test_data2() {
    // Test page 2:
    //   total: 236 bytes
    //   freespace: 30 bytes
    //   used: 206 bytes
    //    - extra freeblocks: 2 (8 bytes)
    //    - cells: 3 (36 bytes)
    //    - data: (162 bytes) 
    //       1. 54 bytes (4 + 50)
    //       2. 54 bytes (4 + 50)
    //       3. 50 bytes (4 + 46)
    // --------------------------------------------
    // | d:50 | f:10 | d:54 | f:5 | d: 54 | f: 15 |
    // --------------------------------------------
    //
    ///////////
    // METADATA
    ///////////
    //     16      12      12      12       4          4        4
    // ----------------------------------------------------------------
    // | header | cell1 | cell2 | cell3 | fblock1 | fblock2 | fblock3 |
    // ----------------------------------------------------------------
    // 0       16      28      40       52       56         60        64
    //
    ///////
    // DATA
    ///////
    //    50        10        54        5       54        15
    // ----------------------------------------------------------
    // | entry3 | fspace1 | entry2 | fspace2 | entry1 | fspace3 |
    // ----------------------------------------------------------
    // 64      114       124      178       183      237       252

    BTree* btree = btree_new(&compare_integers);
    BTPage* page = buffer[btree->root_page_id];
    int expected_freespace = page->hdr->freespace;
    display_freeblocks(page);

    int frc1 = page_insert_freespace(page, td2_freespace3_size);
    TEST_ASSERT_EQUAL_INT(Ok, frc1);
    expected_freespace -= PAGE_FREE_BLOCK_SIZE;
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));
    display_freeblocks(page);

    int irc1 = page_leaf_set(page, &td2_key1, td2_key1_size, td2_data1, td2_data1_size);
    TEST_ASSERT_EQUAL_INT(Ok, irc1);
    expected_freespace -= (td2_entry1_size + PAGE_CELL_PTR_SIZE);
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));
    display_freeblocks(page);

    int frc2 = page_insert_freespace(page, td2_freespace2_size);
    TEST_ASSERT_EQUAL_INT(Ok, frc2);
    expected_freespace -= PAGE_FREE_BLOCK_SIZE;
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));
    display_freeblocks(page);

    int irc2 = page_leaf_set(page, &td2_key2, td2_key2_size, td2_data2, td2_data2_size);
    TEST_ASSERT_EQUAL_INT(Ok, irc2);
    expected_freespace -= (td2_entry2_size + PAGE_CELL_PTR_SIZE);
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));
    display_freeblocks(page);

    int frc3 = page_insert_freespace(page, td2_freespace1_size);
    TEST_ASSERT_EQUAL_INT(Ok, frc3);
    expected_freespace -= PAGE_FREE_BLOCK_SIZE;
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));
    display_freeblocks(page);

    int irc3 = page_leaf_set(page, &td2_key3, td2_key3_size, td2_data3, td2_data3_size);
    TEST_ASSERT_EQUAL_INT(Ok, irc3);
    expected_freespace -= (td2_entry3_size + PAGE_CELL_PTR_SIZE);
    expected_freespace += PAGE_FREE_BLOCK_SIZE; // first free block is removed since its consumed fully 
    TEST_ASSERT_EQUAL_INT(expected_freespace, page_compute_freespace(page));
    display_freeblocks(page);

    verify_test_data2(btree);

    // assert metadata, offsets etc
    TEST_ASSERT_EQUAL_INT(3, page->hdr->cell_count);
    TEST_ASSERT_EQUAL_INT(3, page->hdr->freeblock_count);
    TEST_ASSERT_EQUAL_INT(expected_freespace, page->hdr->freespace);

    BTFreeBlock* fb3 = page_freeblock_at(page, 2);
    u32 expected_freespace3_offset = PAGE_SIZE - td2_freespace3_size;
    TEST_ASSERT_EQUAL_INT(expected_freespace3_offset, fb3->start_offset);
    TEST_ASSERT_EQUAL_INT(expected_freespace3_offset + td2_freespace3_size, fb3->end_offset);

    BTCellPtr* cp1 = page_find_cellptr(page, &td2_key1, td2_key1_size);
    u32 expected_entry1_offset = expected_freespace3_offset - td2_entry1_size;
    TEST_ASSERT_EQUAL_INT(expected_entry1_offset, cp1->offset);
    TEST_ASSERT_EQUAL_INT(td2_key1_size, cp1->key_size);
    TEST_ASSERT_EQUAL_INT(td2_data1_size, cp1->data_size);

    BTFreeBlock* fb2 = page_freeblock_at(page, 1);
    u32 expected_freespace2_offset = expected_entry1_offset - td2_freespace2_size;
    TEST_ASSERT_EQUAL_INT(expected_freespace2_offset, fb2->start_offset);
    TEST_ASSERT_EQUAL_INT(expected_freespace2_offset + td2_freespace2_size, fb2->end_offset);

    BTCellPtr* cp2 = page_find_cellptr(page, &td2_key2, td2_key2_size);
    u32 expected_entry2_offset = expected_freespace2_offset - td2_entry2_size;
    TEST_ASSERT_EQUAL_INT(expected_entry2_offset, cp2->offset);
    TEST_ASSERT_EQUAL_INT(td2_key2_size, cp2->key_size);
    TEST_ASSERT_EQUAL_INT(td2_data2_size, cp2->data_size);

    BTFreeBlock* fb1 = page_freeblock_at(page, 0);
    u32 expected_freespace1_offset = expected_entry2_offset - td2_freespace1_size;
    TEST_ASSERT_EQUAL_INT(expected_freespace1_offset, fb1->start_offset);
    // TEST_ASSERT_EQUAL_INT(expected_freespace1_offset + td2_freespace1_size, fb1->end_offset);

    return btree;
}

//////////////////////////////////////////////////////////////////////////////////////////

void test_set_case1() {
    // - non-empty, there is enough space in first freeblock for cell and data
    // -  first freeblock == payload
    //    test page 1 (cell: 12, data: 11) 

    BTree* btree = test_data1();
    BTPage* page = buffer[btree->root_page_id];
    int initial_freeblock_count = page->hdr->freeblock_count;
    int zero_offset = page->freeblocks->start_offset;

    u32 key = 1234;
    size_t key_size = sizeof(u32);
    char* data = "12122";
    size_t data_size = 6;
    size_t entry_size = key_size + data_size;

    int rc = page_leaf_set(page, &key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);

    // assert that old and new data are ok
    verify_test_data1(btree);
    TEST_ASSERT_EQUAL_STRING(td1_data1, page_data_by_key(page, &td1_key1, td1_key1_size).data);

    // assert metadata, offsets etc

    // 1. initial free space was 67 bytes
    //    we consume 10 for data and 12 for cell ptr (22 in total)
    // 2. first freeblock entry will be removed because its consumed in full
    // 67 - 22 + 4 = 49
    TEST_ASSERT_EQUAL_INT(49, page->hdr->freespace);
    TEST_ASSERT_EQUAL_INT(49, page_compute_freespace(page));
    TEST_ASSERT_EQUAL_INT(initial_freeblock_count - 1, page->hdr->freeblock_count);

    BTCellPtr* cp = page_find_cellptr(page, &key, key_size);
    TEST_ASSERT_EQUAL_INT(key_size, cp->key_size);
    TEST_ASSERT_EQUAL_INT(data_size, cp->data_size);
    TEST_ASSERT_EQUAL_INT(zero_offset + PAGE_CELL_PTR_SIZE, cp->offset);

    btree_destroy(btree);
}

void test_set_case2() {
    // - non-empty, there is enough space in first freeblock for cell and data
    // -  first freeblock > payload
    //    test page 1 (cell: 12, data: 8) 

    BTree* btree = test_data1();
    BTPage* page = buffer[btree->root_page_id];
    int initial_freeblock_count = page->hdr->freeblock_count;
    int zero_offset = page->freeblocks->start_offset;


    u32 key = 1234;
    size_t key_size = sizeof(u32);
    char* data = "121";
    size_t data_size = 4;
    size_t entry_size = key_size + data_size;

    int rc = page_leaf_set(page, &key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);

    // assert that old and new data are ok
    verify_test_data1(btree);
    TEST_ASSERT_EQUAL_STRING(td1_data1, page_data_by_key(page, &td1_key1, td1_key1_size).data);

    // assert metadata, offsets etc

    // 1. initial free space was 67 bytes
    //    we consume 8 for data and 12 for cell ptr (20 in total)
    // 2. first freeblock entry will be shrinked
    // 67 - 20 = 47
    TEST_ASSERT_EQUAL_INT(47, page->hdr->freespace);
    TEST_ASSERT_EQUAL_INT(47, page_compute_freespace(page));
    TEST_ASSERT_EQUAL_INT(initial_freeblock_count, page->hdr->freeblock_count);

    BTFreeBlock* first_fb = page_freeblock_at(page, 0);
    TEST_ASSERT_EQUAL_INT(zero_offset + PAGE_CELL_PTR_SIZE, first_fb->start_offset);
    TEST_ASSERT_EQUAL_INT(zero_offset + PAGE_CELL_PTR_SIZE + 2, first_fb->end_offset);

    BTCellPtr* cp = page_find_cellptr(page, &key, key_size);
    TEST_ASSERT_EQUAL_INT(key_size, cp->key_size);
    TEST_ASSERT_EQUAL_INT(data_size, cp->data_size);
    TEST_ASSERT_EQUAL_INT(first_fb->end_offset, cp->offset);

    btree_destroy(btree);
}

void test_set_case3() {
    // non-empty, there is enough space in first freeblock for cell and in mid freeblock for data
    // - mid freeblock == data
    //   test page 1 (cell: 4, data: 20)

    BTree* btree = test_data1();
    BTPage* page = buffer[btree->root_page_id];
    int hist_freeblock_count = page->hdr->freeblock_count;
    int hist_freeblock1_offset = page->freeblocks->start_offset;
    int hist_freeblock2_offset = (page->freeblocks + 1)->start_offset;


    u32 key = 1234;
    size_t key_size = sizeof(u32);
    char* data = "555555555555555";
    size_t data_size = 16;
    size_t entry_size = key_size + data_size;

    int rc = page_leaf_set(page, &key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);

    // assert that old and new data are ok
    verify_test_data1(btree);
    TEST_ASSERT_EQUAL_STRING(td1_data1, page_data_by_key(page, &td1_key1, td1_key1_size).data);

    // assert metadata, offsets etc

    // 1. initial free space was 67 bytes
    //    we consume 20 for data and 12 for cell ptr (32 in total)
    // 2. first freeblock will be shrinked
    // 3. second freeblock will be consumed fully and removed
    // 67 - 32 + 4 = 39
    TEST_ASSERT_EQUAL_INT(39, page->hdr->freespace);
    TEST_ASSERT_EQUAL_INT(39, page_compute_freespace(page));
    TEST_ASSERT_EQUAL_INT(hist_freeblock_count - 1, page->hdr->freeblock_count);

    BTFreeBlock* first_fb = page_freeblock_at(page, 0);
    int fb_size = first_fb->end_offset - first_fb->start_offset;

    // init = 22
    // new cell = 12
    // removed freeblock = 4
    // 22 - 12 + 4 = 14
    TEST_ASSERT_EQUAL_INT(14, fb_size);
    int new_freeblock1_offset = hist_freeblock1_offset + 8;
    TEST_ASSERT_EQUAL_INT(new_freeblock1_offset, first_fb->start_offset);

    BTCellPtr* cp = page_find_cellptr(page, &key, key_size);
    TEST_ASSERT_EQUAL_INT(key_size, cp->key_size);
    TEST_ASSERT_EQUAL_INT(data_size, cp->data_size);
    TEST_ASSERT_EQUAL_INT(hist_freeblock2_offset, cp->offset);

    btree_destroy(btree);
}

void test_set_case4() {
    // non-empty, there is enough space in first freeblock for cell and in mid freeblock for data
    // - mid freeblock > data
    //   test page 1 (cell: 4, data: 15)

    BTree* btree = test_data1();
    BTPage* page = buffer[btree->root_page_id];
    int hist_freeblock_count = page->hdr->freeblock_count;
    int hist_freeblock1_offset = page->freeblocks->start_offset;
    int hist_freeblock2_offset = (page->freeblocks + 1)->start_offset;

    u32 key = 1234;
    size_t key_size = sizeof(u32);
    char* data = "5555555555";
    size_t data_size = 11;
    size_t entry_size = key_size + data_size;

    int rc = page_leaf_set(page, &key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);

    // assert that old and new data are ok
    verify_test_data1(btree);
    TEST_ASSERT_EQUAL_STRING(td1_data1, page_data_by_key(page, &td1_key1, td1_key1_size).data);

    // assert metadata, offsets etc

    // 1. initial free space was 67 bytes
    //    we consume 15 for data and 12 for cell ptr (27 in total)
    // 2. first freeblock will be shrinked
    // 3. second freeblock will be shrinked
    // 67 - 27 = 30
    TEST_ASSERT_EQUAL_INT(40, page->hdr->freespace);
    TEST_ASSERT_EQUAL_INT(40, page_compute_freespace(page));
    TEST_ASSERT_EQUAL_INT(hist_freeblock_count, page->hdr->freeblock_count);

    BTFreeBlock* fb1 = page_freeblock_at(page, 0);
    int fb1_size = fb1->end_offset - fb1->start_offset;

    // init = 22
    // new cell = 12
    // 22 - 12 = 10
    TEST_ASSERT_EQUAL_INT(10, fb1_size);
    int new_freeblock1_offset = hist_freeblock1_offset + 12;
    TEST_ASSERT_EQUAL_INT(new_freeblock1_offset, fb1->start_offset);
    TEST_ASSERT_EQUAL_INT(new_freeblock1_offset + fb1_size, fb1->end_offset);

    // init = 20
    // new data = 15
    // remainder = 5
    BTFreeBlock* fb2 = page_freeblock_at(page, 1);
    int fb2_size = fb2->end_offset - fb2->start_offset;
    TEST_ASSERT_EQUAL_INT(5, fb2_size);
    TEST_ASSERT_EQUAL_INT(hist_freeblock2_offset, fb2->start_offset);

    BTCellPtr* cp = page_find_cellptr(page, &key, key_size);
    TEST_ASSERT_EQUAL_INT(key_size, cp->key_size);
    TEST_ASSERT_EQUAL_INT(data_size, cp->data_size);
    TEST_ASSERT_EQUAL_INT(fb2->start_offset + fb2_size, cp->offset);

    btree_destroy(btree);
}

void test_set_case5() {
    BTree* btree = test_data2();

    btree_destroy(btree);
}

// - empty and there is enough space


// 108 bytes of data 

// Test page 1:
// -----------------------------------------
// | f:15 | ... | f:20 | .... | f: 25 | ...|
// -----------------------------------------

// Test page 2:
// -----------------------------------------
// | ... | f:5 | ... | f:5 | .... | f: 15  |
// -----------------------------------------

// Test page 3:
// -----------------------------------------
// | f:5 | ... | f:5 | .... | f: 5  |......|
// -----------------------------------------

// - BOTH :

// - non-empty, there is enough space in first freeblock for cell and data

// -       first freeblock == payload
//          test page 1 (cell: 4, data: 11) 

// -       first freeblock > payload 
//          test page 1 (cell: 4, data: 8) 

// - non-empty, there is enough space in first freeblock for cell and in mid freeblock for data

// -       mid freeblock == data
//          test page 1 (cell: 4, data: 20)

// -       mid freeblock > data
//          test page 1 (cell: 4, data: 23)

// - non-empty, there is not enough space for both cell and data

// -       defrag can help
//          test page 2 (cell: 4, data: 17)

// -       defrag can't help
//          test page 2 (cell: 4, data: 23)

// - CELL ONLY:

// - non-empty, there is enough space in first freeblock for cell but not for data

// -       defrag can help
//          test page 1 (cell: 4, data: 27)

// -       defrag can't help
//          test page 3 (cell: 4, data: 27)

// - DATA ONLY:

// - non-empty, there is not enough space in first freeblock for cell

// -      defrag can help
//         test page 2 (cell: 4, data: 4)    

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_page_new);
    RUN_TEST(test_set_when_empty);
    RUN_TEST(test_set_case1);
    RUN_TEST(test_set_case2);
    RUN_TEST(test_set_case3);
    RUN_TEST(test_set_case4);
    RUN_TEST(test_set_case5);
    return UNITY_END();
}

void setUp(void) {
}

void tearDown(void) {
    reset_buffer();
}
