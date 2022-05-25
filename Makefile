APP = thread_test
OBJ = test.o thread_pool.o
CC = gcc
CFLAGS = -g -Wall -std=c99
DEPEND = -lpthread

$(APP) : $(OBJ)
	$(CC) $(CFLAGS) -o $(APP) $(OBJ) $(DEPEND)

memory_pool.o : thread_pool.c thrad_pool.h
	$(CC) $(CFLAGS) -c thread_pool.c -o thread_pool.o

test.o : test.c
	$(CC) $(CFLAGS) -c test.c -o test.o

clean :
	rm -f $(APP) $(OBJ)
