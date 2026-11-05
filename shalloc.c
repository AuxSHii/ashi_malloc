/* basic malloc implementation */

#include <stddef.h>
#include <stdio.h>

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
    
}