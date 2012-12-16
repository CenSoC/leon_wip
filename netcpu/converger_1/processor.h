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

#include <set>
#include <fstream>

#include <boost/type_traits.hpp>
// #include <boost/aligned_storage.hpp>

#include <Eigen/Core>

#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/rand.h>
#include <censoc/halton.h>
#include <censoc/rnd_mirror_grid.h>
#include <censoc/finite_math.h>
#include <censoc/sysinfo.h>
#include <censoc/stl_container_utils.h>
#include <censoc/sha_file.h>

#include <netcpu/io_wrapper.h>
#include <netcpu/fstream_to_wrapper.h>

#include <netcpu/combos_builder.h>
#include <netcpu/range_utils.h>

#include "types.h"
#include "coefficient_metadata_base.h"
#include "message/res.h"
#include "message/meta.h"
#include "message/bulk.h"
#include "message/peer_report.h"
#include "message/server_state_sync.h"

//}
#ifndef CENSOC_NETCPU_CONVERGER_1_PROCESSOR_H
#define CENSOC_NETCPU_CONVERGER_1_PROCESSOR_H

namespace censoc { namespace netcpu { namespace converger_1 { 

template <typename float_type>
class opaque_result {
	float_type value_;
#ifdef __WIN32__
	bool processing_incomplete_;
#endif

public:
	opaque_result()
	: value_(censoc::largish<float_type>()) 
#ifdef __WIN32__
	, processing_incomplete_(false) 
#endif
	{
	}

	typename censoc::strip_const<typename censoc::param<float_type>::type>::type 
	value() const
	{
		return value_;
	}

	void
	value(typename censoc::param<float_type>::type x) 
	{
		value_ = x;
	}

	void
	invalidate_value()
	{
		value_ = censoc::largish<float_type>();
	}

#ifndef NDEBUG
	bool
	is_valid() const
	{
		return value_ == censoc::largish<float_type>() ? false : true;
	}
#endif

#ifdef __WIN32__
	typename censoc::strip_const<typename censoc::param<bool>::type>::type 
	processing_incomplete() const
	{
		return processing_incomplete_;
	}

	void
	flag_incomplete_processing()
	{
		processing_incomplete_ = true;
	}

	void
	clear_incomplete_processing()
	{
		processing_incomplete_ = false;
	}
#else
	typename censoc::strip_const<typename censoc::param<bool>::type>::type 
	processing_incomplete() const
	{
		return false;
	}
	void flag_incomplete_processing() { }
	void clear_incomplete_processing() { }
#endif
};

}}}

// for the time-being model_factory_interface will be automatically declared, no need to include explicitly...
namespace censoc { namespace netcpu { namespace converger_1 { 

template <typename N, typename F>
struct coefficient_metadata : converger_1::coefficient_metadata_base<N, F> {

//{ types
	typedef converger_1::coefficient_metadata_base<N, F> base_type;

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype; 

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype; 

//}

private:
	using base_type::value_from_;
	using base_type::rand_range_;
	using base_type::now_value_;
	using base_type::tmp_value_;
public:
	using base_type::grid_resolutions; // todo later make those more private/protected(ish)
private:
	bool range_ended;
public:
	using base_type::value;
public:
	template <typename Vector>
	void
	sync(Vector const & grid_resolutions, size_paramtype value_i, bool const range_ended, float_paramtype value_f, float_paramtype value_from, float_paramtype rand_range) 
	{
		set_grid_resolutions(grid_resolutions);
		value_from_ = value_from;
		rand_range_ = rand_range;
		if ((this->range_ended = range_ended) == false)
			tmp_value_.index(value_i, grid_resolutions[0], value_from_, rand_range_);
		else
			tmp_value_.clobber(value_i, value_f);
		now_value_ = tmp_value_;
	}

#ifndef NDEBUG
	void
	value_testonly(float_paramtype x)
	{
		tmp_value_.value_testonly(x);
	}
	void
	now_value_testonly(float_paramtype x)
	{
		now_value_.value_testonly(x);
	}
#endif


	/** TODO -- later on 
	(much like in non-network scenarios, provide non-default ctor so that a better quality-assurance coding practices could be adhered to (this would mostly imply designing efficient array class which allows 'newing' objects with non-default ctors)...
	coefficient_metadata()
	{
	}
	*/

	/**
		{ NOTE, TODO } -- a grid-like environment would probably yield a more efficient (lesser number of iterations) converging process, but will take a little more work... so if the overall idea works then TODO for future: instead of mod counters, have each mod to be an array of flags for grid-levels -stepvalues and set each appropriately at the server level, then communicate from server to processor...
		but then, also, if one is to extrapolate the idea of replacing 'rand' coverage with explicitly determined/guaranteed coverage -- there is a question of various combinations of combos. E.g. 2-at-once combo scenario in a 30 coeff space: there may be a whole set of different 2-coeff combos { 1, 2 } { 1, 3 } { 12, 15 } etc. etc. which will need to be *deterministically* covered (as opposed to overshooting the mark with rand-based selections thereby achieving even coverage/determinism).
		*/
	void
	value_rand_on_bootstrap()
	{
		if (range_ended == false)
			tmp_value_.index(censoc::rand_nobias_raw<censoc::rand<uint32_t> >().eval(grid_resolutions[0]), grid_resolutions[0], value_from_, rand_range_); 
		censoc::llog() << "value_rand_on_bootstrap. from: [" << value_from_ << "], to: [" << value_from_ + rand_range_ << "], now: [" << value() << "]\n";
	}

