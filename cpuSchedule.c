#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#define BUF_SIZE 128


sem_t sem_name; //A semaphor to synchonize the queues
/**
 * The node used in the doubly linked list, allows us to get the next and previous values
*/
typedef struct listNode{
    //might need to add more variables for priority and other things
    int* proc;
    int prior;
    int size;
    int index;
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
    DLL *ioq;
};

// needs to be global so threads can comunicate
struct thread_data td;
int stop = 0;
int cpuStop = 0;
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
void insert_node_back(int *tokens,int i, int size, int priority, DLL *d){
    
    //Need a malloc check
    node *n = (node *) malloc(sizeof(node));
    //realloc(tokens, (size + 1) * sizeof(int));
    n->proc = tokens;
    n->prior = priority;
    n->size = size;
    
    n->index = i;
    if(d->head == NULL){
        n->next = NULL;
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
    //Need a malloc check
    char *Str1 = malloc(BUF_SIZE * sizeof(char)); 
    while(fgets(Str1, BUF_SIZE, f)){
        if( strncmp("sleep", Str1, 5) == 0){
            //Case of sleep, get the value for how long and sleep
            int val;

            //Need a malloc check
            char *TempStr = malloc(BUF_SIZE * sizeof(char));
            strncpy(TempStr, Str1+6, BUF_SIZE);
            val = atoi(TempStr);
            free(TempStr);
            sleep(val/1000.0);
            continue;
        }
        if(strncmp("stop", Str1, 4)==0){
            //When we are to stop, then free the data and return null from the function
            free(Str1);
            return NULL;
        }
        if(strncmp("proc", Str1, 4) == 0){
            //Add a new process to the ready queue
            char const delim = ' ';
            int i = 0;
            // first token will always be "proc", so we can ignore it
            char* tok = strtok(Str1, &delim);
            tok = strtok(NULL, &delim);
            int priority = atoi(tok);
            tok = strtok(NULL, &delim);
            int size = atoi(tok);

            int* tokens = malloc((size+1) * sizeof(int));
            tok = strtok(NULL, &delim);
            while(tok != NULL) {
                tokens[i] = atoi(tok);
                i++;
                tok = strtok(NULL, &delim);
            }
            
            tokens[size] = -1;
            sem_wait(&sem_name);
            insert_node_back(tokens, 0, size, priority, d);
            sem_post(&sem_name);
        }
    }

    free(Str1);
    return NULL;        
}


void * ioSchedule(void* param){
    //Same as cpu scheduler, but more stuff
    struct thread_data *myTD = (struct thread_data *) param;
    DLL *d = myTD->r;
    DLL *io = myTD->ioq;
    float zzz;
    node *temp = NULL;
    
 
    while (stop != 1 || d->head != NULL || io->head != NULL) {
	while (io->head == NULL) {
	    // waiting for process in the io queue
            //If out of jobs, meaning the cpu queue has stopped, then finish with thread
            if(cpuStop == 1){ return NULL; }
	}
        //If anything in I/O queue, select fifo method, so get the head
        int index = io->head->index;
        //Sleep for the given I/O burst time
	zzz = io->head->proc[index] / 1000.0;
	printf("io proc is sleeping for %f\n", zzz);
	sleep(zzz);
        
        //Have to wait until ready queue is open to add more to it
        sem_wait(&sem_name);
        //Put the process back on the ready queue
        insert_node_back(io->head->proc,++index, io->head->size, io->head->prior, d);
        sem_post(&sem_name);
        //Get the next I/O burst time
	temp = io->head->next;
	free(io->head);
	io->head = temp;
    }
    return NULL;
}

// *** CPU Scheduling Thread Stuff *** //

void* cpuScheduleFCFS(void* param) {
    struct thread_data *myTD = (struct thread_data *) param;
    DLL *d = myTD->r;
    DLL *io = myTD->ioq;
    float zzz;
    node *temp = NULL;

    while (stop != 1 || d->head != NULL || io->head != NULL) {
	while (d->head == NULL) {
	    // waiting for process in the ready queue
            //always guaranteed to end with cpu burst
	}
        sem_wait(&sem_name);
        //semaphor used to synchronize ready queue

        //Get the first process in the ready queue
        //Then the designated amount of cpu burst time
        int index = d->head->index;                  
	zzz = d->head->proc[index] / 1000.0;
	printf("cpu proc is sleeping for %f\n", zzz);
        //Then sleep for the appropiate amount in milliseconds
	sleep(zzz);
        
        //Once done, either move the process to the i/o queue or terminate process if it's the last burst
	if ( index < d->head->size-1 ) {
            insert_node_back(d->head->proc, ++index, d->head->size, d->head->prior, io);
	}else{    
            free(d->head->proc);
        }
        //Schedule another process for ready queue
	temp = d->head->next;
	free(d->head);
	d->head = temp;
        sem_post(&sem_name);
    }

    printf("end\n");
    cpuStop = 1;//Tell the io scheduler to exit
    return NULL;
}

int main(int argc, char const *argv[]) {
   	
   	// checking if arguments/options are valid
    if (argc < 5) {
	    printf("Not enough arguments given\n");
   	    exit(0);
    }

    // curAlgo corresponds to the index in {FCFS, SJF, PR, RR};
    int curAlgo = -1;

    if (strncmp("-alg", argv[1], 4) == 0) {
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
        printf("Could not find the -alg option\n");
        exit(0);
    }
    sem_init(&sem_name, 0, 1);
    pthread_t tID;
    pthread_t cpuTID;
    pthread_t ioTID;
    void *thread_result;
    FILE* fp;
    DLL* ready = newDLL();
    DLL* ioQueue = newDLL();
    if (curAlgo == 3) {
    	fp = fopen(argv[6], "r");	
    } else {
    	fp = fopen(argv[4], "r");
    }
    if(fp){
        td.f = fp;
        td.r = ready;
        td.ioq = ioQueue;
        pthread_create(&tID, NULL, parse_input, &td);

        // for CPU thread
        if (curAlgo == 0) {
        	pthread_create(&cpuTID, NULL, cpuScheduleFCFS, &td);
    	}
        pthread_create(&ioTID, NULL, ioSchedule, &td);
    }
    else{//In case we can't open a file
        printf("Cannot open input file\n");
        exit(1);
    }
    
    pthread_join(tID, &thread_result);
    if (thread_result != 0) {
    	printf("Input parser thread returned an error\n");
    	exit(1);
    }
    stop = 1;
    
    pthread_join(cpuTID, &thread_result);
    if (thread_result != 0) {
    	printf("CPU thread returned an error\n");
    	exit(1);
    }

    pthread_join(ioTID, &thread_result);
    if (thread_result != 0) {
    	printf("IO thread returned an error\n");
    	exit(1);
    }

    if(fclose(fp) != 0){
        printf("Cannot close file\n");
        exit(1);
    }
    free(ready);
    free(ioQueue);
    return 0;
}
