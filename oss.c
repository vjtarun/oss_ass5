#include "oss.h"
static FILE *fp;
static int totalProcesses;
static int shmid, statsid, rcbid, cid, msgid;

static struct SharedMemory *shm;
static struct statistics *stats;
static struct ResourceControlBlock *rcb;
static struct statistics *stats;

static int (*pendingClaims)[20];

int main(int argc, char* argv[]) {
	// Signal Handler
	signal(SIGINT, signalHandler);
	signal(SIGSEGV, signalHandler);
	
	// Seed the random number generator
	srand((unsigned)(getpid() ^ time(NULL) ^ ((getpid()) >> MAX_USER_PROCESSES)));
	
	verbose = 0;
	int terminationTime = 30;
	int op;
	while ((op = getopt (argc, argv, "hvr:")) != -1) {
		switch (op) {
			case 'h':
				printHelp();
				exit(EXIT_SUCCESS);
				
			case 'v':
				verbose = 1;
				break;
				
			case 'r':
				if(isdigit(*optarg)) {
					terminationTime = atoi(optarg);
				} else {
					printf("'-r' expects an integer value\n");
					exit(EXIT_FAILURE);
				}
				break;
				
			case '?':
				if(optopt == 'r') {
					fprintf(stderr, "-%c requires an argument!\n", optopt);
				} else if(isprint(optopt)) {
					fprintf(stderr, "-%c is an unknown flag\n", optopt);
				} else {
					fprintf(stderr, "%s is unknown\n", argv[optind - 1]);
				}
				
			default:
				printf("-%c is not an option.\n", optarg);
				printHelp();
		}
	}
		
	// Logfile name and execl binaries path
	const char *PATH = "./user";
	char *fileName = "deadlockLog.out";
	
	// Attach shared memory (clock) and Process Control Blocks
	if((shmid = shmget(key, sizeof(struct SharedMemory *) * 2, IPC_CREAT | 0666)) < 0) {
		perror("shmget");
		fprintf(stderr, "shmget() returned an error! Program terminating...\n");
		exit(EXIT_FAILURE);
	}
	
    if((shm = (struct SharedMemory *)shmat(shmid, NULL, 0)) == (struct SharedMemory *) -1) {
		perror("shmat");
        fprintf(stderr, "shmat() returned an error! Program terminating...\n");
        exit(EXIT_FAILURE); 
    }
	shm->totalProcs = 0; 
	shm->turnaroundTimeSec = 0; 
	shm->turnaroundTimeNansec = 0;  
	shm->waitingTimeSec = 0; 
	shm->waitingTimeNansec = 0;  
	shm->cpuUtilizationSec = 0;  
	shm->cpuUtilizationNansec = 0; 
	
	if(verbose) {
		printf("Printing verbose logfiles\n");
		shm->verb = 1;
	} else {
		shm->verb = 0;
	}
	
	fp = fopen(fileName, "w");
	if(fp == NULL) {
		printf("Couldn't open file");
		errno = ENOENT;
		killAll();
		exit(EXIT_FAILURE);
	}
	
	// General variables
	int c, status,
		index = 0;
	pid_t pid, temp, wpid;
	
	int sharableResources = 15 + rand() % 11;
	for(c = 0; c < 20; c++) {
		if(rand() % 100 <= sharableResources) {
			shm->resources[c][0] = 100;
			shm->resources[c][1] = 100;
			shm->resources[c][2] = 0;
			shm->resources[c][3] = 0;
		} else {
			shm->resources[c][0] = 0;
			shm->resources[c][1] = 0;
			shm->resources[c][2] = 0;
			shm->resources[c][3] = 0;
		}
	}
	
	my_sem = sem_open("/mySem", O_CREAT, 0666, 1);
	if(my_sem == SEM_FAILED) {
		perror("OSS sem_open");
		exit(EXIT_FAILURE);
	}
	
	// Message passing variables
    int msgflg = IPC_CREAT | 0666;
	struct msg_buf msgSend;
	
	// Attach message queues
	if ((msgid = msgget(msg_key, msgflg)) < 0) {
		perror("Parent msgget");
		killAll();
		exit(EXIT_FAILURE);
	}

	// Start Clock
	pid = fork();
	if(pid == 0) {
		execl("./clock", NULL);
	} else if(pid < 0) {
		printf("There was an error creating the clock\n");
		signalHandler();
	}
	
	int spawnNewProcess = 0;
	int completed, waitUntil, result;
	int lastTime = 0;
	
	// Begin OSS simulation and stop if it writes 10,000 lines to the log
	while(shm->timePassedSec < terminationTime) {
		sleep(1);
		shm->cpuUtilizationSec += 1;
		if(lastTime + 4 < shm->timePassedSec) {
			
			sem_wait(my_sem);
			int c;
			for(c = 0; c < 20; c++) {
				if(shm->resources[c][2] > 0 && shm->resources[c][1] == 0 && shm->resources[c][3] == 1) {
					printf("DEADLOCK DETECTED: Resource %i is deadlocked... terminating pid #%i\n", c, shm->resources[c][2]);
					fprintf(fp, "DEADLOCK DETECTED: Resource %i is deadlocked... terminating pid #%i\n", c, shm->resources[c][2]);
					msgSend.mtype = shm->resources[c][2];
					sprintf(msgSend.mtext, "terminate");
					
					if(msgsnd(msgid, &msgSend, MSGSZ, IPC_NOWAIT) < 0) {
						perror("msgsnd");
						printf("The reply to child did not send\n");
						signalHandler();
					}
					
					shm->resources[c][2] = 0;
					shm->resources[c][3] = 0;
				}
			}
			sem_post(my_sem);
		
			int lastTime = shm->timePassedSec;
		}
		// Determine time between child spawns
		if(!spawnNewProcess) {
			waitUntil = (rand() % 2);
			
			waitUntil += shm->timePassedSec;
			spawnNewProcess = 1;
		}
		
		//updateTime();
		// Spawn child if allowed
		if((index < 18) && (spawnNewProcess == 1) && (shm->timePassedSec >= waitUntil)) {
			index++;
			spawnNewProcess = 0;
			totalProcesses++;
			
			pid = fork();
			if (pid == 0) {			
				// Spawn slave process
				//https://linux.die.net/man/3/execl
				//execl(PATH, argv[0], argv[1], argv[2], argv[3])
				execl(PATH, fileName, NULL);

				//If child program exec fails, _exit()
				_exit(EXIT_FAILURE);
			} else if(pid < 0) {
				printf("Error forking\n");
			}
		}
		
		// If a child finished, clear the PCB and reset for a new child to spawn
		if ((wpid = waitpid(-1, &status, WNOHANG)) > 0 ) {
			index--;
			completed++;
		}
	}

	printf("Finishing Normally... Terminating all the child processes\n");
	killpg(getpgrp(), SIGINT);
	
	sleep(1);
	return 0;
}

