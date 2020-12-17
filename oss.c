#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <errno.h>

#include "oss.h"

//maximum number of children
#define C_MAX 100

static int arg_c = 5, arg_t = 2;
//number of children started and exited
static int num_c = 0, num_e = 0;

//memory and queue descriptors
static int mfd = -1, qfd = -1;
static struct sh_memory *mem = NULL;

//Start a user process
static int start_user(){

  //if we have started all user processes
  if(num_c >= C_MAX){
    return 0;
  }

  //create a new process
	const pid_t pid = fork();
  if(pid < 0){
    perror("fork");
    return -1;
  }

  //child process
  if(pid == 0){

    execl("user", "user", NULL);
    perror("execl");
    exit(EXIT_FAILURE);

  //master
  }else{
    num_c++;
    printf("Master: Creating new child pid %d at my time %d.%d\n", pid, mem->seconds, mem->nanoseconds);
  }

	return pid;
}

//create the shared memory and queue
static int initialize(){

  //create a key
  key_t fkey = ftok(FTOK_PATH, MFKEY);
	if(fkey < 0){
		perror("ftok");
		return EXIT_FAILURE;
	}

  //get shared memory
	mfd = shmget(fkey, sizeof(struct sh_memory), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  if(mfd == -1){
  	perror("shmget");
  	return EXIT_FAILURE;
  }

	fkey = ftok(FTOK_PATH, QFKEY);
  if(fkey < 0){
		perror("ftok");
		return EXIT_FAILURE;
	}

	qfd = msgget(fkey, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  if(qfd == -1){
  	perror("msgget");
  	return EXIT_FAILURE;
  }

  //attach to the memory region
  mem = (struct sh_memory *) shmat(mfd, NULL, 0);
	if(mem == NULL){
		perror("shmat");
		return -1;
	}

  //clear it
  bzero(mem, sizeof(struct sh_memory));

	return 0;
}

static int opt_handler(const int argc, char * const argv[]){

  int opt, arg_l=0;
	while((opt = getopt(argc, argv, "c:l:t:h")) != -1){
		switch(opt){
      case 'c':
        arg_c	= atoi(optarg);
        break;

      case 'l':
        stdout = freopen(optarg, "w", stdout);
        arg_l = 1;
        break;

      case 't':
        arg_t	= atoi(optarg);
        break;
			default:
				printf("getopt: %c is invalid\n", opt);
        //drop to help
      case 'h':
          printf("Example: oss -c %d -l log.txt -t %d\n", arg_c, arg_t);
          printf("\t-c %d\tMax users\n", arg_c);
          printf("\t-l log.txt\t Log filename\n");
          printf("\t-t %d\t Max time\n", arg_t);
  				return -1;
		}
	}

  if(arg_l == 0)
    stdout = freopen("log.txt", "w", stdout);

  return 0;
}

static void sig_handler(int sig){
  //set number of started children to max, to stop starting new users
  //since we were interrupted
  num_c = C_MAX;
}

int main(const int argc, char * const argv[]){
  int i;
  struct msgbuf mb;
  int mtype = QANY;

  if( (initialize() < 0)||
      (opt_handler(argc, argv) < 0)){
    shmctl(mfd,    IPC_RMID, NULL);
    msgctl(qfd, 0, IPC_RMID);
    return EXIT_FAILURE;
  }

  signal(SIGINT,  SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGALRM, sig_handler);

  //set an alarm to run at most -t seconds
  alarm(arg_t);


  //start the initial batch of processes
  for(i=0; i < arg_c; i++){
    start_user();
  }

	while(num_e < C_MAX){

    pid_t wpid = 0; //pid of exited user

    //wait for a message
    if(msgrcv(qfd, (void*)&mb, MG_LEN, mtype, 0) == -1){
      if(errno != EINTR){
  		    perror("msgrcv");
      }
  		break;
  	}

    //decide what to do
    switch(mb.mtype){
      case QLOCK:
        //after LOCK, we wait for UNLOCK
        mtype = QUNLOCK;
        break;

      case QUNLOCK:
        //after UNLOCK, we wait for LOCK
        mtype = QLOCK;

        //if a process terminated
        if(mem->shmPID > 0){  //he saved its pid

          //save it, before we clear
          wpid = mem->shmPID;
          printf("Master: User %d exited at system time %d.%d\n", mem->shmPID, mem->seconds, mem->nanoseconds);
          mem->shmPID = 0;
          num_e++;

          //start another user
          if(start_user() < 0){
            num_c = C_MAX;
          }
        }
        break;

      default:
        fprintf(stderr, "Error: Invalid message from user\n");
        break;
    }


    //increase the system clock
  	mem->nanoseconds += 100;
  	if(mem->nanoseconds > MAX_NANOS){
  		mem->seconds++;
  		mem->nanoseconds %= MAX_NANOS;
  	}

    //check if our time has passed
    if(mem->seconds >= arg_t){
      printf("Master: Stopping after %d seconds\n", arg_t);
      num_c = C_MAX;
    }

  	mb.mtype = mb.pid;
    mb.pid   = getpid();
  	if(msgsnd(qfd, &mb, MG_LEN, 0) == -1){
  		perror("msgsnd");
  		break;
  	}

    if(wpid > 0){
      waitpid(wpid, NULL, 0);
    }
	}

  shmctl(mfd, IPC_RMID, NULL);
  msgctl(qfd, 0, IPC_RMID);
  shmdt(mem);

  return EXIT_SUCCESS;
}
