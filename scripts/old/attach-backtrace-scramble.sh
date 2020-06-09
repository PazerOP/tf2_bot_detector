#!/usr/bin/env bash

line=$(pidof hl2_linux)
arr=($line)
inst=$1
if [ $# == 0 ]; then
  inst=0
fi

if [ ${#arr[@]} == 0 ]; then
  echo tf2 isn\'t running!
  exit
fi

if [ $inst -gt ${#arr[@]} ] || [ $inst == ${#arr[@]} ]; then
  echo wrong index!
  exit
fi

proc=${arr[$inst]}

echo Running instances: "${arr[@]}"

FILENAME="/tmp/.gl$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 6 | head -n 1)"
cp "bin/libcathook.so" "$FILENAME"

echo Attaching to "$proc"

sudo ./detach $inst bin/libcathook.so

if grep -q "$FILENAME" /proc/"$proc"/maps; then
  echo already loaded
  exit
fi

sudo killall -19 steam
sudo killall -19 steamwebhelper

echo loading "$FILENAME" to "$proc"
gdb -n -q -batch \
  -ex "attach $proc" \
  -ex "set \$dlopen = (void*(*)(char*, int)) dlopen" \
  -ex "call \$dlopen(\"$FILENAME\", 1)" \
  -ex "call dlerror()" \
  -ex "print (char *) $2" \
  -ex "catch syscall exit exit_group" \
  -ex "continue" \
  -ex "backtrace"

sudo killall -18 steamwebhelper
sudo killall -18 steam
