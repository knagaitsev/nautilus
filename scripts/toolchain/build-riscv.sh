#!/bin/bash

# Cross-Compiler Toolchain for ${PLATFORM}
#  by Martin Decky <martin@decky.cz>
#
#  GPL'ed, copyleft
#

# (Modified by Kevin Hayes (github: kjhayes))

check_error() {
    if [ "$1" -ne "0" ]; then
	echo
	echo "Script failed: $2"
	exit
    fi
}

BUILD_BINUTILS=true
BUILD_GCC=true
BUILD_GDB=true

BINUTILS_VERSION="2.39"
GCC_VERSION="12.2.0"
GDB_VERSION="13.2"

BINUTILS="binutils-${BINUTILS_VERSION}.tar.gz"
GCC="gcc-${GCC_VERSION}.tar.gz"
GDB="gdb-${GDB_VERSION}.tar.gz"

BINUTILS_SOURCE="ftp://ftp.gnu.org/gnu/binutils/"
GCC_SOURCE="ftp://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/"
GDB_SOURCE="ftp://ftp.gnu.org/gnu/gdb/"

PLATFORM="riscv64"
WORKDIR=`pwd`
TARGET="${PLATFORM}-linux-gnu"
PREFIX="/home/kjhayes/opt/toolchain/${PLATFORM}"

BINUTILSDIR="${WORKDIR}/binutils-${BINUTILS_VERSION}"
GCCDIR="${WORKDIR}/gcc-${GCC_VERSION}"
GDBDIR="${WORKDIR}/gdb-${GDB_VERSION}"

BINOBJDIR="${WORKDIR}/bin-obj"
GCCOBJDIR="${WORKDIR}/gcc-obj"
GDBOBJDIR="${WORKDIR}/gdb-obj"

echo ">>> Downloading tarballs"

if [ "$BUILD_BINUTILS" = true ]; then
if [ ! -f "${BINUTILS}" ]; then
    wget -c "${BINUTILS_SOURCE}${BINUTILS}"
    check_error $? "Error downloading binutils."
fi
fi

if [ "$BUILD_GCC" = true ]; then
if [ ! -f "${GCC}" ]; then
    wget -c "${GCC_SOURCE}${GCC}"
    check_error $? "Error downloading GCC."
fi
fi

if [ "$BUILD_GDB" = true ]; then
if [ ! -f "${GDB}" ]; then
    wget -c "${GDB_SOURCE}${GDB}"
    check_error $? "Error downloading GDB."
fi
fi

echo ">>> Creating destination directory"
if [ ! -d "${PREFIX}" ]; then
    mkdir -p "${PREFIX}" 
    test -d "${PREFIX}"
    check_error $? "Unable to create ${PREFIX}."
fi

echo ">>> Creating binutils work directory"
if [ "$BUILD_BINUTILS" = true ]; then
if [ ! -d "${BINOBJDIR}" ]; then
    mkdir -p "${BINOBJDIR}"
    test -d "${BINOBJDIR}"
    check_error $? "Unable to create ${BINOBJDIR}."
fi
fi

if [ "$BUILD_GCC" = true ]; then
echo ">>> Creating GCC work directory"
if [ ! -d "${GCCOBJDIR}" ]; then
    mkdir -p "${GCCOBJDIR}"
    test -d "${GCCOBJDIR}"
    check_error $? "Unable to create ${GCCOBJDIR}."
fi
fi

if [ "$BUILD_GDB" = true ]; then
echo ">>> Creating GDB work directory"
if [ ! -d "${GDBOBJDIR}" ]; then
    mkdir -p "${GDBOBJDIR}"
    test -d "${GDBOBJDIR}"
    check_error $? "Unable to create ${GDBOBJDIR}."
fi
fi

echo ">>> Unpacking tarballs"

if [ "$BUILD_BINUTILS" = true ]; then
tar -xvzf "${BINUTILS}"
check_error $? "Error unpacking binutils."
fi

if [ "$BUILD_GCC" = true ]; then
tar -xvzf "${GCC}"
check_error $? "Error unpacking GCC."
fi

if [ "$BUILD_GDB" = true ]; then
tar -xvzf "${GDB}"
check_error $? "Error unpacking GDB."
fi

if [ "$BUILD_BINUTILS" = true ]; then
echo ">>> Compiling and installing binutils"
cd "${BINOBJDIR}"
check_error $? "Change directory failed."
${BINUTILSDIR}/configure "--target=${TARGET}" "--prefix=${PREFIX}" "--program-prefix=${TARGET}-" "--disable-nls"
check_error $? "Error configuring binutils."
make all install
check_error $? "Error compiling/installing binutils."
fi

if [ "$BUILD_GCC" = true ]; then
echo ">>> Compiling and installing GCC"
cd "${GCCOBJDIR}"
check_error $? "Change directory failed."
"${GCCDIR}/configure" "--target=${TARGET}" "--prefix=${PREFIX}" "--program-prefix=${TARGET}-" --with-gnu-as --with-gnu-ld --disable-nls --disable-threads --enable-languages=c,c++ --disable-multilib --disable-libgcj --without-headers --disable-shared
check_error $? "Error configuring GCC."
if false; then
PATH="${PATH}:${PREFIX}/bin" 
fi
make all-gcc install-gcc
check_error $? "Error compiling/installing GCC."
fi

if [ "$BUILD_GDB" = true ]; then
echo ">>> Compiling and installing GDB"
cd "${GDBOBJDIR}"
check_error $? "Change directory failed."
"${GDBDIR}/configure" "--target=${TARGET}" "--prefix=${PREFIX}" "--program-prefix=${TARGET}-"
check_error $? "Error configuring GDB."
if false; then
PATH="${PATH}:${PREFIX}/bin" 
fi
make all-gdb install-gdb
check_error $? "Error compiling/installing GDB."
fi

echo
echo ">>> Cross-compiler for ${TARGET} installed."
