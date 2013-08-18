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
#include "settings.h"

#if 1
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

void
cobra_fdc_reset( int hard )
{
  dbg( "called" );
}

void
cobra_fdc_activate( void )
{
  dbg( "called" );
}

static periph_port_t cobra_fdc_ports[] = {
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
  module_register( &cobra_fdc_module );
  periph_register( PERIPH_TYPE_COBRA_FDC, &cobra_fdc_periph );
}
