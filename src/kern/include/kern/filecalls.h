/*
 * filecalls.h
 *
 *  Created on: Mar 6, 2014
 *      Author: trinity
 */

#ifndef FILECALLS_H_
#define FILECALLS_H_

#include <types.h>
#include <limits.h>

/* File Descriptor Structure */
struct fdesc{
	char file_name[__NAME_MAX];
	struct vnode *vn; //   - Reference to the underlying file 'object'
	off_t offset;     //      - Offset into the file
	struct lock *lk;   //- Some form of synchronization
	int flags;      // - Flags with which the file was opened
	int refcount; //- Reference count
};

/* Prototypes defined by us */
int sys_open(const char *filename, int flags,mode_t mode, int *retval);
int sys_close(int filehandle, int *retval);
int sys_write(int filehandle, const void *buf, size_t size, int *retval);
int sys_read(int filehandle, void *buf, size_t size, int *retval);
off_t sys_lseek(int filehandle, off_t pos, int whence, int *retval, int *retval1);
int sys_dup2(int oldfd, int newfd, int *retval);
int sys_chdir(const char *pathname, int *retval);
int sys___getcwd(char *buf, size_t buflen, int *retval);

int filetable_init(void);


#endif /* FILECALLS_H_ */
