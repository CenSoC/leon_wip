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
#include <netcpu/dataset_1/message/meta.h>

#ifndef CENSOC_NETCPU_GMNL_2_MESSAGE_META_H
#define CENSOC_NETCPU_GMNL_2_MESSAGE_META_H

namespace censoc { namespace netcpu { namespace gmnl_2 { namespace message {

template <typename N>
struct meta {

	typedef netcpu::message::scalar<typename netcpu::message::typepair<uint8_t>::wire> bool_type;
	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;

	bool_type reduce_exp_complexity;
	size_type repetitions;
	size_type draws_sets_size;

	netcpu::dataset_1::message::meta<N> dataset;

	meta()
	: reduce_exp_complexity(-1), repetitions(-1), draws_sets_size(-1)
	{
	}

	void
	print() 
	{
		// a hack indeed...
		censoc::llog() << "reduce_exp_complexity: " << reduce_exp_complexity() << ::std::endl;
		censoc::llog() << "repetitions: " << repetitions() << ::std::endl;
		censoc::llog() << "draws_sets_size: " << draws_sets_size() << ::std::endl; 
		dataset.print();
	}
};

}}}}

#endif
