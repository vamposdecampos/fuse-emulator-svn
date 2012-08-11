/* fuse.c: The Free Unix Spectrum Emulator
   Copyright (c) 1999-2011 Philip Kendall and others

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

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>
#endif				/* #ifdef WIN32 */

#include <unistd.h>

/* We need to include SDL.h on Mac O X and Windows to do some magic
   bootstrapping by redefining main. As we now allow SDL joystick code to be
   used in the GTK+ and Xlib UIs we need to also do the magic when that code is
   in use, feel free to look away for the next line */
#if defined UI_SDL || (defined USE_JOYSTICK && !defined HAVE_JSW_H && (defined UI_X || defined UI_GTK) )
#include <SDL.h>		/* Needed on MacOS X and Windows */
#endif /* #if defined UI_SDL || (defined USE_JOYSTICK && !defined HAVE_JSW_H && (defined UI_X || defined UI_GTK) ) */

#ifdef GEKKO
#include <fat.h>
#endif				/* #ifdef GEKKO */

#include "debugger/debugger.h"
#include "display.h"
#include "event.h"
#include "fuse.h"
#include "keyboard.h"
#include "machine.h"
#include "machines/machines_periph.h"
#include "memory.h"
#include "module.h"
#include "movie.h"
#include "mempool.h"
#include "peripherals/ay.h"
#include "peripherals/dck.h"
#include "peripherals/disk/beta.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/fuller.h"
#include "peripherals/ide/divide.h"
#include "peripherals/ide/simpleide.h"
#include "peripherals/ide/zxatasp.h"
#include "peripherals/ide/zxcf.h"
#include "peripherals/joystick.h"
#include "peripherals/if1.h"
#include "peripherals/if2.h"
#include "peripherals/kempmouse.h"
#include "peripherals/melodik.h"
#include "peripherals/printer.h"
#include "peripherals/scld.h"
#include "peripherals/speccyboot.h"
#include "peripherals/spectranet.h"
#include "peripherals/ula.h"
#include "pokefinder/pokemem.h"
#include "profile.h"
#include "psg.h"
#include "rzx.h"
#include "settings.h"
#include "slt.h"
#include "snapshot.h"
#include "sound.h"
#include "spectrum.h"
#include "tape.h"
#include "timer/timer.h"
#include "ui/scaler/scaler.h"
#include "ui/ui.h"
#include "unittests/unittests.h"
#include "utils.h"

#include "z80/z80.h"

/* What name were we called under? */
char *fuse_progname;

/* Which directory were we started in? */
char fuse_directory[ PATH_MAX ];

/* A flag to say when we want to exit the emulator */
int fuse_exiting;

/* Is Spectrum emulation currently paused, and if so, how many times? */
int fuse_emulation_paused;

/* The creator information we'll store in file formats that support this */
libspectrum_creator *fuse_creator;

/* The earliest version of libspectrum we need */
static const char *LIBSPECTRUM_MIN_VERSION = "0.5.0";

/* The various types of file we may want to run on startup */
typedef struct start_files_t {

  const char *disk_plus3;
  const char *disk_opus;
  const char *disk_plusd;
  const char *disk_beta;
  const char *dock;
  const char *if2;
  const char *playback;
  const char *recording;
  const char *snapshot;
  const char *tape;

  const char *simpleide_master, *simpleide_slave;
  const char *zxatasp_master, *zxatasp_slave;
  const char *zxcf;
  const char *divide_master, *divide_slave;
  const char *mdr[8];

} start_files_t;

static int fuse_init(int argc, char **argv);

static int creator_init( void );
static void fuse_show_copyright(void);
static void fuse_show_version( void );
static void fuse_show_help( void );

static int setup_start_files( start_files_t *start_files );
static int parse_nonoption_args( int argc, char **argv, int first_arg,
				 start_files_t *start_files );
static int do_start_files( start_files_t *start_files );

static int fuse_end(void);

#ifdef UI_WIN32
int fuse_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
  int r;

#ifdef WIN32
  SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );
#endif

#ifdef GEKKO
  fatInitDefault();
