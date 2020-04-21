#include "oss.h"

FILE *fp;

static int shmid, statsid, rcbid, cid, msgid;

static struct SharedMemory *shm;
static struct statistics *stats;

static int myClaims[20] = {0};

static int (*pendingClaims)[20];

static int creationTimeSec;

int main(int argc, char* argv[]) {
	// Signal Handler
	signal(SIGINT, signalHandler);
	signal(SIGSEGV, signalHandler);

	fp = fopen(argv[0], "a");
	if(fp == NULL) {
		printf("Couldn't open file");
		errno = ENOENT;
		killAll();
		exit(EXIT_FAILURE);
	}
	
	// Seed the random number generator
	srand((unsigned)(getpid() ^ time(NULL) ^ ((getpid()) >> MAX_USER_PROCESSES)));
	
	// Get shared memory id 
	if((shmid = shmget(key, sizeof(struct SharedMemory *) * 2, 0666)) < 0) {
		perror("shmget");
		fprintf(stderr, "Child: shmget() $ returned an error! Program terminating...\n");
		killAll();
		exit(EXIT_FAILURE);
	}
	
	// Attach the shared memory
    if ((shm = (struct SharedMemory *)shmat(shmid, NULL, 0)) == (struct SharedMemory *) -1) {
		perror("shmat");
        fprintf(stderr, "shmat() returned an error! Program terminating...\n");
		killAll();
        exit(EXIT_FAILURE);
    }
	
	my_sem = sem_open("/mySem", 0);
	if(my_sem == SEM_FAILED) {
		perror("Child sem_open");
		exit(EXIT_FAILURE);
	}
	

	// Attach message queues
	if ((msgid = msgget(msg_key, 0666)) < 0) {
		perror("Parent msgget");
		killAll();
		exit(EXIT_FAILURE);
	}
	
	verbose = shm->verb;
	
	struct msg_buf msgRec;
	
	creationTimeSec = shm->timePassedSec;
	int terminationTime = rand() % 5;
	terminationTime += creationTimeSec;
	int terminationChance = rand() % 50 + 50;
	
	int claimStatus = 0;
	int toClaimAmount = 0;
	int claimedAmount = 0;
	int fromWhichResource = 0;
	int continueLoop = 1;
	int atleastOneClaim = 0;
	int goneThrough = 0;
	
	int cpuUtilizedSec;
	double cpuUtilizedNan;
	int waitTimeSec;
	double waitTimeNansec;
	
	while(continueLoop && (msgrcv(msgid, &msgRec, MSGSZ, getpid(), IPC_NOWAIT) < 0)) {
		sleep(2);
		if(claimStatus) {
			sem_wait(my_sem);
			
			if(toClaimAmount <= shm->resources[fromWhichResource][1]) {
				claimStatus = 0;
				if(goneThrough > 0) {
					shm->resources[fromWhichResource][3] = 0;
					shm->resources[fromWhichResource][2] = 0;
				}
				goneThrough = 0;
				shm->resources[fromWhichResource][1] -= toClaimAmount;
				
				if(verbose) {
					printf("PROCESS: %i has claimed %i from resource %i which now has %i left\n", getpid(), toClaimAmount, fromWhichResource, shm->resources[fromWhichResource][1]);
					fprintf(fp, "PROCESS: %i has claimed %i from resource %i which now has %i left\n", getpid(), toClaimAmount, fromWhichResource, shm->resources[fromWhichResource][1]);
				}
				
				cpuUtilizedSec = (int)shm->timePassedSec;
				cpuUtilizedNan = (double)shm->timePassedNansec;
				shm->waitingTimeSec = shm->waitingTimeSec + ((double)shm->timePassedSec - waitTimeSec);
				shm->waitingTimeNansec += (waitTimeNansec / 1E9);
				
				waitTimeSec = 0;
				waitTimeNansec = 0;

				claimedAmount += toClaimAmount;
				myClaims[fromWhichResource] += toClaimAmount;
				shm->resources[fromWhichResource][2] = 0;
			} else if (toClaimAmount > shm->resources[fromWhichResource][1] && shm->resources[fromWhichResource][1] > 0) {
				if(shm->resources[fromWhichResource][1] > 0) {
					if(verbose) {
						printf("PROCESS: %i wanted %i but was able to only claim %i from resource %i which now has 0 left\n", getpid(), toClaimAmount, shm->resources[fromWhichResource][1], fromWhichResource);
						fprintf(fp, "PROCESS: %i wanted %i but was able to only claim %i from resource %i which now has 0 left\n", getpid(), toClaimAmount, shm->resources[fromWhichResource][1], fromWhichResource);
					}
					
					claimStatus = 1;
					toClaimAmount -= shm->resources[fromWhichResource][1];
					claimedAmount += shm->resources[fromWhichResource][1];
					myClaims[fromWhichResource] += shm->resources[fromWhichResource][1];
					shm->resources[fromWhichResource][1] = 0;
					shm->resources[fromWhichResource][2] = 0;
				} else {
					//printf("%i is trying to get resources from %i but it has none left\n", getpid(), fromWhichResource);
				}
			} else {
				goneThrough++;
				if(verbose) {
					printf("PROCESS: %i wanted %i from resource %i but it has none left\n", getpid(), toClaimAmount, fromWhichResource);
					fprintf(fp, "PROCESS: %i wanted %i from resource %i but it has none left\n", getpid(), toClaimAmount, fromWhichResource);
				}
				
				if(goneThrough > 1) {
					shm->resources[fromWhichResource][3] = 1;
					shm->resources[fromWhichResource][2] = (int)getpid();
				}
			}
			sem_post(my_sem);
		} else {
			int num = rand() % 3;
			if(num) {
				sem_wait(my_sem);

				do {
					fromWhichResource = rand() % 20;
				} while(shm->resources[fromWhichResource][0] != 100);
				
				toClaimAmount = rand() % 100 + 1;
				shm->resources[fromWhichResource][2] = (int)getpid();

				claimStatus = 1;
				atleastOneClaim = 1;
				sem_post(my_sem);
			} else {
				sem_wait(my_sem);
				
				shm->cpuUtilizationSec = shm->cpuUtilizationSec + (shm->timePassedSec - cpuUtilizedSec);
				shm->cpuUtilizationNansec += (cpuUtilizedNan / 1E9);
				cpuUtilizedSec = 0;
				cpuUtilizedNan = 0;
				
				int c;
				for(c = 0; c < 20; c++) {
					if(myClaims[c] > 0) {
						if(verbose) {
							printf("PROCESS: %i has released %i back to %i which now has %i\n", getpid(), myClaims[c], c, shm->resources[c][1]);
							fprintf(fp, "PROCESS: %i has released %i back to %i which now has %i\n", getpid(), myClaims[c], c, shm->resources[c][1]);
						}
						shm->resources[c][1] += myClaims[c];
						claimedAmount -= myClaims[c];
						myClaims[c] = 0;
					}
				}
				
				sem_post(my_sem);
			}
			
			if(((terminationTime + creationTimeSec) <= shm->timePassedSec) && !claimStatus && atleastOneClaim) {
				if((rand() % 100 + 1) <= terminationChance) {		
					continueLoop = 0;
				} else {
					terminationTime = shm->timePassedSec + (rand() % 4 + 1);
				}
			}	
		}
	}
	
	if(verbose) {
		printf("PROCESS: %i terminating at %i.%u--\n", getpid(), shm->timePassedSec, shm->timePassedNansec);
		fprintf(fp, "PROCESS: %i terminating at %i.%u--\n", getpid(), shm->timePassedSec, shm->timePassedNansec);
	}
	int c;
	for(c = 0; c < 20; c++) {
		if(myClaims[c] > 0) {
			shm->resources[c][1] += myClaims[c];
			if(verbose) {
				printf("PROCESS: %i has released %i back to %i which now has %i--\n", getpid(), myClaims[c], c, shm->resources[c][1]);
				fprintf(fp, "PROCESS: %i has released %i back to %i which now has %i--\n", getpid(), myClaims[c], c, shm->resources[c][1]);
			}
			
			myClaims[c] = 0;
		}	
	}
	
	shm->turnaroundTimeSec += shm->timePassedSec - creationTimeSec;	
	shm->totalProcs += 1;
	
	killAll();
	exit(3);
	
}

// Kills all when signal is received
void signalHandler() {
    pid_t id = getpid();
	printf("Signal received... terminating child process %i\n", id);
	shm->turnaroundTimeSec += shm->timePassedSec - creationTimeSec;	
	
	sem_wait(my_sem);
	int c;
	for(c = 0; c < 20; c++) {
		if(myClaims[c] > 0) {
			if(verbose) {
				printf("PROCESS: %i has released %i back to %i which now has %i--\n", getpid(), myClaims[c], c, shm->resources[c][1]);
				fprintf(fp, "PROCESS: %i has released %i back to %i which now has %i--\n", getpid(), myClaims[c], c, shm->resources[c][1]);
			}
			
			shm->resources[c][1] += myClaims[c];
			myClaims[c] = 0;
		}
	}
	sem_post(my_sem);
	
	killAll();
    killpg(id, SIGINT);
    exit(EXIT_SUCCESS);
}

// Release shared memory
void killAll() {
	shmdt(shm);
	shmdt(shm);
	fclose(fp);
}