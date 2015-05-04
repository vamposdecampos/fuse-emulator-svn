/* cobra_fdc.c: CoBra uPD765 floppy disk controller
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
#include <config.h>
#include <string.h>

#include "module.h"
#include "options.h"
#include "periph.h"
#include "peripherals/cobra_fdc.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/disk/upd_fdc.h"
#include "settings.h"
#include "ui/ui.h"
#include "ui/uimedia.h"
#include "z80/z80.h"

#if 1
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

#define COBRA_NUM_DRIVES 4

struct cobra_ctc {
  libspectrum_byte control_word;
  libspectrum_byte time_constant;
  libspectrum_byte vector;
  libspectrum_byte counter;
  unsigned trigger_pin:1;
  unsigned zc:1;
  unsigned intr:1;
};

static upd_fdc *cobra_fdc;
static upd_fdc_drive cobra_drives[COBRA_NUM_DRIVES];
static ui_media_drive_info_t cobra_ui_drives[ COBRA_NUM_DRIVES ];
static struct cobra_ctc cobra_ctc[4];
int cobra_fdc_available;
int ctc_int_event;

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

static libspectrum_byte
cobra_ctc_read( libspectrum_word port, int *attached )
{
  int channel = (port >> 3) & 3;
  struct cobra_ctc *ctc = &cobra_ctc[channel];

  dbg("ctc port 0x%02x read: channel %d counter %d", port & 0xff, channel, ctc->counter);
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


static void
cobra_ctc_write( libspectrum_word port, libspectrum_byte b )
{
  int channel = (port >> 3) & 3;
  struct cobra_ctc *ctc = &cobra_ctc[channel];
  dbg( "%p ctc port 0x%02x chan %d <- 0x%02x", ctc, port & 0xff, channel, b );
  if( ctc->control_word & Z80_CTC_CONTROL_TIME_CONST ) {
    ctc->control_word &= ~Z80_CTC_CONTROL_TIME_CONST;
    ctc->control_word &= ~Z80_CTC_CONTROL_RESET;

    ctc->time_constant = b;
    ctc->counter = ctc->time_constant;
    ctc->zc = 0; //ctc->counter == 0;
    ctc->intr = 0;
    dbg("channel %d time constant 0x%x / %d", channel, b, b);
    if( channel == 3 && ( ctc->control_word & Z80_CTC_CONTROL_INTR_EN ))
      machine_current->timings.interrupt_length = libspectrum_timings_interrupt_length( machine_current->machine );
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
    if( b & Z80_CTC_CONTROL_RESET ) {
      ctc->zc = 0;
      ctc->intr = 0;
    }
  } else {
    dbg("channel %d vector 0x%02x", channel, b);
    for( channel = 0; channel < 4; channel++ )
      cobra_ctc[ channel ].vector = b;
  }
}

static void
ctc_trigger( struct cobra_ctc *ctc, int trigger )
{
  if( ctc->control_word & Z80_CTC_CONTROL_RESET ) {
    dbg("XXXX channel %p in reset", ctc);
    ctc->trigger_pin = trigger;
    return;
  }
  if( trigger && !ctc->trigger_pin ) {
    ctc->counter--;
    dbg( "%p new counter: %d", ctc, ctc->counter );
    ctc->zc = ctc->counter == 0;
    if( ctc->zc ) {
      ctc->counter = ctc->time_constant;
      dbg( "%p ZC: new counter: %d", ctc, ctc->counter );
      if( ctc->control_word & Z80_CTC_CONTROL_INTR_EN ) {
        ctc->intr = 1;
      }
      event_add( tstates + 1, ctc_int_event );
      //event_add( tstates + 96, ctc_int_event );
    }
  }
  ctc->trigger_pin = trigger;
}

static void
ctc_int_fn( libspectrum_dword tstates, int type, void *user_data )
{
  int chan;

//  upd_fdc_tc( cobra_fdc, cobra_ctc[ 2 ].zc );

  for( chan = 0; chan < 4; chan++ ) {
    struct cobra_ctc *ctc = &cobra_ctc[ chan ];
    if( ctc->intr ) {
        libspectrum_byte vector = (ctc->vector & 0xf8) | (chan << 1);
        dbg( "%p interrupt! vector register 0x%02x, vector 0x%02x", ctc, ctc->vector, vector );
        if( !z80_interrupt_vector( vector ) ) {
          dbg("defer");
          event_add( tstates + 10, ctc_int_event );
        } else {
          dbg("accepted");
          ctc->intr = 0;
        }
    }
  }
}

static void
cobra_fdc_set_intrq( upd_fdc *f )
{
  int old_zc = cobra_ctc[2].zc;
  ctc_trigger( &cobra_ctc[0], 1 );
  if( cobra_ctc[0].zc ) {
    ctc_trigger( &cobra_ctc[1], 1 );
    ctc_trigger( &cobra_ctc[1], 0 );
  }
  if( cobra_ctc[1].zc ) {
    ctc_trigger( &cobra_ctc[2], 1 );
    ctc_trigger( &cobra_ctc[2], 0 );
  }
  if( f && !old_zc && cobra_ctc[2].zc )
    upd_fdc_tc( f, 1 );
}

static void
cobra_fdc_reset_intrq( upd_fdc *f )
{
  ctc_trigger( &cobra_ctc[0], 0 );
}


static int
cobra_fdc_frame_interrupt( void )
{
  /* only allow interrupts in Basic mode */
  if( machine_current->ram.special ) {
    ctc_trigger( &cobra_ctc[3], 1 );
    ctc_trigger( &cobra_ctc[3], 0 );
    fprintf(stderr, "INT!\n");
    return 1;
  }
  return machine_current->ram.special ? 1 : 0;
}