#endif				/* #ifdef GEKKO */
  
  if(fuse_init(argc,argv)) {
    fprintf(stderr,"%s: error initialising -- giving up!\n", fuse_progname);
    return 1;
  }

  if( settings_current.show_help ||
      settings_current.show_version ) return 0;

  if( settings_current.unittests ) {
    r = unittests_run();
  } else {
    while( !fuse_exiting ) {
      z80_do_opcodes();
      event_do_events();
    }
    r = 0;
  }

  fuse_end();
  
  return r;

}

static int fuse_init(int argc, char **argv)
{
  int error, first_arg;
  char *start_scaler;
  start_files_t start_files;

  /* Seed the bad but widely-available random number
     generator with the current time */
  srand( (unsigned)time( NULL ) );

  /* Some platforms (e.g. Wii) do not have argc/argv */
  if(argc > 0)
    fuse_progname = argv[0];
  else
    fuse_progname = "fuse";
  
  libspectrum_error_function = ui_libspectrum_error;

#ifdef GEKKO
  /* On the Wii, init the display first so we have a way of outputting
     messages */
  if( display_init(&argc,&argv) ) return 1;
#endif

  if( !getcwd( fuse_directory, PATH_MAX - 1 ) ) {
    ui_error( UI_ERROR_ERROR, "error getting current working directory: %s",
	      strerror( errno ) );
    return 1;
  }
  strcat( fuse_directory, FUSE_DIR_SEP_STR );

  if( settings_init( &first_arg, argc, argv ) ) return 1;

  if( settings_current.show_version ) {
    fuse_show_version();
    return 0;
  } else if( settings_current.show_help ) {
    fuse_show_help();
    return 0;
  }

  start_scaler = utils_safe_strdup( settings_current.start_scaler_mode );

  /* Windows will create a console for our output if there isn't one already,
   * so we don't display the copyright message on Win32. */
#ifndef WIN32
  fuse_show_copyright();
#endif

  /* FIXME: order of these initialisation calls. Work out what depends on
     what */
  /* FIXME FIXME 20030407: really do this soon. This is getting *far* too
     hairy */
  fuse_joystick_init ();
  fuse_keyboard_init();

  event_init();
  
#ifndef GEKKO
  if( display_init(&argc,&argv) ) return 1;
#endif

  if( libspectrum_check_version( LIBSPECTRUM_MIN_VERSION ) ) {
    if( libspectrum_init() ) return 1;
  } else {
    ui_error( UI_ERROR_ERROR,
              "libspectrum version %s found, but %s required",
	      libspectrum_version(), LIBSPECTRUM_MIN_VERSION );
    return 1;
  }

  /* Must be called after libspectrum_init() so we can get the gcrypt
     version */
  if( creator_init() ) return 1;

#ifdef HAVE_GETEUID
  /* Drop root privs if we have them */
  if( !geteuid() ) { setuid( getuid() ); }
#endif				/* #ifdef HAVE_GETEUID */

  mempool_init();
  memory_init();

  debugger_init();

  spectrum_init();
  printer_init();
  rzx_init();
  psg_init();
  beta_init();
  opus_init();
  plusd_init();
  disciple_init();
  fdd_init_events();
  if( simpleide_init() ) return 1;
  if( zxatasp_init() ) return 1;
  if( zxcf_init() ) return 1;
  if1_init();
  if1_fdc_init();
  if2_init();
  if( divide_init() ) return 1;
  scld_init();
  ula_init();
  ay_init();
  slt_init();
  profile_init();
  kempmouse_init();
  fuller_init();
  melodik_init();
  speccyboot_init();
  specdrum_init();
  spectranet_init();
  machines_periph_init();

  z80_init();

  if( timer_init() ) return 1;

  error = timer_estimate_reset(); if( error ) return error;

  error = machine_init_machines();
  if( error ) return error;

  error = machine_select_id( settings_current.start_machine );
  if( error ) return error;

  tape_init();

  error = scaler_select_id( start_scaler ); libspectrum_free( start_scaler );
  if( error ) return error;

  if( setup_start_files( &start_files ) ) return 1;
  if( parse_nonoption_args( argc, argv, first_arg, &start_files ) ) return 1;
  if( do_start_files( &start_files ) ) return 1;

  /* Must do this after all subsytems are initialised */
  debugger_command_evaluate( settings_current.debugger_command );

  if( ui_mouse_present ) ui_mouse_grabbed = ui_mouse_grab( 1 );

  fuse_emulation_paused = 0;
  movie_init();

  return 0;
}

