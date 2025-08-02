// myos/kernel/lib/string.c

#include <kernel/string.h>
#include <kernel/types.h>
#include <kernel/debug.h> // qemu_debug_* functions

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int atoi(const char* str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        res = res * 10 + str[i] - '0';
    }
    return res;
}

void* memset(void* buf, int c, size_t n) {
    unsigned char* p = buf;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return buf;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

// A byte-by-byte memcpy with debug output to find the exact faulting address.
void* memcpy_debug(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    
    qemu_debug_string("memcpy_debug: START dest=");
    qemu_debug_hex((uint32_t)dest);
    qemu_debug_string(" src=");
    qemu_debug_hex((uint32_t)src);
    qemu_debug_string(" n=");
    qemu_debug_hex(n);
    qemu_debug_string("\n");

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
        // This is too slow to print every byte, let's print every 16 bytes
        if (i > 0 && (i % 16 == 0)) {
            qemu_debug_string("  -> copied ");
            qemu_debug_hex(i);
            qemu_debug_string(" bytes\n");
        }
    }

    qemu_debug_string("memcpy_debug: END\n");
    return dest;
}

// A simple, safe string copy function.
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for ( ; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}
