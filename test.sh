#!/bin/sh

unalias rm
RM=`whereis rm|awk -F: '{print $2}'|awk '{print $1}'`
test -z "$RM" && RM=rm

${RM} test.log
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

PARMS="--with-defaultshell=/bin/sh"
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
    ${RM} -rf lolo*
    src/rootsh -i --logfile=lolo -- ls -l test.sh
    if { grep test.sh lolo.closed; }; then
      echo "OK run simple" >> test.log
    else
      echo "FAIL run simple" >> test.log
    fi
    ${RM} -rf lolo*
    src/rootsh -i --logfile=lolo -- ${RM} lolo
    if [ -f lolo.tampered ] && { grep DELETE lolo.tampered; }; then
      echo "OK run tampered ${RM}" >> test.log
    else
      echo "FAIL run tampered ${RM}" >> test.log
    fi
    ${RM} -rf lolo*
    src/rootsh -i --logfile=lolo -- mv lolo lololo
    if [ -f lolo.tampered ] && { grep DELETE lolo.tampered; }; then
      echo "OK run tampered mv" >> test.log
    else
      echo "FAIL run tampered mv" >> test.log
    fi
    ${RM} -rf lolo*
    src/rootsh -i --logfile=lolo -- "${RM} lolo; touch lolo; touch lolo.tampered"
    if [ -f lolo.tampered ] && { grep RECREATE lolo.tampered; }; then
      echo "OK run tampered touch" >> test.log
    else
      echo "FAIL run tampered touch" >> test.log
    fi
    ${RM} -rf lolo*
    src/rootsh -i --logfile=lolo -- "${RM} lolo; mkdir lolo"
    if [ -f lolo.tampered ] && { grep RECREATE lolo.tampered; }; then
      echo "OK run tampered mkdir" >> test.log
    else
      echo "FAIL run tampered mkdir" >> test.log
    fi
  else
    echo "FAIL make $PARMS" >> test.log
  fi
else
  echo "FAIL ./configure $PARMS" >> test.log
fi


