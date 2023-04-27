#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

/* beginning timeval to measure the beginning time of the program */
struct timeval tbegin;

/* the time quantum (ms) */
int Q = 20;

pthread_mutex_t *queueMutex;
pthread_mutex_t finishedProcessesMutex;
pthread_mutex_t *loadNumMutex;

char* alg;
char* infile;
char* outfile;

/* the number of load in queues*/
int* loadNum;

int outmode; // OUTMODE 

struct Node** readyProcesses;
int readyQueueNum;
struct Node* finishedProcesses; 

typedef struct {
    int pid;
    int burstLength;
    int arrivalTime;
    int remainingTime;
    int finishTime;
    int turnaroundTime;
    int waitingTime;
    int processorId;
} BurstItem;

struct Node {
    BurstItem pcb;
    struct Node *next;
};

struct arg {
    /* the processor's id */
    int processorId;
    /* the process ready queue */
    struct Node** readyQueue;
};

/*function to add a new node to queue linked list*/
static void addNodeToEnd(struct Node** head, int pid, int processorId, int arrivalTime, 
                         int burstLength, int remainingTime, int finishTime, int turnaroundTime,
                         int waitingTime){
    struct Node* nodeNew = (struct Node*) malloc(sizeof(struct Node));
    nodeNew->pcb.pid = pid;
    nodeNew->pcb.processorId = processorId;
    nodeNew->pcb.arrivalTime = arrivalTime;
    nodeNew->pcb.burstLength = burstLength;
    nodeNew->pcb.remainingTime = remainingTime;
    nodeNew->pcb.finishTime = finishTime;
    nodeNew->pcb.turnaroundTime = turnaroundTime;
    nodeNew->pcb.waitingTime = waitingTime;
    nodeNew->next = NULL;

    struct Node* last = *head;

    if(*head == NULL){
        *head = nodeNew;
        return;
    }

    while (last->next != NULL){
        last = last->next;
    }

    last->next = nodeNew;
    return;
}

/*function to add a dummyNode to queue linked list*/
static void addNodeToEndDummy(struct Node** head, int processorId){
    struct Node* nodeNew = (struct Node*) malloc(sizeof(struct Node));
    nodeNew->pcb.pid = -1;
    nodeNew->pcb.processorId = processorId;
    nodeNew->next = NULL;

    struct Node* last = *head;

    if(*head == NULL){
        *head = nodeNew;
        return;
    }

    while (last->next != NULL){
        last = last->next;
    }

    last->next = nodeNew;
    return;
}

/* function that adds according to SJF */
static void addNodeAccordingToSJF(struct Node** head, int pid, int processorId, int arrivalTime, 
                                  int burstLength, int remainingTime, int finishTime, int turnaroundTime,
                                  int waitingTime){
    struct Node* nodeNew = (struct Node*) malloc(sizeof(struct Node));
    nodeNew->pcb.pid = pid;
    nodeNew->pcb.processorId = processorId;
    nodeNew->pcb.arrivalTime = arrivalTime;
    nodeNew->pcb.burstLength = burstLength;
    nodeNew->pcb.remainingTime = remainingTime;
        nodeNew->pcb.finishTime = finishTime;
    nodeNew->pcb.turnaroundTime = turnaroundTime;
    nodeNew->pcb.waitingTime = waitingTime;
    nodeNew->next = NULL;

    struct Node* last = *head;

    if(*head == NULL){
        *head = nodeNew;
        return;
    }

    if(last->pcb.burstLength > nodeNew->pcb.burstLength){
        *head = nodeNew;
        nodeNew->next = last;
        return;
    }

    while (last->next != NULL && 
           last->next->pcb.burstLength <= nodeNew->pcb.burstLength)
    {
        last = last->next;
    }

    struct Node* temp = last->next;
    last->next = nodeNew;
    nodeNew->next = temp;
    return;
}

