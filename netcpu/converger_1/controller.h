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

#include <Eigen/Core>

#include <censoc/lexicast.h>
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
template <typename N, typename F, typename Model>
struct task_loader_detail : netcpu::io_wrapper<netcpu::message::async_driver> {
	Model model;

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

	task_loader_detail(netcpu::message::async_driver & io_driver, int extended_float_res)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver) {

		censoc::llog() << "ctor in task_loader_detail\n";


		// parse here

		::std::pair<size_type, ::std::set<cli_coefficient_range_metadata> > coefficients_ranges;
		censoc::stl::fastsize< ::std::list< ::std::pair<size_type, ::std::set<cli_coefficient_range_metadata_x> > >, size_type> coefficients_ranges_x;

		assert(netcpu::model_metas.size());
		typedef netcpu::model_meta::args_type args_type;
		args_type & args(netcpu::model_metas.front().args);

		size_type use_first_cm_semantics(0);

		for (args_type::iterator i(args.begin()); i != args.end(); ++i) {
			if (i == --args.end())
				throw ::std::runtime_error(xxx() << "every argument must have a value: [" << *i << "]");
			else if (!::strcmp(*i, "--dissect")) {
				::std::string const dissect_do_str(*++i);
				if (dissect_do_str == "off") {
					meta_msg.dissect_do(0);
				} else if (dissect_do_str == "on") {
					meta_msg.dissect_do(1);
				} else
					throw ::std::runtime_error("unknown dissect value: [" + dissect_do_str + ']');
				censoc::llog() << "Dissect solution space @ the end of the run: [" << dissect_do_str << "]\n";

			} else if (!::strcmp(*i, "--complexity_size")) {
				meta_msg.complexity_size(censoc::lexicast<size_type>(*++i));
				censoc::llog() << "Maximum acceptable complexity size: [" << meta_msg.complexity_size() << "]\n";
			} else if (!::strcmp(*i, "--coeffs_atonce_size")) {
				size_type const coeffs_at_once_size(censoc::lexicast<size_type>(*++i));
				if (!coeffs_at_once_size)
					throw ::std::runtime_error("must supply vaild minimum --coeffs_atonce_size value of > 0 ");

				if (!++use_first_cm_semantics)
					use_first_cm_semantics = 2; // overly pedantic wraparound protection (TODO -- highly likely that it is not needed!)

				if (use_first_cm_semantics == 1)
					coefficients_ranges.first = coeffs_at_once_size;
				else {
					coefficients_ranges_x.push_back(::std::make_pair(coeffs_at_once_size, ::std::set<cli_coefficient_range_metadata_x>()));
					coefficients_ranges_x.back().first = coeffs_at_once_size;
				}
			} else if (!::strcmp(*i, "--cm")) {
				if (use_first_cm_semantics == 1) {

					char const static comment[] = "'coeff:"
#if 0
						"clamp_from:clamp_to:"
#endif
						"from:to:minstep:"
#if 0
						"bootfrom:bootto:"
#endif
						"grid_resolution[:grid_resolution:etc.]'";

					char const * rv(::strtok(const_cast<char *>(*++i), ":"));
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					size_type const cr_i = censoc::lexicast<size_type>(rv) - 1;

#if 0
					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					float_type const clamp_from = censoc::lexicast<float_type>(rv);

					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					float_type const clamp_to = censoc::lexicast<float_type>(rv);

					if (clamp_from >= clamp_to)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where 'clamp_from' is less than 'clamp_to'");
#endif

					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					float_type const from = censoc::lexicast<float_type>(rv);

					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					float_type const to = censoc::lexicast<float_type>(rv);

					if (from >= to)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where 'from' is less than 'to'");

					// minstep
					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					float_type const minstep = censoc::lexicast<float_type>(rv);
					if (minstep <= 0)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where 'minstep' > 0");

#if 0
					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					size_type const boot_from = censoc::lexicast<size_type>(rv);

					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					size_type const boot_to = censoc::lexicast<size_type>(rv);
#endif

					cli_coefficient_range_metadata tmp(cr_i, 
#if 0
							clamp_from, clamp_to, 
#endif
							from, to, 
#if 0
							boot_from, boot_to, 
#endif
							minstep);

					// grid_resolution(s)
					for (unsigned j(0); ; ++j) {
						rv = ::strtok(NULL, ":");
						if (rv == NULL)
							if (!j)
								throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
							else
								break;
						if (j == coefficients_ranges.first)
							throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << ", where the size of set of grid resolutions is not > coeffs_at_once value");

						size_type const grid_resolution = censoc::lexicast<size_type>(rv);

#if 0
						// boot values here are still all 1-based and inclusive
						if (!j && (boot_from > boot_to || boot_from >= grid_resolution || boot_from < 1 || boot_to > grid_resolution || boot_to < 1))
							throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where 'boot_from' < 'boot_to' and both are one-based, inclusive and within grid_resolution");
#endif

						if (grid_resolution < 2 || j && grid_resolution > tmp.grid_resolutions.front())
							throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where any 'grid_resolution' > 1 and any of the subsequent (not first) 'grid_resolution' is <= 1st 'grid_resolution'");

						tmp.grid_resolutions.push_back(grid_resolution);
					}

					if (coefficients_ranges.second.insert(tmp).second == false)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where 'coeff' is unique amongst other '--cm' options");
				} else { // subsequent (not first) --cm semantics

					char const static comment[] = "'coeff:threshold:grid_resolution[:grid_resolution:etc.]'";

					char const * rv(::strtok(const_cast<char *>(*++i), ":"));
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					size_type const cr_i = censoc::lexicast<size_type>(rv) - 1;

					rv = ::strtok(NULL, ":");
					if (rv == NULL)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
					float_type const threshold = censoc::lexicast<float_type>(rv);

					cli_coefficient_range_metadata_x tmp(cr_i, threshold);

					// grid_resolution(s)
					for (unsigned j(0); ; ++j) {
						if (j == coefficients_ranges_x.back().first)
							throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << ", where the size of set of grid resolutions is not > coeffs_at_once value");
						rv = ::strtok(NULL, ":");
						if (rv == NULL)
							if (!j)
								throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment);
							else 
								break;

						size_type const grid_resolution = censoc::lexicast<size_type>(rv);
						if (grid_resolution < 2 || tmp.grid_resolutions.empty() == false && grid_resolution > tmp.grid_resolutions.front())
							throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where any 'grid_resolution' > 1 and any of the subsequent (not first) 'grid_resolution' is <= 1st 'grid_resolution'");
						tmp.grid_resolutions.push_back(grid_resolution);
					}

					if (coefficients_ranges_x.back().second.insert(tmp).second == false)
						throw ::std::runtime_error(xxx() << "incorrect --cm value: [" << *i << "] need " << comment << " where 'coeff' is unique amongst other '--cm' options");

				}
			} else if (!::strcmp(*i, "--minimpr")) {
				netcpu::message::serialise_to_decomposed_floating<float_type>(censoc::lexicast<float_type>(*++i), meta_msg.improvement_ratio_min);
				censoc::llog() << "minimpr: [" << netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.improvement_ratio_min) << "]\n";
			} else if (!::strcmp(*i, "--shrink_slowdown")) {
				netcpu::message::serialise_to_decomposed_floating<float_type>(censoc::lexicast<float_type>(*++i), meta_msg.shrink_slowdown);
				censoc::llog() << "shrink_slowdown: [" << netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.shrink_slowdown) << "]\n";
			} else {
				char const * const a(*i);
				if (model.parse_arg(a, *++i, meta_msg, bulk_msg) == false) 
					throw ::std::runtime_error(xxx() << "unknown argument: [" << a << ']');
			}
		}

		if (coefficients_ranges.second.empty() == true)
			throw ::std::runtime_error("Must supply at least one --cm option");

		if (meta_msg.dissect_do() == static_cast<typename netcpu::message::typepair<uint8_t>::wire>(-1))
			throw ::std::runtime_error("must supply dissection @ the end of the run option (--dissect on or off).");

		if (!meta_msg.complexity_size())
			throw ::std::runtime_error("must supply vaild minimum --complexity_size value of > 0 (in fact, likely to need magnitude times more than no. of coefficients)");

		if (netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.improvement_ratio_min) <= 1)
			throw ::std::runtime_error("must supply vaild --minimpr value of greater than 1");

		if (netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.shrink_slowdown) < 0 || netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.shrink_slowdown) >= 1)
			throw ::std::runtime_error("must supply vaild --shrink_slowdown value: 0 <= x < 1");

		size_type const coefficients_size(model.coefficients_size());

		size_type const first_ci(coefficients_ranges.second.begin()->i);
		if (first_ci)
			throw ::std::runtime_error(xxx() << "validity-check failed. currently, the very first --cm option must start with coefficient 1");

		size_type const last_ci(coefficients_ranges.second.rbegin()->i);
		if (last_ci >= coefficients_size)
			throw ::std::runtime_error(xxx() << "validity-check failed. coefficients: [" << coefficients_size << "], but the --cm option(s) suggest(s) max coefficient as: [" << last_ci << "]");

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
				throw ::std::runtime_error(xxx() << "validity-check for gamma failed (must be between 0 and 1). gamma from: [" << k->from << "], gamma to: [" << k->to << "]");
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
		netcpu::combos_builder<N, netcpu::message::array<converger_1::message::coeffwise_bulk<N, F> > > combos_modem;
		combos_modem.build(coefficients_size, complexity_size, coeffs_at_once, bulk_msg.coeffs);
		if (combos_modem.metadata().empty() == true)
			throw ::std::runtime_error(xxx() << "insufficient --complexity size: [" << meta_msg.complexity_size() << "], at the initial zoom level... try doubling it perhaps?");
		else if (coeffs_at_once != coefficients_ranges.first)
			censoc::llog() << "WARNING: coeffs at once was reduced from: [" << coefficients_ranges.first << "], to: [" << coeffs_at_once << "]\n";
		censoc::llog() << "Resulting/final complexity at initial zoom level is: [" << complexity_size << "] and coeffs_atonce: [" << coeffs_at_once << "]\n";

		bulk_msg.coeffs_x.resize(coefficients_ranges_x.size());
		size_type coeffs_x_i(0);
		for (typename ::std::list< ::std::pair<size_type, ::std::set<cli_coefficient_range_metadata_x> > >::const_iterator coefficients_ranges_x_i(coefficients_ranges_x.begin()); coefficients_ranges_x_i != coefficients_ranges_x.end(); ++coefficients_ranges_x_i, ++coeffs_x_i) {
			assert(coeffs_x_i != coefficients_ranges_x.size());

			if (coefficients_ranges_x_i->second.empty() == true)
				throw ::std::runtime_error("Must supply at least one --cm option for any of the 'coeffs_at_once_size' options.");

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
			netcpu::combos_builder<N, netcpu::message::array<converger_1::message::coeffwise_bulk_x<N, F> > > combos_modem;
			combos_modem.build(coefficients_size, complexity_size, coeffs_at_once, bulk_msg.coeffs_x(coeffs_x_i).coeffs);
			if (combos_modem.metadata().empty() == true)
				throw ::std::runtime_error(xxx() << "insufficient --complexity size: [" << meta_msg.complexity_size() << "], at the next zoom level... try doubling it perhaps?");
			else if (coeffs_at_once != coefficients_ranges_x_i->first)
				censoc::llog() << "WARNING: coeffs at once was reduced from: [" << coefficients_ranges_x_i->first << "], to: [" << coeffs_at_once << "]\n";
			censoc::llog() << "Resulting/final complexity at next zoom level is: [" << complexity_size << "] and coeffs_atonce: [" << coeffs_at_once << "]\n";
		}
		assert(coeffs_x_i == coefficients_ranges_x.size());


