#!/bin/sh

rm test.log
touch test.log
PARMS="--disable-syslog --disable-logfile"
./configure $PARMS
if [ $? -eq 0 ]; then
  echo "OK ./configure $PARMS" >> test.log
  make clean
  make
  if [ $? -eq 0 ]; then
    echo "OK make $PARMS" >> test.log
  else
    echo "FAIL make $PARMS" >> test.log
  fi
else
  echo "FAIL ./configure $PARMS" >> test.log
fi


PARMS="--disable-syslog"
./configure $PARMS
if [ $? -eq 0 ]; then
  echo "OK ./configure $PARMS" >> test.log
  make clean
  make
  if [ $? -eq 0 ]; then
    echo "OK make $PARMS" >> test.log
  else
    echo "FAIL make $PARMS" >> test.log
  fi
else
  echo "FAIL ./configure $PARMS" >> test.log
fi

PARMS="--disable-logfile"
./configure $PARMS
if [ $? -eq 0 ]; then
  echo "OK ./configure $PARMS" >> test.log
  make clean
  make
  if [ $? -eq 0 ]; then
    echo "OK make $PARMS" >> test.log
  else
    echo "FAIL make $PARMS" >> test.log
  fi
else
  echo "FAIL ./configure $PARMS" >> test.log
fi

PARMS=""
./configure $PARMS
if [ $? -eq 0 ]; then
  echo "OK ./configure $PARMS" >> test.log
  make clean
  make
  if [ $? -eq 0 ]; then
    echo "OK make $PARMS" >> test.log
  else
    echo "FAIL make $PARMS" >> test.log
  fi
else
  echo "FAIL ./configure $PARMS" >> test.log
fi

