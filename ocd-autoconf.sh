#!/bin/sh
# Rick Foos <rfoos at solengtech.com>

# Packages to do an autoconf build of OpenOCD on fresh LTS Ubuntu.

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
sudo apt-get -y install openssh-server build-essential gcc g++ git make \
    m4 python tar zip unzip bzip2 libtool pkg-config automake \
    autoconf autotools-dev gettext texinfo texlive flex doxygen \
    graphviz libusb-dev libftdi1-dev \
    dos2unix libusb-1.0-0-dev libusb-1.0-0 libftdi1 libftdi1-2 \
    libhidapi-dev libhidapi-hidraw0 libhidapi-libusb0
    
