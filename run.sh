#!/bin/bash
# Run from inside the M1 folder:  bash run.sh
cd "$(dirname "$0")"
gcc -Wall -Wextra comp.c -o comp.out || exit 1
gcc -Wall -Wextra EX1_2.c -o a.out   || exit 1
./a.out setup/config.txt
