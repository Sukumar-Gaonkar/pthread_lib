CC = gcc
CFLAGS = -g -w

all:: vectorMultiply

parallelCal: 
	$(CC) $(CFLAGS) -pthread -o parallelCal parallelCal.c -L../ -lpthread

vectorMultiply: 
	$(CC) $(CFLAGS) -pthread -o vectorMultiply vectorMultiply.c -L../ -lpthread

externalCal: 
	$(CC) $(CFLAGS) -pthread -o externalCal externalCal.c -L../ -lpthread

clean:
	rm -rf parallelCal vectorMultiply externalCal *.o ./record/
