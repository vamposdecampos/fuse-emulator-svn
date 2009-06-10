#!/usr/bin/perl -w

# accessor.pl: generate accessor functions
# Copyright (c) 2003-2009 Philip Kendall

# $Id$

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Author contact information:

# E-mail: philip-fuse@shadowmagic.org.uk

use strict;

print << "CODE";
/* snap_accessors.c: simple accessor functions for libspectrum_snap
   Copyright (c) 2003-2009 Philip Kendall

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

   E-mail: philip-fuse\@shadowmagic.org.uk

*/

/* NB: this file is autogenerated from snap_accessors.txt by accessor.pl */

#include <config.h>

#include "internals.h"

struct libspectrum_snap {

  /* Which machine are we using here? */

  libspectrum_machine machine;

  /* Registers and the like */

  libspectrum_byte a , f ; libspectrum_word bc , de , hl ;
  libspectrum_byte a_, f_; libspectrum_word bc_, de_, hl_;

  libspectrum_word ix, iy; libspectrum_byte i, r;
  libspectrum_word sp, pc;

  libspectrum_byte iff1, iff2, im;

  int halted;			/* Is the Z80 currently HALTed? */
  int last_instruction_ei;	/* Was the last instruction an EI? */

  /* Custom ROM */
  int custom_rom;
  size_t custom_rom_pages;
  libspectrum_byte* roms[ 4 ];
  size_t rom_length[ 4 ];

  /* RAM */

  libspectrum_byte *pages[ SNAPSHOT_RAM_PAGES ];

  /* Data from .slt files */

  libspectrum_byte *slt[ SNAPSHOT_SLT_PAGES ];	/* Level data */
  size_t slt_length[ SNAPSHOT_SLT_PAGES ];	/* Length of each level */

  libspectrum_byte *slt_screen;	/* Loading screen */
  int slt_screen_level;		/* The id of the loading screen. Not used
				   for anything AFAIK, but I\'ll copy it
				   around just in case */

  /* Peripheral status */

  libspectrum_byte out_ula; libspectrum_dword tstates;

  libspectrum_byte out_128_memoryport;

  libspectrum_byte out_ay_registerport, ay_registers[16];

  /* Used for both the +3\'s and the Scorpion\'s 0x1ffd port */
  libspectrum_byte out_plus3_memoryport;

  /* Timex-specific bits */
  libspectrum_byte out_scld_hsr, out_scld_dec;

  /* Interface 1 status */
  int interface1_active;
  int interface1_paged;
  int interface1_drive_count;
  int interface1_custom_rom;
  libspectrum_byte* interface1_rom[1];
  size_t interface1_rom_length[1];	/* Length of the ROM */

  /* Betadisk status */
  int beta_active;
  int beta_paged;
  int beta_custom_rom;
  int beta_direction;	/* FDC seek direction:
			      zero => towards lower cylinders (hubwards)
			  non-zero => towards higher cylinders (rimwards) */
  libspectrum_byte beta_system, beta_track, beta_sector, beta_data,
    beta_status;
  libspectrum_byte *beta_rom[1];

  /* Plus D status */
  int plusd_active;
  int plusd_paged;
  int plusd_custom_rom;
  int plusd_direction;	/* FDC seek direction:
			      zero => towards lower cylinders (hubwards)
			  non-zero => towards higher cylinders (rimwards) */
  libspectrum_byte plusd_control, plusd_track, plusd_sector, plusd_data,
    plusd_status;
  libspectrum_byte *plusd_rom[1];
  libspectrum_byte *plusd_ram[1];

  /* ZXATASP status */
  int zxatasp_active;
  int zxatasp_upload;
  int zxatasp_writeprotect;
  libspectrum_byte zxatasp_port_a, zxatasp_port_b, zxatasp_port_c;
  libspectrum_byte zxatasp_control;
  size_t zxatasp_pages;
  size_t zxatasp_current_page;
  libspectrum_byte *zxatasp_ram[ SNAPSHOT_ZXATASP_PAGES ];

  /* ZXCF status */
  int zxcf_active;
  int zxcf_upload;
  libspectrum_byte zxcf_memctl;
  size_t zxcf_pages;
  libspectrum_byte *zxcf_ram[ SNAPSHOT_ZXCF_PAGES ];

  /* Interface II cartridge */
  int interface2_active;
  libspectrum_byte *interface2_rom[1];

  /* Timex Dock cartridge */
  int dock_active;
  libspectrum_byte exrom_ram[ SNAPSHOT_DOCK_EXROM_PAGES ];
  libspectrum_byte *exrom_cart[ SNAPSHOT_DOCK_EXROM_PAGES ];
  libspectrum_byte dock_ram[ SNAPSHOT_DOCK_EXROM_PAGES ];
  libspectrum_byte *dock_cart[ SNAPSHOT_DOCK_EXROM_PAGES ];

  /* Keyboard emulation */
  int issue2;

  /* Joystick emulation */
  size_t joystick_active_count;
  libspectrum_joystick joystick_list[ SNAPSHOT_JOYSTICKS ];
  int joystick_inputs[ SNAPSHOT_JOYSTICKS ];

  /* Kempston mouse status */
  int kempston_mouse_active;

  /* Simple 8-bit IDE status */
  int simpleide_active;

  /* DivIDE status */
  int divide_active;
  int divide_eprom_writeprotect;
  int divide_paged;
  libspectrum_byte divide_control;
  size_t divide_pages;
  libspectrum_byte* divide_eprom[ 1 ];
  libspectrum_byte* divide_ram[ SNAPSHOT_DIVIDE_PAGES ];

  /* Fuller box status */
  int fuller_box_active;

  /* Melodik status */
  int melodik_active;
};

/* Initialise a libspectrum_snap structure */
libspectrum_snap*
libspectrum_snap_alloc_internal( void )
{
  return libspectrum_malloc( sizeof( libspectrum_snap ) );
}
CODE

while(<>) {

    next if /^\s*$/;
    next if /^\s*#/;

    my( $type, $name, $indexed ) = split;

    if( $indexed ) {

	print << "CODE";

$type
libspectrum_snap_$name( libspectrum_snap *snap, int idx )
{
  return snap->$name\[idx\];
}

void
libspectrum_snap_set_$name( libspectrum_snap *snap, int idx, $type $name )
{
  snap->$name\[idx\] = $name;
}
CODE

    } else {

	print << "CODE";

$type
libspectrum_snap_$name( libspectrum_snap *snap )
{
  return snap->$name;
}

void
libspectrum_snap_set_$name( libspectrum_snap *snap, $type $name )
{
  snap->$name = $name;
}
CODE

    }
}
