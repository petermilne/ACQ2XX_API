#!/usr/bin/python

import shlex, subprocess

#hexdump -e '1/4 "%10d " 5/4 "%08x " "\n"' DR/XX.acq132_060.SHOT.000004.COOKED/ES-dumpA

DUMPF="DR/XX.acq132_060.SHOT.000004.COOKED/%s" % ("ES-dumpA")
#CMD0= ['hexdump', '-e', '''1/4 "%10d " 5/4 "%08x " "\n"''', DUMPF]
CMD0 = 'hexdump -e \'6/4 "%08x " "\\n"\' ' +  DUMPF


print CMD0

proc = subprocess.Popen(CMD0,
                        shell=True,
                        stdout=subprocess.PIPE,
                        )
pdata = proc.communicate()[0]

print pdata

ii = 0
for line in pdata.split("\n"):
     ii = ii + 1
     if len(line) > 1:
          record = [int(n, 16) for n in line.split(" ")]
          offset = record[0]
          es_aa55 = record[2]
          modulo = (record[3] & 0xff00) >> 8
          lat = (record[3] & 0x00fc) >> 2
          if (record[3] & 1) != 0:
               up = True
          else:
               up = False
          if (record[3] & 2) != 0:
               down = True
          else:
               down = False

          print "%10d %2d %2d %s %s\n" % (offset, modulo, lat, up, down)



