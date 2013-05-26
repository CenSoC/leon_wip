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

#SERVER_AT="netcpu.zapto.org:8070"
SERVER_AT="localhost:8081"

CERTLINE=" --server_cert certificate.pem --client_cert client_certificate.pem --client_key client_key.pem --server_at ${SERVER_AT} "

CMDLINE=" --model 3:offer \
	--filepath ../simul_data_gmnl.csv --x 5:6 --respondent 1 --choice_set 2 --alternative 3 --fromrow 2 --sort 1,2,3 --best 4 \
	--repeats 1000 \
	--int_resolution 32 --float_resolution single --extended_float_resolution double --approximate_exponents true --complexity_size 88800000 --minimpr 1.0000001 --draws_sets_size 200 --shrink_slowdown 0.3 --dissect off --reduce_exp_complexity off \
	--coeffs_atonce_size 4 \
		--cm 1:-4:4:.003:4 \
		--cm 3:0:4:.003:4 \
		--cm 5:0:2:.003:3 \
	--coeffs_atonce_size 3 \
		--cm 1:.35:3 \
		--cm 3:.35:3 \
		--cm 5:.15:2 \
	--coeffs_atonce_size 2 \
		--cm 1:.05:2 \
" 

SHORTDESC="--short_description \" `cat <<EOF
test gmnl2 blah blah
${CMDLINE}
EOF
`\""

WHOLELINE="${CERTLINE} ${CMDLINE} ${SHORTDESC}"

# GDB this is for normal sh (not bash) 
echo "catch throw" > controller.gdb
echo run ${WHOLELINE} >> controller.gdb 
gdb controller.exe -x controller.gdb
rm controller.gdb
exit

# no gdb(ing)
eval ./controller.exe ${WHOLELINE}
exit

CMDLINE=" --server_cert certificate.pem --client_cert client_certificate.pem --client_key client_key.pem --server_at ${SERVER_AT} \
--model 77:offer \
	--file ../simul_data_gmnl.csv --x 5:8 --resp 1 --fromrow 2 --sort 1,2,3 --alts 2 --best 4 \
	--repeats 1000 \
	--int_resolution 32 --float_resolution single --extended_float_resolution double --complexity_size 88800000 --minimpr 1.0000001 --draws_sets_size 200 --shrink_slowdown 0.3 --dissect off --reduce_exp_complexity off \
	--coeffs_atonce_size 4 \
		--cm 1:-4:4:.003:4 \
		--cm 5:0:4:.003:4 \
		--cm 9:0:2:.003:3 \
	--coeffs_atonce_size 3 \
		--cm 1:.35:3 \
		--cm 5:.35:3 \
		--cm 9:.15:2 \
	--coeffs_atonce_size 2 \
		--cm 1:.05:2 \
" 
