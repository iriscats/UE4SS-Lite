/*
 * QuickJS C atomics definitions
 *
 * Copyright (c) 2023 Marcin Kolny
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
   // Use GCC builtins for version < 4.9
#  if((__GNUC__ << 16) + __GNUC_MINOR__ < ((4) << 16) + 9)
#    define GCC_BUILTIN_ATOMICS
#  endif
#endif

#if defined(_MSC_VER)
/* MSVC atomics using Interlocked functions */
#include <intrin.h>

#define _Atomic volatile

/* 8-bit atomic operations */
static __forceinline char _qjs_atomic_fetch_add_8(volatile char *obj, char arg) {
    return _InterlockedExchangeAdd8(obj, arg);
}

static __forceinline char _qjs_atomic_fetch_sub_8(volatile char *obj, char arg) {
    return _InterlockedExchangeAdd8(obj, -arg);
}

static __forceinline char _qjs_atomic_fetch_and_8(volatile char *obj, char arg) {
    return _InterlockedAnd8(obj, arg);
}

static __forceinline char _qjs_atomic_fetch_or_8(volatile char *obj, char arg) {
    return _InterlockedOr8(obj, arg);
}

static __forceinline char _qjs_atomic_fetch_xor_8(volatile char *obj, char arg) {
    return _InterlockedXor8(obj, arg);
}

static __forceinline char _qjs_atomic_exchange_8(volatile char *obj, char desired) {
    return _InterlockedExchange8(obj, desired);
}

static __forceinline char _qjs_atomic_load_8(volatile char *obj) {
    return _InterlockedOr8(obj, 0);
}

static __forceinline void _qjs_atomic_store_8(volatile char *obj, char desired) {
    _InterlockedExchange8(obj, desired);
}

static __forceinline int _qjs_atomic_compare_exchange_8(volatile char *obj, char *expected, char desired) {
    char old = _InterlockedCompareExchange8(obj, desired, *expected);
    if (old == *expected) {
        return 1;
    }
    *expected = old;
    return 0;
}

/* 16-bit atomic operations */
static __forceinline short _qjs_atomic_fetch_add_16(volatile short *obj, short arg) {
    return _InterlockedExchangeAdd16(obj, arg);
}

static __forceinline short _qjs_atomic_fetch_sub_16(volatile short *obj, short arg) {
    return _InterlockedExchangeAdd16(obj, -arg);
}

static __forceinline short _qjs_atomic_fetch_and_16(volatile short *obj, short arg) {
    return _InterlockedAnd16(obj, arg);
}

static __forceinline short _qjs_atomic_fetch_or_16(volatile short *obj, short arg) {
    return _InterlockedOr16(obj, arg);
}

static __forceinline short _qjs_atomic_fetch_xor_16(volatile short *obj, short arg) {
    return _InterlockedXor16(obj, arg);
}

static __forceinline short _qjs_atomic_exchange_16(volatile short *obj, short desired) {
    return _InterlockedExchange16(obj, desired);
}

static __forceinline short _qjs_atomic_load_16(volatile short *obj) {
    return _InterlockedOr16(obj, 0);
}

static __forceinline void _qjs_atomic_store_16(volatile short *obj, short desired) {
    _InterlockedExchange16(obj, desired);
}

static __forceinline int _qjs_atomic_compare_exchange_16(volatile short *obj, short *expected, short desired) {
    short old = _InterlockedCompareExchange16(obj, desired, *expected);
    if (old == *expected) {
        return 1;
    }
    *expected = old;
    return 0;
}

/* 32-bit atomic operations */
static __forceinline long _qjs_atomic_fetch_add_32(volatile long *obj, long arg) {
    return _InterlockedExchangeAdd(obj, arg);
}

static __forceinline long _qjs_atomic_fetch_sub_32(volatile long *obj, long arg) {
    return _InterlockedExchangeAdd(obj, -arg);
}

static __forceinline long _qjs_atomic_fetch_and_32(volatile long *obj, long arg) {
    return _InterlockedAnd(obj, arg);
}

static __forceinline long _qjs_atomic_fetch_or_32(volatile long *obj, long arg) {
    return _InterlockedOr(obj, arg);
}

static __forceinline long _qjs_atomic_fetch_xor_32(volatile long *obj, long arg) {
    return _InterlockedXor(obj, arg);
}

static __forceinline long _qjs_atomic_exchange_32(volatile long *obj, long desired) {
    return _InterlockedExchange(obj, desired);
}

