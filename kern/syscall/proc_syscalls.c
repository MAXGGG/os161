#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
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
  // lock_acquire(p->p_cv_lock);
  // while(parray_num(&p->p_children)>0){
  //     cv_wait(p->p_cv, p->p_cv_lock);
  // }
  // lock_release(p->p_cv_lock);
  KASSERT(p!=NULL);
  p->p_state = 1;
  p->p_exitcode = _MKWAIT_EXIT(exitcode);
  if(p->p_parent!=NULL){
    p->p_parent->waitdone= 2;
    p->p_parent->p_exitcode = _MKWAIT_EXIT(exitcode);
    p->p_parent->p_child_count--;
    lock_acquire(p->p_parent->p_cv_lock);
    cv_broadcast(p->p_parent->p_cv, p->p_parent->p_cv_lock);
    lock_release(p->p_parent->p_cv_lock);
  }
  #else
  (void)exitcode;
  #endif

// DEBUG(DB_EXEC, "sys exiting 2\n");
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
  struct proc* parent = curproc;
  struct proc *child = getProcessById(pid);
  if(child==NULL){
     return ECHILD;
  }
  if(child->p_parent!=parent){
     return ECHILD;
  }
  parent->waitdone = 1;
  lock_acquire(parent->p_cv_lock);
  while(parent->waitdone!=2){
     cv_wait(parent->p_cv, parent->p_cv_lock);
  }
  lock_release(parent->p_cv_lock);
  exitstatus = parent->p_exitcode;
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
   parray_add(&currentproc->p_children, newp, NULL);
   curproc->p_child_count++;

   struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
   memcpy(newtf, tf, sizeof(struct trapframe));

   int errno = thread_fork(curthread->t_name, newp, &enter_forked_process, newtf, 0);
   if(errno){
      return errno;
   }

   *retval = newp->p_id;

   return 0;
}
#endif
