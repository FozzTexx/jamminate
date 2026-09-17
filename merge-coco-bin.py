#!/usr/bin/env python3
import argparse
import struct
from dataclasses import dataclass

COCO_HEADER = ">BHH"

def build_argparser():
  parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument("bin1", help="input file")
  parser.add_argument("bin2", help="input file")
  parser.add_argument("output", help="output file")
  parser.add_argument("--flag", action="store_true", help="flag to do something")
  return parser

@dataclass
class CoCoHeader:
  htype: int
  length: int
  address: int

@dataclass
class CoCoSection:
  htype: int
  address: int
  data: bytes

def split_sections(data):
  offset = struct.calcsize(COCO_HEADER)

  sections = []
  while data:
    header = CoCoHeader(*struct.unpack(COCO_HEADER, data[:offset]))
    section = CoCoSection(header.htype, header.address, data[offset:offset + header.length])
    sections.append(section)
    data = data[offset + header.length:]

  return sections

def print_sections(sections):
  for section in sections:
    print(f"type: {section.htype}  addr: {section.address:04x}  length: {len(section.data)}")
  print()
  return

def main():
  args = build_argparser().parse_args()

  with open(args.bin1, mode="rb") as f:
    bin1 = f.read()
  with open(args.bin2, mode="rb") as f:
    bin2 = f.read()

  sections1 = split_sections(bin1)
  sections2 = split_sections(bin2)

  # Sections are concatenated at the addresses they were linked for. Shifting
  # them here would break every absolute address inside the code -- cmoc emits
  # PC-relative code and survives it, but hand-written assembly does not. Build
  # bin2 with the right origin (see LDFLAGS_EXTRA_COCO in the Makefile) instead.
  merged = sections1[:-1]
  end_addr = max(x.address + len(x.data) for x in sections1)
  first = min(x.address for x in sections2 if x.htype != 0xFF)
  if first < end_addr:
    raise SystemExit(f"{args.bin2} loads at ${first:04x}, overlapping "
                     f"{args.bin1} which ends at ${end_addr:04x}. "
                     f"Link it at ${end_addr:04x} or above.")
  merged.extend(sections2)

  with open(args.output, mode="wb") as f:
    for section in merged:
      header = struct.pack(COCO_HEADER, section.htype, len(section.data), section.address)
      f.write(header)
      f.write(section.data)

  return

if __name__ == "__main__":
  exit(main() or 0)
