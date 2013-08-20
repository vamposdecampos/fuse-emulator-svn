/* uimedia.c: Disk media UI routines
   Copyright (c) 2013 Alex Badea

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

   E-mail: vamposdecampos@gmail.com

*/

#include <config.h>

#ifdef HAVE_LIB_GLIB
#include <glib.h>
#endif

#include "ui/ui.h"
#include "ui/uimedia.h"

static GSList *registered_drives = NULL;

int
ui_media_drive_register( ui_media_drive_info_t *drive )
{
  registered_drives = g_slist_append( registered_drives, drive );
  return 0;
}

void
ui_media_drive_end( void )
{
  g_slist_free( registered_drives );
  registered_drives = NULL;
}


static void
any_available( gpointer data, gpointer user_data )
{
  const ui_media_drive_info_t *drive = data;
  int *res_ptr = user_data;

  if( drive->is_available && drive->is_available() )
    *res_ptr = 1;
}

int
ui_media_drive_any_available( void )
{
  gboolean res = 0;
  g_slist_foreach( registered_drives, any_available, &res );
  return res;
}


static void
update_parent_menus( gpointer data, gpointer user_data )
{
  const ui_media_drive_info_t *drive = data;

  if( drive->is_available && drive->menu_item_parent >= 0 )
    ui_menu_activate( drive->menu_item_parent, drive->is_available() );
}

void
ui_media_drive_update_parent_menus( void )
{
  g_slist_foreach( registered_drives, update_parent_menus, NULL );
}

static int
maybe_menu_activate( int id, int activate )
{
  if( id < 0 )
    return 0;
  return ui_menu_activate( id, activate );
}

void
ui_media_drive_update_menus( ui_media_drive_info_t *drive, unsigned flags )
{
  if( !drive->fdd )
    return;

  if( flags & UI_MEDIA_DRIVE_UPDATE_TOP )
    maybe_menu_activate( drive->menu_item_top, drive->fdd->type != FDD_TYPE_NONE );
  if( flags & UI_MEDIA_DRIVE_UPDATE_EJECT )
    maybe_menu_activate( drive->menu_item_eject, drive->fdd->loaded );
  if( flags & UI_MEDIA_DRIVE_UPDATE_FLIP )
    maybe_menu_activate( drive->menu_item_flip, !drive->fdd->upsidedown );
  if( flags & UI_MEDIA_DRIVE_UPDATE_WP )
    maybe_menu_activate( drive->menu_item_wp, !drive->fdd->wrprot );
}