void
cobra_fdc_reset( int hard )
{
  const fdd_params_t *dt;
  int i;

  dbg( "called hard=%d", hard );
  cobra_fdc_available = 0;
  if( !periph_is_active( PERIPH_TYPE_COBRA_FDC ) )
    return;

  memset(cobra_ctc, 0, sizeof(cobra_ctc));
  upd_fdc_master_reset( cobra_fdc );

  for( i = 0; i < COBRA_NUM_DRIVES; i++ ) {
    ui_media_drive_info_t *drive = &cobra_ui_drives[ i ];
    dt = drive->get_params();
    fdd_motoron( drive->fdd, 0 );
    fdd_init( drive->fdd, dt->enabled ? FDD_SHUGART : FDD_TYPE_NONE, dt, 1 );
    fdd_motoron( drive->fdd, 1 );
    ui_media_drive_update_menus( drive, UI_MEDIA_DRIVE_UPDATE_ALL );
  }

  for(i = 0; i < 4; i++ ) {
    struct cobra_ctc *ctc = &cobra_ctc[ i ];
    memset(ctc, 0, sizeof(*ctc));
    ctc->control_word = Z80_CTC_CONTROL_RESET;
  }

  machine_current->frame_interrupt = cobra_fdc_frame_interrupt;
  cobra_fdc_available = 1;
  dbg( "active" );
}

int
cobra_fdc_insert( cobra_drive_number which, const char *filename, int autoload )
{
  if( which >= COBRA_NUM_DRIVES ) {
    ui_error( UI_ERROR_ERROR, "cobra_fdc_insert: unknown drive %d", which );
    fuse_abort(  );
  }

  return ui_media_drive_insert( &cobra_ui_drives[ which ], filename, autoload );
}

fdd_t *cobra_get_fdd(cobra_drive_number which)
{
  return &cobra_drives[which].fdd;
}

void
cobra_fdc_activate( void )
{
  dbg( "called" );
}

libspectrum_byte
cobra_fdc_status( libspectrum_word port, int *attached )
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

libspectrum_byte
cobra_fdc_read( libspectrum_word port, int *attached )
{
  libspectrum_byte ret;

  *attached = 1;
  ret = upd_fdc_read_data( cobra_fdc );
  dbg( "port 0x%02x --> 0x%02x", port & 0xff, ret );
  return ret;
}

void
cobra_fdc_write( libspectrum_word port, libspectrum_byte b )
{
  dbg( "port 0x%02x <-- 0x%02x", port & 0xff, b );
  upd_fdc_write_data( cobra_fdc, b );
}



static periph_port_t cobra_fdc_ports[] = {
  /* CPU A1 -> 8272 nCS;  CPU A3 -> 8272 A0 */
  { 0x000a, 0x0000, cobra_fdc_status, NULL },
  { 0x000a, 0x0008, cobra_fdc_read, cobra_fdc_write },
  /* CPU A2 -> Z80-CTC nCE */
  { 0x0004, 0x0000, cobra_ctc_read, cobra_ctc_write },
  { 0, 0, NULL, NULL }
};

static periph_t cobra_fdc_periph = {
  .option = &settings_current.cobra_fdc,
  .ports = cobra_fdc_ports,
  .activate = cobra_fdc_activate,
};


static module_info_t cobra_fdc_module = {
  .reset = cobra_fdc_reset,
};

void
cobra_fdc_init( void )
{
  int i;

  ctc_int_event = event_register( ctc_int_fn, "CoBra CTC interrupt" );

  cobra_fdc = upd_fdc_alloc_fdc( UPD765A, UPD_CLOCK_8MHZ );
  cobra_fdc->drive[0] = &cobra_drives[2];
  cobra_fdc->drive[1] = &cobra_drives[3];
  cobra_fdc->drive[2] = &cobra_drives[0];
  cobra_fdc->drive[3] = &cobra_drives[1];
  cobra_fdc->set_intrq = cobra_fdc_set_intrq;
  cobra_fdc->reset_intrq = cobra_fdc_reset_intrq;
  cobra_fdc->set_datarq = NULL;
  cobra_fdc->reset_datarq = NULL;

  for( i = 0; i < COBRA_NUM_DRIVES; i++ ) {
    fdd_init( &cobra_drives[ i ].fdd, FDD_SHUGART, NULL, 0 );
    cobra_ui_drives[ i ].fdd = &cobra_drives[ i ].fdd;
    cobra_ui_drives[ i ].disk = &cobra_drives[ i ].disk;
    ui_media_drive_register( &cobra_ui_drives[ i ] );
  }

  module_register( &cobra_fdc_module );
  periph_register( PERIPH_TYPE_COBRA_FDC, &cobra_fdc_periph );
}

