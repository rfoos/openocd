#!/bin/bash
# Move to /usr/local install directory.
[ -n "$*" ] || DESTDIR=/home/rick/master-openocd

find tcl/target -name eta_\* -exec cp {} ${DESTDIR}/usr/local/share/openocd/scripts/target/ \;
find tcl/board/ -name eta_\* -exec cp {} ${DESTDIR}/usr/local/share/openocd/scripts/board/ \;
find tcl/interface/ -name eta_\*
find tcl/interface/ftdi/ -name eta_\* -exec cp {} ${DESTDIR}/usr/local/share/openocd/scripts/interface/ftdi/ \;

