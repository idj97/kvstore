#include "../lib/unity/unity.h"
#include "btree.c"

#define asrt TEST_ASSERT

///////////
// HELPERS
///////////

int page_insert_intstr(BTPage* page, int* intkey, char* strdata) {
    const void* key = (const void*)intkey;
    u32 key_size = sizeof(int);
    const void* data = (const void*)strdata;
    u32 data_size = strlen(strdata) + 1;

    int rc = page_leaf_insert(
        page,
        key, key_size,
        data, data_size
    );
    return rc;
}

int page_get_intkey(BTPage* page, u16 pos) {
    Value key_value = page_key_at(page, pos);
    int vkey = *(int*)(key_value.data);
    return vkey;
}


char* page_get_chardata(BTPage* page, u16 pos) {
    Value data_value = page_data_at(page, pos);
    return (char*)(data_value.data);
}

/////////
// TESTS
/////////

void test_compare_integers() {
    int a1 = 4;
    int a2 = 3;
    int a3 = -5;
    asrt(compare_integers(&a1, 4, &a1, 4) == 0);
    asrt(compare_integers(&a1, 4, &a2, 4) > 0);
    asrt(compare_integers(&a2, 4, &a1, 4) < 0);
    asrt(compare_integers(&a1, 4, &a3, 4) > 0);
    asrt(compare_integers(&a3, 4, &a1, 4) < 0);
}

void test_page_new() {
    BTPage* page1 = page_new(NULL);
    BTPage* page2 = page_new(NULL);
    BTPage* page3 = page_new(NULL);
    asrt(page3->hdr->cell_count == 0);
    asrt(page3->hdr->freespace_end == PAGE_SIZE);
    asrt(page3->hdr->is_leaf == 0);
    asrt(page3->hdr->pid == 2);
    asrt(page3->hdr->rightmost_pid == 0);
    asrt(page3->hdr->rightmost_pid == 0);
}


void test_page_leaf_insert() {
    BTree* btree = btree_new(&compare_integers);
    BTPage* page = buffer[btree->root_page_id];

    int rc;

    int key = 5; char* data = "1213";
    rc = page_insert_intstr(page, &key, data);
    asrt(rc == Ok);

    int key2 = 1; char* data2 = "456";
    rc = page_insert_intstr(page, &key2, data2);
    asrt(rc == Ok);

    int key3 = 4; char* data3 = "789ab";
    rc = page_insert_intstr(page, &key3, data3);
    asrt(rc == Ok);

    TEST_ASSERT_EQUAL_INT(3, page->hdr->cell_count);

    // v1 + v2 + v3
    //  9 + 8 + 10 = 27 
    TEST_ASSERT_EQUAL_INT(PAGE_SIZE - 27, page->hdr->freespace_end);

    BTCellPtr* cellptr = page_cellptr_at(page, 0);
    TEST_ASSERT_EQUAL_INT(4, cellptr->key_size);
    TEST_ASSERT_EQUAL_INT(4, cellptr->data_size);
    TEST_ASSERT_EQUAL_INT(key2, page_get_intkey(page, 0));
    TEST_ASSERT_EQUAL_STRING(data2, page_get_chardata(page, 0));

    BTCellPtr* cellptr2 = page_cellptr_at(page, 1);
    TEST_ASSERT_EQUAL_INT(4, cellptr2->key_size);
    TEST_ASSERT_EQUAL_INT(6, cellptr2->data_size);
    TEST_ASSERT_EQUAL_INT(key3, page_get_intkey(page, 1));
    TEST_ASSERT_EQUAL_STRING(data3, page_get_chardata(page, 1));

    BTCellPtr* cellptr3 = page_cellptr_at(page, 2);
    TEST_ASSERT_EQUAL_INT(4, cellptr3->key_size);
    TEST_ASSERT_EQUAL_INT(5, cellptr3->data_size);
    TEST_ASSERT_EQUAL_INT(key, page_get_intkey(page, 2));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(page, 2));

    btree_destroy(btree);
}

void test_page_leaf_insert_when_full() {
    BTree* btree = btree_new(&compare_integers);
    BTPage* page = buffer[btree->root_page_id];

    // In total we have 128 bytes
    // there is 16 bytes for header
    // so there is 112 bytes for data
    // 
    // Five elemets will be added
    // each takes 22 bytes (12 for cell and 10 for data)
    // 
    // Adding 6th eleemnt of size 22 bytes will fail
    // because there is only 2 bytes available

    char* data = "12345";
    for (int key = 1; key <= 5; key++) {
        int rc = page_insert_intstr(page, &key, data);
        asrt(rc == Ok);
    }

    int key = 6;
    int rc = page_insert_intstr(page, &key, data);
    asrt(rc == NotEnoughSpace);

    btree_destroy(btree);
}


