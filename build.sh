#!/bin/sh

# NOTE(tbt): 4coder build script

./linux_build.sh -m=debug -f
pushd bin
./platform_linux
popd

