#!/usr/bin/python
#
# Convert a .mgt-layout image into a .sad image, with the specified geometry.
#
# Currently this is most useful for a 2*80*9*512 image, which would otherwise
# be guessed as 2*80*18*256.
#
# Example:
# ./mgt2sad.py 2 80 9 512 < CPM22-HC.IMG > cpm22-hc.sad


import sys

sides, cyls, sectors, seclen = map(int, sys.argv[1:])
sys.stdout.write("Aley's disk backup%c%c%c%c" % (sides, cyls, sectors, seclen / 64))

data = {}

for cyl in range(cyls):
	for side in range(sides):
		data[(side, cyl)] = sys.stdin.read(seclen * sectors)

for side in range(sides):
	for cyl in range(cyls):
		sys.stdout.write(data[(side, cyl)])

