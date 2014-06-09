/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

struct semaphore * sem_male;
struct semaphore * sem_female;
struct semaphore * sem_matchmaker;

void whalemating_init() {
	sem_male = sem_create("male",0);
	sem_female = sem_create("female",0);
	sem_matchmaker = sem_create("matchmaker",0);
  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {
	sem_destroy(sem_male);
	sem_destroy(sem_female);
	sem_destroy(sem_matchmaker);
  return;
}

void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;

  male_start();
	// Implement this function
  P(sem_male);

  male_end();

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;

  female_start();
	// Implement this function 
  P(sem_female);

  female_end();

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;

  matchmaker_start();
	// Implement this function 
  V(sem_matchmaker);
  V(sem_male);
  V(sem_female);

  P(sem_matchmaker);
  matchmaker_end();

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car passing
 * straight through the intersection entering from direction X will leave to
 * direction (X + 2) % 4 and pass through quadrants X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

struct semaphore *quad0, *quad1, *quad2, *quad3;
struct lock *lk;

void stoplight_init() {
	quad0 = sem_create("Quadrant 0",1);
	quad1 = sem_create("Quadrant 1",1);
	quad2 = sem_create("Quadrant 2",1);
	quad3 = sem_create("Quadrant 3",1);
	lk = lock_create("Intersection Lock");
  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {
	sem_destroy(quad0);
	sem_destroy(quad1);
	sem_destroy(quad2);
	sem_destroy(quad3);
	lock_destroy(lk);
  return;
}

struct semaphore *
getQuad(unsigned long direction) {
	if(direction == 0)
		return quad0;
	else if(direction == 1)
		return quad1;
	else if(direction == 2)
		return quad2;
	else
		return quad3;
}

void
gostraight(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  //(void)direction;
  struct semaphore *dir, *dir1, *dir2;
  long newDir1, newDir2;

  newDir1 = (direction + 3) % 4;
  newDir2 = (direction + 2) % 4;
  dir = getQuad(direction);
  dir1 = getQuad(newDir1);
  dir2 = getQuad(newDir2);
  int got = 0;

  repeat: lock_acquire(lk);
  if(dir->sem_count == 1 && dir1->sem_count == 1 && dir2->sem_count == 1) {
	  P(dir);
	  P(dir1);
	  P(dir2);
	  got = 1;
  }
  lock_release(lk);

  while(got == 1) {
  inQuadrant(direction);

  //lock_acquire(lk);
  inQuadrant(newDir1);
  V(dir);
  //lock_acquire(lk);

  //lock_acquire(lk);
  inQuadrant(newDir2);
  V(dir1);
  //lock_release(lk);

  //lock_acquire(lk);
  leaveIntersection();
  V(dir2);
  got = 0;
  goto done;
  //lock_release(lk);
  }
  goto repeat;
  done:
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnleft(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
	//(void)direction;
	struct semaphore *dir, *dir1;
	long newDir1;

	newDir1 = direction == 3 ? 0 : (direction + 1);
	dir = getQuad(direction);
	dir1 = getQuad(newDir1);
	int got = 0;

	repeat: lock_acquire(lk);
	if(dir->sem_count == 1 && dir1->sem_count == 1) {
		P(dir);
		P(dir1);
		got = 1;
	}
	lock_release(lk);

	while(got == 1) {
	inQuadrant(direction);

	//lock_acquire(lk);
	inQuadrant(newDir1);
	V(dir);
	//lock_release(lk);

	//lock_acquire(lk);
	leaveIntersection();
	V(dir1);
	got = 0;
	goto done;
	//lock_release(lk);
	}
	goto repeat;
	done:
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnright(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
	//(void)direction;
	struct semaphore *dir, *dir1;
	long newDir1;

	newDir1 = direction == 0 ? 3 : (direction - 1);
	dir = getQuad(direction);
	dir1 = getQuad(newDir1);
	int got = 0;

	repeat: lock_acquire(lk);
	if(dir->sem_count == 1 && dir1->sem_count == 1) {
		P(dir);
		P(dir1);
		got = 1;
	}
	lock_release(lk);

	while(got == 1) {
	inQuadrant(direction);

	//lock_acquire(lk);
	inQuadrant(newDir1);
	V(dir);
	//lock_release(lk);

	//lock_acquire(lk);
	leaveIntersection();
	V(dir1);
	got = 0;
	goto done;
	//lock_release(lk);
	}
	goto repeat;
	done:
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}
