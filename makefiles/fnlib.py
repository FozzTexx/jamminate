#!/usr/bin/env python3
import argparse
import os
import re
import requests
import zipfile
import shlex

FUJINET_REPO = "FujiNetWIFI/fujinet-lib/"
GITHUB_API = "https://api.github.com/repos/"
GITHUB_URL = "https://github.com/"
CACHE_DIR = "_cache"

VERSION_PATTERN = r"v?([0-9]+[.][0-9]+[.][0-9]+)"

def build_argparser():
  parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument("file", nargs="?", help="input file")
  parser.add_argument("--platform", help="platform building for")
  parser.add_argument("--flag", action="store_true", help="flag to do something")
  return parser

class MakeVariables:
  def __init__(self, varList):
    for key in varList:
      setattr(self, key, "")
    return

  def printValues(self):
    attrs = self.__dict__.keys()
    for key in attrs:
      print(f"{key}={shlex.quote(getattr(self, key))}")
    return
  
def main():
  args = build_argparser().parse_args()

  PLATFORM = os.getenv("PLATFORM")
  if args.platform:
    PLATFORM = args.platform

  if not PLATFORM:
    print("Please specify PLATFORM")
    exit(1)

  FUJINET_LIB = args.file

  # FUJINET_LIB can be
  # - a version number such as 4.7.4
  # - a directory which contains the libs for each platform
  # - a zip file with an archived fujinet-lib

  MV = MakeVariables([
    "FUJINET_LIB_DIR",
    "FUJINET_LIB_ARCHIVE",
    "FUJINET_LIB_VERSION",
    "FUJINET_LIB_INCLUDE",
    "FUJINET_LIB_ZIP",
  ])

  FUJINET_CACHE_DIR = os.path.join(CACHE_DIR, "fujinet-lib")
  
  if FUJINET_LIB:
    m = re.match(VERSION_PATTERN, FUJINET_LIB)
    if m:
      MV.FUJINET_LIB_VERSION = m.group(1)
    elif os.path.isfile(FUJINET_LIB):
      _, ext = os.path.splitex(FUJINET_LIB)
      if ext == ".zip":
        MV.FUJINET_LIB_ZIP = FUJINET_LIB
      else:
        MV.FUJINET_LIB_DIR = os.path.dirname(FUJINET_LIB)
        MV.FUJINET_LIB_ARCHIVE = os.path.basename(FUJINET_LIB)
    elif os.path.isdir(FUJINET_LIB):
      MV.FUJINET_LIB_DIR = FUJINET_LIB

  if not MV.FUJINET_LIB_VERSION:
    if MV.FUJINET_LIB_DIR or MV.FUJINET_LIB_ZIP:
      raise ValueError("Which file is the newest?")

    latest_url = f"{GITHUB_API}{FUJINET_REPO}releases/latest"
    try:
      response = requests.get(latest_url)
      response.raise_for_status()  # Raise an exception for bad status codes
      release_info = response.json()
    except requests.exceptions.RequestException as e:
      print(f"An error occurred: {e}")
      exit(1)

    latest_version = release_info.get("tag_name") or release_info.get("name")
    if not latest_version:
      raise ValueError("Can't find version")

    m = re.match(VERSION_PATTERN, latest_version)
    if not m:
      raise ValueError("Not a FujiNet-lib version")

    MV.FUJINET_LIB_VERSION = m.group(1)

  if not MV.FUJINET_LIB_DIR:
    MV.FUJINET_LIB_DIR = os.path.join(FUJINET_CACHE_DIR, f"{MV.FUJINET_LIB_VERSION}-{PLATFORM}")

  if not MV.FUJINET_LIB_ARCHIVE:
    os.makedirs(MV.FUJINET_LIB_DIR, exist_ok=True)

    MV.FUJINET_LIB_ARCHIVE = f"fujinet-{PLATFORM}-{MV.FUJINET_LIB_VERSION}.lib"
    if not os.path.exists(os.path.join(MV.FUJINET_LIB_DIR, MV.FUJINET_LIB_ARCHIVE)):
      zip_path = f"fujinet-lib-{PLATFORM}-{MV.FUJINET_LIB_VERSION}.zip"

      if not MV.FUJINET_LIB_ZIP:
        MV.FUJINET_LIB_ZIP = os.path.join(FUJINET_CACHE_DIR, zip_path)

      if not os.path.exists(MV.FUJINET_LIB_ZIP):
        release_url = f"{GITHUB_URL}{FUJINET_REPO}releases/download" \
          f"/v{MV.FUJINET_LIB_VERSION}/{zip_path}"
        try:
          response = requests.get(release_url, stream=True)
          response.raise_for_status()  # Raise an exception for bad status codes (4xx or 5xx)
        except requests.exceptions.RequestException as e:
          print(f"Error downloading file: {e}")
          exit(1)

        with open(MV.FUJINET_LIB_ZIP, 'wb') as f:
          for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)

      with zipfile.ZipFile(MV.FUJINET_LIB_ZIP, "r") as zf:
        zf.extractall(MV.FUJINET_LIB_DIR)

  if not MV.FUJINET_LIB_INCLUDE:
    if os.path.exists(os.path.join(MV.FUJINET_LIB_DIR, "fujinet-fuji.h")):
      MV.FUJINET_LIB_INCLUDE = MV.FUJINET_LIB_DIR
    else:
      raise ValueError("Must set the include directory!")

  MV.printValues()

  # FIXME - set FUJINET_LIB_DIR
  # FIXME - is FUJINET_LIB a directory? Then use that
  # FIXME - not a dir, need to create a cache dir
  # FIXME - does cache dir match requested version?
  # FIXME - does zip file contain version number?
  # FIXME - unzip if zip file
  # FIXME - else, download
  # FIXME - if no version specified, get latest

  return

if __name__ == "__main__":
  exit(main() or 0)