	void
	value_from_grid(size_paramtype grid_i, size_paramtype grid_res)
	{
		if (range_ended == false)
			base_type::value_from_grid(grid_i, grid_res);
	}
};

template <typename N, typename F, typename Model>
struct task_processor_detail : netcpu::io_wrapper<netcpu::message::async_driver> {

	typedef typename Model::meta_msg_type meta_msg_type;

	//{ typedefs...

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef typename converger_1::key_typepair::ram key_type;
	BOOST_STATIC_ASSERT(::boost::is_unsigned<key_type>::value);

	/*
	Rethinking the whole row-major layout. Mainly because the composite matrix (z ~ x ~ y) appears to be used only '1 row at a time' anyway -- thus even a row-based matrix should not cause performance issues when it is only used to generate 'row views'.
	*/
	typedef ::Eigen::Matrix<float_type, ::Eigen::Dynamic, ::Eigen::Dynamic> matrix_columnmajor_type;
	typedef ::Eigen::Matrix<int, ::Eigen::Dynamic, ::Eigen::Dynamic, ::Eigen::AutoAlign | ::Eigen::RowMajor> int_matrix_rowmajor_type;

	typedef converger_1::coefficient_metadata<N, F> coefficient_metadata_type;
	typedef typename censoc::param<coefficient_metadata_type>::type coefficient_metadata_paramtype;
	typedef censoc::array<coefficient_metadata_type, size_type> coefficients_metadata_type;
	typedef typename censoc::param<coefficients_metadata_type>::type coefficients_metadata_paramtype;

	typedef censoc::lexicast< ::std::string> xxx;

	//}

	//{ data members

	Model model;

	key_type echo_status_key; 


	size_type const coefficients_size;
	coefficients_metadata_type coefficients_metadata;

	size_type const init_complexity_size;

	netcpu::combos_builder<size_type, coefficients_metadata_type> combos_modem;

	typedef ::std::map<size_type, size_type> complexities_type;
	complexities_type remaining_complexities;
	complexities_type unreported_complexities;

	void (task_processor_detail::* peer_report) ();

	class e_min_type {
		float_type x;
		float_type improvement_ratio_min;
		float_type improvement_ratio_min_inv;
		float_type x_target;
	public:
		e_min_type(float_paramtype x, float_paramtype improvement_ratio_min)
		: x(x), improvement_ratio_min(improvement_ratio_min), improvement_ratio_min_inv(1 / improvement_ratio_min), x_target(x < 0 ? x * improvement_ratio_min : x * improvement_ratio_min_inv) {
			assert(improvement_ratio_min != 0);
		}
		operator float_type () const
		{
			return x;
		}
		typename censoc::strip_const<float_paramtype>::type 
		target() const
		{
			return x_target;
		}
		void 
		operator = (float_paramtype new_x)
		{
			if  ((x = new_x) < 0)
				x_target = x * improvement_ratio_min;
			else
				x_target = x * improvement_ratio_min_inv;
		}
	} e_min;

	size_type const static rollback_report_timeout_minutes = 30;
	netcpu::message::async_timer rollback_report_timer; // used to *rarely* report accumulated unreported complexities, so that the server can update its view/state of the overall convergence process and can recover quicker if the server crashes due to power outage, etc... (otherwise a given 'coeffs at once' combination may take very long time to complete (e.g. may be a day) and then, if no new peers are connecting/dropping in the meantime, the after-crash server will need to start from the very start of the given combination thereby loosing a day (or something like that anyway...)
	netcpu::message::async_timer process_iteration_timer;
	bool skip_next_process_iteration;

	// message (net-io) caching
	converger_1::message::bootstrapping_peer_report<N, F> bootstrapping_peer_report_msg;
	converger_1::message::peer_report<N, F> peer_report_msg;
	converger_1::message::server_state_sync<N, F> server_state_sync_msg;
	converger_1::message::server_recentre_only_sync<N, F> server_recentre_only_sync_msg;
	converger_1::message::server_complexity_only_sync<N> server_complexity_only_sync_msg;

	// for printing/testing only!
#ifndef __WIN32__
	::timespec start;
#endif

	//}

