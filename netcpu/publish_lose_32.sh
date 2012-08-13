#! /bin/sh -x

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

. ./bail.sh

SUBREPO="lose_32_"
TARGETS="unknown"

# ( ./custom_env_lose_32.sh make -DBUILD_FAST clean processor && make -DBUILD_SMALL update_client nosleep ) || bail building

# cludge for the time-being later expand builds for specific target cpus
# note MUST build update_client and nosleep with the same flags (see nosleep.cc for reasoning)
( ./custom_env_lose_32.sh make -DBUILD_SMALL clean update_client nosleep ) || bail BUILD_SMALL
. ./publish_common.sh
( ./custom_env_lose_32.sh make -DBUILD_FAST clean processor ) || bail BUILD_FAST
. ./publish_common.sh

exit 0
