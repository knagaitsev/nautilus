#include "wtime.h"
//#include <stdlib.h>
#include <nautilus/nautilus.h>
/*  Prototype  */
void wtime( double * );


/*****************************************************************/
/******         E  L  A  P  S  E  D  _  T  I  M  E          ******/
/*****************************************************************/
double elapsed_time( void )
{
    double t;

    wtime( &t );
    return( t );
}


double start[64], elapsed[64];
uint64_t start_cycles[64], elapsed_cycles[64];

/*****************************************************************/
/******            T  I  M  E  R  _  C  L  E  A  R          ******/
/*****************************************************************/
void timer_clear( int n )
{
    elapsed[n] = 0.0;
    elapsed_cycles[n] = 0.0;
}


/*****************************************************************/
/******            T  I  M  E  R  _  S  T  A  R  T          ******/
/*****************************************************************/
void timer_start( int n )
{
    start[n] = elapsed_time();
    start_cycles[n] = nk_sched_get_realtime();
}


/*****************************************************************/
/******            T  I  M  E  R  _  S  T  O  P             ******/
/*****************************************************************/
void timer_stop( int n )
{
    double t, now;

    uint64_t now_cycles = nk_sched_get_realtime();
    elapsed_cycles[n] += now_cycles - start_cycles[n];

    now = elapsed_time();
    t = now - start[n];
    elapsed[n] += t;

}


/*****************************************************************/
/******            T  I  M  E  R  _  R  E  A  D             ******/
/*****************************************************************/
uint64_t timer_read_cycles( int n )
{
    return( elapsed_cycles[n] );
}


double timer_read( int n )
{
    printk("\n\n\n\nTIMER READ: %d\n\n\n\n", timer_read_cycles(n));
    return( elapsed[n] );
}

