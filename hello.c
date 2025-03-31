// hello.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main(void)
{
    uint32_t *i = malloc(4);
    *i = 0x12345678;
    printf("hello world %x\n", *i);
    return 0;
}
