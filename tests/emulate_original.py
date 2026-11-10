#!/usr/bin/env python3
"""Runs the ORIGINAL KerrStinn 1.2 engine code (32-bit Intel slice of the 2007 Mac
binary) inside the Unicorn CPU emulator and compares it, sample for sample,
against the port (engine_cli).

usage: emulate_original.py <KerrStinn.vst/Contents/MacOS/KerrStinn> <build/tests/engine_cli> [cases] [seed]

The binary is the 2007 KerrStinn 1.2 Mac release (not in this repo).

Requires: pip install unicorn numpy
"""
import os
import random
import struct
import subprocess
import sys
import tempfile

import numpy as np
from unicorn import UcError, Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_PROT_ALL
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

# Addresses in the i386 slice (from nm / otool -Iv).
ENGINE_CTOR = 0x1A44E      # engine::engine()
BIG_WORKER = 0x1A8E8       # engine::bigWorker(int)
SET_PARAMETER = 0x1E036    # AKerrStinn::setParameter(int, float)
STUB_FMOD = 0x2F028
STUB_MEMCPY = 0x2F4F1
STUB_RAND = 0x2F578
STUB_SRAND = 0x2F57D

AK = 0x100000              # fake AKerrStinn object
ENGINE = AK + 0xB0
EDITOR = 0x120000          # fake, closed editor
FMOD_CODE = 0x31000
RET_MAGIC = 0x400000
STACK_TOP = 0x7F100000
BUF_BASE = 0x1000000
DEFAULTS = [0.2, 0.121212125, 0.5, 0.3, 0.0, 1.0]


def i386_slice(path):
    data = open(path, "rb").read()
    magic, nfat = struct.unpack(">II", data[:8])
    if magic != 0xCAFEBABE:
        return data
    for i in range(nfat):
        cputype, _, offset, size, _ = struct.unpack(">iiIII", data[8 + 20 * i: 28 + 20 * i])
        if cputype == 7:  # CPU_TYPE_I386
            return data[offset: offset + size]
    raise SystemExit("no i386 slice")