void test_page_leaf_overwrite() {
    BTree* btree = btree_new(&compare_integers);
    BTPage* page = buffer[btree->root_page_id];
    int rc;

    // In total we have 128 bytes
    // there is 16 bytes for header
    // so there is 112 bytes for data
    // 
    // Five elemets will be added
    // each takes 21 bytes (12 for cell and 9 for data)
    // 
    // 1. overwritting any of the elements with data of 
    //    size less then equal to 9 bytes must succeed 
    // 2. overwritting any element with data of size
    //    greater then 9 bytes must fail


    char* data = "1234";
    for (int key = 1; key <= 5; key++) {
        rc = page_insert_intstr(page, &key, data);
        asrt(rc == Ok);
    }

    int key = 2;

    char* new_data1 = "1111";
    rc = page_insert_intstr(page, &key, new_data1);
    asrt(rc == Ok);
    TEST_ASSERT_EQUAL_INT(2, page_get_intkey(page, 1));
    TEST_ASSERT_EQUAL_STRING(new_data1, page_get_chardata(page, 1));

    char* new_data2 = "22";
    rc = page_insert_intstr(page, &key, new_data2);
    asrt(rc == Ok);
    TEST_ASSERT_EQUAL_INT(2, page_get_intkey(page, 1));
    TEST_ASSERT_EQUAL_STRING(new_data2, page_get_chardata(page, 1));

    char* new_data3 = "333333333333333333333333333333333333333333";
    rc = page_insert_intstr(page, &key, new_data3);
    asrt(rc == PayloadTooBig);

    char* new_data4 = "12345";
    rc = page_insert_intstr(page, &key, new_data4);
    TEST_ASSERT_EQUAL_INT(NotEnoughSpace, rc);

    // double check that we everything is still fine...
    TEST_ASSERT_EQUAL_INT(1, page_get_intkey(page, 0));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(page, 0));

    TEST_ASSERT_EQUAL_INT(2, page_get_intkey(page, 1));
    TEST_ASSERT_EQUAL_STRING(new_data2, page_get_chardata(page, 1));

    TEST_ASSERT_EQUAL_INT(3, page_get_intkey(page, 2));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(page, 2));

    btree_destroy(btree);
}

void test_page_leaf_split() {
    BTree* btree = btree_new(&compare_integers);
    BTPage* page = buffer[btree->root_page_id];

    // In total we have 128
    // there is 16 bytes for header
    // so there is 112 bytes for data
    // 
    // Five elemets will be added
    // each takes 22 bytes (12 for cell and 10 for data)

    char* data = "12345";
    for (int key = 1; key <= 5; key++) {
        int rc = page_insert_intstr(page, &key, data);
        asrt(rc == Ok);
    }

    TEST_ASSERT_EQUAL_INT(2, page_freespace(page));

    BTPageSplitResult res = page_leaf_split(page);
    asrt(res.status == Ok);
    asrt(res.page == page);
    asrt(res.new_page != page);

    // 112 / 2 = 56 bytes
    // We copy all elements that sum up to 56 bytes in size to left page
    // and the rest we copy to the right page
    // 
    // In case of 22 bytes per element, that means that 2 elements will be
    // in left page and 3 elements will be in right page

    // check left page
    TEST_ASSERT_EQUAL_INT(page->hdr->pid, res.page->hdr->pid);
    TEST_ASSERT_EQUAL_INT(2, res.page->hdr->cell_count);
    TEST_ASSERT_EQUAL_INT(1, res.page->hdr->is_leaf);
    TEST_ASSERT_EQUAL_INT(128 - 2 * 10, res.page->hdr->freespace_end);
    // check elements in left page
    TEST_ASSERT_EQUAL_INT(1, page_get_intkey(res.page, 0));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(res.page, 0));
    TEST_ASSERT_EQUAL_INT(2, page_get_intkey(res.page, 1));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(res.page, 1));

    // check right page
    TEST_ASSERT(page->hdr->pid != res.new_page->hdr->pid);
    TEST_ASSERT_EQUAL_INT(3, res.new_page->hdr->cell_count);
    TEST_ASSERT_EQUAL_INT(1, res.new_page->hdr->is_leaf);
    TEST_ASSERT_EQUAL_INT(128 - 3 * 10, res.new_page->hdr->freespace_end);
    // check elements in right page
    TEST_ASSERT_EQUAL_INT(3, page_get_intkey(res.new_page, 0));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(res.new_page, 0));
    TEST_ASSERT_EQUAL_INT(4, page_get_intkey(res.new_page, 1));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(res.new_page, 1));
    TEST_ASSERT_EQUAL_INT(5, page_get_intkey(res.new_page, 2));
    TEST_ASSERT_EQUAL_STRING(data, page_get_chardata(res.new_page, 2));

    btree_destroy(btree);
}



void test_btree_new() {
    BTree* btree = btree_new(&compare_integers);
    btree->root_page_id = 0;
    btree_destroy(btree);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_compare_integers);
    RUN_TEST(test_page_new);
    RUN_TEST(test_btree_new);
    RUN_TEST(test_page_leaf_insert);
    RUN_TEST(test_page_leaf_insert_when_full);
    RUN_TEST(test_page_leaf_overwrite);
    RUN_TEST(test_page_leaf_split);
    return UNITY_END();
}

void setUp(void) {
}

void tearDown(void) {
    reset_buffer();
}
