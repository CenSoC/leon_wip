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

// includes... {

#include <stdio.h>

#include <iostream>

#include <boost/numeric/conversion/bounds.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>

#include <Eigen/Core>

#include <censoc/stl_container_utils.h>
#include <censoc/rand.h>
#include <censoc/cdfinvn.h>

#include <netcpu/message.h>
#include <netcpu/fstream_to_wrapper.h>
#include <netcpu/io_wrapper.h>
#include <netcpu/combos_builder.h>
#include <netcpu/range_utils.h>

#include "coefficient_metadata_base.h"
#include "message/res.h"
#include "message/meta.h"
#include "message/bulk.h"
#include "message/peer_report.h"
#include "message/server_state_sync.h"
#include "message/convergence_state.h"

// includes }

#ifndef CENSOC_NETCPU_CONVERGER_1_SERVER_H
#define CENSOC_NETCPU_CONVERGER_1_SERVER_H

//{ for the time-being model_factory_interface will be automatically declared, no need to include explicitly...}

namespace censoc { namespace netcpu { namespace converger_1 { 

::timespec static start;

template <typename N, typename F>
struct coefficient_metadata : converger_1::coefficient_metadata_base<N, F> {

  //{ typedefs 

	typedef converger_1::coefficient_metadata_base<N, F> base_type;

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype; 

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype; 

	//}

private:
	//{ data members

	// TODO -- these 2 fields may not be really needed (deprecate in future)
	float_type constrain_from_;
	float_type constrain_to_;

#if 0
	float_type clamp_from_;
	float_type clamp_to_;
#endif

	float_type rand_range_end_;

	bool range_ended_;

	netcpu::big_uint<size_type> * accumulating_offset_;
	netcpu::big_uint<size_type> offset_;

	using base_type::value_from_;
	using base_type::rand_range_;
	using base_type::now_value_;
	using base_type::tmp_value_;


	void
	rebuild_index_offset()
	{
		assert(accumulating_offset_ != NULL);
		offset_ = *accumulating_offset_;
#ifndef NDEBUG
		for (size_type i(0); i != grid_resolutions.size(); ++i) 
			assert(grid_resolutions[0] >= grid_resolutions[i]);
#endif
		*accumulating_offset_ *= grid_resolutions[0];
		//::std::clog << "rebuilding the index offsets, offset_.size() " << offset_.size() << ", accumulating_offset_->size() " << accumulating_offset_->size() << "\n";
	}

	//}

public:
	using base_type::grid_resolutions; // todo later make those more private/protected(ish)


#ifndef NDEBUG
	bool
	assert_values() const
	{

		if (constrain_from_ - .00001 > value_from_) {
			censoc::llog() << __LINE__ << ", " << __FILE__ << ", constrain_from <= value_from sanity-check failed. constrain_from: [" << constrain_from_ << "], value_from: [" << value_from_ << "]\n";
			return false;
		}

		if (constrain_to_ + .00001 < value_from_ + rand_range_) {
			censoc::llog() << __LINE__ << ", " << __FILE__ << "constrain_to >= value_to_ sanity-check failed. constrain_to: [" << constrain_to_ << "], value_to: [" << value_from_ + rand_range_ << "]\n";
			return false;
		}

		return true;
	}
#endif

private:
	void inline
	update_rand_range(float_paramtype x, size_type & coefficients_rand_range_ended_wait)
	{
		assert(rand_range_ > rand_range_end_);
		censoc::llog() << "rand_range before: " << rand_range_ << '\n';
		rand_range_ = x;
		censoc::llog() << "rand_range after: " << rand_range_ << '\n';
		if (rand_range_ <= rand_range_end_) {
			range_ended_ = true;
			--coefficients_rand_range_ended_wait;
			::std::clog << "coefficient is done, remaining: " << coefficients_rand_range_ended_wait << '\n';
		}
	}
public:

	bool inline
	range_ended() const
	{
		return range_ended_;
	}

	/**
		@pre -- sequence of calls is: "pre_shrink", "adjust the grid resolutions", "post_shrink"
		*/
	void
	pre_shrink(size_type & coefficients_rand_range_ended_wait, float_paramtype shrink_slowdown)
	{
		assert(assert_values() == true);

		censoc::llog() << "ACTUALLY SHRINKING\n";

		if (range_ended_ == false) {

			float_type const grid_step_size(rand_range_ / (grid_resolutions[0] + 1));
			assert(grid_step_size >= 0);

			float_type const new_value_from(now_value_.value() - grid_step_size);
			float_type const from_diff((new_value_from - value_from_) * shrink_slowdown);
			float_type const new_value_to(now_value_.value() + grid_step_size);
			float_type const to_diff((value_from_ + rand_range_ - new_value_to) * shrink_slowdown);

			value_from_ = new_value_from - from_diff;
			// value_from_ = now_value_.value() - grid_step_size;

			//rand_range_ = grid_step_size + grid_step_size;
			update_rand_range(new_value_to + to_diff - value_from_, coefficients_rand_range_ended_wait);

			assert(assert_values() == true);
		}
	}

	void
	post_shrink(
			//size_type & coefficients_rand_range_ended_wait, 
			size_paramtype intended_coeffs_at_once) 
	{
			if (grid_resolutions.size() < intended_coeffs_at_once) {
				::std::cout << "padding grid resolutions.\nfrom: ";
				for (size_type i(0); i != grid_resolutions.size(); ++i)
					::std::cout << grid_resolutions[i] << ' ';
				grid_resolutions.resize(intended_coeffs_at_once, grid_resolutions.back());
				::std::cout << "\nto: ";
				for (size_type i(0); i != grid_resolutions.size(); ++i)
					::std::cout << grid_resolutions[i] << ' ';
				::std::cout << '\n';
			}

#if 0
			{ // clamp_{from, to} appropriation

				float_type grid_step_size(rand_range_ / (grid_resolutions[0] + 1));

				float_type value_to(value_from_ + rand_range_);

				// do the 'from' adjustment wrt clamping
				float_type first_index_value(value_from_ + grid_step_size);
				if (first_index_value < clamp_from_) {
					grid_step_size = (value_to - clamp_from_) / grid_resolutions[0];
					value_from_ = clamp_from_ - grid_step_size;
				}

				// do the 'to' adjustment wrt clamping
				float_type last_index_value(value_to - grid_step_size);
				if (last_index_value > clamp_to_) {

					if (first_index_value < clamp_from_) { // 'clam_from_' has been adjusted so as 1st index will be on the clamp_from_ mark, so must take extra care!!!
						assert(grid_resolutions[0]);
						grid_step_size = (clamp_to_ - clamp_from_) / (grid_resolutions[0] - 1);
						value_from_ = clamp_from_ - grid_step_size;
					} else // can shrink the grid_step_size all the way from 'value_from_' on the left...
						grid_step_size = (clamp_to_ - value_from_) / grid_resolutions[0];

					value_to = clamp_to_ + grid_step_size;
				}

				assert(last_index_value >= first_index_value);

				// rang_range_ may have shrunk
				if (first_index_value < clamp_from_ || last_index_value > clamp_to_) {
					float_type const new_rand_range(value_to - value_from_);
					assert(new_rand_range <= rand_range_);
					if (rand_range_ > rand_range_end_)
						update_rand_range(new_rand_range, coefficients_rand_range_ended_wait);
					else 
						rand_range_ = new_rand_range;
				}
			}
#endif


		// even if my coeff is not in need of recalculating, the other coeffs might be... and so the total 'offset' will be affected.
		rebuild_index_offset();

		if (range_ended_ == true)
			now_value_.clobber(0, now_value_.value());
		else // this is probably not needed (expected to be randomised on bootstrapping at the processor end anyway -- as every shrink now activates bootstrapping)
			now_value_.index(0, grid_resolutions[0], value_from_, rand_range_);
		tmp_value_ = now_value_;
	}

public:

#if 1
	void
	value_from_grid(size_paramtype grid_i, size_paramtype grid_res)
	{
		if (range_ended_ == false)
			base_type::value_from_grid(grid_i, grid_res);

		assert(accumulating_offset_ != NULL);
		*accumulating_offset_ += offset_ * tmp_value_.index();
		//::std::clog << "value_from_grid, offset_.size() " << offset_.size() << ", accumulating_offset_->size() " << accumulating_offset_->size() << ", tmp_value_.index() " << tmp_value_.index() << "\n";
	}

	void
	value_reset()
	{
		base_type::value_reset();
		assert(accumulating_offset_ != NULL);
		*accumulating_offset_ += offset_ * tmp_value_.index();
		//::std::clog << "value_reset, offset_.size() " << offset_.size() << ", accumulating_offset_->size() " << accumulating_offset_->size() << ", tmp_value_.index() " << tmp_value_.index() << "\n";
	}
#endif

	void
	init(float_paramtype constrain_from, float_paramtype constrain_to, 
#if 0
			float_paramtype clamp_from, float_paramtype clamp_to, 
#endif
			float_paramtype rand_range_end, netcpu::big_uint<size_type> & accumulating_offset)
	{ 
		constrain_from_ = constrain_from;
		constrain_to_ = constrain_to;

#if 0
		clamp_from_ = clamp_from;
		clamp_to_ = clamp_to;
#endif

		rand_range_end_ = rand_range_end;
		accumulating_offset_ = &accumulating_offset;
		assert(accumulating_offset_ != NULL);
	}

	template <typename Vector>
	void
	reset_from_convergence_state(size_paramtype value_i, bool const range_ended, float_paramtype value_f, float_paramtype value_from, float_paramtype rand_range, Vector const & grid_resolutions)
	{
		value_from_ = value_from;
		rand_range_ = rand_range;

		set_grid_resolutions(grid_resolutions);

		rebuild_index_offset();

		if ((range_ended_ = range_ended)  == true)
			now_value_.index(value_i, grid_resolutions[0], value_from_, rand_range_);
		else
			now_value_.clobber(value_i, value_f);

		tmp_value_ = now_value_;
	}

	template <typename Vector>
	void
	reset_before_bootstrapping(Vector const & grid_resolutions)
	{
		range_ended_ = false;
		value_from_ = constrain_from_;
		rand_range_ = constrain_to_ - constrain_from_;

		set_grid_resolutions(grid_resolutions);

		rebuild_index_offset();

		// this is probably not needed (expected to be randomised on bootstrapping at the processor end anyway), unless starting range has already ended (which should not be -- todo, make a runtime check of user-entered values).
		{
		now_value_.index(0, grid_resolutions[0], value_from_, rand_range_);
		tmp_value_ = now_value_;
		}

		assert(assert_values() == true);

	}

