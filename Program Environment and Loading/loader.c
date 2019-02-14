#include "elf32.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main(int argc, char **argv, char **envp) {

    while (*envp) envp++;
    int *auxp = envp + 1;

    while (*auxp) {
        if (*auxp == 3) {
            *auxp = 100;
        }

        auxp += 2;
    }

    FILE *fd;
    if ((fd = fopen(argv[1], "r")) < 0) {
        perror("open");
        exit(1);
    }

    struct elf32_ehdr ehdr;
    fread(&ehdr, sizeof(ehdr), 1, fd);
    // assert(ehdr.e_phentsize == sizeof(struct elf32_phdr));
    int n = ehdr.e_phnum;
    struct elf32_phdr phdrs[n];
    fseek(fd, ehdr.e_phoff, SEEK_SET);
    fread(phdrs, sizeof(phdrs), 1, fd);

    const int round = 0xFFFFF000;

    for (int i = 0; i < n; i++) {
        struct elf32_phdr hdr = phdrs[i];
        if (hdr.p_type == PT_LOAD) {

            int end = hdr.p_vaddr + hdr.p_memsz;
            int base = (int) hdr.p_vaddr & round;

            int endRound = (end + 0xFFF) & round;
            int totallen = endRound - base;

            char *buf = mmap(base, totallen,
                             PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

            fseek(fd, hdr.p_offset, SEEK_SET);

            fread(buf, hdr.p_filesz, 1, fd);

            int exec = (hdr.p_flags & PF_X) > 0;
            int read = (hdr.p_flags & PF_R) > 0;
            int write = (hdr.p_flags & PF_W) > 0;

            //mprotect(base, totallen,
            //         (exec * PROT_EXEC) || (read * PROT_READ) || (write * PROT_WRITE));
        }
    }

    int *p = (int *) argv;
    p[0] = argc - 1;
    p[-1] = ehdr.e_entry;
    __asm__("movl $0, %edx\n");
    __asm__("movl %0, %%esp\n" : : "r" (p - 1) : );
    __asm__("ret");


    return 0;
}

