#!/usr/bin/perl

use warnings;
use strict;

sub write_byte {

  my( $b ) = @_;

  printf "%c", $b;
}

sub write_word {

  my( $w ) = @_;

  write_byte( $w % 0x100 );
  write_byte( $w / 0x100 );
}

sub write_three {

  my( $three ) = @_;

  write_byte( $three % 0x100 );
  write_word( $three / 0x100 );

}

sub write_dword {

  my( $dw ) = @_;

  write_word( $dw % 0x10000 );
  write_word( $dw / 0x10000 );
}

sub write_header {

  print "ZXTape!\x1a";		# Signature
  write_byte(  1 );		# Major version number
  write_byte( 20 );		# Minor version number

}

sub write_standard_speed_data_block {

  my( $data, $pause ) = @_;

  write_byte( 0x10 );
  write_word( $pause );
  write_word( length $data );
  print $data;

}

sub write_turbo_speed_data_block {

  my( $pilot_length, $pilot_count, $sync1_length, $sync2_length,
      $zero_length, $one_length, $data, $bits_in_last_byte, $pause ) = @_;

  write_byte( 0x11 );
  write_word( $pilot_length );
  write_word( $sync1_length );
  write_word( $sync2_length );
  write_word( $zero_length );
  write_word( $one_length );
  write_word( $pilot_count );
  write_byte( $bits_in_last_byte );
  write_word( $pause );
  write_three( length $data );
  print $data;

}

sub write_pure_tone_block {

  my( $length, $count ) = @_;

  write_byte( 0x12 );
  write_word( $length );
  write_word( $count );

}

sub write_pulse_sequence_block {

  my( @data ) = @_;

  write_byte( 0x13 );
  write_byte( scalar @data );
  write_word( $_ ) foreach @data;

}

sub write_pure_data_block {

  my( $zero_length, $one_length, $data, $bits_in_last_byte, $pause ) = @_;

  write_byte( 0x14 );
  write_word( $zero_length );
  write_word( $one_length );
  write_byte( $bits_in_last_byte );
  write_word( $pause );
  write_three( length $data );
  print $data;

}

sub write_pause_block {

  my( $pause ) = @_;

  write_byte( 0x20 );
  write_word( $pause );

}

write_header();

write_standard_speed_data_block( "\xaa", 2345 );

write_turbo_speed_data_block( 1000, 5, 123, 456, 789, 400, "\x00\xff\x55\xa0",
			      4, 987 );

write_pure_tone_block( 535, 666 );

write_pulse_sequence_block( 772, 297, 692 );

write_pure_data_block( 552, 1639, "\xff\x00\xfc", 6, 554 );

write_pause_block( 618 );
