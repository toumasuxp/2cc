TARGET=2cc

TEST_LIBPATH = -lgtest -lgtest_main

TEST_INCLUDE = -I/usr/local/include/gtest

ALL_REQUEST= main.c   \
		 parse.c  \
		 file.c   \
		 lex.c    \
		 vector.c \
		 buffer.c \
		 gen.c \
		 map.c


$(TARGET): $(ALL_REQUEST)
	gcc -o $@ $^


clean:
	rm 2cc

.PHONY: clean