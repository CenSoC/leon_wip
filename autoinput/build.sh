#!/bin/sh -x


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


# temp hack only, use with cross-build env shell settings from netcpu project..

bail()
{
	echo "Error: [$1]"
	exit 1
}


# i686-w64-mingw32-gcc -mwindows -s -O3 -DNDEBUG -D_WIN32_WINNT=0x0501 autoinput.cc  -o autoinput.exe

#g++ -mwindows -s -O3 -DNDEBUG -D_WIN32_WINNT=0x0501 autoinput.cc 

rm *.exe *.zip autoinput/*

mkdir autoinput

# c++ -mwindows -static -s -O3 -DNDEBUG -D_WIN32_WINNT=0x0501 -DACTIONS_FILE=\"start.csv\" autoinput.cc  -o autoinput_start.exe
# c++ -mwindows -static -s -O3 -DNDEBUG -D_WIN32_WINNT=0x0501 -DACTIONS_FILE=\"stop.csv\" autoinput.cc  -o autoinput_stop.exe
c++ -Wall -mwindows -static -s -O3 -DNDEBUG -D_WIN32_WINNT=0x0501 autoinput.cc -o autoinput.exe

#cp autoinput_start.exe autoinput/autoinput_start.xxx
#cp autoinput_stop.exe autoinput/autoinput_stop.xxx
cp info_etc/autoinput.pdf autoinput/README.pdf || bail "doc copy"
cp autoinput.exe autoinput/ || bail "exe copy"
cp start.csv stop.csv autoinput/ || bail "sample csv copy"
cp ../LICENSE.txt autoinput/ || bail "license copy"

zip -r autoinput.zip autoinput

