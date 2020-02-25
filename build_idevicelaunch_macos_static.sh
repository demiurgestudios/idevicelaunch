#!/bin/bash

# store the directory of the script
#
# see also: http://stackoverflow.com/questions/59895/can-a-bash-script-tell-what-directory-its-stored-in
SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# store repos and compilation intermediates in user's HOME folder.
TEMPDIR=${HOME}/idevicelaunch_tmp

# cleanup
function cleanup {
  if [ -d "${TEMPDIR}/libimobiledevice" ]; then
    rm -rf "${TEMPDIR}/libimobiledevice"
  fi
  if [ -d "${TEMPDIR}/libusbmuxd" ]; then
    rm -rf "${TEMPDIR}/libusbmuxd"
  fi
  if [ -d "${TEMPDIR}/libplist" ]; then
    rm -rf "${TEMPDIR}/libplist"
  fi
  if [ -d "${TEMPDIR}" ]; then
    rm -rf "${TEMPDIR}"
  fi
}
trap cleanup exit
cleanup

echo ==============================================================================
echo creating tmpdir...
echo ==============================================================================
mkdir -p ${TEMPDIR}

echo ==============================================================================
echo git openssl...
echo ==============================================================================
cd ${TEMPDIR} && git clone https://github.com/openssl/openssl.git -b "OpenSSL_1_1_1-stable" || exit 1
echo ==============================================================================
echo git libplist...
echo ==============================================================================
cd ${TEMPDIR} && git clone https://github.com/libimobiledevice/libplist.git || exit 1
echo ==============================================================================
echo git libusbmuxd...
echo ==============================================================================
# Copy is a workaround for: https://github.com/libimobiledevice/libimobiledevice/issues/916
cd ${TEMPDIR} && git clone https://github.com/libimobiledevice/libusbmuxd.git && mkdir -p /usr/local/include && cp ${TEMPDIR}/libusbmuxd/include/usbmuxd.h /usr/local/include/usbmuxd.h || exit 1
echo ==============================================================================
echo git libimobiledevice...
echo ==============================================================================
cd ${TEMPDIR} && git clone https://github.com/libimobiledevice/libimobiledevice.git || exit 1

echo ==============================================================================
echo building openssl...
echo ==============================================================================
cd ${TEMPDIR}/openssl && ./Configure darwin64-x86_64-cc no-shared no-ssl2 no-sctp no-camellia no-capieng no-cast no-cms no-idea no-md2 no-mdc2 no-rc5 no-rfc3779 no-seed no-whirlpool no-deprecated no-hw no-engine && make clean && make depend && make build_libs || exit 1
export CPATH="${TEMPDIR}/openssl/include:$CPATH"
export LD_LIBRARY_PATH="${TEMPDIR}/openssl:$LD_LIBRARY_PATH"
export LIBRARY_PATH="${TEMPDIR}/openssl/:$LIBRARY_PATH"
export PKG_CONFIG_PATH="${TEMPDIR}/openssl"

echo ==============================================================================
echo building libplist...
echo ==============================================================================
cd ${TEMPDIR}/libplist && NOCONFIGURE=1 ./autogen.sh && ./configure --prefix=${TEMPDIR}/libplist --without-cython --enable-static --disable-shared && make && make install  || exit 1
export libplist_CFLAGS=-I${TEMPDIR}/libplist/include
export libplist_LIBS=-L${TEMPDIR}/libplist/lib

echo ==============================================================================
echo building libusbmuxd...
echo ==============================================================================
cd ${TEMPDIR}/libusbmuxd && NOCONFIGURE=1 ./autogen.sh && ./configure --prefix=${TEMPDIR}/libusbmuxd LDFLAGS="-L${TEMPDIR}/libplist/lib -lplist" --enable-static --disable-shared && make && make install  || exit 1
export libusbmuxd_CFLAGS=-I${TEMPDIR}/libusbmuxd/include
export libusbmuxd_LIBS=-L${TEMPDIR}/libusbmuxd/lib

echo ==============================================================================
echo building libimobiledevice...
echo ==============================================================================
cd ${TEMPDIR}/libimobiledevice && NOCONFIGURE=1 ./autogen.sh && ./configure --prefix=${TEMPDIR}/libimobiledevice LDFLAGS="-L${TEMPDIR}/libusbmuxd/lib -lusbmuxd -L${TEMPDIR}/libplist/lib -lplist" --without-cython --enable-static --disable-shared && make && make install || exit 1

echo ==============================================================================
echo build...
echo ==============================================================================
cd ${SCRIPTDIR} && clang -Wno-gnu-zero-variadic-macro-arguments -std=c99 -pedantic -pedantic-errors -O3 -I${TEMPDIR}/libimobiledevice/include -I${TEMPDIR}/libplist/include -I${TEMPDIR}/openssl/include -L${TEMPDIR}/libimobiledevice/lib -limobiledevice -L${TEMPDIR}/openssl -lssl -lcrypto -L${TEMPDIR}/libusbmuxd/lib -lusbmuxd -L${TEMPDIR}/libplist/lib -lplist -o idevicelaunch idevicelaunch.c || exit 1
