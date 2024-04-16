init_bin_dir:
	@if [ ! -d "./bin" ]; then @mkdir bin; fi

build:
	gcc src/btree.c

run:
	@gcc src/btree.c
	./a.out

test_build: init_bin_dir
	@gcc \
		-g \
		-Wextra \
		-fsanitize=address \
		-DTEST \
		-o ./bin/test_btree \
		src/test_btree.c \
		lib/unity/unity.c

test: test_build
	./bin/test_btree

test_memcheck: test_build
	valgrind --track-origins=yes --leak-check=full ./bin/test_btree
