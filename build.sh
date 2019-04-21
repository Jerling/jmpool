#!/usr/bin/env bash

INSTALL_DIR=/usr/local
INC_DIR=jmp
BUILD_DIR=build

if [ "$1" == "uninstall" ]
then
    sudo rm -rf $INSTALL_DIR/include/jmp*
    sudo rm -rf $INSTALL_DIR/lib/*jmp*
    exit
fi
if [ ! -d $BUILD_DIR ]
then
    mkdir $BUILD_DIR
fi

cd $BUILD_DIR && cmake .. && make && cd -

if [ "$1" == "install" ]
then
    if [ ! -d $INSTALL_DIR/include/$INC_DIR ]
    then
        sudo mkdir $INSTALL_DIR/include/$INC_DIR
    fi
    sudo cp $INC_DIR/*.h $INSTALL_DIR/include/$INC_DIR
    sudo cp jmpool.h $INSTALL_DIR/include
    sudo cp $BUILD_DIR/lib/libjmpool.so $INSTALL_DIR/lib
fi

