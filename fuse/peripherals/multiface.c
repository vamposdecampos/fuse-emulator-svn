/* multiface.c: Multiface one/128/3 handling routines
   Copyright (c) 2005,2007 Gergely Szasz

   $Id$

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

   Author contact information:

   Gergely: szaszg@hu.inter.net
   
   Many thanks to: Mark Woodmass

*/

#include <config.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "event.h"
#include "multiface.h"
#include "memory.h"
#include "module.h"
#include "options.h"
#include "periph.h"
#include "settings.h"
#include "ui/ui.h"
#include "unittests/unittests.h"
#include "utils.h"
#include "z80/z80.h"

/* Two 8KB memory chunk accessible by the Z80 when /ROMCS is low */
static memory_page multiface_memory_map_romcs_rom[MEMORY_PAGES_IN_8K];
static memory_page multiface_memory_map_romcs_ram[MEMORY_PAGES_IN_8K];

static int romcs = 0;

typedef enum multiface_typ {
  MULTIFACE_ONE = 0,
  MULTIFACE_128,
  MULTIFACE_3,			/**/
} multiface_typ;

typedef struct multiface_t {
  int IC8a_Q;			/* IC8 74LS74 first Flip-flop /Q output*/
  int IC8b_Q;			/* IC8 74LS74 second Flip-flop /Q output */
  int J2;			/* Jumper 2 to disable software paging, or
				   the software on/off state for 128/3 */
  int J1;			/* Jumper 1 to disable joystick (always 0) */
  multiface_typ type;			/* type of multiface: one/128/3 */
  libspectrum_byte ram[8192];	/* 8k RAM */
} multiface_t;

static multiface_t mf;

int multiface_active = 0;
int multiface_activated = 0;
int multiface_available = 0;

static void multiface_page( void );
static void multiface_unpage( void );
static void multiface_reset( int hard_reset );
static void multiface_memory_map( void );
static libspectrum_byte multiface_port_in( libspectrum_word port, int *attached );
static void multiface_port_out( libspectrum_word port, libspectrum_byte val );
static libspectrum_byte multiface_port_in3( libspectrum_word port, int *attached );

static module_info_t multiface_module_info = {
  multiface_reset,
  multiface_memory_map,
  NULL,
  NULL,
  NULL
};

static const periph_port_t multiface_ports[] = {
  { 0x0052, 0x0012, multiface_port_in, multiface_port_out },
  { 0x1f3f, 0x1f3f, multiface_port_in3, NULL },
  { 0, 0, NULL, NULL }
};

static const periph_t multiface_periph = {
  &settings_current.multiface,
  multiface_ports,
  1,
  NULL
};

/* Memory source */
static int multiface_memory_source;

/* Debugger events */
static const char *event_type_string = "multiface";
static int page_event, unpage_event;


int
multiface_init( void )
{
  int i;

  multiface_active = 0;
  multiface_activated = 0;
  multiface_available = 0;

  module_register( &multiface_module_info );

  multiface_memory_source = memory_source_register( "Multiface" );
  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ )
    multiface_memory_map_romcs_rom[i].source = multiface_memory_source;
  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ )
    multiface_memory_map_romcs_ram[i].source = multiface_memory_source;

  periph_register( PERIPH_TYPE_MULTIFACE, &multiface_periph );
  periph_register_paging_events( event_type_string, &page_event,
				 &unpage_event );

  return 0;
}

