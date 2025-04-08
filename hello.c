#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global initialized data (stored in .data section)
int global_data = 0x12345678;

// Global uninitialized data (stored in .bss section)
char global_bss[4096];

// Function to test code segment execution
static void code_segment_test(void)
{
    printf("Code segment test passed\n");
}

int main(int argc, char *argv[])
{
    printf("\n=== Memory Test Program ===\n");

    // Test 1: Stack access
    int stack_var = 0xDEADBEEF;
    printf("Stack test:    %s\n",
           (stack_var == 0xDEADBEEF) ? "PASS" : "FAIL");

    // Test 2: Heap allocation
    char *heap_ptr = malloc(1024);
    strcpy(heap_ptr, "HEAP_TEST");
    int heap_test = strcmp(heap_ptr, "HEAP_TEST") == 0;
    printf("Heap test:     %s\n", heap_test ? "PASS" : "FAIL");
    free(heap_ptr);

    // Test 3: Global data access
    printf("Global data:   %s\n",
           (global_data == 0x12345678) ? "PASS" : "FAIL");

    // Test 4: BSS section access
    // memset(global_bss, 0xAA, sizeof(global_bss));
    // int bss_test = 1;
    // for (size_t i = 0; i < sizeof(global_bss); i++)
    // {
    //     if (global_bss[i] != 0xAA)
    //     {
    //         bss_test = 0;
    //         break;
    //     }
    // }
    // printf("BSS test:      %s\n", bss_test ? "PASS" : "FAIL");

    // Test 5: Code execution
    void (*code_ptr)(void) = code_segment_test;
    code_ptr();

    // Test 6: Memory map consistency
    int consistency = 1;
    consistency &= ((unsigned long)&main >= 0x400000);       // Typical code address
    consistency &= ((unsigned long)&global_data < 0x600000); // Typical data address
    consistency &= ((unsigned long)global_bss < 0x600000);   // Typical BSS address
    printf("Memory layout: %s\n", consistency ? "PASS" : "FAIL");

    // Test 7: Large allocation
    char *big = malloc(1024 * 1024); // 1MB
    memset(big, 0x55, 1024 * 1024);
    int big_test = 1;
    for (int i = 0; i < 1024 * 1024; i += 4096)
    {
        if (big[i] != 0x55)
        {
            big_test = 0;
            break;
        }
    }
    free(big);
    printf("Large alloc:   %s\n", big_test ? "PASS" : "FAIL");

    // Test 8: Function pointer comparison
    // printf("Code pointers: %s\n",
    //        ((void *)main < (void *)code_segment_test) ? "PASS" : "FAIL");

    printf("\nAll tests completed\n");

    // Return special value if all tests passed
    return (stack_var == 0xDEADBEEF &&
            heap_test &&
            global_data == 0x12345678 &&
            consistency &&
            big_test)
               ? 0x55AA
               : 1;
}