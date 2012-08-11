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
#include "periph.h"
#include "settings.h"

#if 0
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

#define IF1_FDC_RAM_SIZE	1024    /* bytes */

int if1_fdc_available;

static int if1_fdc_memory_source;
static memory_page if1_fdc_memory_map_romcs[MEMORY_PAGES_IN_8K];

void
if1_fdc_reset( int hard )
{
  dbg( "called" );

  if1_fdc_available = 0;
  if( !periph_is_active( PERIPH_TYPE_INTERFACE1_FDC ) )
    return;

  if1_fdc_available = 1;
  dbg( "available" );
}

void
if1_fdc_romcs( void )
{
  dbg( "called; if1_active=%d", if1_active );
  if( !if1_active )
    return;
  memory_map_romcs_8k( 0x2000, if1_fdc_memory_map_romcs );
}

void
if1_fdc_activate( void )
{
  dbg( "called" );
}

static periph_port_t if1_fdc_ports[] = {
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
  libspectrum_byte *ram;

  if1_fdc_memory_source = memory_source_register( "If1 RAM" );
  ram = memory_pool_allocate_persistent( IF1_FDC_RAM_SIZE, 1 );

  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ ) {
    libspectrum_word addr = i << MEMORY_PAGE_SIZE_LOGARITHM;
    memory_page *page = &if1_fdc_memory_map_romcs[i];

    page->source = if1_fdc_memory_source;
    page->page = ram + ( addr % IF1_FDC_RAM_SIZE );
    page->writable = ! !( addr & 0x800 );
  }

  module_register( &if1_fdc_module );
  periph_register( PERIPH_TYPE_INTERFACE1_FDC, &if1_fdc_periph );
}
