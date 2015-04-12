/* fmfconv.h: Convert .fmf movie files
   Copyright (c) 2004-2005 Gergely Szasz

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

   E-mail: szaszg@hu.inter.net

*/

#ifndef FMFCONV_H
#define FMFCONV_H

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#define PRIu64 "llu"
#endif /* #ifdef HAVE_INTTYPES_H */

#ifdef HAVE_ZLIB_H
#define USE_ZLIB
#endif

#ifdef USE_FFMPEG
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

#define printe(format...) \
    fprintf (stderr, "*ERR " format)

#define printw(format...) \
    fprintf (stderr, "*WRN " format)

#define printi(lvl,format...) \
    if( lvl <= verbose ) fprintf( stderr, "*INF " format )

enum {
  ERR_OPEN_INP = 1,
  ERR_OPEN_OUT,
  ERR_OPEN_SND,
  ERR_CORRUPT_INP,
  ERR_VERSION_INP,
  ERR_NO_ZLIB,
  ERR_ENDOFFILE,
  ERR_OUTOFMEM,
  ERR_CORRUPT_SND,
  ERR_BAD_PARAM,
  ERR_WRITE_OUT,
  ERR_WRITE_SND,
};


typedef enum {
  TYPE_UNSET = -1,

  TYPE_NONE = 0,

  TYPE_FMF,	/* i */

  TYPE_SCR,	/* io (no conv) */
  TYPE_PPM,	/* o (conv to RGB) */
  TYPE_PNG,
  TYPE_JPEG,

  TYPE_MJPEG,	/* o */
  TYPE_AVI,	/* os */

  TYPE_YUV,	/* o (conv to YUV) */
  TYPE_FFMPEG,	/* os */

  TYPE_WAV,	/* s */
  TYPE_AU,	/* s */
  TYPE_AIFF,	/* s */

  TYPE_444,
  TYPE_422,
  TYPE_420J,
  TYPE_420M,
  TYPE_420,
  TYPE_410,
  TYPE_mono,

  TYPE_AVI_DIB,

  TYPE_RESCALE_WH,
  TYPE_RESCALE_X,

  TYPE_PERC,
  TYPE_BAR,
  TYPE_FRAME,
  TYPE_TIME,

  TYPE_NOCUT,
  TYPE_CUTFROM,
  TYPE_CUT,

  TYPE_ZXS = '$',	/* screen type $ -> normal Spectrum */
  TYPE_TXS = 'X',	/* X -> Timex normal */
  TYPE_HRE = 'R',	/* R -> Timex HighRes */
  TYPE_HCO = 'C',	/* C -> Timex HiColor */

  TYPE_PCM = 'P',	/* 16 bit signed PCM */
  TYPE_ALW = 'A',	/* 8 bit signed A-Law */
  TYPE_ULW = 'U',	/* 8 bit signed u-Law */

  TYPE_MONO= 'M',
  TYPE_STEREO='S',


} type_t;

extern int verbose;

extern FILE *out, *snd;
extern int out_to_stdout;

extern int frm_slice_x, frm_slice_y, frm_slice_w, frm_slice_h;
extern int frm_w, frm_h;
extern int frm_fps, frm_mch;
extern libspectrum_qword output_no;	/* output frame no */

extern type_t scr_t, yuv_t, out_t, snd_t;

extern const char *out_name;
extern int out_w, out_h;
extern int out_fps;			/* desired output frame rate */
extern int out_header_ok;			/* output header ok? */

extern int out_chn, out_rte, out_fsz, out_len;

extern libspectrum_signed_byte *sound8;	/* sound buffer for x-law */
extern libspectrum_signed_word *sound16;	/* sound buffer for pcm */

extern type_t snd_enc;				/* sound type (pcm/alaw/ulaw) */
extern int snd_rte, snd_chn, snd_fsz, snd_len;	/* sound rate (Hz), sound channels (1/2), sound length in byte  */
extern int snd_header_ok;			/* sound header ok? */
extern int snd_little_endian;

extern libspectrum_byte zxscr[];		/* 2x 40x240 bitmap1 bitmap2 */
extern libspectrum_byte attrs[];		/* 40x240 attrib */
extern libspectrum_byte pix_rgb[];		/* other view of data */
extern libspectrum_byte *pix_yuv[];		/* other view of data */
extern int yuv_ylen, yuv_uvlen;
extern int machine_timing[];

extern int force_aifc;			/* record aifc file even PCM sound */

FILE *fopen_overwr( const char *path, const char *mode, int rw );
libspectrum_dword swap_endian_dword( libspectrum_dword d );
void pcm_swap_endian( void );	/* buff == sound */
int next_outname( libspectrum_qword num );
void close_out( void );
int open_out( void );

int snd_write_avi( void );
int out_write_avi( void );
void out_finalize_avi( void );

int out_write_yuv( void );

int out_write_scr( void );
int out_write_ppm( void );
#ifdef USE_LIBPNG
int out_write_png( void );
#endif
#ifdef USE_LIBJPEG
int out_write_jpg( void );
int out_write_mjpeg( void );
int out_build_avi_mjpeg_frame( char **frame_buff,
                               unsigned long int *frame_size );
void out_finalize_mjpeg( void );
#endif

int snd_write_wav( void );
void snd_finalize_wav( void );

int snd_write_au( void );
void snd_finalize_au( void );

int snd_write_aiff( void );
void snd_finalize_aiff( void );

#ifdef USE_FFMPEG_VARS
extern int ffmpeg_arate;		/* audio bitrate */
extern int ffmpeg_vrate;		/* video bitrate */
extern type_t ffmpeg_rescale;
extern const char *ffmpeg_frate;
extern const char *ffmpeg_format;
extern const char *ffmpeg_vcodec;
extern const char *ffmpeg_acodec;
extern AVRational ffmpeg_aspect;
#endif

#ifdef USE_FFMPEG
extern int ffmpeg_libx264;
extern int ffmpeg_list;

int snd_write_ffmpeg( void );
int out_write_ffmpegheader( void );
int out_write_ffmpeg( void );
void out_finalize_ffmpeg( void );

int ffmpeg_resample_audio( void );
int ffmpeg_rescale_video( void );

void ffmpeg_list_ffmpeg( int what );
#endif


#endif	/* FMFCONV_H */