static multiface_typ
multiface_get_type( void )
{
  const char *name[] = {"Multiface One", "Multiface 128", "Multiface +3"};
  multiface_typ t;

  switch( option_enumerate_peripherals_general_multiface_type() ) {
  case 0:
    t = MULTIFACE_ONE;
    break;
  case 2:
    t = MULTIFACE_3;
    break;
  default:
    t = MULTIFACE_128;
  }

  switch( machine_current->machine ) {
  case LIBSPECTRUM_MACHINE_128:
  case LIBSPECTRUM_MACHINE_PLUS2:
/*  case LIBSPECTRUM_MACHINE_TC2068: */
/*  case LIBSPECTRUM_MACHINE_TS2068: */
    if( t != MULTIFACE_128 ) {
      ui_error( UI_ERROR_WARNING, "`%s` not compatible with current machine, switching to `%s`",
	    name[t], name[MULTIFACE_128] );
      t = MULTIFACE_128;		/* fall back to Multiface 128 */
    }
    break;
  case LIBSPECTRUM_MACHINE_PLUS2A:
  case LIBSPECTRUM_MACHINE_PLUS3:
  case LIBSPECTRUM_MACHINE_PLUS3E:
/*  case LIBSPECTRUM_MACHINE_PENT: */
/*  case LIBSPECTRUM_MACHINE_PENT512: */
/*  case LIBSPECTRUM_MACHINE_PENT1024: */
/*  case LIBSPECTRUM_MACHINE_SCORP: */
  case LIBSPECTRUM_MACHINE_SE:
    if( t < MULTIFACE_3 ) {
      ui_error( UI_ERROR_WARNING, "`%s` not compatible with current machine, switching to `%s`",
	    name[t], name[MULTIFACE_3] );
      t = MULTIFACE_3;		/* fall back to Multiface 3 */
    }
    break;
  default:
    break;
  }
  return t;
}

static void
multiface_reset( int hard_reset )
{
  int i;

  multiface_unpage();

  multiface_activated = 0;
  multiface_available = 0;

  if( hard_reset ) memset( mf.ram, 0, 8192 );
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 0 );
  if( !periph_is_active( PERIPH_TYPE_MULTIFACE ) ) return;

  mf.IC8a_Q = 1;
  mf.IC8b_Q = 1;
  mf.J1 = 0;		/* Joystick always disabled :-( */
  mf.type = multiface_get_type();

  if( mf.type == MULTIFACE_ONE )
    mf.J2 = settings_current.multiface_stealth ? 0 : 1;
  else
    mf.J2 = 0;

  if( machine_load_rom_bank( multiface_memory_map_romcs_rom, 0,
	    mf.type == MULTIFACE_ONE ? settings_current.rom_multiface1 :
	      ( mf.type == MULTIFACE_128 ? settings_current.rom_multiface128 :
		    settings_current.rom_multiface3 ),
	    mf.type == MULTIFACE_ONE ? settings_default.rom_multiface1 :
	      ( mf.type == MULTIFACE_128 ? settings_default.rom_multiface128 :
		    settings_default.rom_multiface3 ),
	    0x2000 ) ) {

    settings_current.multiface = 0;
    periph_activate_type( PERIPH_TYPE_MULTIFACE, 0 );

    return;
  }

  machine_current->ram.romcs = 0;

  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ ) {
    multiface_memory_map_romcs_ram[ i ].page = &mf.ram[ i * MEMORY_PAGE_SIZE ];
    multiface_memory_map_romcs_ram[ i ].writable = 1;
  }

  if( mf.type != MULTIFACE_ONE )
    mf.J2 = 0;			/* stealth mode after reset */
  multiface_available = 1;
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 1 );

  return;
}

void
multiface_status_update( void )
{
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 0 );
  if( !periph_is_active( PERIPH_TYPE_MULTIFACE ) ) {
    multiface_available = 0;
    return;
  }
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 1 );
  if( mf.type == MULTIFACE_ONE && mf.J2 == settings_current.multiface_stealth ) {
    mf.J2 = settings_current.multiface_stealth ? 0 : 1;
  }
/*
  if( !multiface_available || mf.type != multiface_get_type() )
    multiface_reset( 0 );
*/
}

static void
multiface_page( void )
{
  if( multiface_active )
    return;
  multiface_active = 1;
  romcs = machine_current->ram.romcs;
  machine_current->ram.romcs = 1;
  machine_current->memory_map();
  if( mf.type != MULTIFACE_ONE )
    mf.J2 = 1;
}

