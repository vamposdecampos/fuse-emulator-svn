/* uimedia.h: Disk media UI routines
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

#ifndef FUSE_UIMEDIA_H
#define FUSE_UIMEDIA_H

typedef int (*ui_media_drive_is_available_fn)( void );

struct fdd_t;

typedef struct ui_media_drive_info_t
{
  const char *name;
  int controller_index;
  int drive_index;
  int menu_item_parent;
  int menu_item_top;
  int menu_item_eject;
  int menu_item_flip;
  int menu_item_wp;
  ui_media_drive_is_available_fn is_available;
  struct fdd_t *fdd;
} ui_media_drive_info_t;

int ui_media_drive_register( ui_media_drive_info_t *drive );
void ui_media_drive_end( void );
ui_media_drive_info_t *ui_media_drive_find( int controller, int drive );

#define UI_MEDIA_DRIVE_UPDATE_ALL	(~0)
#define UI_MEDIA_DRIVE_UPDATE_TOP	(1 << 0)
#define UI_MEDIA_DRIVE_UPDATE_EJECT	(1 << 1)
#define UI_MEDIA_DRIVE_UPDATE_FLIP	(1 << 2)
#define UI_MEDIA_DRIVE_UPDATE_WP	(1 << 3)

int ui_media_drive_any_available( void );
void ui_media_drive_update_parent_menus( void );
void ui_media_drive_update_menus( ui_media_drive_info_t *drive, unsigned flags );

int ui_media_drive_flip( int controller, int which, int flip );

#endif			/* #ifndef FUSE_UIMEDIA_H */