	void
	reset_after_bootstrapping(size_paramtype i)
	{
		assert(range_ended_ == true || (tmp_value_ == now_value_ && tmp_value_.value() == now_value_.value()));
		if (range_ended_ == false) {
			now_value_.index(i, grid_resolutions[0], value_from_, rand_range_);
			tmp_value_ = now_value_;
		}

		assert(accumulating_offset_ != NULL);
		*accumulating_offset_ += offset_ * tmp_value_.index();

		assert(assert_values() == true);
	}

};

template <typename N, typename F, typename Model, netcpu::models_ids::val ModelId> struct processing_peer;

// current end-process sequence: server->'end_process_message'->client; client->'good'->server; client->ready_to_process->server;
struct end_process_writer : netcpu::io_wrapper<netcpu::message::async_driver> {

	bool stale_message_seen;

	end_process_writer(netcpu::message::async_driver & io_driver)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver), stale_message_seen(false) {
		if (io().is_write_pending() == false) 
			io().write(netcpu::message::end_process(), &end_process_writer::on_write, this);
		else
			io().write_callback(&end_process_writer::late_write_end_process, this);
	}
	void
	late_write_end_process()
	{
		io().write(netcpu::message::end_process(), &end_process_writer::on_write, this);
	}
	void
	on_write()
	{
		if (io().is_read_pending() == false) 
			io().read();
	}
	void
	on_read()
	{
		switch (io().read_raw.id()) {
		case netcpu::message::good::myid :
			(new netcpu::peer_connection(io()))->io().read();
			break;
		default :
			if (stale_message_seen == false) { // allow one stale message (e.g. from previous peer or bootstrapping report) to be present in the pipeline...
				stale_message_seen = true;
				io().read();
			} else
				delete this;
		}
	}
};

template <typename N, typename F, typename Model, netcpu::models_ids::val ModelId>
struct task_processor : ::boost::noncopyable {

  //{ typedefs...
	typedef censoc::lexicast< ::std::string> xxx;

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef ::Eigen::Matrix<float_type, ::Eigen::Dynamic, 1> vector_column_type;

	typedef typename converger_1::key_typepair::ram key_type;

	typedef censoc::array<converger_1::coefficient_metadata<N, F>, size_type> coefficients_metadata_type;

	// grid-related accounting
	typedef ::std::map<size_type, size_type> complexities_type;

	//}

	float_type shrink_slowdown;


	size_type coefficients_rand_range_ended_wait;

	size_type coefficients_size;
	size_type intended_coeffs_at_once;
	coefficients_metadata_type coefficients_metadata;
	typedef ::std::list<  // zoom levels
		::std::pair<size_type, // coeffs_at_once
			::std::vector< // individual coefficients
				::std::pair<float_type, // threshold
				::std::vector<size_type> // grid resolutions
			> 
		> > 
	> coefficients_metadata_x_type;
	coefficients_metadata_x_type coefficients_metadata_x;

	float_type e_min;
	bool am_bootstrapping;
	size_type e_min_complexity;

	bool better_find_since_last_recentered_sync;

	::std::list<converger_1::processing_peer<N, F, Model, ModelId> *> scoped_peers; // manages destruction/scoping of peers
	censoc::stl::fastsize< ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>, size_type> processing_peers; // manages processing (io) w.r.t. peers (because peer may be ctored, but it's readiness to do io only comes after reading/processing few meta/res/etc. messages)

	netcpu::message::async_timer server_state_sync_timer;

	// grid-related accounting
	netcpu::combos_builder<size_type, coefficients_metadata_type> combos_modem;
	typename ::std::map<size_type, ::std::vector<size_type> >::const_iterator current_complexity_level;

	// NOTE -- the following todo may no longer hold relevance...
	// TODO -- later refactor 'redistribute_remaining_complexities' and 'remaining_complexities' into the set of 'initial' (when new peer connects or when new complexity level is being established) and 'remaining' (when new peer connects and needs to be given the chunk of the "currently remaining" complexities) ... this will save on "recalculating complexities" during the recentering events (i.e. when better finds are discovered) (although this may yield always 1 more element (array size) in messaging for 'initial' list of complexities... unless a more complex messaging sequences are introduced and supported... may not be worth the trouble... will think about it some more later on...)
	complexities_type accumulated_remaining_complexities_since_last_sync;
	complexities_type remaining_complexities;
	bool redistribute_remaining_complexities;
	bool peer2peer_assisted_complexity_present; // indicates that some peers have their complexities done by other peers...

	netcpu::big_uint<size_type> offset;
	::std::map<netcpu::big_uint<size_type>, size_type> accumulated_visited_places_since_last_sync;
	::std::map<netcpu::big_uint<size_type>, size_type> visited_places;

	task_processor()
	: 
#ifndef NDEBUG
	intended_coeffs_at_once(0),
#endif
	e_min_complexity(-1), better_find_since_last_recentered_sync(false), redistribute_remaining_complexities(false), peer2peer_assisted_complexity_present(false), io_key(0) { 


		netcpu::message::read_wrapper wrapper;

		::std::ifstream res_file((netcpu::root_path + netcpu::pending_tasks.front()->name() + "/res.msg").c_str(), ::std::ios::binary);
		if (res_file == false)
			throw ::std::runtime_error("missing res message during load");

		netcpu::message::fstream_to_wrapper(res_file, wrapper);
		res_msg.from_wire(wrapper);
		censoc::llog() << "read res message:\n";
		res_msg.print();

		assert(res_msg.int_res() == converger_1::message::int_res<N>::value);
		assert(res_msg.float_res() == converger_1::message::float_res<F>::value);

		::std::ifstream meta_file((netcpu::root_path + netcpu::pending_tasks.front()->name() + "/meta.msg").c_str(), ::std::ios::binary);
		if (meta_file == false)
			throw ::std::runtime_error("missing meta message during load");

		netcpu::message::fstream_to_wrapper(meta_file, wrapper);
		meta_msg.from_wire(wrapper);
		censoc::llog() << "read meta message:\n";
		meta_msg.print();

		shrink_slowdown = netcpu::message::deserialise_from_decomposed_floating<float_type>(meta_msg.shrink_slowdown);
		assert(shrink_slowdown >= 0 && shrink_slowdown < 1);

		::std::ifstream bulk_file((netcpu::root_path + netcpu::pending_tasks.front()->name() + "/bulk.msg").c_str(), ::std::ios::binary);
		if (bulk_file == false)
			throw ::std::runtime_error("missing bulk message during load");

		// load the wrapper from file
		netcpu::message::fstream_to_wrapper(bulk_file, wrapper);

		// parse the wrapper into RAM-endianness correct message object
		typename Model::bulk_msg_type bulk_msg;
		bulk_msg.from_wire(wrapper);
		censoc::llog() << "read bulk message:\n";
		bulk_msg.print();

		// also copy the compressed data for re-sending to processing peers
		bulk_msg_wire.resize(wrapper.size());
		::memcpy(bulk_msg_wire.data(), wrapper.head(), wrapper.size());

		coefficients_metadata.reset(coefficients_size = bulk_msg.coeffs.size());

		server_state_sync_msg.coeffs.resize(coefficients_size);

		// start from the previosuly calculated position... if present (NO NETWORK-SERIALIZED BYTE ORDER IS KEPT, it's just a temp cache for native arch-at-a-time of deployment)
		::std::string const convergence_state_filepath(netcpu::root_path + netcpu::pending_tasks.front()->name() + "/convergence_state.msg");
		converger_1::message::convergence_state<N, F> convergence_state_msg;
		if (::boost::filesystem::exists(convergence_state_filepath) == true) {
			::std::ifstream convergence_state_file(convergence_state_filepath.c_str(), ::std::ios::binary);
			netcpu::message::fstream_to_wrapper(convergence_state_file, wrapper);
			convergence_state_msg.from_wire(wrapper);
		}

		if (convergence_state_msg.coeffs.size()) {
			e_min = netcpu::message::deserialise_from_decomposed_floating<float_type>(convergence_state_msg.value);
			am_bootstrapping = convergence_state_msg.am_bootstrapping() ? true : false;
			//assert(e_min != censoc::largish<float_type>());
			coefficients_rand_range_ended_wait = convergence_state_msg.coefficients_rand_range_ended_wait();
			intended_coeffs_at_once = convergence_state_msg.intended_coeffs_at_once();
		} else {
			e_min = censoc::largish<float_type>();
			am_bootstrapping = true;
			coefficients_rand_range_ended_wait = coefficients_size;
			assert(bulk_msg.coeffs.size());
			intended_coeffs_at_once = bulk_msg.coeffs(0).grid_resolutions.size();
		}

		offset.one();
		for (size_type i(0); i != coefficients_size; ++i) {

			converger_1::coefficient_metadata<N, F> & ram(coefficients_metadata[i]);
			converger_1::message::coeffwise_bulk<N, F> const & wire(bulk_msg.coeffs(i));

			assert(netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_from) < netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_to));

			ram.init(netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_from), netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_to), 
#if 0
					netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.clamp_from), netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.clamp_to), 
#endif
					netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.rand_range_end), offset);

			if (convergence_state_msg.coeffs.size()) {
				converger_1::message::coeffwise_server_state_sync<N, F> const & wire(convergence_state_msg.coeffs(i));
				ram.reset_from_convergence_state(wire.value(), wire.range_ended() ? true : false, netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_f), netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.value_from), netcpu::message::deserialise_from_decomposed_floating<float_type>(wire.rand_range), wire.grid_resolutions);
			} else {
				ram.reset_before_bootstrapping(wire.grid_resolutions);
			}

			coefficient_to_server_state_sync_msg(i);

			// todo -- think about this some more... as this may no longer apply -- may be deprecate later...
