#!/usr/bin/env python3
"""Package a GIGA R1 sketch .bin as an Arduino_Portenta_OTA .ota image.

Format (read off the served pomona-2.3.0.ota and the library's utility.cpp):
  [0:4]  length of everything after the CRC field (magic + version + payload), LE
  [4:8]  CRC-32 (zlib polynomial, standard init/final xor) of those bytes, LE
  [8:12] magic 0x23410266 (VID 0x2341 GIGA R1 WiFi, PID 0x0266), LE
  [12:20] 8-byte header version word — copied verbatim from the 2.3.0 image
  [20:]  the .bin, LZSS-compressed (Okumura: EI=11, EJ=4, N=2048, F=17,
         window pre-filled with spaces, MSB-first bit stream) — the exact
         inverse of LZSSDecoder in src/decompress/lzss.cpp.

Usage: lzss_ota.py <in.bin> <out.ota> [--ref pomona-2.3.0.ota]
Self-test: decodes the reference image with the decoder below, round-trips
the input through encode/decode, and checks the reference header's CRC.
"""
import struct, sys, zlib

EI, EJ = 11, 4
N = 1 << EI
F = (1 << EJ) + 1
P = 1
MAGIC = 0x23410266


class BitWriter:
    def __init__(self):
        self.out = bytearray()
        self.buf = 0
        self.mask = 0x80

    def putbit(self, b):
        if b:
            self.buf |= self.mask
        self.mask >>= 1
        if self.mask == 0:
            self.out.append(self.buf)
            self.buf = 0
            self.mask = 0x80

    def putbits(self, n, x):
        for k in range(n - 1, -1, -1):
            self.putbit((x >> k) & 1)

    def flush(self):
        if self.mask != 0x80:
            self.out.append(self.buf)
        return bytes(self.out)


def lzss_encode(data: bytes) -> bytes:
    w = BitWriter()
    buf = bytearray(b" " * (N - F)) + bytearray(data[: N + F])
    pos = N + F  # next input byte not yet in buf
    bufend = len(buf)
    s, r = 0, N - F
    total = len(data)
    while r < bufend:
        f1 = min(F, bufend - r)
        c = buf[r]
        x, y = 0, 1
        # longest match in [s, r), nearest first on ties (like the reference encoder)
        window = bytes(buf[s : r + f1 - 1])
        rel_r = r - s
        i = window.rfind(c, 0, rel_r)
        while i >= 0:
            j = 1
            while j < f1 and buf[s + i + j] == buf[r + j]:
                j += 1
            if j > y:
                x, y = s + i, j
                if y == f1:
                    break
            i = window.rfind(c, 0, i)
        if y <= P:
            y = 1
            w.putbit(1)
            w.putbits(8, c)
        else:
            w.putbit(0)
            w.putbits(EI, x & (N - 1))
            w.putbits(EJ, y - 2)
        r += y
        s += y
        if r >= N * 2 - F:
            del buf[:N]
            bufend -= N
            r -= N
            s -= N
            while bufend < N * 2 and pos < total:
                buf.append(data[pos])
                pos += 1
                bufend += 1
    return w.flush()


def lzss_decode(payload: bytes) -> bytes:
    out = bytearray()
    ring = bytearray(b" " * N)
    r = N - F
    bitbuf, nbits, p = 0, 0, 0

    def getbits(n):
        nonlocal bitbuf, nbits, p
        while nbits < n:
            if p >= len(payload):
                return None
            bitbuf = (bitbuf << 8) | payload[p]
            p += 1
            nbits += 8
        x = bitbuf >> (nbits - n)
        bitbuf &= (1 << (nbits - n)) - 1
        nbits -= n
        return x

    while True:
        flag = getbits(1)
        if flag is None:
            break
        if flag:
            c = getbits(8)
            if c is None:
                break
            out.append(c)
            ring[r] = c
            r = (r + 1) & (N - 1)
        else:
            i = getbits(EI)
            if i is None:
                break
            j = getbits(EJ)
            if j is None:
                break
            for k in range(j + 2):
                c = ring[(i + k) & (N - 1)]
                out.append(c)
                ring[r] = c
                r = (r + 1) & (N - 1)
    return bytes(out)


def package(bin_data: bytes, version_word: bytes) -> bytes:
    payload = lzss_encode(bin_data)
    rest = struct.pack("<I", MAGIC) + version_word + payload
    return struct.pack("<II", len(rest), zlib.crc32(rest) & 0xFFFFFFFF) + rest


def main():
    args = sys.argv[1:]
    ref = None
    if "--ref" in args:
        k = args.index("--ref")
        ref = args[k + 1]
        del args[k : k + 2]
    src, dst = args[0], args[1]
    version_word = bytes.fromhex("0000000000000040")
    if ref:
        d = open(ref, "rb").read()
        ln, crc = struct.unpack("<II", d[:8])
        assert ln == len(d) - 8, f"ref length field {ln} != {len(d) - 8}"
        assert crc == (zlib.crc32(d[8:]) & 0xFFFFFFFF), "ref CRC is not zlib crc32 of bytes[8:]"
        assert struct.unpack("<I", d[8:12])[0] == MAGIC, "ref magic mismatch"
        version_word = d[12:20]
        dec = lzss_decode(d[20:])
        print(f"ref: header ok; version word {version_word.hex()}; payload {len(d) - 20} B decodes to {len(dec)} B, "
              f"first bytes {dec[:8].hex()}")
        re = lzss_encode(dec)
        assert lzss_decode(re) == dec, "round-trip of the reference binary failed"
        print(f"ref: re-encoded {len(re)} B (original payload {len(d) - 20} B); round-trip ok")
    data = open(src, "rb").read()
    payload = lzss_encode(data)
    assert lzss_decode(payload) == data, "round-trip of the input failed"
    img = package(data, version_word)
    open(dst, "wb").write(img)
    print(f"wrote {dst}: {len(img)} B ({len(data)} B bin -> {len(payload)} B lzss); "
          f"first bytes {img[:20].hex()}")


if __name__ == "__main__":
    main()