static
int creator_init( void )
{
  size_t i;
  unsigned int version[4] = { 0, 0, 0, 0 };
  char *custom, osname[ 256 ];
  static const size_t CUSTOM_SIZE = 256;
  
  libspectrum_error error; int sys_error;

  const char *gcrypt_version;

  sscanf( VERSION, "%u.%u.%u.%u",
	  &version[0], &version[1], &version[2], &version[3] );

  for( i=0; i<4; i++ ) if( version[i] > 0xff ) version[i] = 0xff;

  sys_error = compat_osname( osname, sizeof( osname ) );
  if( sys_error ) return 1;

  fuse_creator = libspectrum_creator_alloc();

  error = libspectrum_creator_set_program( fuse_creator, "Fuse" );
  if( error ) { libspectrum_creator_free( fuse_creator ); return error; }

  error = libspectrum_creator_set_major( fuse_creator,
					 version[0] * 0x100 + version[1] );
  if( error ) { libspectrum_creator_free( fuse_creator ); return error; }

  error = libspectrum_creator_set_minor( fuse_creator,
					 version[2] * 0x100 + version[3] );
  if( error ) { libspectrum_creator_free( fuse_creator ); return error; }

  custom = libspectrum_malloc( CUSTOM_SIZE );

  gcrypt_version = libspectrum_gcrypt_version();
  if( !gcrypt_version ) gcrypt_version = "not available";

  snprintf( custom, CUSTOM_SIZE,
	    "gcrypt: %s\nlibspectrum: %s\nuname: %s", gcrypt_version,
	    libspectrum_version(), osname );

  error = libspectrum_creator_set_custom(
    fuse_creator, (libspectrum_byte*)custom, strlen( custom )
  );
  if( error ) {
    libspectrum_free( custom ); libspectrum_creator_free( fuse_creator );
    return error;
  }

  return 0;
}

static void fuse_show_copyright(void)
{
  printf( "\n" );
  fuse_show_version();
  printf(
   "Copyright (c) 1999-2011 Philip Kendall and others; see the file\n"
   "'AUTHORS' for more details.\n"
   "\n"
   "For help, please mail <fuse-emulator-devel@lists.sf.net> or use\n"
   "the forums at <http://sourceforge.net/forum/?group_id=91293>.\n"
   "\n"
   "This program is distributed in the hope that it will be useful,\n"
   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
   "GNU General Public License for more details.\n\n");
}

static void fuse_show_version( void )
{
  printf( "The Free Unix Spectrum Emulator (Fuse) version " VERSION ".\n" );
}

static void fuse_show_help( void )
{
  printf( "\n" );
  fuse_show_version();
  printf(
   "\nAvailable command-line options:\n\n"
   "Boolean options (use `--no-<option>' to turn off):\n\n"
   "--auto-load            Automatically load tape files when opened.\n"
   "--beeper-stereo        Add fake stereo to beeper emulation.\n"
   "--compress-rzx         Write RZX files out compressed.\n"
   "--double-screen        Write screenshots out as double size.\n"
   "--issue2               Emulate an Issue 2 Spectrum.\n"
   "--kempston             Emulate the Kempston joystick on QAOP<space>.\n"
   "--loading-sound        Emulate the sound of tapes loading.\n"
   "--separation           Use ACB stereo for the AY-3-8912 sound chip.\n"
   "--sound                Produce sound.\n"
   "--sound-force-8bit     Generate 8-bit sound even if 16-bit is available.\n"
   "--slt                  Turn SLT traps on.\n"
   "--traps                Turn tape traps on.\n\n"
   "Other options:\n\n"
   "--help                 This information.\n"
   "--machine <type>       Which machine should be emulated?\n"
   "--playback <filename>  Play back RZX file <filename>.\n"
   "--record <filename>    Record to RZX file <filename>.\n"
   "--snapshot <filename>  Load snapshot <filename>.\n"
   "--speed <percentage>   How fast should emulation run?\n"
   "--fb-mode <mode>       Which mode should be used for FB?\n"
   "--tape <filename>      Open tape file <filename>.\n"
   "--version              Print version number and exit.\n\n" );
}

