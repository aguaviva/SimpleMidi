#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir="$script_dir/build/cli"

cmake -S "$script_dir" -B "$build_dir" -DSIMPLEMIDI_CLI_ONLY=ON
cmake --build "$build_dir" --target SimpleMidi