#if 0
			// NOTE/TODO cludge: setting value to max -- thereby guaranteeing that if fpu state is bad on the processor side, then 'cached' values for the main cache will never be saved during bootstrapping (as value_modified will always be true); and moreover guaranteeing that any subsequent sync shall have valid starting points where fpu state is ok (thereby allowing implicit caching of the 'main' caches on the processor side -- as opposed to having to reset to 'max' the cached values when/if help_matrix calculation yields Nan et al for the 'now_values' coeffs). Of course, the probability of 'value_rand' at processors side returning the same value as the now_value is extremely small, but this appoach will make sure. An alternative way could also be 'do value_rand while value_modified == false' but it will cause more comparisons etc during the general runtime of the model, whereas initialising to 'max' is only affecting the bootstrapping stage.
			// Overall, this approach may influence some 'assert' behaviour but it's worth it...
			if (!convergence_state_msg.coeffs.size())
				//server_state_sync_msg.coeffs(i).value(censoc::largish<float_type>());
				server_state_sync_msg.coeffs(i).value(-1);
#endif
		}
		assert(offset.size());

		// initialise complexities
		size_type init_complexity_size(meta_msg.complexity_size());
		assert(init_complexity_size);
		size_type init_coeffs_atonce_size(intended_coeffs_at_once);
		assert(init_coeffs_atonce_size);
		combos_modem.build(coefficients_size, init_complexity_size, init_coeffs_atonce_size, coefficients_metadata);
		assert(init_complexity_size);
		assert(init_coeffs_atonce_size);
		assert(combos_modem.metadata().empty() == false);
		censoc::llog() << "resulting total complexity: [" << init_complexity_size << "]\n";
		censoc::llog() << "resulting total coeffs_atonce: [" << init_coeffs_atonce_size << "]\n";

		// load the zoom levels  from the bulk message
		assert(coefficients_metadata_x.empty() == true);
		for (size_type i(0); i != bulk_msg.coeffs_x.size(); ++i) {
			coefficients_metadata_x.push_back(::std::make_pair(static_cast<size_type>(bulk_msg.coeffs_x(i).coeffs_at_once()), ::std::vector< ::std::pair<float_type, ::std::vector<size_type> > >()));
			coefficients_metadata_x.back().second.resize(coefficients_size);
			for (size_type j(0); j != coefficients_size; ++j) {
				coefficients_metadata_x.back().second[j] = ::std::make_pair(netcpu::message::deserialise_from_decomposed_floating<float_type>(bulk_msg.coeffs_x(i).coeffs(j).threshold), ::std::vector<size_type>());
				size_type const coeffs_at_once_size(bulk_msg.coeffs_x(i).coeffs(j).grid_resolutions.size());
				coefficients_metadata_x.back().second[j].second.resize(coeffs_at_once_size);
				for (size_type k(0); k != coeffs_at_once_size; ++k)
					coefficients_metadata_x.back().second[j].second[k] = bulk_msg.coeffs_x(i).coeffs(j).grid_resolutions(k);
			}
		}

		assert(remaining_complexities.empty() == true);

		if (!convergence_state_msg.complexities.size()) {
			censoc::llog() << "combos-at-once size (incl one at a time): [" << combos_modem.metadata().size() << "]\n";
			current_complexity_level = combos_modem.metadata().begin();
			censoc::netcpu::range_utils::add(remaining_complexities, ::std::pair<size_type, size_type>(0, current_complexity_level->first));
		} else {
			current_complexity_level = combos_modem.metadata().find(convergence_state_msg.current_complexity_level_first());
			assert(current_complexity_level != combos_modem.metadata().end());
			::std::list< ::std::pair<size_type, size_type> > tmp_container;
			for (size_type i(0); i != convergence_state_msg.complexities.size(); ++i) {
				converger_1::message::complexitywise_element<N> const & wire(convergence_state_msg.complexities(i));
				//censoc::netcpu::range_utils::add(remaining_complexities, ::std::pair<size_type, size_type>(wire.complexity_begin(), wire.complexity_size()));
				tmp_container.push_back(::std::pair<size_type, size_type>(wire.complexity_begin(), wire.complexity_size()));
			}
			censoc::netcpu::range_utils::add(remaining_complexities, tmp_container);
		}

		if (convergence_state_msg.visited_places.size()) {
			censoc::llog() << "Loading visited places...\n";
			::std::list< ::std::pair<netcpu::big_uint<size_type>, size_type> > tmp_container;
			for (size_type i(0); i != convergence_state_msg.visited_places.size(); ++i) {
				converger_1::message::visited_place<N> const & wire(convergence_state_msg.visited_places(i));
				netcpu::big_uint<size_type> begin;
				if (wire.begin.size())
					begin.load(wire.begin.data(), wire.begin.size());
				else
					begin.zero();
				//censoc::netcpu::range_utils::add(visited_places, ::std::pair<netcpu::big_uint<size_type>, size_type>(begin, wire.size()));
				tmp_container.push_back(::std::pair<netcpu::big_uint<size_type>, size_type>(begin, wire.size()));
			}
			censoc::netcpu::range_utils::add(visited_places, tmp_container);
			censoc::llog() << "... done loading visited places\n";
		}

#ifndef NDEBUG
		if (convergence_state_msg.complexities.size()) {
			// make sure that all relevant visited places have already been subtracted from currently deployed remaining complexity
			for (typename complexities_type::const_iterator i(remaining_complexities.begin()); i != remaining_complexities.end(); ++i) {
				for (size_type j(0); j != i->second; ++j) {
					offset.zero();
					combos_modem.demodulate(j + i->first, coefficients_metadata, coefficients_size);
					assert(censoc::netcpu::range_utils::find(visited_places, offset) == false);
				}
			}
		}
#endif

		complete_server_state_sync_msg();

		server_state_sync_timer.timeout_callback(&task_processor::on_sync_timeout, this);

#if 1
		if (::clock_gettime(CLOCK_MONOTONIC, &start) == -1)
			throw ::std::runtime_error(xxx() << "clock_gettime(CLOCK_MONOTONIC...) failed: [" << ::strerror(errno) << "]");
