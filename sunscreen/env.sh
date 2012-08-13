#!/bin/sh

#
# Written and contributed by Leonid Zadorin at the Centre for the Study of Choice
# (CenSoC), the University of Technology Sydney (UTS).
#
# Copyright (c) 2012 The University of Technology Sydney (UTS), Australia
# <www.uts.edu.au>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 3. All products which use or are derived from this software must display the
# following acknowledgement:
#   This product includes software developed by the Centre for the Study of
#   Choice (CenSoC) at The University of Technology Sydney (UTS) and its
#   contributors.
# 4. Neither the name of The University of Technology Sydney (UTS) nor the names
# of the Centre for the Study of Choice (CenSoC) and contributors may be used to
# endorse or promote products which use or are derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) AT THE
# UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) AND CONTRIBUTORS ``AS IS'' AND ANY 
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
# DISCLAIMED. IN NO EVENT SHALL THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) OR
# THE UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) OR CONTRIBUTORS BE LIABLE FOR ANY 
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
#

# a quick (not always completely elegant, nor correct) hack for the time-being... indubitably. moreover, it has a lot of legacy stuff (eg mingw32 which is no longer used)... so todo for later on, if there shall be some free time, clean this up...

# TODO -- later merge with the same(ish) code from netcpu project

USER=`whoami`

MYPATH=`realpath ${0}`
MYDIR=`dirname ${MYPATH}`
GCC_VERSION="4.6.2"

TOP=/usr/home/${USER}
TARGET_XXX="x86_64-unknown-freebsd8.2"

GCC_TOP=${TOP}/gcc_sunscreen/installed
GDB_TOP=${TOP}/gdb/installed
BINUTILS_TOP=${TOP}/binutils/installed
BOOST_TOP=${TOP}/boost/installed

# NOTE -- temp hack for LIB XLS...
MYPATH=`realpath ${0}`
MYDIR=`dirname ${MYPATH}`/
# end of LIB XLS hack


GCC_LIBS=${GCC_TOP}/lib/gcc/${TARGET_XXX}/${GCC_VERSION}/:${GCC_TOP}/lib
GDB_LIBS=${GDB_TOP}/lib
BINUTILS_LIBS=${BINUTILS_TOP}/${TARGET_XXX}/lib/:${BINUTILS_TOP}/lib

BOOST_LIBS=${BOOST_TOP}/lib

# not using these because of the cross-compiling instances :/usr/lib:/usr/local/lib
LIBS=${BINUTILS_LIBS}:${GCC_LIBS}:${GDB_LIBS}:${BOOST_LIBS}

export LD_RUN_PATH=${LIBS}${LD_RUN_PATH:+:}${LD_RUN_PATH}
export LD_LIBRARY_PATH=${LIBS}:/usr/home/${USER}/gmp/installed/lib:/usr/home/${USER}/ppl/installed/lib:/usr/home/${USER}/cloog/installed/lib:/usr/home/${USER}/mpfr/installed/lib:/usr/home/${USER}/mpc/installed/lib${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH}

export COMPILER_PATH=${GCC_TOP}/libexec/gcc/${TARGET_XXX}/${GCC_VERSION}:${GCC_TOP}/libexec:${GCC_TOP}/bin:${GCC_TOP}/${TARGET_XXX}/lib:${BINUTILS_TOP}/bin
# export GCC_EXEC_PREFIX=${GCC_TOP}/lib/gcc/

# NOT using LIBRARY_PATH by itself because it is NOT used by cross-compiler builds (e.g. mingw32 cross-build). This variable is ONLY used by native compiler builds...
export LIBRARY_PATH=${LIBS}${LIBRARY_PATH:+:}${LIBRARY_PATH}
# ... instead  will set LIBRARY_PATH_XXX an env var which will be picked up by the makefile:
IFS_TMP=${IFS}
IFS=":"
for i in ${LIBRARY_PATH}
do
LIBRARY_PATH_XXX="${LIBRARY_PATH_XXX} -L${i}"
done
IFS=${IFS_TMP}
export LIBRARY_PATH_XXX

# in the above -- the LIBRARY_PATH is still exported -- because some code is built on native compiler (libxls) still (it, by the way, is not needed by the cross-compiler)... and it does not get build via my own makefile... rather an ugly hack... but will do for the time-being...
# TODO -- on the other hand may take this export out -- since it's only the libxls that is built... and being the lib itself -- it may not need linking with other libs anyway... WILL THINK SOME MORE LATER ON WHEN/IF THERE IS MORE TIME...

BINUTILS_INCS=${BINUTILS_TOP}/include

# probably not needed (leftover from previous experiments)... todo -- clean up if there is time later
GCC_INCS=${GCC_TOP}/lib/gcc/${TARGET_XXX}/${GCC_VERSION}/include:${GCC_TOP}/include/c++/${GCC_VERSION}/${TARGET_XXX}:${GCC_TOP}/include/c++/${GCC_VERSION}:${GCC_TOP}/${TARGET_XXX}/include:${GCC_TOP}/${TARGET_XXX}/include/c++/${GCC_VERSION}:${GCC_TOP}/${TARGET_XXX}/include/c++/${GCC_VERSION}/${TARGET_XXX}

BOOST_INCS=${BOOST_TOP}/include

# not using these because of the cross-compiling instances :/usr/include:/usr/local/include
INCS=${BINUTILS_INCS}:${GCC_INCS}:${BOOST_INCS}

# probably not needed (leftover from previous experiments)... todo -- clean up if there is time later
export CPATH=${INCS}${CPATH:+:}${CPATH}
export C_INCLUDE_PATH=${INCS}${C_INCLUDE_PATH:+:}${C_INCLUDE_PATH}
export CPLUS_INCLUDE_PATH=${INCS}${CPLUS_INCLUDE_PATH:+:}${CPLUS_INCLUDE_PATH}
export OBJC_INCLUDE_PATH=${INCS}${OBJC_INCLUDE_PATH:+:}${OBJC_INCLUDE_PATH}

export PATH=/usr/local/texlive/2011/bin/amd64-freebsd:${BINUTILS_TOP}/${TARGET_XXX}/bin:${BINUTILS_TOP}/bin:${GCC_TOP}/libexec/gcc/${TARGET_XXX}/${GCC_VERSION}:${GCC_TOP}/libexec:${GCC_TOP}/bin:${GDB_TOP}/bin{PATH:+:}${PATH}

export MANPATH=${BINUTILS_TOP}/share/man:${GCC_TOP}/share/man:`manpath`

# for gnuplot
export GDFONTPATH=/usr/local/lib/X11/fonts/TTF
export GNUPLOT_FONTPATH=/usr/local/lib/X11/fonts/TTF


echo ${LD_RUN_PATH}
echo ${LD_LIBRARY_PATH}
echo ${LIBRARY_PATH}
echo ${LIBRARY_PATH_XXX}
echo ${GCC_EXEC_PREFIX}
echo ${CPATH}
echo ${PATH}

if [ ${#} -eq 0 ]
then
echo interactive mode
PS1="++c > " bash
else
echo command mode
${*}
fi