void killAll() {
	fprintf(fp, "Average Turnaround Time: %i.%u seconds\n", (shm->turnaroundTimeSec / shm->totalProcs), shm->turnaroundTimeNansec / 1E9);
	fprintf(fp, "Total CPU Utilization: %i.%u seconds\n", (int)shm->cpuUtilizationSec/100, shm->cpuUtilizationNansec);
	fprintf(fp, "Total Time Spent Waiting: %i.%u seconds\n", (int)shm->waitingTimeSec/100, shm->waitingTimeNansec);
	fprintf(fp, "Total Throughput: %i processes\n", shm->totalProcs);
	fprintf(fp, "------------------------------------------------------------------\n");
	sleep(3);
	sem_unlink("/mySem");
    sem_close(my_sem);
	shmdt(shm);
	shmctl(shmid, IPC_RMID, NULL);
	msgctl(msgid, IPC_RMID, NULL);
	
	fclose(fp);
}

void printHelp() {
	printf("\nCS4760 Project 5 Help!\n");
	printf("-h flag prints this help message\n");
	printf("-v turns on verbose logging\n");
	printf("-r [int] changes how long the program runs before termination\n");
	printf("-t [int] changes chance that children proicess terminate themselves randomly\n\n");
}

void signalHandler() {
	killAll();
	
    pid_t id = getpgrp();
    killpg(id, SIGINT);
	printf("Signal received... terminating master\n");
	
	sleep(1);
    exit(EXIT_SUCCESS);
}