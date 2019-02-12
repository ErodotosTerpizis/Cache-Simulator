/***************************************************************************
 * *    Inf2C-CS Coursework 2: Cache Simulation
 * *
 * *    Instructor: Boris Grot
 * *
 * *    TA: Siavash Katebzadeh
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
/* Do not add any more header files */

/*
 * Various structures
 */
typedef enum {FIFO, LRU, Random} replacement_p;

const char* get_replacement_policy(uint32_t p) {
    switch(p) {
    case FIFO: return "FIFO";
    case LRU: return "LRU";
    case Random: return "Random";
    default: assert(0); return "";
    };
    return "";
}

typedef struct {
    uint32_t address;
} mem_access_t;

// These are statistics for the cache and should be maintained by you.
typedef struct {
    uint32_t cache_hits;
    uint32_t cache_misses;
} result_t;


/*
 * Parameters for the cache that will be populated by the provided code skeleton.
 */

replacement_p replacement_policy = FIFO;
uint32_t associativity = 0;
uint32_t number_of_cache_blocks = 0;
uint32_t cache_block_size = 0;


/*
 * Each of the variables below must be populated by you.
 */
uint32_t g_num_cache_tag_bits = 0;
uint32_t g_cache_offset_bits= 0;
result_t g_result;


/* Reads a memory access from the trace file and returns
 * 32-bit physical memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1002];
    char* token = NULL;
    char* string = buf;
    mem_access_t access;

    if (fgets(buf, 1000, ptr_file)!= NULL) {
        /* Get the address */
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtoul(token, NULL, 16);
        return access;
    }

    /* If there are no more entries in the file return an address 0 */
    access.address = 0;
    return access;
}

void print_statistics(uint32_t num_cache_tag_bits, uint32_t cache_offset_bits, result_t* r) {
    /* Do Not Modify This Function */

    uint32_t cache_total_hits = r->cache_hits;
    uint32_t cache_total_misses = r->cache_misses;
    printf("CacheTagBits:%u\n", num_cache_tag_bits);
    printf("CacheOffsetBits:%u\n", cache_offset_bits);
    printf("Cache:hits:%u\n", r->cache_hits);
    printf("Cache:misses:%u\n", r->cache_misses);
    printf("Cache:hit-rate:%2.1f%%\n", cache_total_hits / (float)(cache_total_hits + cache_total_misses) * 100.0);
}

/*
 *
 * Add any global variables and/or functions here as needed.
 *
 */

// Structure to hold block data
typedef struct block{
    int valid;        //valid bit
    int  tag;
    int num_accesses; //used of lru/fifo policy
} block;

//Structure to hold set data
typedef struct set{
    int occupiedblocks; //used to determine if set is full
    int rank;           //holds the next rank to be assigned
    block* blocks;      //array of blocks
} set;

//Structure to hold the cache data
typedef struct cache {
    replacement_p rp;
    int number_blocks;
    int block_size;
    int cache_size;
    int num_of_sets;
    int blocks_in_set;
    set* sets;         // array of sets
}cache;

cache createCache() {
    cache c;
    //Initialiazation of cache attributes
    c.rp=replacement_policy;    
    c.number_blocks=number_of_cache_blocks;
    c.block_size= cache_block_size;
    c.cache_size = c.number_blocks * c.block_size;
    c.blocks_in_set=associativity;
    c.num_of_sets=number_of_cache_blocks/associativity;

    //memory allocation
    c.sets=  malloc(sizeof(set)*c.num_of_sets); // set is a pointer to a pointer, array pointing to arrays, each set points to an array of blocks
    for (int i=0; i<c.num_of_sets;i++){ 
        //Initialization of set attributes
        c.sets[i].occupiedblocks=0;
        c.sets[i].rank=0;
        c.sets[i].blocks=  malloc(sizeof(block)*c.blocks_in_set); //for each set allocate memory for block array
        for (int j=0; j<c.blocks_in_set;j++){  
           //Initialization of set attributes                  
           c.sets[i].blocks[j].tag=0;;
           c.sets[i].blocks[j].valid=0;
           c.sets[i].blocks[j].num_accesses=0;
        }
    }   
    return c;
}

