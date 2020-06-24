#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "myMalloc.h"
#include "printing.h"

/* Due to the way assert() prints error messges we use out own assert function
 * for deteminism when testing assertions
 */
#ifdef TEST_ASSERT
inline static void assert(int e) {
	if (!e) {
		const char * msg = "Assertion Failed!\n";
		write(2, msg, strlen(msg));
		exit(1);
	}
}
#else
#include <assert.h>
#endif

/*
 * Mutex to ensure thread safety for the freelist
 */
static pthread_mutex_t mutex;

/*
 * Array of sentinel nodes for the freelists
 */
header freelistSentinels[N_LISTS];

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */
header * lastFencePost;

/*
 * Pointer to maintian the base of the heap to allow printing based on the
 * distance from the base of the heap
 */ 
void * base;

/*
 * List of chunks allocated by  the OS for printing boundary tags
 */
header * osChunkList [MAX_OS_CHUNKS];
size_t numOsChunks = 0;

/*
 * direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */
static void init (void) __attribute__ ((constructor));

// Helper functions for manipulating pointers to headers
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off);
static inline header * get_left_header(header * h);
static inline header * ptr_to_header(void * p);

// Helper functions for allocating more memory from the OS
static inline void initialize_fencepost(header * fp, size_t left_size);
static inline void insert_os_chunk(header * hdr);
static inline void insert_fenceposts(void * raw_mem, size_t size);
static header * allocate_chunk(size_t size);

// Helper functions for freeing a block
static inline void deallocate_object(void * p);

// Helper functions for allocating a block
static inline header * allocate_object(size_t raw_size);

// Helper functions for verifying that the data structures are structurally 
// valid
static inline header * detect_cycles();
static inline header * verify_pointers();
static inline bool verify_freelist();
static inline header * verify_chunk(header * chunk);
static inline bool verify_tags();

static void init();

static bool isMallocInitialized;

/**
 * @brief Helper function to retrieve a header pointer from a pointer and an 
 *        offset
 *
 * @param ptr base pointer
 * @param off number of bytes from base pointer where header is located
 *
 * @return a pointer to a header offset bytes from pointer
 */
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off) {
	return (header *)((char *) ptr + off);
}

/**
 * @brief Helper function to get the header to the right of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
header * get_right_header(header * h) {
	return get_header_from_offset(h, get_size(h));
}

/**
 * @brief Helper function to get the header to the left of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
inline static header * get_left_header(header * h) {
	return get_header_from_offset(h, -h->left_size);
}

/**
 * @brief Fenceposts are marked as always allocated and may need to have
 * a left object size to ensure coalescing happens properly
 *
 * @param fp a pointer to the header being used as a fencepost
 * @param left_size the size of the object to the left of the fencepost
 */
inline static void initialize_fencepost(header * fp, size_t left_size) {
	set_state(fp,FENCEPOST);
	set_size(fp, ALLOC_HEADER_SIZE);
	fp->left_size = left_size;
}

/**
 * @brief Helper function to maintain list of chunks from the OS for debugging
 *
 * @param hdr the first fencepost in the chunk allocated by the OS
 */
inline static void insert_os_chunk(header * hdr) {
	if (numOsChunks < MAX_OS_CHUNKS) {
		osChunkList[numOsChunks++] = hdr;
	}
}

/**
 * @brief given a chunk of memory insert fenceposts at the left and 
 * right boundaries of the block to prevent coalescing outside of the
 * block
 *
 * @param raw_mem a void pointer to the memory chunk to initialize
 * @param size the size of the allocated chunk
 */
inline static void insert_fenceposts(void * raw_mem, size_t size) {
	// Convert to char * before performing operations
	char * mem = (char *) raw_mem;

	// Insert a fencepost at the left edge of the block
	header * leftFencePost = (header *) mem;
	initialize_fencepost(leftFencePost, ALLOC_HEADER_SIZE);

	// Insert a fencepost at the right edge of the block
	header * rightFencePost = get_header_from_offset(mem, size - ALLOC_HEADER_SIZE);
	initialize_fencepost(rightFencePost, size - 2 * ALLOC_HEADER_SIZE);
}

/**
 * @brief Allocate another chunk from the OS and prepare to insert it
 * into the free list
 *
 * @param size The size to allocate from the OS
 *
 * @return A pointer to the allocable block in the chunk (just after the 
 * first fencpost)
 */
