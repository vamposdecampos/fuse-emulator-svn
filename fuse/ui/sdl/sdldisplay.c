/* sdldisplay.c: Routines for dealing with the SDL display
   Copyright (c) 2000-2004 Philip Kendall, Matan Ziv-Av, Fredrick Meunier

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

#ifdef UI_SDL			/* Use this iff we're using SDL */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>

#include <libspectrum.h>

#include "display.h"
#include "fuse.h"
#include "machine.h"
#include "scld.h"
#include "screenshot.h"
#include "settings.h"
#include "ui/ui.h"
#include "ui/scaler/scaler.h"
#include "ui/uidisplay.h"
#include "utils.h"
#ifdef USE_WIDGET
#include "widget/widget.h"
#endif				/* #ifdef USE_WIDGET */

SDL_Surface *sdldisplay_gc = NULL;   /* Hardware screen */
static SDL_Surface *tmp_screen=NULL; /* Temporary screen for scalers */

static SDL_Surface *red_cassette[2], *green_cassette[2];
static SDL_Surface *red_disk[2], *green_disk[2];

static ui_statusbar_state sdl_disk_state, sdl_tape_state;

static int tmp_screen_width;

static Uint32 colour_values[16];

static SDL_Color colour_palette[] = {
  {   0,   0,   0,   0 }, 
  {   0,   0, 192,   0 }, 
  { 192,   0,   0,   0 }, 
  { 192,   0, 192,   0 }, 
  {   0, 192,   0,   0 }, 
  {   0, 192, 192,   0 }, 
  { 192, 192,   0,   0 }, 
  { 192, 192, 192,   0 }, 
  {   0,   0,   0,   0 }, 
  {   0,   0, 255,   0 }, 
  { 255,   0,   0,   0 }, 
  { 255,   0, 255,   0 }, 
  {   0, 255,   0,   0 }, 
  {   0, 255, 255,   0 }, 
  { 255, 255,   0,   0 }, 
  { 255, 255, 255,   0 }
};

static Uint32 bw_values[16];

/* This is a rule of thumb for the maximum number of rects that can be updated
   each frame. If more are generated we just update the whole screen */
#define MAX_UPDATE_RECT 300
static SDL_Rect updated_rects[MAX_UPDATE_RECT];
static int num_rects = 0;
static libspectrum_byte sdldisplay_force_full_refresh = 1;

/* The current size of the display (in units of DISPLAY_SCREEN_*) */
static float sdldisplay_current_size = 1;

static libspectrum_byte sdldisplay_is_full_screen = 0;

static int image_width;
static int image_height;

static int timex;

static void init_scalers( void );
static int sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                                        Uint32 *bw_values );

static int sdldisplay_load_gfx_mode( void );

static void
init_scalers( void )
{
  scaler_register_clear();

  scaler_register( SCALER_NORMAL );
  scaler_register( SCALER_DOUBLESIZE );
  scaler_register( SCALER_TRIPLESIZE );
  scaler_register( SCALER_2XSAI );
  scaler_register( SCALER_SUPER2XSAI );
  scaler_register( SCALER_SUPEREAGLE );
  scaler_register( SCALER_ADVMAME2X );
  scaler_register( SCALER_ADVMAME3X );
  scaler_register( SCALER_DOTMATRIX );
  if( machine_current->timex ) {
    scaler_register( SCALER_HALF ); 
    scaler_register( SCALER_HALFSKIP );
    scaler_register( SCALER_TIMEXTV );
    scaler_register( SCALER_TIMEX1_5X );
  } else {
    scaler_register( SCALER_TV2X );
  }
  
  if( scaler_is_supported( current_scaler ) ) {
    scaler_select_scaler( current_scaler );
  } else {
    scaler_select_scaler( SCALER_NORMAL );
  }
}

