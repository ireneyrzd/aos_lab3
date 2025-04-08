#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define SIZE 64

#define MAGIC 0xCC // (can be any value you want)

int main()
{
    printf("=== Memory Test Program ===\n");
    unsigned char *mem = malloc(SIZE);
    for (int i = 0; i < SIZE; i++)
        assert(mem[i] == MAGIC);

    printf("malloc test passed\n");
}
