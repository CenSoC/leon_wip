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

#include "../types.h"
#include "id.h"
#include "complexitywise_element.h"

#ifndef CENSOC_NETCPU_CONVERGER_1_MESSAGE_PEER_REPORT_H
#define CENSOC_NETCPU_CONVERGER_1_MESSAGE_PEER_REPORT_H

namespace censoc { namespace netcpu { namespace converger_1 { namespace message {

/**{ TODO -- the following comments may be outdated!!!

	peer resets coeffs metadata counters on EVERY find (but reporting such a find is only done if it is better than a previous find -- so peer keeps the 'best so far' for the whole duration of calculations)

	the peer when sending the report shall:
	keep mods counter since last report
	keep best-found-value since last report
	on timeout, it will harvest all and send through
	then reset kept counters/values
	}*/

template <typename N>
struct combo_modifications_size {
	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;

	size_type i;
	size_type modifications;
};


template <typename N, typename F>
struct bootstrapping_peer_report : netcpu::message::message_base<message::messages_ids::bootstrapping_peer_report> {

	//{ typedefs...
	typedef netcpu::message::scalar<typename netcpu::message::typepair<uint8_t>::wire> bool_type;
	typedef netcpu::message::scalar<typename converger_1::key_typepair::wire> key_type;
	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	//typedef netcpu::message::scalar<F> float_type;
	typedef netcpu::message::decomposed_floating float_type; 
	typedef netcpu::message::array<typename netcpu::message::typepair<N>::wire> coefficients_type;
	//}

	key_type echo_status_key; 
	coefficients_type coeffs;
	float_type value;

	bootstrapping_peer_report(censoc::param<netcpu::message::read_wrapper>::type raw)
	{
		from_wire(raw);
	}

	// hack indeed
	bootstrapping_peer_report()
	{
	}

	void
	print() const
	{
		// a hack indeed...
		typedef typename netcpu::message::typepair<N>::ram size_type;
		censoc::llog() << "echo_status_key: " << echo_status_key() << ::std::endl;
		censoc::llog() << "value: " << netcpu::message::deserialise_from_decomposed_floating<F>(value) << ::std::endl;
		censoc::llog() << "coeffs: " << coeffs.size() << "\n {";
		for (size_type i(0); i != coeffs.size(); ++i) 
			censoc::llog() << coeffs(i) << (i == coeffs.size() - 1 ? "}\n" : ", ");
	}
};

template <typename N, typename F>
struct peer_report : netcpu::message::message_base<message::messages_ids::peer_report> {

	//{ typedefs...
	typedef netcpu::message::scalar<typename netcpu::message::typepair<uint8_t>::wire> bool_type;
	typedef netcpu::message::scalar<typename converger_1::key_typepair::wire> key_type;
	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	//typedef netcpu::message::scalar<F> float_type;
	typedef netcpu::message::decomposed_floating float_type; 
	typedef netcpu::message::array<complexitywise_element<N> > complexities_type;
	//}

	key_type echo_status_key; 
	float_type value;
	size_type value_complexity;
	complexities_type complexities;

	peer_report(censoc::param<netcpu::message::read_wrapper>::type raw)
	{
		from_wire(raw);
	}

	// hack indeed
	peer_report()
	{
	}

	void
	print() const
	{
		// a hack indeed...
		typedef typename netcpu::message::typepair<N>::ram size_type;
		censoc::llog() << "echo_status_key: " << echo_status_key() << ::std::endl;
		censoc::llog() << "value: " << netcpu::message::deserialise_from_decomposed_floating<F>(value) << ::std::endl;
	}
};


}}}}

#endif
