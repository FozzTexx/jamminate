#!/usr/bin/env python3
"""Stand in for TouchOSC when testing Jamminate against fujinet-pc.

Jamminate has FujiNet listen on TCP 6573 and expects SLIP-framed OSC, so this
connects to that port and writes frames. Start it once the CoCo has printed
"Waiting for connection".

Arguments default to floats, which is one of the three types a TouchOSC layout
author can pick per message. Jamminate reads the OSC type tag and handles all
of them, so --type f, i and b should all behave identically. They did not
always: it used to test only the first argument byte, which is right for a
float, reads every press as a release for an integer (a small int's first byte
is zero), and reads the closing frame marker as the value for a boolean (which
carries no argument bytes at all), so notes played and never stopped. Use the
three settings to confirm that is really fixed.

Note also that Jamminate does not un-stuff SLIP escapes, so an argument
containing a 0xC0 byte would break framing. 0.0 and 1.0 are safe; a continuous
fader value such as 1.5 (3F C0 00 00) is not.
"""

import argparse
import os
import re
import socket
import struct
import sys
import termios
import time
import tty

SLIP_END = 0xC0

MAJOR = [0, 4, 7]
MINOR = [0, 3, 7]


def osc_string(s):
    b = s.encode() + b"\0"
    return b + b"\0" * ((-len(b)) % 4)


def osc_message(addr, value, argtype="f"):
    if argtype == "b":
        # OSC booleans are the type tag itself, with no argument bytes.
        return osc_string(addr) + osc_string("," + ("T" if value else "F"))
    out = osc_string(addr) + osc_string("," + argtype)
    if argtype == "f":
        return out + struct.pack(">f", float(value))
    return out + struct.pack(">i", int(value))


def slip(payload):
    return bytes([SLIP_END]) + payload + bytes([SLIP_END])


def note_frame(n, on, argtype="f"):
    return slip(osc_message("/%d" % n, 1.0 if on else 0.0, argtype))


class Link:
    """Writes frames, optionally split to force the CoCo to reassemble."""

    def __init__(self, sock, fragment=0, verbose=False):
        self.sock = sock
        self.fragment = fragment
        self.verbose = verbose

    def send(self, data, label=""):
        if self.verbose and label:
            print("  %-28s %s" % (label, data.hex()))
        if self.sock is None:
            return
        if self.fragment:
            for i in range(0, len(data), self.fragment):
                self.sock.sendall(data[i:i + self.fragment])
                time.sleep(0.02)
        else:
            self.sock.sendall(data)

    def note(self, n, on, argtype="f"):
        self.send(note_frame(n, on, argtype),
                  "note %d %s" % (n, "on" if on else "off"))

    def play(self, notes, hold, gap=0.05, argtype="f"):
        for n in notes:
            self.note(n, True, argtype)
        time.sleep(hold)
        for n in notes:
            self.note(n, False, argtype)
        time.sleep(gap)


def mode_scale(link, argtype):
    """C major up and down, straddling the keybed edges."""
    for n in [60, 62, 64, 65, 67, 69, 71, 72]:
        link.play([n], 0.25, argtype=argtype)
    for n in [72, 71, 69, 67, 65, 64, 62, 60]:
        link.play([n], 0.25, argtype=argtype)


def mode_chords(link, argtype):
    for root, shape in ((60, MAJOR), (65, MAJOR), (67, MAJOR), (57, MINOR)):
        link.play([root + i for i in shape], 0.7, argtype=argtype)


def mode_edges(link, argtype):
    """Notes outside the keybed, which should fold and light the edge readouts."""
    print("above the keybed (right-hand readout):")
    for n in (84, 88, 96, 120):
        link.play([n], 0.6, argtype=argtype)
    print("below the keybed (left-hand readout):")
    for n in (24, 12, 0):
        link.play([n], 0.6, argtype=argtype)


