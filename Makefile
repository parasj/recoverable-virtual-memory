CC = gcc
LIBS = .
SRC = src/rvm.c 
OBJ = $(SRC:.c=.o)
CFLAGS = -fPIC -ggdb -Wall -Wextra -std=gnu99
LDFLAGS = -Isrc -Lsrc src/rvm.o

all: rvm abort basic multi-abort multi truncate
	rm -rf src/rvm.o

rvm: $(SRC:.c=.o)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

abort: testcase/abort.c ${OBJ} ${OUT}
	$(CC) $(CFLAGS) testcase/abort.c -o bin/abort ${LDFLAGS}

basic: testcase/basic.c ${OBJ} ${OUT}
	$(CC) $(CFLAGS) testcase/basic.c -o bin/basic ${LDFLAGS}

multi-abort: testcase/multi-abort.c ${OBJ} ${OUT}
	$(CC) $(CFLAGS) testcase/multi-abort.c -o bin/multi-abort ${LDFLAGS}

multi: testcase/multi.c ${OBJ} ${OUT}
	$(CC) $(CFLAGS) testcase/multi.c -o bin/multi ${LDFLAGS}

truncate: testcase/truncate.c ${OBJ} ${OUT}
	$(CC) $(CFLAGS) testcase/truncate.c -o bin/truncate ${LDFLAGS}

clean:
	-@rm src/*.o bin/*.a bin/abort bin/basic bin/multi-abort bin/multi bin/truncate;
	@echo Cleaned!