static void
multiface_unpage( void )
{
  if( !multiface_active )
    return;
  multiface_active = 0;
  machine_current->ram.romcs = romcs;
  machine_current->memory_map();
}

static void
multiface_memory_map( void )
{
  if( !multiface_active )
    return;

  memory_map_romcs_8k( 0x0000, multiface_memory_map_romcs_rom );
  memory_map_romcs_8k( 0x2000, multiface_memory_map_romcs_ram );
}

static libspectrum_byte
multiface_port_in( libspectrum_word port, int *attached )
{
  libspectrum_byte ret = 0xff;
  int a7;

  if( !multiface_available ) return ret;

  if( mf.type == MULTIFACE_ONE && port & 0x20 )
    return ret;
  if( mf.type != MULTIFACE_ONE && !( port & 0x20 ) )
    return ret;
  if( mf.type == MULTIFACE_ONE && !mf.J2 )
    return ret;

  *attached = 1;

  /* in () */
  /* Multiface one */
  /* xxxxxxxx 1001xx1x page in  IN A, (159) 10011111*/
  /* xxxxxxxx 0001xx1x page out IN A, (31)  00011111*/

  /* Multiface 128 */
  /* I have only the MF128 user guide which say: */
  /*  IN A, (191) -> page in, and IN A, (63) page out */
  /*  let see: */
  /* xxxxxxxx 10111111 ok, may a0 don't care, so */
  /* xxxxxxxx 1011111x and may have some other don't care bit, but how know? */
  /**/
  /* xxxxxxxx 00111111 ok, may a0 don't care, so */
  /* xxxxxxxx 0011111x and may have some other don't care bit, but how know? */
  
  /* Multiface +3 */
  /* The MF3 user guide say nothing about paging memory :-( */
  /*  but in www.breezer.co.uk/spec/tech/hware.html I found: */
  /*  IN A, (191) -> page in, and IN A, (63) page out */
  /*  let see: */
  /* xxxxxxxx 10111111 ok, may a0 don't care, so */
  /* xxxxxxxx 1011111x and may have some other don't care bit, but how know? */
  /**/
  /* xxxxxxxx 00111111 ok, may a0 don't care, so */
  /* xxxxxxxx 0011111x and may have some other don't care bit, but how know? */

  a7 = port & 0x0080;
  switch( mf.type ) {
  case MULTIFACE_ONE:
    if( mf.J1 ) {
      /* READ joy */
    }
    if( a7 ) {
      multiface_page();
    } else {
      multiface_unpage();		/* a7 == 0 */
    }
    mf.IC8a_Q = ( !a7 );
    break;
  case MULTIFACE_128:
    if( a7 ) {
      if( mf.J2 ) {
        multiface_page();
        ret = machine_current->ram.last_byte & 0x08 ? 0xff : 0x7f;
        mf.IC8a_Q = 0;
      }
    } else {
      multiface_unpage();		/* a7 == 0 */
      mf.IC8a_Q = 1;
    }
    break;
  case MULTIFACE_3:
    if( a7 )			/* a7 == 1 */
      multiface_unpage();
    else
      multiface_page();		/* a7 == 0 */
    mf.IC8a_Q = ( !a7 );
    break;	
  }
  return ret;
}

static void
multiface_port_out( libspectrum_word port, libspectrum_byte val )
{
  int a7;

  if( !multiface_available ) return;

  /* MF one: out () */
  /*         xxxxxxxx x001xx1x page out */
  a7 = port & 0x0080;
  switch( mf.type ) {
  case MULTIFACE_ONE:
    mf.IC8b_Q = 1;
    break;
  case MULTIFACE_128:
  case MULTIFACE_3:
    if( multiface_active ) {
      mf.J2 = a7 ? 1 : 0;
    }
    mf.IC8b_Q = 1;
    break;
  }
}