static int
ui_drive_is_available( void )
{
  return cobra_fdc_available;
}

static const fdd_params_t *
ui_drive_get_params_a( void )
{
  return &fdd_params[ option_enumerate_diskoptions_drive_cobra_fdc_a_type() ];
}

static const fdd_params_t *
ui_drive_get_params_b( void )
{
  return &fdd_params[ option_enumerate_diskoptions_drive_cobra_fdc_b_type() ];
}

static const fdd_params_t *
ui_drive_get_params_c( void )
{
  return &fdd_params[ option_enumerate_diskoptions_drive_cobra_fdc_c_type() ];
}

static const fdd_params_t *
ui_drive_get_params_d( void )
{
  return &fdd_params[ option_enumerate_diskoptions_drive_cobra_fdc_d_type() ];
}

static int
ui_drive_inserted( const ui_media_drive_info_t *drive, int new, int loaded )
{
  if( loaded ) {
    fdd_motoron( drive->fdd, 0 );
    fdd_motoron( drive->fdd, 1 );
  }
  return 0;
}

static ui_media_drive_info_t cobra_ui_drives[ COBRA_NUM_DRIVES ] = {
  {
    .name = "CoBra/Drive A:",
    .controller_index = UI_MEDIA_CONTROLLER_COBRA,
    .drive_index = BETA_DRIVE_A,
    .menu_item_parent = UI_MENU_ITEM_MEDIA_DISK_COBRA,
    .menu_item_top = UI_MENU_ITEM_MEDIA_DISK_COBRA_A,
    .menu_item_eject = UI_MENU_ITEM_MEDIA_DISK_COBRA_A_EJECT,
    .menu_item_flip = UI_MENU_ITEM_MEDIA_DISK_COBRA_A_FLIP_SET,
    .menu_item_wp = UI_MENU_ITEM_MEDIA_DISK_COBRA_A_WP_SET,
    .is_available = &ui_drive_is_available,
    .get_params = &ui_drive_get_params_a,
    .insert_hook = &ui_drive_inserted,
  },
  {
    .name = "CoBra/Drive B:",
    .controller_index = UI_MEDIA_CONTROLLER_COBRA,
    .drive_index = BETA_DRIVE_B,
    .menu_item_parent = UI_MENU_ITEM_MEDIA_DISK_COBRA,
    .menu_item_top = UI_MENU_ITEM_MEDIA_DISK_COBRA_B,
    .menu_item_eject = UI_MENU_ITEM_MEDIA_DISK_COBRA_B_EJECT,
    .menu_item_flip = UI_MENU_ITEM_MEDIA_DISK_COBRA_B_FLIP_SET,
    .menu_item_wp = UI_MENU_ITEM_MEDIA_DISK_COBRA_B_WP_SET,
    .is_available = &ui_drive_is_available,
    .get_params = &ui_drive_get_params_b,
    .insert_hook = &ui_drive_inserted,
  },
  {
    .name = "CoBra/Drive C:",
    .controller_index = UI_MEDIA_CONTROLLER_COBRA,
    .drive_index = BETA_DRIVE_C,
    .menu_item_parent = UI_MENU_ITEM_MEDIA_DISK_COBRA,
    .menu_item_top = UI_MENU_ITEM_MEDIA_DISK_COBRA_C,
    .menu_item_eject = UI_MENU_ITEM_MEDIA_DISK_COBRA_C_EJECT,
    .menu_item_flip = UI_MENU_ITEM_MEDIA_DISK_COBRA_C_FLIP_SET,
    .menu_item_wp = UI_MENU_ITEM_MEDIA_DISK_COBRA_C_WP_SET,
    .is_available = &ui_drive_is_available,
    .get_params = &ui_drive_get_params_c,
    .insert_hook = &ui_drive_inserted,
  },
  {
    .name = "CoBra/Drive D:",
    .controller_index = UI_MEDIA_CONTROLLER_COBRA,
    .drive_index = BETA_DRIVE_D,
    .menu_item_parent = UI_MENU_ITEM_MEDIA_DISK_COBRA,
    .menu_item_top = UI_MENU_ITEM_MEDIA_DISK_COBRA_D,
    .menu_item_eject = UI_MENU_ITEM_MEDIA_DISK_COBRA_D_EJECT,
    .menu_item_flip = UI_MENU_ITEM_MEDIA_DISK_COBRA_D_FLIP_SET,
    .menu_item_wp = UI_MENU_ITEM_MEDIA_DISK_COBRA_D_WP_SET,
    .is_available = &ui_drive_is_available,
    .get_params = &ui_drive_get_params_d,
    .insert_hook = &ui_drive_inserted,
  },
};