/* function to delete the head node in linked list */
static void deleteHeadNode(struct Node** head, int processorId){
    struct Node* current = *head;

    if(*head == NULL){
        return;
    }

    *head = current->next;

    free(current);
    pthread_mutex_lock(&loadNumMutex[processorId]);
    loadNum[processorId]--;
     pthread_mutex_unlock(&loadNumMutex[processorId]);
    return;
}

void static printInformation(struct Node** head, int currentTime){
    if(*head == NULL){
        printf("Error! Empty list\n");
    }
    
    else{
        FILE* out = fopen(outfile, "w");
        struct Node** now = head;

        while(now != NULL){
            if(strlen(outfile) == 0){
                printf("time = %d, cpu = %d, pid = %d, burstlen = %d, remainingtime = %d\n", currentTime, (*now)->pcb.processorId, (*now)->pcb.pid, (*now)->pcb.burstLength, (*now)->pcb.remainingTime);
            }
            else if(strlen(outfile) > 0){
            fprintf(out, "time = %d, cpu = %d, pid = %d, burstlen = %d, remainingtime = %d\n", currentTime, (*now)->pcb.processorId, (*now)->pcb.pid, (*now)->pcb.burstLength, (*now)->pcb.remainingTime);
            }
            now = &((*now)->next);
        }
        fprintf(out, "-------END OUTMODE2-------\n"); // will be deleted
    }
}

void static printOutMode3(struct Node** head, int stayFor, char* alg){
    if(head == NULL){
        printf("Error! Empty list\n");
    }

    else{
        FILE* out = fopen(outfile, "w");
        struct Node** now = head;

        while(now != NULL){
            if(strcmp(alg, "FCFS") == 0 || strcmp(alg, "SJF") == 0){
                if(strlen(outfile) == 0){
                    printf("pid = %d, cpu = %d, it will stay for = %d\n", (*now)->pcb.pid, (*now)->pcb.processorId, (*now)->pcb.remainingTime);
                }
                else if(strlen(outmode) > 0){
                    fprintf(out, "pid = %d, cpu = %d, it will stay for = %d\n", (*now)->pcb.pid, (*now)->pcb.processorId, (*now)->pcb.remainingTime);
                }
            }
            else if(strcmp(alg, "RR") == 0){
                if(strlen(outfile) == 0){
                    printf("pid = %d, remaining time = %d, cpu = %d, it will stay for = %d\n", (*now)->pcb.pid, (*now)->pcb.remainingTime, (*now)->pcb.processorId, stayFor);
                }
                else if(strlen(outmode) > 0){
                    fprintf(out, "pid = %d, remaining time = %d, cpu = %d, it will stay for = %d\n", (*now)->pcb.pid, (*now)->pcb.remainingTime, (*now)->pcb.processorId, stayFor);
                }
            }
            now = &((*now)->next);
        }
        fprintf(out, "-------END OUTMODE3-------\n"); // will be deleted
    }
}

