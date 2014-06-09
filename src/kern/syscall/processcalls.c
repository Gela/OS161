/*
 * processcalls.c
 *
 *  Created on: Mar 9, 2014
 *      Author: trinity
 */

#include <kern/processcalls.h>
#include <thread.h>
#include <types.h>
#include <current.h>

#include <kern/errno.h>
#include <kern/wait.h>
#include <../include/synch.h>
#include <kern/wait.h>
#include <copyinout.h>
#include <kern/fcntl.h>

#include <lib.h>

#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <../include/spl.h>

//static struct process * process_table[PID_MAX];

struct process_table_entry *process_table = NULL;

int proc_count;

pid_t givepid(void) {

	if(process_table == NULL){

		proc_count = 1;
		process_table = (struct process_table_entry *)kmalloc(sizeof(struct process_table_entry));
		process_table->next = NULL;
		process_table->pid = 1;
		process_table->procs = (struct process *) kmalloc(sizeof(struct process));

	}
	//	pid_t pid = PID_MIN;
	//	while (process_table[pid] != NULL ) {
	//		pid++;
	//	}
	return ++proc_count;
}

void init_process(struct thread *t, pid_t id) {
	struct process *temp;
	temp = (struct process *) kmalloc(sizeof(struct process));
	temp->ppid = t->ppid;
	temp->exited = false;
	temp->exitcode = 0;
	temp->thread_for_process = t;
	struct semaphore *sem;
	sem = sem_create("Child", 0);
	temp->exitsem = sem;
	//process_table[id] = temp;
	struct process_table_entry * temp1;
	for(temp1=process_table;temp1->next!=NULL;temp1=temp1->next);
	temp1->next = (struct process_table_entry *)kmalloc(sizeof(struct process_table_entry));
	temp1->next->next = NULL;
	temp1->next->procs = temp;
	temp1->next->pid = id;
}

void destroy_process(pid_t pid) {
	//	if (process_table[pid] != NULL ) {
	//		//sem_destroy(process_table[pid]->exitsem);
	//		kfree(process_table[pid]);
	//		process_table[pid] = NULL;
	//	}
	struct process_table_entry * temp1, *temp2;
	for(temp1=process_table;temp1->next->pid!=pid;temp1=temp1->next);
	temp2 = temp1->next;
	temp1->next = temp1->next->next;
	kfree(temp2->procs);
	kfree(temp2);
}

void changeppid(pid_t change, pid_t ppid) {
	//process_table[change]->ppid = ppid;
	struct process_table_entry * temp1;
	for(temp1=process_table;temp1->pid!=change;temp1=temp1->next);
	temp1->procs->ppid = ppid;
}

pid_t sys_getpid(void) {
	return curthread->pid;
}

pid_t sys_waitpid(pid_t pid, int *status, int options, int *retval) {

	int result;
	struct process *child;

	if (options != 0) {
		//kprintf("WAITPID- Wrong options- %d\n",options);
		return EINVAL;
	}

	if (status == NULL ) {
		//kprintf("WAITPID- invalid status pointer\n");
		return EFAULT;
	}

	if (pid < PID_MIN) {
		//kprintf("WAITPID- Invalid pid- %d\n",pid);
		return EINVAL;
	}

	if(pid > PID_MAX) {
		//kprintf("WAITPID- Non-Existent pid- %d\n",pid);
		return ESRCH;
	}

	if (pid == curthread->pid) {
		//kprintf("WAITPID- pid same as self pid- %d\n",pid);
		return ECHILD;
	}

	if (pid == curthread->ppid) {
		//kprintf("WAITPID- Waiting on parent- %d\n",pid);
		return ECHILD;
	}

	// CHECK!!!!
	struct process_table_entry * temp1;
	for(temp1=process_table; temp1->pid!=pid || temp1==NULL; temp1=temp1->next);// CHECK THIS for && ||

	if (temp1 == NULL ) {
		//kprintf("WAITPID- No such process- %d\n",pid);
		return ESRCH;
	}


	if (temp1->procs->ppid != curthread->pid) {
		//kprintf("WAITPID- Not a child process- ARGUMENT PID: %d,ARGUMENT PID's PPID: %d, CURTHREAD PID: %d, CURTHREAD PPID: %d",pid,process_table[pid]->ppid,curthread->pid,curthread->ppid);
		return ECHILD;
	}

	if (temp1->procs->exited == false) {
		P(temp1->procs->exitsem);
	}

	child = temp1->procs;

	//copyout((const void *)&(child->exitcode),(userptr_t)status, sizeof(int));
	result = copyout((const void *) &(child->exitcode), (userptr_t) status,
			sizeof(int));
	if (result) {
		//kprintf("WAITPID- copyout failed %d, PPID: %d, PID: %d, Current Thread PID: %d\n",result,process_table[pid]->ppid,pid,curthread->pid);
		return EFAULT;
	}

	*retval = pid;

	destroy_process(pid);

	return 0;
}

