#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <array.h>
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
static struct lock *lock;
static struct cv *cv;
static struct array* all_cars;
static struct array* cars_in;

typedef struct Vehicle
{
  Direction origin;
  Direction destination;
} Vehicle;


bool check_can_enter(Vehicle*);
bool if_collide(Vehicle*, Vehicle*);

bool right_turn(Vehicle*);
void remove_car_from_intersection(Direction, Direction);

bool
right_turn(Vehicle* v) {
  KASSERT(v != NULL);
  if (((v->origin == west) && (v->destination == south)) ||
      ((v->origin == south) && (v->destination == east)) ||
      ((v->origin == east) && (v->destination == north)) ||
      ((v->origin == north) && (v->destination == west))) {
    return true;
  } else {
    return false;
  }
}

bool
check_can_enter(Vehicle* v){
  for(unsigned i=0;i<array_num(cars_in);++i){
    Vehicle* in = array_get(cars_in, i);
    if(!if_collide(in, v))
      return false;
  }
  return true;
}

bool
if_collide(Vehicle* v1, Vehicle* v2){
  if(v1->origin==v2->origin){
    return true;
  }else if(v1->origin==v2->destination&&v1->destination==v2->origin){
    return true;
  }else if(v1->destination!=v2->destination&&(right_turn(v1)||right_turn(v2))){
    return true;
  }
  return false;
}

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

  lock = lock_create("lock");
  cv = cv_create("cv");
  all_cars = array_create();
  cars_in = array_create();
  if (lock == NULL) {
    panic("could not create lock");
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
  KASSERT(lock != NULL);
  KASSERT(cv != NULL);
  array_cleanup(all_cars);
  array_cleanup(cars_in);
  lock_destroy(lock);
  cv_destroy(cv);
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
  /* replace this default implementation with your own implementation */
  KASSERT(cv != NULL);
  KASSERT(lock != NULL);
  Vehicle* v = kmalloc(sizeof(struct Vehicle));
  v->origin = origin;
  v->destination = destination;
  int dummy;
  array_add(all_cars, v, dummy);
  lock_acquire(lock);
  while(check_can_enter(v)){
    cv_wait(cv, lock);
  }
  array_add(cars_in, v, dummy);
  lock_release(lock);
}


void
remove_car_from_intersection(Direction o, Direction d){
  for(unsigned i=0; i<array_num(cars_in);++i){
    if(cars_in[i]->origin==o&&cars_in[i]->destination==d){
      array_remove(cars_in, i);
      break;
    }
  }
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
  /* replace this default implementation with your own implementation */
  KASSERT(cv != NULL);
  KASSERT(lock != NULL);
  lock_acquire(lock);
  remove_car_from_intersection(origin, destination);
  cv_broadcast(cv, lock);
  lock_release(lock);
}