static header * allocate_chunk(size_t size) {
	void * mem = sbrk(size);

	insert_fenceposts(mem, size);
	header * hdr = (header *) ((char *)mem + ALLOC_HEADER_SIZE);
	set_state(hdr, UNALLOCATED);
	set_size(hdr, size - 2 * ALLOC_HEADER_SIZE);
	hdr->left_size = ALLOC_HEADER_SIZE;
	return hdr;
}

/**
 * @brief Helper allocate an object given a raw request size from the user
 *
 * @param raw_size number of bytes the user needs
 *
 * @return A block satisfying the user's request
 */
static inline header * allocate_object(size_t raw_size) {
	// TODO implement allocation
	/*
	   (void) raw_size;
	   assert(false);
	   exit(1);
	 */

	if(raw_size == 0)
		return NULL;

	size_t size = 0;
	raw_size = ((raw_size + 7)/8) * 8;

	size = raw_size + ALLOC_HEADER_SIZE;
	if(size < sizeof(header))
		size = sizeof(header);
	size_t start = size/8 - 1;
	header * sentinelNode;
	
	if(start >= N_LISTS)
		start = N_LISTS - 1;
	//int num;
	//printf("Enter before loop");
	//printf("size = %ld, start = %ld", size, start);
	for(int i=start;i<N_LISTS;i++)
	{
		//header * head = (header *) ((char *)mem + ALLOC_HEADER_SIZE)
		sentinelNode = &freelistSentinels[i];
		/*
		if(i==12)
		{
			printf("after assign sentinelNode");
			print_object(sentinelNode);
			sleep(2);
		
			scanf("%d",&num);
		}*/
		if(sentinelNode->next == sentinelNode)
			continue;
		/*
		if(i==12)
		{
			printf("before print sentinelNode");
			print_object(sentinelNode);
			sleep(2);
		}*/

		header * curr = sentinelNode -> next;
		//print_object(sentinelNode);
		while(curr -> next != NULL)
		{
			//print_object(curr);
			//2 cases: block is equal, then remove from free list,alloc
			if(get_size(curr) == size) // if block is equal to request size
			{
				//printf("if equal");
				curr -> prev -> next = curr -> next;
				curr -> next -> prev = curr -> prev;
				curr -> next -> left_size = get_size(curr);//new line
				set_state(curr, ALLOCATED);
				//print_object(curr);
				return curr;
			}
			else if(get_size(curr) > size)// if block is greater than request size
			{
				//set request size to allocated
				//printf("if less than block size");
				//print_object(curr);
				if(get_size(curr) - size > sizeof(header))
				{
					//split block into allocated and remainder size
					//place remainder size in appropriate free list
					//iterate through free list to find correct

					//split the block into request and remainder
					
					//printf("header = %ld", sizeof(header));
					//printf("splitting");
					curr -> prev -> next = curr -> next;
					curr -> next -> prev = curr -> prev;
					header * rem = get_header_from_offset(curr, get_size(curr)-size);
					set_size_and_state(rem, size, ALLOCATED);
					//set_size_and_state(curr, get_size(curr)-size, UNALLOCATED);//new line
					rem -> left_size = get_size(curr) - size;
					//print_object(rem);
					get_right_header(curr) -> left_size = size;
					//curr->next->left_size = size;//new line
					//header * rem = get_right_header(curr);//new line
					set_size(curr, get_size(curr) - size);
					size_t remainder = get_size(curr) - ALLOC_HEADER_SIZE;
					remainder = (remainder+7)/8 - 1;
					//printf("rem= %ld", remainder);
					//print_object(curr);
					header * sentinelNode2;
					if(remainder > N_LISTS-1)
						remainder = N_LISTS - 1;
					sentinelNode2 = &freelistSentinels[remainder];
					curr -> prev = sentinelNode2;
					curr -> next = sentinelNode2 -> next;
					sentinelNode2 -> next -> prev = curr;
					sentinelNode2 -> next = curr;
					//curr -> next -> left_size = get_size(curr->prev);//new line
					
					//printf("after inserting");
					//printf("remainder = %ld", remainder);
					//print_object(curr);
					//print_object(sentinelNode2);
					set_state(rem, ALLOCATED);
					
					return rem;
				}
				else
				{
					//allocate entire block to user
					
					//printf("allocate whole block");
					//sleep(2);
					curr -> prev -> next = curr -> next;
					curr -> next -> prev = curr -> prev;
					//print_object(curr);
					get_right_header(curr) -> left_size = get_size(curr);//new line
					set_state(curr, ALLOCATED);
					//print_object(curr);
					//sleep(2);
					return curr;
				}
			}
			curr = curr -> next;
		}
	}
	
	//Allocate more memory
	size_t alloc_size = 0; 
	//printf("allocate additional mem");
	//printf("size = %ld", size);
	size_t a;
	//scanf("%ld",&a);
	while(alloc_size < size)
	{
		header * mem = allocate_chunk(ARENA_SIZE);
		//check if the previous call is successful
		//while loop until request size < ARENA_SIZE
		//print_object(mem);
		if(mem != NULL)
		{
			header * h = get_header_from_offset(mem,-2*ALLOC_HEADER_SIZE);
			//header * h = mem - ALLOC_HEADER_SIZE;
			header * c; //pointer to coalesced block
			//print_object(h);
			if(get_state(h)==FENCEPOST)
			{
				header * h2 = get_left_header(h);
				//print_object(h2);
				size_t a;
				//scanf("%ld",&a);
				//blocks are contiguous
				//then remove left fencepost, add right fencepost, and coalesce
				//header * rf = h2 + get_size(h2);
				//header * lf = h2 - 32;
				//header * llf = rf - get_size(h2) - 2*get_size(lf);
				if(get_state(h2)==UNALLOCATED)
				{
					//coalesce h2 and h
					h2 -> prev -> next = h2 -> next;
					h2 -> next -> prev = h2 -> prev;
					set_size(h2, get_size(h2)+2*ALLOC_HEADER_SIZE + get_size(mem));
					c = h2;
				}
				else
				{
					set_size(h, 2*ALLOC_HEADER_SIZE+get_size(mem));
					c = h;
				}
			}
			else
			{
				//then add fencepost to both ends of new mem location
				header * left_fence = get_header_from_offset(mem, -ALLOC_HEADER_SIZE);
				insert_os_chunk(left_fence);
				c = mem;
			}
			
			//add mem block to correct free list
			//calculate index from size and add to location
	
			size_t index = (get_size(c))/8 - 1;
			if(index >= N_LISTS)
				index = N_LISTS - 1;
			//printf("index = %ld",index);
			header * sentinelNode = &freelistSentinels[index];
			c -> next = sentinelNode -> next;
			c -> prev = sentinelNode;
			sentinelNode -> next -> prev = c;
			sentinelNode -> next = c;
			set_state(c, UNALLOCATED);
			c -> next -> left_size = get_size(c);
			//print_object(sentinelNode);
			//print_object(c);
			//coalesce with right block if unallocated
				
			//decrement request size
		}
		alloc_size = alloc_size + get_size(mem);
		//printf("alloc size: %ld",alloc_size);
	}
	return allocate_object(raw_size);
	//coalesce with adjacent unallocated block in free list

	//start of second half
	/*	while(&memory < &size)
		{
			memory = sbrk(ARENA_SIZE);
		}
		memory2 = sbrk(ARENA_SIZE);
		if(sbrk(ARENA_SIZE))
		set_size(new_chunk, memory2);
	*/	
	//loop through following process until memory allocated is >= request size
	//have big block and split it, taking the right side
	//two cases: if new chunk is not adjacent to previous chunk, allocate to free list
	//else: fenceposts between chunks are removed, coalesce chunks
	//all new chunks must be arena size


	//malloc 1 byte
	//malloc 2 bytes
	//free 2 bytes, then coalesce
				
	//coalesce:
		//set to unallocated
		//check if right is unallocated
			//if true, increase size of block, left_size of fencepost
				
		//last freelist: insert even if larger than index
		//copy next and prev pointers
				
	/*	void * memory = sbrk(size);
		return memory;
	*/
}

