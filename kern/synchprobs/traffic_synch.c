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
static struct lock *lock;
static struct cv *cv;

typedef struct Vehicles
{
  Direction origin;
  Direction destination;
} Vehicle;

typedef struct Intersections
{
  Vehicle volatile cars[10];
  int current_number = 0;
} Intersetion;

bool check_can_enter(Intersection, Vehicle);
bool if_collide(Vehicle, Vehicle);

bool right_turn(Vehicle);
void add_car_to_intersection(Intersection, Vehicle);
void remove_car_from_intersection(Intersection, Vehicle);

bool
right_turn(Vehicle v) {
  KASSERT(v != NULL);
  if (((v.origin == west) && (v.destination == south)) ||
      ((v.origin == south) && (v.destination == east)) ||
      ((v.origin == east) && (v.destination == north)) ||
      ((v.origin == north) && (v.destination == west))) {
    return true;
  } else {
    return false;
  }
}

bool
check_can_enter(Intersection i, Vheicle v){
  for(int i=0;i<10;++i){
    if(i.cars[i]!=NULL){
      if(!if_collide(i.cars[i], v))
        return false;
    }
  }
  return true;
}

bool
if_collide(Vehicle v1, Vehecle v2){
  if(v1.origin==v2.origin){
    return true;
  }else if(v1.origin==v2.destination&&v1.destination==v2.origin){
    return true;
  }else if(v1.destination!=v2.destination&&(right_turn(v1)||right_turn(v2))){
    return true;
  }
  return false;
}

Intersection volatile i;
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
  if (lock == NULL) {
    panic("could not create lock");
  }
  if (cv == NULL) {
    panic("could not create cv");
  }

  for(int i=0; i<10;++i){
    i.cars[i] = NULL;
  }

  return;
}

void 
add_car_to_intersection(Intersection i, Vehicle v){
  i.[current_number] = v;
  ++current_number;
}

void
remove_car_from_intersection(Intersection i, Direction o, Direction d){
  for(int i=0;i<10;++i){
    if(i.cars[i]!=NULL&&i.cars[i].origin==o&&i.cars[i].destination==d){
      i.cars[i] = NULL;
      break;
    }
  }
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
  Vehicle v;
  v.origin = origin;
  v.destination = destination;
  lock_acquire(lock);
  while(check_can_enter(i,v)){
    cv_wait(cv, lock);
  }
  add_car_to_intersection(i, v);
  lock_release(lock);
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
  remove_car_from_intersection(i, origin, destination);
  cv_broadcast(cv, lock);
  lock_release(lock);
}
