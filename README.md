OS161
=====

OS/161 is a simplified operating system which includes a standalone kernel and a simple userland, all written in C.
It runs on a machine simulator, System/161.

#### About it

OS/161is the operating system, the sys161 is the simulator, the GNU debugger (GDB) is used, and the Git revision control system is used for subversion control. 

#### Synchronization

Implementation of locks, condition variables and reader-writer locks.
Using these synchronization tools solving a few simple toy synchronization problems.
The locks, condition variables, reader/writer interface are implemented in synch.h and the code is in synch.c. 

The following is a self-explanatory template for reader writer code.

    struct rwlock * rwlock_create(const char *);
    void rwlock_destroy(struct rwlock *);

    void rwlock_acquire_read(struct rwlock *);
    void rwlock_release_read(struct rwlock *);
    void rwlock_acquire_write(struct rwlock *);
    void rwlock_release_write(struct rwlock *);

#### System Calls and Process Support

Implementing of the system call interface. After this the kernel should be able to run user programs.

The following system calls have been implemented and test for all types of bad calls

    •	File system support: open, read, write, lseek, close, dup2, chdir, and __getcwd.
    
For any given process, the first file descriptors (0, 1, and 2) are considered to be
standard input (stdin), standard output (stdout), and standard error (stderr).
These file descriptors should start out attached to the console device ("con:"),
but the implementation allows programs to use dup2() to change them to point elsewhere.

Although these system calls may seem to be tied to the filesystem, in fact,
they are really about manipulation of file descriptors, or process-specific filesystem state. 

    •	Process support: getpid, fork, execv, waitpid, and _exit.
    
##### getpid()
    
    •	A pid, or process ID, is a unique number that identifies a process. 

##### fork(), execv(), waitpid() and _exit()
    •	These system calls enable multiprogramming and make OS/161 a usable system.
    •	fork() is the mechanism for creating new processes. It makes a copy of the invoking process
      and makes sure that the parent and child processes each observe the correct
      return value (that is, 0 for the child and the newly created pid for the parent).
      Locks are used for fork() as the syncronization primitives.
    •	execv(), although merely a system call, is really the heart for running user programs.
      It is responsible for taking newly created processes and make them execute something different
      than what the parent is executing. It replaces the existing address space with a brand new one
      for the new executable—created by calling as_create in the current virtual memory system—and then run it.
      While this is similar to starting a process straight out of the kernel, as runprogram() does, it's not
      quite that simple. Remember that this call is coming out of userspace, into the kernel, and then
      returning back to userspace. So necessary changes are made in order to manage the memory that travels
      across these boundaries very carefully. Also, notice that runprogram() doesn't take an argument vector,
      but this this is of course handled correctly by execv().
    •	Although it may seem simple at first, waitpid() requires a fair bit of design.
      The implementation of _exit() is intimately connected to the implementation of waitpid().
      They are essentially two halves of the same mechanism
    •	waitpid()/_exit() are also used as a synchronization primitives 

##### kill_curthread()
    
    •	Eessentially nothing about the current thread's userspace state can be trusted if it has suffered a fatal exception. It must be taken off the processor in as judicious a manner as possible, which is simply done by an explicit call to sys_exit() with exit code 1.


#### Virtual Memory

This implementation includes virtual memory, address translation, TLB management and page replacement.
It does not support swapping yet. After implementing everything the kernel should be able to run forever
without running out of memory.

In order to configure OS161 and run the kernel, follow the steps in
[http://www.ops-class.org/asst/0](http://www.ops-class.org/asst/0)


#### Error Handling

The man pages in the OS/161 distribution contain a description of the error return values that you must return.
If there are conditions that can happen that are not listed in the man page, the most appropriate error code
from kern/include/kern/errno.h is returned. 
