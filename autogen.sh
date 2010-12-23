#!/bin/bash

#
#  This file is part of si application launcher
#

AM_VERSION=1.11
AC_VERSION=2.63

run_versioned() {
    local P
    local V

    V=$(echo "$2" | sed -e 's,\.,,g')

    if [ -e "`which $1$V 2> /dev/null`" ] ; then
        P="$1$V"
    else
        if [ -e "`which $1-$2 2> /dev/null`" ] ; then
            P="$1-$2"
        else
            P="$1"
        fi
    fi

    shift 2
    "$P" "$@"
}

set -ex

if type -p colorgcc > /dev/null ; then
   export CC=colorgcc
fi

if [ "x$1" = "xam" ] ; then
    run_versioned automake "$AM_VERSION" -a -c --foreign
    ./config.status
else
    rm -rf autom4te.cache
    rm -f config.cache

    libtoolize -c --force
    run_versioned aclocal "$AM_VERSION" 
    run_versioned autoconf "$AC_VERSION" -Wall
    run_versioned autoheader "$AC_VERSION"
    run_versioned automake "$AM_VERSION" --copy --foreign --add-missing

    if [ "x$1" != "xac" ]; then
        CFLAGS="$CFLAGS -g -O0" ./configure --sysconfdir=/etc --localstatedir=/var "$@"
        make clean
    fi
fi
