// myos/kernel/string.h

#ifndef STRING_H
#define STRING_H

#include "types.h"

// Compares two strings. Returns 0 if they are equal.
int strcmp(const char* str1, const char* str2);
int atoi(const char* str);
void* memset(void* buf, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
int toupper(int c);
size_t strlen(const char* str);

#endif