/*
 * processcalls.h
 *
 *  Created on: Mar 9, 2014
 *      Author: trinity
 */

#ifndef PROCESSCALLS_H_
#define PROCESSCALLS_H_

#include <../../../user/include/sys/null.h>
#include <limits.h>
#include <types.h>
#include <../../arch/mips/include/trapframe.h>

struct process_table_entry{
	struct process * procs;
	pid_t pid;
	struct process_table_entry * next;
};

struct process {
	pid_t ppid; /* parent process id */
	struct semaphore* exitsem;
	bool exited:1;
	int exitcode;
	struct thread* thread_for_process;
};

extern struct process_table_entry *process_table;

extern int proc_count;

pid_t givepid(void);
void init_process(struct thread *t, pid_t id);
void destroy_process(pid_t pid);
void entrypoint(void* data1, unsigned long data2);
void changeppid(pid_t change,pid_t ppid);

pid_t sys_getpid(void);
pid_t sys_waitpid(pid_t pid, int *status, int options, int *retval);
void sys__exit(int exitcode);
pid_t sys_fork(struct trapframe *tf, int *retval);
int sys_execv(const char *program, char **args);
int sys_sbrk(intptr_t amount, int *retval);
#endif /* PROCESSCALLS_H_ */
