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

#include "debugger/debugger.h"
#include "machine.h"
#include "machines_periph.h"
#include "memory.h"
#include "periph.h"
#include "peripherals/disk/beta.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/disk/upd_fdc.h"
#include "settings.h"
#include "spec48.h"
#include "cobra.h"
#include "spectrum.h"

#if 1
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

struct cobra_ctc {
  libspectrum_byte control_word;
  libspectrum_byte time_constant;
  libspectrum_byte vector;
  libspectrum_byte counter;
  unsigned trigger_pin:1;
  unsigned zc:1;
};

static int cobra_reset( void );
static int cobra_fdc_init( void );
static void cobra_fdc_set_intrq( upd_fdc *f );
static void cobra_fdc_reset_intrq( upd_fdc *f );

static int page_event, unpage_event;

#define COBRA_NUM_DRIVES 4
static upd_fdc_drive cobra_drives[COBRA_NUM_DRIVES];
static struct cobra_ctc cobra_ctc[4];

int cobra_init( fuse_machine_info *machine )
{
  periph_register_paging_events( "cobra", &page_event, &unpage_event );

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

  return cobra_fdc_init();
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
  periph_set_present( PERIPH_TYPE_COBRA_MEMORY, PERIPH_PRESENT_ALWAYS );

  beta_builtin = 0;

  memory_current_screen = 5;
  memory_screen_mask = 0xffff;
  machine_current->ram.last_byte = 0;
  machine_current->ram.last_byte2 = 0x80;

  periph_update();
  spec48_common_display_setup();


  upd_fdc_drive *d;
  d = &cobra_drives[0];

  if (d->disk.type) {
    d->disk.type = DISK_LOG;
    disk_write( &d->disk, "/tmp/disk.log" );
    d->disk.type = DISK_SAD;
    disk_write( &d->disk, "/tmp/disk.sad" );
  }

{
    int error = disk_open( &d->disk, "/tmp/d.td0", 0, 0 );
//    int error = disk_open( &d->disk, "/tmp/d.sad", 0, 0 );
    dbg( "disk_open: %d", error );
}
  fdd_load( &d->fdd, &d->disk, 0 );
  fdd_motoron( &d->fdd, 1 );

  //d->disk.type = DISK_LOG;
  //disk_write( &d->disk, "/tmp/disk.log" );

  d->disk.type = DISK_MGT;


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

static void
memory_ram_set_16k_writable( int page_num, int writable )
{
  int i;

  for( i = 0; i < MEMORY_PAGES_IN_16K; i++ )
    memory_map_ram[ page_num * MEMORY_PAGES_IN_16K + i ].writable = writable;
}

int
cobra_memory_map( void )
{
  /* PO and LO6 signals on the circuit board */
  int po = machine_current->ram.last_byte2 & 0x80;
  int lo6 = machine_current->ram.last_byte2 & 0x40;

  dbg( "last_byte2=0x%02x", machine_current->ram.last_byte2 );
  machine_current->ram.special = po || lo6;

  if( machine_current->ram.special ) {
    memory_ram_set_16k_writable( 10, 1 );
    if( po ) {
      memory_map_16k( 0x0000, memory_map_rom, 0 );
      memory_map_16k( 0x4000, memory_map_rom, 1 );
    } else {
      memory_map_16k( 0x0000, memory_map_ram, 2 );
      memory_map_16k( 0x4000, memory_map_ram, 0 );
    }
    memory_map_16k_subpage( 0x8000, memory_map_ram, 10, 0 );
    memory_map_16k_subpage( 0xa000, memory_map_ram, 5, 1 );
    memory_map_16k_subpage( 0xc000, memory_map_ram, 5, 0 );
    memory_map_16k_subpage( 0xe000, memory_map_ram, 10, 1 );
  } else {
    memory_ram_set_16k_writable( 10, 0 );
    memory_map_16k( 0x0000, memory_map_ram, 10 );
    memory_map_16k( 0x4000, memory_map_ram, 5 );
    memory_map_16k( 0x8000, memory_map_ram, 2 );
    memory_map_16k( 0xc000, memory_map_ram, 0 );
  }
  return 0;
}

libspectrum_byte cobra_ula_read( libspectrum_word port, int *attached )
{
  if(( port & 0xff ) != 0xfe )
    dbg("port 0x%02x read", port & 0xff);
  return 0xff;
}

void cobra_ula_write( libspectrum_word port, libspectrum_byte b )
{
  dbg( "port 0x%02x <- 0x%02x", port & 0xff, b );
  if(( port & 0xff ) == 0xfe )
    dbg( "%s%s%s%s%s",
      (b & (1 << 3)) ? "TO " : "   ",
      (b & (1 << 4)) ? "LS " : "   ",
      (b & (1 << 5)) ? "O5 " : "   ",
      (b & (1 << 6)) ? "O6 " : "   ",
      (b & (1 << 7)) ? "SO " : "   "
    );
  machine_current->ram.last_byte = b;
}

void rfsh_check_page( libspectrum_byte R7 )
{
  dbg( "R7=%d", R7 & 0x80 );
  debugger_event(( R7 & 0x80 ) ? page_event : unpage_event );
  machine_current->ram.last_byte2 = R7 & 0x80;
  /* on the falling edge of R7, latch current value of port FE bit 6 */
  if( !( R7 & 0x80 ))
    machine_current->ram.last_byte2 |= machine_current->ram.last_byte & 0x40;
  machine_current->memory_map();
}

/*
 * i8272 FDC
 */

static upd_fdc *cobra_fdc;

static int cobra_fdc_init()
{
  cobra_fdc = upd_fdc_alloc_fdc( UPD765A, UPD_CLOCK_8MHZ );
  cobra_fdc->drive[0] = &cobra_drives[0];
  cobra_fdc->drive[1] = &cobra_drives[1];
  cobra_fdc->drive[2] = &cobra_drives[2];
  cobra_fdc->drive[3] = &cobra_drives[3];
  cobra_fdc->set_intrq = cobra_fdc_set_intrq;
  cobra_fdc->reset_intrq = cobra_fdc_reset_intrq;
  cobra_fdc->set_datarq = NULL;
  cobra_fdc->reset_datarq = NULL;

  fdd_init( &cobra_drives[0].fdd, FDD_SHUGART, &fdd_params[4], 0 );
  fdd_init( &cobra_drives[1].fdd, FDD_TYPE_NONE, NULL, 0 ); /* drive geometry 'autodetect' */
  fdd_init( &cobra_drives[2].fdd, FDD_TYPE_NONE, NULL, 0 ); /* drive geometry 'autodetect' */
  fdd_init( &cobra_drives[3].fdd, FDD_TYPE_NONE, NULL, 0 ); /* drive geometry 'autodetect' */

  upd_fdc_master_reset( cobra_fdc );

  return 0;
}

libspectrum_byte cobra_mach_fdc_status( libspectrum_word port, int *attached )
{
  libspectrum_byte ret;
  static libspectrum_byte last = 0;

  *attached = 1;
  ret = upd_fdc_read_status( cobra_fdc );
  if( last != ret ) {
    dbg( "port 0x%02x --> 0x%02x", port & 0xff, ret );
    last = ret;
  }
  return ret;
}

libspectrum_byte cobra_mach_fdc_read( libspectrum_word port, int *attached )
{
  libspectrum_byte ret;

  *attached = 1;
  ret = upd_fdc_read_data( cobra_fdc );
  dbg( "port 0x%02x --> 0x%02x", port & 0xff, ret );
  return ret;
}

void cobra_mach_fdc_write( libspectrum_word port, libspectrum_byte b )
{
  dbg( "port 0x%02x <-- 0x%02x", port & 0xff, b );
  upd_fdc_write_data( cobra_fdc, b );
}

/*
 * Z80-CTC
 *
 * CLK/TRG0 <- i8272 INT
 * CLK/TRG1 <- ZC0
 * CLK/TRG2 <- ZC1
 * i8272 TC <- ZC2
 * CLK/TRG3 <- TRG3 (frame interrupt?)
 *
 * => channels <2(MSB), 1, 0(LSB)> count i8272 data bytes
 */

libspectrum_byte cobra_mach_ctc_read( libspectrum_word port, int *attached )
{
  int channel = (port >> 3) & 3;
  struct cobra_ctc *ctc = &cobra_ctc[channel];
  if (ctc->counter != 0xff) // HACK
    dbg("ctc port 0x%02x read: channel %d counter %d #################################################", port & 0xff, channel, ctc->counter);
  *attached = 1;
  return ctc->counter;
}

/* control word bits */
#define Z80_CTC_CONTROL_INTR_EN		0x80	/* interrupt enable */
#define Z80_CTC_CONTROL_COUNTER_MODE	0x40	/* counter mode; timer mode if clear */
#define Z80_CTC_CONTROL_PRESCALER_256	0x20	/* /256 prescaler; /16 if clear */
#define Z80_CTC_CONTROL_RISING_EDGE	0x10	/* trigger on rising edge */
#define Z80_CTC_CONTROL_WAIT_TRG	0x08	/* autostart timer if clear */
#define Z80_CTC_CONTROL_TIME_CONST	0x04	/* time constant follows */
#define Z80_CTC_CONTROL_RESET		0x02	/* software reset */
#define Z80_CTC_CONTROL_CONTROL		0x01	/* control word; vector if clear */


void cobra_mach_ctc_write( libspectrum_word port, libspectrum_byte b )
{
  int channel = (port >> 3) & 3;
  struct cobra_ctc *ctc = &cobra_ctc[channel];
  dbg( "%p ctc port 0x%02x chan %d <- 0x%02x", ctc, port & 0xff, channel, b );
  if( ctc->control_word & Z80_CTC_CONTROL_TIME_CONST ) {
    ctc->control_word &= ~ Z80_CTC_CONTROL_TIME_CONST;
//    if (channel == 2 && (b == 17 || b == 18))
//    	b -= 1; // HACK!

    ctc->time_constant = b;
    ctc->counter = ctc->time_constant;
    ctc->zc = ctc->counter == 0;
    dbg("channel %d time constant 0x%x / %d", channel, b, b);
//    if( channel == 2 && cobra_fdc )
//      upd_fdc_tc( cobra_fdc, cobra_ctc[2].zc );
  } else if ( b & Z80_CTC_CONTROL_CONTROL ) {
    ctc->control_word = b;
    dbg( "channel %d %s%s%s%s%s%s%s%s", channel,
      (b & 0x80) ? "int " : "",
      (b & 0x40) ? "counter " : "timer ",
      (b & 0x20) ? "/256 " : "/16 ",
      (b & 0x10) ? "rising " : "falling ",
      (b & 0x08) ? " " : "auto-trigger ",
      (b & 0x04) ? "time-constant " : "",
      (b & 0x02) ? "reset " : "",
      (b & 0x01) ? "control " : "vector "
    );
  } else {
    ctc->vector = b;
    dbg("channel %d vector 0x%02x", channel, b);
  }
}

static void ctc_trigger( struct cobra_ctc *ctc, int trigger )
{
  if( trigger && !ctc->trigger_pin ) {
    ctc->counter--;
    dbg( "%p new counter: %d", ctc, ctc->counter );
    ctc->zc = ctc->counter == 0;
    if (ctc->zc) {
      ctc->counter = ctc->time_constant;
      dbg( "%p ZC: new counter: %d", ctc, ctc->counter );
    }
  }
  ctc->trigger_pin = trigger;
}

static void cobra_fdc_set_intrq( upd_fdc *f )
{
  ctc_trigger( &cobra_ctc[0], 1 );
  if( cobra_ctc[0].zc ) {
    ctc_trigger( &cobra_ctc[1], 1 );
    ctc_trigger( &cobra_ctc[1], 0 );
  }
  if( cobra_ctc[1].zc ) {
    ctc_trigger( &cobra_ctc[2], 1 );
    ctc_trigger( &cobra_ctc[2], 0 );
  }
//  ctc_trigger( &cobra_ctc[1], cobra_ctc[0].zc );
//  ctc_trigger( &cobra_ctc[2], cobra_ctc[1].zc );
  if( f )
    upd_fdc_tc( f, cobra_ctc[2].zc );
  if( cobra_ctc[2].zc )
    dbg("########## TC! ############");
}

static void cobra_fdc_reset_intrq( upd_fdc *f )
{
  ctc_trigger( &cobra_ctc[0], 0 );
}
