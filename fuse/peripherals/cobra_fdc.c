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
#include "periph.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/disk/upd_fdc.h"
#include "settings.h"

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
};

static upd_fdc *cobra_fdc;
static upd_fdc_drive cobra_drives[COBRA_NUM_DRIVES];
static struct cobra_ctc cobra_ctc[4];

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

    ctc->time_constant = b;
    ctc->counter = ctc->time_constant;
    ctc->zc = ctc->counter == 0;
    dbg("channel %d time constant 0x%x / %d", channel, b, b);
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

static void
ctc_trigger( struct cobra_ctc *ctc, int trigger )
{
  if( trigger && !ctc->trigger_pin ) {
    ctc->counter--;
    dbg( "%p new counter: %d", ctc, ctc->counter );
    ctc->zc = ctc->counter == 0;
    if( ctc->zc ) {
      ctc->counter = ctc->time_constant;
      dbg( "%p ZC: new counter: %d", ctc, ctc->counter );
    }
  }
  ctc->trigger_pin = trigger;
}

static void
cobra_fdc_set_intrq( upd_fdc *f )
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
  if( f )
    upd_fdc_tc( f, cobra_ctc[2].zc );
}

static void
cobra_fdc_reset_intrq( upd_fdc *f )
{
  ctc_trigger( &cobra_ctc[0], 0 );
}


void
cobra_fdc_reset( int hard )
{
  dbg( "called hard=%d", hard );
  if( !periph_is_active( PERIPH_TYPE_COBRA_FDC ) )
    return;

  memset(cobra_ctc, 0, sizeof(cobra_ctc));
  upd_fdc_master_reset( cobra_fdc );

  dbg( "active" );
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

  module_register( &cobra_fdc_module );
  periph_register( PERIPH_TYPE_COBRA_FDC, &cobra_fdc_periph );
}