/* Stop all activities associated with actual Spectrum emulation */
int fuse_emulation_pause(void)
{
  int error;

  /* If we were already paused, just return. In any case, increment
     the pause count */
  if( fuse_emulation_paused++ ) return 0;

  /* Stop recording any competition mode RZX file */
  if( rzx_recording && rzx_competition_mode ) {
    ui_error( UI_ERROR_INFO, "Stopping competition mode RZX recording" );
    error = rzx_stop_recording(); if( error ) return error;
  }
      
  /* If we had sound enabled (and hence doing the speed regulation),
     turn it off */
  sound_pause();

  return 0;
}

/* Restart emulation activities */
int fuse_emulation_unpause(void)
{
  int error;

  /* If this doesn't start us running again, just return. In any case,
     decrement the pause count */
  if( --fuse_emulation_paused ) return 0;

  /* If we now want sound, enable it */
  sound_unpause();

  /* Restart speed estimation with no information */
  error = timer_estimate_reset(); if( error ) return error;

  return 0;
}

static int
setup_start_files( start_files_t *start_files )
{
  start_files->disk_plus3 = settings_current.plus3disk_file;
  start_files->disk_opus = settings_current.opusdisk_file;
  start_files->disk_plusd = settings_current.plusddisk_file;
  start_files->disk_beta = settings_current.betadisk_file;
  start_files->dock = settings_current.dck_file;
  start_files->if2 = settings_current.if2_file;
  start_files->playback = settings_current.playback_file;
  start_files->recording = settings_current.record_file;
  start_files->snapshot = settings_current.snapshot;
  start_files->tape = settings_current.tape_file;

  start_files->simpleide_master =
    utils_safe_strdup( settings_current.simpleide_master_file );
  start_files->simpleide_slave = 
    utils_safe_strdup( settings_current.simpleide_slave_file );

  start_files->zxatasp_master =
    utils_safe_strdup( settings_current.zxatasp_master_file );
  start_files->zxatasp_slave =
    utils_safe_strdup( settings_current.zxatasp_slave_file );

  start_files->zxcf = utils_safe_strdup( settings_current.zxcf_pri_file );

  start_files->divide_master =
    utils_safe_strdup( settings_current.divide_master_file );
  start_files->divide_slave =
    utils_safe_strdup( settings_current.divide_slave_file );

  start_files->mdr[0] = settings_current.mdr_file;
  start_files->mdr[1] = settings_current.mdr_file2;
  start_files->mdr[2] = settings_current.mdr_file3;
  start_files->mdr[3] = settings_current.mdr_file4;
  start_files->mdr[4] = settings_current.mdr_file5;
  start_files->mdr[5] = settings_current.mdr_file6;
  start_files->mdr[6] = settings_current.mdr_file7;
  start_files->mdr[7] = settings_current.mdr_file8;

  return 0;
}

