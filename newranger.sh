#!/bin/bash
mkdir $1
cd $1
ln -s ../display/lib .
ln -s ../display/src .
cp ../display/platformio.ini .
mkdir include
cd include
ln -s ../../display/include/* .
rm Device.h
cp ../../display/include/Device.h .
