/* menu.c: general menu widget
   Copyright (c) 2001-2004 Philip Kendall

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

   E-mail: pak21-fuse@srcf.ucam.org
   Postal address: 15 Crescent Road, Wokingham, Berks, RG40 2DB, England

*/

#include <config.h>

#ifdef USE_WIDGET

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "dck.h"
#include "debugger/debugger.h"
#include "event.h"
#include "fuse.h"
#include "machine.h"
#include "psg.h"
#include "rzx.h"
#include "screenshot.h"
#include "settings.h"
#include "snapshot.h"
#include "specplus3.h"
#include "tape.h"
#include "trdos.h"
#include "ui/uidisplay.h"
#include "utils.h"
#include "widget_internals.h"

widget_menu_entry *menu;

static int widget_load_snapshot( void );

int widget_menu_draw( void *data )
{
  widget_menu_entry *ptr;
  size_t menu_entries, i;

  menu = (widget_menu_entry*)data;

  /* How many menu items do we have? */
  for( ptr = &menu[1]; ptr->text; ptr++ ) ;
  menu_entries = ptr - &menu[1];

  widget_dialog_with_border( 1, 2, 30, menu_entries + 2 );

  widget_printstring( 15 - strlen( menu->text ) / 2, 2,
		      WIDGET_COLOUR_FOREGROUND, menu->text );

  for( i=0; i<menu_entries; i++ )
    widget_printstring( 2, i+4, WIDGET_COLOUR_FOREGROUND,
			menu[i+1].text );

  widget_display_lines( 2, menu_entries + 2 );

  return 0;
}

void
widget_menu_keyhandler( keyboard_key_name key, keyboard_key_name key2 )
{
  widget_menu_entry *ptr;

  switch( key ) {

  case KEYBOARD_Resize:		/* Fake keypress used on window resize */
    widget_menu_draw( menu );
    break;
    
  case KEYBOARD_1: /* 1 used as `Escape' generates `Edit', which is Caps + 1 */
    if( key2 == KEYBOARD_Caps ) widget_end_widget( WIDGET_FINISHED_CANCEL );
    return;

  case KEYBOARD_Enter:
    widget_end_widget( WIDGET_FINISHED_OK );
    return;

  default:	/* Keep gcc happy */
    break;

  }

  for( ptr=&menu[1]; ptr->text; ptr++ ) {
    if( key == ptr->key ) {
      (ptr->action)( ptr->data );	/* function pointer call */
      break;
    }
  }
}

/* General callbacks */

/* The callback used to call another widget */
int widget_menu_widget( void *data )
{
  widget_menu_widget_t *ptr = (widget_menu_widget_t*)data;

  return widget_do( ptr->widget, ptr->data );
}

/* The callback to get a file name and do something with it */
int widget_apply_to_file( void *data )
{
  typedef int (*fn_ptr)(const char*);

  fn_ptr ptr = (fn_ptr)data;

  widget_do( WIDGET_TYPE_FILESELECTOR, NULL );
  if( widget_filesel_name ) {
    ptr( widget_filesel_name );
    free( widget_filesel_name );
  }

  return 0;
}

/* File menu callbacks */

/* File/Open (called via widget_apply_to_file) */
int
widget_menu_open( const char *filename )
{
  return utils_open_file( filename, settings_current.auto_load, NULL );
}

/* File/Save Snapshot*/
int
widget_menu_save_snapshot( void *data GCC_UNUSED )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return snapshot_write( "snapshot.z80" );
}

/* File/Recording/Record */
int
widget_menu_rzx_recording( void *data GCC_UNUSED )
{
  if( rzx_playback || rzx_recording ) return 0;

  widget_end_all( WIDGET_FINISHED_OK );

  return rzx_start_recording( "record.rzx", 1 );
}

/* File/Recording/Record from snap */
int
widget_menu_rzx_recording_snap( void *data GCC_UNUSED )
{
  int error;

  if( rzx_playback || rzx_recording ) return 0;

  /* Get a snapshot name */
  widget_do( WIDGET_TYPE_FILESELECTOR, NULL );

  if( !widget_filesel_name ) {
    widget_end_widget( WIDGET_FINISHED_CANCEL );
    return 0;
  }

  error = snapshot_read( widget_filesel_name );
  if( error ) {
    if( widget_filesel_name ) free( widget_filesel_name );
    return error;
  }

  return rzx_start_recording( "record.rzx", settings_current.embed_snapshot );
}

