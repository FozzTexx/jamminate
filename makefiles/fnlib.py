#!/usr/bin/env python3
import argparse
import os
import re
import requests
import zipfile
import shlex

FUJINET_REPO = "FujiNetWIFI/fujinet-lib"
GITHUB_API = "https://api.github.com/repos"
GITHUB_URL = "https://github.com"
CACHE_DIR = "_cache"
FUJINET_CACHE_DIR = os.path.join(CACHE_DIR, "fujinet-lib")

VERSION_NUM = r"([0-9]+[.][0-9]+[.][0-9]+)"
VERSION_NAME = fr"v?{VERSION_NUM}"

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

class LibLocator:
  def __init__(self, FUJINET_LIB, PLATFORM):
    # FUJINET_LIB can be
    # - a version number such as 4.7.4
    # - a directory which contains the libs for each platform
    # - a zip file with an archived fujinet-lib
    # - empty

    self.MV = MakeVariables([
      "FUJINET_LIB_DIR",
      "FUJINET_LIB_ARCHIVE",
      "FUJINET_LIB_VERSION",
      "FUJINET_LIB_INCLUDE",
      "FUJINET_LIB_ZIP",
    ])

    self.PLATFORM = PLATFORM

    # Two possible library filename patterns:
    #   - fujinet-coco-4.7.6.lib
    #   - fujinet.apple2.lib
    self.LIBRARY_REGEX = fr"fujinet[-.]{self.PLATFORM}(-{VERSION_NUM})?[.]lib$"

    if FUJINET_LIB:
      m = re.match(VERSION_NAME, FUJINET_LIB)
      if m:
        self.MV.FUJINET_LIB_VERSION = m.group(1)
      elif os.path.isfile(FUJINET_LIB):
        _, ext = os.path.splitext(FUJINET_LIB)
        if ext == ".zip":
          self.MV.FUJINET_LIB_ZIP = FUJINET_LIB
        else:
          self.MV.FUJINET_LIB_DIR = os.path.dirname(FUJINET_LIB)
          self.MV.FUJINET_LIB_ARCHIVE = os.path.basename(FUJINET_LIB)
      elif os.path.isdir(FUJINET_LIB):
        self.MV.FUJINET_LIB_DIR = FUJINET_LIB

    if not self.MV.FUJINET_LIB_VERSION:
      self.getVersion()

    if not self.MV.FUJINET_LIB_DIR:
      self.getDirectory()

    if not self.MV.FUJINET_LIB_ARCHIVE:
      self.getArchive()

    if not self.MV.FUJINET_LIB_INCLUDE:
      self.getInclude()

    return

  def checkLibraryFilename(self, filename):
    m = re.match(self.LIBRARY_REGEX, filename)
    if not m:
      if self.PLATFORM == "coco":
        alt_pattern = fr"libfujinet.{self.PLATFORM}.a"
        m = re.match(alt_pattern, filename)
    return m

  def findLibrary(self, filelist):
    for filename in filelist:
      m = self.checkLibraryFilename(filename)
      if m:
        return m
    return None

  def getVersion(self):
    if self.MV.FUJINET_LIB_DIR:
      m = self.findLibrary(os.listdir(self.MV.FUJINET_LIB_DIR))
      if m:
        if len(m.groups()) >= 2:
          self.MV.FUJINET_LIB_VERSION = m.group(2)
        self.MV.FUJINET_LIB_ARCHIVE = filename
        return
      raise ValueError("No library found")

    if self.MV.FUJINET_LIB_ZIP:
      with zipfile.ZipFile(self.MV.FUJINET_LIB_ZIP, "r") as zf:
        m = self.findLibrary(zf.namelist())
        if m:
          if len(m.groups()) >= 2:
            self.MV.FUJINET_LIB_VERSION = m.group(2)
          return

      raise ValueError("Which file is the newest?")

    latest_url = f"{GITHUB_API}/{FUJINET_REPO}/releases/latest"
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

    m = re.match(VERSION_NAME, latest_version)
    if not m:
      raise ValueError("Not a FujiNet-lib version", latest_version)

    self.MV.FUJINET_LIB_VERSION = m.group(1)
    return

  def getDirectory(self):
    self.MV.FUJINET_LIB_DIR = os.path.join(FUJINET_CACHE_DIR,
                                           f"{self.MV.FUJINET_LIB_VERSION}-{self.PLATFORM}")
    return

  def getArchive(self):
    os.makedirs(self.MV.FUJINET_LIB_DIR, exist_ok=True)

    self.MV.FUJINET_LIB_ARCHIVE = f"fujinet-{self.PLATFORM}-{self.MV.FUJINET_LIB_VERSION}.lib"
    if not os.path.exists(os.path.join(self.MV.FUJINET_LIB_DIR, self.MV.FUJINET_LIB_ARCHIVE)):
      zip_path = f"fujinet-lib-{self.PLATFORM}-{self.MV.FUJINET_LIB_VERSION}.zip"

      if not self.MV.FUJINET_LIB_ZIP:
        self.MV.FUJINET_LIB_ZIP = os.path.join(FUJINET_CACHE_DIR, zip_path)

      if not os.path.exists(self.MV.FUJINET_LIB_ZIP):
        release_url = f"{GITHUB_URL}/{FUJINET_REPO}/releases/download" \
          f"/v{self.MV.FUJINET_LIB_VERSION}/{zip_path}"
        try:
          response = requests.get(release_url, stream=True)
          response.raise_for_status()  # Raise an exception for bad status codes (4xx or 5xx)
        except requests.exceptions.RequestException as e:
          print(f"Error downloading file: {e}")
          exit(1)

        with open(self.MV.FUJINET_LIB_ZIP, 'wb') as f:
          for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)

      with zipfile.ZipFile(self.MV.FUJINET_LIB_ZIP, "r") as zf:
        zf.extractall(self.MV.FUJINET_LIB_DIR)

    return

  def getInclude(self):
    parent = os.path.dirname(self.MV.FUJINET_LIB_DIR.rstrip("/"))
    check_dirs = [self.MV.FUJINET_LIB_DIR, parent, os.path.join(parent, "include")]
    for idir in check_dirs:
      if os.path.exists(os.path.join(idir, "fujinet-fuji.h")):
        self.MV.FUJINET_LIB_INCLUDE = idir
        return
    raise ValueError("Unable to find include directory")

    return

  def printMakeVariables(self):
    self.MV.printValues()
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
  if not FUJINET_LIB:
    FUJINET_LIB = os.getenv("FUJINET_LIB")

  fujinetLib = LibLocator(FUJINET_LIB, PLATFORM)
  fujinetLib.printMakeVariables()

  return

if __name__ == "__main__":
  exit(main() or 0)