static int
sdl_convert_icon( SDL_Surface *source, SDL_Surface **icon, int red )
{
  SDL_Surface *copy;   /* Copy with altered palette */
  int i;

  SDL_Color colors[ source->format->palette->ncolors ];

  copy = SDL_ConvertSurface( source, source->format, SDL_SWSURFACE );

  for( i = 0; i < copy->format->palette->ncolors; i++ ) {
    colors[i].r = red ? copy->format->palette->colors[i].r : 0;
    colors[i].g = red ? 0 : copy->format->palette->colors[i].g;
    colors[i].b = 0;
  }

  SDL_SetPalette( copy, SDL_LOGPAL, colors, 0, i );

  icon[0] = SDL_ConvertSurface( copy, tmp_screen->format, SDL_SWSURFACE );

  SDL_FreeSurface( copy );

  icon[1] = SDL_CreateRGBSurface( SDL_SWSURFACE,
                                  (icon[0]->w)<<1, (icon[0]->h)<<1,
                                  icon[0]->format->BitsPerPixel,
                                  icon[0]->format->Rmask,
                                  icon[0]->format->Gmask,
                                  icon[0]->format->Bmask,
                                  icon[0]->format->Amask
                                );

  ( scaler_get_proc16( SCALER_DOUBLESIZE ) )(
        (libspectrum_byte*)icon[0]->pixels,
        icon[0]->pitch,
        (libspectrum_byte*)icon[1]->pixels,
        icon[1]->pitch, icon[0]->w, icon[0]->h
      );

  return 0;
}

static int
sdl_load_status_icon( const char*filename, SDL_Surface **red, SDL_Surface **green )
{
  char path[ PATH_MAX ];
  SDL_Surface *temp;    /* Copy of image as loaded */

  if( utils_find_file_path( filename, path, UTILS_AUXILIARY_LIB ) ) {
    fprintf( stderr, "%s: Error getting path for icons\n", fuse_progname );
    return -1;
  }

  if((temp = SDL_LoadBMP(path)) == NULL) {
    fprintf( stderr, "%s: Error loading icon \"%s\" text:%s\n", fuse_progname,
             path, SDL_GetError() );
    return -1;
  }

  if(temp->format->palette == NULL) {
    fprintf( stderr, "%s: Icon \"%s\" is not paletted\n", fuse_progname, path );
    return -1;
  }

  sdl_convert_icon( temp, red, 1 );
  sdl_convert_icon( temp, green, 0 );

  SDL_FreeSurface( temp );

  return 0;
}

int
uidisplay_init( int width, int height )
{
  image_width = width;
  image_height = height;

  timex = machine_current->timex;

  init_scalers();

  if ( scaler_select_scaler( current_scaler ) ) scaler_select_scaler( SCALER_NORMAL );

  sdldisplay_load_gfx_mode();

  SDL_WM_SetCaption( "Fuse", "Fuse" );

  /* We can now output error messages to our output device */
  display_ui_initialised = 1;

  sdl_load_status_icon( "cassette.bmp", red_cassette, green_cassette );
  sdl_load_status_icon( "plus3disk.bmp", red_disk, green_disk );

  return 0;
}

static int
sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                             Uint32 *bw_values )
{
  int i;
  Uint8 red, green, blue, grey;

  for( i = 0; i < numColours; i++ ) {

      red = colour_palette[i].r;
    green = colour_palette[i].g;
     blue = colour_palette[i].b;

    /* Addition of 0.5 is to avoid rounding errors */
    grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;

    colour_values[i] = SDL_MapRGB( tmp_screen->format,  red, green, blue );
    bw_values[i]     = SDL_MapRGB( tmp_screen->format, grey,  grey, grey );
  }

  return 0;
}

static int
sdldisplay_load_gfx_mode( void )
{
  Uint16 *tmp_screen_pixels;

  sdldisplay_force_full_refresh = 1;

  tmp_screen = NULL;
  tmp_screen_width = (image_width + 3);

  sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );

  /* Create the surface that contains the scaled graphics in 16 bit mode */
  sdldisplay_gc = SDL_SetVideoMode(
    image_width * sdldisplay_current_size,
    image_height * sdldisplay_current_size,
    16,
    settings_current.full_screen ? (SDL_FULLSCREEN|SDL_SWSURFACE)
                                 : SDL_SWSURFACE
  );
  if( !sdldisplay_gc ) {
    fprintf( stderr, "%s: couldn't create gc\n", fuse_progname );
    return 1;
  }

  sdldisplay_is_full_screen = settings_current.full_screen = !!(sdldisplay_gc->flags & SDL_FULLSCREEN);

  /* Distinguish 555 and 565 mode */
  if( sdldisplay_gc->format->Gmask >> sdldisplay_gc->format->Gshift == 0x1f )
    scaler_select_bitformat( 555 );
  else
    scaler_select_bitformat( 565 );

  /* Create the surface used for the graphics in 16 bit before scaling */

  /* Need some extra bytes around when using 2xSaI */
  tmp_screen_pixels = (Uint16*)calloc(tmp_screen_width*(image_height+3), sizeof(Uint16));
  tmp_screen = SDL_CreateRGBSurfaceFrom(tmp_screen_pixels,
                                        tmp_screen_width,
                                        image_height + 3,
                                        16, tmp_screen_width*2,
                                        sdldisplay_gc->format->Rmask,
                                        sdldisplay_gc->format->Gmask,
                                        sdldisplay_gc->format->Bmask,
                                        sdldisplay_gc->format->Amask );

  if( !tmp_screen ) {
    fprintf( stderr, "%s: couldn't create tmp_screen\n", fuse_progname );
    return 1;
  }

  sdldisplay_allocate_colours( 16, colour_values, bw_values );

  /* Redraw the entire screen... */
  display_refresh_all();

  return 0;
}

