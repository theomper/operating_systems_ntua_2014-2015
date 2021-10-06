#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "proc-common.h"

#define SLEEP_PROC_SEC  10
#define SLEEP_TREE_SEC  3
//#define SLEEP_20_SEC  20

/*
 * Create this process tree:
 * A-+-B---D
 *   `-C
 */
/*
void b_fork(void)
{
	pid_t b, d, p;
	int status;
	change_pname("B");
		
		fprintf(stderr, "Parent, PID = %ld: Creating child...\n", (long)getpid());
		d = fork();
		if (d < 0) {
			// fork failed //
			perror("fork");
			exit(1);
		}
		if (d == 0) {
			// In child process //
			change_pname("D");
			printf("D: Sleeping...\n");
			sleep(SLEEP_20_SEC);
			printf("D: Exiting...\n");
			exit(13);
		}
		
		change_pname("B");
		//
		// In parent process. Wait for the child to terminate
		// and report its termination status.
		  //
		printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)b);
		p = wait(&status);
		explain_wait_status(p, status);	
		printf("B: Exiting...\n");
}*/
	

void fork_procs(void)
{
	/*
	 * initial process is A.
	 */

	pid_t b, c, d;
	int status;	

	change_pname("A");
	printf("A: Created.\n");
	
	
	fprintf(stderr, "Parent, PID = %ld: Creating child...\n", (long)getpid());
	b = fork();
	if (b < 0) {
		/* fork failed */
		perror("fork");
		exit(1);
	}
	if (b == 0) {
		/* In child process */
		
		change_pname("B");
		printf("B: Created.\n");
		fprintf(stderr, "Parent, PID = %ld: Creating child...\n", (long)getpid());	
		//exit(19);
		d = fork();
		if (d < 0){
			perror("fork");
			exit(1);
		}
		if (d == 0){
			/*In child D process */
			change_pname("D");
			printf("D: Created!\n");
			printf("D: Sleeping...\n");
			sleep(SLEEP_PROC_SEC);
			printf("D: Exiting...\n");
			exit(13);
		}
		change_pname("B");
		printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)d);
		printf("B: Waiting...\n");
		d = wait(&status);
		explain_wait_status(d, status);
		printf("B: Exiting...\n");
		exit(19);
	}
	
	
	//change_pname("A");
	/*
	 * In parent process. Wait for the child to terminate
	 * and report its termination status.
	 */
	//printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)p);
	//b = wait(&status);
	//explain_wait_status(p, status);
	
	fprintf(stderr, "Parent, PID = %ld: Creating child...\n", (long)getpid());
	c = fork();
	if (c < 0) {
		/* fork failed */
		perror("fork");
		exit(1);
	}
	if (c == 0) {
		/* In child process */
		change_pname("C");
		printf("C: Created.\n");
		printf("C: Sleeping...\n");
		sleep(SLEEP_PROC_SEC);
		printf("C: Exiting...\n");
		exit(17);
	}
	
	
	change_pname("A");
	/*
	 * In parent process. Wait for the child to terminate
	 * and report its termination status.
	 */
	printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)b);
	printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)c);
	printf("A: Waiting...\n");
	b = wait(&status);
	explain_wait_status(b, status);
	c = wait(&status);
	explain_wait_status(c, status);
	
	printf("A: Exiting...\n");
	exit(16);
}

/*
 * The initial process forks the root of the process tree,
 * waits for the process tree to be completely created,
 * then takes a photo of it using show_pstree().
 *
 * How to wait for the process tree to be ready?
 * In ask2-{fork, tree}:
 *      wait for a few seconds, hope for the best.
 * In ask2-signals:
 *      use wait_for_ready_children() to wait until
 *      the first process raises SIGSTOP.
 */
int main(void)
{
	pid_t pid;
	int status;

	/* Fork root of process tree */
	pid = fork();
	if (pid < 0) {
		perror("main: fork");
		exit(1);
	}
	if (pid == 0) {
		/* Child */
		fork_procs();
		exit(1);
	}
	
	/*
	 * Father
	 */
	/* for ask2-signals */
	/* wait_for_ready_children(1); */

	/* for ask2-{fork, tree} */
	sleep(SLEEP_TREE_SEC);
	

	/* Print the process tree root at pid */
	show_pstree(pid);

	/* for ask2-signals */
	/* kill(pid, SIGCONT); */

	/* Wait for the root of the process tree to terminate */
	pid = wait(&status);
	explain_wait_status(pid, status);

	return 0;
}
