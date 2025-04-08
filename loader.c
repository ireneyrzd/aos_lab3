#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <ucontext.h>
#include <assert.h>

#define PAGE_ALIGN(addr, page_size) ((addr) & ~((page_size) - 1))
#define PAGE_OFFSET(addr, page_size) ((addr) & ((page_size) - 1))
#define PUSH(type, value)                \
    do                                   \
    {                                    \
        top_of_stack -= sizeof(type);    \
        *(type *)top_of_stack = (value); \
    } while (0)

#define MAX_SEGMENTS 16
// gcc -T default.ld -g -o loader loader.c -static -lc
// ./loader ./hello
// gdb --args ./loader ./hello
// gcc -g -static -o hook_test hook_test.c

// store loadable segment info.
typedef struct
{
    uint64_t vaddr;  // start virtual address
    uint64_t memsz;  // memory size of segment
    uint64_t filesz; // file size of segment
    off_t offset;    // offset in file
    int prot;        // protection flags
} segment_t;

static segment_t segments[MAX_SEGMENTS];
static int num_segments = 0;
static long page_size;
static int elf_fd = -1; // File descriptor for the ELF file (global so the signal handler can use it)

void demand_page_handler(int sig, siginfo_t *si, void *unused);
void stack_check(void *top_of_stack, uint64_t argc, char **argv);

