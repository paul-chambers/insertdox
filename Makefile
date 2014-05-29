OBJS = insertdox.o parser.o bufferutils.o stringutils.o

CFLAGS += -ggdb -O3 -pedantic -std=c99 -Wall -Wextra -Wno-missing-field-initializers -Wunused

all: insertdox

insertdox: ${OBJS}

clean:
	rm -vf ${OBJS}

.PHONY: all clean