/**
 * @brief Helper to get the header from a pointer allocated with malloc
 *
 * @param p pointer to the data region of the block
 *
 * @return A pointer to the header of the block
 */
static inline header * ptr_to_header(void * p) {
	return (header *)((char *) p - ALLOC_HEADER_SIZE); //sizeof(header));
}

/**
 * @brief Helper to manage deallocation of a pointer returned by the user
 *
 * @param p The pointer returned to the user by a call to malloc
 */
static inline void deallocate_object(void * p) {
	// TODO implement deallocation
	//4 cases: left unallocated, right unallocated, both/neither
	if(p != NULL)
	{
		header * h = ptr_to_header(p);
		header * left = get_left_header(h);
		header * right = get_right_header(h);
	
		if(get_state(h) == UNALLOCATED)
		{
			fprintf(stderr, "Double Free Detected\n");
			assert(false);
		} else {
			set_state(h, UNALLOCATED);
		}
		if(get_state(left) != UNALLOCATED && get_state(right) != UNALLOCATED)
		{
			//Insert block into appropriate free list
			size_t size = (get_size(h) - ALLOC_HEADER_SIZE)/8 - 1;
			if(size >= N_LISTS)
				size = N_LISTS - 1;
			header * sentinelNode = &freelistSentinels[size];
			h -> next = sentinelNode -> next;
			h -> prev = sentinelNode;
			sentinelNode -> next -> prev = h;
			sentinelNode -> next = h;
			set_state(h, UNALLOCATED);
		}
		else if(get_state(left) == UNALLOCATED && get_state(right) ==UNALLOCATED)
		{
			//Coalesce w/ both neighbors
			//Place coalesced block where left block was in free list
			set_size(left,get_size(h) + get_size(left) + get_size(right));
			header * right_right = get_right_header(right);
			right_right->left_size = get_size(left);
			set_state(left, UNALLOCATED);
			left -> prev -> next = left -> next;
			right -> prev -> next = right -> next;

			left -> next -> prev = left -> prev;
			right -> next -> prev = right -> prev;

			//TODO Insert coalesced block into appropriate free list
			size_t i;
			size_t size = (get_size(left) - ALLOC_HEADER_SIZE)/8 - 1;
			if(size >= N_LISTS)
				size = N_LISTS - 1;
			header * sentinelNode = &freelistSentinels[size];
			left -> next = sentinelNode -> next;
			left -> prev = sentinelNode;
			sentinelNode -> next -> prev = left;
			sentinelNode -> next = left;
			set_state(left, UNALLOCATED);
		}
		else if(get_state(right) == UNALLOCATED && get_state(left) != UNALLOCATED)
		{
			//Coalesce current and right
			//Place coalesced block where right block was in free list
			set_size(h,get_size(h) + get_size(right));
			header * right_right = get_right_header(right);
			right_right->left_size = get_size(h);
			set_state(h, UNALLOCATED);
			right -> prev -> next = right -> next;
			right -> next -> prev = right -> prev;

			//TODO Insert coalesced block into appropriate free list
			size_t i;
			size_t size = (get_size(h) - ALLOC_HEADER_SIZE)/8 - 1;
			if(size >= N_LISTS)
				size = N_LISTS - 1;
			header * sentinelNode = &freelistSentinels[size];
			h -> next = sentinelNode -> next;
			h -> prev = sentinelNode;
			sentinelNode -> next -> prev = h;
			sentinelNode -> next = h;
		}
		else if(get_state(right) != UNALLOCATED && get_state(left) == UNALLOCATED)
		{
			//Coalesce current and left
			//Place coalesced block where left block was in free list
			set_size(left,get_size(h) + get_size(left));
			set_state(left, UNALLOCATED);
			right->left_size = get_size(left);
			left -> prev -> next = left -> next;
			left -> next -> prev = left -> prev;
		
			//TODO Insert coalesced block into appropriate free list
			size_t size = (get_size(left) - ALLOC_HEADER_SIZE)/8 - 1;
			if(size >= N_LISTS)
				size = N_LISTS - 1;
			header * sentinelNode = &freelistSentinels[size];
			left -> next = sentinelNode -> next;
			left -> prev = sentinelNode;
			sentinelNode -> next -> prev = left;
			sentinelNode -> next = left;
			left -> next -> left_size = get_size(left);
		}
	}
}