class Original:
    def __init__(self, binary, n):
        self.n = n
        self.next = 1
        uc = self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0, 0x32000, UC_PROT_ALL)
        uc.mem_write(0, binary[:0x30000])

        # fmod(x, y) via x87 fprem, which is exactly C fmod.
        code = bytes([0xDD, 0x44, 0x24, 0x0C, 0xDD, 0x44, 0x24, 0x04,
                      0xD9, 0xF8, 0xDF, 0xE0, 0x9E, 0x7A, 0xF9, 0xDD, 0xD9, 0xC3])
        uc.mem_write(FMOD_CODE, code)
        uc.mem_write(STUB_FMOD, b"\xE9" + struct.pack("<i", FMOD_CODE - (STUB_FMOD + 5)))

        uc.mem_map(AK, 0x40000, UC_PROT_ALL)
        uc.mem_map(RET_MAGIC, 0x1000, UC_PROT_ALL)
        uc.mem_map(STACK_TOP - 0x100000, 0x100000, UC_PROT_ALL)

        # Four buffers (raw L/R, processed L/R) with wide zeroed gaps, since the
        # original reads past the end of its buffers.
        self.stride = ((n * 4 * 3 + 0x10000) + 0xFFF) & ~0xFFF
        uc.mem_map(BUF_BASE, self.stride * 4, UC_PROT_ALL)
        self.bufs = [BUF_BASE + self.stride * k for k in range(4)]

        uc.hook_add(UC_HOOK_CODE, self._rand, begin=STUB_RAND, end=STUB_RAND)
        uc.hook_add(UC_HOOK_CODE, self._srand, begin=STUB_SRAND, end=STUB_SRAND)
        uc.hook_add(UC_HOOK_CODE, self._memcpy, begin=STUB_MEMCPY, end=STUB_MEMCPY)

        self.call(ENGINE_CTOR, ENGINE)
        for k in range(4):
            self.w32(ENGINE + 4 * k, self.bufs[k])
        self.w32(AK + 0x13C64, EDITOR)  # editor with no frame, not open
        self.w32(AK + 0x1C, 0)          # current program
        for i, v in enumerate(DEFAULTS):
            self.uc.mem_write(AK + 0xB93C + 4 * i, struct.pack("<f", v))

    # --- libc stand-ins -----------------------------------------------------
    def _ret(self, value):
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        ret = self.r32(esp)
        self.uc.reg_write(UC_X86_REG_EAX, value & 0xFFFFFFFF)
        self.uc.reg_write(UC_X86_REG_ESP, esp + 4)
        self.uc.reg_write(UC_X86_REG_EIP, ret)

    def _arg(self, i):
        return self.r32(self.uc.reg_read(UC_X86_REG_ESP) + 4 + 4 * i)

    def _rand(self, uc, addr, size, ud):
        x = self.next if self.next != 0 else 123459876
        if x >= 0x80000000:
            x -= 0x100000000
        hi, lo = int(x / 127773), x - int(x / 127773) * 127773
        x = 16807 * lo - 2836 * hi
        if x < 0:
            x += 0x7FFFFFFF
        self.next = x & 0xFFFFFFFF
        self._ret(x % 0x80000000)

    def _srand(self, uc, addr, size, ud):
        self.next = self._arg(0)
        self._ret(0)

    def _memcpy(self, uc, addr, size, ud):
        dst, src, n = self._arg(0), self._arg(1), self._arg(2)
        uc.mem_write(dst, bytes(uc.mem_read(src, n)))
        self._ret(dst)

    # --- helpers -------------------------------------------------------------
    def r32(self, a):
        return struct.unpack("<I", self.uc.mem_read(a, 4))[0]

    def w32(self, a, v):
        self.uc.mem_write(a, struct.pack("<I", v & 0xFFFFFFFF))

    def call(self, fn, *args):
        esp = STACK_TOP - 0x1000
        for a in reversed(args):
            esp -= 4
            self.uc.mem_write(esp, a if isinstance(a, bytes) else struct.pack("<I", a & 0xFFFFFFFF))
        esp -= 4
        self.w32(esp, RET_MAGIC)
        self.uc.reg_write(UC_X86_REG_ESP, esp)
        self.uc.emu_start(fn, RET_MAGIC)

    # --- operations matching engine_cli -------------------------------------
    def set(self, idx, value_bits):
        self.call(SET_PARAMETER, AK, idx, struct.pack("<I", value_bits))

    def load(self, left, right):
        n = self.n
        self.uc.mem_write(ENGINE + 0x44, b"\0")
        for k in range(4):  # engine::Init's memset(buf, 0, n) - n bytes, not floats
            self.uc.mem_write(self.bufs[k], bytes(n))
        self.uc.mem_write(self.bufs[0], left.astype("<f4").tobytes())
        self.uc.mem_write(self.bufs[1], right.astype("<f4").tobytes())
        self.w32(ENGINE + 0x60, n)
        self.uc.mem_write(ENGINE + 0x44, b"\1")
        slices = self.r32(ENGINE + 0x78)
        for i in range(1, slices + 1):
            self.call(BIG_WORKER, ENGINE, i)

    def processed(self):
        n = self.n
        l = np.frombuffer(bytes(self.uc.mem_read(self.bufs[2], n * 4)), "<f4")
        r = np.frombuffer(bytes(self.uc.mem_read(self.bufs[3], n * 4)), "<f4")
        return l, r


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def make_case(rng):
    n = rng.randint(3000, 40000)
    t = np.arange(n)
    left = 0.6 * np.sin(t * rng.uniform(0.001, 0.05)) + np.array([rng.uniform(-0.4, 0.4) for _ in range(n)])
    right = 0.7 * np.sin(t * rng.uniform(0.001, 0.05) + 1.0) * np.exp(-(t % 4000) / 900.0)
    left = np.round(np.clip(left, -1, 1) * 32767) / 32767.0
    right = np.round(np.clip(right, -1, 1) * 32767) / 32767.0
    left, right = left.astype(np.float32), right.astype(np.float32)

    ops = ["init", "load", "set 6 %08x" % fbits(0.0)]
    for _ in range(rng.randint(6, 14)):
        idx = rng.choice([0, 1, 2, 3, 3, 3, 4, 5])
        if idx == 3:
            g = rng.randint(1, 26)
            v = (g - 1.99 + rng.uniform(0.02, 0.98)) / 25.0
        elif idx == 2:
            v = rng.choice([0.0, 1.0, rng.random(), rng.random()])
        else:
            v = rng.random()
        ops.append("set %d %08x" % (idx, fbits(min(max(v, 0.0), 1.0))))
    return n, left, right, ops


