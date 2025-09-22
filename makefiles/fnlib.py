#!/usr/bin/env python3
import argparse
import os, sys
import re
import zipfile
import urllib.request
import json
import subprocess

FUJINET_REPO = "FujiNetWIFI/fujinet-lib"
GITHUB_API = "https://api.github.com/repos"
GITHUB_URL = "https://github.com"
CACHE_DIR = "_cache"
FUJINET_CACHE_DIR = os.path.join(CACHE_DIR, "fujinet-lib")

VERSION_NUM = r"([0-9]+[.][0-9]+[.][0-9]+)"
VERSION_NAME = fr"v?{VERSION_NUM}"
LDLIB_REGEX = r"lib(.*)[.]a$"

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

  @staticmethod
  def escapeForMake(val):
    if not val:
      return ""
    return (
      val
      .replace("\\", "\\\\")  # escape backslashes first
      .replace('"', '\\"')
      .replace("$", "$$")
      .replace(" ", "\\ ")
      .replace(":", "\\:")
      .replace("#", "\\#")
    )

  def printValues(self):
    attrs = self.__dict__.keys()
    for key in attrs:
      print(f"{key}:={self.escapeForMake(getattr(self, key))}")
    return

class LibLocator:
  def __init__(self, FUJINET_LIB, PLATFORM):
    """
    FUJINET_LIB can be
      - a version number such as 4.7.4
      - a directory which contains the libs for each platform
      - a zip file with an archived fujinet-lib
      - a URL to a git repo
      - empty
    """

    self.MV = MakeVariables([
      "FUJINET_LIB_DIR",
      "FUJINET_LIB_FILE",
      "FUJINET_LIB_LDLIB",
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
      rxm = re.match(VERSION_NAME, FUJINET_LIB)
      if rxm:
        self.MV.FUJINET_LIB_VERSION = rxm.group(1)
      elif "://" in FUJINET_LIB:
        self.gitClone(FUJINET_LIB)
      elif os.path.isfile(FUJINET_LIB):
        _, ext = os.path.splitext(FUJINET_LIB)
        if ext == ".zip":
          self.MV.FUJINET_LIB_ZIP = FUJINET_LIB
        else:
          self.MV.FUJINET_LIB_DIR = os.path.dirname(FUJINET_LIB)
          self.MV.FUJINET_LIB_FILE = os.path.basename(FUJINET_LIB)
      elif os.path.isdir(FUJINET_LIB):
        self.MV.FUJINET_LIB_DIR = FUJINET_LIB

    if not self.MV.FUJINET_LIB_VERSION:
      self.getVersion()

    if not self.MV.FUJINET_LIB_DIR:
      self.getDirectory()

    if not self.MV.FUJINET_LIB_FILE:
      self.downloadZip()

    if not self.MV.FUJINET_LIB_INCLUDE:
      self.getInclude()

    # Some linkers require the library to be named specially in order
    # to be used with the `-l` flag. Create symlink if necessary.
    if self.PLATFORM == "coco":
      if not re.match(LDLIB_REGEX, self.MV.FUJINET_LIB_FILE):
        version = ""
        if self.MV.FUJINET_LIB_VERSION:
          version = f"-{self.MV.FUJINET_LIB_VERSION}"
        symlink_file = f"libfujinet-{self.PLATFORM}{version}.a"
        symlink_path = os.path.join(self.MV.FUJINET_LIB_DIR, symlink_file)
        if not os.path.exists(symlink_path):
          os.symlink(self.MV.FUJINET_LIB_FILE, symlink_path)
        self.MV.FUJINET_LIB_FILE = symlink_file

    self.MV.FUJINET_LIB_LDLIB = self.MV.FUJINET_LIB_FILE

    # If FUJINET_LIB_LDLIB is specially named for linker to find, make
    # sure FUJINET_LIB_LDLIB is fixed up appropriately
    rxm = re.match(LDLIB_REGEX, self.MV.FUJINET_LIB_LDLIB)
    if rxm:
      self.MV.FUJINET_LIB_LDLIB = rxm.group(1)

    return

  def checkLibraryFilename(self, filename):
    rxm = re.match(self.LIBRARY_REGEX, filename)
    if not rxm:
      if self.PLATFORM == "coco":
        alt_pattern = fr"libfujinet.{self.PLATFORM}.a"
        rxm = re.match(alt_pattern, filename)
    return rxm

  def findLibrary(self, filelist):
    for filename in filelist:
      rxm = self.checkLibraryFilename(filename)
      if rxm:
        return rxm
    return None

  def getVersion(self):
    if self.MV.FUJINET_LIB_DIR:
      rxm = self.findLibrary(os.listdir(self.MV.FUJINET_LIB_DIR))
      if rxm:
        if len(rxm.groups()) >= 2:
          self.MV.FUJINET_LIB_VERSION = rxm.group(2)
        self.MV.FUJINET_LIB_FILE = rxm.group(0)
        return
      raise ValueError("No library found")

    if self.MV.FUJINET_LIB_ZIP:
      with zipfile.ZipFile(self.MV.FUJINET_LIB_ZIP, "r") as zf:
        rxm = self.findLibrary(zf.namelist())
        if rxm:
          if len(rxm.groups()) >= 2:
            self.MV.FUJINET_LIB_VERSION = rxm.group(2)
          return

      raise ValueError("Which file is the newest?")

    latest_url = f"{GITHUB_API}/{FUJINET_REPO}/releases/latest"
    with urllib.request.urlopen(latest_url) as response:
      response = response.read().decode("UTF-8")
      release_info = json.loads(response)

    latest_version = release_info.get("tag_name") or release_info.get("name")
    if not latest_version:
      raise ValueError("Can't find version")

    rxm = re.match(VERSION_NAME, latest_version)
    if not rxm:
      raise ValueError("Not a FujiNet-lib version", latest_version)

    self.MV.FUJINET_LIB_VERSION = rxm.group(1)
    return

  def getDirectory(self):
    global FUJINET_CACHE_DIR
    self.MV.FUJINET_LIB_DIR = os.path.join(FUJINET_CACHE_DIR,
                                           f"{self.MV.FUJINET_LIB_VERSION}-{self.PLATFORM}")
    return

  def downloadZip(self):
    global FUJINET_CACHE_DIR
    os.makedirs(self.MV.FUJINET_LIB_DIR, exist_ok=True)

    self.MV.FUJINET_LIB_FILE = f"fujinet-{self.PLATFORM}-{self.MV.FUJINET_LIB_VERSION}.lib"
    if not os.path.exists(os.path.join(self.MV.FUJINET_LIB_DIR, self.MV.FUJINET_LIB_FILE)):
      zip_path = f"fujinet-lib-{self.PLATFORM}-{self.MV.FUJINET_LIB_VERSION}.zip"

      if not self.MV.FUJINET_LIB_ZIP:
        self.MV.FUJINET_LIB_ZIP = os.path.join(FUJINET_CACHE_DIR, zip_path)

      if not os.path.exists(self.MV.FUJINET_LIB_ZIP):
        release_url = f"{GITHUB_URL}/{FUJINET_REPO}/releases/download" \
          f"/v{self.MV.FUJINET_LIB_VERSION}/{zip_path}"
        try:
          urllib.request.urlretrieve(release_url, self.MV.FUJINET_LIB_ZIP)
        except:
          error_exit("Unable to download FujiNet library from", release_url)

      with zipfile.ZipFile(self.MV.FUJINET_LIB_ZIP, "r") as zf:
        zf.extractall(self.MV.FUJINET_LIB_DIR)

    return

  def gitClone(self, url):
    global FUJINET_CACHE_DIR
    os.makedirs(FUJINET_CACHE_DIR, exist_ok=True)
    branch = ""
    if "#" in url:
      url, branch = url.split("#")
    base = url.rstrip("/").split("/")[-1]
    if base.endswith(".git"):
      base = base.rsplit(".", 1)[0]
    repoDir = os.path.join(FUJINET_CACHE_DIR, base)
    # FIXME - yah, this is harcoded to where we expect the output
    self.MV.FUJINET_LIB_DIR = os.path.join(repoDir, "build")

    if not os.path.exists(repoDir):
      cmd = ["git", "clone", url]
      if branch:
        cmd.extend(["-b", branch])
      subprocess.run(cmd, cwd=FUJINET_CACHE_DIR, check=True)

    if self.PLATFORM == "coco":
      self.MV.FUJINET_LIB_FILE = fr"libfujinet.{self.PLATFORM}.a"
    else:
      self.MV.FUJINET_LIB_FILE = f"fujinet.{self.PLATFORM}.lib"
    libPath = os.path.join(self.MV.FUJINET_LIB_DIR, self.MV.FUJINET_LIB_FILE)
    if not os.path.exists(libPath):
      cmd = ["make", ]
      subprocess.run(cmd, cwd=repoDir, check=True)

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

# Print errors to stderr so that `make` doesn't try to interpret them in `$(eval)`
def error_exit(*args):
  print(*args, file=sys.stderr)
  exit(1)

def main():
  global CACHE_DIR, FUJINET_CACHE_DIR

  args = build_argparser().parse_args()

  PLATFORM = os.getenv("PLATFORM")
  if args.platform:
    PLATFORM = args.platform

  if not PLATFORM:
    error_exit("Please specify PLATFORM")

  FUJINET_LIB = args.file
  if not FUJINET_LIB:
    FUJINET_LIB = os.getenv("FUJINET_LIB")

  env_cache_dir = os.getenv("CACHE_DIR")
  if env_cache_dir:
    CACHE_DIR = env_cache_dir
    FUJINET_CACHE_DIR = os.path.join(CACHE_DIR, os.path.basename(FUJINET_CACHE_DIR))

  fujinetLib = LibLocator(FUJINET_LIB, PLATFORM)
  fujinetLib.printMakeVariables()

  return

if __name__ == "__main__":
  exit(main() or 0)
