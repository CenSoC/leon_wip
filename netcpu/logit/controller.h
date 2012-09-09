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

#include <iostream>
#include <set>

#include <censoc/lexicast.h>
#include <censoc/spreadsheet/util.h>
#include <censoc/spreadsheet/csv.h>
#include <censoc/compare.h>
#include <censoc/stl_container_utils.h>
#include <netcpu/io_wrapper.h>
#include <netcpu/combos_builder.h>
#include <netcpu/converger_1/controller.h>
#include <netcpu/dataset_1/controller.h>

#include "message/meta.h"
#include "message/bulk.h"


#ifndef CENSOC_NETCPU_LOGIT_CONTROLLER_H
#define CENSOC_NETCPU_LOGIT_CONTROLLER_H


// for the time-being model_factory_interface will be automatically declared, no need to include explicitly...

namespace censoc { namespace netcpu { namespace logit { 

template <typename N, typename F>
struct model_traits {

	typedef netcpu::converger_1::message::meta<N, F, logit::message::meta<N> > meta_msg_type;
	typedef netcpu::converger_1::message::bulk<N, F, logit::message::bulk> bulk_msg_type;

	typedef typename netcpu::message::typepair<N>::wire size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	netcpu::dataset_1::composite_matrix_loader<N, F> dataset_loader;

	size_type
	coefficients_size() const
	{
		return dataset_loader.x_unique.size();
	}

	bool
	parse_arg(char const * const name, char const * const value, meta_msg_type & meta_msg, bulk_msg_type & bulk_msg) 
	{
		if (!::strcmp(name, "--reduce_exp_complexity")) {
			::std::string const reduce_exp_complexity_do_str(value);
			if (reduce_exp_complexity_do_str == "off") {
				meta_msg.model.reduce_exp_complexity(0);
			} else if (reduce_exp_complexity_do_str == "on") {
				meta_msg.model.reduce_exp_complexity(1);
			} else
				throw ::std::runtime_error("unknown reduce_exp_complexity value: [" + reduce_exp_complexity_do_str + ']');
			censoc::llog() << "Reduce exponential complexity during computation run: [" << reduce_exp_complexity_do_str << "]\n";
		} else 
			return dataset_loader.parse_arg(name, value, meta_msg.model.dataset, bulk_msg.model.dataset);
		return true;
	}

	void
	verify_args(meta_msg_type & meta_msg, bulk_msg_type & bulk_msg)
	{
		// calling it before, because 'respondents_choiceset_sets.size()' is relied upon by the following code (e.g. when setting 'draws_sets_size')
		dataset_loader.verify_args(meta_msg.model.dataset, bulk_msg.model.dataset);

		if (meta_msg.model.reduce_exp_complexity() == static_cast<typename netcpu::message::typepair<uint8_t>::wire>(-1))
			throw ::std::runtime_error("must supply --reduce_exp_complexity on or off");
	}
};

netcpu::converger_1::model_factory<logit::model_traits, netcpu::models_ids::logit> static factory;

}}}

#endif