void sys__exit(int exitcode) {
	//kprintf("Child PID: %d, Parent PID %d\n",curthread->pid, curthread->ppid);

	struct process_table_entry * temp1;
	for(temp1=process_table; temp1->pid!=curthread->ppid || temp1 == NULL; temp1=temp1->next);

	if (temp1 == NULL ) {
		//destroy_process(curthread->pid);
	} else if (temp1->procs->exited == false) {

		struct process_table_entry * temp2;
		for(temp2=process_table; temp2->pid!=curthread->pid || temp2==NULL; temp2=temp2->next);
		if(temp2 == NULL){
			kprintf("Process with PID %d not present in process table to exit/n",curthread->pid);
		}
		temp2->procs->exitcode = _MKWAIT_EXIT(exitcode);
		//kprintf("Exitcode: %d\n",process_table[curthread->pid]->exitcode);
		temp2->procs->exited = true;
		V(temp2->procs->exitsem);
	} else {
		destroy_process(curthread->pid);
	}

	thread_exit();
}

int sys_execv(const char *program, char **uargs) {
	//kprintf("Inside EXECV\n");
	//kprintf("Currently free pages in coremap- (execv before) %d\n",corefree());
	//COPY ARGUMENTS FROM USER SPACE INTO KERNEL BUFFER

	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, len;
	int index = 0;

	int i = 0;
	lock_acquire(elk);
	if (program == NULL || uargs == NULL ) {
		//kprintf("EXECV- Argument is a null pointer\n");
		return EFAULT;
	}

	char *progname;
	size_t size;
	progname = (char *) kmalloc(sizeof(char) * PATH_MAX);
	result = copyinstr((const_userptr_t) program, progname, PATH_MAX, &size);
	if (result) {
		//kprintf("EXECV- copyinstr failed- %d\n",result);
		kfree(progname);
		return EFAULT;
	}
	if (size == 1) {
		//kprintf("EXECV- Empty Program Name\n");
		kfree(progname);
		return EINVAL;
	}

	char **args = (char **) kmalloc(sizeof(char **));
	result = copyin((const_userptr_t) uargs, args, sizeof(char **));
	if (result) {
		//kprintf("EXECV- copyin failed- %d\n",result);
		kfree(progname);
		kfree(args);
		return EFAULT;
	}

	while (uargs[i] != NULL ) {
		args[i] = (char *) kmalloc(sizeof(char) * PATH_MAX);
		result = copyinstr((const_userptr_t) uargs[i], args[i], PATH_MAX,
				&size);
		if (result) {
			kfree(progname);
			kfree(args);
			return EFAULT;
		}
		i++;
	}
	args[i] = NULL;

	//	 Open the file.
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		kfree(progname);
		kfree(args);
		return result;
	}

	// We should be a new thread.
	//KASSERT(curthread->t_addrspace == NULL);

	//Create a new address space.
	struct addrspace *temp;
	temp = 	curthread->t_addrspace;

	if(curthread->t_addrspace != NULL){
		as_destroy(curthread->t_addrspace);
		curthread->t_addrspace = NULL;
	}

	KASSERT(curthread->t_addrspace == NULL);

	if ((curthread->t_addrspace = as_create()) == NULL ) {
		kfree(progname);
		kfree(args);
		vfs_close(v);
		return ENOMEM;
	}

	//Activate it.
	as_activate(curthread->t_addrspace);

	//Load the executable.
	result = load_elf(v, &entrypoint);
	if (result) {
		//thread_exit destroys curthread->t_addrspace
		kfree(progname);
		kfree(args);
		return result;
	}

	//Done with the file now.
	vfs_close(v);

	// Define the user stack in the address space
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		//thread_exit destroys curthread->t_addrspace
		kfree(progname);
		kfree(args);
		return result;
	}

	while (args[index] != NULL ) {
		char * arg;
		len = strlen(args[index]) + 1; // +1 for Null terminator \0

		int oglen = len;
		if (len % 4 != 0) {
			len = len + (4 - len % 4);
		}

		arg = kmalloc(sizeof(len));
		arg = kstrdup(args[index]);

		for (int i = 0; i < len; i++) {

			if (i >= oglen)
				arg[i] = '\0';
			else
				arg[i] = args[index][i];
		}

		stackptr -= len;

		result = copyout((const void *) arg, (userptr_t) stackptr,
				(size_t) len);
		if (result) {
			//kprintf("EXECV- copyout1 failed %d\n",result);
			kfree(progname);
			kfree(args);
			kfree(arg);
			return result;
		}

		kfree(arg);
		args[index] = (char *) stackptr;

		index++;
	}

	if (args[index] == NULL ) {
		stackptr -= 4 * sizeof(char);
	}

	for (int i = (index - 1); i >= 0; i--) {
		stackptr = stackptr - sizeof(char*);
		result = copyout((const void *) (args + i), (userptr_t) stackptr,
				(sizeof(char *)));
		if (result) {
			//kprintf("EXECV- copyout2 failed, Result %d, Array Index %d\n",result, i);
			kfree(progname);
			kfree(args);
			return result;
		}
	}

	kfree(progname);
	kfree(args);

	//kprintf("Currently free pages in coremap- (execv after) %d\n",corefree());

	lock_release(elk);
	//Warp to user mode.
	enter_new_process(index /*argc*/,
			(userptr_t) stackptr /*userspace addr of argv*/, stackptr,
			entrypoint);

	//enter_new_process does not return.
	panic("execv- enter_new_process returned\n");
	return EINVAL;

}

