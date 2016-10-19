#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>
//#include <stdlib.h>

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
 //use conditional variable for the three conditions, instead of semaphore:
//static struct semaphore *intersectionSem;
//copy of the vihecle structure avoid double decl

typedef struct simVehicles {
  Direction origin;
  Direction destination;
}simVehicles;

//Decl of var used
static struct cv* condvarTS;
static struct lock* lksTS;

//array and counter
struct array * arrV;
//volatile int ctr = 0; //used along with pointer indicating last position of viehcle

//resolve no previous prototype issue... (header file no struct)
bool rt_chkr (simVehicles* sim);
bool constraints(simVehicles * new, simVehicles* existed);
bool vehiclehlpr(simVehicles* new);

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
//replace the semlock with cv
  //intersectionSem = sem_create("intersectionSem",1);
  kprintf("start *************************************\n");
  lksTS = lock_create("lksTS");
  condvarTS = cv_create("condvarTS");
  //arrV = (struct simVehicles *) kmalloc (sizeof(simVehicles *)*100);
  arrV = array_create();
  array_init(arrV);
  if (condvarTS == NULL || lksTS == NULL|| arrV == NULL) {
    panic("could not create intersection condvar");
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
  //KASSERT(intersectionSem != NULL);
  //routine check
  KASSERT(lksTS!= NULL);
  KASSERT(condvarTS!= NULL);
  KASSERT(arrV != NULL);
  //using free mem functions free locks and built in to free vihecles
  lock_destroy(lksTS);
  cv_destroy (condvarTS);
  //kfree(arrV);
  array_destroy(arrV);
  arrV = NULL;
}


//hlpr fun for right turn
bool rt_chkr (simVehicles* sim){
  if (((sim->origin == north) && (sim->destination == west)) ||
      ((sim->origin == south) && (sim->destination == east)) ||
      ((sim->origin == east) && (sim->destination == north)) ||
      ((sim->origin == west) && (sim->destination == south))){
    return true;
  } else {
    return false;
  }
}

//constraints checker (3 rules) for viehcles
bool constraints(simVehicles * new, simVehicles* existed){
  if (new -> origin == existed-> origin){
    return true;
  } else if ((existed -> destination == new -> origin) && (new -> destination == existed-> origin)){
    return true;
  } else if ((new -> destination != existed -> destination) && (rt_chkr(new) || rt_chkr(existed))){
    return true;
  } else {
    return false;
  }
}

//a hlpr function 
bool vehiclehlpr(simVehicles* new){
  for (unsigned int i = 0; i < array_num(arrV); i++){
    if(!constraints(new, array_get(arrV,i))){
      //kprintf("putting to sleep\n");
      cv_wait(condvarTS, lksTS);
      return false;
    }
  }
  KASSERT(lock_do_i_hold(lksTS));
  //arrV[ctr] = *new;
  //kprintf("adding new \n");
  array_add(arrV,new,NULL);
  //++ctr;
  return true;
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
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //routine check
  KASSERT(lksTS != NULL);
  KASSERT(condvarTS != NULL);
  KASSERT(arrV != NULL);
  //acquire lock, create struct and then push in array
  lock_acquire(lksTS);
  simVehicles* temp = kmalloc(sizeof(struct simVehicles));
  KASSERT(temp != NULL);
  temp->origin = origin;
  temp-> destination = destination;
  //kprintf("start legal check\n");
  while(!vehiclehlpr(temp)){} // do nothing
  //kfree(temp);
  lock_release(lksTS);
  //P(intersectionSem);
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
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //routine check
  KASSERT(condvarTS != NULL);
  KASSERT(lksTS != NULL);
  KASSERT(arrV != NULL);
  //V(intersectionSem);
  //acquire lock, check for direction and which vehicle, and then shift all to front
  lock_acquire(lksTS);
  for (unsigned int i = 0; i < array_num(arrV); i++){
    simVehicles * temp = array_get(arrV,i);
    if((temp -> origin == origin) && (temp->destination == destination)){
      //shift everything to front
      /*for(int j = i; j < ctr-1; j++){
        arrV[j]= arrV[j+1];
      }*/
      //wakeup the everyone and recheck
      array_remove(arrV,i);
      //kprintf("waking up all\n");
      cv_broadcast(condvarTS,lksTS);
      //ctr--;
      break;
    }
  }
  lock_release(lksTS);
}