/**
 * @brief Helper to detect cycles in the free list
 * https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_Tortoise_and_Hare
 *
 * @return One of the nodes in the cycle or NULL if no cycle is present
 */
static inline header * detect_cycles() {
	for (int i = 0; i < N_LISTS; i++) {
		header * freelist = &freelistSentinels[i];
		for (header * slow = freelist->next, * fast = freelist->next->next; 
				fast != freelist; 
				slow = slow->next, fast = fast->next->next) {
			if (slow == fast) {
				return slow;
			}
		}
	}
	return NULL;
}

/**
 * @brief Helper to verify that there are no unlinked previous or next pointers
 *        in the free list
 *
 * @return A node whose previous and next pointers are incorrect or NULL if no
 *         such node exists
 */
static inline header * verify_pointers() {
	for (int i = 0; i < N_LISTS; i++) {
		header * freelist = &freelistSentinels[i];
		for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
			if (cur->next->prev != cur || cur->prev->next != cur) {
				return cur;
			}
		}
	}
	return NULL;
}

/**
 * @brief Verify the structure of the free list is correct by checkin for 
 *        cycles and misdirected pointers
 *
 * @return true if the list is valid
 */
static inline bool verify_freelist() {
	header * cycle = detect_cycles();
	if (cycle != NULL) {
		fprintf(stderr, "Cycle Detected\n");
		print_sublist(print_object, cycle->next, cycle);
		return false;
	}

	header * invalid = verify_pointers();
	if (invalid != NULL) {
		fprintf(stderr, "Invalid pointers\n");
		print_object(invalid);
		return false;
	}

	return true;
}

