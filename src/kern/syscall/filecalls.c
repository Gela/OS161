/*
 * filecalls.c
 *
 *  Created on: Mar 6, 2014
 *      Author: trinity
 */

#include <kern/filecalls.h>
#include <syscall.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
#include <stdarg.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <uio.h>
#include <kern/iovec.h>
#include <synch.h>
#include <current.h>
#include <copyinout.h>
#include <kern/seek.h>
#include <kern/stat.h>



int
filetable_init(void) {

	//kprintf("Inside INIT\n");

	struct vnode *vnin;
	struct vnode *vnout;
	struct vnode *vnerr;
	char *in = NULL;
	char *out = NULL;
	char *er = NULL;

	in = kstrdup("con:");
	out = kstrdup("con:");
	er = kstrdup("con:");

	// STDIN
	if(vfs_open(in, O_RDONLY, 0, &vnin)) {
		//kprintf("INIT- IN Initialization Failed\n");
		kfree(in);
		kfree(out);
		kfree(er);
		vfs_close(vnin);
		return EINVAL;
	}

	curthread->fdtable[0] = (struct fdesc *)kmalloc(sizeof(struct fdesc));
	curthread->fdtable[0]->vn = vnin;
	strcpy(curthread->fdtable[0]->file_name,"con:");
	curthread->fdtable[0]->flags = O_RDONLY;
	curthread->fdtable[0]->offset = 0;
	curthread->fdtable[0]->refcount = 1;
	curthread->fdtable[0]->lk = lock_create(in);

	// STDOUT
	if(vfs_open(out, O_WRONLY, 0, &vnout)) {
		//kprintf("INIT- OUT Initialization Failed\n");
		kfree(in);
		kfree(out);
		kfree(er);
		lock_destroy(curthread->fdtable[0]->lk);
		kfree(curthread->fdtable[0]);
		vfs_close(vnin);
		vfs_close(vnout);
		return EINVAL;
	}

	curthread->fdtable[1] = (struct fdesc *)kmalloc(sizeof(struct fdesc));
	curthread->fdtable[1]->vn = vnout;
	strcpy(curthread->fdtable[1]->file_name,"con:");
	curthread->fdtable[1]->flags = O_WRONLY;
	curthread->fdtable[1]->offset = 0;
	curthread->fdtable[1]->refcount = 1;
	curthread->fdtable[1]->lk = lock_create(out);

	// STDERR
	if(vfs_open(er, O_WRONLY, 0, &vnerr)) {
		//kprintf("INIT- ERR Initialization Failed\n");
		kfree(in);
		kfree(out);
		kfree(er);
		lock_destroy(curthread->fdtable[0]->lk);
		kfree(curthread->fdtable[0]);
		lock_destroy(curthread->fdtable[1]->lk);
		kfree(curthread->fdtable[1]);
		vfs_close(vnin);
		vfs_close(vnout);
		vfs_close(vnerr);
		return EINVAL;
	}

	curthread->fdtable[2] = (struct fdesc *)kmalloc(sizeof(struct fdesc));
	curthread->fdtable[2]->vn = vnerr;
	strcpy(curthread->fdtable[2]->file_name,"con:");
	curthread->fdtable[2]->flags = O_WRONLY;
	curthread->fdtable[2]->offset = 0;
	curthread->fdtable[2]->refcount = 1;
	curthread->fdtable[2]->lk = lock_create(er);

	//kprintf("Outside INIT\n");
	return 0;
}

int
sys_open(const char *filename, int flags, mode_t mode, int *retval) {

	//kprintf("Inside OPEN\n");

	int result=0, index = 3;
	struct vnode *vn;
	char *kbuf;
	size_t len;
	kbuf = (char *) kmalloc(sizeof(char)*PATH_MAX);
	result = copyinstr((const_userptr_t)filename,kbuf, PATH_MAX, &len);
	if(result) {
		//kprintf("OPEN- copyinstr failed- %d\n",result);
		kfree(kbuf);
		return result;
	}

	/*	if(!(flags==O_RDONLY || flags==O_WRONLY || flags==O_RDWR || flags==(O_RDWR|O_CREAT|O_TRUNC))) {
		kprintf("OPEN- Wrong flags- %d\n",flags);
		return EINVAL;
	}*/

	/*	if(curthread->fdtable[0] == NULL) {
		result = filetable_init();
		if(result) {
			kprintf("OPEN- INIT failed\n");
			return result;
		}
	}*/

	while (curthread->fdtable[index] != NULL ) {
		index++;
	}

	if(index == OPEN_MAX) {
		//kprintf("OPEN- index>128\n");
		kfree(kbuf);
		return ENFILE;
	}

	curthread->fdtable[index] = (struct fdesc *)kmalloc(sizeof(struct fdesc*));
	if(curthread->fdtable[index] == NULL) {
		//kprintf("OPEN- filetable at given index is null\n");
		kfree(kbuf);
		return ENFILE;
	}

	result = vfs_open(kbuf,flags,mode,&vn);
	if(result) {
		//kprintf("OPEN- vfs_open failed\n");
		kfree(kbuf);
		kfree(curthread->fdtable[index]);
		curthread->fdtable[index] = NULL;
		return result;
	}

	curthread->fdtable[index]->vn = vn;
	strcpy(curthread->fdtable[index]->file_name, kbuf);
	curthread->fdtable[index]->flags = flags;
	curthread->fdtable[index]->refcount = 1;
	curthread->fdtable[index]->offset = 0;
	curthread->fdtable[index]->lk= lock_create(kbuf);

	*retval = index;
	kfree(kbuf);
	//kprintf("Outside OPEN, Returned Index is %d, Flags- %d\n", index,curthread->fdtable[index]->flags);
	return 0;
}

