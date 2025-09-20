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

def main():
  args = build_argparser().parse_args()

  with open(args.bin1, mode="rb") as f:
    bin1 = f.read()
  with open(args.bin2, mode="rb") as f:
    bin2 = f.read()

  sections1 = split_sections(bin1)
  for section in sections1:
    print(f"type: {section.htype}  addr: {section.address:04x}  length: {len(section.data)}")
  print()

  sections2 = split_sections(bin2)
  for section in sections2:
    print(f"type: {section.htype}  addr: {section.address:04x}  length: {len(section.data)}")
  print()

  merged = sections1[:-1]
  end_addr = max(x.address + len(x.data) for x in sections1)
  start = ((end_addr + 255) // 256) * 256
  first = sections2[0].address
  for section in sections2:
    htype = section.htype
    address = section.address
    merged.append(CoCoSection(htype, address - first + start, section.data))

  for section in merged:
    print(f"type: {section.htype}  addr: {section.address:04x}  length: {len(section.data)}")
  print()

  with open(args.output, mode="wb") as f:
    for section in merged:
      header = struct.pack(COCO_HEADER, section.htype, len(section.data), section.address)
      f.write(header)
      f.write(section.data)

  return

if __name__ == "__main__":
  exit(main() or 0)
