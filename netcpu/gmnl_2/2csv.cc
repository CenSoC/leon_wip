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

#include <fstream>

#include <censoc/lexicast.h>
#include <netcpu/fstream_to_wrapper.h>
#include <netcpu/converger_1/message/res.h>
#include <netcpu/converger_1/message/meta.h>
#include <netcpu/converger_1/message/bulk.h>
#include "message/meta.h"
#include "message/bulk.h"

namespace censoc { namespace netcpu { namespace gmnl_2 {

typedef censoc::lexicast< ::std::string> xxx;

::std::string const static separator(",");

template <typename N, typename F>
void static
extract()
{
	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef netcpu::converger_1::message::meta<N, F, gmnl_2::message::meta<N> > meta_msg_type;
	typedef netcpu::converger_1::message::bulk<N, F, gmnl_2::message::bulk<N> > bulk_msg_type;

	netcpu::message::read_wrapper msg_wire;

	::std::ifstream meta_msg_file("meta.msg", ::std::ios::binary);
	if (meta_msg_file == false)
		throw ::std::runtime_error("missing meta message during load");
	netcpu::message::fstream_to_wrapper(meta_msg_file, msg_wire);
	meta_msg_type meta_msg;
	meta_msg.from_wire(msg_wire);
	//meta_msg.print();

	::std::ifstream bulk_msg_file("bulk.msg", ::std::ios::binary);
	if (bulk_msg_file == false)
		throw ::std::runtime_error("missing bulk message during load");
	netcpu::message::fstream_to_wrapper(bulk_msg_file, msg_wire);
	bulk_msg_type bulk_msg;
	bulk_msg.from_wire(msg_wire);

	::std::vector<uint8_t> choice_sets_alternatives(bulk_msg.model.dataset.choice_sets_alternatives.size());
	::memcpy(choice_sets_alternatives.data(), bulk_msg.model.dataset.choice_sets_alternatives.data(), choice_sets_alternatives.size());
	uint8_t * choice_sets_alternatives_ptr(choice_sets_alternatives.data());

	::std::vector<int8_t> matrix_composite(bulk_msg.model.dataset.matrix_composite.size());
	for (size_type i(0); i != matrix_composite.size(); ++i)
		matrix_composite[i] = netcpu::message::deserialise_from_unsigned_to_signed_integral(bulk_msg.model.dataset.matrix_composite(i));
	int8_t * matrix_composite_ptr(matrix_composite.data());

	size_type const x_size(meta_msg.model.dataset.x_size()); 
	assert(x_size);
	::std::vector<size_type> respondents_choice_sets;
	respondents_choice_sets.reserve(static_cast< ::std::size_t>(bulk_msg.model.dataset.respondents_choice_sets.size()));
	for (size_type i(0); i != bulk_msg.model.dataset.respondents_choice_sets.size(); ++i) 
		respondents_choice_sets.push_back(bulk_msg.model.dataset.respondents_choice_sets(i));

	::std::cout << "respondent" << separator << "choice_set" << separator << "alternative" << separator << "choice";
	for (size_type i(0); i != x_size; ++i)
		::std::cout << separator << "attribute" << i + 1;
	::std::cout << '\n';

	for (size_type i(0); i != respondents_choice_sets.size(); ++i) {
		for (size_type t(0); t != respondents_choice_sets[i]; ++t) {
			uint8_t const alternatives(*choice_sets_alternatives_ptr++);
			int8_t const choice(matrix_composite_ptr[alternatives * x_size]);
			for (uint8_t a(0); a != alternatives; ++a) {
				::std::cout << i + 1 << separator << t + 1 << separator << a + 1 << separator;
				if (a == choice)
					::std::cout << 1;
				else
					::std::cout << 0;
				for (size_type x(0); x != x_size; ++x)
					::std::cout << separator << static_cast<int>(matrix_composite_ptr[x * alternatives + a]);
				::std::cout << '\n';
			}
			matrix_composite_ptr += alternatives *  x_size + 1;
		}
	} 
}

template <typename IntRes>
void 
extract(netcpu::converger_1::message::res const & msg)
{
	if (msg.float_res() == netcpu::converger_1::message::float_res<float>::value)
		extract<IntRes, float>(); 
	else if (msg.float_res() == netcpu::converger_1::message::float_res<double>::value)
		extract<IntRes, double>();
	else // TODO -- may be as per 'on_read' -- write a bad message to client (more verbose)
		throw ::std::runtime_error(xxx("unsupported floating resolution: [") << msg.float_res() << ']');
}

void
main()
{
		::std::ifstream msg_file("res.msg", ::std::ios::binary);
		if (msg_file == false)
			throw ::std::runtime_error("missing res message during load");

		netcpu::message::read_wrapper msg_wire;
		netcpu::message::fstream_to_wrapper(msg_file, msg_wire);
		netcpu::converger_1::message::res msg;
		msg.from_wire(msg_wire);
		switch (msg.int_res()) {
			case netcpu::converger_1::message::int_res<uint32_t>::value :
			extract<uint32_t>(msg);
		break;
			case netcpu::converger_1::message::int_res<uint64_t>::value :
			extract<uint64_t>(msg);
		break;
			throw ::std::runtime_error(xxx("unsupported resolution(=") << msg.int_res() << ")");
		}
}

}}}

int 
main()
{
	::censoc::netcpu::gmnl_2::main();
	return 0;
}
