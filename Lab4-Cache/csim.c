/*
 *  cache simulator
 *	author: chj
 *	email: 506933131@qq.com
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#define LENGTH_OF_FILENAME 30
#define LENGTH_OF_INSTRUCTION 5
#define UPPER_BOUND_ON_BITS_FOR_CACHE 30 // (set index bits + block index bits)'s upper_bound

/* define situations and return value for hit, miss and eviction */
#define FAILURE 0
#define HIT 1
#define MISS 2
#define MISS_EVICTION 3
#define MISS_HIT 4
#define MISS_EVICTION_HIT 5
const char *verbose_string[] = {"fail to manipulate the behavior of cache", "hit", "miss", "miss eviction",
    "miss hit", "miss eviction hit"};

typedef unsigned long long ULL;
/* store the cache(tag bits) and valid bit, dimension: set, row */
ULL **cache;
int **valid;
/* data structure that help allocate memory in the cache */
struct USED_ROW
{
    int row_index;
    struct USED_ROW *next;
};
struct USED_ROWS
{
    struct USED_ROW *head;
    struct USED_ROW *tail;
    int length;
} *used_rows;
/* s: Number of set index bits, E: Associativiy, b: Number of block index bits */
int s, E, b;
int set_count, block_count;
int verbose;
int hit_count, miss_count, eviction_count;

/* function to load the data in the memory */
int Load();
/* function to store the data in the memory */
int Store();
/* function to modify(load and store) the data in the memory */ 
int Modify();



/* move a row to the head of used_rows, which means this row is recently used */
void update_used_rows(int set_address_int, struct USED_ROW *row){
    if(row == used_rows[set_address_int].head)return;
    struct USED_ROW *row_front = used_rows[set_address_int].head;
    while(row_front->next != row){
        row_front = row_front->next;
    }
    if(used_rows[set_address_int].tail == row){
        used_rows[set_address_int].tail = row_front;
    }
    row_front->next = row->next;
    row->next = used_rows[set_address_int].head;
    used_rows[set_address_int].head = row;
    return;
}

/* insert a recently used row to the head of used_rows */
int push_head(int set_address_int, ULL tag_address){
    struct USED_ROW *new_row = (struct USED_ROW *)malloc(sizeof(struct USED_ROW) * 1);
    if(new_row == NULL || used_rows[set_address_int].length >= E)return 0;
    new_row -> next = used_rows[set_address_int].head;
    used_rows[set_address_int].head = new_row;
    if(used_rows[set_address_int].length == 0){
        used_rows[set_address_int].tail = new_row;
    }

    int row_index;
    for(row_index = 0;row_index < E;row_index++){
        if(valid[set_address_int][row_index] == 0)break;
    }
    new_row->row_index = row_index;
    cache[set_address_int][row_index] = tag_address;
    valid[set_address_int][row_index] = 1;
    used_rows[set_address_int].length++;
    return 1;
}

/* pop the least recently row in a particular used_rows */
int pop_tail(int set_address_int){
    if(used_rows[set_address_int].length == 0 || used_rows[set_address_int].tail == NULL)return 0;
    valid[set_address_int][used_rows[set_address_int].tail -> row_index] = 0;
    if(used_rows[set_address_int].length == 1){
        free(used_rows[set_address_int].tail);
        used_rows[set_address_int].head = NULL;
        used_rows[set_address_int].tail = NULL;
        used_rows[set_address_int].length = 0;
        return 1;
    }
    struct USED_ROW *tail_front = used_rows[set_address_int].head;
    while(tail_front->next != used_rows[set_address_int].tail){
        tail_front = tail_front -> next;
    }
    tail_front->next = NULL;
    free(used_rows[set_address_int].tail);
    used_rows[set_address_int].tail = tail_front;
    used_rows[set_address_int].length--;
    return 1;
}



/* main function */

