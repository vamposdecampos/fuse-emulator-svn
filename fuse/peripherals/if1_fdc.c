/* if1_fdc.c: IF1 uPD765 floppy disk controller
   Copyright (c) 1999-2011 Philip Kendall, Darren Salt
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
#include "if1_fdc.h"

#include "compat.h"
#include "if1.h"
#include "memory.h"
#include "module.h"
#include "options.h"
#include "periph.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/disk/upd_fdc.h"
#include "settings.h"
#include "ui/ui.h"
#include "ui/uimedia.h"

#if 0
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#define dbgp(x...) dbg(x)
#else
#define dbg(x...)
#define dbgp(x...)
#endif

#define IF1_FDC_RAM_SIZE	1024    /* bytes */
#define IF1_NUM_DRIVES 2

int if1_fdc_available;

static upd_fdc *if1_fdc;
static upd_fdc_drive if1_drives[IF1_NUM_DRIVES];
static ui_media_drive_info_t if1_fdc_ui_drives[IF1_NUM_DRIVES];
static int deselect_event;

static int if1_fdc_memory_source;
static libspectrum_byte *if1_fdc_ram;
static memory_page if1_fdc_memory_map_romcs[MEMORY_PAGES_IN_16K];

int
if1_fdc_insert( if1_drive_number which, const char *filename, int autoload )
{
  if( which >= IF1_NUM_DRIVES ) {
    ui_error( UI_ERROR_ERROR, "if1_fdc_insert: unknown drive %d", which );
    fuse_abort(  );
  }

  return ui_media_drive_insert( &if1_fdc_ui_drives[ which ], filename, autoload );
}

fdd_t *
if1_fdc_get_fdd( if1_drive_number which )
{
  return &if1_drives[which].fdd;
}



void
if1_fdc_reset( int hard )
{
  const fdd_params_t *dt;
  int i;

  dbg( "called" );

  if1_fdc_available = 0;
  if( !periph_is_active( PERIPH_TYPE_INTERFACE1_FDC ) )
    return;

  dbg( "loading ROM" );

  /* (re)load ROM */
  if( machine_load_rom_bank( if1_fdc_memory_map_romcs, 0,
			     settings_current.rom_hc_interface1,
			     settings_default.rom_hc_interface1,
			     0x4000 ) )
    return;

  /* overlay RAM pages */
  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ ) {
    libspectrum_word addr = i << MEMORY_PAGE_SIZE_LOGARITHM;
    memory_page *page = &if1_fdc_memory_map_romcs[MEMORY_PAGES_IN_8K + i];

    if( !( addr & 0x800 ) )
      continue;

    page->source = if1_fdc_memory_source;
    page->page = if1_fdc_ram + ( addr % IF1_FDC_RAM_SIZE );
    page->writable = 1;
  }

  upd_fdc_master_reset( if1_fdc );

  dt = &fdd_params[option_enumerate_diskoptions_drive_if1_fdc_a_type(  ) + 1];  /* +1 => there is no `Disabled' */
  fdd_init( &if1_drives[0].fdd, FDD_SHUGART, dt, 1 );

  dt = &fdd_params[option_enumerate_diskoptions_drive_if1_fdc_b_type(  )];
  fdd_init( &if1_drives[1].fdd, dt->enabled ? FDD_SHUGART : FDD_TYPE_NONE, dt, 1 );

  if1_fdc_available = 1;
  dbg( "available" );
}

void
if1_fdc_romcs( void )
{
  dbg( "called; if1_active=%d if1_fdc_available=%d", if1_active, if1_fdc_available );
  if( !if1_active || !if1_fdc_available )
    return;
  memory_map_romcs_16k( 0x0000, if1_fdc_memory_map_romcs );
}

void
if1_fdc_activate( void )
{
  dbg( "called" );
}


libspectrum_byte
if1_fdc_status( libspectrum_word port GCC_UNUSED, int *attached )
{
  libspectrum_byte ret;

  *attached = 1;
  ret = upd_fdc_read_status( if1_fdc );
  dbgp( "port 0x%02x --> 0x%02x", port & 0xff, ret );
  return ret;
}

libspectrum_byte
if1_fdc_read( libspectrum_word port GCC_UNUSED, int *attached )
{
  libspectrum_byte ret;

  *attached = 1;
  ret = upd_fdc_read_data( if1_fdc );
  dbgp( "port 0x%02x --> 0x%02x", port & 0xff, ret );
  return ret;
}

void
if1_fdc_write( libspectrum_word port GCC_UNUSED, libspectrum_byte data )
{
  dbgp( "port 0x%02x <-- 0x%02x", port & 0xff, data );
  upd_fdc_write_data( if1_fdc, data );
}

libspectrum_byte
if1_fdc_sel_read( libspectrum_word port GCC_UNUSED, int *attached )
{
  libspectrum_byte ret;
  int i;

  *attached = 1;
  ret = 0xff;
  for( i = 0; i < IF1_NUM_DRIVES; i++ )
    if( if1_drives[i].fdd.motoron )
      ret &= ~1;
  dbgp( "port 0x%02x --> 0x%02x", port & 0xff, ret );
  return ret;
}

