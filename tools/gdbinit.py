#!/bin/env python3

# Copyright 2021 Rémi Bernon for CodeWeavers
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA

from __future__ import print_function

import gdb
import re
import subprocess
import sys

class LoadSymbolFiles(gdb.Command):
  'Command to load symbol files directly from /proc/<pid>/maps.'
  
  def __init__(self):
    sup = super(LoadSymbolFiles, self)
    sup.__init__('load-symbol-files', gdb.COMMAND_FILES, gdb.COMPLETE_NONE,
                 False)

    self.libs = {}
    gdb.execute('alias -a lsf = load-symbol-files', True)

  def invoke(self, arg, from_tty):
    pid = gdb.selected_inferior().pid
    if not pid in self.libs: self.libs[pid] = {}

    def command(cmd, confirm=from_tty):
      to_string = not from_tty
      gdb.execute(cmd, from_tty=confirm, to_string=to_string)

    def execute(cmd):
      return subprocess.check_output(cmd, stderr=subprocess.STDOUT) \
                       .decode('utf-8')

    # load mappings addresses
    libs = {}
    with open('/proc/{}/maps'.format(pid), 'r') as maps:
      for line in maps:
        addr, _, _, _, node, path = re.split(r'\s+', line, 5)
        path = path.strip()
        if node == '0': continue
        if path in libs: continue
        libs[path] = int(addr.split('-')[0], 16)

    # unload symbol file if address changed
    for k in set(libs) & set(self.libs[pid]):
      if libs[k] != self.libs[pid][k]:
        command('remove-symbol-file "{}"'.format(k), confirm=False)
        del self.libs[k]

    # load symbol file for new mappings
    for k in set(libs) - set(self.libs[pid]):
        if arg is not None and re.search(arg, k) is None: continue
        addr = self.libs[pid][k] = libs[k]
        offs = None

        try:
          out = execute(['file', k])
        except:
          continue

        # try loading mapping as ELF
        try:
          out = execute(['readelf', '-l', k])
          for line in out.split('\n'):
            if not 'LOAD' in line: continue
            base = int(line.split()[2], 16)
            break

          out = execute(['objdump', '-h', k])
          for line in out.split('\n'):
            if not '.text' in line: continue
            offs = int(line.split()[3], 16) - base
            break
          if offs is None: continue

        # try again, assuming mapping is PE
        except:
          try:
            out = execute(['objdump', '-h', k])
            for line in out.split('\n'):
              if not '.text' in line: continue
              offs = int(line.split()[5], 16)
              break
            if offs is None: continue

          except:
            continue

        command('add-symbol-file "{}" 0x{:x}'.format(k, addr + offs),
                confirm=False)


LoadSymbolFiles()
