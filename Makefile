init_bin_dir:
	@if [ ! -d "./bin" ]; then @mkdir bin; fi

run_binsearch:
	@gcc src/binary_search.c 
	./a.out

run_collation:
	@gcc src/collation.c
	./a.out

run_btree:
	@gcc src/btree_leaf.c
	./a.out

test_build: init_bin_dir
	@gcc \
		-g \
		-Wextra \
		-fsanitize=address \
		-o ./bin/test.out \
		src/test.c \
		slotted_page.c \
		lib/unity/unity.c

test: test_build	
	./bin/test.out

test_memcheck: test_build
	valgrind --track-origins=yes --leak-check=full ./bin/test.out
