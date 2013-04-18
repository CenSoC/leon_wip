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

#include <iostream>
#include <set>

#include <boost/tokenizer.hpp>

#include <censoc/lexicast.h>
#include <censoc/sha_file.h>

#include <netcpu/io_wrapper.h>
#include <netcpu/combos_builder.h>

#include "types.h"
#include "message/res.h"
#include "message/meta.h"
#include "message/bulk.h"

#ifndef CENSOC_NETCPU_CONVERGER_1_CONTROLLER_H
#define CENSOC_NETCPU_CONVERGER_1_CONTROLLER_H


// for the time-being model_factory_interface will be automatically declared, no need to include explicitly...

namespace censoc { namespace netcpu { namespace converger_1 { 

	/** @note N and F semantics explanation ... 
	such stipulate high-level processing/calculation resolutions/domain. Then each of the classes (e.g. network-related message, task_loader_detail, some processor-related ram structs) are free to typedef the exact float_type type to use for actual calculations. Even though this may seem somewhat convoluted for RAM-only structs (or by the same token wire-only structs) it allows for a more uniform design-policy).
		*/
template <typename AsyncDriver, typename N, typename F, typename Model>
struct task_loader_detail : netcpu::io_wrapper<AsyncDriver> {

	typedef netcpu::io_wrapper<AsyncDriver> base_type;

	Model model;

	::boost::shared_ptr<netcpu::model_meta> mm;

	// todo -- temp hack for the time-being...
	::std::string feedback_string;

	/* 
  mostly referencing directly (not used much for anything else but IO)
	so using messages directly and using 'wire' types from typepairs mostly...
	*/

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef typename netcpu::message::typepair<N>::wire size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef censoc::lexicast< ::std::string> xxx;

	typename Model::meta_msg_type meta_msg;
	typename Model::bulk_msg_type bulk_msg;

	struct cli_coefficient_range_metadata {
		cli_coefficient_range_metadata(size_paramtype i, 
#if 0
				float_paramtype clamp_from, float_paramtype clamp_to, 
#endif
				float_paramtype from, float_paramtype to, 
#if 0
				float_paramtype boot_from, float_paramtype boot_to, 
#endif
				float_paramtype minstep)
		: i(i), 
#if 0
		clamp_from(clamp_from), clamp_to(clamp_to), 
#endif
		from(from), to(to), 
#if 0
		boot_from(boot_from), boot_to(boot_to), 
#endif
		minstep(minstep) {
		}
		size_type const i;
#if 0
		float_type const clamp_from;
		float_type const clamp_to;
#endif
		float_type const from;
		float_type const to;
#if 0
		float_type const boot_from; // 1-based, inclusive
		float_type const boot_to; // 1-based, inclusive
#endif
		float_type const minstep;
		::std::list<size_type> grid_resolutions;
		bool 
		operator < (cli_coefficient_range_metadata const & rhs) const
		{
			return i < rhs.i;
		}
	};

	struct cli_coefficient_range_metadata_x { // extra ones -- for additional zoom levels
		cli_coefficient_range_metadata_x(size_paramtype i, float_paramtype threshold)
		: i(i), threshold(threshold) {
		}
		size_type const i;
		float_type const threshold;
		censoc::stl::fastsize< ::std::list<size_type>, size_type> grid_resolutions;
		bool 
		operator < (cli_coefficient_range_metadata_x const & rhs) const
		{
			return i < rhs.i;
		}
	};

