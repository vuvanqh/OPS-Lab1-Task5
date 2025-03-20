LDLIBS=-lpthread -lm

CC=gcc
CFLAGS= -std=gnu99 -Wall
L_FLAGS=-fsanitize=address,undefined

TARGET=prog
FILES=${TARGET}.o

.PHONY: clean all

${TARGET} : ${FILES}
	${CC} ${L_FLAGS} -o ${TARGET} ${FILES}

${TARGET}.o: ${TARGET}.c
	${CC} ${C_FLAGS} -o ${TARGET}.o -c ${TARGET}.c

prog2.c: 
	${CC} ${L_FLAGS} prog2.c -o prog2 
all: ${TARGET} prog2

clean:
	rm -f ${FILES} ${TARGET}