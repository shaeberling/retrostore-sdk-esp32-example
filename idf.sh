#!/bin/bash

if [ -z "$1" ]
  then
    echo "1st argument missing: [serial-port number]"
    exit 1
fi
port="$1"
if [ -z "$2" ]
  then
    echo "2nd argument missing: [command]"
    exit 1
fi
all_arguments=("$@")
cmd_args=("${all_arguments[@]:1}")

idf.py -p /dev/ttyS"$port" -b 921600 "${cmd_args[@]}"
