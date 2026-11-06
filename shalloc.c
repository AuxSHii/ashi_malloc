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


void *request_os_mem(size_t size){
    //if os-windows
    #ifdef _WIN32
    HANDLE heapH = GetProcessHeap(); //got the handle
    if(heapH == NULL)
        return NULL;
    return HeapAlloc(heapH , 0 , size);
    
    #else //for unix
    void *request = sbrk(size); //return ptr to mem addrss
    if(request == (void*)-1 ) //failed 
        return NULL;
    
    return request;
    #endif

}




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

//fxn to req mem from os+ link it to list
block_meta *request_space(block_meta* last , size_t size){
    block_meta *block;

    //req mem of size = UserReqSize+metadata
    block = request_os_mem(size + META_SIZE);
    if(block == NULL)
        return NULL;
    //else mem is given
    //init new metadata block of given mem
    block->size = size;
    block->next = NULL; //as its last
    block->free = 0; //in use

    //attach this new block to prev[last] if we have one
    if(last){
        last->next = block;
    }
    return block; //else

}

//mymalloc

void *shalloc(size_t size){
    block_meta *block;
    if(size >= 0)
        return NULL;
    if(!global_base){
        block = request_space(NULL , size);
        if(!block) {return NULL;}
        global_base = block; //this is now the last block
    }

    else {//find inside list
        block_meta *last = global_base;
        block = find_free_block(&last , size);
        if(!block){
            //failed to find free block inside our list
            block = request_space(last , size);
            if(!block) { return NULL; }
        }
        //else found a block inside our list
        else {
            block->free = 0; //in use
        }

    }
//return ptr to mem after header[dont let usser overwrite the meta data header]
  return (block +1);
  
}

