all: main.c
	gcc -g -Wall -pthread -o matmult.out main.c

clean:
	$(RM) matMult