//
// Setup the loadable segments: parse the ELF headers and store segment info.
//
void load_elf_segments(const char *filename)
{
    elf_fd = open(filename, O_RDONLY);
    // printf("elf_fd: %d\n", elf_fd);
    if (elf_fd < 0)
    {
        perror("open ELF file");
        exit(EXIT_FAILURE);
    }
    struct stat st;
    if (fstat(elf_fd, &st) < 0)
    {
        perror("fstat");
        close(elf_fd);
        exit(EXIT_FAILURE);
    }
    // printf("st.st_size: %ld\n", st.st_size);
    // Read the ELF header.
    Elf64_Ehdr ehdr;
    if (read(elf_fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
    {
        perror("read ELF header");
        close(elf_fd);
        exit(EXIT_FAILURE);
    }
    // printf("ehdr.e_phoff: %ld\n", ehdr.e_phoff);
    // Check that it's a valid ELF file.
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
    {
        fprintf(stderr, "Not a valid ELF file.\n");
        close(elf_fd);
        exit(EXIT_FAILURE);
    }
    // printf("ehdr.e_phnum: %d\n", ehdr.e_phnum);
    // Seek to the program header table.
    if (lseek(elf_fd, ehdr.e_phoff, SEEK_SET) < 0)
    {
        perror("lseek");
        close(elf_fd);
        exit(EXIT_FAILURE);
    }
    // printf("num of segments: %d\n", num_segments);
    // Process each program header.
    for (int i = 0; i < ehdr.e_phnum; i++)
    {
        Elf64_Phdr phdr;
        if (read(elf_fd, &phdr, sizeof(phdr)) != sizeof(phdr))
        {
            perror("read program header");
            close(elf_fd);
            exit(EXIT_FAILURE);
        }
        if (phdr.p_type != PT_LOAD)
            continue;
        if (num_segments >= MAX_SEGMENTS)
        {
            fprintf(stderr, "Too many segments!\n");
            exit(EXIT_FAILURE);
        }
        // Store segment information.
        segments[num_segments].vaddr = phdr.p_vaddr;
        segments[num_segments].memsz = phdr.p_memsz;
        segments[num_segments].filesz = phdr.p_filesz;
        segments[num_segments].offset = phdr.p_offset;
        int prot = 0;
        if (phdr.p_flags & PF_R)
            prot |= PROT_READ;
        if (phdr.p_flags & PF_W)
            prot |= PROT_WRITE;
        if (phdr.p_flags & PF_X)
            prot |= PROT_EXEC;
        segments[num_segments].prot = prot;
        num_segments++;
    }
    // printf("done processing program headers\n");

    // Map the segments.
    for (int i = 0; i < num_segments; i++)
    {
        // printf("mapping segment %d\n", i);
        uint64_t seg_vaddr = segments[i].vaddr;
        uint64_t seg_filesz = segments[i].filesz;
        uint64_t seg_memsz = segments[i].memsz;
        off_t seg_offset = segments[i].offset;
        int prot = segments[i].prot;

        // Get system page size
        long page_size = sysconf(_SC_PAGESIZE);
        if (page_size < 0)
        {
            perror("sysconf");
            exit(EXIT_FAILURE);
        }

        // Calculate aligned addresses
        uint64_t aligned_vaddr = PAGE_ALIGN(seg_vaddr, page_size);
        uint64_t mem_end = seg_vaddr + seg_memsz;
        uint64_t aligned_mem_end = PAGE_ALIGN(mem_end + page_size - 1, page_size);
        size_t total_mem_size = aligned_mem_end - aligned_vaddr;

        // Map the entire region with temporary write permissions
        void *mapped = mmap((void *)aligned_vaddr, total_mem_size,
                            PROT_READ | PROT_WRITE, // Temporary permissions
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                            -1, 0);
        if (mapped == MAP_FAILED)
        {
            perror("mmap segment");
            exit(EXIT_FAILURE);
        }

        // Load file content directly into memory
        if (seg_filesz > 0)
        {
            ssize_t bytes_read = pread(elf_fd, (void *)seg_vaddr, seg_filesz, seg_offset);
            if (bytes_read != seg_filesz)
            {
                perror("pread");
                exit(EXIT_FAILURE);
            }
        }

        // Set final memory protections
        if (mprotect(mapped, total_mem_size, prot) < 0)
        {
            perror("mprotect");
            exit(EXIT_FAILURE);
        }
    }

    // printf("done mapping segments\n");
    // close(elf_fd); // Close ELF file after loading all segments
}

//
// Set up a new stack for the loaded program.
//
void *setup_new_stack(int argc, char **argv, uint64_t *envp_start, uint64_t *envp_end,
                      Elf64_auxv_t *auxv_start, Elf64_auxv_t *auxv_end)
{
    size_t stack_size = PAGE_ALIGN((long)1024 * 1024 * (long)10000, page_size); // 1 MB stack

    // Allocate the stack with RW permissions.
    void *new_stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_stack == MAP_FAILED)
    {
        perror("mmap new stack");
        exit(EXIT_FAILURE);
    }

    // Top of the stack, stack grows downwards
    char *top_of_stack = (char *)new_stack + stack_size;
    // printf("top_of_stack: %p\n", top_of_stack);

    // Align the stack pointer to 16 bytes.
    top_of_stack = (char *)((uintptr_t)top_of_stack & ~0xF);

    // PUSH(char *, NULL); // Terminate auxv

    // Push auxiliary vector entries
    // printf("starting to push auxv. start: %p, end: %p\n", auxv_start, auxv_end);
    for (Elf64_auxv_t *p = auxv_end; p >= auxv_start; p--)
    {
        // printf("auxv: %lx %lx\n", p->a_type == AT_NULL ? 0 : "NULL", p->a_un.a_val);
        PUSH(Elf64_auxv_t, *p);
    }

    // PUSH(char *, NULL); // Terminate envp

    // Push environment variables and NULL terminator
    for (uint64_t *p = envp_end; p >= envp_start; p--)
    {
        PUSH(char *, *p);
    }
    // PUSH(char *, NULL); // Terminate argv

    // Push argv parameters and NULL terminator
    for (int i = argc; i >= 0; i--)
    {
        PUSH(char *, argv[i]);
    }

    // Push argc
    PUSH(long, argc);

    // printf("top_of_stack: %p\n", top_of_stack);
    return (void *)top_of_stack;
}

//
// Clear registers and transfer control to the entry point of the loaded ELF.
// This function uses inline assembly to clear registers (except stack pointer and instruction pointer)
// and jump to the entry point. This is a simplified version; careful adjustments may be needed.
//
void transfer_control(void *entry_point, void *new_stack)
{
    __asm__ volatile(
        // Switch to new stack and push entry point
        "mov %[stack], %%rsp\n\t" // Set stack pointer to new_stack
        "push %[entry]\n\t"       // Push entry point to new stack

        // Zero all general purpose registers
        "xor %%rax, %%rax\n\t"
        "xor %%rbx, %%rbx\n\t"
        "xor %%rcx, %%rcx\n\t"
        "xor %%rdx, %%rdx\n\t"
        "xor %%rsi, %%rsi\n\t"
        "xor %%rdi, %%rdi\n\t"
        "xor %%rbp, %%rbp\n\t"
        "xor %%r8, %%r8\n\t"
        "xor %%r9, %%r9\n\t"
        "xor %%r10, %%r10\n\t"
        "xor %%r11, %%r11\n\t"
        "xor %%r12, %%r12\n\t"
        "xor %%r13, %%r13\n\t"
        "xor %%r14, %%r14\n\t"
        "xor %%r15, %%r15\n\t"

        // Clear direction flag and jump
        "cld\n\t" // Clear direction flag
        "ret\n\t" // Pop entry point from stack and jump

        // Input/output operands
        :
        : [stack] "r"(new_stack),
          [entry] "r"(entry_point)
        : "memory");

    // Compiler hint that we won't return
    __builtin_unreachable();
}
//
// Main function.
// Usage: ./loader <elf_executable>
//
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <elf_executable>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    page_size = sysconf(_SC_PAGESIZE); // store page size
    if (page_size <= 0)
    {
        perror("sysconf");
        exit(EXIT_FAILURE);
    }

    // Set up SIGSEGV handler for demand paging.
    // setup_sigsegv_handler();

    // Load the ELF segments from the file.
    load_elf_segments(argv[1]);

    // Print out the loaded segments.
    // printf("Loaded %d segments:\n", num_segments);
    // for (int i = 0; i < num_segments; i++)
    // {
    //     printf("  Segment %d: vaddr=0x%lx, filesz=0x%lx, memsz=0x%lx, prot=0x%x\n",
    //            i, segments[i].vaddr, segments[i].filesz, segments[i].memsz, segments[i].prot);
    // }

    // run stack check for loader stack
    stack_check((void *)(argv - 1), argc, argv);

    void *end_of_stack = &argv[argc]; // NULL terminator for argv

    uint64_t *start_of_envp = (uint64_t *)&argv[argc + 1]; // envp[0]
    uint64_t *end_of_envp = &start_of_envp[0];
    while (*end_of_envp != 0)
    {
        end_of_envp++;
    }
    // printf("end of envp: %p\n", end_of_envp);
    Elf64_auxv_t *start_of_auxv = (Elf64_auxv_t *)(end_of_envp + 1); // auxv[0]
    Elf64_auxv_t *end_of_auxv = &start_of_auxv[0];
    while (end_of_auxv->a_type != AT_NULL)
    {
        end_of_auxv++;
    }

    // Set up a new stack for the loaded program.
    void *new_stack = setup_new_stack(argc, argv, start_of_envp, end_of_envp, start_of_auxv, end_of_auxv);

    // printf("New stack set up at %p\n", new_stack);

    // Check the stack.
    stack_check(new_stack, argc, argv);

    // Save the entry point from the ELF header.
    // We re-read the ELF header from the file.
    if (lseek(elf_fd, 0, SEEK_SET) < 0)
    {
        perror("lseek to ELF header");
        exit(EXIT_FAILURE);
    }
    Elf64_Ehdr ehdr;
    if (read(elf_fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
    {
        perror("read ELF header");
        exit(EXIT_FAILURE);
    }
    void *entry_point = (void *)ehdr.e_entry;
    // printf("Transferring control to entry point: %p\n", entry_point);

    // Close the ELF file; further demand paging may require it open.
    // (For a robust solution, you might keep it open until the new program is running.)
    close(elf_fd);

    // Transfer control to the loaded executable.
    transfer_control(entry_point, new_stack);

    // Unreachable.
    return 0;
}

/**
 * Routine for checking stack made for child program.
 * top_of_stack: stack pointer that will given to child program as %rsp
 * argc: Expected number of arguments
 * argv: Expected argument strings
 */
void stack_check(void *top_of_stack, uint64_t argc, char **argv)
{
    // printf("----- stack check -----\n");

    assert(((uint64_t)top_of_stack) % 8 == 0);
    // printf("top of stack is 8-byte aligned\n");

    uint64_t *stack = (uint64_t *)top_of_stack;
    uint64_t actual_argc = *(stack++);
    // printf("argc: %lu\n", actual_argc);
    assert(actual_argc == argc);
    // printf("argc matches\n");

    for (int i = 0; i < argc; i++)
    {
        char *argp = (char *)*(stack++);
        assert(strcmp(argp, argv[i]) == 0);
        // printf("arg %d: %s\n", i, argp);
    }
    // Argument list ends with null pointer
    assert(*(stack++) == 0);

    int envp_count = 0;
    while (*(stack++) != 0)
    {
        envp_count++;
        // printf("env %d: %p\n", *(stack - 1));
    }

    // printf("env count: %d\n", envp_count);

    Elf64_auxv_t *auxv_start = (Elf64_auxv_t *)stack;
    Elf64_auxv_t *auxv_null = auxv_start;
    while (auxv_null->a_type != AT_NULL)
    {
        // printf("auxv: %lx %lx\n", auxv_null->a_type == AT_NULL ? 0 : "NULL", auxv_null->a_un.a_val);
        auxv_null++;
    }
    // printf("aux count: %lu\n", auxv_null - auxv_start);
    // printf("----- end stack check -----\n");
}