	task_loader_detail(AsyncDriver & io_driver, int extended_float_res, int approximate_exponents, ::boost::shared_ptr<netcpu::model_meta> mm)
	: netcpu::io_wrapper<AsyncDriver>(io_driver), mm(mm) {

		censoc::llog() << "ctor in task_loader_detail\n";


		// parse here

		::std::pair<size_type, ::std::set<cli_coefficient_range_metadata> > coefficients_ranges;
		censoc::stl::fastsize< ::std::list< ::std::pair<size_type, ::std::set<cli_coefficient_range_metadata_x> > >, size_type> coefficients_ranges_x;

		typedef netcpu::model_meta::args_type args_type;
		args_type & args(mm->args);

		size_type use_first_cm_semantics(0);

		for (args_type::iterator i(args.begin()); i != args.end(); ++i) {
			if (i->first == "--dissect") {
				if (i->second == "off") {
					meta_msg.dissect_do(0);
				} else if (i->second == "on") {
					meta_msg.dissect_do(1);
				} else
					throw netcpu::message::exception("unknown dissect value: [" + i->second + ']');
				censoc::llog() << "Dissect solution space @ the end of the run: [" << i->second << "]\n";
			} else if (i->first == "--complexity_size") {
				meta_msg.complexity_size(censoc::lexicast<size_type>(i->second));
				censoc::llog() << "Maximum acceptable complexity size: [" << meta_msg.complexity_size() << "]\n";
			} else if (i->first == "--coeffs_atonce_size") {
				size_type const coeffs_at_once_size(censoc::lexicast<size_type>(i->second));
				if (!coeffs_at_once_size)
					throw netcpu::message::exception("must supply vaild minimum --coeffs_atonce_size value of > 0 ");

				if (!++use_first_cm_semantics)
					use_first_cm_semantics = 2; // overly pedantic wraparound protection (TODO -- highly likely that it is not needed!)

				if (use_first_cm_semantics == 1)
					coefficients_ranges.first = coeffs_at_once_size;
				else
					coefficients_ranges_x.push_back(::std::make_pair(coeffs_at_once_size, ::std::set<cli_coefficient_range_metadata_x>()));
			} else if (i->first == "--cm") {

				typedef ::boost::tokenizer< ::boost::char_separator<char> > tokenizer_type;
				tokenizer_type tokens(i->second, ::boost::char_separator<char>(":"));
				tokenizer_type::const_iterator token_i(tokens.begin()); 
					
				if (use_first_cm_semantics == 1) {

					char const static comment[] = "'coeff:"
						"from:to:minstep:"
						"grid_resolution[:grid_resolution:etc.]'";

					if (token_i == tokens.end())
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
					size_type const cr_i = censoc::lexicast<size_type>(*token_i) - 1;
					
					if (++token_i == tokens.end())
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
					float_type const from = censoc::lexicast<float_type>(*token_i);

					if (++token_i == tokens.end())
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
					float_type const to = censoc::lexicast<float_type>(*token_i);

					if (from >= to)
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << " where 'from' is less than 'to'");

					// minstep
					if (++token_i == tokens.end())
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
					float_type const minstep = censoc::lexicast<float_type>(*token_i);
					if (minstep <= 0)
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << " where 'minstep' > 0");

					cli_coefficient_range_metadata tmp(cr_i, 
							from, to, 
							minstep);

					// grid_resolution(s)
					for (unsigned j(0); ; ++j) {
						if (++token_i == tokens.end())
							if (!j)
								throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
							else
								break;
						if (j == coefficients_ranges.first)
							throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << ", where the size of set of grid resolutions is not > coeffs_at_once value");

						size_type const grid_resolution = censoc::lexicast<size_type>(*token_i);

						if (grid_resolution < 2 || j && grid_resolution > tmp.grid_resolutions.front())
							throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << " where any 'grid_resolution' > 1 and any of the subsequent (not first) 'grid_resolution' is <= 1st 'grid_resolution'");

						tmp.grid_resolutions.push_back(grid_resolution);
					}

					if (coefficients_ranges.second.insert(tmp).second == false)
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << " where 'coeff' is unique amongst other '--cm' options");
				} else { // subsequent (not first) --cm semantics

					char const static comment[] = "'coeff:threshold:grid_resolution[:grid_resolution:etc.]'";

					if (token_i == tokens.end())
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
					size_type const cr_i = censoc::lexicast<size_type>(*token_i) - 1;

					if (++token_i == tokens.end())
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
					float_type const threshold = censoc::lexicast<float_type>(*token_i);

					cli_coefficient_range_metadata_x tmp(cr_i, threshold);

					// grid_resolution(s)
					for (unsigned j(0); ; ++j) {
						if (++token_i == tokens.end())
							if (!j)
								throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment);
							else 
								break;
						if (j == coefficients_ranges_x.back().first)
							throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << ", where the size of set of grid resolutions is not > coeffs_at_once value");

