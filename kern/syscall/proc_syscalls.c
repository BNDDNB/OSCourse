//sys rel
#include <types.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
//aiding rel
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <copyinout.h>
//misc rel...
#include <lib.h>
#include <addrspace.h>
//#include <test.h>
#include <vm.h>
#include <vfs.h>
#include <mips/trapframe.h>
#include <limits.h>
#include <array.h>
#include <spl.h>
#include <bitmap.h>



  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode,int stats) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  //(void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);
  //acquire lock for fin process
  lock_acquire(p->p_fin_lk);
  //if par not fin and currently running
  //kprintf("%d\n",!p->p_par_fin);
  if(p->pid > 1 && !p->p_par_fin){
    //change stats to fin, wake up all, and term all ch
    p_to_fin(p, stats, exitcode);
    cv_broadcast(p->p_fin_cv, p->p_fin_lk);
    p_update_child(p);

    //
    proc_remthread(curthread);
    proc_destroy2(p);

    lock_release(p->p_fin_lk);
    //if its not running or par not running
  } else {
    p_update_child(p);
    lock_release(p->p_fin_lk);
    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_remthread(curthread);

    /* if this is the last user process in the system, proc_destroy()
       will wake up the kernel menu thread */
    proc_destroy(p);
  }

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */

//running 1
//nopid -1
//zomb -2
//exited 0

