#!/bin/sh

## 
## Sets up and generates all of the stuff that
## automake and libtool do for us.
##

echo "Setting up libtool"
libtoolize --force
echo "Setting up aclocal"
aclocal -I m4
echo "Generating configure"
autoconf
echo "Generating Makefile.in"
automake --add-missing
echo "** runing autogen.sh in dep/rest-client-c"
sh -c "cd dep/rest-client-c && ./autogen.sh"

