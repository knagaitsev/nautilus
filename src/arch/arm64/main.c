/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#define __NAUTILUS_MAIN__

/*
 * The purpose of both a main and an init is somewhat unclear to me (symbol relocation distances might be an answer)
 * But I know for certain we cannot include "nautilus/nautilus.h" here or else nautilus_info will be redefined
 * because of the weird way that it is defined within the header
 */

#include <arch/arm64/init.h>

void
main (unsigned long dtb_raw, unsigned long x1, unsigned long x2, unsigned long x3)
{
    init(dtb_raw, x1, x2, x3);
}