	task_processor_detail(netcpu::message::async_driver & io_driver, meta_msg_type const & meta_msg)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver),
	model(meta_msg), 
	echo_status_key(0),
	// should be the same as the bulk's coeffs metadata size of array...
	coefficients_size(model.get_coefficients_size()) ,
	coefficients_metadata(coefficients_size),
	init_complexity_size(meta_msg.complexity_size()),
	e_min(censoc::largish<float_type>(), netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.improvement_ratio_min)), 
	skip_next_process_iteration(false)
	{
		// todo -- see if this is still needed (decomposed floating gets automatically initialised to 0 now... if this automatic initialisation is to be taken away, for efficiency purposes, then things in meta message like min improvement threshold would need to be explicitly set to 0 -- as things like controller.exe rely on testing for whether this was set in the command line by the user when creating a task...)
		netcpu::message::serialise_to_decomposed_floating(static_cast<float_type>(0), bootstrapping_peer_report_msg.value);

		censoc::llog() << "ctor in converger_1::task_processor_detail\n";

		::std::string bulk_msg_path((netcpu::ownpath.branch_path() /= "bulk.msg").string());
		if (::boost::filesystem::exists(bulk_msg_path) == true) {
			if (censoc::sha_file<size_type>::hushlen == meta_msg.bulk_msg_hush.size()) { // server may choose to send zero-hush to force the reloading of bulk message 
				censoc::sha_file<size_type> sha;
				if (!::memcmp(sha.calculate(bulk_msg_path), meta_msg.bulk_msg_hush.data(), censoc::sha_file<size_type>::hushlen)) {

					::std::ifstream bulk_msg_file(bulk_msg_path.c_str(), ::std::ios::binary | ::std::ios::trunc);
					if (bulk_msg_file) {
						censoc::llog() << "loading cached bulk message\n";

						netcpu::message::read_wrapper wrapper;
						netcpu::message::fstream_to_wrapper(bulk_msg_file, wrapper);

						typedef typename Model::bulk_msg_type bulk_msg_type;
						bulk_msg_type bulk_msg;
						bulk_msg.from_wire(wrapper);
						assert(coefficients_size == bulk_msg.coeffs.size());

						model.on_bulk_read(bulk_msg);
						io().write(converger_1::message::skip_bulk(), &task_processor_detail::on_skip_bulk_reply_write, this);

						return;
					}
				}
			}
			::boost::filesystem::remove(bulk_msg_path);
		}

		io().write(netcpu::message::good(), &task_processor_detail::on_meta_reply_write, this);
	}

	void
	on_skip_bulk_reply_write()
	{
		io().read(&task_processor_detail::on_first_server_state_sync_read, this);
	}

	void
	on_meta_reply_write()
	{
		censoc::llog() << "written meta reply good -- now waiting to read bulk\n";
		io().read(&task_processor_detail::on_bulk_read, this);
	}

#ifndef NDEBUG
	void
	pin_testonly(float_type const * data, float_paramtype new_emin)
	{
		for (size_type i(0); i != coefficients_size; ++i) 
			coefficients_metadata[i].now_value_testonly(data[i]);

		e_min = new_emin;
	}
	void
	eval_testonly(float_type const * data, float_paramtype expected)
	{
		for (size_type i(0); i != coefficients_size; ++i) 
			coefficients_metadata[i].value_testonly(data[i]);

		coefficients_print();
		opaque_result<float_type> result;
		do { 
			result.clear_incomplete_processing(); 
			model.log_likelihood(coefficients_metadata, e_min.target(), result); 
		} while (result.processing_incomplete() == true);
		censoc::llog() << "TEST, expecting: [" << expected << "], got: [" << result.value() << "]\n";
		if (result.value() < e_min.target()) 
			e_min = result.value();
	}
#endif

	void
	on_bulk_read()
	{
		censoc::llog() << "on_bulk_read called\n";

		typedef typename Model::bulk_msg_type bulk_msg_type;

		if (bulk_msg_type::myid != io().read_raw.id()) 
			throw ::std::runtime_error(xxx("wrong message: [") << io().read_raw.id() << "] want bulk id: [" << static_cast<netcpu::message::id_type>(bulk_msg_type::myid) << ']'); 

		censoc::llog() << "on_bulk_read ctored bulk_msg\n";
		bulk_msg_type bulk_msg;
		bulk_msg.from_wire(io().read_raw);

		censoc::llog() << "received bulk message: " << io().read_raw.size() << ::std::endl;

		assert(coefficients_size == bulk_msg.coeffs.size());

		model.on_bulk_read(bulk_msg);

		// note -- no need to do this really (unless testing in debug mode in the following code)...
		//combos_modem.build(coefficients_size, init_complexity_size, init_coeffs_atonce_size, coefficients_metadata);

		{
			::std::string bulk_msg_path((netcpu::ownpath.branch_path() /= "bulk.msg").string());
			::std::ofstream bulk_msg_file(bulk_msg_path.c_str(), ::std::ios::binary | ::std::ios::trunc);
			bulk_msg_file.write(reinterpret_cast<char const *>(io().read_raw.head()), io().read_raw.size());
			if (bulk_msg_file == false)
				throw ::std::runtime_error("could not write " + bulk_msg_path);
		}

		io().read(&task_processor_detail::on_first_server_state_sync_read, this);
	}


	// for the time-being not getting haltons from server, rather calculating directly to save on network b/w