//method to free allocated memory from cache
void clearCache(cache c){
    for (int i=0;i<c.num_of_sets;i++){
        free(c.sets[i].blocks);       
    }
    free(c.sets);
}

//global variable to hold the index bits
uint32_t g_num_cache_index_bits = 0;

//method to populate the global variables
 void calcparameters(uint32_t as, uint32_t blocks, uint32_t blocks_size){
    
     uint32_t temp_blocks_size=blocks_size;
     g_cache_offset_bits=log(temp_blocks_size)/log(2); //find the power of 2 needed to repressent the block size number
     uint32_t setsnum=blocks/as;
     g_num_cache_index_bits= log(setsnum)/log(2);      //find the power of 2 needed to repressent the  number of sets in a cache
     g_num_cache_tag_bits=32-g_cache_offset_bits-g_num_cache_index_bits;
 }


 uint32_t getTag(uint32_t address){
    uint32_t tag;
    uint32_t shiftnum=g_cache_offset_bits+ g_num_cache_index_bits; //number of shift will be the number of offset bits and the address bits
    tag= address>>shiftnum; //  shift right to get the tag bits
    return tag;
}


uint32_t getIndex(uint32_t address){
    uint32_t index;
    // bit masking
    //shift bits right (get rid of offset) and use logic AND with 1s in order to keep the index (1s have the length of index)
    index= (address>>g_cache_offset_bits) & ((1<<g_num_cache_index_bits)-1); //create a mask of (num of index bits) 1s .
    return index;
}


uint32_t getOffset(uint32_t address){
    uint32_t offset,shift;
    shift=g_num_cache_tag_bits+g_num_cache_index_bits;
    offset= address & ((1<<(32-shift))-1); //bit masking/same principle as index
    return offset;
}

//method to access the cache with every new address given and determine whether the cache contains the data
void cacheaccess(uint32_t address,cache c){
    uint32_t tag=getTag(address);
    uint32_t index=getIndex(address); 
    
    int hit=0;              //used to indicate that we had a hit
    uint32_t temptag=0;    //holds the tag of each block, used for comparison
    uint32_t vbit=0;      //holds the valid bit of each block, used for comparison

    //traverse through all blocks in the set.
    //In case of fully associative, index will be 0, hence we will traverse through all the blocks of the cache
    //otherwise, index specifies which set to search
    for (int i=0;i<c.blocks_in_set;i++){
        temptag=c.sets[index].blocks[i].tag;
        vbit =c.sets[index].blocks[i].valid;
        if ((tag==temptag) & (vbit==1)){ // hit
            g_result.cache_hits++;
            if (c.rp==LRU){              // IF LRU, then update rank because it was accessed recently so rank should be updated
                c.sets[index].blocks[i].num_accesses=c.sets[index].rank;
                c.sets[index].rank++;
            }
            hit=1;
            break;
        }
    }
    
    
    if (!hit){ //miss
        g_result.cache_misses++;
        if (c.sets[index].occupiedblocks!=c.blocks_in_set){   //set not full
            
            for (int i=0;i<c.blocks_in_set;i++){
                vbit =c.sets[index].blocks[i].valid;
                if (vbit==0){  //if a block has valit bit 0, we can use it to write data
                    c.sets[index].blocks[i].tag=tag;
                    c.sets[index].blocks[i].valid=1;
                    c.sets[index].blocks[i].num_accesses=c.sets[index].rank; //allocate the next rank
                    c.sets[index].rank++;                                    //increment the rank hold in the current set
                    c.sets[index].occupiedblocks++;
                    break;
                }
            }
        }
        else{ //set is full and we need to replace a block
            int min;
            int r=0;                   //used to hold the rank of each block to compare.
            uint32_t block_position=0; //index of the block to evict
            if (c.rp==Random){        //when replacement policy is Random
                block_position=( rand() % (c.blocks_in_set) );
            }
            else{//In both LRU and FIFO policy, we search for the block that has the lowest rank to replace it
                min =c.sets[index].blocks[0].num_accesses;
                for (int i=1;i<c.blocks_in_set;i++){
                    r=c.sets[index].blocks[i].num_accesses;
                    if (min>r){
                        min=r;
                        block_position=i; //holds the position of the block with the lowest rank
                    }
                }
            }
            //replace the block with the new details
            c.sets[index].blocks[block_position].tag=tag;
            c.sets[index].blocks[block_position].valid=1;
            c.sets[index].blocks[block_position].num_accesses=c.sets[index].rank;
            c.sets[index].rank++;        
        }               
    }
}



