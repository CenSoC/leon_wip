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

#ifndef CENSOC_NETCPU_CONVERGER_1_MESSAGE_META_H
#define CENSOC_NETCPU_CONVERGER_1_MESSAGE_META_H

namespace censoc { namespace netcpu { namespace converger_1 { namespace message {

/**
	@note -- usage mode for messages: controller may just use message directly (not much referencing of message's field in arithmetic formulas). others should use in-ram types...

	More importantly -- the only reason to allow 'direct/in-ram' usage is when one is only doing IO on such fields (i.e. not using values in various arithmetic calculations). Otherwise, the use of faster types (like uint_fast8_t vs uint8_t) would be advised thus deprecating the need for direct use of message's fields (not to mention 'tight' cache packing that message structure lacks due to virtual methods et al).

	Now, in case of a 'controller' case, there may not be much arithmetic use of the message's fields. But in case of the processor and server, the need for variables in arthmetic context is much greater (otherwise one would not need to communicate those in the first place).
	*/

struct meta_base : netcpu::message::message_base<message::messages_ids::meta> {};

template <typename N, typename F, typename Model>
struct meta : meta_base {

	typedef netcpu::message::scalar<typename netcpu::message::typepair<uint8_t>::wire> bool_type;
	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	typedef netcpu::message::array<typename netcpu::message::typepair<N>::wire> array_type;
	//typedef netcpu::message::scalar<F> float_type; 
	typedef netcpu::message::decomposed_floating<> float_type; 

	size_type dissect_do;
	size_type complexity_size;
	float_type improvement_ratio_min;
	netcpu::message::array<uint8_t> bulk_msg_hush;

	Model model;

	meta()
	: dissect_do(-1), complexity_size(0)
	{
		netcpu::message::serialise_to_decomposed_floating<F>(0, improvement_ratio_min);
	}

	void
	print() 
	{
		// a hack indeed...
		censoc::llog() << "dissect_do: " << dissect_do() << ::std::endl;
		censoc::llog() << "total complexity limit: " << complexity_size() << ::std::endl; 
		censoc::llog() << "minimpr: " << netcpu::message::deserialise_from_decomposed_floating<F>(improvement_ratio_min) << ::std::endl; 
		model.print();
	}
};

struct skip_bulk : netcpu::message::message_base<message::messages_ids::skip_bulk>  {
};

}}}}

#endif
