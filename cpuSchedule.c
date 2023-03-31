#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#define BUF_SIZE 128
#define DEBUG 0

//Using mutexes for synchronization instead of semaphores
pthread_mutex_t ready_lock;
pthread_mutex_t io_lock;

int usleep(suseconds_t usec);

/**
 * The node used in the doubly linked list, allows us to get the next and previous values
*/
typedef struct listNode{
    int* proc;
    int prior;
    int size;
    int index;
    double arrivalTime;
    double lastUsed;
   	double waitTime;
    double execTime;
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
    DLL *comp;
    int alg;
};

// needs to be global so threads can comunicate
struct thread_data td;
int stop = 0;
int quantum = 0;

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
void moveNode(node* n, DLL* d) {
	if(d->head == NULL){
        n->next = NULL;
        n->prev = NULL;
        d->head = n;
        d->tail = n;
    }else{
    	node* temp = d->head;
        while (temp->next != NULL) {
        	temp = temp->next;
        }

        temp->next = n;
        n->prev = temp;
        n->next = NULL;
        d->tail = n;
    }
}

void insertNewNode(int *tokens,int i, int size, int priority, DLL *d){
    node *n = (node *) malloc(sizeof(node));
    if(n == NULL) exit(1);

    n->proc = tokens;
    n->prior = priority;
    n->size = size;
    n->index = i;

    
    struct timeval t;
    gettimeofday(&t, 0);
    n->arrivalTime = t.tv_sec * 1000.0 + (t.tv_usec/1000.0);
    
    n->lastUsed = n->arrivalTime;
    n->waitTime = 0;
    n->execTime = 0;

    /*
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
    }*/

    moveNode(n, d);
}

void newInsertNewNode(int *tokens,int i, int size, int priority, double arTime, double lu, double wTime, double eTime, DLL *d){
    
    node *n = (node *) malloc(sizeof(node));
    if(n == NULL) exit(1);

    n->proc = tokens;
    n->prior = priority;
    n->size = size;
    n->index = i;

    n->arrivalTime = arTime;
    n->lastUsed = lu;
    n->waitTime = wTime;
    n->execTime = eTime;

    moveNode(n, d);
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
        free(n);
        n = t;
    }
    free(n);
}

int ready_empty(DLL *r){
    int check;
    pthread_mutex_lock(&ready_lock);
    if(r->head == NULL) check = 1;
    else check = 0;
    pthread_mutex_unlock(&ready_lock);
    return check;
}

int io_empty(DLL *r){
    int check;
    pthread_mutex_lock(&io_lock);
    if(r->head == NULL) check = 1;
    else check = 0;
    pthread_mutex_unlock(&io_lock);
    return check;
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
            unsigned int val;
            char *TempStr = malloc(BUF_SIZE * sizeof(char));
            if(TempStr == NULL) exit(1);

            strncpy(TempStr, Str1+6, BUF_SIZE);
            val = atoi(TempStr);
            free(TempStr);
	    if(usleep(val * 1000) == -1){
                printf("Error with usleep\n");
                exit(1);
            }
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
            //Put the process on the ready queue
            pthread_mutex_lock(&ready_lock);
            insertNewNode(tokens, 0, size, priority, d);
            pthread_mutex_unlock(&ready_lock);
        }
    }

    free(Str1);
    return NULL;        
}

double getTime(node* n) {
	struct timeval t;
	gettimeofday(&t, 0);
	double curMili = t.tv_sec * 1000.0 + (t.tv_usec/1000.0);
    return curMili;
}

void * ioSchedule(void* param){
    //Same as cpu scheduler, but more stuff
    struct thread_data *myTD = (struct thread_data *) param;
    DLL *d = myTD->r;
    DLL *io = myTD->ioq;
    unsigned int zzz;
    node *temp = NULL;
 
    while (stop != 1 || ready_empty(d) == 0 || io_empty(io) == 0) {
	//The IO queue isn't the last thing, so if both are empty,then it is done
        if(io_empty(io) == 1) continue; 
        // waiting for process in the io queue
        //If out of jobs, meaning the cpu queue has stopped, then finish with thread
        //Lock the io queue, get/remove the head of queue
        pthread_mutex_lock(&io_lock);
        temp = io->head;
        pthread_mutex_unlock(&io_lock);
        
        //If anything in I/O queue, select fifo method, so get the head
        int index = temp->index;
        //Sleep for the given I/O burst time
	zzz = temp->proc[index] * 1000;
	if (DEBUG == 1) {
	    printf("io proc is sleeping for %d\n", zzz/1000);
	}
	if(usleep(zzz) == -1){
            printf("Error with usleep\n");
            exit(1);
        }

		temp->lastUsed = getTime(temp);

        //Have to wait until ready queue is open to add more to it
        //Put the process back on the ready queue
        pthread_mutex_lock(&ready_lock);
        //insertNewNode(temp->proc,++index, temp->size, temp->prior, d);
        //temp->index += 1;
        //moveNode(temp, d);
        newInsertNewNode(temp->proc,++index, temp->size, temp->prior, temp->arrivalTime, temp->lastUsed, temp->waitTime, temp->execTime, d);
        pthread_mutex_unlock(&ready_lock);

        //Get the next I/O burst time
        
        //Remove the node from the IO queue 
        pthread_mutex_lock(&io_lock);
	io->head = temp->next;
        pthread_mutex_unlock(&io_lock);
	free(temp);

    }
    return NULL;
}