void
if1_fdc_sel_write( libspectrum_word port GCC_UNUSED, libspectrum_byte data )
{
  libspectrum_byte armed;

  dbg( "port 0x%02x <-- 0x%02x", port & 0xff, data );

  if( !( data & 0x10 ) ) {
    dbg( "FDC reset" );
    upd_fdc_master_reset( if1_fdc );
  }
  upd_fdc_tc( if1_fdc, data & 1 );

  event_remove_type( deselect_event );

  armed = data & 0x08;
  if( armed ) {
    fdd_select( &if1_drives[0].fdd, armed && ( data & 0x02 ) );
    fdd_select( &if1_drives[1].fdd, armed && ( data & 0x04 ) );
    fdd_motoron( &if1_drives[0].fdd, armed && ( data & 0x02 ) );
    fdd_motoron( &if1_drives[1].fdd, armed && ( data & 0x04 ) );
  }
  else {
    /* The IF1 contains a 555 timer configured as a retriggerable
     * monostable.  It uses a 470 kOhm resistor and 3.3 uF capacitor.
     * This should give a delay of approximately 1.7 seconds.
     */
    event_add( tstates + 17 * machine_current->timings.processor_speed / 10, deselect_event );
  }

  ui_statusbar_update( UI_STATUSBAR_ITEM_DISK, armed ? UI_STATUSBAR_STATE_ACTIVE : UI_STATUSBAR_STATE_INACTIVE );
}

static void
if1_deselect_cb( libspectrum_dword last_tstates, int type, void *user_data )
{
  dbg( "called" );
  fdd_select( &if1_drives[0].fdd, 0 );
  fdd_select( &if1_drives[1].fdd, 0 );
  fdd_motoron( &if1_drives[0].fdd, 0 );
  fdd_motoron( &if1_drives[1].fdd, 0 );
}

static periph_port_t if1_fdc_ports[] = {
  {0x00fd, 0x0005, if1_fdc_sel_read, if1_fdc_sel_write},
  {0x00ff, 0x0085, if1_fdc_status, NULL},
  {0x00ff, 0x0087, if1_fdc_read, if1_fdc_write},
  {0, 0, NULL, NULL,},
};

static periph_t if1_fdc_periph = {
  .option = &settings_current.interface1_fdc,
  .ports = if1_fdc_ports,
  .activate = if1_fdc_activate,
};

static module_info_t if1_fdc_module = {
  .reset = if1_fdc_reset,
  .romcs = if1_fdc_romcs,
};

void
if1_fdc_init( void )
{
  int i;

  if1_fdc_memory_source = memory_source_register( "If1 RAM" );
  if1_fdc_ram = memory_pool_allocate_persistent( IF1_FDC_RAM_SIZE, 1 );

  if1_fdc = upd_fdc_alloc_fdc( UPD765A, UPD_CLOCK_8MHZ );
  if1_fdc->drive[0] = &if1_drives[0];
  if1_fdc->drive[1] = &if1_drives[1];
  if1_fdc->drive[2] = &if1_drives[0];
  if1_fdc->drive[3] = &if1_drives[1];

  fdd_init( &if1_drives[0].fdd, FDD_SHUGART, &fdd_params[4], 0 );
  fdd_init( &if1_drives[1].fdd, FDD_SHUGART, NULL, 0 ); /* drive geometry 'autodetect' */
  if1_fdc->set_intrq = NULL;
  if1_fdc->reset_intrq = NULL;
  if1_fdc->set_datarq = NULL;
  if1_fdc->reset_datarq = NULL;

  deselect_event = event_register( if1_deselect_cb, "IF1 FDC deselect" );

  module_register( &if1_fdc_module );
  periph_register( PERIPH_TYPE_INTERFACE1_FDC, &if1_fdc_periph );

  for( i = 0; i < IF1_NUM_DRIVES; i++ ) {
    if1_fdc_ui_drives[ i ].fdd = &if1_drives[ i ].fdd;
    if1_fdc_ui_drives[ i ].disk = &if1_drives[ i ].disk;
    ui_media_drive_register( &if1_fdc_ui_drives[ i ] );
  }
}

static int
ui_drive_is_available( void )
{
  return if1_fdc_available;
}

static const fdd_params_t *
ui_drive_get_params_1( void )
{
  return &fdd_params[ option_enumerate_diskoptions_drive_if1_fdc_a_type() + 1 ];	/* +1 => there is no `Disabled' */
}

static const fdd_params_t *
ui_drive_get_params_2( void )
{
  return &fdd_params[ option_enumerate_diskoptions_drive_if1_fdc_b_type() ];
}

static ui_media_drive_info_t if1_fdc_ui_drives[ IF1_NUM_DRIVES ] = {
  {
    .name = "HC Interface 1/Drive 1",
    .controller_index = UI_MEDIA_CONTROLLER_IF1_FDC,
    .drive_index = IF1_DRIVE_1,
    .menu_item_parent = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC,
    .menu_item_top = UI_MENU_ITEM_INVALID,
    .menu_item_eject = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_A_EJECT,
    .menu_item_flip = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_A_FLIP_SET,
    .menu_item_wp = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_A_WP_SET,
    .is_available = &ui_drive_is_available,
    .get_params = &ui_drive_get_params_1,
  },
  {
    .name = "HC Interface 1/Drive 2",
    .controller_index = UI_MEDIA_CONTROLLER_IF1_FDC,
    .drive_index = IF1_DRIVE_2,
    .menu_item_parent = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC,
    .menu_item_top = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B,
    .menu_item_eject = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B_EJECT,
    .menu_item_flip = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B_FLIP_SET,
    .menu_item_wp = UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B_WP_SET,
    .is_available = &ui_drive_is_available,
    .get_params = &ui_drive_get_params_2,
  },
};

