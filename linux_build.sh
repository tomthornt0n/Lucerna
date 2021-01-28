#!/bin/bash

set -e

MODE="none"
FULL="false"

for i in "$@"
do
case $i in
    -m=*|--mode=*)
    MODE="${i#*=}"
    shift
    ;;
    -f|--full)
    FULL="true"
    shift
    ;;
    --default)
    shift 
    ;;
    *)
        echo "Ignoring unrecognised option $i"
    ;;
esac
done

if [ "$MODE" == "debug" ]; then
    FLAGS="-DLUCERNA_DEBUG -g"
elif [ "$MODE" == "release" ]; then
    FLAGS="-O2 -msse2"
elif [ "$MODE" == "none" ]; then
    echo -e "Must specify mode (-m={mode} or --mode={mode})"
    exit
else
    echo -e "Unknown mode '$MODE'"
    exit
fi

main(){
TIMEFORMAT="done in %Rs"

if [ "$FULL" == "true" ]; then
    pushd platform > /dev/null
    ./linux_build.sh $MODE
    popd > /dev/null

    pushd lcddl > /dev/null
    ./linux_build.sh
    popd > /dev/null
fi

echo -e "\033[35mrunning lcddl.\033[0m"
pushd bin > /dev/null
time ./lcddl ./lcddl_user_layer.so ../assets/entities/*
popd > /dev/null

echo -e "\033[35mbuilding game in $MODE mode.\033[0m"
time gcc -Iinclude -fPIC -shared $FLAGS game/main.c -lGL -lm -o bin/liblucerna.so

echo ""
TIMEFORMAT="total time taken: %Rs"
}

time main

