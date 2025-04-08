
// gcc -o hook_test hook_test.c

// gcc -shared -fPIC -o libmalloc_hook.so hook.c -ldl
// LD_PRELOAD=./libmalloc_hook.so ./hook_test
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

static void *(*real_malloc)(size_t) = NULL;
static int malloc_hook_enabled = 0;

void __attribute__((constructor)) init_malloc_hook()
{
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    if (!real_malloc)
    {
        fprintf(stderr, "Error loading real malloc: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }
}

void *malloc(size_t size)
{
    if (malloc_hook_enabled)
    {
        return real_malloc(size);
    }

    malloc_hook_enabled = 1;

    printf("malloc(%zu)\n", size);
    void *ptr = real_malloc(size);
    printf(" -> %p\n", ptr);

    if (ptr && size > 0)
    {
        memset(ptr, 0xCC, size); // Fill with 0xAA to detect uninitialized memory
    }

    malloc_hook_enabled = 0;
    return ptr;
}