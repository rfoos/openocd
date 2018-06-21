#!/bin/bash
# Copy OCD target routines to OpenOCD tree.

OCDFIRMWARE=~/firmware/soc/ecm3531/boards/eta_val/projects
OCDCONTRIB=~/openocd/contrib/loaders/flash/etacorem3

# Source files
cp ${OCDFIRMWARE}/read/src/read.c ${OCDCONTRIB}/.
cp ${OCDFIRMWARE}/erase/src/erase.c ${OCDCONTRIB}/.
cp ${OCDFIRMWARE}/write/src/write.c ${OCDCONTRIB}/.

# include
cp ${OCDFIRMWARE}/include/etacorem3_flash_common.h ${OCDCONTRIB}/.