	/**
		@note -- server state sync message should also tell the 'coeffs at once' parameter (for the time-being anyway, until one moves this kind of message into a standalone entitiy).
		*/
	void
	on_process_timeout()
	{
#ifdef __WIN32__
		if (netcpu::should_i_sleep == true) {
			process_iteration_timer.timeout(::boost::posix_time::milliseconds(netcpu::should_i_sleep.sleep_for()));
			return;
		}
#endif

		if (skip_next_process_iteration == true) {
			skip_next_process_iteration = false;
			process_iteration_timer.timeout();
			return;
		} else if (remaining_complexities.empty() == true) {
			// not settting the timer -- doing so instead when new complexities are created on on_server_state_sync thingy... (at the end there-- if not pending then timeout)
			// process_iteration_timer.timeout(::boost::posix_time::seconds(5));
			return;
		}

		// TODO -- take this comparator outside (determine whether to call based on 1st sync read from the server)
		size_type const i(remaining_complexities.begin()->first + remaining_complexities.begin()->second - 1); // doing from the back -- this way the range is likely not to be in need of re-inserting, but rather 'shortening' or adding' in the relevant map containers

	::std::pair<size_type, size_type> const processed_complexity(i, 1);

	censoc::llog() << "about to process, current e_min: " << e_min << ", current complexity: " << i << ", current io key: " << echo_status_key << ::std::endl;
	//coefficients_print();
	combos_modem.demodulate(i, coefficients_metadata, coefficients_size);

#ifndef NDEBUG
		{
			size_type modified_size(0);
			for (size_type j(0); j != coefficients_size; ++j) {
				if (coefficients_metadata[j].value_modified() == true)
					++modified_size;
			}
			assert(modified_size <= combos_modem.calculate_combo_size(i));
		}
#endif

		coefficients_print();
		opaque_result<float_type> result;
		timestamp_begin();
		for (size_type j(0); j != coefficients_size; ++j)
			if (coefficients_metadata[j].value_modified() == true) {
				model.log_likelihood(coefficients_metadata, e_min.target(), result);
				break;
			}
		timestamp_end();

#ifdef __WIN32__
		if (result.processing_incomplete() == true) {
			process_iteration_timer.timeout(::boost::posix_time::milliseconds(netcpu::should_i_sleep.sleep_for()));
			return;
		} 
#endif

		assert(processed_complexity.second);
		censoc::netcpu::range_utils::subtract(remaining_complexities, processed_complexity);
		censoc::netcpu::range_utils::add(unreported_complexities, processed_complexity);

		assert(censoc::isfinite(e_min) == true);

		size_type value_complexity(-1);
		if (result.value() < e_min.target()) {


			censoc::llog() << "found best of yet: " << result.value() << ", impr ratio: " << e_min / result.value() << '\n';

			e_min = result.value();
			value_complexity = i;
			assert(value_complexity != static_cast<size_type>(-1));
			assert(unreported_complexities.empty() == false);

			//@note -- cannot clear unreported_complexities because they will be used at the server side for 'visited places' and so will be of relevance!

#ifndef NDEBUG
			{
				size_type coeffs_changed(0);
				for (size_type i(0); i != coefficients_size; ++i)
					if (coefficients_metadata[i].value_modified() == true) 
						++coeffs_changed;
				assert(coeffs_changed);
			}
#endif
		} 
		if (value_complexity != static_cast<size_type>(-1) || remaining_complexities.empty() == true) {
			assert(unreported_complexities.empty() == false);
			assert(peer_report == &task_processor_detail::on_peer_report);

			netcpu::message::serialise_to_decomposed_floating(static_cast<float_type>(e_min), peer_report_msg.value);
			peer_report_msg.echo_status_key(echo_status_key);

			peer_report_msg.value_complexity(value_complexity);

			peer_report_msg.complexities.resize(unreported_complexities.size());
			size_type j(0);
			for (typename complexities_type::const_iterator i(unreported_complexities.begin()); i != unreported_complexities.end(); ++i) {
				converger_1::message::complexitywise_element<N> & complexitywise_element_msg(peer_report_msg.complexities(j++));
				complexitywise_element_msg.complexity_begin(i->first);
				complexitywise_element_msg.complexity_size(i->second);
			}
			assert(j == unreported_complexities.size());
			unreported_complexities.clear();

			if (io().is_write_pending() == false) 
				on_peer_report();
			else
				io().write_callback(&task_processor_detail::on_peer_report_write, this);
		} else // only need to start another iteration if nothing better was found and some complexities still remain 
			process_iteration_timer.timeout();
	}

	void
	on_peer_report()
	{
		peer_report_msg.print();
		censoc::llog() << "on_peer_report, SENDING a peer report\n";
		io().write(peer_report_msg);
	}

	void
	on_rollback_report_timeout()
	{
		if (unreported_complexities.empty() == false && io().is_write_pending() == false) // really simple -- if network/io buffering is an issue, then not reporting (not critical to report anyway)...
			on_peer_report();
		rollback_report_timer.timeout(::boost::posix_time::minutes(rollback_report_timeout_minutes));
	}

