#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct memory_region{
  size_t * start;
  size_t * end;
};

struct memory_region global_mem;
struct memory_region heap_mem;
struct memory_region stack_mem;

void walk_region_and_mark(void* start, void* end);

//how many ptrs into the heap we have
#define INDEX_SIZE 1000
void* heapindex[INDEX_SIZE];


//grabbing the address and size of the global memory region from proc 
void init_global_range(){
  char file[100];
  char * line=NULL;
  size_t n=0;
  size_t read_bytes=0;
  size_t start, end;

  sprintf(file, "/proc/%d/maps", getpid());
  FILE * mapfile  = fopen(file, "r");
  if (mapfile==NULL){
    perror("opening maps file failed\n");
    exit(-1);
  }

  int counter=0;
  while ((read_bytes = getline(&line, &n, mapfile)) != -1) {
    if (strstr(line, "hw4")!=NULL){
      ++counter;
      if (counter==3){
        sscanf(line, "%lx-%lx", &start, &end);
        global_mem.start=(size_t*)start;
        // with a regular address space, our globals spill over into the heap
        global_mem.end=malloc(256);
        free(global_mem.end);
      }
    }
    else if (read_bytes > 0 && counter==3) {
      if(strstr(line,"heap")==NULL) {
        // with a randomized address space, our globals spill over into an unnamed segment directly following the globals
        sscanf(line, "%lx-%lx", &start, &end);
        printf("found an extra segment, ending at %zx\n",end);						
        global_mem.end=(size_t*)end;
      }
      break;
    }
  }
  fclose(mapfile);
}


//marking related operations

int is_marked(size_t* chunk) {
  return ((*chunk) & 0x2) > 0;
}

void mark(size_t* chunk) {
  (*chunk)|=0x2;
}

void clear_mark(size_t* chunk) {
  (*chunk)&=(~0x2);
}

// chunk related operations

#define chunk_size(c)  ((*((size_t*)c))& ~(size_t)3 )

 
void* next_chunk(void* c) { 
  if(chunk_size(c) == 0) {
    printf("Panic, chunk is of zero size.\n");
  }
  if((c+chunk_size(c)) < sbrk(0))
    return ((void*)c+chunk_size(c));
  else 
    return 0;
}
int in_use(void *c) { 
  return (next_chunk(c) && ((*(size_t*)next_chunk(c)) & 1));
}


// index related operations

#define IND_INTERVAL ((sbrk(0) - (void*)(heap_mem.start - 1)) / INDEX_SIZE)
void build_heap_index() {
  // TODO
}


// the actual collection code
void sweep() {
  
  //Create a variable that points to the chunk header of the beginning of the heap
  size_t* start = heap_mem.start - 1;
  
  //Create a variable that points to the end of the heap
  size_t* end = heap_mem.end;
  
  //Declare a variable that points to the next chunk from the current
  size_t* next;
  
  //While the start ptr does not reach the end, and is not NULL...
  while(start < end && start != NULL)
  {
	  //Create a temp ptr that points to our current ptr
	  size_t* tempStart = start;
	  
	  //Set the next ptr as the next_chunk of the temp
	  next = next_chunk(tempStart);
	  
	  //If the chunk is marked, clear the mark
	  if(is_marked(tempStart))
		  clear_mark(tempStart);
	  //Otherwise, if the chunk is in_use.. free the ptr from the MEM section
	  else if(in_use(tempStart))
		  free(tempStart + 1);

	  //Set the start as the next
	  start = next;
	  
	  //Update the heap size using sbrk(0), important to do so we dont miss anything
	  end = sbrk(0);  
  }
  
  //Update the heap_mem.end as a sanity check, in case anything goes wrong.
  heap_mem.end = sbrk(0);
  
}


//determine if what "looks" like a pointer actually points to a block in the heap
size_t * is_pointer(size_t * ptr) {
  // TODO

  /*If the pointer is not within the range of the heap, or it's not allocated
   *then return NULL*/
 if((ptr < heap_mem.start || ptr >= heap_mem.end) || !ptr)
	return NULL;

  //Make a temp pointer to iterate through heap (that way no changes will be made to the heap itself, just a traversal)
  size_t* temp = heap_mem.start - 1;
  
  //Make a pointer that will point to the next section on the heap
  size_t* next = next_chunk(temp);
 
  //While we have not reached the end of the heap and our temp is not null...
  while(temp != NULL && temp < heap_mem.end)
  {
	    //next = next_chunk(temp);
		//If our ptr is between the current and the next block on the heap
		if(temp <= ptr && next > ptr)
		{
			//Return the chunk header of the CURRENT block
			return temp;
		} 
		
		//Set the current as the next
		temp = next;
		
		//SANITY CHECK: Make sure that the next doesn't reach NULL space
		//If it does, break out of the loop and return NULL
		if(next >= heap_mem.end || !next)
			break;
		
		//Find the next chunk
		next = next_chunk(temp);


  }
  	
  return NULL;


}


void markChunk(size_t* ptr)
{	

	//Determine if the pointer really is a pointer or not
	size_t* chunk = is_pointer((size_t*)*ptr);
	
	//If the chunk is NULL (not a pointer) OR if the chunk is already marked..
	//Do nothing, return out of the function and continue our walk
    if(!chunk || is_marked(chunk))
		return;

	//Otherwise, mark the current chunk
	mark(chunk);
	
	//Determine the next chunk
	size_t* next = next_chunk(chunk);
	
	/*
	Initially, I tried to follow the example outlined in the lecture slides by treating each chunk
	like an array and recursively going through it. But I was going through recursively into
	markChunk again, instead of walking through it like I did in walk_region_and_mark.
	Conceptually, it should have worked, but the problem was I was going through it like I do with an
	array of chunks, instead of treating each chunk as its own walkable region.
	That's when I realized that I can just call walk_region_and_mark on the chunk itself because
	it can be used on any kind of region, so long as I coded is_pointer right.
	I left my previous thoughts as a block comment below, if you would like to reference it.
	*/
	if(next < heap_mem.end)
		walk_region_and_mark(chunk, next);
	
	

/*	int i;
	int len = chunk_size(chunk);
	size_t* nextChunk;
	for(i = 0; i < len; i++)
	{
		nextChunk = chunk[i];
		if(nextChunk < heap_mem.end)
		{
			markChunk(nextChunk);
		}
	}
*/
}



void walk_region_and_mark(void* start, void* end) {
 
 //Make both the start and end of type size_t*
 //So we can perform the appropriate pointer arithmetic
 size_t* tempStart = (size_t*)start;
 size_t* tempEnd = (size_t*)end;

  //While the start is less than the end..
  while(tempStart < tempEnd)
  { 
	//Mark the current chunk
	markChunk(tempStart);
	
	//Go to the next ptr within the range...
	tempStart++;    
  }
   
  
}

// standard initialization 

void init_gc() {
  size_t stack_var;
  init_global_range();
  heap_mem.start=malloc(512);
  //since the heap grows down, the end is found first
  stack_mem.end=(size_t *)&stack_var;
}

void gc() {
  size_t stack_var;
  heap_mem.end=sbrk(0);
  //grows down, so start is a lower address
  stack_mem.start=(size_t *)&stack_var;
  

  // build the index that makes determining valid ptrs easier
  // implementing this smart function for collecting garbage can get bonus;
  // if you can't figure it out, just comment out this function.
  // walk_region_and_mark and sweep are enough for this project.
  //build_heap_index();

  //walk memory regions
  walk_region_and_mark(global_mem.start,global_mem.end);
  walk_region_and_mark(stack_mem.start,stack_mem.end);
  sweep();

}
