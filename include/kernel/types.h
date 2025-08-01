// myos/include/kernel/types.h
#ifndef TYPES_H
#define TYPES_H

// Define fixed-width integer types for cross-platform compatibility
// Unsigned integer types
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
typedef unsigned long long uint64_t;

// Signed integer types
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

// Standard size type, essential for memory operations
typedef uint32_t size_t;

// Define a boolean type for our freestanding environment
typedef int bool;
#define true 1
#define false 0

// Definition for a null pointer constant
#define NULL ((void*)0)

#endif