#endif
	}

	~task_processor()
	{
		while (scoped_peers.empty() == false) 
			new converger_1::end_process_writer(scoped_peers.front()->io());
	}

	/**
		@precon -- 'bootstrapping_peer_report_msg' is already been parsed
		*/
	void
	on_bootstrapping_peer_report()
	{
		assert(::std::isfinite(netcpu::message::deserialise_from_decomposed_floating<float_type>(bootstrapping_peer_report_msg.value)) == true);
		assert(::std::isfinite(e_min) == true);
		//assert(e_min > bootstrapping_peer_report_msg.value()); 
		//assert(e_min == censoc::largish<float_type>()); 
		assert(am_bootstrapping == true);

		e_min = netcpu::message::deserialise_from_decomposed_floating<float_type>(bootstrapping_peer_report_msg.value);
		//assert(e_min != censoc::largish<float_type>()); 
		am_bootstrapping = false;

		censoc::lerr() << "bootstrapping to : " << e_min << ::std::endl;
		bootstrapping_peer_report_msg.print();

		assert(bootstrapping_peer_report_msg.coeffs.size() == coefficients_size); 
		offset.reset();
		offset.zero();
		for (size_type i(0); i != coefficients_size; ++i) {
			coefficients_metadata[i].reset_after_bootstrapping(bootstrapping_peer_report_msg.coeffs(i));
			coefficient_to_server_state_sync_msg(i);
		}
		::std::pair<netcpu::big_uint<size_type>, size_type> (offset, 1);
		assert(visited_places.empty() == true);
		visited_places.insert(::std::pair<netcpu::big_uint<size_type>, size_type> (offset, 1));


		assert(remaining_complexities.size() == 1);
		assert(combos_modem.metadata().begin() == current_complexity_level);
		assert(current_complexity_level->first == remaining_complexities.begin()->second);
		assert(!remaining_complexities.begin()->first);
		reduce_remaining_complexities(0, current_complexity_level->first);

		do_redistribute_remaining_peers_complexities();
		redistribute_remaining_complexities = false;

		complete_server_state_sync_msg();
		assert(server_state_sync_msg.coeffs.size() == coefficients_size);

		sync_peers_to_server_state();

		assert(processing_peers.empty() == false);

		//server_state_sync_timer.timeout(boost::posix_time::seconds(3)); // TODO make it a runtime param
		if (server_state_sync_timer.is_pending() == false)
			server_state_sync_timer.timeout(); 
	}

	/**
		@precon -- 'peer_report_msg' is already been parsed
		*/
	void
	on_peer_report(processing_peer<N, F, Model, ModelId> & reporting_peer)
	{
		assert(am_bootstrapping == false);
		assert(::std::isfinite(netcpu::message::deserialise_from_decomposed_floating<float_type>(peer_report_msg.value)) == true);
		assert(::std::isfinite(e_min) == true);
		bool immediate_on_sync_timeout(false);
		if (peer_report_msg.value_complexity() != static_cast<typename netcpu::message::typepair<N>::wire>(-1)) { // ... one must trust processor anyway on a greater scale of things. TODO -- later will queue/cache all of the reported results and have a verification-process done by "trusted" processing peers (done from latest to earliest order -- like a stack really) and if any are seen as wrong, will 'resync' to the best latest correct set of coeffs... TODO for future!
			censoc::lerr() << "e_min/peer_report_msg.value(): " << e_min / netcpu::message::deserialise_from_decomposed_floating<float_type>(peer_report_msg.value) << ::std::endl;
			censoc::lerr() << "e_min: " << e_min << ", peer_report_msg.value() " << netcpu::message::deserialise_from_decomposed_floating<float_type>(peer_report_msg.value) << ::std::endl;

			immediate_on_sync_timeout = true;

			e_min = netcpu::message::deserialise_from_decomposed_floating<float_type>(peer_report_msg.value);
			e_min_complexity = peer_report_msg.value_complexity();
			assert(e_min_complexity != static_cast<typename netcpu::message::typepair<N>::wire>(-1));
#ifndef NDEBUG
			offset.zero();
			combos_modem.demodulate(e_min_complexity, coefficients_metadata, coefficients_size);
			censoc::llog() << '{';
			for (size_type i(0); i != coefficients_size; ++i) 
				censoc::llog() << coefficients_metadata[i].value() << (i != coefficients_size - 1 ? ", " : "");
			censoc::llog() << "}\n";
#endif

			redistribute_remaining_complexities = true;

			better_find_since_last_recentered_sync =  true;

			// a non-true (w.r.t. server_state_sync_msg coeffs values) e_min, but it is here so that if any intermediate (in-between syncs) peers get connected and get sent server_state message -- they will be able to have a quicker 'early exit' (i.e. not spend time finding "better" finds when server already knows about a better one)... although this may not be that much of use -- subsequent 'recentre' message will be sent asap anyway
			// server_state_sync_msg.value(e_min);

#if 1
			::timespec end;
			if (::clock_gettime(CLOCK_MONOTONIC, &end) == -1)
				throw ::std::runtime_error(xxx() << "clock_gettime(CLOCK_MONOTONIC...) failed: [" << ::strerror(errno) << "]");

			// note, technically speaking, if tv_nsec was an unsigned integral, the following 'end.tv_nsec - start.tv_nsec' may yield large positive 
			//	(i.e. small negative) number; and whilst it is perfectly ok for complement-2 notation when adding/dealing-with other integrals -- 
			// any intermediate casting to floating-point would be a disaster (large positive number would not then wrap-around back to normal values but would be treated as actually-large value). 
			// However, following the timspec specifications as per 'man clock_gettime' (in FreeBSD) and as per POSIX (opengroup) specs, 
			// one is relying on 'tv_nsec' being a signed number.
			float_type rv = end.tv_sec - converger_1::start.tv_sec + (end.tv_nsec - converger_1::start.tv_nsec) * static_cast<float_type>(1e-9);
			censoc::llog() << "ll processing duration secs: [" << rv  << "], mins: [" << rv / 60 << "], hrs: [" << rv / 3600 << "]; resulting value: [" << e_min << "]\n";
			peer_report_msg.print();
#endif
		}

		//  appropriate the complexities 
		offset.reset();
		for (size_type i(0); i != peer_report_msg.complexities.size(); ++i) {

			size_type const complexity_begin(peer_report_msg.complexities(i).complexity_begin());
			size_type const complexity_size(peer_report_msg.complexities(i).complexity_size());

			censoc::llog() << "reported complexity: " << complexity_begin << "," << complexity_size << ::std::endl;
			::std::pair<size_type, size_type> tmp_pair(complexity_begin, complexity_size);
			// @note -- it is still important to have this: otherwise a self-reported complexity will be sent back to the source peer when 'peer2peer' assisted complexities are processed in 'on_sync_timeout'
			reporting_peer.subtract_from_remaining_complexities(tmp_pair);
			//censoc::netcpu::range_utils::subtract(remaining_complexities, tmp_pair);
			censoc::netcpu::range_utils::add(accumulated_remaining_complexities_since_last_sync, tmp_pair);

			// store reported complexity into the whole model context (visited_places)
			for (size_type j(0); j != complexity_size; ++j) {
				offset.zero();
				combos_modem.demodulate(complexity_begin + j, coefficients_metadata, coefficients_size);
				::std::pair<netcpu::big_uint<size_type>, size_type> tmp_pair(offset, 1);
				censoc::netcpu::range_utils::add(accumulated_visited_places_since_last_sync, tmp_pair);
			}

		}

		// this is a 'to be deprecated' diagnostic (as currently this line is the longest visible on screen -- yeah I know, not a good reason, it's just a quick hack, TODO -- delete it altogether)!!!
		censoc::llog() << "players: " << processing_peers.size() << '\n';

		if (immediate_on_sync_timeout == true)
			on_sync_timeout();
		else if (server_state_sync_timer.is_pending() == false) 
			server_state_sync_timer.timeout(boost::posix_time::seconds(3));

	}

	void
	coefficient_to_server_state_sync_msg(size_paramtype i)
	{
		coefficient_metadata<N, F> & ram(coefficients_metadata[i]);
		converger_1::message::coeffwise_server_state_sync<N, F> & wire(server_state_sync_msg.coeffs(i));

		assert(ram.assert_values() == true);
		wire.value(ram.saved_index()); 
		wire.range_ended(ram.range_ended() == true ? 1 : 0);
		netcpu::message::serialise_to_decomposed_floating(ram.saved_value(), wire.value_f);
		netcpu::message::serialise_to_decomposed_floating(ram.value_from(), wire.value_from);
		netcpu::message::serialise_to_decomposed_floating(ram.rand_range(), wire.rand_range);

		size_type const grid_resolutions_size(ram.grid_resolutions.size());
		wire.grid_resolutions.resize(grid_resolutions_size);
		for (size_type i(0); i != grid_resolutions_size; ++i)
			wire.grid_resolutions(i, ram.grid_resolutions[i]);

	}

	void
	complete_server_state_sync_msg()
	{
		netcpu::message::serialise_to_decomposed_floating(e_min, server_state_sync_msg.value);
		server_state_sync_msg.am_bootstrapping(am_bootstrapping == true ? 1 : 0);
		++io_key;
		server_state_sync_msg.echo_status_key(io_key);
		server_recentre_only_sync_msg.echo_status_key(io_key);
	}

	typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator
	processing_peers_insert(converger_1::processing_peer<N, F, Model, ModelId> & x)
	{
		x.io_key_echo(io_key); // it is ok to be the same -- by design the processing peer shall not send any reports until after the 1st sync to server state (if, on the other hand, such peer does not conform to the design, then one'll have a much greater issue: namely how to handle the incorrectly-behaving peers).
		x.sync_peer_to_server(server_state_sync_msg);

		redistribute_remaining_complexities = true;

		//if (e_min != censoc::largish<float_type>() && processing_peers.empty() == true && server_state_sync_timer.is_pending() == false)
		if (am_bootstrapping == false && server_state_sync_timer.is_pending() == false)
			server_state_sync_timer.timeout(boost::posix_time::seconds(3)); // TODO -- make the timeout a  runtime parameter

		return processing_peers.insert(processing_peers.end(), &x);
	}

	void
	do_redistribute_remaining_peers_complexities()
	{
		// will iterate first to find out the total remaining complexity size...
		// NOTE -- instead of harvesting the total remaining size via loop iteration -- one could, theoretically, specialize 'second type' for pair to have a dtor which will decrement (and ctor which will increment) the commonly-referenced 'total remaining complexity value' variable. But it ought to be kept in mind that the act of inc/dec of such variable will occur more frequently (once per peer_report with done complexity ranges) as opposed to once-per-sync-only harvesting. Also, albeit not statistically often, the act of harvesting will only impose 'inc' (aside of loop iteration of course), whilst act of modifying commonly-referenced variable may cause additional 'inc' calls as well.  
		size_type total_remaining_complexity_size(0);
		typename complexities_type::const_iterator i(remaining_complexities.begin());
		for (; i != remaining_complexities.end(); ++i) {
			assert(i->second);
			total_remaining_complexity_size += i->second;
		}

		// allocate complexities to processing peers...
		/*
			@note -- it is OK to have left-over complexities which have not been allocated to some processing peer. This is because, currently, upon any peers completion of its complexities the 'redistribute_remaining_complexities' flag will be set and things will get recalculated anyway -- eventually leading to all remaining complexities being done.
		*/

		size_type const raw_per_peer_complexity_size(total_remaining_complexity_size / processing_peers.size());  // just a hint
		size_type const per_peer_complexity_size(raw_per_peer_complexity_size ? raw_per_peer_complexity_size : 1); // do not want to add 0-sized complexities :-)

		i = remaining_complexities.begin();
		size_type avail_i_complexity(i->second);
		size_type avail_peer_complexity(per_peer_complexity_size);

		typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator j(processing_peers.begin());
		(*j)->clear_remaining_complexities();
		for (typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator j(processing_peers.begin());;) { 

			assert(i != remaining_complexities.end() || avail_i_complexity);
			assert(i->second);
			assert(avail_peer_complexity);
			assert(avail_i_complexity);

			size_type const chunk(::std::min(avail_peer_complexity, avail_i_complexity));

			(*j)->add_to_remaining_complexities(::std::make_pair(i->first + i->second - avail_i_complexity, chunk));

			if (!(avail_i_complexity -= chunk)) {
				// re-service the same job(s) to different proecssing peers ...
				if (++i == remaining_complexities.end()) {
					typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator k(j);
					++k;
					if (k != processing_peers.end()) // ... if there are 'other peers' remaining that is.
						i = remaining_complexities.begin();
					else
						break;
				}
				avail_i_complexity = i->second;
			}

			if (!(avail_peer_complexity -= chunk)) { 
				if (++j == processing_peers.end())
					break;
				(*j)->clear_remaining_complexities();
				avail_peer_complexity = per_peer_complexity_size;
			}
		}
	}

	void
	reduce_remaining_complexities(size_paramtype begin, size_paramtype end)
	{
		for (size_type i(begin); i != end; ++i) {
			offset.zero();
			combos_modem.demodulate(i, coefficients_metadata, coefficients_size);
			if (censoc::netcpu::range_utils::find(visited_places, offset) == true) {
				censoc::netcpu::range_utils::subtract(remaining_complexities, ::std::pair<size_type, size_type>(i, 1));
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
				assert(censoc::netcpu::range_utils::find(remaining_complexities, i) == false);
#endif
#endif
			}
		}
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
		{
			for (typename complexities_type::const_iterator i(remaining_complexities.begin()); i != remaining_complexities.end(); ++i) {
				for (size_type j(0); j != i->second; ++j) {
					offset.zero();
					combos_modem.demodulate(j + i->first, coefficients_metadata, coefficients_size);
					assert(censoc::netcpu::range_utils::find(visited_places, offset) == false);
				}
			}
		}
#endif
#endif
	}
	

	/** 
		@note to be called by timer at certain intervals
		*/
	struct postcdfinvn {
		vector_column_type normal;
		template <typename TMP>
		void inline 
		hook(TMP const & x) 
		{
#ifndef NDEBUG
			for (N i(0); i != static_cast<N>(x.size()); ++i) {
				assert(x[i] < 10);
				assert(x[i] > -10);
			}
#endif
			normal = x;
		}
	};

	/**
		@post 'true' if ok, 'false' if nothing is left to be redistributed (e.g. visited places alaready have all)
		*/
	bool
	do_redistribute_remaining_complexities() 
	{
		while (remaining_complexities.empty() == true) { // current complexity level is done 
			size_type const pair_first(current_complexity_level->first);
			if (++current_complexity_level == combos_modem.metadata().end()) { // entirely all complexities have been done (from all levels) 
				return false;
				// no need to reduce remaining complexities -- on_bootstrapping_peer_report will do just that
			} else { // moving on to the next level whilst still centered on the current point
				censoc::netcpu::range_utils::add(remaining_complexities, ::std::make_pair(pair_first, current_complexity_level->first - pair_first));
				// scan visited places and subtract from remaining complexities (no need for the round-trip, as well as saving on 'visited_places' ram usage on both server and processors ends)
				reduce_remaining_complexities(pair_first, current_complexity_level->first);
			}
		} // some peer must have done it's complexities... or similar
		do_redistribute_remaining_peers_complexities();
		return true;
	}


	void
	on_sync_timeout()
	{
		/* ::std::clog.precision(5); */ censoc::llog() << "on_sync_timeout. e_min so far: [" << e_min << "], coeffs to wait on: [" << coefficients_rand_range_ended_wait << "], players: [" << processing_peers.size() << "]\n";

		if (processing_peers.empty() == true)
			return;

		if (accumulated_remaining_complexities_since_last_sync.empty() == false) {

			censoc::netcpu::range_utils::subtract(remaining_complexities, accumulated_remaining_complexities_since_last_sync);
#ifndef NDEBUG
			for (typename complexities_type::const_iterator i(accumulated_remaining_complexities_since_last_sync.begin()); i != accumulated_remaining_complexities_since_last_sync.end(); ++i) 
				for (size_type j(0); j != i->second; ++j) 
					assert(censoc::netcpu::range_utils::find(remaining_complexities, j + i->first) == false);
#endif
			if (remaining_complexities.empty() == true)
				redistribute_remaining_complexities = true;
			else {
				// loop through all peers and subtract again (it will set needed peer2peer_assisted_complexity_present flag)
				for (typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator i(processing_peers.begin()); i != processing_peers.end(); ++i) 
					(*i)->subtract_from_remaining_complexities(accumulated_remaining_complexities_since_last_sync);
			}
			accumulated_remaining_complexities_since_last_sync.clear();

			// print out remaining complexities
			censoc::llog() << "Remaining complexities start:\n";
			for (typename complexities_type::const_iterator i(remaining_complexities.begin()); i != remaining_complexities.end(); ++i) 
				censoc::llog() << i->first << "," << i->second << ' ';
			censoc::llog() << "\n... remaining complexities end\n";
		}

		if (accumulated_visited_places_since_last_sync.empty() == false) {
			censoc::netcpu::range_utils::add(visited_places, accumulated_visited_places_since_last_sync);
			accumulated_visited_places_since_last_sync.clear();
			censoc::llog() << "visited_places: " << visited_places.size() << '\n';
#ifndef NDEBUG
			{
				for (typename complexities_type::const_iterator i(remaining_complexities.begin()); i != remaining_complexities.end(); ++i) {
					for (size_type j(0); j != i->second; ++j) {
						offset.zero();
						combos_modem.demodulate(j + i->first, coefficients_metadata, coefficients_size);
						assert(censoc::netcpu::range_utils::find(visited_places, offset) == false);
					}
				}
			}
#endif
		}

		bool do_shrink(false);
		if (better_find_since_last_recentered_sync == false) { // 'better find took place' cases are taken care of later... (need to reduce complexities AFTER recentering)
			if (redistribute_remaining_complexities == true) { // ... who knows why -- could be many different causes... see below:
				if (do_redistribute_remaining_complexities() == false)
					do_shrink = true;
			}
		}


		// NOTE -- MUST COOK BOTH MESSAGES (recentre_only and the whole of state_sync) AT ONCE (they must reflect data that is in-sync w.r.t. each other) -- because sync_peer_to_server and on_write_sync_peer_to_server in processing_peers rely on this (a simple hack -- see on_write_sync_peer_to_server) !!!

		bool do_send_recentre_message(false);
		if (better_find_since_last_recentered_sync == true) {
			better_find_since_last_recentered_sync = false;

			assert(current_complexity_level != combos_modem.metadata().end());
			assert(current_complexity_level->second.size() == coefficients_size);

			netcpu::message::serialise_to_decomposed_floating(e_min, server_recentre_only_sync_msg.value);
			server_recentre_only_sync_msg.value_complexity(e_min_complexity);

			assert(e_min_complexity != static_cast<size_type>(-1));

			// because on_peer_report clobbers coeffs 'value' when converting complexity to index for 'visited_places' et al
			offset.zero();
			combos_modem.demodulate(e_min_complexity, coefficients_metadata, coefficients_size);
			for (size_type i(0); i != coefficients_size; ++i) {
				coefficients_metadata[i].value_save();
				coefficient_to_server_state_sync_msg(i);
			}
			e_min_complexity = static_cast<size_type>(-1);

			assert(redistribute_remaining_complexities == true);
			assert(current_complexity_level != combos_modem.metadata().end());

			remaining_complexities.clear();
			censoc::netcpu::range_utils::add(remaining_complexities, ::std::make_pair(static_cast<size_type>(0), (current_complexity_level = combos_modem.metadata().begin())->first));
			reduce_remaining_complexities(0, current_complexity_level->first);
			if (do_redistribute_remaining_complexities() == true) {
				do_redistribute_remaining_peers_complexities();
				do_send_recentre_message = true;
			} else
				do_shrink = true;
		} 

		if (do_send_recentre_message == true) {
			complete_server_state_sync_msg();
			sync_peers_to_server_recentre_only();
		} else if (do_shrink == true) {

			censoc::llog() << "on_sync_timeout: will recentre!\n";

			assert(better_find_since_last_recentered_sync == false);

			am_bootstrapping = true;
			float_type e_min_debug(e_min);
			e_min = censoc::largish<float_type>();

			offset.one();
			assert(offset.size());
			for (size_type i(0); i != coefficients_size; ++i)
				coefficients_metadata[i].pre_shrink(coefficients_rand_range_ended_wait, shrink_slowdown);

			// pseudo -- keep going through zoom levels and pick the latest acceptable one (if any)
			for (typename coefficients_metadata_x_type::const_iterator coefficients_metadata_x_i(coefficients_metadata_x.begin()); coefficients_metadata_x_i != coefficients_metadata_x.end(); ++coefficients_metadata_x_i) {
				::std::cout << "iterating through a level of resolutions... ";
				// is current level acceptable
				bool ok(true);
				for (size_type i(0); i != coefficients_size; ++i) {
					::std::cout << "[coeff(=" << i << "), range(=" << coefficients_metadata[i].rand_range() << "), threshold(=" << (*coefficients_metadata_x_i).second[i].first << ")] ";
					if (coefficients_metadata[i].rand_range() > (*coefficients_metadata_x_i).second[i].first) 
						ok = false;
					else
						coefficients_metadata[i].set_grid_resolutions((*coefficients_metadata_x_i).second[i].second);
				}
				::std::cout << "ok(=" << ok << ")\n";
				if (ok == true)
					intended_coeffs_at_once = (*coefficients_metadata_x_i).first;
			}

			assert(intended_coeffs_at_once);

			for (size_type i(0); i != coefficients_size; ++i) {
				coefficients_metadata[i].post_shrink(
						//coefficients_rand_range_ended_wait, 
						intended_coeffs_at_once);
				coefficient_to_server_state_sync_msg(i);
			}

			assert(offset.size());

			if (!coefficients_rand_range_ended_wait) {
				save_convergence_state();
				float_type tmp_max(::boost::numeric::bounds<float_type>::lowest());
				for (size_type i(0); i != coefficients_size - 1; ++i)
				//for (size_type i(0); i != coefficients_size; ++i)
					if (tmp_max < coefficients_metadata[i].saved_value())
						tmp_max = coefficients_metadata[i].saved_value();
				assert(tmp_max != 0);
				//censoc::llog() 
				::std::cout
					<< "TODO -- WRITE DONE CRITERIA -- CURRENT RUN OF CONVERGENCE HAS ENDED!, e_min: " << e_min_debug << '\n';
				//censoc::llog() 
				::std::cout
					<< "raw coeffs are:\n";
				for (size_type i(0); i != coefficients_size; ++i) {
					//censoc::llog() 
					::std::cout
						<< coefficients_metadata[i].saved_value() << " ";
				} 
				//censoc::llog() 
				::std::cout
					<< "\n normalised coeffs are:\n";
				for (size_type i(0); i != coefficients_size; ++i) {
					//censoc::llog() 
					::std::cout
						<< coefficients_metadata[i].saved_value() / tmp_max << " ";
				} 
				//::std::cout << coefficients_metadata[coefficients_size - 1].saved_value() << " ";
				//censoc::llog() 
				::std::cout
					<< ::std::endl;

				//assert(0);
				netcpu::pending_tasks.front()->markcompleted();
				return;
			}


			size_type init_coeffs_atonce_size(intended_coeffs_at_once);
			assert(init_coeffs_atonce_size);
			size_type init_complexity_size(meta_msg.complexity_size());
			assert(init_complexity_size);
			combos_modem.build(coefficients_size, init_complexity_size, init_coeffs_atonce_size, coefficients_metadata);

			current_complexity_level = combos_modem.metadata().begin();
			censoc::netcpu::range_utils::add(remaining_complexities, ::std::make_pair(static_cast<size_type>(0), current_complexity_level->first));
			assert(current_complexity_level->first == remaining_complexities.begin()->second);
			assert(current_complexity_level != combos_modem.metadata().end());
			assert(current_complexity_level->second.size() == coefficients_size);
			// no need to reduce remaining complexities -- on_bootstrapping_peer_report will do just that

			// NOTE -- currently visited places do not map onto the 'shrunken' grid (possible todo for future -- rescale/remap from currently visited places onto the newly shrunk grid any of the still-relevant visited places). would involve looping through all stored visited places and then seeing if their coordinates are no more than 1 step away from current mid point... then will also have to remember to NOT clear the 'visited_places' before calling the 'complete_server_state_sync_msg'... but currently will need to clear it exactly before calling 'complete_xxx' code
			visited_places.clear();

			complete_server_state_sync_msg();
			sync_peers_to_server_state();

			return;

		} else if (redistribute_remaining_complexities == true) 
			sync_peers_to_server_complexity_only();
		else if (peer2peer_assisted_complexity_present == true) {

			/** @note -- how will peer know that subtracted range is from itself or not?
				answer -- still retain 'on_peer_report' clearing the peer (only don't set the flag there)
				then additionally, call subtract on all peers for the whole range again
			*/
			server_complexity_only_sync_msg.noreply(!0);
			censoc::llog() << "about to write peer2peer-assisted complexity only sync msg\n";
			for (typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator i(processing_peers.begin()); i != processing_peers.end(); ++i) 
				if ((*i)->peer2peer_assisted_complexity_present == true) 
					(*i)->sync_peer_to_server(server_complexity_only_sync_msg);
			server_complexity_only_sync_msg.noreply(0);
			//@note -- one could think that the 'noreply' field could be added to server_state_sync message as well (in case of 'write_more' flag issuing a delayed write in a form of the whole server state sync); BUT it is wrong -- because the aforementioned 'server state sync' message could be a delayed write to 'sync_peers_to_server_complexity_only' when reply IS needed (i.e. whilst io_key has not changed, the replies are needed due to something like some peers reconnecting, etc.) -- considering that there could be 'sync complexities with reply' followed by 'sync complexities without reply' sequence of events before anything gets written out...
			//@note -- also, keep in mind that really, the occasional reply from peer (when not needed) will not really break anything; and, in fact, will be of some use as the server can update its 'rollback'/'recovery' condition to a more recent state.
		}

		peer2peer_assisted_complexity_present = redistribute_remaining_complexities = false;

		// for writing to disk... (a simple hack for the time-being)

		size_type static write_to_disk_wait(3);
		if (!--write_to_disk_wait) {
			write_to_disk_wait = 7;
			save_convergence_state();
		}

#if 0
		// call timer again...
		if (server_state_sync_timer.is_pending() == false) // could have been called from 'on_peer_report' immediately (not via timer) -- so the timer is still pending/going independently...
			server_state_sync_timer.timeout(boost::posix_time::seconds(3)); // TODO make it a runtime param
#endif
	}

	void
	save_convergence_state()
	{

		censoc::llog() << "Writing convergence state to disk...\n";

		// build the message
		converger_1::message::convergence_state<N, F> convergence_state_msg;
		convergence_state_msg.coeffs.resize(coefficients_size);
		for (size_type i(0); i != coefficients_size; ++i) {
			coefficient_metadata<N, F> const & ram(coefficients_metadata[i]);
			converger_1::message::coeffwise_server_state_sync<N, F> & wire(convergence_state_msg.coeffs(i));
			wire.value(ram.saved_index());
			wire.range_ended(ram.range_ended() == true ? 1 : 0);
			netcpu::message::serialise_to_decomposed_floating(ram.saved_value(), wire.value_f);
			netcpu::message::serialise_to_decomposed_floating(ram.value_from(), wire.value_from);
			netcpu::message::serialise_to_decomposed_floating(ram.rand_range(), wire.rand_range);
			size_type const grid_resolutions_size(ram.grid_resolutions.size());
			wire.grid_resolutions.resize(grid_resolutions_size);
			for (size_type i(0); i != grid_resolutions_size; ++i)
				wire.grid_resolutions(i, ram.grid_resolutions[i]);
		}
		netcpu::message::serialise_to_decomposed_floating(e_min, convergence_state_msg.value);
		convergence_state_msg.am_bootstrapping(am_bootstrapping == true ? 1 : 0);
		convergence_state_msg.intended_coeffs_at_once(intended_coeffs_at_once);
		convergence_state_msg.coefficients_rand_range_ended_wait(coefficients_rand_range_ended_wait);
		assert(current_complexity_level != combos_modem.metadata().end());
		convergence_state_msg.current_complexity_level_first(current_complexity_level->first);
		convergence_state_msg.complexities.resize(remaining_complexities.size());
		size_type j(0);
		for (typename complexities_type::const_iterator i(remaining_complexities.begin()); i != remaining_complexities.end(); ++i) {
			converger_1::message::complexitywise_element<N> & wire(convergence_state_msg.complexities(j++));
			wire.complexity_begin(i->first);
			wire.complexity_size(i->second);
		}

		convergence_state_msg.visited_places.resize(visited_places.size());
		j = 0;
		for (typename ::std::map<netcpu::big_uint<size_type>, size_type>::const_iterator i(visited_places.begin()); i != visited_places.end(); ++i) {
			converger_1::message::visited_place<N> & wire(convergence_state_msg.visited_places(j++));
			//assert(i->first.size());
			wire.begin.resize(i->first.size());
			if (wire.begin.size())
				i->first.store(wire.begin.data());
			wire.size(i->second);
		}

		// serialise
		::std::string const convergence_state_filepath(netcpu::root_path + netcpu::pending_tasks.front()->name() + "/convergence_state.msg");
		{
			netcpu::message::write_wrapper write_raw; 
			convergence_state_msg.to_wire(write_raw);
			::std::ofstream convergence_state_file((convergence_state_filepath + ".tmp").c_str(), ::std::ios::binary | ::std::ios::trunc);
			convergence_state_file.write(reinterpret_cast<char const *>(write_raw.head()), write_raw.size());
			if (convergence_state_file == false)
				throw ::std::runtime_error("could not write " + convergence_state_filepath + ".tmp");
		}
		::rename((convergence_state_filepath + ".tmp").c_str(), convergence_state_filepath.c_str());

		censoc::llog() << "...done writing to disk\n";
	}

	void
	sync_peers_to_server_state()
	{
		censoc::llog() << "about to write whole state sync msg\n";
		// cycle and write-out to peers
		//{ // TODO -- WATCH OUT FOR PEERS DOING 'delete this' AND THEREBY DELETING THEMSELVES FROM THE ITERATED LOOP!!!
		// it should be reasonably safe to advance iterator (after saving current as tmp) -- for as long as any given peer will only 'delete this' and not cause the deletion of other peers in a 'chain reaction' type of thing.
		// altenatively -- make a guarantee that 'sync_peer_to_server' will not throw... }
		for (typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator i(processing_peers.begin()); i != processing_peers.end(); ++i) 
			(*i)->sync_peer_to_server(server_state_sync_msg);
	}

	void
	sync_peers_to_server_recentre_only()
	{
		censoc::llog() << "about to write recentre only sync msg\n";
		for (typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator i(processing_peers.begin()); i != processing_peers.end(); ++i) 
			(*i)->sync_peer_to_server(server_recentre_only_sync_msg);
	}

	void
	sync_peers_to_server_complexity_only()
	{
		censoc::llog() << "about to write complexity only sync msg\n";
		for (typename ::std::list<converger_1::processing_peer<N, F, Model, ModelId> *>::iterator i(processing_peers.begin()); i != processing_peers.end(); ++i) 
			(*i)->sync_peer_to_server(server_complexity_only_sync_msg);
	}

	void
	new_processing_peer(netcpu::message::async_driver &);

	// io-messages cached (for activating/communicating-with processing peers)
	converger_1::message::res res_msg;
	typename Model::meta_msg_type meta_msg;

	//typename Model::bulk_msg_type bulk_msg; // now just using the cached raw wrapper to save on re-compressing the same (unchanged) data whenever sending the bulk_msg to the newly connected processing peer (of which there could be many, e.g. > 100)
	::std::vector<uint8_t> bulk_msg_wire;

	converger_1::message::server_state_sync<N, F> server_state_sync_msg;
	converger_1::message::server_recentre_only_sync<N, F> server_recentre_only_sync_msg;
	converger_1::message::server_complexity_only_sync<N> server_complexity_only_sync_msg;
	converger_1::message::bootstrapping_peer_report<N, F> bootstrapping_peer_report_msg;
	converger_1::message::peer_report<N, F> peer_report_msg;

	key_type io_key;
};


