#!/bin/sh
# Rick Foos <rfoos at solengtech.com>
# Packages to do a docker build of OpenOCD on Ubuntu.

prompt_install="Do you want to install these packages now (y|n)?"
prompt_please="\nPlease answer y, or n\n"

cat << EOM
Root or Sudo priviledges are required to do this. If you do not have
sufficient priviledge, copy the install command from above, and have
your admin install the packages.

EOM

while true; do
read -r -p  "${prompt_install}" yn
case $yn in
    [Yy]* ) printf "\nStarting package install (sudo required).\n"; break;;
    [Nn]* ) printf "\nInstall canceled.\n"; exit 0;;
    * )     printf "${prompt_please}";;
esac
done

sudo apt-get -y update && sudo apt-get -y upgrade && \
sudo apt-get -y install lsb-release gcc g++ git make cmake m4 python \
    tar zip unzip bzip2 xz-utils curl wget libtool pkg-config automake \
    autoconf autotools-dev gettext libglib2.0-dev libx11-dev libxext-dev \
    libudev-dev libconfuse-dev libboost-all-dev texinfo texlive bison \
    flex doxygen patchelf chrpath libx11-dev libxext-dev nsis dos2unix \
    mingw-w64
