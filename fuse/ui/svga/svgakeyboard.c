/* svgakeyboard.c: svgalib routines for dealing with the keyboard
   Copyright (c) 2000-2004 Philip Kendall, Matan Ziv-Av

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
   Foundation, Inc., 49 Temple Place, Suite 330, Boston, MA 02111-1307 USA

   Author contact information:

   E-mail: pak21-fuse@srcf.ucam.org
   Postal address: 15 Crescent Road, Wokingham, Berks, RG40 2DB, England

*/

#include <config.h>

#ifdef UI_SVGA			/* Use this iff we're using svgalib */

#include <stdio.h>

#include <vga.h>
#include <vgakeyboard.h>

#include "display.h"
#include "fuse.h"
#include "keyboard.h"
#include "machine.h"
#include "settings.h"
#include "snapshot.h"
#include "spectrum.h"
#include "tape.h"
#include "utils.h"
#ifdef USE_WIDGET
#include "widget/widget.h"
#endif				/* #ifdef USE_WIDGET */

static void svgakeyboard_keystroke(int scancode, int press);
static int svgakeyboard_keypress( int keysym );
static int svgakeyboard_keyrelease( int keysym );

int svgakeyboard_init(void)
{
  keyboard_init();
  keyboard_seteventhandler(svgakeyboard_keystroke);
  return 0;
}

static void svgakeyboard_keystroke(int scancode, int press)  {
  if(press) {
    svgakeyboard_keypress(scancode);
  } else {
    svgakeyboard_keyrelease(scancode);
  }
}

static int
svgakeyboard_keypress( int keysym )
{
  input_key fuse_keysym;
  input_event_t fuse_event;

  fuse_keysym = keysyms_remap( keysym );

  if( fuse_keysym == INPUT_KEY_NONE ) return 0;

  fuse_event.type = INPUT_EVENT_KEYPRESS;
  fuse_event.types.key.key = fuse_keysym;

  return input_event( &fuse_event );
}

static int
svgakeyboard_keyrelease( int keysym )
{
  input_key fuse_keysym;
  input_event_t fuse_event;

  fuse_keysym = keysyms_remap( keysym );

  if( fuse_keysym == INPUT_KEY_NONE ) return 0;

  fuse_event.type = INPUT_EVENT_KEYRELEASE;
  fuse_event.types.key.key = fuse_keysym;

  return input_event( &fuse_event );
}

int svgakeyboard_end(void)
{
  keyboard_close();
  return 0;
}

#endif				/* #ifdef UI_SVGA */
