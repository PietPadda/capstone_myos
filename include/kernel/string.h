// myos/include/kernel/string.h

#ifndef STRING_H
#define STRING_H

#include <kernel/types.h>

int strcmp(const char* str1, const char* str2);
int atoi(const char* str);
void* memset(void* buf, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
int toupper(int c);
size_t strlen(const char* str);

#endif