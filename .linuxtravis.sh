#!/bin/bash

set -ev

# Backup the cloned repository, because here we will compile for both Linux and Windows
cd ..
cp -R MonetDBLite MonetDBLiteWindows/
cp -R MonetDBLite MonetDBLiteMacOS/
mkdir -p libs/{linux,windows,macos}

# Do the Linux compilation
cd MonetDBLite
sh linux.sh
cd ..
cp -R MonetDBLite/monetdb-java-lite/src/main/resources/libs/linux libs/linux

# Do the Windows compilation
cd MonetDBLiteWindows
sh windows.sh
cd ..
cp -R MonetDBLiteWindows/monetdb-java-lite/src/main/resources/libs/windows libs/windows

# Do the MacOS compilation
cd MonetDBLiteMacOS
sh macos.sh
cd ..
cp -R MonetDBLiteMacOS/monetdb-java-lite/src/main/resources/libs/macos libs/macos

# On the deploy phase we will upload the libs

