/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <opt-A2.h>
#include <copyinout.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char** argv, unsigned long argc)
{
	//DEBUG(DB_SYSCALL,"in runprogram\n");
	if (argc > 64){
		return E2BIG;
	}
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	vaddr_t pptr; //present ptr
	int result;
	char* odargs = kmalloc(ARG_MAX); //1d args
	int ofst = 0; //hlpr var 
	size_t * ofstsarr = kmalloc(argc * sizeof (size_t));//loc of each arg using offsets
	userptr_t * usr_args = kmalloc(sizeof(userptr_t) * (argc+1));//usr stack


	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}
	//write something here

	// while(argv[index] != NULL) {
	// 	char * temp;
	// 	len = strlen(argv[index]) + 1; // \0

	// 	int olen = len;
	// 	if(len % 4 != 0) {
	// 		len = len + (4 - len % 4);
	// 	}

	// 	temp=kmalloc(sizeof(len));
	// 	temp= kstrdup(argv[index]);

	// 	for(int i=0; i < len; i++) {

	// 		if(i>=olen)
	// 			arg[i]= '\0';
	// 		else
	// 			arg[i]=argv[index][i];
	// 	}
	//}

	for (unsigned int i = 0; i < argc; i++){
		char * temp = argv[i];
		if(strlen(temp)+ofst +1 >ARG_MAX){// \0 char
			return E2BIG;
		}
		//copy string onto args
		strcpy(odargs+ofst, temp);
		ofstsarr [i] = ofst;

		ofst = ofst + ROUNDUP(strlen(temp)+1,8);
	}
	//assigning present ptr to stack
	pptr = stackptr;
	pptr = pptr-ofst;
	//copyout result
	result = copyout((const void *)odargs, (userptr_t)pptr, (size_t)ofst);
	if (result != 0){
		//kprintf("wrong runprog result after 1st copyout\n");
		return result;
	}
	kfree(odargs);

	//usr stack using given var
	for (unsigned int i = 0; i<argc;i++){
		userptr_t temp = (userptr_t)pptr + ofstsarr[i];
		usr_args [i] = temp;
	}
	usr_args[argc] = NULL;
	pptr = pptr - sizeof(userptr_t) * (argc+1);

	result = copyout((const void *)usr_args, (userptr_t)pptr, (size_t)(sizeof(userptr_t) *(argc+1)));
	if (result != 0){
		//kprintf("wrong runprog result after 2nd copyout\n");
		return result;
	}
	kfree(usr_args);
	kfree(ofstsarr);

	/* Warp to user mode. */
	enter_new_process(argc, (userptr_t)pptr /*userspace addr of argv*/,
			  pptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

