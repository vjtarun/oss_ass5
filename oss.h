#ifndef PROJECT5_H
#define PROJECT5_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>  
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_USER_PROCESSES 18
#define MSGSZ 128

struct msg_buf {
	long mtype;
	char mtext[MSGSZ];
};

struct SharedMemory {
	int timePassedSec;
	
	int resources[20][4];
	int pendingClaims[20];
	
	pid_t pid;
	
	struct timespec timeStart, timeNow, timePassed;
	
	unsigned long int timePassedNansec;
	int verb;
	int totalProcs;
	int waitingTimeSec, cpuUtilizationSec, turnaroundTimeSec;
	long long unsigned turnaroundTimeNansec;
	double waitingTimeNansec, cpuUtilizationNansec;
};

static int verbose;

sem_t *my_sem;

key_t key = 5782000;
key_t rcbKey = 4031609;
key_t statKey = 2771116;
key_t cidKey = 2239686;
key_t msg_key = 3868970;

long getRightTime();

void clearPCB(int);
void deadlockDetect();
void killAll();
void printHelp();
void signalHandler();
void updateTime();

#endif