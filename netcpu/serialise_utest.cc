/*

Written and contributed by Leonid Zadorin at the Centre for the Study of Choice
(CenSoC), the University of Technology Sydney (UTS).

Copyright (c) 2012 The University of Technology Sydney (UTS), Australia
<www.uts.edu.au>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
3. All products which use or are derived from this software must display the
following acknowledgement:
  This product includes software developed by the Centre for the Study of
  Choice (CenSoC) at The University of Technology Sydney (UTS) and its
  contributors.
4. Neither the name of The University of Technology Sydney (UTS) nor the names
of the Centre for the Study of Choice (CenSoC) and contributors may be used to
endorse or promote products which use or are derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) AT THE
UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) AND CONTRIBUTORS ``AS IS'' AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) OR
THE UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

*/

#include <iostream>

#include <censoc/lexicast.h>
#include "message.h"
#include "converger_1/message/meta.h"

typedef ::censoc::lexicast< ::std::string> xxx;

template <typename F>
void static
run_fp(F x)
{
	::censoc::netcpu::converger_1::message::meta<unsigned, F, char> dfp;
	::censoc::netcpu::message::serialise_to_decomposed_floating(x, dfp.improvement_ratio_min);
	F y(::censoc::netcpu::message::deserialise_from_decomposed_floating<F>(dfp.improvement_ratio_min));
	::std::cout << x << ", " << y << ::std::endl;
	assert(::std::abs(x - y) < .000001);
}

void static
run_both_fp(double x)
{
	run_fp(x);
	run_fp(static_cast<float>(x));
}


template <typename N>
void static
run_int(N x)
{
	BOOST_STATIC_ASSERT(::boost::is_signed<N>::value == true);
	typedef typename ::boost::make_unsigned<N>::type unsigned_type;
	unsigned_type const x_(x);
	N const y(::censoc::netcpu::message::deserialise_from_unsigned_to_signed_integral(x_));
	::std::cout << x << ", " << y << ::std::endl;
	assert(y == x);
}

int main()
{
	run_both_fp(-5.0);
	run_both_fp(-.0);
	run_both_fp(.0);
	run_both_fp(1.);
	run_both_fp(-1.);
	run_both_fp(5.);
	run_both_fp(5.5);
	run_both_fp(-5.5);
	run_both_fp(-0.5);
	run_both_fp(0.5);
	run_both_fp(1.012345);
	run_both_fp(-1.012345);
	run_both_fp(.012345);
	run_both_fp(-.012345);

	run_int<int>(-1);
	run_int<int>(-2);
	run_int<int>(-3);
	run_int<int>(1);
	run_int<int>(2);
	run_int<int>(3);
	return 0;
}

