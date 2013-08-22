/* cobra_fdc.h: CoBra uPD765 floppy disk controller
   Copyright (c) 1999-2011 Philip Kendall, Darren Salt
   Copyright (c) 2012-2013 Alex Badea

   $Id$

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   Alex: vamposdecampos@gmail.com

*/

#ifndef FUSE_COBRA_FDC_H
#define FUSE_COBRA_FDC_H

#include "peripherals/disk/fdd.h"

typedef enum cobra_drive_number {
	COBRA_DRIVE_A = 0,
	COBRA_DRIVE_B,
	COBRA_DRIVE_C,
	COBRA_DRIVE_D,
} cobra_drive_number;


int cobra_fdc_available;

void cobra_fdc_init(void);
int cobra_fdc_insert(cobra_drive_number which, const char *filename, int autoload);
fdd_t *cobra_get_fdd(cobra_drive_number which);

#endif				/* #ifndef FUSE_COBRA_FDC_H */
