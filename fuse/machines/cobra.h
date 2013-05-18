/* cobra.h: CoBra specific routines
   Copyright (c) 1999-2004 Philip Kendall
   Copyright (c) 2012 Alex Badea

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

#ifndef FUSE_COBRA_H
#define FUSE_COBRA_H

#include <libspectrum.h>

#include "machine.h"

int cobra_port_from_ula( libspectrum_word port );

int cobra_init( fuse_machine_info *machine );
void cobra_common_display_setup( void );
int cobra_common_reset( void );
int cobra_memory_map( void );

libspectrum_byte cobra_ula_read( libspectrum_word port, int *attached );
void cobra_ula_write( libspectrum_word port, libspectrum_byte b );
void rfsh_check_page( libspectrum_byte R7 );

#endif			/* #ifndef FUSE_COBRA_H */
