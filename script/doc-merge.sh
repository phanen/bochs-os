#!/usr/bin/sh
find $1 -name "[0-9]*" -exec sh -c 'echo \# $(basename {}) && echo && cat {}' \;
