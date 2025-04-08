#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 100000
#define ELEMENTS_PER_PAGE (PAGE_SIZE / sizeof(int))
#define ARRAY_SIZE (NUM_PAGES * ELEMENTS_PER_PAGE)

volatile int data[ARRAY_SIZE] = {1};
// volatile int data2[ARRAY_SIZE] = {1};
// volatile int data3[ARRAY_SIZE] = {1};
// volatile int data4[ARRAY_SIZE] = {1};
// volatile int data5[ARRAY_SIZE] = {1};

int main(void)
{
    for (long i = 0; i < 50 * ELEMENTS_PER_PAGE; i++)
    {
        data[i] = i * 100; // Unique value per page.
        // data2[i] = i * 200; // Unique value per page.
        // data3[i] = i * 300; // Unique value per page.
        // data4[i] = i * 400; // Unique value per page.
        // data5[i] = i * 500; // Unique value per page.
    }

    // Access and "touch" the entry from each page.
    for (long i = 0; i < 50 * ELEMENTS_PER_PAGE; i++)
    {
        data[i] = data[i] + 1; // Touch the page to ensure it's loaded.
        // data2[i] = data2[i] + 1; // Touch the page to ensure it's loaded.
        // data3[i] = data3[i] + 1; // Touch the page to ensure it's loaded.
        // data4[i] = data4[i] + 1; // Touch the page to ensure it's loaded.
        // data5[i] = data5[i] + 1; // Touch the page to ensure it's loaded.
    }
    return 0;
}
