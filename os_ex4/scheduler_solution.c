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


/////////////////////
//struct definition//
/////////////////////

typedef struct node {
	pid_t pid;
	int id;
	char procname[TASK_NAME_SZ];
	struct node *previous;
	struct node *next;
}queue;

//global variables
queue *baton;
int remain_proc;

queue *insert_to_queue(pid_t pid, int id, queue *last, char *execname) {
	if (last->next == NULL) {
	        last->previous = last;
	        last->next = last;
         }
	else{
		queue *new = (queue*)malloc(sizeof(queue));
       		 if (!new){
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
	last->id = id;
	strcpy(last->procname, execname);
	return last; 
}

queue *remove_from_queue(queue *pointer) {
    if (pointer == pointer->next) {
        
    }
    queue *temp = pointer->previous;
    (pointer->previous)->next = pointer->next;
    (pointer->next)->previous = pointer->previous;
    pointer->next = NULL;
    pointer->previous = NULL;
    free(pointer);
    return temp;
}

queue *find_pid_queue(queue *pointer ,int pid) {
    queue *temp = pointer; 
    while (temp->pid != pid) {
	temp = temp->next;
    }
    return temp;
}

/*
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum)
{
	if (signum != SIGALRM) {
		fprintf(stderr, "Internal error: Called for signum %d, not SIGALRM\n",
                signum);
		exit(1);
	}
	if (baton) {
        	kill(baton->pid, SIGSTOP);
		printf("\n");
		printf("\t [Alarm]: Time is Up!Quantum has elapsed!\n\n");
	}

//apomama	assert(0 && "Please fill me!");
}

void update(int msg) {
    remain_proc -= 1;
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
    if (remain_proc) {
        kill(baton->pid, SIGCONT);
        alarm(SCHED_TQ_SEC);
        printf("\t [Scheduler]: Advancing to next process...\n");
    }
    else {
        printf("\t [Scheduler]: All tasks finished. Exiting...\n");
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
    	queue *temp ; 
	if (signum != SIGCHLD) {
		fprintf(stderr, "Internal error: Called for signum %d, not SIGCHLD\n",
                signum);
		exit(1);
	}
	p = waitpid(-1, &status, WUNTRACED|WCONTINUED);
	if (p < 0) {
        	perror("sigchld handler: waitpid");
	        exit(1);
	}
	temp = find_pid_queue(baton,p);
	printf("\t [Scheduler]: Process PNAME: %s  PID: %d  ID: %d ", temp->procname, temp->pid, temp->id);
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
		        if (baton->pid == p) { 
        			update(1);
			}
            		else {
        	        	remain_proc -= 1;
				temp = remove_from_queue(temp);
				printf("and has been removed from RR-Queue!\n");
			}
		}
	}
	else if (WIFSTOPPED(status)) {
        	/* Current process needs more time */
	        baton = baton->previous;
	        kill(baton->pid, SIGCONT);
		alarm(SCHED_TQ_SEC);
		printf("was stopped ! Advancing to next process...\n");
	}

//apomama	assert(0 && "Please fill me!");
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


void create_execve(char *exec) {
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

	/*
	 * For each of argv[1] to argv[argc - 1],
	 * create a new child process, add it to the process list.
	 */

//=>apomama	nproc = 0; /* number of proccesses goes here */
	nproc = argc - 1;
	remain_proc = nproc; /*number of remaining proccesses goes here*/
	

	for (i = 0; i < nproc; i++) {
	        if ((p = fork()) < 0){
			perror("[Scheduler]: fork could not be done");
			exit(1);
		}
		if (p == 0) {
			/* Child Code : Process */
			create_execve(argv[i + 1]);
			fprintf(stderr,"Error ! unreachable point in child's code\n");
			exit(0);
        	}
	        /* Parent's Code : Scheduler */
		baton = insert_to_queue(p, i, baton, argv[i+1]);
		printf("\t [Sheduler]: Inserted to Queue PNAME: %s\t PID: %d\t ID: %d\n",baton->procname,baton->pid,baton->id);
	}	

	if (nproc == 0) {
		printf("\t [Scheduler]: No tasks. Exiting...\n");
		exit(1);
	}

	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

	baton = head;
	kill(baton->pid, SIGCONT);
	alarm(SCHED_TQ_SEC);

//	if (nproc == 0) {
//		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
//		exit(1);
//	}


	/* loop forever  until we exit from inside a signal handler. */
	while (pause())
		;

	/* Unreachable */
	fprintf(stderr, "Internal error: Reached unreachable point\n");
	return 1;
}
