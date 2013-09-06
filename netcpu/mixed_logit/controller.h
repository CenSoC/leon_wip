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

// TODO -- if the model is going to be used past the test-only environment, then refactor the common elements with the gmnl_2 code

#include <censoc/exception.h>
#include <censoc/lexicast.h>
#include <netcpu/dataset_1/controller.h>

#include "message/meta.h"
#include "message/bulk.h"


#ifndef CENSOC_NETCPU_MIXED_LOGIT_CONTROLLER_H
#define CENSOC_NETCPU_MIXED_LOGIT_CONTROLLER_H


// for the time-being model_factory_interface will be automatically declared, no need to include explicitly...

namespace censoc { namespace netcpu { namespace mixed_logit { 

template <typename N, typename F>
struct model_traits {

	typedef mixed_logit::message::meta<N> meta_msg_type;
	typedef mixed_logit::message::bulk<N> bulk_msg_type;

	typedef typename netcpu::message::typepair<N>::wire size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	netcpu::dataset_1::composite_matrix_loader<N, F> dataset_loader;

	size_type
	coefficients_size() const
	{
		return dataset_loader.x_unique.size() * 2;
	}

	bool
	parse_arg(::std::string const & name, ::std::string const & value, meta_msg_type & meta_msg, bulk_msg_type & bulk_msg) 
	{
		if (name == "--draws_sets_size") {
			meta_msg.draws_sets_size(censoc::lexicast<size_type>(value));
			censoc::llog() << "Draws sets size: [" << meta_msg.draws_sets_size() << "]\n";
		} else if (name == "--reduce_exp_complexity") {
			if (value == "off") {
				meta_msg.reduce_exp_complexity(0);
			} else if (value == "on") {
				meta_msg.reduce_exp_complexity(1);
			} else
				throw censoc::exception::validation("unknown reduce_exp_complexity value: [" + value + ']');
			censoc::llog() << "Reduce exponential complexity during computation run: [" << value << "]\n";
		} else if (name == "--repeats")  {
			meta_msg.repetitions(censoc::lexicast<size_type>(value));
			censoc::llog() << "Repetitions in a simulation run: [" << meta_msg.repetitions() << "]\n";
		} else 
			return dataset_loader.parse_arg(name, value, meta_msg.dataset, bulk_msg.dataset);
		return true;
	}

	template <unsigned DataSetMsgId>
	void
	verify_args(meta_msg_type & meta_msg, bulk_msg_type & bulk_msg, netcpu::dataset_1::message::dataset_meta<DataSetMsgId> & dataset_meta_msg)
	{
		// calling it before, because 'respondents_choiceset_sets.size()' is relied upon by the following code (e.g. when setting 'draws_sets_size')
		dataset_loader.verify_args(meta_msg.dataset, bulk_msg.dataset, dataset_meta_msg);

		if (meta_msg.repetitions() == static_cast<typename netcpu::message::typepair<N>::wire>(-1))
			throw censoc::exception::validation("must supply number of repetitions in a simulation run");

		if (meta_msg.repetitions() % 2)
			throw censoc::exception::validation("the number of repetitions must be even");

		if (meta_msg.reduce_exp_complexity() == static_cast<typename netcpu::message::typepair<uint8_t>::wire>(-1))
			throw censoc::exception::validation("must supply --reduce_exp_complexity on or off");

		if (meta_msg.draws_sets_size() == static_cast<typename netcpu::message::typepair<N>::wire>(-1))
			throw censoc::exception::validation("must supply vaild draws sets size of > 0");
		else if (!meta_msg.draws_sets_size())
			meta_msg.draws_sets_size(bulk_msg.dataset.respondents_choice_sets.size());
		else
			meta_msg.draws_sets_size(::std::min(static_cast<size_type>(bulk_msg.dataset.respondents_choice_sets.size()), static_cast<size_type>(meta_msg.draws_sets_size())));

	}
};

}}}

#endif
