#!/bin/bash

gdb="g"
con="c"
reb_f="f"

if [ $# = 0 ]
then
    rm mkfs/mkfs fs.img
    # rm fs.img
    make mkfs/mkfs
    make fs.img
    make qemu -j 8
    exit
fi

if [ $1 = $gdb ]
then
    # rm fs.img
    # rm mkfs/mkfs fs.img
    # make mkfs/mkfs
    # make fs.img
    make qemu-gdb
elif [ $1 = $con ]
then
    make gdb
elif [ $1 = $reb_f ]
then
    rm fs.img
    make fs.img
fi