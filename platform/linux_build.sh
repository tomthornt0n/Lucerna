#!/bin/sh

set -e

TIMEFORMAT="done in %Rs"

if [ "$1" == "debug" ]; then
    FLAGS="-DLUCERNA_DEBUG -g"
elif [ "$1" == "release" ]; then
    FLAGS="-O2 -msse2"
else
    echo -e "Unknown mode '$1'"
    exit
fi

echo -e "\033[35mbuilding linux platform layer in $1 mode.\033[0m"
time gcc -I../include $FLAGS linux_lucerna.c -rdynamic -ldl -lpthread -lGL -lxcb -lX11 -lX11-xcb -lm -lasound -o ../bin/platform_linux