/* Make 'best guesses' as to what to do with non-option arguments */
static int
parse_nonoption_args( int argc, char **argv, int first_arg,
		      start_files_t *start_files )
{
  size_t i, j;
  const char *filename;
  utils_file file;
  libspectrum_id_t type;
  libspectrum_class_t class;
  int error;

#ifdef GEKKO
  /* No argv on the Wii. Just return */
  return 0;
#endif

  for( i = first_arg; i < (size_t)argc; i++ ) {

    filename = argv[i];

    error = utils_read_file( filename, &file );
    if( error ) return error;

    error = libspectrum_identify_file_with_class( &type, &class, filename,
						  file.buffer, file.length );
    if( error ) {
      utils_close_file( &file );
      return error;
    }

    switch( class ) {

    case LIBSPECTRUM_CLASS_CARTRIDGE_TIMEX:
      start_files->dock = filename; break;

    case LIBSPECTRUM_CLASS_CARTRIDGE_IF2:
      start_files->if2 = filename; break;

    case LIBSPECTRUM_CLASS_HARDDISK:
      if( settings_current.zxcf_active ) {
	start_files->zxcf = filename;
      } else if( settings_current.zxatasp_active ) {
	start_files->zxatasp_master = filename;
      } else if( settings_current.simpleide_active ) {
	start_files->simpleide_master = filename;
      } else if( settings_current.divide_enabled ) {
	start_files->divide_master = filename;
      } else {
	/* No IDE interface active, so activate the ZXCF */
	settings_current.zxcf_active = 1;
	start_files->zxcf = filename;
      }
      break;

    case LIBSPECTRUM_CLASS_DISK_PLUS3:
      start_files->disk_plus3 = filename; break;

    case LIBSPECTRUM_CLASS_DISK_OPUS:
      start_files->disk_opus = filename; break;

    case LIBSPECTRUM_CLASS_DISK_PLUSD:
      start_files->disk_plusd = filename; break;

    case LIBSPECTRUM_CLASS_DISK_TRDOS:
      start_files->disk_beta = filename; break;

    case LIBSPECTRUM_CLASS_DISK_GENERIC:
      if( machine_current->machine == LIBSPECTRUM_MACHINE_PLUS3 ||
          machine_current->machine == LIBSPECTRUM_MACHINE_PLUS2A )
        start_files->disk_plus3 = filename;
      else if( machine_current->machine == LIBSPECTRUM_MACHINE_PENT ||
          machine_current->machine == LIBSPECTRUM_MACHINE_PENT512 ||
          machine_current->machine == LIBSPECTRUM_MACHINE_PENT1024 ||
          machine_current->machine == LIBSPECTRUM_MACHINE_SCORP )
        start_files->disk_beta = filename; 
      else {
        if( periph_is_active( PERIPH_TYPE_BETA128 ) )
          start_files->disk_beta = filename; 
        else if( periph_is_active( PERIPH_TYPE_PLUSD ) )
          start_files->disk_plusd = filename;
        else if( periph_is_active( PERIPH_TYPE_OPUS ) )
          start_files->disk_opus = filename;
      }
      break;

    case LIBSPECTRUM_CLASS_RECORDING:
      start_files->playback = filename; break;

    case LIBSPECTRUM_CLASS_SNAPSHOT:
      start_files->snapshot = filename; break;

    case LIBSPECTRUM_CLASS_MICRODRIVE:
      for( j = 0; j < 8; j++ ) {
        if( !start_files->mdr[j] ) {
	  start_files->mdr[j] = filename;
	  break;
	}
      }
      break;

    case LIBSPECTRUM_CLASS_TAPE:
      start_files->tape = filename; break;

    case LIBSPECTRUM_CLASS_AUXILIARY:
      if( type == LIBSPECTRUM_ID_AUX_POK ) {
        pokemem_set_pokfile( filename );
      }
      break;

    case LIBSPECTRUM_CLASS_UNKNOWN:
      ui_error( UI_ERROR_WARNING, "couldn't identify '%s'; ignoring it",
		filename );
      break;

    default:
      ui_error( UI_ERROR_ERROR, "parse_nonoption_args: unknown file class %d",
		class );
      break;

    }

    utils_close_file( &file );
  }

  return 0;
}