	void
	on_bootstrapping_process_timeout()
	{
#ifdef __WIN32__
		if (netcpu::should_i_sleep == true) {
			process_iteration_timer.timeout(::boost::posix_time::milliseconds(netcpu::should_i_sleep.sleep_for()));
			return;
		}
#endif

		if (skip_next_process_iteration == true) {
			skip_next_process_iteration = false;
			process_iteration_timer.timeout();
			return;
		} 

		for (size_type i(0); i != coefficients_size; ++i) 
			coefficients_metadata[i].value_rand_on_bootstrap();

		censoc::llog() << "doing a bootstrapping run...\n";

		timestamp_begin();

		coefficients_print();
		opaque_result<float_type> result;
		for (size_type j(0); j != coefficients_size; ++j)
			if (coefficients_metadata[j].value_modified() == true) {
				model.log_likelihood(coefficients_metadata, e_min.target(), result);
				break;
			}

#ifdef __WIN32__
		if (result.processing_incomplete() == true) {
			process_iteration_timer.timeout(::boost::posix_time::milliseconds(netcpu::should_i_sleep.sleep_for()));
			return;
		} 
#endif

		timestamp_end();
		assert(censoc::isfinite(e_min) == true);

		if (result.value() < e_min.target()) {
			censoc::llog() << "found bootstrapping at: " << result.value() << '\n';
			//@todo -- may not be needed, as packing the report message right here now... and any subsequent re-sync from server should reset the e_min anyways...
			e_min = result.value();

			bootstrapping_peer_report_msg.coeffs.resize(coefficients_size);
			for (size_type i(0); i != coefficients_size; ++i) 
				bootstrapping_peer_report_msg.coeffs(i, coefficients_metadata[i].index());

			netcpu::message::serialise_to_decomposed_floating(static_cast<float_type>(e_min), bootstrapping_peer_report_msg.value);
			bootstrapping_peer_report_msg.echo_status_key(echo_status_key);

			assert(peer_report == &task_processor_detail::on_bootstrapping_peer_report);
			if (io().is_write_pending() == false) 
				on_bootstrapping_peer_report();	
			else
				io().write_callback(&task_processor_detail::on_peer_report_write, this);

			//@note -- do not do any more processing (inline with "stopping right after the 1st find" logic as well as for the fact of maintaining coefficients values for when reporting is written out to wire.

		} else 
			process_iteration_timer.timeout();
	}

	void
	on_bootstrapping_peer_report()
	{
		censoc::llog() << "on_bootstrapping_peer_report, SENDING a peer report\n";

		io().write(bootstrapping_peer_report_msg);
	}

	void 
	coefficients_print()
	{
		censoc::llog() << '{';
		for (size_type i(0); i != coefficients_size; ++i) 
			censoc::llog() << coefficients_metadata[i].value() << (i != coefficients_size - 1 ? ", " : "");
		censoc::llog() << "}\n";
	}

	void virtual
	on_write()
	{
	}

	void
	on_peer_report_write()
	{
		io().write_callback(&task_processor_detail::on_write, this);
		(this->*peer_report)();
	}

	void
	on_first_server_state_sync_read()
	{
		assert(process_iteration_timer.is_pending() == false);
		rollback_report_timer.timeout(::boost::posix_time::minutes(rollback_report_timeout_minutes), &task_processor_detail::on_rollback_report_timeout, this);
		process_iteration_timer.timeout(&task_processor_detail::on_bootstrapping_process_timeout, this);
		peer_report = &task_processor_detail::on_bootstrapping_peer_report;
		io().read_callback(&task_processor_detail::on_server_state_sync_read, this);
		on_server_state_sync_read();
	}

	template <typename MsgType>
	void
	append_to_remaining_complexities(MsgType const & msg)
	{
		::std::list< ::std::pair<size_type, size_type> > tmp_container;
		for (size_type i(0); i != msg.complexities.size(); ++i) {
			converger_1::message::complexitywise_element<N> const & wire(msg.complexities(i));
			//censoc::netcpu::range_utils::add(remaining_complexities, ::std::make_pair(wire.complexity_begin(), wire.complexity_size()));
			tmp_container.push_back(::std::make_pair(wire.complexity_begin(), wire.complexity_size()));
		}
		if (tmp_container.empty() == false)
			censoc::netcpu::range_utils::add(remaining_complexities, tmp_container);
	}

	void
	respond_to_end_process()
	{
		assert(io().is_write_pending() == false);
		io().write(netcpu::message::good(), &task_processor_detail::on_end_process_reply_written, this);
	}

	void
	on_end_process_reply_written()
	{
		(new netcpu::peer_connection(io()))->on_write();
	}

	/** { //note should only be called last in the code -- may call 'delete this'
		} */
	void
	on_server_state_sync_read()
	{
		censoc::llog() << "processing server_state sync\n";
		typename censoc::param<netcpu::message::id_type>::type msg_id(io().read_raw.id());
		if (netcpu::message::end_process::myid == msg_id) {
			if (io().is_write_pending() == false)
				respond_to_end_process();
			else
				io().write_callback(&task_processor_detail::respond_to_end_process, this);
			return;
		} else if (converger_1::message::server_state_sync<N, F>::myid == msg_id) {

			censoc::llog() << "... server_state\n";

			server_state_sync_msg.from_wire(io().read_raw);

			server_state_sync_msg.print();

			// if want to be bootstrapping
			if (server_state_sync_msg.am_bootstrapping()) {
				assert(netcpu::message::deserialise_from_decomposed_floating<float_type>(server_state_sync_msg.value) == censoc::largish<float_type>());
				peer_report = &task_processor_detail::on_bootstrapping_peer_report;
				if (process_iteration_timer.is_pending() == false) 
					process_iteration_timer.timeout(&task_processor_detail::on_bootstrapping_process_timeout, this);
				else 
					process_iteration_timer.timeout_callback(&task_processor_detail::on_bootstrapping_process_timeout, this);
			} else { // if want to be processing
				//first_bootstrapping = false;
				peer_report = &task_processor_detail::on_peer_report;
				if (process_iteration_timer.is_pending() == false) 
					process_iteration_timer.timeout(&task_processor_detail::on_process_timeout, this);
				else 
					process_iteration_timer.timeout_callback(&task_processor_detail::on_process_timeout, this);
			}

			if (echo_status_key != server_state_sync_msg.echo_status_key()) {
				echo_status_key = server_state_sync_msg.echo_status_key();
				io().write_callback(&task_processor_detail::on_write, this);
				unreported_complexities.clear();
				e_min = netcpu::message::deserialise_from_decomposed_floating<float_type>(server_state_sync_msg.value);

				// assign in-ram coefficients data
				assert(server_state_sync_msg.coeffs.size() == coefficients_size); // server should send all coeffs for the 'whole state' sync (it's like a hammer of truth)
				for (size_type i(0); i != coefficients_size; ++i) {
					coefficient_metadata_type & ram(coefficients_metadata[i]);
					converger_1::message::coeffwise_server_state_sync<N, F> const & wire(server_state_sync_msg.coeffs(i));
					//assert(first_bootstrapping == false || ram.boot_end() <= wire.grid_resolutions[0]);
					ram.sync(wire.grid_resolutions, wire.value(), wire.range_ended() ? true : false, netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_f), netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_from), netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.rand_range));
				}