static libspectrum_byte
multiface_port_in3( libspectrum_word port, int *attached )
{
  libspectrum_byte ret = 0xff;

  if( !multiface_available || !mf.J2 )
    return ret;

  *attached = 1;

  if( port & 0x4000 )		/* 7f3f */
    ret = machine_current->ram.last_byte;
  else
    ret =  machine_current->ram.last_byte2;
  return ret;
}

void
multiface_red_button( void )
{
  if( !multiface_available || mf.IC8b_Q == 0 ) return;

  mf.IC8b_Q = 0;
  multiface_activated = 1;
  event_add( 0, z80_nmi_event );	/* pull /NMI */
}

void
multiface_setic8( void )
{
  if( !multiface_available || mf.IC8b_Q == 1 ) return;

  mf.IC8a_Q = 0;
  multiface_activated = 0;
  multiface_page();
}

int
multiface_unittest( void )
{
  int r = 0;

  multiface_page();

  r += unittests_assert_8k_page( 0x0000, multiface_memory_source, 0 );
  r += unittests_assert_8k_page( 0x2000, multiface_memory_source, 0 );
  r += unittests_assert_16k_ram_page( 0x4000, 5 );
  r += unittests_assert_16k_ram_page( 0x8000, 2 );
  r += unittests_assert_16k_ram_page( 0xc000, 0 );

  multiface_unpage();

  r += unittests_paging_test_48( 2 );

  return r;
}


/*
static void
multiface_enabled_snapshot( libspectrum_snap *snap )
{
  if( libspectrum_snap_multiface_active( snap ) )
    settings_current.multiface = 1;
}

static void
multiface_from_snapshot( libspectrum_snap *snap )
{
  if( !libspectrum_snap_multiface_active( snap ) ) return;

  if( libspectrum_snap_multiface_custom_rom( snap ) &&
      libspectrum_snap_multiface_rom( snap, 0 ) &&
      machine_load_rom_bank_from_buffer(
                             mf1_memory_map_romcs, 0,
                             libspectrum_snap_multiface_rom( snap, 0 ),
                             libspectrum_snap_multiface_rom_length( snap, 0 ),
                             1 ) )
    return;

  if( libspectrum_snap_multiface_paged( snap ) ) {
    multiface_page();
  } else {
    multiface_unpage();
  }
}

static void
multiface_to_snapshot( libspectrum_snap *snap )
{
  libspectrum_byte *buffer;

  if( !periph_is_active( PERIPH_TYPE_INTERFACE1 ) ) return;

  libspectrum_snap_set_multiface_active( snap, 1 );
  libspectrum_snap_set_multiface_paged ( snap, multiface_active );
  libspectrum_snap_set_multiface_drive_count( snap, 8 );

  if( if1_memory_map_romcs[0].save_to_snapshot ) {
    size_t rom_length = MEMORY_PAGE_SIZE;

    if( if1_memory_map_romcs[1].save_to_snapshot ) {
      rom_length <<= 1;
    }

    libspectrum_snap_set_multiface_custom_rom( snap, 1 );
    libspectrum_snap_set_multiface_rom_length( snap, 0, rom_length );

    buffer = malloc( rom_length );
    if( !buffer ) {
      ui_error( UI_ERROR_ERROR, "Out of memory at %s:%d", __FILE__, __LINE__ );
      return;
    }

    memcpy( buffer, if1_memory_map_romcs[0].page, MEMORY_PAGE_SIZE );

    if( rom_length == MEMORY_PAGE_SIZE*2 ) {
      memcpy( buffer + MEMORY_PAGE_SIZE, multiface_memory_map_romcs[1].page,
              MEMORY_PAGE_SIZE );
    }

    libspectrum_snap_set_multiface_rom( snap, 0, buffer );
  }
}
*/
/* multiface.c: Multiface one/128/3 handling routines
   Copyright (c) 2005,2007 Gergely Szasz

   $Id$

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

   Author contact information:

   Gergely: szaszg@hu.inter.net
   
   Many thanks to: Mark Woodmass

*/