template <typename N, typename F, typename Model, netcpu::models_ids::val ModelId>
struct processing_peer : netcpu::io_wrapper<netcpu::message::async_driver> {

	typedef censoc::lexicast< ::std::string> xxx;
	typedef typename converger_1::key_typepair::ram key_type;
	BOOST_STATIC_ASSERT(::boost::is_unsigned<key_type>::value);

	typedef typename netcpu::message::typepair<N>::ram size_type;

	// grid-related accounting
	typedef ::std::map<size_type, size_type> complexities_type;
	//


	struct scoped_peers_iterator_traits {

		typedef ::std::list<processing_peer *> container_type;
		typedef container_type & ctor_type;

		container_type & container_;
		scoped_peers_iterator_traits(ctor_type container)
		: container_(container) {
		}

		container_type &
		container() const
		{
			return container_;
		}
	};

	struct processing_peers_iterator_traits {

		typedef censoc::stl::fastsize< ::std::list<processing_peer *>, size_type> container_type;
		typedef container_type & ctor_type;

		container_type & container_;
		processing_peers_iterator_traits(ctor_type container)
		: container_(container) {
		}

		container_type &
		container() const
		{
			return container_;
		}
	};

	task_processor<N, F, Model, ModelId> & processor;
	censoc::scoped_membership_iterator<scoped_peers_iterator_traits> scoped_peers_i;
	censoc::scoped_membership_iterator<processing_peers_iterator_traits> processing_peers_i;

