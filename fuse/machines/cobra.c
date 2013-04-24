/* cobra.c: CoBra specific routines
   Copyright (c) 1999-2011 Philip Kendall
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

#include <config.h>

#include <stdio.h>

#include <libspectrum.h>

#include "machine.h"
#include "machines_periph.h"
#include "memory.h"
#include "periph.h"
#include "peripherals/disk/beta.h"
#include "settings.h"
#include "spec48.h"
#include "cobra.h"
#include "spectrum.h"

#if 0
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

static int cobra_reset( void );

int cobra_init( fuse_machine_info *machine )
{
  machine->machine = LIBSPECTRUM_MACHINE_COBRA;
  machine->id = "cobra";

  machine->reset = cobra_reset;

  machine->timex = 0;
  machine->ram.port_from_ula         = spec48_port_from_ula;
  machine->ram.contend_delay	     = spectrum_contend_delay_65432100;
  machine->ram.contend_delay_no_mreq = spectrum_contend_delay_65432100;
  machine->ram.valid_pages	     = 3;

  machine->unattached_port = spectrum_unattached_port;

  machine->shutdown = NULL;

  machine->memory_map = cobra_memory_map;

  return 0;
}

static int
cobra_reset( void )
{
  int error;

  error = machine_load_rom( 0, settings_current.rom_cobra_0,
                            settings_default.rom_cobra_0, 0x4000 );
  if( error ) return error;
  error = machine_load_rom( 1, settings_current.rom_cobra_1,
                            settings_default.rom_cobra_1, 0x4000 );
  if( error ) return error;

  periph_clear();
  machines_periph_48();
  periph_update();

  beta_builtin = 0;

  memory_current_screen = 5;
  memory_screen_mask = 0xffff;

  spec48_common_display_setup();

  return spec48_common_reset();
}

static void
memory_map_16k_subpage( libspectrum_word address, memory_page source[], int page_num, int half )
{
  int i;

  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ ) {
    int page = ( address >> MEMORY_PAGE_SIZE_LOGARITHM ) + i;
    memory_map_read[ page ] = memory_map_write[ page ] =
      source[ page_num * MEMORY_PAGES_IN_16K + i + half * MEMORY_PAGES_IN_8K ];
  }
}

int
cobra_memory_map( void )
{
  memory_map_16k( 0x0000, memory_map_rom, 0 );
  memory_map_16k( 0x4000, memory_map_rom, 1 );
  memory_map_16k_subpage( 0xa000, memory_map_ram, 0, 0 );
  memory_map_16k( 0xc000, memory_map_ram, 5 );
  memory_map_16k_subpage( 0xe000, memory_map_ram, 0, 1 );
  memory_romcs_map();
  return 0;
}

void rfsh_check_page( libspectrum_byte R7 )
{
  dbg( "R7=%d", R7 & 0x80 );
}
