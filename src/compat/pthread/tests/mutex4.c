/*
 * mutex4.c
 *
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *
 *      Based upon Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 *
 *      Contact Email: rpj@callisto.canberra.edu.au
 *
 *      The original list of contributors to the Pthreads-win32 project
 *      is contained in the file CONTRIBUTORS.ptw32 included with the
 *      source code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 * --------------------------------------------------------------------------
 *
 * Thread A locks mutex - thread B tries to unlock.
 *
 * Depends on API functions:
 *	pthread_mutex_lock()
 *	pthread_mutex_trylock()
 *	pthread_mutex_unlock()
 */

//#include <stdio.h>
//#include <stdlib.h>

#include "test.h"

static int wasHere = 0;

static pthread_mutex_t mutex1;

void * unlocker(void * arg)
{
  extern int printf (const char * s, ...);
  int expectedResult = (int) arg;
  
  wasHere++;
  
  int res =pthread_mutex_unlock(&mutex1);
  assert(res == expectedResult);
  wasHere++;
  return NULL;
}

int
pthread_test_mutex4()
{
  pthread_t t;
  pthread_mutexattr_t ma;

  assert(pthread_mutexattr_init(&ma) == 0);

  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_DEFAULT) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  /*
   * NORMAL (fast) mutexes don't check ownership.
   */
  assert(pthread_create(&t, NULL, unlocker, (void *) 0) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == EPERM);
  assert(wasHere == 2);
  assert(pthread_mutex_destroy(&mutex1) == 0);

  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_NORMAL) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  /*
   * NORMAL (fast) mutexes don't check ownership.
   */
  assert(pthread_create(&t, NULL, unlocker, (void *) 0) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == EPERM);
  assert(wasHere == 2);
  assert(pthread_mutex_destroy(&mutex1) == 0);


  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_ERRORCHECK) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void *) EPERM) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == 0);
  assert(wasHere == 2);
  assert(pthread_mutex_destroy(&mutex1) == 0);

  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void *) EPERM) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == 0);
  assert(wasHere == 2);
  assert(pthread_mutex_destroy(&mutex1) == 0);

  return 0;
}