void
uidisplay_hotswap_gfx_mode( void )
{
  fuse_emulation_pause();

  /* Free the old surface */
  if( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }

  /* Setup the new GFX mode */
  sdldisplay_load_gfx_mode();

  /* reset palette */
  SDL_SetColors( sdldisplay_gc, colour_palette, 0, 16 );

  /* Mac OS X resets the state of the cursor after a switch to full screen
     mode */
  if ( settings_current.full_screen || ui_mouse_grabbed ) {
    SDL_ShowCursor( SDL_DISABLE );
    SDL_WarpMouse( 128, 128 );
  } else {
    SDL_ShowCursor( SDL_ENABLE );
  }

  fuse_emulation_unpause();
}

static void
sdl_blit_icon( SDL_Surface **icon,
               SDL_Rect *r, Uint32 tmp_screen_pitch,
               Uint32 dstPitch )
{
  int x, y, w, h, dst_y, dst_h;

  if( timex ) {
    r->x<<=1;
    r->y<<=1;
    r->w<<=1;
    r->h<<=1;
  }

  x = r->x;
  y = r->y;
  w = r->w;
  h = r->h;
  r->x++;
  r->y++;

  if( SDL_BlitSurface( icon[timex], NULL, tmp_screen, r ) ) return;

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &w, &h, image_width, image_height );

  dst_y = y * sdldisplay_current_size;
  dst_h = h;

  scaler_proc16(
	(libspectrum_byte*)tmp_screen->pixels +
			(x+1) * tmp_screen->format->BytesPerPixel +
	                (y+1) * tmp_screen_pitch,
	tmp_screen_pitch,
	(libspectrum_byte*)sdldisplay_gc->pixels +
			x * (libspectrum_byte)
				(sdldisplay_gc->format->BytesPerPixel *
				sdldisplay_current_size) +
			dst_y * dstPitch,
	dstPitch, w, dst_h
  );

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Adjust rects for the destination rect size */
  updated_rects[num_rects].x = x * sdldisplay_current_size;
  updated_rects[num_rects].y = dst_y;
  updated_rects[num_rects].w = w * sdldisplay_current_size;
  updated_rects[num_rects].h = dst_h * sdldisplay_current_size;

  num_rects++;
}

static void
sdl_icon_overlay( Uint32 tmp_screen_pitch, Uint32 dstPitch )
{
  SDL_Rect r = { 264, 218, red_disk[0]->w, red_disk[0]->h };

  switch( sdl_disk_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }

  r.x = 285;
  r.y = 220;
  r.w = red_cassette[0]->w;
  r.h = red_cassette[0]->h;

  switch( sdl_tape_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }
}

