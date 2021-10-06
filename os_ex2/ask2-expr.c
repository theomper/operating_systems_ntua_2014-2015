#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "proc-common.h"
#include "tree.h"

#define SLEEP_PROC_SEC  8
#define SLEEP_TREE_SEC  3 

void fork_procs(struct tree_node* root)
{
	/*
	 * initial process is root
	 */

	pid_t pid_child;
	int i, status;
	pid_t* child;

	printf("PID=%ld, name %s,starting...\n",(long)getpid(),root->name);
	change_pname(root->name);

	child = (pid_t*)malloc(sizeof(pid_t)*root->nr_children);
	if (child == NULL){
		fprintf(stderr,"allocate children failed\n");
		exit(1);
	}	
	
	//if root has childen then forking is necessary
	for (i = 0; i < root->nr_children; i++){
		fprintf(stderr, "Parent %s with PID = %ld: Creating child with name %s...\n", root->name, (long)getpid(), root->children[i].name);
		pid_child = fork();
		if (pid_child < 0) {
			/* fork failed */
			perror("fork");
			exit(1);
		}
		if (pid_child == 0) {
			/* In child process */
			fork_procs(&root->children[i]);
		}
		//Parent saves child's PID
		child[i] = pid_child;
	}
	printf("%s: Waiting for children...\n", root->name);
	wait_for_ready_children(root->nr_children);
	raise(SIGSTOP);
	printf("%s:Awake...\n",root->name);
	for (i = 0; i < root->nr_children; ++i){
		printf("%s: Waking child %s...\n", root->name, root->children[i].name);//?
		kill(child[i], SIGCONT);
		child[i] = wait(&status);
		explain_wait_status(child[i], status);
	}
	printf("%s: Exiting...\n", root->name);
	free(child);
	exit(0);
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
	pid_t pid;
	int status;
	struct tree_node *root;

	if (argc != 2){
		fprintf(stderr, "Usage: %s <input_tree_file>\n\n", argv[0]);
		exit(1);
	}

	root = get_tree_from_file(argv[1]);
	print_tree(root);

	/* Fork root of process tree */
	pid = fork();
	if (pid < 0) {
		perror("main: fork");
		exit(1);
	}
	if (pid == 0) {
		/* Child */
		fork_procs(root);
		exit(1);
	}
	
	/*
	 * Father
	 */
	/* for ask2-signals */
	/* wait_for_ready_children(1); */
	wait_for_ready_children(1);

	/* Print the process tree root at pid */
	show_pstree(pid);

	/* for ask2-signals */
	kill(pid,SIGCONT);

	/* Wait for the root of the process tree to terminate */
	pid = wait(&status);
	explain_wait_status(pid, status);

	return 0;
}