// *** CPU Scheduling Thread Stuff *** //

//Scheduler for non prememptive first come first serve
//Returns the next process node for the cpu to do, and removes it from queue 
node * scheduleFCFS(DLL *d){
    node *n = d->head;
    //d->head = n->next;
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
    return n;
}
void* cpuSchedule(void* param) {
    struct thread_data *myTD = (struct thread_data *) param;
    DLL *d = myTD->r;
    DLL *io = myTD->ioq;
    DLL *comp = myTD->comp;
    int algo = myTD->alg;
    unsigned int zzz;
    node *temp = NULL;
    
    while (stop != 1 || ready_empty(d) == 0 || io_empty(io) == 0) {
	
	// waiting for process in the ready queue
        //always guaranteed to end with cpu burst
        while(d->head == NULL); 

        //Mutex used to synchronize ready queue

        pthread_mutex_lock(&ready_lock);
        //Get the process in the ready queue based on the scheduler
        //if unlocked by mutex
        if(algo == 0 || algo == 3) temp = scheduleFCFS(d);
        if(algo == 1) temp = scheduleSJF(d); 
        if(algo == 2) temp = schedulePR(d);
        pthread_mutex_unlock(&ready_lock);

        temp->waitTime += getTime(temp) - temp->lastUsed;

        //Then the designated amount of cpu burst time
        int index = temp->index;  
	zzz = temp->proc[index] * 1000;
	if (DEBUG == 1) {
		printf("cpu proc is sleeping for %d\n", zzz/1000);
	}
        //Then sleep for the appropiate amount in milliseconds
	if(usleep(zzz) == -1){
            printf("Error with usleep\n");
            exit(1);
        }

	temp->execTime += temp->proc[index];

        //Once done, either move the process to the i/o queue or terminate process if it's the last burst
	if ( index < temp->size-1 ) {
            //If the I/O queue can be manipulated, then add to I/O queue
            pthread_mutex_lock(&io_lock);
            //insertNewNode(temp->proc, ++index, temp->size, temp->prior, io);
            //temp->index += 1;
        	//moveNode(temp, io);
            newInsertNewNode(temp->proc, ++index, temp->size, temp->prior, temp->arrivalTime, temp->lastUsed, temp->waitTime, temp->execTime, io);
            pthread_mutex_unlock(&io_lock);

	}else{    
			temp->lastUsed = getTime(temp);
            newInsertNewNode(temp->proc, 0, temp->size, temp->prior, temp->arrivalTime, temp->lastUsed, temp->waitTime, temp->execTime, comp);
            free(temp->proc);
        }
        
        //Remove from the queue once the node is done
        pthread_mutex_lock(&ready_lock);
        if(temp == d->head){
            d->head = temp->next;
            temp->prev = NULL;
        }
        else if(temp== d->tail){
            d->tail = temp->prev;
            temp->prev->next = NULL;
        }else{
            temp->next->prev = temp->prev;    
            temp->prev->next = temp->next;
        }
        pthread_mutex_unlock(&ready_lock);

	free(temp);
    }
        
    return NULL;
}

int min(int a, int b) {
    if (a > b) {
        return b;
    }
    return a;
}

