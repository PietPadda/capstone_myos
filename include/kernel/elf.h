// myos/include/kernel/elf.h

#ifndef ELF_H
#define ELF_H

#include <kernel/types.h>

#define ELF_MAGIC 0x464C457F // corresponds to "\x7FELF"

// ELF Header Structure
typedef struct {
    uint32_t magic;
    uint8_t  ident[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

// Program Header Structure
typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) Elf32_Phdr;

// Program header types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4

#endif