#if 0
		Matrix3d md(1,2,3);
		Matrix3f mf = md.cast<float>();

		Note that casting to the same scalar type in an expression is free.
#endif

		model.verify_args(meta_msg, bulk_msg); // should throw

		censoc::llog() << "Parsing done... writing the res message..." << ::std::endl;
		converger_1::message::res res_msg;
		res_msg.int_res(converger_1::message::int_res<N>::value);
		res_msg.float_res(converger_1::message::float_res<F>::value);
		res_msg.extended_float_res(extended_float_res);
		res_msg.print();
		io().write(res_msg, &task_loader_detail::on_write_res, this);
	}

	void
	on_write_res()
	{
		censoc::llog() << "written the res message, now writing the meta message..." << ::std::endl;
		meta_msg.print();
		io().write(meta_msg, &task_loader_detail::on_write_meta, this);
	}

	void
	on_write_meta()
	{
		io().read(&task_loader_detail::on_read_meta_response, this);
	}

	void
	on_read_meta_response()
	{
		if (netcpu::message::good::myid == io().read_raw.id()) {
			censoc::llog() << "meta accepted -- will continue to send the bulk of the tasks..." << ::std::endl;
			bulk_msg.print();
			io().write(bulk_msg, &task_loader_detail::on_write_bulk, this);
		} else 
			delete this;
	}

	void
	on_write_bulk()
	{
		censoc::llog() << "bulk written..." << ::std::endl;
		io().read(&task_loader_detail::on_read_bulk_response, this);
	}
	void
	on_read_bulk_response()
	{
		if (netcpu::message::new_taskname::myid == io().read_raw.id()) {
			netcpu::message::new_taskname msg(io().read_raw);
			censoc::llog() << "NEW JOB ACCEPTED: ";
			msg.print();
		} else
			censoc::llog() << "wanted new_taskname message, but did not get one.\n";

		// TODO -- the previous code of calling "on_new_in_chain" is currently deprecated... (i.e. loading multiple jobs in one invocation)... just deleting this (dropping connection, etc.)
		delete this;
	}

	~task_loader_detail()
	{
		censoc::llog() << "task_loader_detail dtor: " << this << ::std::endl;
	}
};