						size_type const grid_resolution = censoc::lexicast<size_type>(*token_i);
						if (grid_resolution < 2 || tmp.grid_resolutions.empty() == false && grid_resolution > tmp.grid_resolutions.front())
							throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << " where any 'grid_resolution' > 1 and any of the subsequent (not first) 'grid_resolution' is <= 1st 'grid_resolution'");
						tmp.grid_resolutions.push_back(grid_resolution);
					}

					if (coefficients_ranges_x.back().second.insert(tmp).second == false)
						throw netcpu::message::exception(xxx() << "incorrect --cm value: [" << i->second << "] need " << comment << " where 'coeff' is unique amongst other '--cm' options");

				}
			} else if (i->first == "--minimpr") {
				netcpu::message::serialise_to_decomposed_floating<float_type>(censoc::lexicast<float_type>(i->second), meta_msg.improvement_ratio_min);
				censoc::llog() << "minimpr: [" << netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.improvement_ratio_min) << "]\n";
			} else if (i->first == "--shrink_slowdown") {
				netcpu::message::serialise_to_decomposed_floating<float_type>(censoc::lexicast<float_type>(i->second), meta_msg.shrink_slowdown);
				censoc::llog() << "shrink_slowdown: [" << netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.shrink_slowdown) << "]\n";
			} else {
				if (model.parse_arg(i->first, i->second, meta_msg, bulk_msg) == false) 
					throw netcpu::message::exception(xxx() << "unknown argument: [" << i->first << ']');
			}
		}

		if (coefficients_ranges.second.empty() == true)
			throw netcpu::message::exception("Must supply at least one --cm option");

		if (meta_msg.dissect_do() == static_cast<typename netcpu::message::typepair<uint8_t>::wire>(-1))
			throw netcpu::message::exception("must supply dissection @ the end of the run option (--dissect on or off).");

		if (!meta_msg.complexity_size())
			throw netcpu::message::exception("must supply vaild minimum --complexity_size value of > 0 (in fact, likely to need magnitude times more than no. of coefficients)");

		if (netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.improvement_ratio_min) < 1)
			throw netcpu::message::exception("must supply vaild --minimpr value of no less than 1");

		if (netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.shrink_slowdown) < 0 || netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.shrink_slowdown) >= 1)
			throw netcpu::message::exception("must supply vaild --shrink_slowdown value: 0 <= x < 1");

		size_type const coefficients_size(model.coefficients_size());

		size_type const first_ci(coefficients_ranges.second.begin()->i);
		if (first_ci)
			throw netcpu::message::exception(xxx() << "validity-check failed. currently, the very first --cm option must start with coefficient 1");

		size_type const last_ci(coefficients_ranges.second.rbegin()->i);
		if (last_ci >= coefficients_size)
			throw netcpu::message::exception(xxx() << "validity-check failed. coefficients: [" << coefficients_size << "], but the --cm option(s) suggest(s) max coefficient as: [" << last_ci << "]");

		bulk_msg.coeffs.resize(coefficients_size);

		//censoc::llog() << "Coefficients' metadata:";
		typename ::std::set<cli_coefficient_range_metadata>::const_iterator j(coefficients_ranges.second.begin());
		typename ::std::set<cli_coefficient_range_metadata>::const_iterator k;
		for (size_type i(0); i != coefficients_size; ++i) {
			if (j != coefficients_ranges.second.end() && j->i == i) 
				k = j++;

#if 0
			// this is for the deprecated (but to be possibly returned) full gmnl model
			if (i == coefficients_size - 2 && (k->from < 0 || k->from > 1))
				throw netcpu::message::exception(xxx() << "validity-check for gamma failed (must be between 0 and 1). gamma from: [" << k->from << "], gamma to: [" << k->to << "]");
#endif

#if 0
			netcpu::message::serialise_to_decomposed_floating(k->clamp_from, bulk_msg.coeffs(i).clamp_from);
			netcpu::message::serialise_to_decomposed_floating(k->clamp_to, bulk_msg.coeffs(i).clamp_to);
#endif
			netcpu::message::serialise_to_decomposed_floating(k->from, bulk_msg.coeffs(i).value_from);
			netcpu::message::serialise_to_decomposed_floating(k->to, bulk_msg.coeffs(i).value_to);
#if 0
			bulk_msg.coeffs(i).boot_begin(k->boot_from - 1); // one-based to zero-based
			bulk_msg.coeffs(i).boot_end(k->boot_to); // source is one-based, but is inclusize -- dest. is zero-based but exclusive, so no offsetting is needed
#endif

			bulk_msg.coeffs(i).grid_resolutions.resize(coefficients_ranges.first);

			typename ::std::list<size_type>::const_iterator grid_resolution_i_tmp(k->grid_resolutions.begin());
			typename ::std::list<size_type>::const_iterator grid_resolution_i;
			for (size_type l(0); l != coefficients_ranges.first; ++l) {
				if (grid_resolution_i_tmp != k->grid_resolutions.end()) 
					grid_resolution_i = grid_resolution_i_tmp++;
				bulk_msg.coeffs(i).grid_resolutions(l) = *grid_resolution_i;
			}


			netcpu::message::serialise_to_decomposed_floating(k->minstep, bulk_msg.coeffs(i).rand_range_end);

		}

		size_type complexity_size(meta_msg.complexity_size());
		size_type coeffs_at_once(coefficients_ranges.first);
		if (coefficients_size < coeffs_at_once)
			throw netcpu::message::exception(xxx() << "incompatible --coeffs_at_once : [" << coeffs_at_once << "], and coefficient_size :[" << coefficients_size << "]. You cannot have more coeffs-at-once than there are coefficients in the model! That's common sense really...");
		netcpu::combos_builder<N, netcpu::message::array<converger_1::message::coeffwise_bulk<N, F> > > combos_modem;
		combos_modem.build(coefficients_size, complexity_size, coeffs_at_once, bulk_msg.coeffs);
		if (combos_modem.metadata().empty() == true)
			throw netcpu::message::exception(xxx() << "insufficient --complexity size: [" << meta_msg.complexity_size() << "], at the initial zoom level... try doubling it perhaps?");
		else if (coeffs_at_once != coefficients_ranges.first)
			throw netcpu::message::exception(xxx() << "coeffs_atonce vs. complexity_size conflict... coeffs at once would have been reduced from: [" << coefficients_ranges.first << "] to: [" << coeffs_at_once << ']');
		censoc::llog() << (feedback_string += xxx() << "Resulting/final complexity at initial zoom level is: [" << complexity_size << "] and coeffs_atonce: [" << coeffs_at_once << ']') << '\n';
		feedback_string += "\\n";

		bulk_msg.coeffs_x.resize(coefficients_ranges_x.size());
		size_type coeffs_x_i(0);
		for (typename ::std::list< ::std::pair<size_type, ::std::set<cli_coefficient_range_metadata_x> > >::const_iterator coefficients_ranges_x_i(coefficients_ranges_x.begin()); coefficients_ranges_x_i != coefficients_ranges_x.end(); ++coefficients_ranges_x_i, ++coeffs_x_i) {
			assert(coeffs_x_i != coefficients_ranges_x.size());

			if (coefficients_ranges_x_i->second.empty() == true)
				throw netcpu::message::exception("Must supply at least one --cm option for any of the 'coeffs_at_once_size' options.");

			bulk_msg.coeffs_x(coeffs_x_i).coeffs.resize(coefficients_size);

			typename ::std::set<cli_coefficient_range_metadata_x>::const_iterator j(coefficients_ranges_x_i->second.begin());
			assert(j != coefficients_ranges_x_i->second.end());
			typename ::std::set<cli_coefficient_range_metadata_x>::const_iterator k;
			for (size_type i(0); i != coefficients_size; ++i) {
				if (j != coefficients_ranges_x_i->second.end() && j->i == i) 
					k = j++;

				netcpu::message::serialise_to_decomposed_floating(k->threshold, bulk_msg.coeffs_x(coeffs_x_i).coeffs(i).threshold);
				bulk_msg.coeffs_x(coeffs_x_i).coeffs_at_once(coefficients_ranges_x_i->first);

				size_type const grid_resolutions_size(::std::max(coefficients_ranges_x_i->first, k->grid_resolutions.size()));
				bulk_msg.coeffs_x(coeffs_x_i).coeffs(i).grid_resolutions.resize(grid_resolutions_size);

				typename ::std::list<size_type>::const_iterator grid_resolution_i_tmp(k->grid_resolutions.begin());
				typename ::std::list<size_type>::const_iterator grid_resolution_i;
				for (size_type l(0); l != grid_resolutions_size; ++l) {
					if (grid_resolution_i_tmp != k->grid_resolutions.end()) 
						grid_resolution_i = grid_resolution_i_tmp++;
					bulk_msg.coeffs_x(coeffs_x_i).coeffs(i).grid_resolutions(l) = *grid_resolution_i;
				}
			}
			size_type complexity_size(meta_msg.complexity_size());
			size_type coeffs_at_once(coefficients_ranges_x_i->first);
			if (coefficients_size < coeffs_at_once)
				throw netcpu::message::exception(xxx() << "In some of the subsequent complexity specifications there are incompatible --coeffs_at_once : [" << coeffs_at_once << "], and coefficient_size :[" << coefficients_size << "]. You cannot have more coeffs-at-once than there are coefficients in the model! That's common sense really...");
			netcpu::combos_builder<N, netcpu::message::array<converger_1::message::coeffwise_bulk_x<N, F> > > combos_modem;
			combos_modem.build(coefficients_size, complexity_size, coeffs_at_once, bulk_msg.coeffs_x(coeffs_x_i).coeffs);
			if (combos_modem.metadata().empty() == true)
				throw netcpu::message::exception(xxx() << "insufficient --complexity size: [" << meta_msg.complexity_size() << "], at the next zoom level... try doubling it perhaps?");
			else if (coeffs_at_once != coefficients_ranges_x_i->first)
				throw netcpu::message::exception(xxx() << "coeffs_atonce vs. complexity_size conflict... coeffs at once would have been reduced from: [" << coefficients_ranges_x_i->first << "], to: [" << coeffs_at_once << ']');
			censoc::llog() << (feedback_string += xxx() << "Resulting/final complexity at next zoom level is: [" << complexity_size << "] and coeffs_atonce: [" << coeffs_at_once << ']') << '\n';
			feedback_string += "\\n";
		}
		assert(coeffs_x_i == coefficients_ranges_x.size());