				// now also building modem after 'ram.sync' since it now sets the grid_resolutions for every coeff as well.
				size_type init_complexity_size_tmp(init_complexity_size);
				assert(init_complexity_size);
				size_type init_coeffs_atonce_size(coefficients_metadata[0].grid_resolutions.size());
				assert(init_coeffs_atonce_size);
				combos_modem.build(coefficients_size, init_complexity_size_tmp, init_coeffs_atonce_size, coefficients_metadata);
				assert(init_complexity_size_tmp);
				assert(init_coeffs_atonce_size);

			} else if (unreported_complexities.empty() == false) {
				// if echo key is the same, and unreported complexities present -- write them out (in case the message was sent as a delayed cause of recalculate peers complexities from the server)
				assert(peer_report == &task_processor_detail::on_peer_report);
				if (io().is_write_pending() == false) 
					on_peer_report();
				else
					io().write_callback(&task_processor_detail::on_peer_report_write, this);
			}
				
			// substitute complexity range (need to do in all cases -- as could be a part of either -- proper state-sync, or just recalculation of complexities due to another peer connecting to server...
			remaining_complexities.clear();
			append_to_remaining_complexities(server_state_sync_msg);

#ifndef NDEBUG
			if (!server_state_sync_msg.am_bootstrapping()) {


				// tmp-based testing: force recalculation of all caches:
				model.invalidate_caches_1();

				float_type const e_min_real(e_min);
				e_min = censoc::largish<float_type>();

				censoc::llog() << "e_min: " << e_min_real << ::std::endl;
				for (size_type i(0); i != coefficients_size; ++i) 
					coefficients_metadata[i].value_reset();
				coefficients_print();
				opaque_result<float_type> result;
				do {
					result.clear_incomplete_processing(); 
					model.log_likelihood(coefficients_metadata, e_min.target(), result); 
				} while (result.processing_incomplete() == true);
				censoc::llog() << "e : " << result.value() << ::std::endl;
				assert(::std::abs(result.value() - e_min_real) < 1
					||
					// different h/w architectures may have different compile-time order if fp instructions and/or overall tolerance to underflows/overflows generally... so, esp. as number of observations per respondent increases and subsequently the explicit product sequence becomes much longer, one must accept that verification on one machine would yield a 'largish' value whereas another (reporting) machine would produce a perfectly finite/acceptable value...
					result.is_valid() == false
				);

				e_min = e_min_real;
			}
#endif

		} else if (converger_1::message::server_recentre_only_sync<N, F>::myid == msg_id) {

			censoc::llog() << "... recentre\n";

			io().write_callback(&task_processor_detail::on_write, this);

			server_recentre_only_sync_msg.from_wire(io().read_raw);
			assert(echo_status_key != server_recentre_only_sync_msg.echo_status_key());
			echo_status_key = server_recentre_only_sync_msg.echo_status_key();
			e_min = netcpu::message::deserialise_from_decomposed_floating<float_type>(server_recentre_only_sync_msg.value);

			// substitute complexity range
			unreported_complexities.clear();
			remaining_complexities.clear();
			append_to_remaining_complexities(server_recentre_only_sync_msg);
			assert(remaining_complexities.empty() == false);

			// assign in-ram coefficients data 
			assert(server_recentre_only_sync_msg.value_complexity() != static_cast<typename netcpu::message::typepair<N>::wire>(-1));
			combos_modem.demodulate(server_recentre_only_sync_msg.value_complexity(), coefficients_metadata, coefficients_size);
			for (size_type i(0); i != coefficients_size; ++i) 
				coefficients_metadata[i].value_save();

#ifndef NDEBUG
			{
				// tmp-based testing: force recalculation of all caches:
				model.invalidate_caches_1();

				float_type const e_min_real(e_min);
				e_min = censoc::largish<float_type>();
				censoc::llog() << "expected e_min: " << e_min_real << ::std::endl;

				coefficients_print();
				opaque_result<float_type> result;
				do {
					result.clear_incomplete_processing(); 
					model.log_likelihood(coefficients_metadata, e_min.target(), result); 
				} while (result.processing_incomplete() == true);
				censoc::llog() << "e : " << result.value() << ::std::endl;
				// TODO!!!
				assert(::std::abs(result.value() - e_min_real) < 1
					||
					// different h/w architectures may have different compile-time order if fp instructions and/or overall tolerance to underflows/overflows generally... so, esp. as number of observations per respondent increases and subsequently the explicit product sequence becomes much longer, one must accept that verification on one machine would yield a 'largish' value whereas another (reporting) machine would produce a perfectly finite/acceptable value...
					result.is_valid() == false
				);

				e_min = e_min_real;
			}
#endif

			// peer may have stopped after finding a better find (which is what is supposed to do btw -- no point in starting on potentially new iteration which may take quite some time, when a new 'recentre' message may be received half-way through such a new iteration and given non-preemptive nature of the design there may actually be more cpu cycles wasted).
			if (process_iteration_timer.is_pending() == false) 
				process_iteration_timer.timeout();

		} else if (converger_1::message::server_complexity_only_sync<N>::myid == msg_id) {
			censoc::llog() << "... complexity sync\n";

			server_complexity_only_sync_msg.from_wire(io().read_raw);
			if (!server_complexity_only_sync_msg.noreply() && unreported_complexities.empty() == false) {
				assert(peer_report == &task_processor_detail::on_peer_report);
				if (io().is_write_pending() == false) 
					on_peer_report();
				else
					io().write_callback(&task_processor_detail::on_peer_report_write, this);
			}
			// NOTE -- no need to have echo_status_key/io_key for this message (not atomic/serialised w.r.t. server state)
			remaining_complexities.clear();
			append_to_remaining_complexities(server_complexity_only_sync_msg);

			// peer may have stopped because all of it's complexities were done
			if (process_iteration_timer.is_pending() == false) 
				process_iteration_timer.timeout();
		} else 
			throw ::std::runtime_error(xxx("wrong message: [") << msg_id << "] want either server_state_sync or server_recentre_only_sync id: [" << static_cast<netcpu::message::id_type>(converger_1::message::server_state_sync<N, F>::myid) << ']');

		skip_next_process_iteration = true; // in case there are many pending messages from server (e.g. many finds have been discovered by other multiple faster computers)... as it would be pointless to be processing the effects of each intermediate one with 'on_process_timeout'
		io().read();
	}

	void 
	timestamp_begin()
	{
#ifndef __WIN32__
		if (::clock_gettime(CLOCK_MONOTONIC, &start) == -1)
			throw ::std::runtime_error(xxx() << "clock_gettime(CLOCK_MONOTONIC...) failed: [" << ::strerror(errno) << "]");
#else
		// may be use ::clock()
#endif
	}

	void
	timestamp_end() 
	{
#ifndef __WIN32__
		::timespec end;
		if (::clock_gettime(CLOCK_MONOTONIC, &end) == -1)
			throw ::std::runtime_error(xxx() << "clock_gettime(CLOCK_MONOTONIC...) failed: [" << ::strerror(errno) << "]");

		// note, technically speaking, if tv_nsec was an unsigned integral, the following 'end.tv_nsec - start.tv_nsec' may yield large positive 
		//	(i.e. small negative) number; and whilst it is perfectly ok for complement-2 notation when adding/dealing-with other integrals -- 
		// any intermediate casting to floating-point would be a disaster (large positive number would not then wrap-around back to normal values but would be treated as actually-large value). 
		// However, following the timspec specifications as per 'man clock_gettime' (in FreeBSD) and as per POSIX (opengroup) specs, 
		// one is relying on 'tv_nsec' being a signed number.
		float_type const rv(end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) * static_cast<float_type>(1e-9));
		censoc::llog() << "timestamp duration (sec): [" << rv  << "]\n";
