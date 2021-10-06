#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "proc-common.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 2                /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */


/////////////////////
//struct definition//
/////////////////////

typedef struct node{
    
    pid_t pid ;
    int id, priority;
    char procname [TASK_NAME_SZ];
    struct node *previous;
    struct node *next;
}queue;

//global variables
queue *baton;
int remain_proc, num_high = 0, max_proc;
pid_t pid_shell;
const int PRIORITY_LOW = 0;
const int PRIORITY_HIGH = 1;
//global end

queue *insert_to_queue(pid_t pid, int id, int priority, queue *last, char *execname) {
    
    if (last->next == NULL) {
       	//insertion of the first process : create self-loop cycle
        last->previous = last;
        last->next = last ;   
    }
    //queue has at least one process already inserted!
    else {
	queue *new = (queue *)malloc(sizeof(queue));
        if (!new) {
        	perror("queue : no memory!");
        	exit(1);
		}
    	queue *temp = last->previous;
        //connect new node with the last double-connected
        //         __________________________
        // [new]--'->[last]<->....<->[head]<-'
        new->next = last;
        // connect the last double-connected with the new node
        //         __________________________
        // [new]<-'->[last]<-->...<->[head]<-'
        last->previous = new;
        // connect new node with head i.e. close the cycle
        //  ___________________________________
        // '->[new]<-->[last]<-->...<->[head]<-'
        new->previous = temp;
        temp->next = new;
        // connection done ! make new node the last connected
        last = new;
    }
    // write process p(id) [same for both cases]
    last->pid = pid;
    last->id = id ;
    strcpy(last->procname, execname);
    return last; 
}

queue *remove_from_queue(queue *pointer) {
//    if (pointer == pointer->next) {
// 
//    }
    queue *temp = pointer->previous;
    (pointer->previous)->next = pointer->next;
    (pointer->next)->previous = pointer->previous;
    pointer->next = NULL;
    pointer->previous = NULL;
    if (pointer->priority == PRIORITY_HIGH)
    	num_high -= 1;
    free(pointer);
    remain_proc -= 1;
    return temp;
}

queue *find_pid_queue(queue *pointer, int pid) {
	queue *temp = pointer; 
	while (temp->pid != pid) {
		temp = temp->next;
	}
    return temp;
}

void print_queue(queue *head, int num){
    int i = 0;
    queue *temp = head;
    for (i = 0; i < num; i++) {
        printf("PID:%d\tID:%d\n", temp->pid, temp->id);
        temp = temp->next ;
    }
}

/*Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void)
{
	printf("\t [Scheduler]: The following processes are currently running:\n");
	print_queue(baton, remain_proc + 1);
	printf("\t The ongoin process has process id: %d",baton->id);

//	assert(0 && "Please fill me!");
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int
sched_kill_task_by_id(int id)
{
	queue *temp = baton;
	int id2 = temp->id;
	while (temp->id != id) {
		temp = temp->next;
		if (temp->id == id2)
			 break;
	}
	if (temp->id == id) {
    		printf("\t Killing process with id %i.\n", id);
	    	kill(temp->id, SIGKILL);
		remove_from_queue(temp);
	}
	return 0;

//ssert(0 && "Please fill me!");
//eturn -ENOSYS;
}

static void
sched_high_priority(int id)
{
	if (id != 0){
		queue *temp = baton;
		int id2 = temp->id;
		while (temp->id != id) {
	       		temp = temp->next;
			if (temp->id == id2)
				break;
		}
		if ((temp->id == id) && (temp->priority == PRIORITY_LOW)) {
			printf("\t UPgrading the priority of process with id %i.\n", id);
			num_high += 1;
			temp->priority = PRIORITY_HIGH;        	

		}
	}
}

static void
sched_low_priority(int id)
{
	queue *temp = baton;
	int id2 = temp->id;
    	while (temp->id != id) {
       		temp = temp->next;
		if (temp->id == id2)
			break;
    	}
	if ((temp->id == id) && (temp->priority == PRIORITY_HIGH)) {
        	printf("\t  DOWNgrading the priority of process with id %i.\n", id);
		num_high -= 1;
	        temp->priority = PRIORITY_LOW;        	
    	}
}


/* Create a new task.  */
static void
sched_create_task(char *executable)
{
	char *newargv[] = {executable, NULL, NULL, NULL};
	char *newenviron[] = {NULL};
	pid_t pid;
	pid = fork();
	if (pid < 0) {
		perror("Create_new_task: fork could not be done!");
		exit(1);
	}
	if (pid == 0) {
		/* Child */
		raise(SIGSTOP);
		execve(executable, newargv, newenviron);
		exit(1);
	}
	remain_proc += 1;
	max_proc += 1;
	insert_to_queue(pid, max_proc, 0, baton, executable);

//	assert(0 && "Please fill me!");
}