int
sys_close(int filehandle, int *retval) {

	//kprintf("Inside CLOSE\n");

	if(filehandle >= OPEN_MAX || filehandle < 0) {
		//kprintf("CLOSE- Bad filehandle\n");
		return EBADF;
	}

	if(curthread->fdtable[filehandle] == NULL) {
		//kprintf("CLOSE- filetable at given index is null\n");
		return EBADF;
	}

	if(curthread->fdtable[filehandle]->vn == NULL) {
		//kprintf("CLOSE- vnode in the given filetable index is null\n");
		return EBADF;
	}

	if(curthread->fdtable[filehandle]->refcount == 1) {
		VOP_CLOSE(curthread->fdtable[filehandle]->vn);
		lock_destroy(curthread->fdtable[filehandle]->lk);
		kfree(curthread->fdtable[filehandle]);
		curthread->fdtable[filehandle] = NULL;
	} else {
		curthread->fdtable[filehandle]->refcount -= 1;
	}

	*retval = 0;
	return 0;
	//kprintf("Outside CLOSE\n");

}

int
sys_read(int filehandle, void *buf, size_t size, int *retval) {

	//kprintf("Inside READ, Filehandle- %d\n",filehandle);

	int result=0;

	/*	if(curthread->fdtable[0] == NULL) {
		result = filetable_init();
		if(result) {
			kprintf("READ- INIT failed\n");
			return result;
		}
	}*/

	if(filehandle >= OPEN_MAX || filehandle < 0) {
		//kprintf("READ- Bad Filehandle\n");
		return EBADF;
	}

	if(curthread->fdtable[filehandle] == NULL) {
		//kprintf("READ- Filetable at given index is null\n");
		return EBADF;
	}

	if (curthread->fdtable[filehandle]->flags == O_WRONLY) {
		//kprintf("READ- Wrong Flag\n");
		return EBADF;
	}

	struct iovec iov;
	struct uio ku;
	void *kbuf;
	kbuf = kmalloc(sizeof(*buf)*size);
	if(kbuf == NULL) {
		//kprintf("READ- kbuf is null\n");
		return EINVAL;
	}

	lock_acquire(curthread->fdtable[filehandle]->lk);

	//uio_kinit(&iov, &ku, kbuf, size ,curthread->fdtable[filehandle]->offset,UIO_READ);

	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = size;
	ku.uio_iov = &iov;
	ku.uio_iovcnt = 1;
	ku.uio_offset = curthread->fdtable[filehandle]->offset;
	ku.uio_resid = size;
	ku.uio_segflg = UIO_USERSPACE;
	ku.uio_rw = UIO_READ;
	ku.uio_space = curthread->t_addrspace;


	result = VOP_READ(curthread->fdtable[filehandle]->vn, &ku);
	if(result) {
		//kprintf("READ- VOP_READ Failed\n");
		kfree(kbuf);
		lock_release(curthread->fdtable[filehandle]->lk);
		return result;
	}

	//	result = copyout((const void*)kbuf, (userptr_t)buf, size);
	//	if(result) {
	//		//kprintf("READ- copyout failed\n");
	//		lock_release(curthread->fdtable[filehandle]->lk);
	//		return result;
	//	}

	curthread->fdtable[filehandle]->offset = ku.uio_offset;

	*retval = size - ku.uio_resid;
	//kprintf("Outside READ, Bytes read- %d\n", (size - ku.uio_resid));
	kfree(kbuf);
	lock_release(curthread->fdtable[filehandle]->lk);
	return 0;

}

