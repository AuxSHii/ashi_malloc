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
• HEAP_ZERO_MEMORY (0x8): Automatically clears the memory block, setting all bytes to zero.
• HEAP_GENERATE_EXCEPTIONS (0x4): Tells the OS to raise a structured exception (STATUS_NO_MEMORY) upon failure instead of returning NULL.
• HEAP_NO_SERIALIZE (0x1): Disables mutual exclusion (thread safety) for this call. Use this only if you are managing thread safety manually, as it offers a small performance boost but can lead to heap corruption if multiple threads access the heap simultaneously.

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
## Section 2: Structuring Memory (Headers and Metadata)
### The Minimum Theory
When you call `malloc(32)`, how does `free()` later know you allocated exactly 32 bytes?
The secret is **Metadata** (data about data). Before the actual memory we give to the user, we
store a hidden **Block Header**. This header keeps track of the size of the block, whether it is
free or in use, and links to the next block.
By chaining these blocks together, we create a **[Linked
List](https://en.wikipedia.org/wiki/Linked_list)**. When `malloc` is called, we can traverse this list
to find a block that is marked "free" and is large enough to hold the requested data.
### High-Level Steps
1. Define a C `struct` to act as our block header.
2. Include variables for block size, a flag to check if it's free, and a pointer to the next block.
3. Keep a global variable pointing to the very first block in our heap (the head of our linked list).
### Detailed Low-Level Steps
1. Create `struct block_meta`.
2. Add `size_t size` (how much space is in this block).
3. Add `int free` (1 if free, 0 if in use).
4. Add `struct block_meta *next` (pointer to the next block).
5. Define a global pointer `global_base = NULL` to remember where our memory starts.
6. Create a constant for the size of this header, so we know exactly how much extra memory to
request from the OS.
### Pseudo Code
```text
STRUCTURE block_meta
size_t size
integer free
POINTER next_block
END STRUCTURE
GLOBAL POINTER global_base = NULL
MACRO META_SIZE = size of (block_meta)
```
### The Exact Code
```c
// The metadata header that sits right before the user's memory
struct block_meta {
size_t size; // Size of the memory block
int free; // 1 if free, 0 if in use
struct block_meta *next; // Pointer to the next block in the list
};
// Size of our struct. We need this to calculate total memory needed.
#define META_SIZE sizeof(struct block_meta)
// A global pointer keeping track of the start of our linked list
void *global_base = NULL;
```
---
## Section 3: Implementing `malloc` (Finding and Allocating Blocks)
### The Minimum Theory
When a user asks for memory, we don't just blindly ask the OS for more. We first check if we
already have a free block of memory that is large enough (maybe from a previous `free()` call).
We will use a strategy called **First Fit**: we traverse our linked list from the beginning and stop
at the *first* block that is free and large enough. If we reach the end of the list and find no
suitable block, *then* we ask the OS for more memory and attach it to the end of our list.
### High-Level Steps
1. If this is the very first time `malloc` is called, ask the OS for memory directly.
2. If not, traverse the linked list to find a free block large enough for the request.
3. If a block is found, mark it as in use and return it.
4. If no block is found, ask the OS for more memory, create a new header, and link it.
### Detailed Low-Level Steps
1. Create a helper function `find_free_block(last_block, size)` that loops through the linked list.
2. Create `request_space(last_block, size)` which uses our `request_os_memory` function. It
creates a new `block_meta`, sets its size, sets `free = 0`, and links it to `last_block`.
3. In the main `my_malloc(size)` function:
- Check if size <= 0. If so, return NULL.
- If `global_base` is NULL, call `request_space`.
- If not NULL, call `find_free_block`.
- If found, set `free = 0`.
- If not found, call `request_space`.
- Finally, return the pointer *after* the metadata block (i.e., `header + 1`).
### Pseudo Code
```text
FUNCTION my_malloc(size)
IF size <= 0 THEN RETURN NULL
IF global_base is NULL THEN
block = request_space(NULL, size)
global_base = block
ELSE
block = find_free_block(last_block, size)
IF block is NOT NULL THEN
block.free = 0
ELSE
block = request_space(last_block, size)
END IF
END IF
RETURN pointer right after the block header
END FUNCTION
```
### The Exact Code
```c
// Helper to find a free block using the "First Fit" algorithm
struct block_meta *find_free_block(struct block_meta **last, size_t size) {
struct block_meta *current = global_base;
// Loop through the list while current is not NULL
while (current) {
if (current->free && current->size >= size) {
return current; // Found a big enough free block!
}
*last = current; // Keep track of the last block we looked at
current = current->next;
}
return NULL; // No suitable block found
}
// Helper to request space from the OS and link it to our list
struct block_meta *request_space(struct block_meta* last, size_t size) {
struct block_meta *block;
// We request size PLUS the size of our metadata header
block = request_os_memory(size + META_SIZE);
if (block == NULL) {
return NULL; // OS refused to give us memory
}
// Initialize our new block's metadata
block->size = size;
block->next = NULL;
block->free = 0; // It's in use
// If we have a previous block, link it to this new one
if (last) {
last->next = block;
}
return block;
}
// Our custom malloc!
void *my_malloc(size_t size) {
struct block_meta *block;
if (size <= 0) {
return NULL;
}
if (!global_base) {
// First call to malloc ever
block = request_space(NULL, size);
if (!block) { return NULL; }
global_base = block;
} else {
struct block_meta *last = global_base;
block = find_free_block(&last, size);
if (!block) {
// Failed to find a free block, ask OS for a new one
block = request_space(last, size);
if (!block) { return NULL; }
} else {
// Found a free block
block->free = 0;
}
}
// Return a pointer to the memory *after* the header
return (block + 1);
}
```
---
## Section 4: Implementing `free` (Releasing Memory)
### The Minimum Theory
When a user finishes with their memory, they call `free(pointer)`. But how do we know what
block to free?
Because we always return a pointer directly *after* our hidden metadata block, we can find the
metadata by subtracting the size of the header from the pointer. Once we have the header, we
simply set the `free` flag to `1`.
*(Note: A more advanced implementation might merge adjacent free blocks to prevent
fragmentation, or give memory back to the OS, but for this beginner guide, simply marking the
block as free is perfectly sufficient).*
### High-Level Steps
1. Verify the pointer passed to `free` is not NULL.
2. Step backward in memory by the size of the block header to find the metadata.
3. Mark the `free` flag in the metadata as 1.
### Detailed Low-Level Steps
1. Create `my_free(void *ptr)`.
2. Check `if (!ptr) return;`
3. Cast `ptr` to a `struct block_meta*`.
4. Subtract 1 from the casted pointer. Because of [pointer
arithmetic](https://en.wikipedia.org/wiki/Pointer_(computer_programming)#Pointer_arithmetic),
`ptr - 1` steps backward exactly by `META_SIZE`.
5. Set `block_ptr->free = 1;`.
### Pseudo Code
```text
FUNCTION my_free(pointer)
IF pointer is NULL THEN
RETURN
END IF
block_header = pointer - size of (block_meta)
block_header.free = 1
END FUNCTION
```
### The Exact Code
```c
// Helper function to get the block metadata from a user pointer
struct block_meta *get_block_ptr(void *ptr) {
// Cast the void pointer to our struct type and step back one struct size
return (struct block_meta*)ptr - 1;
}
// Our custom free!
void my_free(void *ptr) {
if (!ptr) {
return; // Nothing to free
}
// Get the hidden metadata header
struct block_meta* block_ptr = get_block_ptr(ptr);
// Mark the block as free so my_malloc can reuse it later
block_ptr->free = 1;
}
```