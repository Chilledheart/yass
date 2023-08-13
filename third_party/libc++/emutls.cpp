//===----------------------- emutls.cpp --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Bridge gcc's __thread keyword to libc++ implementation
// useful when many mingw64 implementations changed to posix thread model (slow on window)

#include <__threading_support>

extern "C" {
void __cdecl __attribute__((noreturn)) abort(void);

void* __cdecl calloc(size_t _NumOfElements, size_t _SizeOfElements);
void __cdecl free(void* _Memory);
void* __cdecl malloc(size_t _Size);
void* __cdecl realloc(void* _Memory, size_t _NewSize);
}

typedef unsigned int word __attribute__((mode(word)));
typedef unsigned int pointer __attribute__((mode(pointer)));

// Hopefully, this ABI wouldn't change
extern "C"
struct __emutls_object {
  word size; // Size of TLS object
  word align; // Alignment of TLS object
  union {
    pointer offset;
    void *ptr;
  } loc;
  void *templ;
};

extern "C" _LIBCPP_EXPORTED_FROM_ABI
void* __emutls_get_address(struct __emutls_object*);

extern "C" _LIBCPP_EXPORTED_FROM_ABI
void __emutls_register_common(struct __emutls_object* obj,
                              word size,
                              word align,
                              void* templ);

static std::__libcpp_tls_key emutls_key;
static std::__libcpp_mutex_t emutls_mutex = _LIBCPP_MUTEX_INITIALIZER;
static pointer emutls_size;

struct _aligned_storage {
  void *ptr;
  char data[0];
};

// https://devblogs.microsoft.com/oldnewthing/20160613-00/?p=93655
// see also TLS_MINIMUM_AVAILABLE
#define EMULATED_THREADS_TSS_SLOTNUM 1024
typedef void** __emutls_arr_t;

// plain implementation of vc's _aligned_malloc
static void* __restrict__ _aligned_malloc(size_t size, size_t alignment) {
  _aligned_storage *storage;
  void *memblock;
  if (alignment <= sizeof(void*)) {
    storage = static_cast<_aligned_storage*>(malloc(sizeof(_aligned_storage) + size));
    if (storage == nullptr) {
      abort();
    }
    memblock = storage->data;
  } else {
    storage = static_cast<_aligned_storage*>(malloc(sizeof(_aligned_storage) + size + alignment - 1));
    if (storage == nullptr) {
      abort();
    }
    memblock = (void*)(((pointer)(storage->data + alignment - 1)) &
                  ~(pointer)(alignment - 1));
  }
  reinterpret_cast<_aligned_storage*>((char*)memblock - __builtin_offsetof(_aligned_storage, data))->ptr = storage;

  return memblock;
}

// plain implementation of vc's _aligned_free
static void _aligned_free(void* memblock) {
  if (!memblock)
    return;
  free(reinterpret_cast<_aligned_storage*>((char*)memblock - __builtin_offsetof(_aligned_storage, data))->ptr);
}

static void _LIBCPP_TLS_DESTRUCTOR_CC emutls_destroy(void* ptr) {
  auto emutls_arr = static_cast<__emutls_arr_t>(ptr);
  for (int i = 0; i < EMULATED_THREADS_TSS_SLOTNUM; ++i) {
    _aligned_free(emutls_arr[i]);
    emutls_arr[i] = nullptr;
  }
  free(emutls_arr);
}

static void emutls_init(void) {
  if (std::__libcpp_tls_create(&emutls_key, emutls_destroy) != 0) {
    abort();
  }
#if _WIN32_WINNT < 0x0600
  std::__libcpp_mutex_init(&emutls_mutex);
#endif
}

static void* emutls_alloc(struct __emutls_object* obj) {
  void *ret = _aligned_malloc(obj->size, obj->align);

  if (obj->templ)
    __builtin_memcpy(ret, obj->templ, obj->size);
  else
    __builtin_memset(ret, 0, obj->size);

  return ret;
}

void* __emutls_get_address(struct __emutls_object* obj) {
  pointer offset = __atomic_load_n(&obj->loc.offset, __ATOMIC_ACQUIRE);

  if (__builtin_expect(offset == 0, 0)) {
    static std::__libcpp_exec_once_flag once = _LIBCPP_EXEC_ONCE_INITIALIZER;
    std::__libcpp_execute_once(&once, emutls_init);
    std::__libcpp_mutex_lock(&emutls_mutex);
    offset = obj->loc.offset;
    if (offset == 0) {
      offset = ++emutls_size;
      __atomic_store_n(&obj->loc.offset, offset, __ATOMIC_RELEASE);
    }
    std::__libcpp_mutex_unlock(&emutls_mutex);
  }

  if (__builtin_expect(offset > EMULATED_THREADS_TSS_SLOTNUM, 0)) {
    abort();
  }

  auto emutls_arr = static_cast<__emutls_arr_t>(std::__libcpp_tls_get(emutls_key));
  if (__builtin_expect(emutls_arr == nullptr, 0)) {
    emutls_arr = static_cast<__emutls_arr_t>(malloc(sizeof(void*) * EMULATED_THREADS_TSS_SLOTNUM));
    if (emutls_arr == nullptr) {
      abort();
    }
    __builtin_memset(emutls_arr, 0, sizeof(void*) * EMULATED_THREADS_TSS_SLOTNUM);
    if (std::__libcpp_tls_set(emutls_key, emutls_arr) != 0) {
      abort();
    }
  }

  auto ret = emutls_arr[offset - 1];
  if (__builtin_expect(ret == nullptr, 0)) {
    ret = emutls_alloc(obj);
    emutls_arr[offset - 1] = ret;
  }

  return ret;
}

void __emutls_register_common(struct __emutls_object* obj,
                              word size,
                              word align,
                              void* templ) {
  if (obj->size < size) {
    obj->size = size;
    obj->templ = nullptr;
  }
  if (obj->align < align)
    obj->align = align;
  if (templ && size == obj->size)
    obj->templ = templ;
}
