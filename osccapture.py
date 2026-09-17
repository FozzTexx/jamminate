#!/usr/bin/env python3
"""Show exactly what an OSC controller puts on the wire.

Stands in for FujiNet rather than for the controller: it listens, so point
TouchOSC at this machine's address and the port below instead of at the CoCo.
Nothing else needs to be running -- no emulator, no fujinet-pc.

It exists to settle two questions that cannot be answered by reading code:

  1. Does the controller escape the frame-marker byte? SLIP (RFC 1055) requires
     192 -> 219,220 and 219 -> 219,221. Jamminate does NOT undo those
     substitutions, so seeing whether any 219s ever arrive tells us whether
     that matters in practice.
  2. What argument type does it send? Jamminate decides note on vs off from the
     FIRST byte of the argument, which is correct for a float and backwards for
     an integer.

Every frame is reported twice: as it arrived, and with the substitutions undone
the way a correct reader would. If those two differ, Jamminate is reading the
altered bytes.
"""

import argparse
import socket
import struct
import sys
import time

SLIP_END = 0xC0
SLIP_ESC = 0xDB
SLIP_ESC_END = 0xDC
SLIP_ESC_ESC = 0xDD


class Stats:
    def __init__(self):
        self.frames = 0
        self.escapes = 0
        self.bad = 0
        self.types = {}
        self.addrs = {}
        self.raw = bytearray()


def unescape(data):
    """Undo SLIP substitutions; returns (bytes, count_undone)."""
    out = bytearray()
    n = 0
    i = 0
    while i < len(data):
        if data[i] == SLIP_ESC and i + 1 < len(data):
            nxt = data[i + 1]
            if nxt == SLIP_ESC_END:
                out.append(SLIP_END); n += 1; i += 2; continue
            if nxt == SLIP_ESC_ESC:
                out.append(SLIP_ESC); n += 1; i += 2; continue
        out.append(data[i])
        i += 1
    return bytes(out), n


def osc_decode(payload):
    """Decode one OSC message, padding each field over its null as the spec says."""
    def field(buf, at):
        end = buf.index(b"\0", at)
        text = buf[at:end].decode("ascii", "replace")
        return text, at + ((end - at + 4) // 4) * 4

    try:
        addr, pos = field(payload, 0)
        types, pos = field(payload, pos)
        args = []
        for t in types[1:]:
            if t == "f":
                args.append(struct.unpack(">f", payload[pos:pos + 4])[0]); pos += 4
            elif t == "i":
                args.append(struct.unpack(">i", payload[pos:pos + 4])[0]); pos += 4
            elif t == "s":
                s, pos = field(payload, pos); args.append(s)
            else:
                args.append("<%s?>" % t)
        return addr, types, args
    except Exception as exc:
        return None, None, str(exc)


def report(frame, st):
    st.frames += 1
    cooked, n = unescape(frame)
    st.escapes += n

    print("  frame %3d bytes  %s" % (len(frame), frame.hex()))
    if n:
        print("    %d escape(s) undone -> %s" % (n, cooked.hex()))
        print("    NOTE: Jamminate would read the escaped form, not this.")

    addr, types, args = osc_decode(cooked)
    if addr is None:
        st.bad += 1
        print("    not decodable as OSC: %s" % args)
        return

    st.types[types] = st.types.get(types, 0) + 1
    st.addrs[addr] = st.addrs.get(addr, 0) + 1
    print("    address %s   types %s   args %s" % (addr, types, args))

    # what Jamminate itself would conclude
    try:
        pos = ((len(addr) + 4) // 4) * 4
        pos = pos + ((len(types) + 4) // 4) * 4
        first = cooked[pos]
        print("    Jamminate reads first arg byte 0x%02x -> %s" %
              (first, "note ON" if first else "note OFF"))
    except IndexError:
        print("    Jamminate would find no argument byte")


def feed(buf, st):
    """Pull complete frames out of the held buffer."""
    while True:
        try:
            a = buf.index(SLIP_END)
        except ValueError:
            return
        while a + 1 < len(buf) and buf[a + 1] == SLIP_END:
            a += 1
        try:
            b = buf.index(SLIP_END, a + 1)
        except ValueError:
            del buf[:a]
            return
        if b > a + 1:
            report(bytes(buf[a + 1:b]), st)
        del buf[:b]
        if buf and buf[0] == SLIP_END:
            del buf[:1]


def summarise(st, path):
    print("\n" + "=" * 62)
    print("frames decoded      : %d  (%d undecodable)" % (st.frames, st.bad))
    print("argument types seen : %s" %
          (", ".join("%s x%d" % kv for kv in st.types.items()) or "none"))
    print("addresses seen      : %s" %
          (", ".join(sorted(st.addrs)[:12]) or "none"))
    print()
    if st.escapes:
        print("ESCAPING: YES -- %d substitution(s) seen. The controller follows"
              % st.escapes)
        print("  the spec. Jamminate does not undo them, so any argument")
        print("  containing 192 or 219 reaches it altered.")
    else:
        print("ESCAPING: none seen. Either no argument contained 192 or 219")
        print("  (expected for buttons sending only 0.0 and 1.0), or the")
        print("  controller does not escape. Sweep a fader and look again.")
    print()
    if "\x2cf" in st.types or ",f" in st.types:
        print("ARGUMENTS: float -- what Jamminate's first-byte test expects.")
    elif ",i" in st.types:
        print("ARGUMENTS: integer -- Jamminate's first-byte test reads every")
        print("  note-on as a note-off with this type. This would be the bug.")
    if path:
        open(path, "wb").write(bytes(st.raw))
        print("\nraw bytes written to %s" % path)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=6573)
    ap.add_argument("--bind", default="0.0.0.0",
                    help="0.0.0.0 so the controller can reach us over the network")
    ap.add_argument("--udp", action="store_true",
                    help="listen on UDP instead, if the controller is set that way")
    ap.add_argument("--save", metavar="FILE", help="write the raw bytes too")
    args = ap.parse_args()

    st = Stats()
    buf = bytearray()

    try:
        if args.udp:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind((args.bind, args.port))
            print("listening on UDP %s:%d -- ctrl-c to stop\n" % (args.bind, args.port))
            while True:
                data, who = s.recvfrom(4096)
                st.raw += data
                print("datagram %d bytes from %s:%d  %s" %
                      (len(data), who[0], who[1], data.hex()))
                if data and data[0] != SLIP_END:
                    print("  no frame marker: UDP usually carries one bare OSC")
                    print("  message per datagram, with no SLIP framing at all.")
                    report(data, st)
                else:
                    buf += data
                    feed(buf, st)
        else:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((args.bind, args.port))
            s.listen(1)
            print("listening on TCP %s:%d -- ctrl-c to stop" % (args.bind, args.port))
            conn, who = s.accept()
            print("connection from %s:%d\n" % who)
            first = True
            while True:
                data = conn.recv(4096)
                if not data:
                    print("\npeer closed")
                    break
                st.raw += data
                print("chunk %d bytes  %s" % (len(data), data.hex()))
                if first:
                    first = False
                    if data[0] != SLIP_END:
                        print("  WARNING: does not begin with the 192 frame marker.")
                        print("  If the first four bytes are a length, the")
                        print("  controller is using length-prefixed framing and")
                        print("  Jamminate will not understand it at all.")
                buf += data
                feed(buf, st)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        summarise(st, args.save)
    return 0


if __name__ == "__main__":
    sys.exit(main())