static __forceinline long _qjs_atomic_load_32(volatile long *obj) {
    return _InterlockedOr(obj, 0);
}

static __forceinline void _qjs_atomic_store_32(volatile long *obj, long desired) {
    _InterlockedExchange(obj, desired);
}

static __forceinline int _qjs_atomic_compare_exchange_32(volatile long *obj, long *expected, long desired) {
    long old = _InterlockedCompareExchange(obj, desired, *expected);
    if (old == *expected) {
        return 1;
    }
    *expected = old;
    return 0;
}

/* 64-bit atomic operations */
static __forceinline __int64 _qjs_atomic_fetch_add_64(volatile __int64 *obj, __int64 arg) {
    return _InterlockedExchangeAdd64(obj, arg);
}

static __forceinline __int64 _qjs_atomic_fetch_sub_64(volatile __int64 *obj, __int64 arg) {
    return _InterlockedExchangeAdd64(obj, -arg);
}

static __forceinline __int64 _qjs_atomic_fetch_and_64(volatile __int64 *obj, __int64 arg) {
    return _InterlockedAnd64(obj, arg);
}

static __forceinline __int64 _qjs_atomic_fetch_or_64(volatile __int64 *obj, __int64 arg) {
    return _InterlockedOr64(obj, arg);
}

static __forceinline __int64 _qjs_atomic_fetch_xor_64(volatile __int64 *obj, __int64 arg) {
    return _InterlockedXor64(obj, arg);
}

static __forceinline __int64 _qjs_atomic_exchange_64(volatile __int64 *obj, __int64 desired) {
    return _InterlockedExchange64(obj, desired);
}

static __forceinline __int64 _qjs_atomic_load_64(volatile __int64 *obj) {
    return _InterlockedOr64(obj, 0);
}

static __forceinline void _qjs_atomic_store_64(volatile __int64 *obj, __int64 desired) {
    _InterlockedExchange64(obj, desired);
}

static __forceinline int _qjs_atomic_compare_exchange_64(volatile __int64 *obj, __int64 *expected, __int64 desired) {
    __int64 old = _InterlockedCompareExchange64(obj, desired, *expected);
    if (old == *expected) {
        return 1;
    }
    *expected = old;
    return 0;
}

/* Size-based dispatch macros */
#define atomic_fetch_add(obj, arg) \
    ((sizeof(*(obj)) == 8) ? (__int64)_qjs_atomic_fetch_add_64((volatile __int64*)(obj), (__int64)(arg)) : \
     (sizeof(*(obj)) == 4) ? (long)_qjs_atomic_fetch_add_32((volatile long*)(obj), (long)(arg)) : \
     (sizeof(*(obj)) == 2) ? (short)_qjs_atomic_fetch_add_16((volatile short*)(obj), (short)(arg)) : \
     (char)_qjs_atomic_fetch_add_8((volatile char*)(obj), (char)(arg)))

#define atomic_fetch_sub(obj, arg) \
    ((sizeof(*(obj)) == 8) ? (__int64)_qjs_atomic_fetch_sub_64((volatile __int64*)(obj), (__int64)(arg)) : \
     (sizeof(*(obj)) == 4) ? (long)_qjs_atomic_fetch_sub_32((volatile long*)(obj), (long)(arg)) : \
     (sizeof(*(obj)) == 2) ? (short)_qjs_atomic_fetch_sub_16((volatile short*)(obj), (short)(arg)) : \
     (char)_qjs_atomic_fetch_sub_8((volatile char*)(obj), (char)(arg)))

#define atomic_fetch_and(obj, arg) \
    ((sizeof(*(obj)) == 8) ? (__int64)_qjs_atomic_fetch_and_64((volatile __int64*)(obj), (__int64)(arg)) : \
     (sizeof(*(obj)) == 4) ? (long)_qjs_atomic_fetch_and_32((volatile long*)(obj), (long)(arg)) : \
     (sizeof(*(obj)) == 2) ? (short)_qjs_atomic_fetch_and_16((volatile short*)(obj), (short)(arg)) : \
     (char)_qjs_atomic_fetch_and_8((volatile char*)(obj), (char)(arg)))