template <template <typename, typename> class Model, netcpu::models_ids::val ModelId>
struct task_loader : netcpu::io_wrapper<netcpu::message::async_driver> {

	task_loader(netcpu::message::async_driver & io_driver)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver) {
		// TODO -- if it proves that this 'offer writing' procedure/sequence is commonly shared by other models, then move it into some base class 
		// write the offer
		netcpu::message::task_offer msg;
		msg.task_id(ModelId);
		io().write(msg, &task_loader::on_write_offer, this);

		censoc::llog() << "ctor in task_loader" << ::std::endl;
	}
	~task_loader() throw()
	{
		censoc::llog() << "dtor in task_loader" << ::std::endl;
	}
	void
	on_write_offer()
	{
		io().read(&task_loader::on_read_offer_response, this);
	}
	void
	on_read_offer_response()
	{
		censoc::llog() << "on_read_offer_response\n";
		if (netcpu::message::good::myid == io().read_raw.id()) {
			censoc::llog() << "on_read_offer_response -- is good\n";

			assert(netcpu::model_metas.size());
			netcpu::model_meta::args_type & args(netcpu::model_metas.front().args);
			int int_res(-1);
			int float_res(-1);
			int extended_float_res(-1);
			for (netcpu::model_meta::args_type::iterator i(args.begin()); i != args.end(); ++i) {
				if (!::strcmp(*i, "--int_resolution")) {
					netcpu::model_meta::args_type::iterator j(i++);
					args.erase(j);
					if (!::strcmp(*i, "32")) {
						args.erase(i);
						int_res = converger_1::message::int_res<uint32_t>::value;
					} else if (!::strcmp(*i, "64")) {
						args.erase(i);
						int_res = converger_1::message::int_res<uint64_t>::value;
					} else
						throw ::std::runtime_error(netcpu::xxx("Can't have --int_resolution with this option:[") << *i << ']');
				}
				if (!::strcmp(*i, "--float_resolution")) {
					netcpu::model_meta::args_type::iterator j(i++);
					args.erase(j);
					if (!::strcmp(*i, "single")) {
						args.erase(i);
						float_res = converger_1::message::float_res<float>::value;
					} else if (!::strcmp(*i, "double")) {
						args.erase(i);
						float_res = converger_1::message::float_res<double>::value;
					} else
						throw ::std::runtime_error(netcpu::xxx("Can't have --float_resolution with this option:[") << *i << ']');
				}
				if (!::strcmp(*i, "--extended_float_resolution")) {
					netcpu::model_meta::args_type::iterator j(i++);
					args.erase(j);
					if (!::strcmp(*i, "single")) {
						args.erase(i);
						extended_float_res = converger_1::message::float_res<float>::value;
					} else if (!::strcmp(*i, "double")) {
						args.erase(i);
						extended_float_res = converger_1::message::float_res<double>::value;
					} else
						throw ::std::runtime_error(netcpu::xxx("Can't have --extended_float_resolution with this option:[") << *i << ']');
				}
			}

			if (extended_float_res == -1)
				throw ::std::runtime_error("--extended_float_resolution option must be specified");

			if (int_res == converger_1::message::int_res<uint32_t>::value)
				new_task_loader_detail<uint32_t>(float_res, extended_float_res);
			else if (int_res == converger_1::message::int_res<uint64_t>::value)
				new_task_loader_detail<uint64_t>(float_res, extended_float_res);
			else
				throw ::std::runtime_error("--int_resolution option must be specified");

		} else {
			censoc::llog() << "on_read_offer_response has read message that is not 'good', id: [" << io().read_raw.id() << "] -- deleting 'this'\n";
			delete this;
		}
	}
	template <typename IntRes>
	void 
	new_task_loader_detail(int float_res, int extended_float_res)
	{
		if (float_res == converger_1::message::float_res<float>::value)
			new task_loader_detail<IntRes, float, Model<IntRes, float> >(io(), extended_float_res);
		else if (float_res == converger_1::message::float_res<double>::value)
			new task_loader_detail<IntRes, double, Model<IntRes, double> >(io(), extended_float_res);
		else
			throw ::std::runtime_error("--float_resolution option must be specified");
	}
};

// this must be a very very small class (it will exist through the whole of the lifespan of the server)
template <template <typename, typename> class Model, netcpu::models_ids::val ModelId>
struct model_factory : netcpu::model_factory_base<ModelId> {
	void 
	new_task(netcpu::message::async_driver & io_driver)
	{
		assert(netcpu::model_metas.size());

		netcpu::model_meta const & current(netcpu::model_metas.front());

		if (current.args.empty() == true)
			throw ::std::runtime_error("Must supply some model-specific arguments: none are present.");

		censoc::llog() << "model factory is instantiating a task" << ::std::endl;

		if (current.what_to_do == netcpu::model_meta::task_offer)
			new task_loader<Model, ModelId>(io_driver);
		else
			throw ::std::runtime_error("Currently the only supported action is to load new computations.");
	}
};

}}}

#endif