int main(int argc, char* argv[]){
	verbose = 0;
	s = -1, E = 0, b = -1;
	char filename[LENGTH_OF_FILENAME];
	FILE* trace;
	int opt;
	while((opt = getopt(argc, argv, "vs:E:b:t:")) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                strncpy(filename, optarg, LENGTH_OF_FILENAME);
                break;
            default:
                fprintf(stderr, "Usage: %s [-v] -s <s> -E <E> -b <b> -t <tracefile>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

	/* check if some certain conditions are satisfied */
	if(s<=-1 || E<=0 || b<=-1){
	    fprintf(stderr, "Please specify the value of s, E and b or the values are invalid.\n");
	    exit(EXIT_FAILURE);
	}
	if(s+b > UPPER_BOUND_ON_BITS_FOR_CACHE){
	    fprintf(stderr, "Cache size is too big.\n");
	    exit(EXIT_FAILURE);
	}

    trace = fopen(filename, "r");
	if(trace == NULL){
	    fprintf(stderr, "Can't open the file <%s>, or you don't input the correct filename.\n", filename);
	    exit(EXIT_FAILURE);
	}

	/* allocate space for cache ,valid bits and relevant data structure */
    set_count = 1<<s;
    block_count = 1<<b;
    cache = (ULL **)malloc(sizeof(ULL *) * set_count);
	if(cache == NULL){
	    fprintf(stderr, "Can't allocate enough space for cache(first dimension).\n");
	    exit(EXIT_FAILURE);
	}
	valid = (int **)malloc(sizeof(int *) * set_count);
    if(valid == NULL){
        fprintf(stderr, "Can't allocate enough space for valid bits(first dimension).\n");
        exit(EXIT_FAILURE);
    }

    int i;
    for(i = 0;i < set_count;i++){
        cache[i] = (ULL *)malloc(sizeof(ULL) * E);
        valid[i] = (int *)malloc(sizeof(int) * E);
        if(cache[i] == NULL || valid[i] == NULL){
            fprintf(stderr, "Can't allocate enough space for cache[%d] or valid bits[%d].\n", i, i);
            exit(EXIT_FAILURE);
        }
        /* initialize the value in the valid[][] to be zero */
        memset(valid[i], 0, E * sizeof(int));
    }

    used_rows = (struct USED_ROWS *)malloc(sizeof(struct USED_ROWS) * set_count);
    if(used_rows == NULL){
        fprintf(stderr, "Can't allocate enough space for used_rows to record the usage of cache.\n");
        exit(EXIT_FAILURE);
    }
    for(i = 0;i < set_count;i++){
        used_rows[i].head = NULL;
        used_rows[i].tail = NULL;
        used_rows[i].length = 0;
    }

    /* count the hits, misses and evictions */
    hit_count = miss_count = eviction_count = 0;
    char instruction[LENGTH_OF_INSTRUCTION];
    ULL address;
    ULL tag_address, set_address; // not to use block_address
    int byte_count;
    while(fscanf(trace, "%s %llx,%d", instruction, &address, &byte_count) == 3){
        if(instruction[0] == 'I')continue;
        set_address = (address>>b) % ((ULL)set_count);
        tag_address = address >> (s+b);
        int result = FAILURE;
        switch (instruction[0])
        {
            case 'L':
                result = Load(tag_address, set_address);
                break;
            case 'S':
                result = Store(tag_address, set_address);
                break;
            case 'M':
                result = Modify(tag_address, set_address);
                break;
            default:
                fprintf(stderr, "wrong instruction: %s\n", instruction);
                break;
        }
        if(verbose)printf("%c %llx,%d %s\n", instruction[0], address, 1, verbose_string[result]);
        if(result == FAILURE){
            fprintf(stderr, "Reture value is not accurate!\n");
            exit(EXIT_FAILURE);
        }
    }

    /* free the allocated space */
    for(i = 0;i < set_count;i++){
        struct USED_ROW *row = used_rows[i].head;
        struct USED_ROW *row_front;
        while(row){
            row_front = row;
            row = row->next;
            free(row_front);
        }
    }
    free(used_rows);

    for(i = 0;i < set_count;i++){
        free(valid[i]);
        free(cache[i]);
    }
    free(valid);
    free(cache);
    fclose(trace);

    /* call the function provided to print result */
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

int Load(ULL tag_address, ULL set_address){
    int set_address_int = (int)set_address;
    /* a loop on the used rows of the given set to find if we can hit */
    int hit_flag = 0; // 1 means hit, miss by default
    struct USED_ROW *row = used_rows[set_address_int].head;
    while(row != NULL){
        if(cache[set_address_int][row->row_index] == tag_address){
            hit_flag = 1;
            break;
        }
        row = row->next;
    }
    if(hit_flag){
        hit_count++;
        update_used_rows(set_address_int, row);
        return HIT;
    }

    /* miss situation */
    miss_count++;
    if(used_rows[set_address_int].length == E){ /* miss and eviction */
        eviction_count++;
        if(pop_tail(set_address_int) == 0) return FAILURE;
        if(push_head(set_address_int, tag_address) == 0) return FAILURE;
        return MISS_EVICTION;
    }
    else{ /* miss but the set is not full */
        return push_head(set_address_int, tag_address) ? MISS : FAILURE;
    }
}

int Store(ULL tag_address, ULL set_address){
    return Load(tag_address, set_address);
}

int Modify(ULL tag_address, ULL set_address){
    hit_count++;
    int flag_load = Load(tag_address, set_address);
    if(flag_load == FAILURE)return FAILURE;
    if(flag_load == HIT)return HIT;
    if(flag_load == MISS)return MISS_HIT;
    if(flag_load == MISS_EVICTION)return MISS_EVICTION_HIT;
    return FAILURE;
}
