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


### for unix:
### Understanding the Unix Heap: The sbrk System Call
#### The Minimum Theory
in Unix-like operating systems, your program's memory has a boundary called the **program break**. This break marks the exact end of your heap. Everything below the program break is memory that your OS has officially mapped and given your program permission to use. everything above it is unallocated; if your program tries to touch it, the OS will trigger a crash (a segmentation fault).
[read more](https://pubs.opengroup.org/onlinepubs/7908799/xsh/brk.html)
[mem management](https://blog.pr4tt.com/2016/02/01/posix-memory-management/)

the sbrk() (set break) system call is the mechanism we use to ask the OS to move this boundary line upwards, which effectively creates more flexible heap space for us to divide up and hand to the user.
[here](https://docs.oracle.com/cd/E36784_01/html/E36872/sbrk-2.html)

#### How to Use It Here
to use sbrk in our malloc implementation, we must include the <unistd.h> header. The function takes a single argument: an integer representing the number of bytes by which you want to increase the heap.
 * **Allocating Memory:** calling sbrk(1024) tells the OS to move the program break up by 1,024 bytes. If it succeeds, it returns a void pointer to the *start* of the newly created space.
 * **Checking Current Position:** calling sbrk(0) is a standard trick. It adds zero bytes, meaning it doesn't allocate any new memory. Instead, it simply returns the current memory address of the program break, letting you know exactly where your heap currently ends.
 * **Handling Errors:** if the system is entirely out of RAM or hits a memory limit, sbrk will fail. Instead of returning NULL, it returns (void*) -1. your code must explicitly check for (void*) -1 to safely reject a malloc request rather than trying to write data to an invalid -1 memory address.





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

### ok! read this to get more comfortable with the concept

### Understanding the Linked List in Memory Allocation

a **Linked List** is a foundational data structure made up of individual elements called **nodes**. Instead of being packed tightly next to each other like items in an array, nodes can exist anywhere in memory. They stay connected because each node contains a **pointer**—a variable that stores the exact memory address of the next node in the sequence.

this creates a chain of data. To read the list, you must start at the very first node (called the **head**) and follow the pointers one by one until you reach the end, which is typically marked by a NULL pointer.

#### How We Use It Here
the OS gives us a massive, completely empty expanse of heap memory. It has no built-in organization. By placing our struct block_meta header just before every chunk of memory we hand out, we artificially lay a linked list over this raw memory. In our implementation, the block_meta headers act as the nodes.
 * **The Head:** we define a global pointer (void *global_base = NULL;) to act as the head of our list. It remembers exactly where our very first memory block begins.
 * **Traversal:** when a user asks for memory via malloc(size), we start at global_base and "walk" down the chain by following the next pointers. This action is called **traversing** the list.
 * **Searching:** as we traverse, we inspect each node's metadata. We check if the block is currently empty (free == 1) and if its capacity (size) is greater than or equal to what the user requested. If we hit NULL before finding a suitable block, we know we have to ask the OS to expand the heap.


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

### read about allocation flow: How malloc Works actually:

when a user program needs memory, it simply asks for a number of bytes, for example by calling malloc(50). from the user's perspective, memory magically appears. But under the hood, our custom malloc acts as a middleman between the user and the Operating system.

because asking the OS for memory (via sbrk or HeapAlloc) requires a **Context Switch**—a relatively slow and expensive process where the CPU pauses the user program to let the operating system kernel take over—we want to bother the OS as little as possible.

therefore, our primary goal is to **recycle** memory that has been previously used and freed, only asking the OS for more heap space as an absolute last resort...

to find this recyclable memory efficiently, we use a search strategy called the **First Fit Algorithm**.

#### the Bird's-Eye View (The Flow)

 step by step negotiation that happens every time the user calls our malloc:

 1. **the initial check:** is this the very first time malloc is being called in the entire program? If our global_base pointer is NULL, we currently manage zero memory. We must immediately ask the OS for the requested size (plus our header size), set this new chunk as our global_base, and hand it to the user.

 2. **the search (first fit):** if global_base is not NULL, we already hav a linked list of memory blocks. We start at global_base and walk through the chain. We look at every block's header and ask two questions: *Is this block marked as free?* and *Is its size greater than or equal to what the user requested?*

 3. **the match:** The moment we find a block that answers "yes" to both questions, we stop searching. we flip its status from free = 1 to free = 0 (in use). We do not care if there is a "better fitting" block further down the list; we take the very *first fit* we find to keep the function running fast.
 4. **the fallback:** If we check every single block in our linked list and none of them are both free and large enough, we have exhausted our recycled memory. We calculate the size we need, ask the OS to expand the heap to accommodate a new block, attach this newly minted block to the very end of our linked list, and mark it as in use.

#### using here: + returning address

when we finally find or create the right memory block, a crucial detail is what we hand back to the user. We **cannot** just return the pointer to the block itself. If we did, the user would immediately write their data over our hidden metadata (corrupting our size and next pointer variables).

instead, we use C pointer arithmetic and return block + 1. bcuz block is a pointer of type struct block_meta*, adding 1 tells the C compiler to step forward in memory by exactly the size of one block_meta struct. This perfectly skips over our hidden header and points the user to the exact byte where their safe, empty memory begins.


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