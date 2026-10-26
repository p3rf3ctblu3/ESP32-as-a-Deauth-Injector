#!/bin/bash

SRC=""
DST=""
GLOBAL="false"
INDEX=""

# Parse the arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -src) SRC="$2"; shift ;;
        -dst) DST="$2"; shift ;;
        -i) INDEX="$2"; shift ;;
        --global) GLOBAL="true" ;;
        *) echo "[-] Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

# Route based on which flags were used
if [ "$GLOBAL" == "true" ]; then
    curl -s "http://127.0.0.1:1880/cli?global=true"
elif [ -n "$INDEX" ]; then
    curl -s "http://127.0.0.1:1880/cli?i=$INDEX"
elif [ -n "$DST" ] && [ -n "$SRC" ]; then
    curl -s "http://127.0.0.1:1880/cli?src=$SRC&dst=$DST"
elif [ -n "$DST" ]; then
    curl -s "http://127.0.0.1:1880/cli?dst=$DST"
else
    echo "Usage: strike -i <index>  OR  strike -src <mac> -dst <mac>  OR  strike -dst <mac>  OR  strike --global"
fi

