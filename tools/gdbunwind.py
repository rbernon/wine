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

from gdb.unwinder import Unwinder
import gdb
import re


def registers(pc, sp, pending_frame):
    ptr = gdb.lookup_type('void').pointer()
    frame = sp

    if ptr.sizeof == 4:
        frame += 16 if 'unix_call' in str(pc) else 32
    if ptr.sizeof == 8 and 'syscall' in str(pc):
        rbp = pending_frame.read_register("rbp")
        frame = rbp - 0x98

    frame = gdb.execute(f'print *(struct syscall_frame *){int(frame)}',
                        from_tty=False, to_string=True)

    for line in frame.splitlines()[1:]:
        if 'prev_frame' in line: break
        if 'syscall_cfa' in line: break
        if 'restore_flags' in line: continue
        if 'syscall_flags' in line: continue

        reg, _, val = line.strip(',').split()
        val = gdb.Value(int(val)).cast(ptr)

        if reg in ('eflags', 'cs', 'ss', 'ds', 'es', 'fs', 'gs'):
            int32 = gdb.lookup_type('int')
            val = val.cast(int32)

        yield reg, val


class WineSyscallFrameId(object):
    def __init__(self, sp, pc):
        self.sp = sp
        self.pc = pc


class WineSyscallUnwinder(Unwinder):
    def __init__(self):
        super().__init__("WineSyscallUnwinder")
        self.pattern = re.compile('__wine_(syscall|unix_call)')

    def __call__(self, pending_frame):
        pc = pending_frame.read_register("pc")
        if self.pattern.search(str(pc)) is None:
            return None

        sp = pending_frame.read_register("sp")
        frame = WineSyscallFrameId(sp, pc)

        unwind_info = pending_frame.create_unwind_info(frame)
        for reg, val in registers(pc, sp, pending_frame):
            unwind_info.add_saved_register(reg, val)
        return unwind_info


gdb.unwinder.register_unwinder(None, WineSyscallUnwinder(), replace=True)