#else
		// may be use ::clock()
#endif
	}
};

template <typename N, typename F, typename Model>
struct task_processor_detail_meta : netcpu::io_wrapper<netcpu::message::async_driver> {
	typedef censoc::lexicast< ::std::string> xxx;
	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef F float_type;

	task_processor_detail_meta(netcpu::message::async_driver & io_driver)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver) {
		censoc::llog() << "ctor in converger_1::task_processor_detail_meta\n";
		io().write(netcpu::message::good(), &task_processor_detail_meta::on_written_good, this);
	}
	void
	on_written_good()
	{
		io().read(&task_processor_detail_meta::on_read, this);
	}

	~task_processor_detail_meta()
	{
		censoc::llog() << "dtor in converger_1::task_processor_detail_meta\n";
	}

	void
	on_read()
	{
		typedef typename Model::meta_msg_type meta_msg_type;

		if (meta_msg_type::myid != io().read_raw.id()) 
			throw ::std::runtime_error(xxx("wrong message: [") << io().read_raw.id() << "] want meta id: [" << static_cast<netcpu::message::id_type>(meta_msg_type::myid) << ']');

		meta_msg_type meta_msg;
		meta_msg.from_wire(io().read_raw);

		if (Model::on_meta_read(meta_msg) == false)
			io().write(netcpu::message::bad(), &task_processor_detail_meta::on_bad_meta_reply_write, this);
		else {
			new converger_1::task_processor_detail<N, F, Model>(io(), meta_msg);
			//new converger_1::task_processor_detail<N, F, Model>(io(), meta_msg.reduce_exp_complexity() ? true : false, alternatives, repetitions, meta_msg.complexity_size(), netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.improvement_ratio_min), meta_msg.matrix_composite_rows(), meta_msg.matrix_composite_columns(), meta_msg.x_size(), meta_msg.respondents_choice_sets, meta_msg.draws_sets_size());
		}
	}

	void 
	on_end_process_read()
	{
		typename censoc::param<netcpu::message::id_type>::type msg_id(io().read_raw.id());
		if (netcpu::message::end_process::myid == msg_id)
			io().write(netcpu::message::good(), &task_processor_detail_meta::on_end_process_replied_write, this);
		else  
			throw ::std::runtime_error("expected end_process after sending bad message, but got something else");
	}

	void
	on_end_process_replied_write()
	{
		censoc::llog() << "got end_process -- will nuke self...\n";
		(new netcpu::peer_connection(io()))->on_write();
	}

	void
	on_bad_meta_reply_write()
	{
		censoc::llog() << "written meta reply bad -- now waiting to read end_process...\n";
		io().read(&task_processor_detail_meta::on_end_process_read, this);
	}
};

