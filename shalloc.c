/* basic malloc implementation */

#include <stddef.h>
#include <stdio.h>

typedef struct block_meta 
{
    size_t size; 
    int free; //1=free , 0=use
    struct block_meta *next; //next block in list

} block_meta;

//global header ptr
void *global_base = NULL;

//macro for the size of struct - for totl mem needed
#define META_SIZE sizeof(block_meta);




#ifdef _WIN32
#include <windows.h>

#else
#include <unistd.h> //for unix

#endif

//fxn to loop through linked list and find free block
block_meta *find_free_block(block_meta **last , size_t size){
	block_meta *current = global_base; //init val
    //loopp through the list
    while(current){
        if(current->free && current->size >= size){
            return current;
        }
        *last = current; //keep track of last block
        current = current->next; //incremented
    }

    return NULL;
}










void *request_os_mem(size_t size){
	//if os-windows
	#ifdef _WIN32
	HANDLE heapH = GetProcessHeap(); //got the handle
    if(heapH == NULL)
    	return NULL;
    return HeapAlloc(heapH , 0 , size);
    
    #else //for unix
    void *request = sbrk(size); //returna ptr to mem addrss
    if(request == (void*)-1 ) //failed 
    	return NULL;
    
    return request;
    #endif

}