/* File/Recording/Play */
int
widget_menu_rzx_playback( void *data GCC_UNUSED )
{
  int error;

  if( rzx_playback || rzx_recording ) return 0;

  widget_do( WIDGET_TYPE_FILESELECTOR, NULL );

  if( widget_filesel_name ) {
    error = rzx_start_playback( widget_filesel_name, widget_load_snapshot );
    free( widget_filesel_name );
    return error;
  } else {
    return 0;
  }
}

static int widget_load_snapshot( void )
{
  return widget_apply_to_file( snapshot_read );
}

/* File/Recording/Stop */
int
widget_menu_rzx_stop( void *data GCC_UNUSED )
{
  if( rzx_recording ) rzx_stop_recording();
  if( rzx_playback  ) rzx_stop_playback( 1 );

  widget_end_all( WIDGET_FINISHED_OK );
  return 0;
}  

/* File/AY Logging/Record */
int
widget_menu_psg_record( void *data GCC_UNUSED )
{
  if( psg_recording ) return 0;

  widget_end_all( WIDGET_FINISHED_OK );

  return psg_start_recording( "ay.psg" );
}

/* File/AY Logging/Stop */
int
widget_menu_psg_stop( void *data GCC_UNUSED )
{
  if( !psg_recording ) return 0;
  
  return psg_stop_recording();
}  

#ifdef USE_LIBPNG
/* File/Save Screenshot */
int
widget_menu_save_screen( void *data GCC_UNUSED )
{
  widget_do( WIDGET_TYPE_SCALER, screenshot_available_scalers );
  if( widget_scaler == SCALER_NUM ) return 0;

  return screenshot_write( "fuse.png", widget_scaler );
}
#endif			/* #ifdef USE_LIBPNG */

/* File/Save Scr */
int
widget_menu_save_scr( void *data GCC_UNUSED )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return screenshot_scr_write( "fuse.scr" );
}

/* File/Exit */
int widget_menu_exit( void *data GCC_UNUSED )
{
  fuse_exiting = 1;
  widget_end_all( WIDGET_FINISHED_OK );
  return 0;
}

/* Options/Filter */
int
widget_menu_filter( void *data GCC_UNUSED )
{
  int error;

  error = widget_do( WIDGET_TYPE_SCALER, scaler_is_supported );
  if( error ) return error;

  if( widget_scaler == SCALER_NUM ) return 0;

  if( widget_scaler != current_scaler )
    return scaler_select_scaler( widget_scaler );

  return 0;
}

#ifdef HAVE_LIB_XML2
/* Options/Save */
int
widget_menu_save_options( void *data GCC_UNUSED )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return settings_write_config( &settings_current );
}
#endif				/* #ifdef HAVE_LIB_XML2 */

/* Machine/Reset */
int widget_menu_reset( void *data GCC_UNUSED )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return machine_reset();
}

/* Machine/Break */
int
widget_menu_break( void *data GCC_UNUSED )
{
  debugger_mode = DEBUGGER_MODE_HALTED;
  widget_do( WIDGET_TYPE_DEBUGGER, NULL );
  return 0;
}

/* Machine/NMI */
int
widget_menu_nmi( void *data GCC_UNUSED )
{
  int error;

  widget_end_all( WIDGET_FINISHED_OK );

  error = event_add( 0, EVENT_TYPE_NMI );
  if( error ) return error;

  return 0;
}

/* Tape/Play */
int widget_menu_play_tape( void *data GCC_UNUSED )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return tape_toggle_play();
}

/* Tape/Rewind */
int widget_menu_rewind_tape( void *data GCC_UNUSED )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return tape_select_block( 0 );
}

