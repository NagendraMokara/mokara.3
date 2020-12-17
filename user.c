#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>

#include "oss.h"

//Send a message
static void msg(const int qfd, int type){
	struct msgbuf mb;

	mb.mtype = type;	//set message type
	mb.pid = getpid();	//set sender, to master can return it to us
	if(	(msgsnd(qfd, &mb, MG_LEN, 0) == -1) ||
			(msgrcv(qfd, (void*)&mb, MG_LEN, mb.pid, 0) == -1) ){
		perror("msg");
		exit(EXIT_FAILURE);
	}
}

int main(const int argc, char * const argv[]){
	int mfd, qfd;
	unsigned int seconds_end=0, nanoseconds_end=0, nanoseconds_runtime=0;
	struct sh_memory *mem = NULL;

	srand(getpid());

	key_t fkey = ftok(FTOK_PATH, MFKEY);
	if(fkey < 0){
		perror("ftok");
		return EXIT_FAILURE;
	}

	mfd = shmget(fkey, 0, 0);
  if(mfd == -1){
  	perror("semget");
  	return EXIT_FAILURE;
  }

	fkey = ftok(FTOK_PATH, QFKEY);
	qfd = msgget(fkey, 0);
  if(qfd == -1){
  	perror("semget");
  	return EXIT_FAILURE;
  }

	mem = (struct sh_memory *) shmat(mfd, NULL, 0);
	if(mem == NULL){
		perror("shmat");
		return EXIT_FAILURE;
	}

	//determine how much we will run
	nanoseconds_runtime = 1 + rand() % (1000000 - 1);

	//copy system clock, and set our end time
	msg(qfd, QLOCK);
	seconds_end = mem->seconds;
	nanoseconds_end = mem->nanoseconds + nanoseconds_runtime;
	if(nanoseconds_end > MAX_NANOS){
		seconds_end++;
		nanoseconds_end %= MAX_NANOS;
	}
	msg(qfd, QUNLOCK);


	while(1){

		msg(qfd, QLOCK);

		if( (mem->seconds >= seconds_end) ||
				( (mem->seconds >= seconds_end) && (mem->nanoseconds >= nanoseconds_end)) ){

			if(mem->shmPID == 0){
				mem->shmPID = getpid();
				msg(qfd, QUNLOCK);
				break;
			}
		}
		msg(qfd, QUNLOCK);
	}
	shmdt(mem);

	return EXIT_SUCCESS;
}
