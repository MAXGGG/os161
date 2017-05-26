#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
// static struct semaphore *intersectionSem;
static struct lock *cv_lock;
static struct cv *cv;
static struct lock *available_lock;


/*
 *arrary for availabilities 
 *0:N-W, 1:N-S, 2:N-E
 *3:W-N, 4:W-E, 5:W-S
 *6:S-W, 7:S-N, 8:S-E
 *9:E-S, 10:E-W, 11:E-N
 */
int volatile available[12] = {0};

int volatile disable_list[12][7] = {{4,6,7,9,10,-1,-1},
                                    {3,4,5,6,9,10,-1},
                                    {3,4,6,7,8,9,10},
                                    {1,2,6,7,9,10,11},
                                    {1,2,6,7,8,9,-1},
                                    {1,2,7,9,10,-1,-1},
                                    {0,1,2,3,4,9,10},
                                    {2,3,4,9,10,-1,-1},
                                    {1,2,3,4,10,-1,-1},
                                    {1,2,3,4,5,6,7},
                                    {0,1,2,3,6,7,-1},
                                    {1,3,4,6,7,-1,-1}};

int get_index(Direction, Direction);
void wait_for_available(int);
void enable_routes(int);
void disable_routes(int);

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  cv_lock = lock_create("cv_lock");
  available_lock = lock_create("available_lock");
  cv = cv_create("cv");
  if (available_lock == NULL) {
    panic("could not create available_lock lock");
  }
  if (cv_lock == NULL) {
    panic("could not create cv lock");
  }
  if (cv == NULL) {
    panic("could not create cv");
  }

  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(cv_lock != NULL);
  KASSERT(cv != NULL);
  KASSERT(available_lock != NULL);
  lock_destroy(cv_lock);
  lock_destroy(available_lock);
  cv_destroy(cv);
}

int
get_index(Direction origin, Direction destination){ 
  if(origin==north&&destination==west) return 0;
  if(origin==north&&destination==south) return 1;
  if(origin==north&&destination==east) return 2;
  if(origin==west &&destination==north) return 3;
  if(origin==west &&destination==east) return 4;
  if(origin==west &&destination==south) return 5;
  if(origin==south&&destination==west) return 6;
  if(origin==south&&destination==north) return 7;
  if(origin==south&&destination==east) return 8;
  if(origin==east &&destination==south) return 9;
  if(origin==east &&destination==west) return 10;
  if(origin==east &&destination==north) return 11;
  return -1;
}

void
wait_for_available(int index){
  (void)index;
  lock_acquire(cv_lock);
  while(available[0]>0)
    cv_wait(cv, cv_lock);
}

void
disable_routes(int index){
  (void)index;
  // for(int i=0;i<7;++i){
  //     if(disable_list[index][i]!=-1){
  //       available[disable_list[index][i]]++;
  //     }
  // }
  available[0] = 1;
  lock_release(cv_lock);
}

void
enable_routes(int index){
  (void)index;
  lock_acquire(cv_lock);
  // for(int i=0;i<7;++i){
  //     if(disable_list[index][i]!=-1){
  //       available[disable_list[index][i]]--;
  //     }
  // }
  available[0] = 0;
  cv_broadcast(cv,cv_lock);
  lock_release(cv_lock);
  // lock_acquire(cv_lock);
  // cv_broadcast(cv,cv_lock);
  // lock_release(cv_lock);
}

/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  KASSERT(cv_lock != NULL);
  KASSERT(cv != NULL);
  KASSERT(available_lock != NULL);
  int index = get_index(origin, destination);
  KASSERT(index != -1);
  wait_for_available(index);
  disable_routes(index);
  
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  KASSERT(cv_lock != NULL);
  KASSERT(cv != NULL);
  KASSERT(available_lock != NULL);
  int index = get_index(origin, destination);
  KASSERT(index != -1);
  enable_routes(index);
}