int main(int argc, char** argv) {
    time_t t;
    /* Intializes random number generator */
    /* Important: *DO NOT* call this function anywhere else. */
    srand((unsigned) time(&t));
    /* ----------------------------------------------------- */
    /* ----------------------------------------------------- */

    /*
     *
     * Read command-line parameters and initialize configuration variables.
     *
     */
    int improper_args = 0;
    char file[10000];
    if (argc < 6) {
        improper_args = 1;
        printf("Usage: ./mem_sim [replacement_policy: FIFO LRU Random] [associativity: 1 2 4 8 ...] [number_of_cache_blocks: 16 64 256 1024] [cache_block_size: 32 64] mem_trace.txt\n");
    } else  {
        /* argv[0] is program name, parameters start with argv[1] */
        if (strcmp(argv[1], "FIFO") == 0) {
            replacement_policy = FIFO;
        } else if (strcmp(argv[1], "LRU") == 0) {
            replacement_policy = LRU;
        } else if (strcmp(argv[1], "Random") == 0) {
            replacement_policy = Random;
        } else {
            improper_args = 1;
            printf("Usage: ./mem_sim [replacement_policy: FIFO LRU Random] [associativity: 1 2 4 8 ...] [number_of_cache_blocks: 16 64 256 1024] [cache_block_size: 32 64] mem_trace.txt\n");
        }
        associativity = atoi(argv[2]);
        number_of_cache_blocks = atoi(argv[3]);
        cache_block_size = atoi(argv[4]);
        strcpy(file, argv[5]);
    }
    if (improper_args) {
        exit(-1);
    }
    assert(number_of_cache_blocks == 16 || number_of_cache_blocks == 64 || number_of_cache_blocks == 256 || number_of_cache_blocks == 1024);
    assert(cache_block_size == 32 || cache_block_size == 64);
    assert(number_of_cache_blocks >= associativity);
    assert(associativity >= 1);

    printf("input:trace_file: %s\n", file);
    printf("input:replacement_policy: %s\n", get_replacement_policy(replacement_policy));
    printf("input:associativity: %u\n", associativity);
    printf("input:number_of_cache_blocks: %u\n", number_of_cache_blocks);
    printf("input:cache_block_size: %u\n", cache_block_size);
    printf("\n");

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen(file,"r");
    if (!ptr_file) {
        printf("Unable to open the trace file: %s\n", file);
        exit(-1);
    }

    /* result structure is initialized for you. */
    memset(&g_result, 0, sizeof(result_t));

    /* Do not delete any of the lines below.
     * Use the following snippet and add your code to finish the task. */

    /* You may want to setup your Cache structure here. */

    //call the method to populate the global variables
    calcparameters(associativity,number_of_cache_blocks, cache_block_size);

    //create the cache 
    cache c=createCache();

    mem_access_t access;
    /* Loop until the whole trace file has been read. */

    while(1) {
        access = read_transaction(ptr_file);
        // If no transactions left, break out of loop.
        if (access.address == 0)
            break;
      
        /* Add your code here */
        //for every address, access the cache and populate the results
        cacheaccess(access.address,c);
    }
   
    /* Do not modify code below. */
    /* Make sure that all the parameters are appropriately populated. */
    print_statistics(g_num_cache_tag_bits, g_cache_offset_bits, &g_result);

    //when the trace file has no more addresses, free the memory from the cache structure
    clearCache(c);
    /* Close the trace file. */
    fclose(ptr_file);

    return 0;
}