#define atomic_fetch_or(obj, arg) \
    ((sizeof(*(obj)) == 8) ? (__int64)_qjs_atomic_fetch_or_64((volatile __int64*)(obj), (__int64)(arg)) : \
     (sizeof(*(obj)) == 4) ? (long)_qjs_atomic_fetch_or_32((volatile long*)(obj), (long)(arg)) : \
     (sizeof(*(obj)) == 2) ? (short)_qjs_atomic_fetch_or_16((volatile short*)(obj), (short)(arg)) : \
     (char)_qjs_atomic_fetch_or_8((volatile char*)(obj), (char)(arg)))

#define atomic_fetch_xor(obj, arg) \
    ((sizeof(*(obj)) == 8) ? (__int64)_qjs_atomic_fetch_xor_64((volatile __int64*)(obj), (__int64)(arg)) : \
     (sizeof(*(obj)) == 4) ? (long)_qjs_atomic_fetch_xor_32((volatile long*)(obj), (long)(arg)) : \
     (sizeof(*(obj)) == 2) ? (short)_qjs_atomic_fetch_xor_16((volatile short*)(obj), (short)(arg)) : \
     (char)_qjs_atomic_fetch_xor_8((volatile char*)(obj), (char)(arg)))

#define atomic_exchange(obj, desired) \
    ((sizeof(*(obj)) == 8) ? (__int64)_qjs_atomic_exchange_64((volatile __int64*)(obj), (__int64)(desired)) : \
     (sizeof(*(obj)) == 4) ? (long)_qjs_atomic_exchange_32((volatile long*)(obj), (long)(desired)) : \
     (sizeof(*(obj)) == 2) ? (short)_qjs_atomic_exchange_16((volatile short*)(obj), (short)(desired)) : \
     (char)_qjs_atomic_exchange_8((volatile char*)(obj), (char)(desired)))

#define atomic_load(obj) \
    ((sizeof(*(obj)) == 8) ? (__int64)_qjs_atomic_load_64((volatile __int64*)(obj)) : \
     (sizeof(*(obj)) == 4) ? (long)_qjs_atomic_load_32((volatile long*)(obj)) : \
     (sizeof(*(obj)) == 2) ? (short)_qjs_atomic_load_16((volatile short*)(obj)) : \
     (char)_qjs_atomic_load_8((volatile char*)(obj)))

#define atomic_store(obj, desired) \
    ((sizeof(*(obj)) == 8) ? (void)_qjs_atomic_store_64((volatile __int64*)(obj), (__int64)(desired)) : \
     (sizeof(*(obj)) == 4) ? (void)_qjs_atomic_store_32((volatile long*)(obj), (long)(desired)) : \
     (sizeof(*(obj)) == 2) ? (void)_qjs_atomic_store_16((volatile short*)(obj), (short)(desired)) : \
     (void)_qjs_atomic_store_8((volatile char*)(obj), (char)(desired)))

#define atomic_compare_exchange_strong(obj, expected, desired) \
    ((sizeof(*(obj)) == 8) ? _qjs_atomic_compare_exchange_64((volatile __int64*)(obj), (__int64*)(expected), (__int64)(desired)) : \
     (sizeof(*(obj)) == 4) ? _qjs_atomic_compare_exchange_32((volatile long*)(obj), (long*)(expected), (long)(desired)) : \
     (sizeof(*(obj)) == 2) ? _qjs_atomic_compare_exchange_16((volatile short*)(obj), (short*)(expected), (short)(desired)) : \
     _qjs_atomic_compare_exchange_8((volatile char*)(obj), (char*)(expected), (char)(desired)))

#elif defined(GCC_BUILTIN_ATOMICS)
#define atomic_fetch_add(obj, arg) \
    __atomic_fetch_add(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_strong(obj, expected, desired) \
    __atomic_compare_exchange_n(obj, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_exchange(obj, desired) \
    __atomic_exchange_n (obj, desired, __ATOMIC_SEQ_CST)
#define atomic_load(obj) \
    __atomic_load_n(obj, __ATOMIC_SEQ_CST)
#define atomic_store(obj, desired) \
    __atomic_store_n(obj, desired, __ATOMIC_SEQ_CST)
#define atomic_fetch_or(obj, arg) \
    __atomic_fetch_or(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_fetch_xor(obj, arg) \
    __atomic_fetch_xor(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_fetch_and(obj, arg) \
    __atomic_fetch_and(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_fetch_sub(obj, arg) \
    __atomic_fetch_sub(obj, arg, __ATOMIC_SEQ_CST)
#define _Atomic
#else
#include <stdatomic.h>
#endif