/* Disk/Drive A:/Insert (called via widget_apply_to_file) */
int
widget_insert_disk_a( const char *filename )
{
#ifdef HAVE_765_H
  if( machine_current->machine == LIBSPECTRUM_MACHINE_PLUS3 ) {
    return specplus3_disk_insert( SPECPLUS3_DRIVE_A, filename );
  }
#endif				/* #ifdef HAVE_765_H */
  return trdos_disk_insert( TRDOS_DRIVE_A, filename );
}

/* Disk/Drive B:/Insert (called via widget_apply_to_file) */
int
widget_insert_disk_b( const char *filename )
{
#ifdef HAVE_765_H
  if( machine_current->machine == LIBSPECTRUM_MACHINE_PLUS3 ) {
    return specplus3_disk_insert( SPECPLUS3_DRIVE_B, filename );
  }
#endif				/* #ifdef HAVE_765_H */
  return trdos_disk_insert( TRDOS_DRIVE_B, filename );
}

/* Disk/Drive ?:/Eject */
int
widget_menu_eject_disk( void *data )
{
  trdos_drive_number which = *(trdos_drive_number*)data;

#ifdef HAVE_765_H
  if( machine_current->machine == LIBSPECTRUM_MACHINE_PLUS3 )
    return specplus3_disk_eject( which, 0 );
#endif				/* #ifdef HAVE_765_H */

  return trdos_disk_eject( which, 0 );
}

/* Disk/Drive ?:/Eject and write */
int
widget_menu_eject_write_disk( void *data )
{
  trdos_drive_number which = *(trdos_drive_number*)data;

#ifdef HAVE_765_H
  if( machine_current->machine == LIBSPECTRUM_MACHINE_PLUS3 )
    return specplus3_disk_eject( which, 1 );
#endif				/* #ifdef HAVE_765_H */

  return trdos_disk_eject( which, 1 );
}

#ifdef HAVE_765_H
int
ui_plus3_disk_write( specplus3_drive_number which )
{
  char filename[ 20 ];

  snprintf( filename, 20, "drive%c.dsk", (char)( 'a' + which ) );

  return specplus3_disk_write( which, filename );
}
#endif				/* #ifdef HAVE_765_H */

int
ui_trdos_disk_write( trdos_drive_number which )
{
  char filename[ 20 ];

  snprintf( filename, 20, "drive%c.trd", (char)( 'a' + which ) );

  return trdos_disk_write( which, filename );
}

/* Cartridge/Timex Dock/Insert (called via widget_apply_to_file) */
int
widget_insert_dock( const char *filename )
{
  return dck_insert( filename );
}

/* Cartridge/Timex Dock/Eject */
int
widget_menu_eject_dock( void *data )
{
  dck_eject();

  return 0;
}

/* Tape/Clear */
int widget_menu_clear_tape( void *data GCC_UNUSED )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return tape_close();
}

/* Tape/Write */
int widget_menu_write_tape( void *data GCC_UNUSED )
{
  return ui_tape_write();
}

int
ui_tape_write( void )
{
  widget_end_all( WIDGET_FINISHED_OK );
  return tape_write( "tape.tzx" );
}

/* Help/Keyboard Picture */
int widget_menu_keyboard( void *data )
{
  widget_picture_data *ptr = (widget_picture_data*)data;

  int error, fd;
  utils_file file;

  fd = utils_find_auxiliary_file( ptr->filename, UTILS_AUXILIARY_LIB );
  if( fd == -1 ) {
    ui_error( UI_ERROR_ERROR, "couldn't find keyboard picture ('%s')",
	      ptr->filename );
    return 1;
  }
  
  error = utils_read_fd( fd, ptr->filename, &file );
  if( error ) return error;

  if( file.length != 6912 ) {
    ui_error( UI_ERROR_ERROR, "keyboard picture ('%s') is not 6912 bytes long",
	      ptr->filename );
    utils_close_file( &file );
    return 1;
  }

  ptr->screen = file.buffer;

  widget_do( WIDGET_TYPE_PICTURE, ptr );

  if( utils_close_file( &file ) ) return 1;

  return 0;
}

/* Function to (de)activate specific menu items */
/* FIXME: make this work */
int
ui_menu_activate( ui_menu_item item, int active )
{
  return 0;
}

#endif				/* #ifdef USE_WIDGET */
