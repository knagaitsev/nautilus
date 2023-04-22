#!/bin/bash

# Cross-Compiler Toolchain for ${PLATFORM}
#  by Martin Decky <martin@decky.cz>
#
#  GPL'ed, copyleft
#


check_error() {
    if [ "$1" -ne "0" ]; then
	echo
	echo "Script failed: $2"
	exit
    fi
}

BINUTILS_VERSION="2.39"
GCC_VERSION="12.2.0"

BINUTILS="binutils-${BINUTILS_VERSION}.tar.gz"
GCC="gcc-${GCC_VERSION}.tar.gz"

BINUTILS_SOURCE="ftp://ftp.gnu.org/gnu/binutils/"
GCC_SOURCE="ftp://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/"

PLATFORM="aarch64"
WORKDIR=`pwd`
TARGET="${PLATFORM}-linux-gnu"
PREFIX="/opt/toolchain/${PLATFORM}"
BINUTILSDIR="${WORKDIR}/binutils-${BINUTILS_VERSION}"
GCCDIR="${WORKDIR}/gcc-${GCC_VERSION}"
BINOBJDIR="${WORKDIR}/bin-obj"
OBJDIR="${WORKDIR}/gcc-obj"

if false; then

echo ">>> Downloading tarballs"

if [ ! -f "${BINUTILS}" ]; then
    wget -c "${BINUTILS_SOURCE}${BINUTILS}"
    check_error $? "Error downloading binutils."
fi
if [ ! -f "${GCC_CPP}" ]; then
    wget -c "${GCC_SOURCE}${GCC}"
    check_error $? "Error downloading GCC."
fi

echo ">>> Creating destination directory"
if [ ! -d "${PREFIX}" ]; then
    mkdir -p "${PREFIX}" 
    test -d "${PREFIX}"
    check_error $? "Unable to create ${PREFIX}."
fi

echo ">>> Creating binutils work directory"
if [ ! -d "${BINOBJDIR}" ]; then
    mkdir -p "${BINOBJDIR}"
    test -d "${BINOBJDIR}"
    check_error $? "Unable to create ${BINOBJDIR}."
fi

echo ">>> Creating GCC work directory"
if [ ! -d "${OBJDIR}" ]; then
    mkdir -p "${OBJDIR}"
    test -d "${OBJDIR}"
    check_error $? "Unable to create ${OBJDIR}."
fi

echo ">>> Unpacking tarballs"
tar -xvzf "${BINUTILS}"
check_error $? "Error unpacking binutils."
tar -xvzf "${GCC}"
check_error $? "Error unpacking GCC."

echo ">>> Compiling and installing binutils"
cd "${BINOBJDIR}"
check_error $? "Change directory failed."
${BINUTILSDIR}/configure "--target=${TARGET}" "--prefix=${PREFIX}" "--program-prefix=${TARGET}-" "--disable-nls"
check_error $? "Error configuring binutils."
make all install
check_error $? "Error compiling/installing binutils."

fi

echo ">>> Compiling and installing GCC"
cd "${OBJDIR}"
check_error $? "Change directory failed."
"${GCCDIR}/configure" "--target=${TARGET}" "--prefix=${PREFIX}" "--program-prefix=${TARGET}-" --with-gnu-as --with-gnu-ld --disable-nls --disable-threads --enable-languages=c,c++ --disable-multilib --disable-libgcj --without-headers --disable-shared
check_error $? "Error configuring GCC."
if false; then
PATH="${PATH}:${PREFIX}/bin" 
fi
make all-gcc install-gcc
check_error $? "Error compiling/installing GCC."

echo
echo ">>> Cross-compiler for ${TARGET} installed."
