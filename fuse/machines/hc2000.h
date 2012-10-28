/* hc2000.h: Spectrum 48K specific routines
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

#ifndef FUSE_HC2000_H
#define FUSE_HC2000_H

#include <libspectrum.h>

#include "machine.h"

int hc2000_port_from_ula( libspectrum_word port );

int hc2000_init( fuse_machine_info *machine );
int hc2000_memory_map( void );

libspectrum_byte hc2000_config_read(libspectrum_word port, int *attached);
void hc2000_config_write(libspectrum_word port, libspectrum_byte b);
void hc2000_memoryport_write(libspectrum_word port, libspectrum_byte b);

#endif			/* #ifndef FUSE_HC2000_H */