/* Process requests by the shell.  */
static int
process_request(struct request_struct *rq)
{
	switch (rq->request_no) {
		case REQ_PRINT_TASKS:
			sched_print_tasks();
			return 0;

		case REQ_KILL_TASK:
			printf("Killing in the name of...\n");
			return sched_kill_task_by_id(rq->task_arg);

		case REQ_EXEC_TASK:
			sched_create_task(rq->exec_task_arg);
			return 0;

		case REQ_HIGH_TASK:
	        	sched_high_priority(rq->task_arg);
		        return 0;

        	case REQ_LOW_TASK:
		        sched_low_priority(rq->task_arg);
	        	return 0;

		default:
			return -ENOSYS;
	}
}

/* 
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum)
{
    if (signum != SIGALRM) {
		fprintf(stderr, "Internal error: Called for signum %d, not SIGALRM\n", signum);
		exit(1);
	}
    if (baton) {
        kill(baton->pid, SIGSTOP);
        printf("\n [Alarm]: Time is Up!Quantum has elapsed!\n\n");
	alarm(SCHED_TQ_SEC);
    }	

//	assert(0 && "Please fill me!");
}

void update(int msg){
//    remain_proc -= 1;
    queue *temp2;
    baton = remove_from_queue(baton);
    switch (msg) {
	    case 0 :
			printf("successfully terminated within time quantum !\n");
			break;
	    case 1 : 
			printf("and has been removed from RR-Queue!\n");
			break;
	    default:
			printf(" unreachable switch case option\n");
			exit(1);
    }
    if (remain_proc >= 0) {
	temp2 = baton;
  	while((temp2->id !=0) && (temp2->priority != PRIORITY_HIGH) && (num_high != 0)){
   		temp2 = temp2->next;
  	}
        kill(baton->pid, SIGCONT);
        alarm(SCHED_TQ_SEC);
        printf("[Scheduler]: Advancing to next process...\n");
    }
    else {
        printf("[Scheduler]: All tasks finished. Exiting...\n");
        exit(1);
    }
}


/* 
 * SIGCHLD handler
 */
static void
sigchld_handler(int signum)
{
	pid_t p;
	int status;
	queue *temp, *temp2; 
	if (signum != SIGCHLD) {
		fprintf(stderr, "Internal error: Called for signum %d, not SIGCHLD\n", signum);
		exit(1);
	}
    	p = waitpid(-1, &status, WUNTRACED | WCONTINUED);
    	if (p < 0) {
        	perror("sigchld handler: waitpid");
	        exit(1);
	}
	temp = find_pid_queue(baton, p);
	printf(" [Scheduler]: Process PNAME: %s  PID: %d  ID: %d ", temp->procname,temp->pid,temp->id);
	if (WIFCONTINUED(status)) {
		printf("continues!\n\n");
		return; 
	}
	if (WIFEXITED(status)) {
        	/*  Current process has finished within time */
	        update(0);
    	}
    	else if (WIFSIGNALED(status)) { 
		if (WTERMSIG(status) == 9) {
                	printf("was killed ");
	                if (baton->pid == p){
        			update(1);
               		}
            		else {
       //         	remain_proc -= 1;
			temp = remove_from_queue(temp);
			printf("and has been removed from RR-Queue!\n");
            		}
        	}
	}	
	else if (WIFSTOPPED(status)) {
        	/* Current process needs more time */
		temp2 = baton;
  		temp2 = temp2->next;
  		while((temp2->id !=0) && (temp2->priority != PRIORITY_HIGH) && (num_high != 0)){
   			temp2 = temp2->next;
  		}
  		baton = temp2;
	        kill(baton->pid, SIGCONT);
	        alarm(SCHED_TQ_SEC);
	        printf("was stopped ! Advancing to next process...\n");
	}


//	assert(0 && "Please fill me!");
}

/* Disable delivery of SIGALRM and SIGCHLD. */
static void
signals_disable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		perror("signals_disable: sigprocmask");
		exit(1);
	}
}

/* Enable delivery of SIGALRM and SIGCHLD.  */
static void
signals_enable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
		perror("signals_enable: sigprocmask");
		exit(1);
	}
}