// scheduler for round robin
void* cpuScheduleRR(void* param) {
    struct thread_data *myTD = (struct thread_data *) param;
    DLL *d = myTD->r;
    DLL *io = myTD->ioq;
    DLL *comp = myTD->comp;
    unsigned int zzz;
    node *temp = NULL;
    int procRunTime = 0;
    int index = 0;
    
    // Dequeue proc from head of ready queue
    // Run job for at most one quantum
    	// If it hasn't completed, preempt and add to tail of ready queue
		// If it is finished or blocked, pick another proc immediantly
	// Repeat

    while (stop != 1 || ready_empty(d) == 0 || io_empty(io) == 0) {
        //Guaranteed that cpu burst will be the last, so if any queue has something in it
        //there will be a cpu burst at the end
	while (d->head == NULL) {};

		pthread_mutex_lock(&ready_lock);
        temp = scheduleFCFS(d); // FCFS & RR get procs the same way (top of DLL)
        pthread_mutex_unlock(&ready_lock);

        temp->waitTime += getTime(temp) - temp->lastUsed;

        index = temp->index;
        procRunTime = min(temp->proc[index], quantum);
        temp->proc[index] -= procRunTime;
		
		zzz = procRunTime  * 1000;
		if (DEBUG == 1) {
			printf("cpu proc is sleeping for %d\n", zzz/1000);
		}
		//Then sleep for the appropiate amount in milliseconds
		if(usleep(zzz) == -1){
            printf("Error with usleep\n");
            exit(1);
        }

		temp->execTime += procRunTime;

	    if (temp->proc[index] == 0) {
	        if (index < temp->size-1) {
		    pthread_mutex_lock(&io_lock);
		    //insertNewNode(temp->proc, ++index, temp->size, temp->prior, io);
		    newInsertNewNode(temp->proc, ++index, temp->size, temp->prior, temp->arrivalTime, temp->lastUsed, temp->waitTime, temp->execTime, io);
		    pthread_mutex_unlock(&io_lock);
		} else {
		    // proc is done
		    temp->lastUsed = getTime(temp);

		    //insertNewNode(temp->proc, 0, temp->size, temp->prior, comp);
		    newInsertNewNode(temp->proc, 0, temp->size, temp->prior, temp->arrivalTime, temp->lastUsed, temp->waitTime, temp->execTime, comp);
		    free(temp->proc);
		}
	    } else {

	    	temp->lastUsed = getTime(temp);

	    	pthread_mutex_lock(&ready_lock);
	    	//insertNewNode(temp->proc, index, temp->size, temp->prior, d);
	    	newInsertNewNode(temp->proc, index, temp->size, temp->prior, temp->arrivalTime, temp->lastUsed, temp->waitTime, temp->execTime, d);
	    	pthread_mutex_unlock(&ready_lock);
	    }

	    //Remove the node from the queue
	    pthread_mutex_lock(&ready_lock);
            d->head = temp->next;
            temp->prev = NULL;
	    pthread_mutex_unlock(&ready_lock);
	    free(temp);
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
    DLL* completedProc = newDLL();
    struct timeval t;
    double startTime = 0;
    
    if (curAlgo == 3) {
    	fp = fopen(argv[6], "r");	
    } else {
    	fp = fopen(argv[4], "r");
    }

    gettimeofday(&t, 0);
    startTime = t.tv_sec * 1000.0 + (t.tv_usec/1000.0);

    if(fp){
        td.f = fp;
        td.r = ready;
        td.ioq = ioQueue;
        td.comp = completedProc;
        td.alg = curAlgo;

        error = pthread_create(&tID, NULL, parse_input, &td);
        if(error != 0){
            printf("Input Parser thread could not be created\n");
            exit(1);
        }

        // for CPU thread
        if (curAlgo == 3) {
            error = pthread_create(&cpuTID, NULL, cpuScheduleRR, &td);
        } else {
            error = pthread_create(&cpuTID, NULL, cpuSchedule, &td);
        }
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

    pthread_join(ioTID, &thread_result);
    if (thread_result != 0) {
    	printf("IO thread returned an error\n");
    	exit(1);
    }

    if(fclose(fp) != 0){
        printf("Cannot close file\n");
        exit(1);
    }

    // calculating times

    gettimeofday(&t, 0);
    double endTime = t.tv_sec * 1000.0 + (t.tv_usec/1000.0);
    node* temp = completedProc->head;
    float procAmount = 0;
    double wtSum = 0;
    double ttSum = 0;

    if (DEBUG == 1) {
    	printf("Time taken: %f\n", endTime - startTime);
    }

    while (temp != NULL) {
    	procAmount++;
    	wtSum += temp->waitTime;
    	ttSum += temp->lastUsed - temp->arrivalTime;
    	if (DEBUG == 1) {
    		printf("process #%d times: wait - %f | exec - %f | turn around - %f\n", (int) procAmount, temp->waitTime, temp->execTime, temp->lastUsed - temp->arrivalTime);
    	}
    	temp = temp->next;
    }

    if(curAlgo == 3){
        printf("Input File Name                 : %s\n", argv[6]);
        printf("CPU Scheduling Alg              : %s (%d)\n", argv[2], quantum);
    }else{
        printf("Input File Name                 : %s\n", argv[4]);
        printf("CPU Scheduling Alg              : %s\n", argv[2]);
    }
    
    printf("Throughput                      : %f\n", procAmount/((endTime - startTime)));
    printf("Avg. Turnaround Time            : %f\n", ttSum/procAmount);
    printf("Avg. Waiting Time in Ready Queue: %f\n", wtSum/procAmount);

    // memory clean-up
    free_DLL(completedProc);
    free(completedProc);
    free(ready);
    free(ioQueue);
    return 0;
}
