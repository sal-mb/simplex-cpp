#!/bin/bash
set -e

# --- Parse build type argument (debug by default, 'release' to opt in) ---
BUILDTYPE="debug"
if [ "$1" = "release" ] || [ "$1" = "debug" ]; then
  BUILDTYPE="$1"
  shift
fi

# --- Set up build dir if missing, otherwise apply build type ---
if [ ! -d .build ]; then
  meson setup .build --wipe --wrap-mode=forcefallback --buildtype="$BUILDTYPE"
else
  meson configure .build --buildtype="$BUILDTYPE"
fi

# --- Build before running ---
meson compile -C .build

# --- If no args, show help ---
if [ $# -eq 0 ]; then
  ./.build/simplex --help
  exit 0
fi

# --- Forward all arguments to the executable ---
./.build/simplex "$@"
