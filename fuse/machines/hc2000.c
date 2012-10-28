/* hc2000.c: Spectrum 48K specific routines
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

#include "compat.h"
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

static int hc2000_reset( void );

int
hc2000_port_from_ula( libspectrum_word port GCC_UNUSED )
{
  /* No contended ports */
  return 0;
}

int hc2000_init( fuse_machine_info *machine )
{
  machine->machine = LIBSPECTRUM_MACHINE_HC2000;
  machine->id = "hc2000";

  machine->reset = hc2000_reset;

  machine->timex = 0;
  machine->ram.port_from_ula         = hc2000_port_from_ula;
  machine->ram.contend_delay	     = spectrum_contend_delay_65432100;
  machine->ram.contend_delay_no_mreq = spectrum_contend_delay_65432100;
  machine->ram.valid_pages	     = 4;

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

  error = machine_load_rom( 1, settings_current.rom_hc2000_1,
                            settings_default.rom_hc2000_1, 0x4000 );
  if( error ) return error;

  periph_clear();
  machines_periph_48();
  periph_set_present( PERIPH_TYPE_INTERFACE1, PERIPH_PRESENT_ALWAYS );
  periph_set_present( PERIPH_TYPE_INTERFACE1_FDC, PERIPH_PRESENT_ALWAYS );
  periph_set_present( PERIPH_TYPE_HC2000_MEMORY, PERIPH_PRESENT_ALWAYS );
  periph_update();

  beta_builtin = 0;

  memory_current_screen = 5;
  memory_screen_mask = 0xffff;
  machine_current->ram.locked = 0;
  machine_current->ram.last_byte = 0;
  machine_current->ram.last_byte2 = 0xc5;

  spec48_common_display_setup();

  error = spec48_common_reset();
  if( error )
    return error;

  memory_ram_set_16k_contention( 0, 0 );
  memory_ram_set_16k_contention( 1, 0 );
  memory_ram_set_16k_contention( 2, 0 );
  memory_ram_set_16k_contention( 3, 0 );

  return 0;
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
hc2000_memory_map( void )
{
  int rom, rom_page, cpm_page, vid_page;
  int screen;

  rom = !!(machine_current->ram.last_byte & 0x01); /* B/OL */
  rom_page = !!(machine_current->ram.last_byte & 0x02); /* CPMPL */
  vid_page = !!(machine_current->ram.last_byte & 0x08); /* CVS */
  cpm_page = machine_current->ram.last_byte2 == 0xc7; /* CPM */
  dbg("rom=%d rom_page=%d vid_page=%d cpm_page=%d", rom, rom_page, vid_page, cpm_page);

  if( !rom_page )
    memory_map_16k( 0x0000, memory_map_rom, rom );
  else
    memory_map_16k( 0x0000, memory_map_ram, 0 );
  memory_map_16k( 0x4000, memory_map_ram, 1 );
  memory_map_16k( 0x8000, memory_map_ram, 2 );
  memory_map_16k( 0xc000, memory_map_ram, 3 );

  if( cpm_page )
    memory_map_16k_subpage( 0xc000, memory_map_ram, 3, 1 );
  if( rom_page )
    memory_map_16k_subpage( 0xe000, memory_map_rom, rom, 1 );

  screen = vid_page ? 3 : 1;
  if( memory_current_screen != screen ) {
    memory_current_screen = screen;
    display_update_critical( 0, 0 );
    display_refresh_main_screen();
  }

  if( !rom_page )
    memory_romcs_map();
  return 0;
}

libspectrum_byte hc2000_config_read(libspectrum_word port, int *attached)
{
  dbg("value=0x%02x", machine_current->ram.last_byte);
  *attached = 1;
  return machine_current->ram.last_byte;
}

void hc2000_config_write(libspectrum_word port, libspectrum_byte b)
{
  dbg("%scfg=0x%02x", machine_current->ram.locked ? "**LOCKED** " : "", b);
  if( machine_current->ram.locked )
    return;
  machine_current->ram.last_byte = b;
  machine_current->ram.locked = b & 0x04;
  machine_current->memory_map();
}

void hc2000_memoryport_write(libspectrum_word port, libspectrum_byte b)
{
  dbg("port=0x%04x value=0x%02x", port, b);
  machine_current->ram.last_byte2 = port & 0xff;
  machine_current->memory_map();
}

