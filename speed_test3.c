#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 10000
#define ELEMENTS_PER_PAGE (PAGE_SIZE / sizeof(int))
#define ARRAY_SIZE (NUM_PAGES * ELEMENTS_PER_PAGE)

volatile int data[ARRAY_SIZE];

int main(void)
{
    for (long i = 0; i < ARRAY_SIZE; i++)
    {
        data[i] = i * 100; // Unique value per page.
    }

    // // Access and "touch" the entry from each page.
    for (long i = 0; i < ARRAY_SIZE; i++)
    {
        data[i] = data[i] + 1; // Touch the page to ensure it's loaded.
    }
    return 0;
}
