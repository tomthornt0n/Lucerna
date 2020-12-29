#!/bin/bash

TODO_COUNT=$(grep -r "TODO" ./game/* | wc -l)
echo "found $TODO_COUNT TODOs:"
grep --colour=always -r -c "TODO" ./game/* | column -t -s$':'

echo -e "\n"

NOTE_COUNT=$(grep -r "NOTE" ./game/* | wc -l)
echo "found $NOTE_COUNT NOTEs:"
grep --colour=always -r -c "NOTE" ./game/* | column -t -s$':'
