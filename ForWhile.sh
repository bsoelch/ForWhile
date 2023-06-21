#!/bin/sh

cArgs=( "-g" "-Wall" "-Wextra" "-Wshadow" "-Wold-style-definition" "-Wcast-qual" "-Werror" "-pedantic" "-lm" )
gcc ${cArgs[@]} "./src/ForWhile.c" -o ./forWhile
./forWhile "-f" "test.txt"