#if 0
		Matrix3d md(1,2,3);
		Matrix3f mf = md.cast<float>();

		Note that casting to the same scalar type in an expression is free.
#endif

		model.verify_args(meta_msg, bulk_msg); // should throw

		{ // store sha hush of bulk message in meta message...
			netcpu::message::write_wrapper write_raw; 
			bulk_msg.to_wire(write_raw);
			censoc::sha_buf<size_type> sha;
			sha.calculate(write_raw.head(), write_raw.size());
			meta_msg.bulk_msg_hush.resize(sha.hushlen);
			::memcpy(meta_msg.bulk_msg_hush.data(), sha.get(), sha.hushlen);
		}

		censoc::llog() << "Parsing done... writing the res message..." << ::std::endl;
		converger_1::message::res res_msg;
		res_msg.int_res(converger_1::message::int_res<N>::value);
		res_msg.float_res(converger_1::message::float_res<F>::value);
		res_msg.extended_float_res(extended_float_res);
		res_msg.approximate_exponents(approximate_exponents);
		res_msg.print();
		base_type::io().AsyncDriver::native_protocol::write(res_msg, &task_loader_detail::on_write_res, this);
	}

	void
	on_write_res()
	{
		censoc::llog() << "written the res message, now writing the meta message..." << ::std::endl;
		meta_msg.print();
		base_type::io().AsyncDriver::native_protocol::write(meta_msg, &task_loader_detail::on_write_meta, this);
	}

	void
	on_write_meta()
	{
		base_type::io().AsyncDriver::native_protocol::read(&task_loader_detail::on_read_meta_response, this);
	}

	void
	on_read_meta_response()
	{
		if (netcpu::message::good::myid == base_type::io().AsyncDriver::native_protocol::read_raw.id()) {
			censoc::llog() << "meta accepted -- will continue to send the bulk of the tasks..." << ::std::endl;
			bulk_msg.print();
			base_type::io().AsyncDriver::native_protocol::write(bulk_msg, &task_loader_detail::on_write_bulk, this);
		} else 
			delete this;
	}

	void
	on_write_bulk()
	{
		censoc::llog() << "bulk written..." << ::std::endl;
		base_type::io().AsyncDriver::native_protocol::read(&task_loader_detail::on_read_bulk_response, this);
	}
	void
	on_read_bulk_response()
	{
		if (netcpu::message::new_taskname::myid == base_type::io().AsyncDriver::native_protocol::read_raw.id()) {
			netcpu::message::new_taskname msg(base_type::io().AsyncDriver::native_protocol::read_raw);
			censoc::llog() << "NEW JOB ACCEPTED: ";
			msg.print();
			// TODO -- the code of calling "on_new_in_chain" is currently untested... (i.e. loading multiple jobs in one invocation)... 

			// TODO -- move this whole 'json' thingy to http codebase!!! remains here as a quick hack to be refactored later when theres some time to do so...
			(new netcpu::peer_connection(base_type::io()))->on_new_in_chain("{\"echo\":\"" + feedback_string + "New job accepted(=" + ::std::string(msg.name.data(), msg.name.size()) + ")\"}");
		} else
			censoc::llog() << "wanted new_taskname message, but did not get one.\n";
	}

	~task_loader_detail()
	{
		censoc::llog() << "task_loader_detail dtor: " << this << ::std::endl;
	}
};

