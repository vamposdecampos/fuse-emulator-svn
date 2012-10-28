/* spec48.c: Spectrum 48K specific routines
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
#include "hc2000.h"
#include "spectrum.h"

#if 0
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

static memory_page hc2000_memory_map_cpm[MEMORY_PAGES_IN_16K];

static int hc2000_reset( void );

int hc2000_init( fuse_machine_info *machine )
{
  machine->machine = LIBSPECTRUM_MACHINE_48;
  machine->id = "hc2000";

  machine->reset = hc2000_reset;

  machine->timex = 0;
  machine->ram.port_from_ula         = spec48_port_from_ula;
  machine->ram.contend_delay	     = spectrum_contend_delay_65432100;
  machine->ram.contend_delay_no_mreq = spectrum_contend_delay_65432100;
  machine->ram.valid_pages	     = 3;

  machine->unattached_port = spectrum_unattached_port;

  machine->shutdown = NULL;

  machine->memory_map = hc2000_memory_map;

  return 0;
}

static int
hc2000_reset( void )
{
  int error;

  error = machine_load_rom( 0, settings_current.rom_hc2000_0,
                            settings_default.rom_hc2000_0, 0x4000 );
  if( error ) return error;

  error = machine_load_rom_bank( hc2000_memory_map_cpm, 0,
                                 settings_current.rom_hc2000_1,
                                 settings_default.rom_hc2000_1, 0x4000 );
  if( error ) return error;

  periph_clear();
  machines_periph_48();
  periph_set_present( PERIPH_TYPE_INTERFACE1, PERIPH_PRESENT_ALWAYS );
  periph_set_present( PERIPH_TYPE_INTERFACE1_FDC, PERIPH_PRESENT_ALWAYS );
  periph_update();

  beta_builtin = 0;

  memory_current_screen = 5;
  memory_screen_mask = 0xffff;

  spec48_common_display_setup();

  return spec48_common_reset();
}

/*
 * System configuration port at 0x7e:
 *  D0 - B/OL
 *       ROM source select (0=Basic, 1=CP/M).  This simply controls the A14
 *       address line of a 32KB ROM chip.
 *  D1 - CPMPL (complement of CPMP)
 *       Address decoding logic switch (0=standard, 1=CP/M).  When enabled:
 *       * ROM is selected for accesses to 0xe000..0xffff (instead of 0..0x3fff)
 *       * ROMCS override on the extension header is ignored (?)
 *  D2 - lockout bit (1=ignore further writes to port)
 *  D3 - CVS
 *       Video memory address select (0=0x4000, 1=0xc000).  This controls an
 *       address line output from the video generator to DRAM., i.e. the video
 *       generator will actually read from a different memory area.
 *
 * CPM signal (set when writing to port 0xc7, cleared when writing to 0xc5 and
 * on system reset); when enabled:
 *  * pulls A13 high when CPU accesses memory in the 0xc000..0xdfff area
 *    (this effectively maps the 0xe000 RAM in at 0xc000)
 */

int
hc2000_memory_map( void )
{
  memory_map_16k( 0x0000, memory_map_rom, 0 );
  memory_romcs_map();
  return 0;
}
