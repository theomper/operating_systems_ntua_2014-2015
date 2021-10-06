#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include "proc-common.h"
#include "tree.h"

#define SLEEP_PROC_SEC  8
#define SLEEP_TREE_SEC  3 

void fork_procs(struct tree_node* root,int fd)
{
	/*
	 * initial process is root
	 */

	int pfd[2][2];
	int value;
	int value1,value2;
	pid_t pid_child;
	int i, status;
	pid_t* child;

	change_pname(root->name);
	printf("%s: Created with PID = %ld,and given fd= %d.\n", root->name, (long)getpid(),fd);

	child = (pid_t*)malloc(sizeof(pid_t)*root->nr_children);
	if (child == NULL){
		fprintf(stderr,"allocate children failed\n");
		exit(1);
	}

	if(root->nr_children==0){
		value=atoi(root->name);
		if(write(fd,&value,sizeof(value)) !=sizeof(value)){
			perror("write to pipe");
			exit(1);
		}
		exit(0);
	}

	//if root has childen then forking is necessary
	for (i = 0; i < root->nr_children; i++){

		if (pipe(pfd[i])<0){	
       			perror("pipe");
      		       	exit(1);
		}

		pid_child = fork();
		if (pid_child < 0) {
			/* fork failed */
			perror("fork");
			exit(1);
		}
		if (pid_child == 0) {
			/* In child process */
			close(pfd[i][0]);
			fork_procs(&root->children[i],pfd[i][1]);
		}
	}
	pid_child = wait(&status);
	explain_wait_status(pid_child,status);
	pid_child = wait(&status);
	explain_wait_status(pid_child,status);
	
	if (read(pfd[0][0], &value1, sizeof(value1)) != sizeof(value1)) {
		perror("child: read from pipe");
		exit(1);
	}
	if (read(pfd[1][0], &value2, sizeof(value2)) != sizeof(value2)) {
		perror("child: read from pipe");
		exit(1);
	}
	printf("%s: Read from pipes values %d & %d \n",root->name,value1,value2);
	if (strcmp(root->name,"+") == 0)
		value = value1 + value2;
	else
		value = value1 * value2;
	if (write(fd, &value, sizeof(value)) != sizeof(value)) {
		perror("parent: write to pipe");
		exit(1);
	}
	close(fd);
	printf("%s: Wrote to pipe result: %d!\n",root->name,value);
	printf("%s: Exiting...\n",root->name);
	free(child);
	
	
	exit(7);
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
int main(int argc, char *argv[])
{
	int fd[2];
	pid_t pid;
	int status,value;
	struct tree_node *root;

	if (argc != 2){
		fprintf(stderr, "Usage: %s <input_tree_file>\n\n", argv[0]);
		exit(1);
	}

	root = get_tree_from_file(argv[1]);
	print_tree(root);
	if(pipe(fd)<0){
		perror("pipe");
		exit(1);
	}
		
	/* Fork root of process tree */
	pid = fork();
	if (pid < 0) {
		perror("main: fork");
		exit(1);
	}
	if (pid == 0) {
		/* Child */
		close(fd[0]);
		fork_procs(root,fd[1]);
	}
	close(fd[1]);	
	/*
	 * Father
	 */
	
	/* Wait for the root of the process tree to terminate */
	pid = wait(&status);
	explain_wait_status(pid, status);
	
	if(read(fd[0],&value,sizeof(value))!=sizeof(value)){
		    perror("read from pipe");			
    		    exit(1);
       	}
	close(fd[0]);


	printf("FINAL RESULT IS %d!!\n",value);
	return 0;
}