template <template <typename, typename, typename, bool> class Model>
struct task_processor : netcpu::io_wrapper<netcpu::message::async_driver> {
	typedef censoc::lexicast< ::std::string> xxx;
	typedef converger_1::message::res res_msg_type;

	task_processor(netcpu::message::async_driver & io_driver)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver) {
		censoc::llog() << "task_processor ctor in converger_1" << ::std::endl;
		io().write(netcpu::message::good(), &task_processor::on_written_good, this);
	}

	~task_processor() throw()
	{
		censoc::llog() << "dtor of task_processor in converger_1: " << this << ::std::endl;
	}

	void
	on_written_good()
	{
		io().read(&task_processor::on_read, this);
	}

	void
	on_read()
	{
		if (res_msg_type::myid != io().read_raw.id()) 
			//delete this;
			throw ::std::runtime_error(xxx("wrong message: [") << io().read_raw.id() << "] want res id: [" << static_cast<netcpu::message::id_type>(res_msg_type::myid) << ']');

		res_msg_type msg(io().read_raw);
		censoc::llog() << "received res message\n";
		msg.print();

		switch (msg.int_res()) {
			case converger_1::message::int_res<uint32_t>::value :
			new_task_processor_detail_meta_exponent_approximation_choice<uint32_t>(msg);
		break;
			case converger_1::message::int_res<uint64_t>::value :
			new_task_processor_detail_meta_exponent_approximation_choice<uint64_t>(msg);
		break;
		censoc::llog() << "unsupported resolution: [" << msg.int_res() << ']';
		io().write(netcpu::message::bad(), &task_processor::on_bad_write, this);
		}
	}

	// todo -- a quick hack, later on also test for incorrect values
	template <typename IntRes>
	void 
	new_task_processor_detail_meta_exponent_approximation_choice(res_msg_type const & msg)
	{
		if (msg.approximate_exponents()) {
			new_task_processor_detail_meta<IntRes, true>(msg);
		} else {
			new_task_processor_detail_meta<IntRes, false>(msg);
		}
	}

	template <typename IntRes, bool ApproximateExponents>
	void 
	new_task_processor_detail_meta(res_msg_type const & msg)
	{
		// explicit because no need really to have compiler go through "<double, float>" arrangement...
		if (msg.float_res() == converger_1::message::float_res<float>::value && msg.extended_float_res() == converger_1::message::float_res<float>::value)
			new converger_1::task_processor_detail_meta<IntRes, float, Model<IntRes, float, float, ApproximateExponents> >(io());
		else if (msg.float_res() == converger_1::message::float_res<double>::value && msg.extended_float_res() == converger_1::message::float_res<double>::value)
			new converger_1::task_processor_detail_meta<IntRes, double, Model<IntRes, double, double, ApproximateExponents> >(io());
		else if (msg.float_res() == converger_1::message::float_res<float>::value && msg.extended_float_res() == converger_1::message::float_res<double>::value)
			new converger_1::task_processor_detail_meta<IntRes, float, Model<IntRes, float, double, ApproximateExponents> >(io());
		else // TODO -- may be as per 'on_read' -- write a bad message to client (more verbose)
			throw ::std::runtime_error(xxx("unsupported floating resolutions: [") << msg.float_res() << "], [" << msg.extended_float_res() << ']');
	}
	void
	on_bad_write()
	{
		io().read(&task_processor::on_end_process_read, this);
	}
	void 
	on_end_process_read()
	{
		typename censoc::param<netcpu::message::id_type>::type msg_id(io().read_raw.id());
		if (netcpu::message::end_process::myid == msg_id)
			io().write(netcpu::message::good(), &task_processor::on_end_process_replied_write, this);
		else  
			throw ::std::runtime_error("expected end_process after sending bad message, but got something else");
	}

	void
	on_end_process_replied_write()
	{
		censoc::llog() << "got end_process -- will nuke self...\n";
		(new netcpu::peer_connection(io()))->on_write();
	}
};

// this must be a very very small class (it will exist through the whole of the lifespan of the program)
template <template <typename, typename, typename, bool> class Model, netcpu::models_ids::val ModelId>
struct model_factory : netcpu::model_factory_base<ModelId> {
	void 
	new_task(netcpu::message::async_driver & io_driver)
	{
		censoc::llog() << "model factory is instantiating a task" << ::std::endl;
		new converger_1::task_processor<Model>(io_driver);
	}
};

}}}



#endif
