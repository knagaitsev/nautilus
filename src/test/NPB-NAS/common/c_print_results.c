/*****************************************************************/
/******     C  _  P  R  I  N  T  _  R  E  S  U  L  T  S     ******/
/*****************************************************************/
//#include <stdlib.h>
//#include <stdio.h>

#include<nautilus/nautilus.h>

void c_print_results( char   *name,
                      char   class,
                      int    n1, 
                      int    n2,
                      int    n3,
                      int    niter,
		      int    nthreads,
                      double t,
                      double mops,
		      char   *optype,
                      int    passed_verification,
                      char   *npbversion,
                      char   *compiletime,
                      char   *cc,
                      char   *clink,
                      char   *c_lib,
                      char   *c_inc,
                      char   *cflags,
                      char   *clinkflags,
		      char   *rand)
{
    char *evalue="1000";

    printk( "\n\n %s Benchmark Completed\n", name ); 

    printk( " Class           =                        %c\n", class );

    if( n2 == 0 && n3 == 0 )
        printk( " Size            =             %12d\n", n1 );   /* as in IS */
    else
        printk( " Size            =              %3dx%3dx%3d\n", n1,n2,n3 );

    printk( " Iterations      =             %12d\n", niter );
    
    printk( " Threads         =             %12d\n", nthreads );
 
    printk( " Time in seconds =             %12.2f\n", t );

    printk( " Mop/s total     =             %12.2f\n", mops );

    printk( " Operation type  = %24s\n", optype);

    if( passed_verification )
        printk( " Verification    =               SUCCESSFUL\n" );
    else
        printk( " Verification    =             UNSUCCESSFUL\n" );

    printk( " Version         =           %12s\n", npbversion );

    printk( " Compile date    =             %12s\n", compiletime );

    printk( "\n Compile options:\n" );

    printk( "    CC           = %s\n", cc );

    printk( "    CLINK        = %s\n", clink );

    printk( "    C_LIB        = %s\n", c_lib );

    printk( "    C_INC        = %s\n", c_inc );

    printk( "    CFLAGS       = %s\n", cflags );

    printk( "    CLINKFLAGS   = %s\n", clinkflags );

    printk( "    RAND         = %s\n", rand );
#ifdef SMP
    evalue = getenv("MP_SET_NUMTHREADS");
    printk( "   MULTICPUS = %s\n", evalue );
#endif

/*    printk( "\n\n" );
    printk( " Please send the results of this run to:\n\n" );
    printk( " NPB Development Team\n" );
    printk( " Internet: npb@nas.nasa.gov\n \n" );
    printk( " If email is not available, send this to:\n\n" );
    printk( " MS T27A-1\n" );
    printk( " NASA Ames Research Center\n" );
    printk( " Moffett Field, CA  94035-1000\n\n" );
    printk( " Fax: 415-604-3957\n\n" );*/
}
 
