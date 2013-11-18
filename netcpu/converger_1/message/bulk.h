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

#ifndef CENSOC_NETCPU_CONVERGER_1_MESSAGE_BULK_H
#define CENSOC_NETCPU_CONVERGER_1_MESSAGE_BULK_H

namespace censoc { namespace netcpu { namespace converger_1 { namespace message {

template <typename N, typename F>
struct coeffwise_bulk {

	coeffwise_bulk()
	{
		netcpu::message::serialise_to_decomposed_floating<F>(-1, shrink_slowdown);
	}
	~coeffwise_bulk()
	{
	}

	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	typedef netcpu::message::array<typename netcpu::message::typepair<N>::wire> size_arraytype; 
	//typedef netcpu::message::scalar<F> float_type; 
	typedef netcpu::message::decomposed_floating<> float_type; 

#if 0
	float_type clamp_from;
	float_type clamp_to;
#endif
	float_type value_from;
	float_type value_to;
#if 0
	size_type boot_begin;
	size_type boot_end;
#endif
	float_type rand_range_end; // TODO -- possibly refactor later on -- bulk message when communicated to processing peer needs not have this variable (i.e. only used by server), but then again -- this 'bulk' communication only happens once per job per peer processor...
	float_type shrink_slowdown;
	size_arraytype grid_resolutions;

	// TODO -- make a more quality-assured design w.r.t. contructors, arrays and copy-constructors... (one of the simplest ways would be to have an array of pointer-to-type, not the actual types... although this will make acts of re-using the same message object to read multiple messages 'from_wire' slower as each of such readings will invoke a few dtor/ctors and new/delete calls)

	void
	print()
	{
		// a bit of debugging 
		censoc::llog() 
#if 0
			<< "clamp from: [" << netcpu::message::deserialise_from_decomposed_floating<F>(clamp_from) 
			<< "] clamp to: [" << netcpu::message::deserialise_from_decomposed_floating<F>(clamp_to) 
#endif
			<< "] value from: [" << netcpu::message::deserialise_from_decomposed_floating<F>(value_from) 
			<< "] value to: [" << netcpu::message::deserialise_from_decomposed_floating<F>(value_to) 
#if 0
			<< "] boot from: [" << boot_begin() 
			<< "] boot to: [" << boot_end() 
#endif
			<< "] rand_range_end: [" << netcpu::message::deserialise_from_decomposed_floating<F>(rand_range_end)
			<< "]shrink_slowdown: [" << netcpu::message::deserialise_from_decomposed_floating<F>(shrink_slowdown) 
			<< "] grid_resolutions: [";
		for (typename netcpu::message::typepair<N>::wire i(0); i != grid_resolutions.size(); ++i)
			censoc::llog() << '[' << grid_resolutions(i) << ']';
		censoc::llog() << ']';
	}
};

template <typename N, typename F>
struct coeffwise_bulk_x {

	coeffwise_bulk_x()
	{
	}
	~coeffwise_bulk_x()
	{
	}

	typedef netcpu::message::array<typename netcpu::message::typepair<N>::wire> size_arraytype; 
	//typedef netcpu::message::scalar<F> float_type; 
	typedef netcpu::message::decomposed_floating<> float_type; 

	float_type threshold;
	float_type shrink_slowdown;
	size_arraytype grid_resolutions;

	void
	print()
	{
		censoc::llog() 
			<< "threshold: [" << netcpu::message::deserialise_from_decomposed_floating<F>(threshold) 
			<< "shrink_slowdown: [" << netcpu::message::deserialise_from_decomposed_floating<F>(shrink_slowdown) 
			<< "] grid_resolutions: [";
		for (typename netcpu::message::typepair<N>::wire i(0); i != grid_resolutions.size(); ++i)
			censoc::llog() << '[' << grid_resolutions(i) << ']';
		censoc::llog() << ']';
	}
};

template <typename N, typename F>
struct zoomwise_bulk_x {

	netcpu::message::scalar<uint8_t> coeffs_at_once;

	// todo -- later just have "array of array" type of configuration...
	netcpu::message::array<message::coeffwise_bulk_x<N, F> > coeffs;

	void 
	print()
	{
		typedef typename netcpu::message::typepair<N>::ram size_type;
		for (size_type i(0); i != coeffs.size(); ++i) {
			censoc::llog() << "coeff_x metadata for coeff: [" << i << "] >> \n";
			coeffs(i).print();
			censoc::llog() << ::std::endl;
		}
	}
};


struct bulk_base : netcpu::message::message_base<converger_1::message::messages_ids::bulk> {};

template <typename N, typename F, typename Model>
struct bulk : bulk_base {

	typedef netcpu::message::scalar<typename netcpu::message::typepair<N>::wire> size_type;
	typedef netcpu::message::array<typename netcpu::message::typepair<N>::wire> size_arraytype; 
	typedef netcpu::message::array<F> float_arraytype; 

	netcpu::message::array<message::coeffwise_bulk<N, F> > coeffs;
	netcpu::message::array<message::zoomwise_bulk_x<N, F> > coeffs_x;
	Model model;
	// float_arraytype matrix_z;

	// hack indeed
	void 
	print()
	{
		typedef typename netcpu::message::typepair<N>::ram size_type;
		for (size_type i(0); i != coeffs.size(); ++i) {
			censoc::llog() << "coeff metadata for coeff: [" << i << "] > \n";
			coeffs(i).print();
			censoc::llog() << ::std::endl;
		}
		if (coeffs_x.size()) 
			for (size_type i(0); i != coeffs_x.size(); ++i) {
				censoc::llog() << "coeff_x metadata subset: [" << i << "] > \n";
				coeffs_x(i).print();
			}

		model.print();
	}
};


}}}}

#endif
