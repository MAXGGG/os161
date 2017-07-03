#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <vfs.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <limits.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <kern/fcntl.h>
#include <synch.h>
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

   // DEBUG(DB_EXEC, "sys exiting 1\n");

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  #if OPT_A2
  KASSERT(p!=NULL);
  //set current process state to exit
  p->p_state = 1;
  p->p_exitcode = _MKWAIT_EXIT(exitcode);
  if(p->p_parent!=NULL){

    //save exit information into parent status array
    struct childrenStatus* cs = getChildrenByPid(p->p_parent, p->p_id);
    cs->p_exitcode = _MKWAIT_EXIT(exitcode);
    cs->p_state = 1;

    lock_acquire(p->p_parent->p_cv_lock);
    //wake up parent
    cv_broadcast(p->p_parent->p_cv, p->p_parent->p_cv_lock);
    lock_release(p->p_parent->p_cv_lock);
  }
  #else
  (void)exitcode;
  #endif

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();

  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
   // DEBUG(DB_EXEC, "sys exiting 3\n");
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);
  // DEBUG(DB_EXEC, "sys exiting 4\n");
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  // DEBUG(DB_EXEC, "sys exiting 5\n");

  thread_exit();
  // DEBUG(DB_EXEC, "sys exiting finished\n");
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
   KASSERT(curproc!=NULL);
   *retval = curproc->p_id;
  #else
  *retval = 1;
  #endif
  return(0);

}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  #if OPT_A2

  struct proc *parent = curproc;
  struct childrenStatus *cs = getChildrenByPid(parent, pid);

  if(cs==NULL){
     return ECHILD;
  }

  lock_acquire(parent->p_cv_lock);

  DEBUG(DB_EXEC,"pid: %d is being blocked \n",(int)parent->p_id);
  //if child process is still running, block
  while(cs->p_state!=1){
     DEBUG(DB_EXEC,"pid: %d is being blocked \n",(int)parent->p_id);
     cv_wait(parent->p_cv, parent->p_cv_lock);
  }
  DEBUG(DB_EXEC,"pid: %d is released \n",(int)parent->p_id);

  lock_release(parent->p_cv_lock);
  exitstatus = cs->p_exitcode;
  #else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  #endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int
sys_fork(struct trapframe *tf, pid_t *retval)
{
   KASSERT(curproc!=NULL);
   struct proc *currentproc = curproc;
   struct proc *newp = proc_create_runprogram(currentproc->p_name);

   if(newp==NULL){
      return ENOMEM;
   }

   struct addrspace *newaddr = as_create();
   if(newaddr==NULL){
      return ENOMEM;
   }

   int result = as_copy(currentproc->p_addrspace, &newaddr);
   if(result==ENOMEM){
      return ENOMEM;
   }

   spinlock_acquire(&newp->p_lock);
 	struct addrspace *oldas = newp->p_addrspace;
 	newp->p_addrspace = newaddr;
 	spinlock_release(&newp->p_lock);

   as_destroy(oldas);

   newp->p_parent = currentproc;

   //create parent children relationship
   struct childrenStatus *cs = kmalloc(sizeof(struct childrenStatus));
   cs->p_id = newp->p_id;
   cs->p_state = 0;
   carray_add(&currentproc->p_children_status, cs, NULL);

   struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
   memcpy(newtf, tf, sizeof(struct trapframe));

   int errno = thread_fork(curthread->t_name, newp, &enter_forked_process, newtf, 0);
   if(errno){
      return errno;
   }

   *retval = newp->p_id;

   return 0;
}

int
sys_execv(userptr_t program, userptr_t args){
   (void) args;
   struct addrspace *as;
   struct vnode *v;
   vaddr_t entrypoint, stackptr;
   int result, argc;

   char* pname = (char*) program;

   char* program_path = kmalloc(sizeof(char)*strlen(pname));
   if(!program_path){
         return ENOMEM;
   }
   strcpy(program_path, pname);

   for(char** i=(char**)args; *i!=NULL;++i){
      argc++;
   }

   char** argv = kmalloc(sizeof(char*) * (argc+1));
   if(!argv){
      return ENOMEM;
   }
   result = copyinstr((userptr_t)program, argv[0], strlen(pname)+1, NULL );
   if(result){
      return result;
   }

   char** arg_a = (char**) args;
   for(int i=1;i<argc;++i){
      argv[i] = kmalloc(strlen(arg_a[i])+1);
      if(argv[i]){
         result = copyinstr((userptr_t)arg_a[i], argv[i], strlen(arg_a[i])+1, NULL);
         if(result){
            return result;
         }
      }else{
         return ENOMEM;
      }
   }

   argv[argc] = NULL;

   /* Open the file. */
   result = vfs_open(program_path, O_RDONLY, 0, &v);
   if (result) {
      return result;
   }


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

   /* Warp to user mode. */
   stackptr -= stackptr%4;
   stackptr -= sizeof(char*) * (argc+1);
   char ** args_u = (char**)stackptr;
   for(int i=0;i<argc;++i){
      char* arg = argv[i];
      stackptr -= strlen(arg)+1;
      result = copyoutstr(arg, (userptr_t)stackptr, strlen(arg)+1, NULL);
      if(result){
         return result;
      }
      args_u[i] = (char*) stackptr;
   }
   args_u[argc] = NULL;
   stackptr -= stackptr%8;
   enter_new_process(argc /*argc*/, (userptr_t)args_u /*userspace addr of argv*/,
           stackptr, entrypoint);

   /* enter_new_process does not return. */
   panic("enter_new_process returned\n");
   return EINVAL;
}

#endif