def mode_stress(link, argtype):
    """Everything the error handling is supposed to survive."""
    print("1. a note split across writes (reassembly)")
    saved, link.fragment = link.fragment, 3
    link.note(60, True, argtype)
    time.sleep(0.5)
    link.note(60, False, argtype)
    link.fragment = saved
    time.sleep(0.3)

    print("2. runs of delimiters around a frame (must NOT warn)")
    link.send(bytes([SLIP_END]) * 3 + note_frame(64, True, argtype)
              + bytes([SLIP_END]) * 2, "padded frame")
    time.sleep(0.5)
    link.note(64, False, argtype)
    time.sleep(0.3)

    print("3. a complete but malformed frame (expect warning 1)")
    link.send(slip(b"not an osc packet"), "garbage frame")
    time.sleep(1.0)

    print("4. note number above MIDI range (expect warning 2)")
    link.send(note_frame(999, True, argtype), "note 999")
    time.sleep(1.0)

    print("5. five notes at once -- the 5th is dropped, silently by design")
    for n in (60, 62, 64, 65, 67):
        link.note(n, True, argtype)
    time.sleep(1.0)
    for n in (60, 62, 64, 65, 67):
        link.note(n, False, argtype)
    time.sleep(0.3)

    print("6. leading junk before a frame")
    link.send(b"\x11\x22junk" + note_frame(67, True, argtype), "junk + frame")
    time.sleep(0.5)
    link.note(67, False, argtype)


def load_tune():
    """Read the tune straight out of demo.c so the two cannot drift apart."""
    here = os.path.dirname(os.path.abspath(__file__))
    src = os.path.join(here, "src", "coco", "demo.c")
    rows = []
    for line in open(src):
        m = re.match(r"\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}", line)
        if m:
            v = [int(g) for g in m.groups()]
            rows.append(([n for n in v[:4] if n], v[4]))
    return rows


def mode_tune(link, argtype):
    """The same Fuer Elise the CoCo plays, but over the wire.

    Worth more than a scale as an end-to-end check: if it comes out
    recognizable, the whole path is working, and your ear does the assertion.
    """
    rows = load_tune()
    print("%d steps from src/coco/demo.c" % len(rows))
    for notes, sixteenths in rows:
        link.play(notes, sixteenths * 0.144, gap=0.015, argtype=argtype)


# a s d f g h j = white keys, w e t y u = the black keys between them
KEYMAP = {"a": 0, "w": 1, "s": 2, "e": 3, "d": 4, "f": 5, "t": 6,
          "g": 7, "y": 8, "h": 9, "u": 10, "j": 11, "k": 12}
PROBES = {"1": 0, "2": 24, "3": 96, "4": 120}

HELP = """
  a s d f g h j k   white keys (C D E F G A B C)
  w e   t y u       black keys
  - =               octave down / up
  /                 sustain on/off (notes latch instead of releasing)
  1 2 3 4           notes outside the keybed: 0, 24, 96, 120
  p                 panic -- release everything
  ?                 this help
  q                 quit
"""


def getch():
    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        return sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved)


def mode_play(link, argtype):
    """Play it by hand from the keyboard.

    A terminal reports key presses but never releases, so a press either plays
    for a fixed moment (default) or latches until pressed again (sustain). Use
    sustain to build a chord and watch several keys light at once -- and to see
    the fifth note dropped at the 4-voice ceiling.
    """
    base = 60
    hold = 0.35
    sustain = False
    on = set()

    def send(n, state):
        link.note(n, state, argtype)

    print("interactive -- ? for keys, q to quit")
    print(HELP)
    try:
        while True:
            c = getch()
            if c in ("q", "\x03", "\x1b"):
                break
            if c == "?":
                print(HELP)
                continue
            if c == "-":
                base = max(0, base - 12)
                print("base note %d" % base)
                continue
            if c == "=":
                base = min(115, base + 12)
                print("base note %d" % base)
                continue
            if c == "/":
                sustain = not sustain
                print("sustain %s" % ("ON" if sustain else "off"))
                continue
            if c == "p":
                for n in sorted(on):
                    send(n, False)
                on.clear()
                print("all released")
                continue

            if c in PROBES:
                n = PROBES[c]
            elif c in KEYMAP:
                n = base + KEYMAP[c]
            else:
                continue

            if n > 127:
                print("note %d is above MIDI range" % n)
                continue

            if sustain:
                if n in on:
                    send(n, False)
                    on.discard(n)
                    print("note %d off" % n)
                else:
                    send(n, True)
                    on.add(n)
                    print("note %d on   (%d held)" % (n, len(on)))
            else:
                send(n, True)
                time.sleep(hold)
                send(n, False)
                print("note %d" % n)
    finally:
        for n in sorted(on):
            send(n, False)
        if on:
            print("released %d held note(s) on the way out" % len(on))


