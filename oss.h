//Used in ftok functions
#define FTOK_PATH "/"
#define QFKEY 356423
#define MFKEY 141343

//used in nanoseconds
#define MAX_NANOS 1000000000

//shared memory structure
struct sh_memory {
	int seconds, nanoseconds;
	int shmPID;
};


//type of messages in our system
enum q_mtype { QANY=0, QLOCK, QUNLOCK };

struct msgbuf {
	long mtype;
	pid_t pid;
};
#define MG_LEN sizeof(pid_t)
