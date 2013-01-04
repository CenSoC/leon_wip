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

//{ includes...
#include <netcpu/message.h>
#include <netcpu/types.h>

#include "id.h"
#include "complexitywise_element.h"
#include "coeffwise_server_state_sync.h"
//}

#ifndef CENSOC_NETCPU_CONVERGER_1_MESSAGE_SERVER_STATE_SYNC_H
#define CENSOC_NETCPU_CONVERGER_1_MESSAGE_SERVER_STATE_SYNC_H

namespace censoc { namespace netcpu { namespace converger_1 { namespace message {

template <typename N, typename F>
struct server_state_sync : netcpu::message::message_base<message::messages_ids::server_state_sync> {
  //{ typedefs
	typedef netcpu::message::scalar<typename converger_1::key_typepair::wire> key_type;
	typedef netcpu::message::array<coeffwise_server_state_sync<N, F> > coeff_arraytype;
	//typedef netcpu::message::scalar<F> float_type;
	typedef netcpu::message::decomposed_floating<> float_type; 
	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	typedef netcpu::message::array<complexitywise_element<N> > complexities_type;
	typedef netcpu::message::scalar<typename netcpu::message::typepair<uint8_t>::wire> bool_type;
	//}

	key_type echo_status_key;
	coeff_arraytype coeffs; 
	float_type value;
	bool_type am_bootstrapping;
	complexities_type complexities;

	void
	print() const
	{
		// a hack indeed...
		typedef typename netcpu::message::typepair<N>::ram size_type;
		censoc::llog() << "echo_status_key: " << echo_status_key() << ::std::endl;
		censoc::llog() << "value: " << netcpu::message::deserialise_from_decomposed_floating<F>(value) << ::std::endl;
		censoc::llog() << "am_bootstrapping: " << am_bootstrapping() << ::std::endl;
		censoc::llog() << "coeffs: " << coeffs.size() << ::std::endl;
		for (size_type i(0); i != coeffs.size(); ++i) 
			coeffs(i).print();
	}
};

template <typename N, typename F>
struct server_recentre_only_sync : netcpu::message::message_base<message::messages_ids::server_recentre_only_sync>  {
  //{ typedefs
	typedef netcpu::message::scalar<typename converger_1::key_typepair::wire> key_type;
	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	//typedef netcpu::message::scalar<F> float_type;
	typedef netcpu::message::decomposed_floating<> float_type; 
	typedef netcpu::message::array<complexitywise_element<N> > complexities_type;
	//}

	key_type echo_status_key;
	float_type value;
	size_type value_complexity;
	complexities_type complexities;

};

template <typename N>
struct server_complexity_only_sync : netcpu::message::message_base<message::messages_ids::server_complexity_only_sync> {
  //{ typedefs
	typedef netcpu::message::scalar<typename netcpu::message::typepair<uint8_t>::wire> bool_type;
	typedef netcpu::message::array<complexitywise_element<N> > complexities_type;
	//}

	bool_type noreply;
	complexities_type complexities;
};


}}}}

#endif