	key_type io_key_echo_;
	bool peer2peer_assisted_complexity_present; // used to indicate that other peer has done some of my own complexities (currently, as per design, no need to initialise in ctor; as user code in bootstrapping, recentering, do_recalculated_remaining_complexities, will set it) 

#ifndef NDEBUG
	bool do_assert_processing_peers_i;
#endif

	// grid-related accounting
	complexities_type remaining_complexities;

	processing_peer(task_processor<N, F, Model, ModelId> & processor, netcpu::message::async_driver & io_driver)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver), processor(processor), scoped_peers_i(processor.scoped_peers, processor.scoped_peers.insert(processor.scoped_peers.end(), this)), processing_peers_i(processor.processing_peers)
#ifndef NDEBUG
		, do_assert_processing_peers_i(false) 
#endif
	{
		netcpu::message::task_offer msg;
		msg.task_id(ModelId);
		io().write(msg, &processing_peer::on_write, this);
		censoc::llog() << "processing_peer ctor is done\n";
	}

	void
	io_key_echo(typename censoc::param<key_type>::type x)
	{
		io_key_echo_ = x;
	}

	template <typename T>
	void
	sync_peer_to_server(T & msg) throw() // danger... for the time-being
	{
		assert(do_assert_processing_peers_i == true && processing_peers_i.nulled() == false || !0);
		if (io().is_write_pending() == false) {
			msg.complexities.resize(remaining_complexities.size());
			size_type j(0);
			for (typename complexities_type::const_iterator i(remaining_complexities.begin()); i != remaining_complexities.end(); ++i) {
				converger_1::message::complexitywise_element<N> & complexitywise_element_msg(msg.complexities(j++));
				complexitywise_element_msg.complexity_begin(i->first);
				complexitywise_element_msg.complexity_size(i->second);
			}
			io().write(msg);
		} else 
			io().write_callback(&processing_peer::on_late_write_sync_peer_to_server, this);
	}

	void
	on_late_write_sync_peer_to_server()
	{
		io().write_callback(&processing_peer::on_write_sync_peer_to_server, this);
		assert(processing_peers_i.nulled() == false);
		censoc::llog() << "cathing up a peer to the latest state of the server (due to faster writes than network communication or client processing of messages)" << ::std::endl;
		sync_peer_to_server(processor.server_state_sync_msg); // running late -- catch up completely (do the whole state sync -- a simple hack for the time-being)
	}

	void
	on_write_sync_peer_to_server()
	{
		assert(processing_peers_i.nulled() == false);

		enum {stale_limit = 8192}; // todo -- make parametric
		BOOST_STATIC_ASSERT(::boost::integer_traits<key_type>::const_max > stale_limit); // to allow robustness against key_type wraparound (unlikely but this safety is compile-time -- free)

		if (processor.io_key - io_key_echo_ > stale_limit) 
			throw ::std::runtime_error(xxx("stale peer (not echoing fast enough): [") << io().socket.lowest_layer().remote_endpoint(netcpu::ec) << ']'); 
	}

	void 
	on_read() // for peer-reporting 
	{
		if (converger_1::message::bootstrapping_peer_report<N, F>::myid == io().read_raw.id()) {
			processor.bootstrapping_peer_report_msg.from_wire(io().read_raw);
			io_key_echo_ = processor.bootstrapping_peer_report_msg.echo_status_key();
			if (processor.io_key == io_key_echo_) 
				processor.on_bootstrapping_peer_report();
		} else if (converger_1::message::peer_report<N, F>::myid == io().read_raw.id()) {
			processor.peer_report_msg.from_wire(io().read_raw);
			io_key_echo_ = processor.peer_report_msg.echo_status_key();
			censoc::llog() << "received on_read in processing peer, my key: [" << processor.io_key << "], peer key: [" << io_key_echo_ << "]\n";
			if (processor.io_key == io_key_echo_) 
				processor.on_peer_report(*this);
		} else {
			io().cancel();
			return;
		}
		io().read();
	}

	/**
		@note -- as per design, it is called when 'do_recalculate_remaining_complexities' are being acted upon in the user code
		*/
	void
	clear_remaining_complexities()
	{
		peer2peer_assisted_complexity_present = false;
		remaining_complexities.clear();
	}

	void
	add_to_remaining_complexities(::std::pair<size_type, size_type> const & range)
	{
		remaining_complexities.insert(range);
	}

	void
	subtract_from_remaining_complexities(::std::pair<size_type, size_type> const & range)
	{
		censoc::netcpu::range_utils::subtract(remaining_complexities, range);
		if (remaining_complexities.empty() == true)
			processor.redistribute_remaining_complexities = true;
	}

	/**
		@pre -- call above version on each of the 'on_peer_report' first
		*/
	void
	subtract_from_remaining_complexities(complexities_type const & x)
	{
		assert(x.empty() == false); // cannot be a pair and cannot be empty
		peer2peer_assisted_complexity_present = censoc::netcpu::range_utils::subtract(remaining_complexities, x);
		if (peer2peer_assisted_complexity_present == true)
			processor.peer2peer_assisted_complexity_present = true;
		if (remaining_complexities.empty() == true)
			processor.redistribute_remaining_complexities = true;
	}

	void
	on_write()
	{
		if (io().is_read_pending() == false) // on_write can happen during new_processing_peer in server.cc, or upon the already-constructed 'ready-to-process' peer list (e.g. because there was no active task available during the initiall client's connection to the server).
			io().read(&processing_peer::on_read_offer_response, this);
		else
			io().read_callback(&processing_peer::on_read_offer_response, this);
	}

	void
	on_read_offer_response()
	{
		if (netcpu::message::good::myid != io().read_raw.id())
			io().read(); // will 'pause' in limbo state until end_process_writer shoots the message
		else {
			io().write(processor.res_msg, &processing_peer::on_write_res, this);
			censoc::llog() << "issued write res message command\n";
		}
	}

	void
	on_write_res()
	{
		io().read(&processing_peer::on_read_res_response, this);
	}

	void
	on_read_res_response() 
	{
		if (netcpu::message::good::myid != io().read_raw.id()) {
			censoc::llog() << "limboing\n";
			io().read(); // will 'pause' in limbo state until end_process_writer shoots the message
		} else {
			io().write(processor.meta_msg, &processing_peer::on_write_meta, this);
			censoc::llog() << "issued write meta message command\n";
		}
	}

	void
	on_write_meta()
	{
		io().read(&processing_peer::on_read_meta_response, this);
	}

	void
	on_read_meta_response()
	{
		if (netcpu::message::good::myid != io().read_raw.id()) 
			io().read(); // will 'pause' in limbo state until end_process_writer shoots the message
		else {
			io().write(processor.bulk_msg_wire, &processing_peer::on_write_bulk, this);
			censoc::llog() << "issued write bulk message command\n";
		}
	}

	void
	on_write_bulk()
	{
		// ready to process -- issue read/write biodir state... (don't forget to set flag for 'pending writes' from now on)
		// DO NOT throw on 'bad' message received from peer, just call 'delete this' instead.

		censoc::llog() << "in on_write_bulk, issued read\n";
		io().read(&processing_peer::on_read, this);
		io().write_callback(&processing_peer::on_write_sync_peer_to_server, this);

		processing_peers_i = processor.processing_peers_insert(*this);

#ifndef NDEBUG
		do_assert_processing_peers_i = true;
#endif
	}
};


