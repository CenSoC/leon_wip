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

#include <netcpu/message.h>

#include "id.h"

#ifndef CENSOC_NETCPU_CONVERGER_1_MESSAGE_RES_H
#define CENSOC_NETCPU_CONVERGER_1_MESSAGE_RES_H

namespace censoc { namespace netcpu { namespace converger_1 { namespace message {

template <typename F> struct float_res;
template<> struct float_res<float> { enum {value = 0}; };
template <> struct float_res<double> { enum {value = 1}; };

template <typename N> struct int_res;
template<> struct int_res<uint32_t> { enum {value = 0}; };
template <> struct int_res<uint64_t> { enum {value = 1}; };

struct res : netcpu::message::message_base<message::messages_ids::res> {

	typedef netcpu::message::scalar<uint8_t> data_type;

	data_type int_res; 
	data_type float_res;
	data_type extended_float_res;
	data_type approximate_exponents;

	res()
	: int_res(-1), float_res(-1), extended_float_res(-1), approximate_exponents(-1) {
	}

	res(netcpu::message::read_wrapper & raw)
	{
		from_wire(raw);
	}
	void
	print() 
	{
		// a hack indeed...
		censoc::llog() << "int resolution: " << int_res() << ::std::endl; 
		censoc::llog() << "float resolutin: " << float_res() << ::std::endl;
		censoc::llog() << "extended float resolutin: " << float_res() << ::std::endl;
		censoc::llog() << "approximate exponents: " << approximate_exponents() << ::std::endl;
	}
};

}}}}

#endif