#include <config.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "event.h"
#include "multiface.h"
#include "memory.h"
#include "module.h"
#include "options.h"
#include "periph.h"
#include "settings.h"
#include "ui/ui.h"
#include "unittests/unittests.h"
#include "utils.h"
#include "z80/z80.h"

/* Two 8KB memory chunk accessible by the Z80 when /ROMCS is low */
static memory_page multiface_memory_map_romcs_rom[MEMORY_PAGES_IN_8K];
static memory_page multiface_memory_map_romcs_ram[MEMORY_PAGES_IN_8K];

static int romcs = 0;

typedef enum multiface_typ {
  MULTIFACE_ONE = 0,
  MULTIFACE_128,
  MULTIFACE_3,			/**/
} multiface_typ;

typedef struct multiface_t {
  int IC8a_Q;			/* IC8 74LS74 first Flip-flop /Q output*/
  int IC8b_Q;			/* IC8 74LS74 second Flip-flop /Q output */
  int J2;			/* Jumper 2 to disable software paging, or
				   the software on/off state for 128/3 */
  int J1;			/* Jumper 1 to disable joystick (always 0) */
  multiface_typ type;			/* type of multiface: one/128/3 */
  libspectrum_byte ram[8192];	/* 8k RAM */
} multiface_t;

static multiface_t mf;

int multiface_active = 0;
int multiface_activated = 0;
int multiface_available = 0;

static void multiface_page( void );
static void multiface_unpage( void );
static void multiface_reset( int hard_reset );
static void multiface_memory_map( void );
static libspectrum_byte multiface_port_in( libspectrum_word port, int *attached );
static void multiface_port_out( libspectrum_word port, libspectrum_byte val );
static libspectrum_byte multiface_port_in3( libspectrum_word port, int *attached );

static module_info_t multiface_module_info = {
  multiface_reset,
  multiface_memory_map,
  NULL,
  NULL,
  NULL
};

static const periph_port_t multiface_ports[] = {
  { 0x0052, 0x0012, multiface_port_in, multiface_port_out },
  { 0x1f3f, 0x1f3f, multiface_port_in3, NULL },
  { 0, 0, NULL, NULL }
};

static const periph_t multiface_periph = {
  &settings_current.multiface,
  multiface_ports,
  1,
  NULL
};

/* Memory source */
static int multiface_memory_source;

/* Debugger events */
static const char *event_type_string = "multiface";
static int page_event, unpage_event;


int
multiface_init( void )
{
  int i;

  multiface_active = 0;
  multiface_activated = 0;
  multiface_available = 0;

  module_register( &multiface_module_info );

  multiface_memory_source = memory_source_register( "Multiface" );
  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ )
    multiface_memory_map_romcs_rom[i].source = multiface_memory_source;
  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ )
    multiface_memory_map_romcs_ram[i].source = multiface_memory_source;

  periph_register( PERIPH_TYPE_MULTIFACE, &multiface_periph );
  periph_register_paging_events( event_type_string, &page_event,
				 &unpage_event );

  return 0;
}