template <typename N, typename F, typename Model, netcpu::models_ids::val ModelId>
void
task_processor<N, F, Model, ModelId>::new_processing_peer(netcpu::message::async_driver & io_driver)
{
	new processing_peer<N, F, Model, ModelId>(*this, io_driver);
}

/** @note -- the task may not be a processor because a given processor may occupy a bit of memory/resources (e.g. cached matricies, messages, etc.) -- and there may be many tasks in RAM... but there is/should only be 1 active (i.e. singleton-like) processor (i.e. only 1 active task ought to be present at any time).
	*/
template <typename N, typename F, typename Model, netcpu::models_ids::val ModelId>
struct task : netcpu::task {

	typedef F float_type;
	typedef typename netcpu::message::typepair<N>::ram size_type;

	::boost::scoped_ptr<converger_1::task_processor<N, F, Model, ModelId> > active_task_processor;

	task(::std::string const & name, time_t birthday = ::time(NULL))
	: netcpu::task(name, birthday) {
	}
	~task()
	{
		deactivate();
	}

	void 
	deactivate() 
	{
		active_task_processor.reset();
	}

	void 
	load_coefficients_info(netcpu::message::task_info & task)
	{
		::std::string const convergence_state_filepath(netcpu::root_path + name() + "/convergence_state.msg");
		converger_1::message::convergence_state<N, F> convergence_state_msg;
		if (::boost::filesystem::exists(convergence_state_filepath) == true) {
			::std::ifstream convergence_state_file(convergence_state_filepath.c_str(), ::std::ios::binary);
			netcpu::message::read_wrapper wrapper;
			netcpu::message::fstream_to_wrapper(convergence_state_file, wrapper);
			convergence_state_msg.from_wire(wrapper);

			assert(convergence_state_msg.coeffs.size());
			task.coefficients.resize(convergence_state_msg.coeffs.size());

			for (size_type i(0); i != convergence_state_msg.coeffs.size(); ++i) {
				netcpu::message::task_coefficient_info & to(task.coefficients(i));
				converger_1::message::coeffwise_server_state_sync<N, F> const & from(convergence_state_msg.coeffs(i));

				netcpu::message::serialise_to_decomposed_floating(netcpu::message::deserialise_from_decomposed_floating<float_type>(from.value_f), to.value);
				netcpu::message::serialise_to_decomposed_floating(netcpu::message::deserialise_from_decomposed_floating<float_type>(from.value_from), to.from);
				netcpu::message::serialise_to_decomposed_floating(netcpu::message::deserialise_from_decomposed_floating<float_type>(from.rand_range), to.range);
			}

		}
	}

	void activate() 
	{
		/* { NOTE -- what to do if reactivating and generic list of processing-ready peers already contains processors which have rejected current (this) task and are in the list because they are awaiting a new task?
			 The answer is easy -- just re-process those as if they were not the prior rejectors. This may appear wastefull but keeping in mind that a task is very likely to be deactivated AND another task is activated in it's place -- so the 'prior rejectors' list may well contain those that are no longer relevant to this particular task anyway.

			 Besides -- it's not like activate/deactivate is expected to be done frequently at all... indeed.
			 } */
		
		assert(netcpu::pending_tasks.empty() == false);
		assert(active_task_processor.get() == NULL);
		censoc::llog() << "converger_1's model factory is instantiating a task processor" << ::std::endl;
		active_task_processor.reset(new converger_1::task_processor<N, F, Model, ModelId>);

		// here loop through 'netcpu::ready_to_process_peers' and ask active_task_processor to 'new up' the right implementation for every one.

		// BUT do not iterate with iterator (newing up the peer will remove it from the list thus invalidating 'current' iterator)
		// a better way would be 'while (size()) front()' thingy
		// { WHY would in need to remove/invalidate from the list? -- see explanations in server.cc (got to do with ending current task whilst pending/lingering peer messages are being queued and then new model/taks getting stale messages from previous task) }

#ifndef NDEBUG
		size_type generic_peers_size(netcpu::ready_to_process_peers.size());
#endif

		while(netcpu::ready_to_process_peers.empty() == false) {
			active_task_processor->new_processing_peer(netcpu::ready_to_process_peers.front()->io());

			assert(--generic_peers_size == netcpu::ready_to_process_peers.size());
		}
	}

	void 
	new_processing_peer(netcpu::message::async_driver & io_driver)
	{
		assert(active() == true);
		assert(io_driver.is_read_pending() == false);
		active_task_processor->new_processing_peer(io_driver);
	}