int
sys_write(int filehandle, const void *buf, size_t size, int *retval) {

	//kprintf("Inside WRITE, Filehandle- %d, Size- %d\n",filehandle, size);

	int result=0;

	/*	if(curthread->fdtable[0] == NULL) {
		result = filetable_init();
		if(result) {
			kprintf("WRITE- INIT failed\n");
			return result;
		}
	}*/

	if(filehandle >= OPEN_MAX || filehandle < 0) {
		//kprintf("WRITE- Bad Filehandle\n");
		return EBADF;
	}

	if(curthread->fdtable[filehandle] == NULL) {
		//kprintf("WRITE- Filetable at given index is null\n");
		return EBADF;
	}

	if (curthread->fdtable[filehandle]->flags == O_RDONLY) {
		//kprintf("WRITE- Wrong Flag\n");
		return EBADF;
	}


	struct iovec iov;
	struct uio ku;
	void *kbuf;
	kbuf = kmalloc(sizeof(*buf)*size);
	if(kbuf == NULL) {
		//kprintf("READ- kbuf is null\n");
		return EINVAL;
	}

	lock_acquire(curthread->fdtable[filehandle]->lk);

	result = copyin((const_userptr_t)buf,kbuf,size);
	if(result) {
		//kprintf("WRITE- copyin Failed\n");
		kfree(kbuf);
		lock_release(curthread->fdtable[filehandle]->lk);
		return result;
	}

	//uio_kinit(&iov, &ku, kbuf, size ,curthread->fdtable[filehandle]->offset,UIO_WRITE);

	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = size;
	ku.uio_iov = &iov;
	ku.uio_iovcnt = 1;
	ku.uio_offset = curthread->fdtable[filehandle]->offset;
	ku.uio_resid = size;
	ku.uio_segflg = UIO_USERSPACE;
	ku.uio_rw = UIO_WRITE;
	ku.uio_space = curthread->t_addrspace;

	result = VOP_WRITE(curthread->fdtable[filehandle]->vn, &ku);
	if(result) {
		//kprintf("WRITE- VOP_WRITE Failed\n");
		kfree(kbuf);
		lock_release(curthread->fdtable[filehandle]->lk);
		return result;
	}

	curthread->fdtable[filehandle]->offset = ku.uio_offset;

	*retval = size - ku.uio_resid;
	//kprintf("Outside WRITE, Bytes written- %d\n", (size - ku.uio_resid) );
	kfree(kbuf);
	lock_release(curthread->fdtable[filehandle]->lk);
	return 0;

}

off_t
sys_lseek(int filehandle, off_t pos, int whence, int *retval, int *retval1) {

	//kprintf("Inside LSEEK, Filehandle- %d\n",filehandle);

	int result;
	if(filehandle >= OPEN_MAX || filehandle < 0) {
		//		if(filehandle == 0 || filehandle == 1 || filehandle == 2) {
		//			if(curthread->fdtable[filehandle] != NULL) {
		//				if(curthread->fdtable[filehandle]->flags != O_WRONLY){
		//					kprintf("LSEEK- Filehandle does not support seeking\n");
		//					return ESPIPE;
		//				}
		//			}
		//		}
		//kprintf("LSEEK- Bad Filehandle\n");
		return EBADF;
	}

	if(curthread->fdtable[filehandle] == NULL) {
		//kprintf("LSEEK- Filetable at given index is null\n");
		return EBADF;
	}

	off_t offset, file_size;
	struct stat statbuf;

	lock_acquire(curthread->fdtable[filehandle]->lk);
	result = VOP_STAT(curthread->fdtable[filehandle]->vn, &statbuf);
	if(result) {
		//kprintf("LSEEK - VOP_STAT failed\n");
		lock_release(curthread->fdtable[filehandle]->lk);
		return result;
	}

	file_size = statbuf.st_size;

	if(whence == SEEK_SET) {
		result = VOP_TRYSEEK(curthread->fdtable[filehandle]->vn,pos);
		if(result) {
			//kprintf("LSEEK- VOP_TRYSEEK case1 failed\n");
			lock_release(curthread->fdtable[filehandle]->lk);
			return result;
		}
		offset = pos;

	}else if(whence == SEEK_CUR) {
		result = VOP_TRYSEEK(curthread->fdtable[filehandle]->vn,(curthread->fdtable[filehandle]->offset + pos));
		if(result) {
			//kprintf("LSEEK- VOP_TRYSEEK case2 failed\n");
			lock_release(curthread->fdtable[filehandle]->lk);
			return result;
		}
		offset = curthread->fdtable[filehandle]->offset + pos;

	}else if(whence == SEEK_END) {
		result = VOP_TRYSEEK(curthread->fdtable[filehandle]->vn,(file_size + pos));
		if(result) {
			//kprintf("LSEEK- VOP_TRYSEEK case3 failed\n");
			lock_release(curthread->fdtable[filehandle]->lk);
			return result;
		}
		offset = file_size + pos;

	}else {
		//kprintf("LSEEK - Invalid whence\n");
		lock_release(curthread->fdtable[filehandle]->lk);
		return EINVAL;
	}

	if(offset < (off_t)0) {
		//kprintf("LSEEK- Invalid/Negative resulting kseek position\n");
		lock_release(curthread->fdtable[filehandle]->lk);
		return EINVAL;
	}
	curthread->fdtable[filehandle]->offset = offset;

	*retval = (uint32_t)((offset & 0xFFFFFFFF00000000) >> 32);
	*retval1 = (uint32_t)(offset & 0xFFFFFFFF);

	//kprintf("Outside LSEEK\n");
	lock_release(curthread->fdtable[filehandle]->lk);
	return 0;
}

