# Implementing Your Own Malloc in C: A Beginner's Guide (Unix & Windows)
Welcome to the world of memory allocation! Building your own `malloc` (memory allocator)
might sound intimidating, but it is entirely possible once you break it down into manageable
pieces. This guide will walk you through building a custom memory allocator that works on both
Unix-like systems (Linux/macOS) and Windows.
## Section 1: Asking the OS for Memory
### The Minimum Theory
Your program uses two main types of memory:
1. **[Stack](https://en.wikipedia.org/wiki/Stack-based_memory_allocation):** Small, automatic
memory for things with known sizes at compile time (like `int x = 5;`).
2. **[Heap](https://en.wikipedia.org/wiki/C_dynamic_memory_allocation):** Large, flexible
memory that can grow or shrink while your program runs.
The Operating System (OS) gives your program a chunk of heap space, but it won't organize it
for you. If you blindly write to the heap, you might overwrite your own data. We need a way to
track which parts of the heap are empty and which are occupied. `malloc()` is a function that
finds an empty spot in the heap, reserves it, and gives you a safe pointer to use.
To do this, we first need to ask the OS to give us raw heap space.
- On **Unix/Linux**, we use the `sbrk()` system call to move the end of the heap.
- On **Windows**, we use `HeapAlloc()` and `GetProcessHeap()` from the native Windows API.

#### for windows:
windows: To request memory from the heap using the native Windows API, the standard and most efficient function to use is HeapAlloc().

Before you can allocate memory from a heap, you need a handle (HANDLE) to that heap. You can either use the default heap provided to your process by calling GetProcessHeap, or create a private heap using HeapCreate.


When calling HeapAlloc, you can fine-tune its behavior with the following dwFlags:
1. HEAP_ZERO_MEMORY (0x8): Automatically clears the memory block, setting all bytes to zero.
2. HEAP_GENERATE_EXCEPTIONS (0x4): Tells the OS to raise a structured exception (STATUS_NO_MEMORY) upon failure instead of returning NULL.
3. HEAP_NO_SERIALIZE (0x1): Disables mutual exclusion (thread safety) for this call. Use this only if you are managing thread safety manually, as it offers a small performance boost but can lead to heap corruption if multiple threads access the heap simultaneously.

step we are gonna use: 
1. Get a Heap Handle: Retrieve the default heap handle for the calling process.
[read](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap)
2. Allocate Memory: Call HeapAlloc passing the handle, formatting flags, and the desired size in bytes.[read more](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc)

3. Free Memory: When finished, release the allocated block back to the heap using HeapFree
[read more](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc)




### High-Level Steps
1. Determine which Operating System the code is running on.
2. If Unix, use `sbrk()` to request a block of memory.
3. If Windows, get the process heap handle and use `HeapAlloc()` to request memory.
### Detailed Low-Level Steps
1. Use C preprocessor macros (`#ifdef _WIN32`) to separate Windows code from Unix code.
2. Create a generic function named `request_os_memory(size_t size)`.
3. **For Windows:** Include `<windows.h>`. Call `GetProcessHeap()` to get the handle. Then
call `HeapAlloc(handle, 0, size)` to get the memory.
4. **For Unix:** Include `<unistd.h>`. Call `sbrk(size)`. If it fails, it returns `(void*)-1`, which we
should handle.
### Pseudo Code
```text
FUNCTION request_os_memory(size)
IF operating system is Windows THEN
heap_handle = GetProcessHeap()
RETURN HeapAlloc(heap_handle, size)
ELSE IF operating system is Unix THEN
memory = sbrk(size)
IF memory == -1 THEN
RETURN NULL
END IF
RETURN memory
END IF
END FUNCTION
```
### The Exact Code
```c
#include <stddef.h> // For size_t
#include <stdio.h>
// 1. Determine the OS at compile time
#ifdef _WIN32
#include <windows.h> // Required for Windows API
#else
#include <unistd.h> // Required for Unix sbrk()
#endif
// A wrapper to request raw memory from the OS
void* request_os_memory(size_t size) {
#ifdef _WIN32
// Get the default heap handle for the current Windows process
HANDLE heap = GetProcessHeap();
if (heap == NULL) {
return NULL;
}
// Allocate memory using the Windows API
return HeapAlloc(heap, 0, size);
#else
// Request memory in Unix using sbrk
void* request = sbrk(size);
if (request == (void*) -1) {
return NULL; // sbrk failed
}
return request;
#endif
}
```
---