static multiface_typ
multiface_get_type( void )
{
  const char *name[] = {"Multiface One", "Multiface 128", "Multiface +3"};
  multiface_typ t;

  switch( option_enumerate_peripherals_general_multiface_type() ) {
  case 0:
    t = MULTIFACE_ONE;
    break;
  case 2:
    t = MULTIFACE_3;
    break;
  default:
    t = MULTIFACE_128;
  }

  switch( machine_current->machine ) {
  case LIBSPECTRUM_MACHINE_128:
  case LIBSPECTRUM_MACHINE_PLUS2:
/*  case LIBSPECTRUM_MACHINE_TC2068: */
/*  case LIBSPECTRUM_MACHINE_TS2068: */
    if( t != MULTIFACE_128 ) {
      ui_error( UI_ERROR_WARNING, "`%s` not compatible with current machine, switching to `%s`",
	    name[t], name[MULTIFACE_128] );
      t = MULTIFACE_128;		/* fall back to Multiface 128 */
    }
    break;
  case LIBSPECTRUM_MACHINE_PLUS2A:
  case LIBSPECTRUM_MACHINE_PLUS3:
  case LIBSPECTRUM_MACHINE_PLUS3E:
/*  case LIBSPECTRUM_MACHINE_PENT: */
/*  case LIBSPECTRUM_MACHINE_PENT512: */
/*  case LIBSPECTRUM_MACHINE_PENT1024: */
/*  case LIBSPECTRUM_MACHINE_SCORP: */
  case LIBSPECTRUM_MACHINE_SE:
    if( t < MULTIFACE_3 ) {
      ui_error( UI_ERROR_WARNING, "`%s` not compatible with current machine, switching to `%s`",
	    name[t], name[MULTIFACE_3] );
      t = MULTIFACE_3;		/* fall back to Multiface 3 */
    }
    break;
  default:
    break;
  }
  return t;
}

static void
multiface_reset( int hard_reset )
{
  int i;

  multiface_unpage();

  multiface_activated = 0;
  multiface_available = 0;

  if( hard_reset ) memset( mf.ram, 0, 8192 );
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 0 );
  if( !periph_is_active( PERIPH_TYPE_MULTIFACE ) ) return;

  mf.IC8a_Q = 1;
  mf.IC8b_Q = 1;
  mf.J1 = 0;		/* Joystick always disabled :-( */
  mf.type = multiface_get_type();

  if( mf.type == MULTIFACE_ONE )
    mf.J2 = settings_current.multiface_stealth ? 0 : 1;
  else
    mf.J2 = 0;

  if( machine_load_rom_bank( multiface_memory_map_romcs_rom, 0,
	    mf.type == MULTIFACE_ONE ? settings_current.rom_multiface1 :
	      ( mf.type == MULTIFACE_128 ? settings_current.rom_multiface128 :
		    settings_current.rom_multiface3 ),
	    mf.type == MULTIFACE_ONE ? settings_default.rom_multiface1 :
	      ( mf.type == MULTIFACE_128 ? settings_default.rom_multiface128 :
		    settings_default.rom_multiface3 ),
	    0x2000 ) ) {

    settings_current.multiface = 0;
    periph_activate_type( PERIPH_TYPE_MULTIFACE, 0 );

    return;
  }

  machine_current->ram.romcs = 0;

  for( i = 0; i < MEMORY_PAGES_IN_8K; i++ ) {
    multiface_memory_map_romcs_ram[ i ].page = &mf.ram[ i * MEMORY_PAGE_SIZE ];
    multiface_memory_map_romcs_ram[ i ].writable = 1;
  }

  if( mf.type != MULTIFACE_ONE )
    mf.J2 = 0;			/* stealth mode after reset */
  multiface_available = 1;
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 1 );

  return;
}

void
multiface_status_update( void )
{
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 0 );
  if( !periph_is_active( PERIPH_TYPE_MULTIFACE ) ) {
    multiface_available = 0;
    return;
  }
  ui_menu_activate( UI_MENU_ITEM_MACHINE_MULTIFACE, 1 );
  if( mf.type == MULTIFACE_ONE && mf.J2 == settings_current.multiface_stealth ) {
    mf.J2 = settings_current.multiface_stealth ? 0 : 1;
  }
/*
  if( !multiface_available || mf.type != multiface_get_type() )
    multiface_reset( 0 );
*/
}