int
sys_dup2(int oldfd, int newfd, int *retval) {

	int result=0;
	/*	if(curthread->fdtable[0] == NULL) {
		result = filetable_init();
		if(result) {
			kprintf("DUP2- INIT failed\n");
			return result;
		}
	}*/

	if(oldfd >= OPEN_MAX || oldfd < 0 || newfd >= OPEN_MAX || newfd < 0) {
		//kprintf("DUP2- Bad Filehandle\n");
		return EBADF;
	}

	if(oldfd == newfd) {
		//kprintf("DUP2 - Same Filehandles\n");
		*retval = newfd;
		return 0;
	}

	if(curthread->fdtable[oldfd] == NULL) {
		//kprintf("DUP2- Filetable at given index is null\n");
		return EBADF;
	}

	if(curthread->fdtable[newfd] != NULL) {
		result = sys_close(newfd, retval);
		if(result) {
			//kprintf("DUP2- newfd sys_close failed\n");
			return EBADF;
		}
	}
	else {
		curthread->fdtable[newfd] = (struct fdesc *)kmalloc(sizeof(struct fdesc*));
	}
	lock_acquire(curthread->fdtable[oldfd]->lk);
	curthread->fdtable[newfd]->vn = curthread->fdtable[oldfd]->vn;
	curthread->fdtable[newfd]->offset = curthread->fdtable[oldfd]->offset;
	curthread->fdtable[newfd]->flags = curthread->fdtable[oldfd]->flags;

	strcpy(curthread->fdtable[newfd]->file_name, curthread->fdtable[oldfd]->file_name);
	curthread->fdtable[newfd]->lk = lock_create("dup2 file");
	//			curthread->fdtable[oldfd]->lk;



	lock_release(curthread->fdtable[oldfd]->lk);
	*retval = newfd;
	return 0;
}

int
sys_chdir(const char *pathname, int *retval) {

	int result=0;
	char *kbuf;
	size_t len;
	kbuf = (char *) kmalloc(sizeof(char)*PATH_MAX);
	result = copyinstr((const_userptr_t)pathname,kbuf, PATH_MAX, &len);
	if(result) {
		//kprintf("CHDIR- copyinstr failed- %d\n",result);
		kfree(kbuf);
		return EFAULT;
	}

	result = vfs_chdir(kbuf);
	if(result) {
		//kprintf("CHDIR- vfs_chdir failed\n");
		kfree(kbuf);
		return result;
	}

	*retval = 0;
	kfree(kbuf);
	return 0;
}

int
sys___getcwd(char *buf, size_t buflen, int *retval) {

	int result=0;
	struct iovec iov;
	struct uio ku;
	char *kbuf;
	size_t len;
	if(buf == NULL) {
		//kprintf("GETCWD- buf is null\n");
		return EFAULT;
	}

	kbuf = kmalloc(sizeof(*buf)*buflen);
	if(kbuf == NULL) {
		//kprintf("GETCWD- kbuf is null\n");
		return EINVAL;
	}

	result = copyinstr((const_userptr_t)buf,kbuf, PATH_MAX, &len);
	if(result) {
		//kprintf("GETCWD- copyinstr failed- %d\n",result);
		kfree(kbuf);
		return EFAULT;
	}

	// Pass only (buflen-1) because we have to manually null terminate the last character
	//uio_kinit(&iov, &ku,(void *)kbuf, (buflen-1), (off_t)0 , UIO_READ);

	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = (buflen-1);
	ku.uio_iov = &iov;
	ku.uio_iovcnt = 1;
	ku.uio_offset = (off_t)0;
	ku.uio_resid = (buflen-1);
	ku.uio_segflg = UIO_USERSPACE;
	ku.uio_rw = UIO_READ;
	ku.uio_space = curthread->t_addrspace;

	result = vfs_getcwd(&ku);
	if(result) {
		//kprintf("GETCWD- vfs_getcwd failed\n");
		kfree(kbuf);
		return result;
	}

	// Null Terminate the last character of the buffer
	buf[sizeof(buf)-1 - ku.uio_resid] = '\0';

	*retval = strlen(buf);
	kfree(kbuf);
	return 0;
}