template <typename AsyncDriver, template <typename, typename> class Model, netcpu::models_ids::val ModelId>
struct task_loader : netcpu::io_wrapper<AsyncDriver> {

	typedef netcpu::io_wrapper<AsyncDriver> base_type;

	::boost::shared_ptr<netcpu::model_meta> mm;

	task_loader(AsyncDriver & io_driver, ::boost::shared_ptr<netcpu::model_meta> mm)
	: netcpu::io_wrapper<AsyncDriver>(io_driver), mm(mm) {
		// TODO -- if it proves that this 'offer writing' procedure/sequence is commonly shared by other models, then move it into some base class 
		// write the offer
		netcpu::message::task_offer msg;
		msg.task_id(ModelId);
		base_type::io().AsyncDriver::native_protocol::write(msg, &task_loader::on_write_offer, this);

		censoc::llog() << "ctor in task_loader" << ::std::endl;
	}
	~task_loader() throw()
	{
		censoc::llog() << "dtor in task_loader" << ::std::endl;
	}
	void
	on_write_offer()
	{
		base_type::io().AsyncDriver::native_protocol::read(&task_loader::on_read_offer_response, this);
	}
	void
	on_read_offer_response()
	{
		censoc::llog() << "on_read_offer_response\n";
		if (netcpu::message::good::myid == base_type::io().AsyncDriver::native_protocol::read_raw.id()) {
			censoc::llog() << "on_read_offer_response -- is good\n";

			netcpu::model_meta::args_type & args(mm->args);
			int int_res(-1);
			int float_res(-1);
			int extended_float_res(-1);
			int approximate_exponents(-1);
			for (netcpu::model_meta::args_type::iterator i(args.begin()); i != args.end();) {
				if (i->first == "--int_resolution") {
					if (i->second == "32")
						int_res = converger_1::message::int_res<uint32_t>::value;
					else if (i->second == "64")
						int_res = converger_1::message::int_res<uint64_t>::value;
					else
						throw netcpu::message::exception(netcpu::xxx("Can't have --int_resolution with this option:[") << i->second << ']');
					args.erase(i++);
				} else if (i->first == "--float_resolution") {
					if (i->second == "single")
						float_res = converger_1::message::float_res<float>::value;
					else if (i->second == "double")
						float_res = converger_1::message::float_res<double>::value;
					else
						throw netcpu::message::exception(netcpu::xxx("Can't have --float_resolution with this option:[") << i->second << ']');
					args.erase(i++);
				} else if (i->first == "--extended_float_resolution") {
					if (i->second == "single")
						extended_float_res = converger_1::message::float_res<float>::value;
					else if (i->second == "double")
						extended_float_res = converger_1::message::float_res<double>::value;
					else
						throw netcpu::message::exception(netcpu::xxx("Can't have --extended_float_resolution with this option:[") << i->second << ']');
					args.erase(i++);
				} else if (i->first == "--approximate_exponents") {
					if (i->second == "true")
						approximate_exponents = 1;
					else if (i->second == "false")
						approximate_exponents = 0;
					else
						throw netcpu::message::exception(netcpu::xxx("Can't have --approximate_exponents with this option:[") << i->second << ']');
					args.erase(i++);
				} else 
					++i;
			}

			if (extended_float_res == -1)
				throw netcpu::message::exception("--extended_float_resolution option must be specified");

			if (approximate_exponents == -1)
				throw netcpu::message::exception("--approximate_exponents option must be specified");

			if (int_res == converger_1::message::int_res<uint32_t>::value)
				new_task_loader_detail<uint32_t>(float_res, extended_float_res, approximate_exponents);
			else if (int_res == converger_1::message::int_res<uint64_t>::value)
				new_task_loader_detail<uint64_t>(float_res, extended_float_res, approximate_exponents);
			else
				throw netcpu::message::exception("--int_resolution option must be specified");

		} else {
			censoc::llog() << "on_read_offer_response has read message that is not 'good', id: [" << base_type::io().AsyncDriver::native_protocol::read_raw.id() << "] -- deleting 'this'\n";
			delete this;
		}
	}
	template <typename IntRes>
	void 
	new_task_loader_detail(int float_res, int extended_float_res, int approximate_exponents)
	{
		if (float_res == converger_1::message::float_res<float>::value)
			new task_loader_detail<AsyncDriver, IntRes, float, Model<IntRes, float> >(base_type::io(), extended_float_res, approximate_exponents, mm);
		else if (float_res == converger_1::message::float_res<double>::value)
			new task_loader_detail<AsyncDriver, IntRes, double, Model<IntRes, double> >(base_type::io(), extended_float_res, approximate_exponents, mm);
		else
			throw netcpu::message::exception("--float_resolution option must be specified");
	}
};

// this must be a very very small class (it will exist through the whole of the lifespan of the server)
template <typename AsyncDriver, template <typename, typename> class Model, netcpu::models_ids::val ModelId>
struct model_factory : netcpu::model_factory_base<ModelId> {
	void 
	new_task(AsyncDriver & io_driver, ::boost::shared_ptr<netcpu::model_meta> mm) 
	{
		if (mm->args.empty() == true)
			throw netcpu::message::exception("Must supply some model-specific arguments: none are present.");

		censoc::llog() << "model factory is instantiating a task" << ::std::endl;

		if (mm->what_to_do == netcpu::model_meta::task_offer)
			new task_loader<AsyncDriver, Model, ModelId>(io_driver, mm);
		else
			throw netcpu::message::exception("Currently the only supported action is to load new computations.");
	}
};

}}}

#endif