MODES = {
    "scale": mode_scale,
    "chords": mode_chords,
    "edges": mode_edges,
    "stress": mode_stress,
    "tune": mode_tune,
    "play": mode_play,
}


def selftest():
    """Check our frames against a model of Jamminate's own parser."""
    def frame_length(buf, ln):
        a = 0
        while a < ln and buf[a] != SLIP_END:
            a += 1
        if a >= ln:
            return 0
        while a + 1 < ln and buf[a + 1] == SLIP_END:
            a += 1
        b = a + 1
        while b < ln and buf[b] != SLIP_END:
            b += 1
        return 0 if b >= ln else b + 1

    def parse(msg, msglen):
        off = 0
        while off < msglen and msg[off] != SLIP_END:
            off += 1
        if off >= msglen:
            return None
        off += 1
        while off < msglen and msg[off] == SLIP_END:
            off += 1
        if off >= msglen or msg[off] != ord("/"):
            return None
        addr = off
        pos = 0
        while off + pos < msglen and msg[off + pos]:
            pos += 1
        if off + pos >= msglen or msg[off + pos]:
            return None
        pos = ((pos + 4) // 4) * 4
        if off + pos >= msglen or msg[off + pos] != ord(","):
            return None
        types_at = off + pos
        while off + pos < msglen and msg[off + pos]:
            pos += 1
        if off + pos >= msglen or msg[off + pos]:
            return None
        pos = ((pos + 4) // 4) * 4
        if off + pos >= msglen:
            return None
        n = int(msg[addr:msglen].split(b"\0")[0][1:])

        # mirrors osc_note_state() in main.c
        tag = chr(msg[types_at + 1])
        vals = msg[off + pos:]
        avail = msglen - (off + pos)
        if tag == "T":
            state = "on"
        elif tag in ("F", "N"):
            state = "off"
        elif tag in ("f", "i"):
            if avail < 4:
                state = "unusable"
            elif (vals[0] & (0x7F if tag == "f" else 0xFF)) or any(vals[1:4]):
                state = "on"
            else:
                state = "off"
        else:
            state = "unusable"
        return n, state

    ok = True
    for argtype in ("f", "i", "b"):
        for n, on in ((60, True), (60, False), (0, True), (127, True),
                      (100, True), (100, False)):
            f = note_frame(n, on, argtype)
            got = parse(f, frame_length(f, len(f)))
            want = (n, "on" if on else "off")
            if got != want:
                ok = False
            print("  %s ,%s note %-3d %-3s -> %s" %
                  ("ok " if got == want else "BAD", argtype, n,
                   "on" if on else "off", got))
    print("\nAll three argument types should read identically. Any BAD line\n"
          "means osc_note_state() in main.c and this model have diverged.")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", nargs="?", default="scale",
                    choices=sorted(MODES) + ["all"])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=6573)
    ap.add_argument("--type", dest="argtype", default="f",
                    choices=("f", "i", "b"),
                    help="OSC argument type: float, integer or boolean; a "
                         "TouchOSC layout can use any of the three")
    ap.add_argument("--fragment", type=int, default=0, metavar="N",
                    help="split every write into N-byte pieces")
    ap.add_argument("--dry-run", action="store_true",
                    help="print frames instead of connecting")
    ap.add_argument("--selftest", action="store_true",
                    help="parse our own frames with a model of the CoCo parser")
    args = ap.parse_args()

    if args.selftest:
        return 0 if selftest() else 1

    sock = None
    if not args.dry_run:
        print("connecting to %s:%d ..." % (args.host, args.port))
        sock = socket.create_connection((args.host, args.port), timeout=10)
        print("connected")

    link = Link(sock, fragment=args.fragment, verbose=args.dry_run)
    modes = sorted(MODES) if args.mode == "all" else [args.mode]
    try:
        for m in modes:
            print("--- %s ---" % m)
            MODES[m](link, args.argtype)
    finally:
        if sock:
            sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
