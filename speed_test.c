#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 100000
#define ELEMENTS_PER_PAGE (PAGE_SIZE / sizeof(int))
#define ARRAY_SIZE (NUM_PAGES * ELEMENTS_PER_PAGE)

volatile int data[ARRAY_SIZE] = {1};

int main(void)
{
    // Initialize one entry per page with a meaningful value.
    // This simulates having sparse data where each page has one valid entry.
    for (long i = 0; i < NUM_PAGES; i++)
    {
        long index = i * ELEMENTS_PER_PAGE;
        data[index] = i * 100; // Unique value per page.
    }

    // Access and "touch" the entry from each page.
    for (long i = 0; i < NUM_PAGES; i++)
    {
        long index = i * ELEMENTS_PER_PAGE;
        data[index] = data[index] + 1; // Touch the page to ensure it's loaded.
    }

    return 0;
}
