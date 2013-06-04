#!/usr/bin/python

import sys
import struct

class Teledisk(object):
	def __init__(self, buf):
		self.buf = buf
		self.data = {}
	def read(self, fmt):
		fmt = "<" + fmt # ?
		size = struct.calcsize(fmt)
		data, self.buf = self.buf[:size], self.buf[size:]
		return struct.unpack(fmt, data)
	def parse(self):
		signature, seq, check_seq, version, data_rate, drive_type, stepping, dos_alloc_flag, sides, crc = \
			self.read("2s BBBBBBBBH")
		#print signature, seq, check_seq, version, data_rate, drive_type, stepping, dos_alloc_flag, sides, crc
		assert signature == "TD"
		# TODO: comment
		while self.parse_track():
			pass
	def parse_track(self):
		n_sec, cyl, head, crc = self.read("BBBB")
		if n_sec == 0xff:
			return False
		for sec in xrange(n_sec):
			self.data[(head, cyl, sec)] = self.parse_sector()
		return True
	def parse_sector(self):
		cyl, head, sec, sec_len, flags, crc = self.read("BBBBBB")
		if flags & 0x30:
			return
		sec_len = 0x80 << sec_len
		block_len, encoding = self.read("HB")
		if encoding == 0:
			data = self.read("%ds" % sec_len)[0]
		elif encoding == 1:
			data = ""
			while len(data) < sec_len:
				cnt, value = self.read("H 2s")
				data += value * cnt
		elif encoding == 2:
			data = ""
			while len(data) < sec_len:
				cnt, = self.read("B")
				if cnt == 0:
					length, = self.read("B")
					data += self.read("%ds" % length)[0]
				else:
					length = cnt * 2
					repeat, = self.read("B")
					data += self.read("%ds" % length)[0] * repeat
		else:
			assert encoding in (0, 1, 2)
		return data

	def dump(self, fp):
		seclen = min((len(x) for x in self.data.values()))
		assert seclen == max((len(x) for x in self.data.values()))
		sides = 1 + max(head for head, cyl, sec in self.data.keys())
		cyls = 1 + max(cyl for head, cyl, sec in self.data.keys())
		sectors = 1 + max(sec for head, cyl, sec in self.data.keys())
		fp.write("Aley's disk backup%c%c%c%c" % (sides, cyls, sectors, seclen / 64))
		for side in range(sides):
			for cyl in range(cyls):
				for sec in range(sectors):
					fp.write(self.data[(side, cyl, sec)])

if __name__ == "__main__":
	td = Teledisk(sys.stdin.read())
	td.parse()
	td.dump(sys.stdout)
