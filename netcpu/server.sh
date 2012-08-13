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

MYPATH=`realpath ${0}`
MYDIR=`dirname ${MYPATH}`/

#CMDLINE="--key key.pem --cert certificate.pem --clients_certificates clients_certificates --listen_at 127.0.0.1:8081 --root ${MYDIR}tasks --task_oldage 500 --task_oldage_check 1"
CMDLINE="--key key.pem --cert certificate.pem --clients_certificates clients_certificates --listen_at netcpu.zapto.org:8070 --root ${MYDIR}tasks --task_oldage 500 --task_oldage_check 1"

# GDB this is for normal sh (not bash) 
echo "catch throw" > server.gdb
echo "run ${CMDLINE}" >> server.gdb 
MALLOC_OPTIONS="10nPJ" gdb server.exe -x server.gdb
rm server.gdb

exit

# note -- whilst 'processor' code is concerned with portability issues (i.e. must run on many different systesm) 
# the 'server' et al are not, indeed, expected to run on anything other than a 
# specific/chosen one. Therefore, can afford to write a sys-specific code. 
# In this case such is FreeBSD-centric MALLOC_OPTIONS env setting
MALLOC_OPTIONS="10nPJ" ./server.exe ${CMDLINE}

exit

# release mode
MALLOC_OPTIONS="10nP" ./server.exe ${CMDLINE}

exit

old one with teh use of --dh option
CMDLINE="--key key.pem --cert certificate.pem --dh dh.pem --listen_at 127.0.0.1:8081 --root ${MYDIR}tasks --task_oldage 500 --task_oldage_check 1"
