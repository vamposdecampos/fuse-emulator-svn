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

static upd_fdc *cobra_fdc;
static upd_fdc_drive cobra_drives[COBRA_NUM_DRIVES];

void
cobra_fdc_reset( int hard )
{
  dbg( "called" );
  if( !periph_is_active( PERIPH_TYPE_COBRA_FDC ) )
    return;

  upd_fdc_master_reset( cobra_fdc );
  dbg( "active" );
}

void
cobra_fdc_set_intrq( upd_fdc *f )
{
}

void
cobra_fdc_reset_intrq( upd_fdc *f )
{
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
