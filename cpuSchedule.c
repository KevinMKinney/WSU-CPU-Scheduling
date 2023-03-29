#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#define BUF_SIZE 128

//Using mutexes for synchronization instead of semaphores
pthread_mutex_t ready_lock;
pthread_mutex_t io_lock;

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
    int alg;
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
    if(d == NULL) exit(1);

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
    
    node *n = (node *) malloc(sizeof(node));
    if(n == NULL) exit(1);

    n->proc = tokens;
    n->prior = priority;
    n->size = size;
    
    n->index = i;
    if(d->head == NULL){
        n->next = NULL;
        n->prev = NULL;
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
    if(Str1 == NULL) exit(1);

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
            pthread_mutex_lock(&ready_lock);
            insert_node_back(tokens, 0, size, priority, d);
            pthread_mutex_unlock(&ready_lock);
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
        //Lock the io queue, get/remove the head of queue
        pthread_mutex_lock(&io_lock);
        temp = io->head;
	io->head = temp->next;
        pthread_mutex_unlock(&io_lock);

        //If anything in I/O queue, select fifo method, so get the head
        int index = temp->index;
        //Sleep for the given I/O burst time
	zzz = temp->proc[index] / 1000.0;
	printf("io proc is sleeping for %f\n", zzz);
	sleep(zzz);
        
        //Have to wait until ready queue is open to add more to it
        //Put the process back on the ready queue
        pthread_mutex_lock(&ready_lock);
        insert_node_back(temp->proc,++index, temp->size, temp->prior, d);
        pthread_mutex_unlock(&ready_lock);
        //Get the next I/O burst time
	free(temp);
        
    }
    return NULL;
}

// *** CPU Scheduling Thread Stuff *** //

//Scheduler for non prememptive first come first serve
//Returns the next process node for the cpu to do, and removes it from queue 
node * scheduleFCFS(DLL *d){
    node *n = d->head;
    d->head = n->next;
    //d->head->prev = NULL;
    return n;
}


//Scheduler for non prememptive shortest job first
//Returns the next process node for the cpu to do, and removes it from queue 
node * scheduleSJF(DLL *d){
    node *n = d->head;
    node *a = n->next;
    while(a != NULL){
        if(a->proc[a->index] < n->proc[n->index]){
            n = a;
        }
        a = a->next;
    }
    if(n == d->head){
        d->head = n->next;
        n->prev = NULL;
        return n;
    }
    else if(n== d->tail){
        d->tail = n->prev;
        n->prev->next = NULL;
        return n;
    }
    n->next->prev = n->prev;    
    n->prev->next = n->next;
    return n;
}

//Scheduler for non prememptive priority 
//Returns the next process node for the cpu to do, and removes it from queue 
node * schedulePR(DLL *d){
    node *n = d->head;
    node *a = n->next;
    while(a != NULL){
        if(a->prior  > n->prior){
            n = a;
        }
        a = a->next;
    }
    if(n == d->head){
        d->head = n->next;
        n->prev = NULL;
        return n;
    }
    else if(n== d->tail){
        d->tail = n->prev;
        n->prev->next = NULL;
        return n;
    }
    n->next->prev = n->prev;    
    n->prev->next = n->next;
    return n;
}
void* cpuScheduleFCFS(void* param) {
    struct thread_data *myTD = (struct thread_data *) param;
    DLL *d = myTD->r;
    DLL *io = myTD->ioq;
    int algo = myTD->alg;
    float zzz;
    node *temp = NULL;
    while (stop != 1 || d->head != NULL || io->head != NULL) {
	while (d->head == NULL) {
	    // waiting for process in the ready queue
            //always guaranteed to end with cpu burst
	}
        //Mutex used to synchronize ready queue

        //Get the first process in the ready queue, if unlocked by mutex
        pthread_mutex_lock(&ready_lock);
        if(algo == 0 || algo == 3) { temp = scheduleFCFS(d); }
        if(algo == 1) { temp = scheduleSJF(d); }
        if(algo == 2) { temp = schedulePR(d); }
        pthread_mutex_unlock(&ready_lock);

        //Then the designated amount of cpu burst time
        int index = temp->index;  
	zzz = temp->proc[index] / 1000.0;
	printf("cpu proc is sleeping for %f\n", zzz);
        //Then sleep for the appropiate amount in milliseconds
	sleep(zzz);
        
        //Once done, either move the process to the i/o queue or terminate process if it's the last burst
	if ( index < temp->size-1 ) {
            //If the I/O queue can be manipulated, then add to I/O queue
            pthread_mutex_lock(&io_lock);
            insert_node_back(temp->proc, ++index, temp->size, temp->prior, io);
            pthread_mutex_unlock(&io_lock);

	}else{    
            free(temp->proc);
        }
        //Or add back to list if there still process time left after quantum for when we implement Round Robin

	//free item from queue, since it's no longer needed
	free(temp);
	
        //Schedule another process for ready queue
    }
        
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
    int quantum = -1;
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
            quantum = atoi(argv[4]);
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

    //Set up threads and mutexes
    if(pthread_mutex_init(&ready_lock, NULL) != 0 || pthread_mutex_init(&io_lock, NULL) != 0){
        printf("The mutexes could not be created\n");
    }
    pthread_t tID;
    pthread_t cpuTID;
    pthread_t ioTID;
    void *thread_result;
    FILE* fp;
    int error;
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
        td.alg = curAlgo;
        error = pthread_create(&tID, NULL, parse_input, &td);
        if(error != 0){
            printf("Input Parser thread could not be created\n");
            exit(1);
        }

        // for CPU thread
        error = pthread_create(&cpuTID, NULL, cpuScheduleFCFS, &td);
        if(error != 0){
            printf("CPU thread could not be created\n");
            exit(1);
        }

        //For IO Thread
        error = pthread_create(&ioTID, NULL, ioSchedule, &td);
        if(error != 0){
            printf("IO thread could not be created\n");
            exit(1);
        }
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
    cpuStop = 1;

    pthread_join(ioTID, &thread_result);
    if (thread_result != 0) {
    	printf("IO thread returned an error\n");
    	exit(1);
    }

    if(fclose(fp) != 0){
        printf("Cannot close file\n");
        exit(1);
    }

    if(curAlgo == 3){
        printf("Input File Name                 : %s\n", argv[6]);
        printf("CPU Scheduling Alg              : %s (%d)\n", argv[2], quantum);
    }else{
        printf("Input File Name                 : %s\n", argv[4]);
        printf("CPU Scheduling Alg              : %s\n", argv[2]);
    }
    
    printf("Throughput                      : %d\n", 1);
    printf("Avg. Turnaround Time            : %d\n", 1);
    printf("Avg. Waiting Time in Ready Queue: %d\n", 1);
    free(ready);
    free(ioQueue);
    return 0;
}