void entrypoint(void* data1, unsigned long data2) { // Arguments?? (void* data1, unsigned long data2)

	struct trapframe *tf, tfnew;
	struct addrspace * addr;

	tf = (struct trapframe *) data1;
	addr = (struct addrspace *) data2;

	tf->tf_a3 = 0;
	tf->tf_v0 = 0;
	tf->tf_epc += 4;

	// Load Address Space of Child and Activate it.
	curthread->t_addrspace = addr;
	as_activate(addr);

	tfnew = *tf;
	mips_usermode(&tfnew);
}

pid_t sys_fork(struct trapframe *tf, int *retval) {
	//kprintf("Currently free pages in coremap- (Fork before) %d\n",corefree());
	int result = 0;
	struct thread * ch_thread;

	//Copy parent’s address space
	struct addrspace * child_addrspace;
	result = as_copy(curthread->t_addrspace, &child_addrspace);
	if (result) {
		//kprintf("FORK: as_copy failed, %d\n",result);
		return ENOMEM;
	}

	//Copy parents trapframe
	struct trapframe *tf_child = kmalloc(sizeof(struct trapframe));
	if (tf_child == NULL ) {
		//kprintf("FORK: kmalloc for trapframe failed\n");
		return ENOMEM;
	}
	*tf_child = *tf;
	//memcpy(tf_child, tf, sizeof(struct trapframe));

	//Create child thread (using thread_fork)
	result = thread_fork("Child Thread", entrypoint,
			(struct trapframe *) tf_child, (unsigned long) child_addrspace,
			&ch_thread);
	if (result) {
		//kprintf("FORK: thread_fork failed, %d\n",result);
		return ENOMEM;
	}

	// Parent returns with child’s pid immediately
	*retval = ch_thread->pid;
	//kprintf("Currently free pages in coremap- (Fork after) %d\n",corefree());
	return 0;
}

int
sys_sbrk(intptr_t amount, int *retval){

	struct addrspace * as = curthread->t_addrspace;
	vaddr_t heap_start = as->heap_start;
	vaddr_t heap_end = as->heap_end;

	if(amount == 0 ){

		*retval = heap_end;
		return 0;

	}

	if(amount % 4){
		//amount = amount + (4 - amount % 4);
		*retval = -1;
		return EINVAL;
	}

	//	if(!(heap_end + amount >= heap_start)){
	//		return EINVAL;
	//	}

	if(amount < 0){
		if((long)heap_end + (long)amount < (long)heap_start ){
			*retval = -1;
			return EINVAL;
		}

		amount = amount * -1;

		if(amount < PAGE_SIZE){

			*retval = heap_end;
			curthread->t_addrspace->heap_end -= amount;

		}else {
			int num = amount / PAGE_SIZE;
			struct page_table_entry *pages;
			for(int i=0; i < num; i++){

				for(pages=as->heap;pages->next->next!=NULL;pages=pages->next);
				free_upages(pages->next->paddr);
				kfree(pages->next);
				pages->next = NULL;
			}

			*retval = heap_end;
			curthread->t_addrspace->heap_end -= amount;


		}


	}else {

		if((heap_end + amount) > (USERSTACK - 12 * PAGE_SIZE)){
			*retval = -1;
			return ENOMEM;
		}

		if((long)amount < (long)PAGE_SIZE && (((long)PAGE_SIZE - (((long)heap_end - (long)heap_start) % (long)PAGE_SIZE)) > (long)amount)){

			*retval = heap_end;
			curthread->t_addrspace->heap_end += amount;

		}else {
			intptr_t og_amount = amount;
			amount = amount - (PAGE_SIZE - ((heap_end - heap_start) % PAGE_SIZE));
			int num = amount / PAGE_SIZE;
			if(amount % PAGE_SIZE){
				num++;
			}

			if(num > corefree()){
				*retval = -1;
				return ENOMEM;
			}
			paddr_t paddr = 0;
			struct page_table_entry *pages;
			for(pages=as->heap;pages->next!=NULL;pages=pages->next);
			for(int i=0; i < num; i++){

				struct page_table_entry *heap_page = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
				pages->next = heap_page;
				heap_page->next = NULL;

				paddr = alloc_upages(1);
				if(paddr == 0){
					*retval = -1;
					return ENOMEM;
				}

				heap_page->paddr = paddr;
				heap_page->vaddr = pages->vaddr + PAGE_SIZE;

				pages = pages->next;

			}

			*retval = heap_end;
			curthread->t_addrspace->heap_end += og_amount;

		}
	}
	return 0;
}
