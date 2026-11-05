#my notes for writing malloc

[the article i followed](https://levelup.gitconnected.com/malloc-is-not-magic-implementing-my-own-memory-allocator-e0354e914402)

but it has my own windows native implementation.


## os memory
os gives a program acess to 2 kinds of memories

1. stack : small size , stores data of known size at compile time.
2. heap : large mem , resize at demand or runtime.


OS prevents prcoesses from writing outside the heap mem, 
writng outside the flexible free space distributed to you by os , you can re-write certain parts of memory that can corrupt your instance of os and other necessery processes.

but , you can still overwrite data in the same heap mem you are working in.

to avoid that: need to get POSITION from heap 
where you wont rewrite anything.

in c , malloc() = get new mem posn with given length ,can safely write to.

free() = to free up the same mem loc.

so,we are not overwriting anything we are taking a free mem location and allocating it for our process.

malloc implementation should do:
implementation does the following:
1. Keeps a list of memory locations inside the heap where the memory is free.
2. Keeps track of all allocations made.
3. On demand, it can return a new free memory area of a given size. If the size is bigger than all existing free blocks, this implementation will request the OS to grow the heap.
4. On demand, it can free a chosen block. Freeing blocks can result in a request to the OS to reduce the size of the heap.
5. It is thread-safe (it can be used from multiple threads at the same time.)

### request mem from OS:
unix: sbrk system call

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