static int
do_start_files( start_files_t *start_files )
{
  int autoload, error, i, check_snapshot;

  /* Can't do both input recording and playback */
  if( start_files->playback && start_files->recording ) {
    ui_error(
      UI_ERROR_WARNING,
      "can't do both input playback and recording; recording disabled"
    );
    start_files->recording = NULL;
  }

  /* Can't use both +3 and TR-DOS disks simultaneously */
  if( start_files->disk_plus3 && start_files->disk_beta ) {
    ui_error(
      UI_ERROR_WARNING,
      "can't use +3 and TR-DOS disks simultaneously; +3 disk ignored"
    );
    start_files->disk_plus3 = NULL;
  }

  /* Can't use disks and the dock simultaneously */
  if( ( start_files->disk_plus3 || start_files->disk_beta ) &&
      start_files->dock                                         ) {
    ui_error(
      UI_ERROR_WARNING,
      "can't use disks and the dock simultaneously; dock cartridge ignored"
    );
    start_files->dock = NULL;
  }

  /* Can't use disks and the Interface II simultaneously */
  if( ( start_files->disk_plus3 || start_files->disk_beta ) &&
      start_files->if2                                          ) {
    ui_error(
      UI_ERROR_WARNING,
      "can't use disks and the Interface II simultaneously; cartridge ignored"
    );
    start_files->if2 = NULL;
  }

  /* If a snapshot has been specified, don't autoload tape, disks etc */
  autoload = start_files->snapshot ? 0 : tape_can_autoload();

  /* Load in each of the files. Input recording must be done after
     snapshot loading such that the right snapshot is embedded into
     the file; input playback being done after snapshot loading means
     any embedded snapshot in the input recording will override any
     specified snapshot */

  if( start_files->disk_plus3 ) {
    error = utils_open_file( start_files->disk_plus3, autoload, NULL );
    if( error ) return error;
  }

  if( start_files->disk_plusd ) {
    error = utils_open_file( start_files->disk_plusd, autoload, NULL );
    if( error ) return error;
  }

  if( start_files->disk_opus ) {
    error = utils_open_file( start_files->disk_opus, autoload, NULL );
    if( error ) return error;
  }

  if( start_files->disk_beta ) {
    error = utils_open_file( start_files->disk_beta, autoload, NULL );
    if( error ) return error;
  }

  if( start_files->dock ) {
    error = utils_open_file( start_files->dock, autoload, NULL );
    if( error ) return error;
  }

  if( start_files->if2 ) {
    error = utils_open_file( start_files->if2, autoload, NULL );
    if( error ) return error;
  }

  if( start_files->snapshot ) {
    error = utils_open_file( start_files->snapshot, autoload, NULL );
    if( error ) return error;
  }

  if( start_files->tape ) {
    error = utils_open_file( start_files->tape, autoload, NULL );
    if( error ) return error;
  }

  /* Microdrive cartridges */

  for( i = 0; i < 8; i++ ) {
    if( start_files->mdr[i] ) {
      error = utils_open_file( start_files->mdr[i], autoload, NULL );
      if( error ) return error;
    }
  }

  /* IDE hard disk images */

  if( start_files->simpleide_master ) {
    error = simpleide_insert( start_files->simpleide_master,
			      LIBSPECTRUM_IDE_MASTER );
    simpleide_reset( 0 );
    if( error ) return error;
  }

  if( start_files->simpleide_slave ) {
    error = simpleide_insert( start_files->simpleide_slave,
			      LIBSPECTRUM_IDE_SLAVE );
    simpleide_reset( 0 );
    if( error ) return error;
  }

  if( start_files->zxatasp_master ) {
    error = zxatasp_insert( start_files->zxatasp_master,
			    LIBSPECTRUM_IDE_MASTER );
    if( error ) return error;
  }

  if( start_files->zxatasp_slave ) {
    error = zxatasp_insert( start_files->zxatasp_slave,
			    LIBSPECTRUM_IDE_SLAVE );
    if( error ) return error;
  }

  if( start_files->zxcf ) {
    error = zxcf_insert( start_files->zxcf ); if( error ) return error;
  }

  if( start_files->divide_master ) {
    error = divide_insert( start_files->divide_master,
			    LIBSPECTRUM_IDE_MASTER );
    if( error ) return error;
  }

  if( start_files->divide_slave ) {
    error = divide_insert( start_files->divide_slave,
			    LIBSPECTRUM_IDE_SLAVE );
    if( error ) return error;
  }

  /* Input recordings */

  if( start_files->playback ) {
    check_snapshot = start_files->snapshot ? 0 : 1;
    error = rzx_start_playback( start_files->playback, check_snapshot );
    if( error ) return error;
  }

  if( start_files->recording ) {
    error = rzx_start_recording( start_files->recording,
				 settings_current.embed_snapshot );
    if( error ) return error;
  }

  return 0;
}

/* Tidy-up function called at end of emulation */
static int fuse_end(void)
{
  movie_stop();		/* stop movie recording */
  /* Must happen before memory is deallocated as we read the character
     set from memory for the text output */
  printer_end();

  /* also required before memory is deallocated on Fuse for OS X where
     settings need to look up machine names etc. */
  settings_end();

  psg_end();
  rzx_end();
  tape_end();
  debugger_end();
  simpleide_end();
  zxatasp_end();
  zxcf_end();
  if1_end();
  divide_end();
  beta_end();
  opus_end();
  plusd_end();
  disciple_end();
  spectranet_end();
  speccyboot_end();

  machine_end();

  timer_end();

  sound_end();
  event_end();
  periph_end();
  fuse_keyboard_end();
  fuse_joystick_end();
  ui_end();
  memory_end();
  mempool_end();
  module_end();
  pokemem_end();

  libspectrum_creator_free( fuse_creator );
  libspectrum_end();

  return 0;
}

/* Emergency shutdown */
void
fuse_abort( void )
{
  fuse_end();
  abort();
}