int
sys_getpid(pid_t *retval)
{
  DEBUG(DB_SYSCALL,"Syscall: _getpid\n");
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  DEBUG(DB_SYSCALL,"Syscall: _waitpid(%d)\n", pid);
  int exitstatus;
  int result;
  int temp;
  struct proc * waitedproc = pfindr(curproc,pid);

  //condition checking
  if (pid<PID_MIN){
    //kprintf("less than min on waitpid\n");
    return EINVAL;
  }
  if(pid > PID_MAX){
    //kprintf("more than max on waitpid\n");
    return EINVAL;
  }
  if (waitedproc == NULL){
    spinlock_acquire(&pid_map_splk);
    temp = bitmap_isset(pid_map, pid);
    spinlock_release(&pid_map_splk);
    //kprintf("pid not set\n");
    // if set then theres no proc, otherwise not set
    return temp? ECHILD:ESRCH; 
  }
  if(status == NULL){
    //kprintf("no status pointer waitpid\n");
    return EFAULT;
  }
  if(options!=0){
    //kprintf("invalid options \n");
    return EINVAL;
  }

 

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */
  // if (options != 0) {
  //   return(EINVAL);
  // }
  lock_acquire(waitedproc -> p_fin_lk);
   //while running
   while(!waitedproc->p_fin){
    cv_wait(waitedproc->p_fin_cv, waitedproc->p_fin_lk);
  }
  exitstatus = waitedproc -> p_fin_stat;
  lock_release(waitedproc->p_fin_lk);
  /* for now, just pretend the exitstatus is 0 */
  //exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

//new sys_call fork requires parents trapframe and child val

pid_t sys_fork(struct trapframe* ptf, pid_t* retval){
  //create a new process for use, with same name as parent
  int temp = 0;
  int spl;
  spl = splhigh();

  //child trapframe
  struct trapframe * ctf = kmalloc(sizeof(struct trapframe));
  if(ctf ==NULL){
    //kprintf("cannot create new trapframe (forking)\n");
    splx(spl);
    return ENOMEM;
  }
  struct proc * nProc = proc_create_runprogram(curproc->p_name);
  if (nProc == NULL){
    //kprintf("unable to create new process (forking)\n");
    kfree(ctf);
    splx(spl);
    return ENPROC;
  }
  //created successful, make a copy of current space
  as_copy(curproc_getas(),&(nProc->p_addrspace));
  if(nProc->p_addrspace == NULL){
    //kprintf("could not make address space(forking)\n");
    proc_destroy(nProc);
    kfree(ctf);
    splx(spl);
    return ENOMEM;
  }
  //kprintf("created new proc and alloc new add spaces(forking)\n");
  //copy of section of trap frame & set priority to high
  *ctf = *ptf;
  //memcpy(ctf,ptf, sizeof(struct trapframe));
  temp = thread_fork(curthread->t_name, nProc,&enter_forked_process_hlpr,(void *)ctf,
    (unsigned long)curproc->p_addrspace);
  splx(spl);
  if (temp){
    //failed to fork then undo everything
    proc_destroy(nProc);
    kfree(ctf);
    ctf = NULL;
    //no mem left?
    return temp;
  }
  *retval = nProc->pid;
  return 0;
}

/*
new sys_call execv, transform into another program
requires programs name and their arguments (runprog)
*/

int sys_execv(userptr_t program_name, userptr_t args){
  //using limit.h path_max restric progname
  //program name length for tempvar
  char* progname = kmalloc(PATH_MAX);
    //separation of args to list of em
  char** args_arr = kmalloc(ARG_MAX);
  size_t pgl;
  unsigned int argc = 0;
  char* argv = kmalloc(ARG_MAX);
  //hldr
  char* tempchar = NULL;
  //runprog parameter
  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  vaddr_t pptr; //present ptr
  int result;
  //char* odargs = kmalloc(ARG_MAX); //1d args
  int ofst = 0; //hlpr var 
  size_t * ofstsarr = kmalloc(argc * sizeof (size_t));//loc of each arg using offsets
  userptr_t * usr_args = kmalloc(sizeof(userptr_t) * (argc+1));//usr stack
  struct addrspace* curaddr = curproc_getas();//current to destroy

  //default checkr for error returning
  if((char*)program_name == NULL||(char**)args == NULL){
    //wrong arg ptr
    return EFAULT;
  }
  //count num of args and separate
  //kprintf("11111");
  while(true){
    copyin(argc*sizeof(char*) + args, &tempchar,sizeof(char*));
    args_arr[argc] = tempchar;
    if(tempchar == NULL){
      break;
    }else{
      argc++;
    }
  }
//argument number >64 reutnr err
  if (argc>64){
    //exceeding
    //kprintf("toobig of args");
    return E2BIG;
  }
  result = copyinstr(program_name, progname, PATH_MAX, &pgl);
  //err chkr for copy progname
  if(result){
    //kprintf("error copy progname");
    return result;
  }

  //checking each arguments for its length, similar to runprog
  for (unsigned int i = 0; i < argc; i++){
    //char * temp = argv[i];
    /*if(strlen(temp)+ofst +1 >ARG_MAX){// \0 char
      return E2BIG;
    }*/
    size_t temp;
    result = copyinstr((userptr_t)args_arr[i],argv+ofst, 
      ARG_MAX - ofst, &temp);
    if(result){
      return result;
    }
    //copy string onto args
    //strcpy(odargs+ofst, temp);
    ofstsarr [i] = ofst;
    ofst = ofst + ROUNDUP(temp+1,8);
  }
  //the operation of running program with above arguments

  /* Open the file. */
  result = vfs_open(progname, O_RDONLY, 0, &v);
  if (result) {
    return result;
  }
  kfree(progname);
  /* We should NOT be a new process. */
  //KASSERT(curproc_getas() == NULL);

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
  //assigning present ptr to stack
  pptr = stackptr;
  pptr = pptr-ofst;
  //copyout result
  result = copyout((const void *)argv, (userptr_t)pptr, (size_t)ofst);
  if (result != 0){
    //kprintf("wrong runprog result after 1st copyout\n");
    return result;
  }
  //kfree(odargs);

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
  //remove current process
  //kprint("freeing, start\n");
  as_destroy(curaddr);
  kfree(usr_args);
  kfree(ofstsarr);
  kfree(argv);
  kfree(args_arr);
  //kprintf("free finished");
  /* Warp to user mode. */
  enter_new_process(argc, (userptr_t)pptr /*userspace addr of argv*/,
        pptr, entrypoint);
  /* enter_new_process does not return. */
  panic ("enter_new_process returned\n");
  return EINVAL;

}