/**
 * @brief Helper to verify that the sizes in a chunk from the OS are correct
 *        and that allocated node's canary values are correct
 *
 * @param chunk AREA_SIZE chunk allocated from the OS
 *
 * @return a pointer to an invalid header or NULL if all header's are valid
 */
static inline header * verify_chunk(header * chunk) {
	if (get_state(chunk) != FENCEPOST) {
		fprintf(stderr, "Invalid fencepost\n");
		print_object(chunk);
		return chunk;
	}

	for (; get_state(chunk) != FENCEPOST; chunk = get_right_header(chunk)) {
		if (get_size(chunk)  != get_right_header(chunk)->left_size) {
			fprintf(stderr, "Invalid sizes\n");
			print_object(chunk);
			return chunk;
		}
	}

	return NULL;
}

/**
 * @brief For each chunk allocated by the OS verify that the boundary tags
 *        are consistent
 *
 * @return true if the boundary tags are valid
 */
static inline bool verify_tags() {
	for (size_t i = 0; i < numOsChunks; i++) {
		header * invalid = verify_chunk(osChunkList[i]);
		if (invalid != NULL) {
			return invalid;
		}
	}

	return NULL;
}

/**
 * @brief Initialize mutex lock and prepare an initial chunk of memory for allocation
 */
static void init() {
	// Initialize mutex for thread safety
	pthread_mutex_init(&mutex, NULL);

#ifdef DEBUG
	// Manually set printf buffer so it won't call malloc when debugging the allocator
	setvbuf(stdout, NULL, _IONBF, 0);
#endif // DEBUG

	// Allocate the first chunk from the OS
	header * block = allocate_chunk(ARENA_SIZE);

	header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
	insert_os_chunk(prevFencePost);

	lastFencePost = get_header_from_offset(block, get_size(block));

	// Set the base pointer to the beginning of the first fencepost in the first
	// chunk from the OS
	base = ((char *) block) - ALLOC_HEADER_SIZE; //sizeof(header);

	// Initialize freelist sentinels
	for (int i = 0; i < N_LISTS; i++) {
		header * freelist = &freelistSentinels[i];
		freelist->next = freelist;
		freelist->prev = freelist;
	}

	// Insert first chunk into the free list
	header * freelist = &freelistSentinels[N_LISTS - 1];
	freelist->next = block;
	freelist->prev = block;
	block->next = freelist;
	block->prev = freelist;
}

/* 
 * External interface
 */
void * my_malloc(size_t size) {
	pthread_mutex_lock(&mutex);
	header * hdr = allocate_object(size); 
	pthread_mutex_unlock(&mutex);
	if (hdr == NULL)
		return NULL;
	return (void*) &hdr->data[0];
}

void * my_calloc(size_t nmemb, size_t size) {
	return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {
	void * mem = my_malloc(size);
	memcpy(mem, ptr, size);
	my_free(ptr);
	return mem; 
}

void my_free(void * p) {
	//printf("my_free");
	pthread_mutex_lock(&mutex);
	//printf("before dealloc");
	deallocate_object(p);
	pthread_mutex_unlock(&mutex);
}

bool verify() {
	return verify_freelist() && verify_tags();
}