/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void
install_signal_handlers(void)
{
	sigset_t sigset;
	struct sigaction sa;

	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigaddset(&sigset, SIGALRM);
	sa.sa_mask = sigset;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction: sigchld");
		exit(1);
	}

	sa.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction: sigalrm");
		exit(1);
	}

	/*
	 * Ignore SIGPIPE, so that write()s to pipes
	 * with no reader do not result in us being killed,
	 * and write() returns EPIPE instead.
	 */
	if (signal(SIGPIPE, SIG_IGN) < 0) {
		perror("signal: sigpipe");
		exit(1);
	}
}

static void
do_shell(char *executable, int wfd, int rfd)
{
	char arg1[10], arg2[10];
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = {NULL};

	sprintf(arg1, "%05d", wfd);
	sprintf(arg2, "%05d", rfd);
	newargv[1] = arg1;
	newargv[2] = arg2;

	raise(SIGSTOP);
	execve(executable, newargv, newenviron);

	/* execve() only returns on error */
	perror("scheduler: child: execve");
	exit(1);
}

/* Create a new shell task.
 *
 * The shell gets special treatment:
 * two pipes are created for communication and passed
 * as command-line arguments to the executable.
 */
static pid_t
sched_create_shell(char *executable, int *request_fd, int *return_fd)
{
	pid_t p;
	int pfds_rq[2], pfds_ret[2];

	if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
		perror("pipe");
		exit(1);
	}

	p = fork();
	if (p < 0) {
		perror("scheduler: fork");
		exit(1);
	}

	if (p == 0) {
		/* Child */
		//pid_shell = getpid();

		close(pfds_rq[0]);
		close(pfds_ret[1]);
		do_shell(executable, pfds_rq[1], pfds_ret[0]);
		assert(0);
	}
	pid_shell = getpid();
	/* Parent */
	close(pfds_rq[1]);
	close(pfds_ret[0]);
	*request_fd = pfds_rq[0];
	*return_fd = pfds_ret[1];

	return p;
}

static void
shell_request_loop(int request_fd, int return_fd)
{
	int ret;
	struct request_struct rq;

	/*
	 * Keep receiving requests from the shell.
	 */
	for (;;) {
		if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
			perror("scheduler: read from shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}

		signals_disable();
		ret = process_request(&rq);
		signals_enable();

		if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
			perror("scheduler: write to shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}
	}
}

void create_execve(char *exec){
	char *newargv[] = {exec, NULL, NULL, NULL };
	char *newenviron[] = {NULL};
	raise(SIGSTOP);
	execve(exec, newargv, newenviron);
	/* execve() only returns on error */
	perror("execve");
	exit(1);
}


int main(int argc, char *argv[])
{
	int nproc;

	int i;
	pid_t p;
	queue *head = (queue *)malloc(sizeof(queue));
	baton = head; 
	head->next = NULL;
	head->previous = NULL;
	/* Two file descriptors for communication with the shell */
	static int request_fd, return_fd;

	/* Create the shell. */
	p = sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);
	/* TODO: add the shell to the scheduler's tasks */
	baton = insert_to_queue(p, 0, PRIORITY_LOW, baton, "shell");
	printf(" [Sheduler]: Inserted to Queue PNAME: shell\t PID: %d\t ID: 0\n", p);

	/*
	 * For each of argv[1] to argv[argc - 1],
	 * create a new child process, add it to the process list.
	 */

//apomama	nproc = 0; /* number of proccesses goes here */
	nproc = argc - 1;
	remain_proc = nproc;
	max_proc = nproc;

	/* Wait for all children to raise SIGSTOP before exec()ing. */
	for (i = 0; i < nproc ; i++) {
        	if ((p = fork()) < 0) {
	            	perror("[Scheduler]: fork could not be done");
			exit(1);
        	}
        	if (p == 0) {
            		/* Child Code : Process */
            		create_execve(argv[i+1]);
			fprintf(stderr,"Error ! unreachable point in child's code\n");
			exit(0);
		}
	        /* Parent's Code : Scheduler */
		baton = insert_to_queue(p, i + 1, PRIORITY_LOW, baton, argv[i+1]);
		printf(" [Sheduler]: Inserted to Queue PNAME: %s\t PID: %d\t ID: %d\n", baton->procname, baton->pid, baton->id);
	}

	wait_for_ready_children(nproc);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

	baton = head;
	kill(baton->pid, SIGCONT);
	alarm(SCHED_TQ_SEC);

	if (nproc < 0) {
		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
		exit(1);
	}

	shell_request_loop(request_fd, return_fd);

	/* Now that the shell is gone, just loop forever
	 * until we exit from inside a signal handler.
	 */
	while (pause())
		;

	/* Unreachable */
	fprintf(stderr, "Internal error: Reached unreachable point\n");
	return 1;
}