static void
multiface_page( void )
{
  if( multiface_active )
    return;
  multiface_active = 1;
  romcs = machine_current->ram.romcs;
  machine_current->ram.romcs = 1;
  machine_current->memory_map();
  if( mf.type != MULTIFACE_ONE )
    mf.J2 = 1;
}

static void
multiface_unpage( void )
{
  if( !multiface_active )
    return;
  multiface_active = 0;
  machine_current->ram.romcs = romcs;
  machine_current->memory_map();
}

static void
multiface_memory_map( void )
{
  if( !multiface_active )
    return;

  memory_map_romcs_8k( 0x0000, multiface_memory_map_romcs_rom );
  memory_map_romcs_8k( 0x2000, multiface_memory_map_romcs_ram );
}

static libspectrum_byte
multiface_port_in( libspectrum_word port, int *attached )
{
  libspectrum_byte ret = 0xff;
  int a7;

  if( !multiface_available ) return ret;

  if( mf.type == MULTIFACE_ONE && port & 0x20 )
    return ret;
  if( mf.type != MULTIFACE_ONE && !( port & 0x20 ) )
    return ret;
  if( mf.type == MULTIFACE_ONE && !mf.J2 )
    return ret;

  *attached = 1;

  /* in () */
  /* Multiface one */
  /* xxxxxxxx 1001xx1x page in  IN A, (159) 10011111*/
  /* xxxxxxxx 0001xx1x page out IN A, (31)  00011111*/

  /* Multiface 128 */
  /* I have only the MF128 user guide which say: */
  /*  IN A, (191) -> page in, and IN A, (63) page out */
  /*  let see: */
  /* xxxxxxxx 10111111 ok, may a0 don't care, so */
  /* xxxxxxxx 1011111x and may have some other don't care bit, but how know? */
  /**/
  /* xxxxxxxx 00111111 ok, may a0 don't care, so */
  /* xxxxxxxx 0011111x and may have some other don't care bit, but how know? */
  
  /* Multiface +3 */
  /* The MF3 user guide say nothing about paging memory :-( */
  /*  but in www.breezer.co.uk/spec/tech/hware.html I found: */
  /*  IN A, (191) -> page in, and IN A, (63) page out */
  /*  let see: */
  /* xxxxxxxx 10111111 ok, may a0 don't care, so */
  /* xxxxxxxx 1011111x and may have some other don't care bit, but how know? */
  /**/
  /* xxxxxxxx 00111111 ok, may a0 don't care, so */
  /* xxxxxxxx 0011111x and may have some other don't care bit, but how know? */

  a7 = port & 0x0080;
  switch( mf.type ) {
  case MULTIFACE_ONE:
    if( mf.J1 ) {
      /* READ joy */
    }
    if( a7 ) {
      multiface_page();
    } else {
      multiface_unpage();		/* a7 == 0 */
    }
    mf.IC8a_Q = ( !a7 );
    break;
  case MULTIFACE_128:
    if( a7 ) {
      if( mf.J2 ) {
        multiface_page();
        ret = machine_current->ram.last_byte & 0x08 ? 0xff : 0x7f;
        mf.IC8a_Q = 0;
      }
    } else {
      multiface_unpage();		/* a7 == 0 */
      mf.IC8a_Q = 1;
    }
    break;
  case MULTIFACE_3:
    if( a7 )			/* a7 == 1 */
      multiface_unpage();
    else
      multiface_page();		/* a7 == 0 */
    mf.IC8a_Q = ( !a7 );
    break;	
  }
  return ret;
}

static void
multiface_port_out( libspectrum_word port, libspectrum_byte val )
{
  int a7;

  if( !multiface_available ) return;

  /* MF one: out () */
  /*         xxxxxxxx x001xx1x page out */
  a7 = port & 0x0080;
  switch( mf.type ) {
  case MULTIFACE_ONE:
    mf.IC8b_Q = 1;
    break;
  case MULTIFACE_128:
  case MULTIFACE_3:
    if( multiface_active ) {
      mf.J2 = a7 ? 1 : 0;
    }
    mf.IC8b_Q = 1;
    break;
  }
}

