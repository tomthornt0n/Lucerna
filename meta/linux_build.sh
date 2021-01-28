#!/bin/sh

TIMEFORMAT="done in %Rs"

echo -e "\033[35mbuilding lcddl user layer.\033[0m"
time gcc lcddl_user_layer.c -shared -fPIC -o ../bin/lcddl_user_layer.so
echo -e "\033[35mbuilding lcddl.\033[0m"
time gcc lcddl.c -ldl -o ../bin/lcddl

