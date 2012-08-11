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

#include "module.h"

#if 0
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

void
if1_fdc_reset( int hard )
{
  dbg( "called" );
}

void
if1_fdc_romcs( void )
{
  dbg( "called" );
}

static module_info_t if1_fdc_module = {
  .reset = if1_fdc_reset,
  .romcs = if1_fdc_romcs,
};

void
if1_fdc_init( void )
{
  module_register( &if1_fdc_module );
}