void
uidisplay_frame_end( void )
{
  SDL_Rect *r;
  Uint32 tmp_screen_pitch, dstPitch;
  SDL_Rect *last_rect;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;

  /* We check for a switch to fullscreen here to give systems with a
     windowed-only UI a chance to free menu etc. resources before
     the switch to fullscreen (e.g. Mac OS X) */
  if( sdldisplay_is_full_screen != settings_current.full_screen ) {
    uidisplay_hotswap_gfx_mode();
  }

  /* Force a full redraw if requested */
  if ( sdldisplay_force_full_refresh ) {
    num_rects = 1;

    updated_rects[0].x = 0;
    updated_rects[0].y = 0;
    updated_rects[0].w = image_width;
    updated_rects[0].h = image_height;
  }

#ifdef USE_WIDGET
  if ( !(widget_level >= 0) && num_rects == 0 ) return;
#else                   /* #ifdef USE_WIDGET */
  if ( num_rects == 0 ) return;
#endif                  /* #ifdef USE_WIDGET */

  if( SDL_MUSTLOCK( tmp_screen ) ) SDL_LockSurface( tmp_screen );
  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_LockSurface( sdldisplay_gc );

  tmp_screen_pitch = tmp_screen->pitch;

  dstPitch = sdldisplay_gc->pitch;

  last_rect = updated_rects + num_rects;

  for( r = updated_rects; r != last_rect; r++ ) {

    libspectrum_word *dest_base, *dest;
    size_t xx,yy;
    int dst_y = r->y * sdldisplay_current_size;
    int dst_h = r->h;

    dest_base =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (r->x+1) * tmp_screen->format->BytesPerPixel +
			   (r->y+1)*tmp_screen_pitch );

    for( yy = r->y; yy < r->y + r->h; yy++ ) {

      for( xx = r->x, dest = dest_base; xx < r->x + r->w; xx++, dest++ )
        *dest = palette_values[ display_image[yy][xx] ];

      dest_base = (libspectrum_word*)
        ( (libspectrum_byte*)dest_base + tmp_screen_pitch );
    }
	  
    scaler_proc16(
      (libspectrum_byte*)tmp_screen->pixels +
                        (r->x+1) * tmp_screen->format->BytesPerPixel +
	                (r->y+1)*tmp_screen_pitch,
      tmp_screen_pitch,
      (libspectrum_byte*)sdldisplay_gc->pixels +
	                 r->x*(libspectrum_byte)
                               (sdldisplay_gc->format->BytesPerPixel *
                                sdldisplay_current_size) +
			 dst_y*dstPitch,
      dstPitch, r->w, dst_h
    );

    /* Adjust rects for the destination rect size */
    r->x *= sdldisplay_current_size;
    r->y = dst_y;
    r->w *= sdldisplay_current_size;
    r->h = dst_h * sdldisplay_current_size;
  }

  if ( settings_current.statusbar )
    sdl_icon_overlay( tmp_screen_pitch, dstPitch );

  if( SDL_MUSTLOCK( tmp_screen ) ) SDL_UnlockSurface( tmp_screen );
  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_UnlockSurface( sdldisplay_gc );

  /* Finally, blit all our changes to the screen */
  SDL_UpdateRects( sdldisplay_gc, num_rects, updated_rects );

  num_rects = 0;
  sdldisplay_force_full_refresh = 0;
}

void
uidisplay_area( int x, int y, int width, int height )
{
  if ( sdldisplay_force_full_refresh )
    return;

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &width, &height, image_width, image_height );

  updated_rects[num_rects].x = x;
  updated_rects[num_rects].y = y;
  updated_rects[num_rects].w = width;
  updated_rects[num_rects].h = height;

  num_rects++;
}

int
uidisplay_end( void )
{
  int i;
  display_ui_initialised = 0;
  if ( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }
  for( i=0; i<2; i++ ) {
    if ( red_cassette[i] ) {
      SDL_FreeSurface( red_cassette[i] ); red_cassette[i] = NULL;
    }
    if ( green_cassette[i] ) {
      SDL_FreeSurface( green_cassette[i] ); green_cassette[i] = NULL;
    }
    if ( red_disk[i] ) {
      SDL_FreeSurface( red_disk[i] ); red_disk[i] = NULL;
    }
    if ( green_disk[i] ) {
      SDL_FreeSurface( green_disk[i] ); green_disk[i] = NULL;
    }
  }
  return 0;
}

/* The statusbar handling function */
int
ui_statusbar_update( ui_statusbar_item item, ui_statusbar_state state )
{
  switch( item ) {

  case UI_STATUSBAR_ITEM_DISK:
    sdl_disk_state = state;
    return 0;

  case UI_STATUSBAR_ITEM_PAUSED:
    // We don't support pausing this version of Fuse
    return 0;

  case UI_STATUSBAR_ITEM_TAPE:
    sdl_tape_state = state;
    return 0;

  case UI_STATUSBAR_ITEM_MOUSE:
    // We don't support showing a grab icon
    return 0;

  }

  ui_error( UI_ERROR_ERROR, "Attempt to update unknown statusbar item %d",
            item );
  return 1;
}

#endif        /* #ifdef UI_SDL */
