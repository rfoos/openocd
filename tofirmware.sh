#!/bin/bash
# Copy OCD target routines to OpenOCD tree.

OCDFIRMWARE=~/firmware/soc/ecm3531/boards/eta_val/projects
OCDCONTRIB=~/openocd/contrib/loaders/flash/etacorem3

# Source files
cp ${OCDCONTRIB}/read.c ${OCDFIRMWARE}/read/src/read.c 
cp ${OCDCONTRIB}/erase.c ${OCDFIRMWARE}/erase/src/erase.c
cp ${OCDCONTRIB}/write.c ${OCDFIRMWARE}/write/src/write.c
cp ${OCDCONTRIB}/load.c ${OCDFIRMWARE}/load/src/load.c
cp ${OCDCONTRIB}/store.c ${OCDFIRMWARE}/store/src/store.c

# include
cp ${OCDCONTRIB}/etacorem3_flash_common.h ${OCDFIRMWARE}/include/.