		// todo -- write additional method -- assert(active() == true) and which will accept newly-ready-to-process generic peer and call on active task processor to 'new up' the right implementation.

	bool
	active() 
	{
		return active_task_processor.get() != NULL ? true : false;
	}
};

template <typename N, typename F, typename Model, netcpu::models_ids::val ModelId>
struct task_loader_detail : netcpu::io_wrapper<netcpu::message::async_driver> {
	typedef censoc::lexicast< ::std::string> xxx;

	converger_1::task<N, F, Model, ModelId> * t;

	task_loader_detail(netcpu::message::async_driver & io_driver)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver), 
	// generating name in a form "COUNTER_MODELID-INTRESxFLOATRES" e.g. "1_1-0x1" (TODO -- make it more elegant)
	t(new converger_1::task<N, F, Model, ModelId>(netcpu::generate_taskdir(::std::string(xxx(int(ModelId))) + '-' + ::std::string(xxx(int(converger_1::message::int_res<N>::value))) + 'x' + ::std::string(xxx(int(converger_1::message::float_res<F>::value)))))) {
		assert(t != NULL);

		::std::string const res_filepath(netcpu::root_path + t->name() + "/res.msg");
		{
			::std::ofstream res_file((res_filepath + ".tmp").c_str(), ::std::ios::binary | ::std::ios::trunc);
			res_file.write(reinterpret_cast<char const *>(io().read_raw.head()), io().read_raw.size());
			if (res_file == false)
				throw ::std::runtime_error("could not write " + res_filepath + ".tmp");
		}
		::rename((res_filepath + ".tmp").c_str(), res_filepath.c_str());

		::std::cerr << "ctor task loader detail\n";
		io().read(&task_loader_detail::on_read, this);
	}

	void
	on_read()
	{
		typedef typename Model::meta_msg_type meta_msg_type;

		if (meta_msg_type::myid != io().read_raw.id()) 
			throw ::std::runtime_error(xxx("wrong message: [") << io().read_raw.id() << "] want meta id: [" << static_cast<netcpu::message::id_type>(meta_msg_type::myid) << ']');

		// todo verify file and meta msg here

		::std::cerr << "on_read for meta msg in task loader in converger_1\n";

		::std::string const meta_filepath(netcpu::root_path + t->name() + "/meta.msg");
		{
			::std::ofstream meta_file((meta_filepath + ".tmp").c_str(), ::std::ios::binary | ::std::ios::trunc);
			meta_file.write(reinterpret_cast<char const *>(io().read_raw.head()), io().read_raw.size());
			if (meta_file == false)
				throw ::std::runtime_error("could not write " + meta_filepath + ".tmp");
		}
		::rename((meta_filepath + ".tmp").c_str(), meta_filepath .c_str());

		io().write(netcpu::message::good(), &task_loader_detail::on_meta_reply_write, this);
	}

	void
	on_meta_reply_write()
	{
		assert(io().is_read_pending() == false);
		io().read(&task_loader_detail::on_bulk_read, this);
	}

	void
	on_bulk_read()
	{

		censoc::llog() << "on_bulk_read\n";
		assert(t != NULL);

		typedef typename Model::bulk_msg_type bulk_msg_type;
		if (bulk_msg_type::myid != io().read_raw.id()) 
			throw ::std::runtime_error(xxx("wrong message: [") << io().read_raw.id() << "] want bulk id: [" << static_cast<netcpu::message::id_type>(bulk_msg_type::myid) << ']');

#if 0
		bulk_msg_type blah;
		censoc::llog() << "on_bulk_read blah ctored: " << io().read_raw.size() << "\n";
		blah.from_wire(io().read_raw);
#endif

		censoc::llog() << "received bulk message: " << io().read_raw.size() << ::std::endl;

		::std::string const bulk_filepath(netcpu::root_path + t->name() + "/bulk.msg");
		{
			::std::ofstream bulk_file((bulk_filepath + ".tmp").c_str(), ::std::ios::binary | ::std::ios::trunc);
			bulk_file.write(reinterpret_cast<char const *>(io().read_raw.head()), io().read_raw.size());
			if (bulk_file == false)
				throw ::std::runtime_error("could not write " + bulk_filepath + ".tmp");
		}
		::rename((bulk_filepath + ".tmp").c_str(), bulk_filepath.c_str());

		netcpu::message::new_taskname msg(t->name());
		io().write(msg, &task_loader_detail::on_bulk_response_write, this);

	}

	void
	on_bulk_response_write()
	{
		assert(io().is_read_pending() == false);
		t->markpending();
		censoc::llog() << "task stored -- the model-specific loader is done with this task\n";
		new netcpu::peer_connection(io());
	}

	~task_loader_detail() throw()
	{
		censoc::llog() << "dtor of task_loader_detail in converger_1: " << this << ::std::endl;
	}
};


// TODO!!! -- MUST re-use commonly used code (like this struct) w.r.t. processor.h (e.g. task_processor in processor.h)
template <template <typename, typename> class Model, netcpu::models_ids::val ModelId>
struct task_loader : netcpu::io_wrapper<netcpu::message::async_driver> {
	typedef censoc::lexicast< ::std::string> xxx;

	task_loader(netcpu::message::async_driver & io_driver)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io_driver) {
		io().write(netcpu::message::good(), &task_loader::on_write, this);
		censoc::llog() << "ctor in task_loader in converger_1: " << this << ::std::endl;
	}
	~task_loader() throw()
	{
		censoc::llog() << "dtor of task_loader in converger_1: " << this << ::std::endl;
	}

	void
	on_write()
	{
		censoc::llog() << "written some...\n";
		io().read(&task_loader::on_read, this);
		censoc::llog() << "issued async read...\n";
	}

	void
	on_read()
	{
		censoc::llog() << "read some...\n";
		if (converger_1::message::res::myid != io().read_raw.id()) 
			//delete this;
			throw ::std::runtime_error(xxx("wrong message: [") << io().read_raw.id() << "] want res id: [" << static_cast<netcpu::message::id_type>(converger_1::message::res::myid) << ']');

		converger_1::message::res msg(io().read_raw);
		censoc::llog() << "received res message\n";
		msg.print();

		switch (msg.int_res()) {
		case converger_1::message::int_res<uint32_t>::value :
			new_task_loader_detail<uint32_t>(msg);
		break;
		case converger_1::message::int_res<uint64_t>::value :
			new_task_loader_detail<uint64_t>(msg);
		break;
		censoc::llog() << "unsupported resolution: [" << msg.int_res() << ']';
		io().write(netcpu::message::bad(), &task_loader::on_bad_write, this);
		}
	}
	template <typename IntRes>
	void 
	new_task_loader_detail(converger_1::message::res const & msg)
	{
		if (msg.float_res() == converger_1::message::float_res<float>::value)
			new task_loader_detail<IntRes, float, Model<IntRes, float>, ModelId>(io());
		else if (msg.float_res() == converger_1::message::float_res<double>::value)
			new task_loader_detail<IntRes, double, Model<IntRes, double>, ModelId >(io());
		else // TODO -- may be as per 'on_read' -- write a bad message to client (more verbose)
			throw ::std::runtime_error(xxx("unsupported floating resolution: [") << msg.float_res() << ']');
	}
	void
	on_bad_write()
	{
		new netcpu::peer_connection(io());
	}
};

// this must be a very very small class (it will exist through the whole of the lifespan of the server)
template <template <typename, typename> class Model, netcpu::models_ids::val ModelId>
struct model_factory : netcpu::model_factory_base<ModelId> {
	typedef censoc::lexicast< ::std::string> xxx;

	void 
	new_task(netcpu::message::async_driver & io_driver, netcpu::message::task_offer & m)
	{
		assert(m.task_id() == ModelId);
		assert(io_driver.is_read_pending() == false);
		censoc::llog() << "model factory is instantiating a task loader" << ::std::endl;
		new converger_1::task_loader<Model, ModelId>(io_driver);
	}

	void
	new_task(::std::string const & name, bool pending, time_t birthday)
	{
		// expecting name in a form "COUNTER_MODELID-INTRESxFLOATRES" e.g. "1_1-0x1"
		::std::string::size_type int_res_pos(name.find_first_of('-'));
		if (int_res_pos == ::std::string::npos || name.size() <= ++int_res_pos)
			throw ::std::runtime_error(xxx("incorrect taskdir found: [") << name << ']' );

		::std::string::size_type float_res_pos(name.find_first_of('x'));
		if (float_res_pos == ::std::string::npos || name.size() <= ++float_res_pos)
			throw ::std::runtime_error(xxx("incorrect taskdir found: [") << name << ']' );

		int const int_res(censoc::lexicast<int>(name.substr(int_res_pos, float_res_pos - int_res_pos)));
		int const float_res(censoc::lexicast<int>(name.substr(float_res_pos)));

		::std::clog << "int res from name: " << int_res << ::std::endl;
		::std::clog << "float res from name: " << float_res << ::std::endl;

		if (int_res == converger_1::message::int_res<uint32_t>::value)
			new_task_detail<uint32_t>(float_res, name, pending, birthday);
		else if (int_res == converger_1::message::int_res<uint64_t>::value)
			new_task_detail<uint64_t>(float_res, name, pending, birthday);
		else
			throw ::std::runtime_error(xxx("unsupported int resolution: [") << int_res << ']');
	}

private:
	template <typename IntRes>
	void 
	new_task_detail(int float_res, ::std::string const & name, bool pending, time_t birthday)
	{
		netcpu::task * t;
		if (float_res == converger_1::message::float_res<float>::value) {
			t = new converger_1::task<IntRes, float, Model<IntRes, float>, ModelId>(name, birthday);
		} else if (float_res == converger_1::message::float_res<double>::value) {
			t = new converger_1::task<IntRes, double, Model<IntRes, double>, ModelId>(name, birthday);
		} else // TODO -- may be as per 'on_read' -- write a bad message to client (more verbose)
			throw ::std::runtime_error(xxx("unsupported floating resolution: [") << float_res << ']');
		// no need for scope or shared ptr -- queued_taks is self-managed (and externally managed by the pending_tasks list), moreover if any of the insertions throw in ctor of task -- then such will not be ctored/newed-up anyway

		if (pending == true)
			t->markpending();
		else 
			t->markcompleted();
	}
};

}}}

#endif