/* the function to be executed by threads (processors) */
static void *processBurst(void *arg_ptr){
    int processorId = ((struct arg *) arg_ptr)->processorId;
    struct Node** readyQueue = ((struct arg *) arg_ptr)->readyQueue;
    int isDummyDetected = 0;
    struct timeval now;
    struct timeval finishTime;
    while(*readyQueue == NULL || !isDummyDetected || loadNum[processorId] > 1){
        pthread_mutex_lock(&queueMutex[processorId]);
        if(*readyQueue == NULL){
            pthread_mutex_unlock(&queueMutex[processorId]);
            usleep(1000);
        }
        else if((*readyQueue)->pcb.pid == -1){
            printf("dummy detected\n");
            isDummyDetected = 1;
        }
        else{
            //printf("Process starts, processor id = %d, pid = %d burst length = %d\n", (*readyQueue)->pcb.processorId, (*readyQueue)->pcb.pid, (*readyQueue)->pcb.burstLength);
            int isFinished = 0;
            struct Node** current;
            int finPid, finBurst, finArr, finRem, finFinTime, finTurn, finWaiting, finProcessorId;

            if(strcmp(alg, "SJF") == 0 || strcmp(alg, "FCFS") == 0)
            {
                current = readyQueue;

                if(outmode == 2){
                    printf("OUTMODE 2\n");
                    gettimeofday(&now, NULL);
                    int currentTime = 1000 * (now.tv_sec - tbegin.tv_sec) + 0.001 * (now.tv_usec - tbegin.tv_usec);
                    printInformation(current, currentTime);
                }

                else if(outmode == 3){
                    printf("OUTMODE 3\n"); 
                    printOutMode3(current, (*current)->pcb.remainingTime, alg);
                }
                usleep((*current)->pcb.burstLength * 1000); 
                gettimeofday(&finishTime, 0);
                (*current)->pcb.finishTime = 1000 * (finishTime.tv_sec - tbegin.tv_sec) + 0.001 * (finishTime.tv_usec - tbegin.tv_usec);
                (*current)->pcb.turnaroundTime = (*current)->pcb.finishTime - (*current)->pcb.arrivalTime;
                (*current)->pcb.waitingTime = (*current)->pcb.turnaroundTime - (*current)->pcb.burstLength;
                (*current)->pcb.remainingTime = 0; 
                isFinished = 1;
                finPid = (*current)->pcb.pid;
                finBurst = (*current)->pcb.burstLength;
                finArr = (*current)->pcb.arrivalTime;
                finRem = (*current)->pcb.remainingTime;
                finFinTime = (*current)->pcb.finishTime;
                finTurn = (*current)->pcb.turnaroundTime;
                finWaiting = (*current)->pcb.waitingTime;
                finProcessorId = (*current)->pcb.processorId;
                deleteHeadNode(readyQueue, (*current)->pcb.processorId);
                pthread_mutex_unlock(&queueMutex[processorId]);
            }

            if(strcmp(alg, "RR") == 0){
                current = readyQueue;
                if((*current)->pcb.remainingTime < Q){
                    if(outmode == 2){
                        printf("OUTMODE 2\n");
                        gettimeofday(&now, NULL);
                        double currentTime = 1000 * (now.tv_sec - tbegin.tv_sec) + 0.001 * (now.tv_usec - tbegin.tv_usec);
                        printInformation(current, currentTime);
                    }
                    else if(outmode == 3){
                        printf("OUTMODE 3\n");
                        if((*current)->pcb.remainingTime < Q){
                            printOutMode3(current, (*current)->pcb.remainingTime, alg);
                        }
                        else{
                            printOutMode3(current, Q, alg);
                        }                       
                    }
                    usleep((*current)->pcb.remainingTime * 1000);
                    gettimeofday(&finishTime, 0);
                    (*current)->pcb.finishTime = 1000 * (finishTime.tv_sec - tbegin.tv_sec) + 0.001 * (finishTime.tv_usec - tbegin.tv_usec);
                    (*current)->pcb.turnaroundTime = (*current)->pcb.finishTime - (*current)->pcb.arrivalTime;
                    (*current)->pcb.waitingTime = (*current)->pcb.turnaroundTime - (*current)->pcb.burstLength;
                    (*current)->pcb.remainingTime = 0; 
                    isFinished = 1;
                    finPid = (*current)->pcb.pid;
                    finBurst = (*current)->pcb.burstLength;
                    finArr = (*current)->pcb.arrivalTime;
                    finRem = (*current)->pcb.remainingTime;
                    finFinTime = (*current)->pcb.finishTime;
                    finTurn = (*current)->pcb.turnaroundTime;
                    finWaiting = (*current)->pcb.waitingTime;
                    finProcessorId = (*current)->pcb.processorId;
                    deleteHeadNode(readyQueue, (*current)->pcb.processorId);
                }
                else {
                    if(outmode == 2){
                        printf("OUTMODE 2\n");
                        gettimeofday(&now, NULL);
                        double currentTime = 1000 * (now.tv_sec - tbegin.tv_sec) + 0.001 * (now.tv_usec - tbegin.tv_usec);
                        printInformation(current, currentTime);
                    }
                    else if(outmode == 3){
                        printf("OUTMODE 3\n");
                        if((*current)->pcb.remainingTime < Q){
                            printOutMode3(current, (*current)->pcb.remainingTime, alg);
                        }
                        else{
                            printOutMode3(current, Q, alg);
                        }    
                    }
                    usleep(Q);
                    (*current)->pcb.remainingTime = (*current)->pcb.remainingTime - Q;
                    deleteHeadNode(readyQueue, (*current)->pcb.processorId);
                    addNodeToEnd(readyQueue, (*current)->pcb.pid, (*current)->pcb.processorId, (*current)->pcb.arrivalTime, 
                                (*current)->pcb.burstLength, (*current)->pcb.remainingTime, 0, 0 , 0);
                }
                pthread_mutex_unlock(&queueMutex[processorId]);
            }

            if(isFinished)
            {
                pthread_mutex_lock(&finishedProcessesMutex);
                addNodeToEnd(&finishedProcesses,finPid, finProcessorId, finArr, 
                             finBurst, finRem, finFinTime, 
                             finTurn, finWaiting);
                pthread_mutex_unlock(&finishedProcessesMutex);
            }
        }  
    }
    printf("Thread no %d exited\n", processorId);
    pthread_exit(NULL); 
}