static libspectrum_byte
multiface_port_in3( libspectrum_word port, int *attached )
{
  libspectrum_byte ret = 0xff;

  if( !multiface_available || !mf.J2 )
    return ret;

  *attached = 1;

  if( port & 0x4000 )		/* 7f3f */
    ret = machine_current->ram.last_byte;
  else
    ret =  machine_current->ram.last_byte2;
  return ret;
}

void
multiface_red_button( void )
{
  if( !multiface_available || mf.IC8b_Q == 0 ) return;

  mf.IC8b_Q = 0;
  multiface_activated = 1;
  event_add( 0, z80_nmi_event );	/* pull /NMI */
}

void
multiface_setic8( void )
{
  if( !multiface_available || mf.IC8b_Q == 1 ) return;

  mf.IC8a_Q = 0;
  multiface_activated = 0;
  multiface_page();
}

int
multiface_unittest( void )
{
  int r = 0;

  multiface_page();

  r += unittests_assert_8k_page( 0x0000, multiface_memory_source, 0 );
  r += unittests_assert_8k_page( 0x2000, multiface_memory_source, 0 );
  r += unittests_assert_16k_ram_page( 0x4000, 5 );
  r += unittests_assert_16k_ram_page( 0x8000, 2 );
  r += unittests_assert_16k_ram_page( 0xc000, 0 );

  multiface_unpage();

  r += unittests_paging_test_48( 2 );

  return r;
}


/*
static void
multiface_enabled_snapshot( libspectrum_snap *snap )
{
  if( libspectrum_snap_multiface_active( snap ) )
    settings_current.multiface = 1;
}

static void
multiface_from_snapshot( libspectrum_snap *snap )
{
  if( !libspectrum_snap_multiface_active( snap ) ) return;

  if( libspectrum_snap_multiface_custom_rom( snap ) &&
      libspectrum_snap_multiface_rom( snap, 0 ) &&
      machine_load_rom_bank_from_buffer(
                             mf1_memory_map_romcs, 0,
                             libspectrum_snap_multiface_rom( snap, 0 ),
                             libspectrum_snap_multiface_rom_length( snap, 0 ),
                             1 ) )
    return;

  if( libspectrum_snap_multiface_paged( snap ) ) {
    multiface_page();
  } else {
    multiface_unpage();
  }
}

static void
multiface_to_snapshot( libspectrum_snap *snap )
{
  libspectrum_byte *buffer;

  if( !periph_is_active( PERIPH_TYPE_INTERFACE1 ) ) return;

  libspectrum_snap_set_multiface_active( snap, 1 );
  libspectrum_snap_set_multiface_paged ( snap, multiface_active );
  libspectrum_snap_set_multiface_drive_count( snap, 8 );

  if( if1_memory_map_romcs[0].save_to_snapshot ) {
    size_t rom_length = MEMORY_PAGE_SIZE;

    if( if1_memory_map_romcs[1].save_to_snapshot ) {
      rom_length <<= 1;
    }

    libspectrum_snap_set_multiface_custom_rom( snap, 1 );
    libspectrum_snap_set_multiface_rom_length( snap, 0, rom_length );

    buffer = malloc( rom_length );
    if( !buffer ) {
      ui_error( UI_ERROR_ERROR, "Out of memory at %s:%d", __FILE__, __LINE__ );
      return;
    }

    memcpy( buffer, if1_memory_map_romcs[0].page, MEMORY_PAGE_SIZE );

    if( rom_length == MEMORY_PAGE_SIZE*2 ) {
      memcpy( buffer + MEMORY_PAGE_SIZE, multiface_memory_map_romcs[1].page,
              MEMORY_PAGE_SIZE );
    }

    libspectrum_snap_set_multiface_rom( snap, 0, buffer );
  }
}
*/
