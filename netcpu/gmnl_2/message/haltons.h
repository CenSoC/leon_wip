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

/**
	@note -- currently deprecated (not communicating haltons msg from server to processing peers as the haltons message may be quite large in memory (> 50 Meg) ...

	the code is retained in case later on will improve on compression/network-bandwidth/or-bitorrent-router-like-data-distribution...
	*/

#include <netcpu/message.h>

#include "id.h"

#ifndef CENSOC_NETCPU_GMNL_2_MESSAGE_HALTONS_H
#define CENSOC_NETCPU_GMNL_2_MESSAGE_HALTONS_H

namespace censoc { namespace netcpu { namespace gmnl_2 { namespace message {

struct haltons_base : netcpu::message::message_base<message::messages_ids::haltons> {};

template <typename N, typename F>
struct haltons : haltons_base {

	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	typedef netcpu::message::array<F> float_arraytype; 

	size_type sigma_rows;
	size_type nonsigma_rows;
	size_type nonsigma_columns;
	float_arraytype sigma;
	float_arraytype nonsigma;

	haltons(censoc::param<netcpu::message::read_wrapper>::type raw)
	{
		from_wire(raw);
	}
	
	// hack indeed...
	haltons()
	{
	}
};


}}}}

#endif