def main():
    binary = i386_slice(sys.argv[1])
    cli = sys.argv[2]
    cases = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    rng = random.Random(int(sys.argv[4]) if len(sys.argv) > 4 else 1234)

    total_ops = mismatches = 0
    crashes = []
    active = {}  # generator -> number of ops where it audibly changed the loop
    with tempfile.TemporaryDirectory() as tmp:
        for c in range(cases):
            n, left, right, ops = make_case(rng)
            with open(os.path.join(tmp, "in.bin"), "wb") as f:
                f.write(struct.pack("<i", n) + left.tobytes() + right.tobytes())
            with open(os.path.join(tmp, "ops.txt"), "w") as f:
                f.write("\n".join(ops) + "\n")
            subprocess.run([cli, os.path.join(tmp, "in.bin"), os.path.join(tmp, "ops.txt"),
                            os.path.join(tmp, "out.bin")], check=True)
            port = np.fromfile(os.path.join(tmp, "out.bin"), "<f4").reshape(len(ops), 2, n)

            orig = Original(binary, n)
            for k, op in enumerate(ops):
                parts = op.split()
                if parts[0] == "init":
                    for idx in (0, 1, 2, 3, 5, 4):
                        orig.set(idx, fbits(DEFAULTS[idx]))
                elif parts[0] == "load":
                    orig.load(left, right)
                else:
                    try:
                        orig.set(int(parts[1]), int(parts[2], 16))
                    except UcError as e:
                        eip = orig.uc.reg_read(UC_X86_REG_EIP)
                        crashes.append((c, k, op, orig.r32(ENGINE + 0x84), orig.r32(ENGINE + 0x78), eip, str(e)))
                        print("case %d op %d (%s): ORIGINAL CRASHED in gen %d, slices %d at eip %#x (%s)"
                              % crashes[-1], flush=True)
                        break

                ol, oriR = orig.processed()
                total_ops += 1
                if parts[0] == "set" and orig.r32(ENGINE + 0x44) & 0xFF:
                    changed = not (np.array_equal(ol, left) and np.array_equal(oriR, right))
                    g = orig.r32(ENGINE + 0x84)
                    active[g] = active.get(g, 0) + (1 if changed else 0)
                same = (np.array_equal(ol.view(np.uint32), port[k, 0].view(np.uint32))
                        and np.array_equal(oriR.view(np.uint32), port[k, 1].view(np.uint32)))
                if not same:
                    mismatches += 1
                    d = np.nonzero((ol.view(np.uint32) != port[k, 0].view(np.uint32))
                                   | (oriR.view(np.uint32) != port[k, 1].view(np.uint32)))[0]
                    gen = orig.r32(ENGINE + 0x84)
                    print("case %d op %d (%s) gen %d slices %d: %d samples differ, first at %d "
                          "(orig %r/%r port %r/%r)" % (c, k, op, gen, orig.r32(ENGINE + 0x78), len(d), d[0],
                                                      ol[d[0]], oriR[d[0]], port[k, 0, d[0]], port[k, 1, d[0]]))
            print("case %d: n=%d, %d ops checked" % (c, n, len(ops)), flush=True)

    print("\nops per generator where the original changed the loop:",
          ", ".join("%d:%d" % kv for kv in sorted(active.items())))
    print("%d/%d operations bit-identical, %d cases where the original crashed"
          % (total_ops - mismatches, total_ops, len(crashes)))
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
