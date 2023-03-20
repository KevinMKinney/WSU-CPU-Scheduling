#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define BUF_SIZE 128


/**
 * The node used in the doubly linked list, allows us to get the next and previous values
*/
typedef struct listNode{
    //might need to add more variables for priority and other things
    char* proc;
    struct listNode *next;
    struct listNode *prev;
}node;

/**
 * The doubly linked list used for our ready and io queues. Stores our head and tail values.
*/
typedef struct doubleLinkedList{
    node *head;
    node *tail;
} DLL;

/**
 * Used to store parameters for the input parsing thread. 
*/
struct thread_data{
    FILE *f; //the file pointer of the file we are looking at
    DLL *r; //the ready queue for what we store all our processes in for cpu bursts
};

/**
 * Represents which algorithm is in use
*/
enum algo {FCFS, SJF, PR, RR};

/**
 * Creates a new doubly linked list with null values
*/
DLL * newDLL(){
    DLL *d;
    d = (DLL *) malloc(sizeof(DLL));
    d->head = NULL;
    d->tail = NULL;
    return d;
}

/**
 * This is used to insert a new node into the linked list with the contents provided from the parameters. It is inserted into the back
 * unless the struct is empty, then we just add it to the front. 
 *
 * Paramters: d, the doubly linked list we are adding the node into
 *            s, The contents we are adding into the node, this could be changed from string to an int
*/
void insert_node_back(char *s, DLL *d){
    
    node *n = (node *) malloc(sizeof(node));
    n->proc = malloc(BUF_SIZE * sizeof(char));

    strncpy(n->proc, s, BUF_SIZE);

    if(d->head == NULL){
        n->next = d->tail;
        n->prev = d->head;
        d->head = n;
        d->tail = n;
    }else{
        n->prev = d->tail;
        n->next = NULL;
        d->tail->next = n;
        d->tail = n;
    }
    
}

/**
* This frees all the nodes in doubly linked list, and it's contents.
* Parameters: d, the doubly linked list to free it's values 
*/
void free_DLL(DLL *d){
    node *n = d->head;
    node *t;
    while(n->next != NULL){
        t = n->next;
        free(n->proc);        
        free(n);
        n = t;
    }
    free(n->proc);
    free(n);
}

void *  parse_input(void *param){
    struct thread_data *myTD = (struct thread_data *) param;
    FILE *f = myTD->f;
    DLL *d = myTD->r;
    char *Str1 = malloc(BUF_SIZE * sizeof(char)); 
    while(fgets(Str1, BUF_SIZE, f)){
        if( strncmp("sleep", Str1, 5) == 0){
            //Case of sleep, get the value for how long and sleep
            int val;
            char *TempStr = malloc(BUF_SIZE * sizeof(char));
            strncpy(TempStr, Str1+6, BUF_SIZE);
            val = atoi(TempStr);
            free(TempStr);
            sleep(val/1000);
            continue;
        }
        if(strncmp("stop", Str1, 4)==0){
            //When we are to stop, then free the data and return null from the function
            free(Str1);
            return NULL;
        }
        if(strncmp("proc", Str1, 4) == 0){
            //Add a new process to the ready queue
            insert_node_back(Str1, d);
        }
    }
     
    free(Str1);
    return NULL;        
}

int main(int argc, char const *argv[]) {
   	
   	// checking if arguments/options are valid
   	if (argc < 5) {
   		printf("Not enough arguments given\n");
   		exit(0);
   	}

   	enum algo curAlgo = -1;

   	if (strncmp("-arg", argv[1], 4) == 0) {
   		if (strncmp("FCFS", argv[2], 4) == 0) {
   			curAlgo = 0;
   		}
   		if (strncmp("SJF", argv[2], 3) == 0) {
   			curAlgo = 1;
   		}
   		if (strncmp("PR", argv[2], 2) == 0) {
   			curAlgo = 2;
   		}
   		if (strncmp("RR", argv[2], 2) == 0) {
   			if (argc < 7) {
   				printf("Not enough arguments given\n");
   				exit(0);
   			}
   			curAlgo = 3;
   		}

   		if (curAlgo == -1) {
   			printf("Not a valid algorithm\n");
   			exit(0);
   		} 
   	} else {
   		printf("Could not find the -arg option\n");
   		exit(0);
   	}

    pthread_t tID;
    void *thread_result;
    FILE* fp;
    DLL* ready = newDLL();
    if (curAlgo == 3) {
    	fp = fopen(argv[6], "r");	
    } else {
    	fp = fopen(argv[4], "r");
    }
    if(fp){
        struct thread_data td;
        td.f = fp;
        td.r = ready;
        pthread_create(&tID, NULL, parse_input, &td);
    }
    else{//In case we can't open a file
        printf("Cannot open input file\n");
        exit(1);
    }
    
    pthread_join(tID, &thread_result);
    if(fclose(fp) != 0){
        printf("Cannot close file\n");
        exit(1);
    }
    free_DLL(ready);
    free(ready);
    return 0;
}