/*function to find the smallest integer position in an array*/
static int findSmallestIntPos(int* intArr, int numOfElements){
    int val = intArr[0];
    int position = 0;
    for(int i = 0; i < numOfElements; i++){
        if(val > intArr[i]){
            val = intArr[i];
            position = i;
        }
    }
    return position;
}

int main(int argc, char* argv[])
{
    /* get the start time of the execution */
    gettimeofday(&tbegin, 0);

    /* the thread (processor) ids */
    pthread_t* tids;
    /* thread (processor) function arguments */
    struct arg* t_args;
    /* the number of processors */
    int N = 2;
    int T=200, T1=10, T2=1000, L=100, L1=10, L2=500, pc=10;
    outmode = 1;
    /* the scheduling approach S or M */
    char* sap = "M";
    /* the queue selection method */
    char* qs = "RM";
    /* the scheduling algorithm */
    alg = "RR";

    /* pid count */
    int pidCount = 0;

    /* file pointer */
    FILE* filePtr;

    /* the turn number for queues for RM */
    int queueTurn = 0;

    /* check if -i specified */
    int isISpecified = 0;

    /* check if -r specified */
    int isRSpecified = 0;

    time_t t;

    for(int i = 1; i < argc; i++) {
        char* cur = argv[i];

        if(strcmp(cur, "-n") == 0) {
            N = atoi(argv[++i]);
        }
        else if(strcmp(cur, "-a") == 0) {
            sap = argv[++i];
            qs = argv[++i];
        }
        else if(strcmp(cur, "-s") == 0) {
            alg = argv[++i];
            Q = atoi(argv[++i]);
        }
        else if(strcmp(cur, "-i") == 0) {
            infile = argv[++i];
            isISpecified = 1;
        }
        else if(strcmp(cur, "-m") == 0) {
            outmode = atoi(argv[++i]);
        }
        else if(strcmp(cur, "-o") == 0) {
            outfile = argv[++i];
        }
        else if(strcmp(cur, "-r") == 0) {
            T=atoi(argv[++i]); T1=atoi(argv[++i]); T2=atoi(argv[++i]);
            L=atoi(argv[++i]); L1=atoi(argv[++i]); L2=atoi(argv[++i]);
            pc=atoi(argv[++i]);
            isRSpecified = 1;
        }
        else {
            printf("[-] Ignoring the unknown flag/argument: %s", cur);
        }
    }

    /* checking specified input conditions:
        * If sap is set to be M, we should have N to be greater than 1
        * If sap is set to be S, qs must be set to NA
        * If alg is set to FCFS or SJF, Q has to be set to 0
        NOTE: The checking procedure will be done after the inputs are taken because
        flags can be specified in random order!
    */

    if(strcmp(sap, "M") == 0 && N <= 1){
        N = 2; //set N to its default value if the input N is wrong
    }

    if(strcmp(sap, "S") == 0){
        strcpy(qs, "NA"); //not applicable 
    }

    if(strcmp(alg, "FCFS") == 0 || strcmp(alg,"SJF") == 0){
        Q = 0;
    }


    /* After everything is set, the process can begin...*/

    /* MUTEX LOCK INITIALIZATION */
    /* initialize the queue mutex array */
    if(strcmp(sap, "M") == 0){
        queueMutex = (pthread_mutex_t*) malloc(N * sizeof(pthread_mutex_t));
        loadNumMutex = (pthread_mutex_t*) malloc(N * sizeof(pthread_mutex_t));
        /* initialize every mutex lock of queue(s) inside for loop */
        for (int i = 0; i < N; i++){
            pthread_mutex_init(&queueMutex[i], NULL);
            pthread_mutex_init(&loadNumMutex[i], NULL);
        }        
    }
    else if(strcmp(sap, "S") == 0){
        queueMutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        loadNumMutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(&queueMutex[0], NULL);
        pthread_mutex_init(&loadNumMutex[0], NULL);
    }

    /* initialize the finished processes mutex lock*/
    pthread_mutex_init(&finishedProcessesMutex, NULL);
    
    /* QUEUE(S) CREATION */
    if(strcmp(sap, "M") == 0){
        readyProcesses = (struct Node**) malloc(N * sizeof(struct Node*));
        readyQueueNum = N;
        for(int i = 0; i < N; i++){
            readyProcesses[i] = NULL;
        }
        loadNum = (int*) malloc(N * sizeof(int));
        for(int i = 0; i < N; i++){
            loadNum[i] = 0;
        }
    }
    else if(strcmp(sap, "S") == 0){
        readyProcesses = (struct Node**) malloc(sizeof(struct Node*));
        readyQueueNum = 1;
        readyProcesses[0] = NULL;
        loadNum = (int*) malloc(sizeof(int));
        loadNum[0] = 0;
    }

    /* THREAD CREATION PART */
    /* dynamically allocate the number of thread ids */
    tids = (pthread_t*) malloc(N * sizeof(pthread_t));

    /* dynamically allocate the number of thread arguments */
    t_args = (struct arg*) malloc (N * sizeof(struct arg)); 

    /*  creating threads (processors) */
    int ret;

    for(int tIndex = 0; tIndex < N; tIndex++){
        t_args[tIndex].processorId = tIndex;
        if(strcmp(sap, "M") == 0){
            t_args[tIndex].readyQueue = &readyProcesses[tIndex];
        }
        else if(strcmp(sap, "S") == 0){
            t_args[tIndex].readyQueue = &readyProcesses[0];
        }
        ret = pthread_create(&(tids[tIndex]), NULL, processBurst,
                             (void *) &(t_args[tIndex]));
        
        if(ret != 0){
            exit(1);
        }
    }
    
    /*the time of the arrival of the process*/
    struct timeval tarrival;

    if(isISpecified && !isRSpecified){

        /* MAIN THREAD PROCESS READ FROM FILE SEGMENT */
        filePtr = fopen(infile, "r");

        if(filePtr == NULL)
        {
            exit(1);
        }

        /*information string*/
        char inputType[64];
        /*amount of time (ms)*/
        int timeInput;

        while(fscanf(filePtr, "%s %d", inputType, &timeInput) != EOF){
            if(strcmp(inputType, "PL") == 0){
                if(strcmp(qs, "RM") == 0){
                    pthread_mutex_lock(&queueMutex[queueTurn % N]);
                    gettimeofday(&tarrival, 0);
                    int arrivalTime = 1000 * (tarrival.tv_sec - tbegin.tv_sec) + 0.001 * (tarrival.tv_usec - tbegin.tv_usec);
                    if(strcmp(alg, "FCFS") == 0 || strcmp(alg, "RR") == 0){
                        addNodeToEnd(&readyProcesses[queueTurn % N],pidCount,queueTurn % N, arrivalTime, timeInput, timeInput, 0, 0, 0);
                    }

                    if(strcmp(alg,"SJF") == 0){
                        addNodeAccordingToSJF(&readyProcesses[queueTurn % N],pidCount,queueTurn % N, arrivalTime, timeInput, timeInput, 0, 0, 0);
                    }
                    queueTurn++;
                    pidCount++;
                    pthread_mutex_unlock(&queueMutex[queueTurn % N]);
                    pthread_mutex_lock(&loadNumMutex[queueTurn % N]);
                    loadNum[queueTurn % N] = loadNum[queueTurn % N] + 1;
                    pthread_mutex_unlock(&loadNumMutex[queueTurn % N]);
                }
                if(strcmp(qs,"LM") == 0){
                    int smallestIntPos = findSmallestIntPos(loadNum, N);
                    pthread_mutex_lock(&queueMutex[smallestIntPos]);
                    gettimeofday(&tarrival, 0);
                    int arrivalTime = 1000 * (tarrival.tv_sec - tbegin.tv_sec) + 0.001 * (tarrival.tv_usec - tbegin.tv_usec);
                    if(strcmp(alg, "FCFS") == 0 || strcmp(alg, "RR") == 0){
                        addNodeToEnd(&readyProcesses[smallestIntPos],pidCount,smallestIntPos, arrivalTime, timeInput, timeInput, 0, 0, 0);
                    }

                    if(strcmp(alg,"SJF") == 0){
                        addNodeAccordingToSJF(&readyProcesses[smallestIntPos],pidCount,smallestIntPos, arrivalTime, timeInput, timeInput, 0, 0, 0);
                    }
                    pidCount++;
                    pthread_mutex_unlock(&queueMutex[smallestIntPos]);
                    pthread_mutex_lock(&loadNumMutex[smallestIntPos]);
                    loadNum[smallestIntPos] = loadNum[smallestIntPos] + 1;
                    pthread_mutex_unlock(&loadNumMutex[smallestIntPos]);
                }
            }

            if(strcmp(inputType, "IAT") == 0){
                //usleep suspends execution for x microseconds
                usleep(timeInput * 1000);
            }
        }

        fclose(filePtr);
    }
    else {
        srand((unsigned) time(&t));

        for(int i = 0; i < pc; i++){
            double rateParameter = 1.0/L;
            double u, x;
            int num;
            do {
                u = (double)rand() / (double)RAND_MAX;
                x = (-log(1-u))/rateParameter;

                num = (int)round(x);
            } while(num < L1 || num > L2);

            //burst process
            if(strcmp(qs, "RM") == 0){
                pthread_mutex_lock(&queueMutex[queueTurn % N]);
                gettimeofday(&tarrival, 0);
                int arrivalTime = 1000 * (tarrival.tv_sec - tbegin.tv_sec) + 0.001 * (tarrival.tv_usec - tbegin.tv_usec);
                if(strcmp(alg, "FCFS") == 0 || strcmp(alg, "RR") == 0){
                    addNodeToEnd(&readyProcesses[queueTurn % N],pidCount,queueTurn % N, arrivalTime, num, num, 0, 0, 0);
                }

                if(strcmp(alg,"SJF") == 0){
                    addNodeAccordingToSJF(&readyProcesses[queueTurn % N],pidCount,queueTurn % N, arrivalTime, num, num, 0, 0, 0);
                }
                queueTurn++;
                pidCount++;
                pthread_mutex_unlock(&queueMutex[queueTurn % N]);
            }
            if(strcmp(qs,"LM") == 0){
                int smallestIntPos = findSmallestIntPos(loadNum, N);
                pthread_mutex_lock(&queueMutex[smallestIntPos]);
                gettimeofday(&tarrival, 0);
                int arrivalTime = 1000 * (tarrival.tv_sec - tbegin.tv_sec) + 0.001 * (tarrival.tv_usec - tbegin.tv_usec);
                if(strcmp(alg, "FCFS") == 0 || strcmp(alg, "RR") == 0){
                    //printf("The burst pid = %d, burst length = %d \n", pidCount, num);
                    addNodeToEnd(&readyProcesses[smallestIntPos], pidCount, smallestIntPos, arrivalTime, num, num, 0, 0, 0);
                }

                if(strcmp(alg,"SJF") == 0){
                    addNodeAccordingToSJF(&readyProcesses[smallestIntPos],pidCount,smallestIntPos, arrivalTime, num, num, 0, 0, 0);
                }
                pidCount++;
                pthread_mutex_unlock(&queueMutex[smallestIntPos]);
                loadNum[smallestIntPos] = loadNum[smallestIntPos] + 1;
            }

            //iat process
            rateParameter = 1.0/T;
            do {
                u = (double)rand() / (double)RAND_MAX;
                x = (-log(1-u))/rateParameter;

                num = (int)round(x);
            } while(num < T1 || num > T2);

            usleep(1000 * num);
        }
    }

    if(strcmp(sap, "M") == 0){
        for(int i = 0; i < N; i++){
            pthread_mutex_lock(&queueMutex[i]);
            addNodeToEndDummy(&readyProcesses[i],i);
            pthread_mutex_unlock(&queueMutex[i]);
        }
    }

    if(strcmp(sap, "S") == 0){
        pthread_mutex_lock(&queueMutex[0]);
        addNodeToEndDummy(&readyProcesses[0],0);
        pthread_mutex_unlock(&queueMutex[0]);
    }

    /* joining threads after their termination */
    for (int tIndex = 0; tIndex < N; tIndex++) {
	    ret = pthread_join(tids[tIndex], NULL);
		if (ret != 0) {
			exit(1);
		}
	}

    struct Node* curr = finishedProcesses;

    FILE* out = fopen(outfile, "w");

    fprintf(out, "%-10s %-10s %-10s %-10s %-10s %-12s %-10s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime", "turnaround");
    int avgTurnaround = 0;
    int countForAvg = 0;
    while(curr != NULL){
        fprintf(out, "%-10d %-10d %-10d %-10d %-10d %-12d %-10d\n", curr->pcb.pid, curr->pcb.processorId, curr->pcb.burstLength, curr->pcb.arrivalTime, curr->pcb.finishTime, curr->pcb.waitingTime, curr->pcb.turnaroundTime);
        avgTurnaround = avgTurnaround + curr->pcb.turnaroundTime;
        countForAvg++;
        curr = curr->next;
    }
    avgTurnaround = avgTurnaround / countForAvg;
    fprintf(out, "Average turnaround time: %d\n", avgTurnaround);

    fclose(out);

    free(tids);
    free(t_args);

    if(strcmp(sap, "S") == 0){
        struct Node** current = &readyProcesses[0];
        struct Node* next;

        while((*current) != NULL){
            next = (*current)->next;
            free((*current));
            (*current) = next;
        }

        pthread_mutex_destroy(&queueMutex[0]);
        pthread_mutex_destroy(&loadNumMutex[0]);
        free(queueMutex);
        free(loadNumMutex);
    }

    if(strcmp(sap, "M") == 0){
        for(int i = 0; i < N; i++){
            struct Node** current = &readyProcesses[i];
            struct Node* next;

            while((*current) != NULL){
                next = (*current)->next;
                free((*current));
                (*current) = next;
            }

            pthread_mutex_destroy(&queueMutex[i]);
            pthread_mutex_destroy(&loadNumMutex[i]);
        }
        free(queueMutex);
        free(loadNumMutex);
    }

    free(readyProcesses);

    struct Node* current = finishedProcesses;
    struct Node* next;

    while(current != NULL){
        next = current->next;
        free(current);
        current = next;
    }

    free(loadNum);

    exit(0);
    return(0);
}