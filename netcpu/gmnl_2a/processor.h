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

#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/rand.h>
#include <censoc/halton.h>
#include <censoc/rnd_mirror_grid.h>
#include <censoc/finite_math.h>
#include <censoc/sysinfo.h>
#include <censoc/exp_lookup.h>

#include <netcpu/converger_1/processor.h>

#include <netcpu/converger_1/message/bulk.h>

#include <netcpu/gmnl_2/message/meta.h>
#include <netcpu/gmnl_2/message/bulk.h>

//}
#ifndef CENSOC_NETCPU_GMNL_2a_PROCESSOR_H
#define CENSOC_NETCPU_GMNL_2a_PROCESSOR_H

// for the time-being model_factory_interface will be automatically declared, no need to include explicitly...
namespace censoc { namespace netcpu { namespace gmnl_2a { 

template <typename N, typename F>
struct halton_matrix_provider {

//{ typedefs...

	typedef F float_type;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	// quick hack
	struct ctor_type {
		float_type * sigma;
		float_type * nonsigma;
	};
	typedef typename censoc::param<ctor_type>::type ctor_paramtype;

//}

	halton_matrix_provider(ctor_paramtype ctor)
	: sigma_(ctor.sigma), nonsigma_(ctor.nonsigma){
	}

	float_type * 
	nonsigma() 
	{
		return nonsigma_;
	}

	float_type * 
	sigma() 
	{
		return sigma_;
	}

	float_type const * 
	nonsigma() const
	{
		return nonsigma_;
	}

	float_type const * 
	sigma() const
	{
		return sigma_;
	}

protected:

	float_type * sigma_;
	float_type * nonsigma_;
};

template <typename N, typename F, typename EF, bool approximate_exponents>
struct task_processor : censoc::exp_lookup::exponent_evaluation_choice<EF, typename netcpu::message::typepair<N>::ram, approximate_exponents> {

	BOOST_STATIC_ASSERT(censoc::DefaultAlign >= sizeof(F));
	BOOST_STATIC_ASSERT(!(censoc::DefaultAlign % sizeof(F)));
	BOOST_STATIC_ASSERT(censoc::DefaultAlign >= sizeof(EF));
	BOOST_STATIC_ASSERT(!(censoc::DefaultAlign % sizeof(EF)));
	BOOST_STATIC_ASSERT(sizeof(EF) >= sizeof(F));

	//{ typedefs...
	
	typedef netcpu::converger_1::message::meta<N, F, gmnl_2::message::meta<N> > meta_msg_type;
	typedef netcpu::converger_1::message::bulk<N, F, gmnl_2::message::bulk<N> > bulk_msg_type;

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef EF extended_float_type;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef censoc::lexicast< ::std::string> xxx;


	typedef censoc::rnd_mirror_grid<size_type, float_type, halton_matrix_provider<N, F> > haltons_type;
	//typedef censoc::halton<size_type, float_type, halton_matrix_provider<N, F> > haltons_type;

	typedef censoc::exp_lookup::exponent_evaluation_choice<EF, size_type, approximate_exponents> exponent_evaluation_choice_base;
  
	//}

	//{ data members

	size_type const max_alternatives;
	size_type const repetitions;
	float_type const accumulated_probability_log_repetitions_offset;
	size_type float_repetitions_column_stride_size;
	size_type extended_float_repetitions_column_stride_size;

	size_type const x_size;
	::std::vector<size_type> respondents_choice_sets;
	size_type const draws_sets_size;

	censoc::aligned_alloc<uint8_t> raw_data;

	float_type * sigma_bar_tau_cached_main;
	float_type * sigma_bar_tau_cached_tmp;

	float_type * phi_covariance_cached_main;
	float_type * phi_covariance_cached_tmp;

	float_type * phi_scaled_covariance_cached_main;
	float_type * phi_scaled_covariance_cached_tmp;
	float_type * phi_variance_cached_main;
	float_type * phi_variance_cached_tmp;

	float_type * complete_phi_mean_cached_main;
	float_type * complete_phi_mean_cached_tmp;
	float_type * complete_phi_variance_cached_main;
	float_type * complete_phi_variance_cached_tmp;
	float_type * complete_phi_covariance_cached_main;
	float_type * complete_phi_covariance_cached_tmp;

#ifndef NDEBUG
	uint8_t * debug_choice_sets_alternatives_end;
	float_type * debug_matrix_composite_end;
	uint8_t * debug_choice_column_end;
#endif
	::boost::scoped_array<uint8_t> choice_sets_alternatives;
	::boost::scoped_array<float_type> matrix_composite; 
	::boost::scoped_array<uint8_t> choice_column;


	float_type * matrix_haltons_sigma;
	float_type * matrix_haltons_nonsigma;
	float_type * phi_draws;

	extended_float_type * respondent_probability_cache;
	extended_float_type * repetitions_cache_rowwise_tmp;
	float_type * stage3_composite_equation_tau_cached; // not reusing the 'sigma_bar_tau_cached' ptr because composite equation (eta_cache et al) is one single cache (not main/tmp kind)
	float_type * stage3_composite_equation_phi_mean_cached; 
	float_type * stage3_composite_equation_phi_variance_cached; 
	float_type * stage3_composite_equation_phi_covariance_cached; 
	float_type * etavars_cache;
	float_type * betavars_cache;
	float_type * stage3_etavars_cache;
	float_type * sigma_cache;  
	float_type * sigma_cache_main;  // TODO -- make the main one a part of the global Map memory block
	float_type * sigma_cache_tmp; 

	float_type * phi_covariance_draws_cache;
	float_type * phi_covariance_draws_cache_main;
	float_type * phi_covariance_draws_cache_tmp;

	float_type * phi_scaled_draws_cache;
	float_type * phi_scaled_draws_cache_main;
	float_type * phi_scaled_draws_cache_tmp;

	float_type * complete_phi_draws_cache;
	float_type * complete_phi_draws_cache_main;
	float_type * complete_phi_draws_cache_tmp;

	float_type * eta_cache_x;
	float_type * eta_cache;
	float_type * ev_cache_tmp_or_censor_cache_tmp;
	extended_float_type * ev_cache; 

	bool reduce_exp_complexity;

	//}

	// TODO -- later deprecate message.h typedef size_type and pass it via the template arg!!!
	task_processor(meta_msg_type const & meta_msg)	
	: max_alternatives(meta_msg.model.dataset.max_alternatives()), repetitions(meta_msg.model.repetitions()), accumulated_probability_log_repetitions_offset(::std::log(static_cast<float_type>(repetitions))), x_size(meta_msg.model.dataset.x_size()), draws_sets_size(meta_msg.model.draws_sets_size()), 
#ifndef NDEBUG
	sigma_cache(NULL),
	phi_covariance_draws_cache(NULL),
	phi_scaled_draws_cache(NULL),
	complete_phi_draws_cache(NULL),
#endif
	reduce_exp_complexity(meta_msg.model.reduce_exp_complexity())
	{
		// default stride for majority of float_type vectorized data access
		size_type const float_stride_align_by((censoc::DefaultAlign / sizeof(float_type)));
		assert(repetitions > float_stride_align_by);
		size_type const float_repetitions_column_stride_size_modulo(repetitions % float_stride_align_by);
		assert(float_stride_align_by > float_repetitions_column_stride_size_modulo);
		size_type const repetitions_column_stride_size_extend_by(float_stride_align_by - float_repetitions_column_stride_size_modulo);
		if (float_repetitions_column_stride_size_modulo)
			float_repetitions_column_stride_size = repetitions + repetitions_column_stride_size_extend_by;
		else
			float_repetitions_column_stride_size = repetitions;

		size_type const extended_float_stride_align_by((censoc::DefaultAlign / sizeof(extended_float_type)));
		assert(repetitions > extended_float_stride_align_by);
		size_type const extended_float_repetitions_column_stride_size_modulo(repetitions % extended_float_stride_align_by);
		assert(extended_float_stride_align_by > extended_float_repetitions_column_stride_size_modulo);
		if (extended_float_repetitions_column_stride_size_modulo)
			extended_float_repetitions_column_stride_size = repetitions + extended_float_stride_align_by - extended_float_repetitions_column_stride_size_modulo;
		else
			extended_float_repetitions_column_stride_size = repetitions;

		//---
		respondent_probability_cache = static_cast<extended_float_type*>(0);
		//
		repetitions_cache_rowwise_tmp = respondent_probability_cache + extended_float_repetitions_column_stride_size;
		set_pointer(sigma_cache_main, repetitions_cache_rowwise_tmp + extended_float_repetitions_column_stride_size);
		sigma_cache_tmp = sigma_cache_main + float_repetitions_column_stride_size * draws_sets_size;
		phi_covariance_draws_cache_main = sigma_cache_tmp + float_repetitions_column_stride_size * draws_sets_size;
		phi_covariance_draws_cache_tmp = phi_covariance_draws_cache_main + float_repetitions_column_stride_size * draws_sets_size;
		phi_scaled_draws_cache_main = phi_covariance_draws_cache_tmp + float_repetitions_column_stride_size * draws_sets_size;
		phi_scaled_draws_cache_tmp = phi_scaled_draws_cache_main + float_repetitions_column_stride_size * draws_sets_size;
		complete_phi_draws_cache_main = phi_scaled_draws_cache_tmp + float_repetitions_column_stride_size * draws_sets_size;
		complete_phi_draws_cache_tmp = complete_phi_draws_cache_main + float_repetitions_column_stride_size * draws_sets_size;
		eta_cache_x = complete_phi_draws_cache_tmp + float_repetitions_column_stride_size * draws_sets_size;
		eta_cache = eta_cache_x + float_repetitions_column_stride_size * draws_sets_size * x_size;
		ev_cache_tmp_or_censor_cache_tmp = eta_cache + float_repetitions_column_stride_size * draws_sets_size * x_size;
		set_pointer(ev_cache, ev_cache_tmp_or_censor_cache_tmp + float_repetitions_column_stride_size * max_alternatives);
		set_pointer(matrix_haltons_nonsigma, ev_cache + extended_float_repetitions_column_stride_size);
		matrix_haltons_sigma = matrix_haltons_nonsigma + float_repetitions_column_stride_size * x_size * draws_sets_size;
		phi_draws = matrix_haltons_sigma + float_repetitions_column_stride_size * draws_sets_size;
		sigma_bar_tau_cached_main = phi_draws + float_repetitions_column_stride_size * draws_sets_size - (float_repetitions_column_stride_size_modulo ? repetitions_column_stride_size_extend_by : 0);
		sigma_bar_tau_cached_tmp = sigma_bar_tau_cached_main + draws_sets_size;
		phi_covariance_cached_main = sigma_bar_tau_cached_tmp + draws_sets_size;
		phi_covariance_cached_tmp = phi_covariance_cached_main + draws_sets_size;
		phi_scaled_covariance_cached_main = phi_covariance_cached_tmp + draws_sets_size;
		phi_scaled_covariance_cached_tmp = phi_scaled_covariance_cached_main + draws_sets_size;
		phi_variance_cached_main = phi_scaled_covariance_cached_tmp + draws_sets_size;
		phi_variance_cached_tmp = phi_variance_cached_main + draws_sets_size;
		complete_phi_mean_cached_main = phi_variance_cached_tmp + draws_sets_size;
		complete_phi_mean_cached_tmp = complete_phi_mean_cached_main + draws_sets_size;
		complete_phi_variance_cached_main = complete_phi_mean_cached_tmp + draws_sets_size;
		complete_phi_variance_cached_tmp = complete_phi_variance_cached_main + draws_sets_size;
		complete_phi_covariance_cached_main = complete_phi_variance_cached_tmp + draws_sets_size;
		complete_phi_covariance_cached_tmp = complete_phi_covariance_cached_main + draws_sets_size;
		stage3_composite_equation_tau_cached = complete_phi_covariance_cached_tmp + draws_sets_size;
		stage3_composite_equation_phi_mean_cached = stage3_composite_equation_tau_cached + x_size * draws_sets_size;
		stage3_composite_equation_phi_variance_cached = stage3_composite_equation_phi_mean_cached + x_size * draws_sets_size;
		stage3_composite_equation_phi_covariance_cached = stage3_composite_equation_phi_variance_cached + x_size * draws_sets_size;
		etavars_cache = stage3_composite_equation_phi_covariance_cached + x_size * draws_sets_size;
		betavars_cache = etavars_cache + x_size * draws_sets_size;
		stage3_etavars_cache = betavars_cache + x_size * draws_sets_size;
		//
		set_pointer(respondent_probability_cache, raw_data.alloc(uintptr_t(stage3_etavars_cache + x_size * draws_sets_size)));
		assert(respondent_probability_cache != NULL);
		assert(!((uintptr_t)respondent_probability_cache % censoc::DefaultAlign));
		//
		post_offset_pointer(repetitions_cache_rowwise_tmp);
		post_offset_pointer(sigma_cache_main);
		post_offset_pointer(sigma_cache_tmp);
		post_offset_pointer(phi_covariance_draws_cache_main);
		post_offset_pointer(phi_covariance_draws_cache_tmp);
		post_offset_pointer(phi_scaled_draws_cache_main);
		post_offset_pointer(phi_scaled_draws_cache_tmp);
		post_offset_pointer(complete_phi_draws_cache_main);
		post_offset_pointer(complete_phi_draws_cache_tmp);
		post_offset_pointer(eta_cache_x);
		post_offset_pointer(eta_cache);
		post_offset_pointer(ev_cache_tmp_or_censor_cache_tmp);
		post_offset_pointer(ev_cache);
		post_offset_pointer(matrix_haltons_nonsigma);
		post_offset_pointer(matrix_haltons_sigma);
		post_offset_pointer(phi_draws);
		post_offset_pointer(sigma_bar_tau_cached_main);
		post_offset_pointer(sigma_bar_tau_cached_tmp);
		post_offset_pointer(phi_covariance_cached_main);
		post_offset_pointer(phi_covariance_cached_tmp);
		post_offset_pointer(phi_scaled_covariance_cached_main);
		post_offset_pointer(phi_scaled_covariance_cached_tmp);
		post_offset_pointer(phi_variance_cached_main);
		post_offset_pointer(phi_variance_cached_tmp);
		post_offset_pointer(complete_phi_mean_cached_main);
		post_offset_pointer(complete_phi_mean_cached_tmp);
		post_offset_pointer(complete_phi_variance_cached_main);
		post_offset_pointer(complete_phi_variance_cached_tmp);
		post_offset_pointer(complete_phi_covariance_cached_main);
		post_offset_pointer(complete_phi_covariance_cached_tmp);
		post_offset_pointer(stage3_composite_equation_tau_cached);
		post_offset_pointer(stage3_composite_equation_phi_mean_cached);
		post_offset_pointer(stage3_composite_equation_phi_variance_cached);
		post_offset_pointer(stage3_composite_equation_phi_covariance_cached);
		post_offset_pointer(etavars_cache);
		post_offset_pointer(betavars_cache);
		post_offset_pointer(stage3_etavars_cache);
		//---

		invalidate_caches_1();

		typename halton_matrix_provider<N, F>::ctor_type haltons_ctor;

		// sigma and etas
		{
			haltons_ctor.sigma = matrix_haltons_sigma;
			haltons_ctor.nonsigma = matrix_haltons_nonsigma;
			haltons_type haltons(haltons_ctor);
			// TODO --  put "should sleep" test (for the time-being) during halton generation as well (and combos builder)!!!
			haltons.init(draws_sets_size, repetitions, x_size, float_repetitions_column_stride_size);
		}
		
		// phi
		{
			haltons_ctor.sigma = phi_draws;
			haltons_ctor.nonsigma = NULL;
			haltons_type haltons(haltons_ctor);
			// TODO --  put "should sleep" test (for the time-being) during halton generation as well (and combos builder)!!!
			haltons.init(draws_sets_size, repetitions, x_size, float_repetitions_column_stride_size);
		}

		censoc::llog() << "ctor in gmnl_2a::task_processor\n";
	}

private:
	template <typename D, typename S>
	void
	set_pointer(D*& dst, S*const src)
	{
		dst = (D*)src;
	}
	template <typename T>
	void
	post_offset_pointer(T*& x)
	{
		x = (T*)((uintptr_t)x + (uintptr_t)respondent_probability_cache);
	}
public:

	size_type
	get_coefficients_size() const noexcept
	{	
		return x_size * 2 + 4;
	}

	void 
	invalidate_caches_1()
	{
		// tmp-based testing: force recalculation of all caches:                                                                                                                   
		// NOTE -- this is ONLY working because of not required for alignment for such variables/caches (otherwise repetetions_stride size thingy must be taken into account)
		CENSOC_RESTRICTED_CONST_PTR(float_type, sigma_bar_tau_cached_main_, sigma_bar_tau_cached_main);
		CENSOC_RESTRICTED_CONST_PTR(float_type, sigma_bar_tau_cached_tmp_, sigma_bar_tau_cached_tmp);

		CENSOC_RESTRICTED_CONST_PTR(float_type, phi_covariance_cached_main_, phi_covariance_cached_main);
		CENSOC_RESTRICTED_CONST_PTR(float_type, phi_covariance_cached_tmp_, phi_covariance_cached_tmp);

		CENSOC_RESTRICTED_CONST_PTR(float_type, phi_scaled_covariance_cached_main_, phi_scaled_covariance_cached_main);
		CENSOC_RESTRICTED_CONST_PTR(float_type, phi_scaled_covariance_cached_tmp_, phi_scaled_covariance_cached_tmp);
		CENSOC_RESTRICTED_CONST_PTR(float_type, phi_variance_cached_main_, phi_variance_cached_main);
		CENSOC_RESTRICTED_CONST_PTR(float_type, phi_variance_cached_tmp_, phi_variance_cached_tmp);

		CENSOC_RESTRICTED_CONST_PTR(float_type, complete_phi_mean_cached_main_, complete_phi_mean_cached_main);
		CENSOC_RESTRICTED_CONST_PTR(float_type, complete_phi_mean_cached_tmp_, complete_phi_mean_cached_tmp);
		CENSOC_RESTRICTED_CONST_PTR(float_type, complete_phi_variance_cached_main_, complete_phi_variance_cached_main);
		CENSOC_RESTRICTED_CONST_PTR(float_type, complete_phi_variance_cached_tmp_, complete_phi_variance_cached_tmp);
		CENSOC_RESTRICTED_CONST_PTR(float_type, complete_phi_covariance_cached_main_, complete_phi_covariance_cached_main);
		CENSOC_RESTRICTED_CONST_PTR(float_type, complete_phi_covariance_cached_tmp_, complete_phi_covariance_cached_tmp);

		for (size_type i(0); i != draws_sets_size; ++i)
			sigma_bar_tau_cached_main_[i] =
			sigma_bar_tau_cached_tmp_[i] =

			phi_covariance_cached_main_[i] =
			phi_covariance_cached_tmp_[i] =

			phi_scaled_covariance_cached_main_[i] =
			phi_scaled_covariance_cached_tmp_[i] =
			phi_variance_cached_main_[i] =
			phi_variance_cached_tmp_[i] =

			complete_phi_mean_cached_main_[i] =
			complete_phi_mean_cached_tmp_[i] =
			complete_phi_variance_cached_main_[i] =
			complete_phi_variance_cached_tmp_[i] =
			complete_phi_covariance_cached_main_[i] =
			complete_phi_covariance_cached_tmp_[i] =

			censoc::largish<float_type>();

		CENSOC_RESTRICTED_CONST_PTR(float_type, stage3_composite_equation_tau_cached_, stage3_composite_equation_tau_cached);
		CENSOC_RESTRICTED_CONST_PTR(float_type, stage3_composite_equation_phi_mean_cached_, stage3_composite_equation_phi_mean_cached);
		CENSOC_RESTRICTED_CONST_PTR(float_type, stage3_composite_equation_phi_variance_cached_, stage3_composite_equation_phi_variance_cached);
		CENSOC_RESTRICTED_CONST_PTR(float_type, stage3_composite_equation_phi_covariance_cached_, stage3_composite_equation_phi_covariance_cached);
		CENSOC_RESTRICTED_CONST_PTR(float_type, etavars_cache_, etavars_cache);
		CENSOC_RESTRICTED_CONST_PTR(float_type, betavars_cache_, betavars_cache);
		CENSOC_RESTRICTED_CONST_PTR(float_type, stage3_etavars_cache_, stage3_etavars_cache);
		size_type const i_end(draws_sets_size * x_size);
		for (size_type i(0); i != i_end; ++i) 
			stage3_composite_equation_tau_cached_[i] =
			stage3_composite_equation_phi_mean_cached_[i] =
			stage3_composite_equation_phi_variance_cached_[i] =
			stage3_composite_equation_phi_covariance_cached_[i] =
			etavars_cache_[i] =
			betavars_cache_[i] =
			stage3_etavars_cache_[i] =
			censoc::largish<float_type>();
	}

	void
	on_bulk_read(bulk_msg_type const & bulk_msg)
	{
		assert(bulk_msg.coeffs.size() == get_coefficients_size());

		size_type const respondents_size(bulk_msg.model.dataset.respondents_choice_sets.size());
		respondents_choice_sets.reserve(respondents_size);
		for (unsigned i(0); i != respondents_size; ++i)
			respondents_choice_sets.push_back(bulk_msg.model.dataset.respondents_choice_sets(i));

		size_type const choice_sets_alternatives_size(bulk_msg.model.dataset.choice_sets_alternatives.size());
		choice_sets_alternatives.reset(new uint8_t[choice_sets_alternatives_size]);
		::memcpy(choice_sets_alternatives.get(), bulk_msg.model.dataset.choice_sets_alternatives.data(), choice_sets_alternatives_size);

		size_type const matrix_composite_size(bulk_msg.model.dataset.matrix_composite.size());
		matrix_composite.reset(new float_type[matrix_composite_size]);
		for (size_type i(0); i != matrix_composite_size; ++i)
			matrix_composite[i] = netcpu::message::deserialise_from_decomposed_floating<float_type>(bulk_msg.model.dataset.matrix_composite(i));

		size_type const choice_column_size(bulk_msg.model.dataset.choice_column.size());
		choice_column.reset(new uint8_t[choice_column_size]);
		for (size_type i(0); i != choice_column_size; ++i)
			choice_column[i] = bulk_msg.model.dataset.choice_column(i);

#ifndef NDEBUG
		{
		debug_choice_sets_alternatives_end = choice_sets_alternatives.get() + choice_sets_alternatives_size;
		assert(debug_choice_sets_alternatives_end > choice_sets_alternatives.get());
		debug_matrix_composite_end = matrix_composite.get() + matrix_composite_size;
		assert(debug_matrix_composite_end > matrix_composite.get());
		debug_choice_column_end = choice_column.get() + choice_column_size;
		assert(debug_choice_column_end > choice_column.get());
		size_type total_choicesets_from_respondents_choice_sets(0);
		for (size_type i(0); i != respondents_size; total_choicesets_from_respondents_choice_sets += respondents_choice_sets[i++]);
		assert(total_choicesets_from_respondents_choice_sets == choice_sets_alternatives_size);
		size_type total_alternatives_count(0);
		for (uint8_t * i(choice_sets_alternatives.get()); i != debug_choice_sets_alternatives_end; total_alternatives_count += *i, ++i);
		assert(total_alternatives_count * x_size == matrix_composite_size);
		}
#endif
	}


	// for the time-being not getting haltons from server, rather calculating directly to save on network b/w
	/** @note slightly verbose (thus not so optimised) implementation for the time-being
		@post shall not return value which evaluates to '::censoc::isfinite(x) == false' (i.e. it shall handle the cases of NAN/Inf and return 'censoc::largish<float_type>()' value (which, of coures is not really 'Inf' but is, indeed, '::censoc::isfinite(x) == true')
		*/

	typedef netcpu::converger_1::coefficient_metadata<N, F> coefficient_metadata_type;
	typedef typename censoc::param<coefficient_metadata_type>::type coefficient_metadata_paramtype;
	typedef censoc::array<coefficient_metadata_type, size_type> coefficients_metadata_type;
	typedef typename censoc::param<coefficients_metadata_type>::type coefficients_metadata_paramtype;

	void 
	log_likelihood(coefficients_metadata_paramtype coefficients, float_paramtype target, netcpu::converger_1::opaque_result<float_type> & result)
	{
		censoc::llog() << "ll start\n";

		assert(censoc::was_fpu_ok(&censoc::init_fpu_check_anchor));

		assert(result.is_valid() == false);

		coefficient_metadata_paramtype tau(coefficients[2 * x_size]);
		coefficient_metadata_paramtype phi_variance(coefficients[2 * x_size + 2]);
		coefficient_metadata_paramtype phi_covariance(coefficients[2 * x_size + 3]);

		// note -- theta is also accessed directly as an offset from 'coefficients' array

		// RATHER PRIMITIVE-BASIC caching only at this stage...

		float_type accumulated_probability(0); 

		uint8_t const * choice_sets_alternatives_ptr(choice_sets_alternatives.get());
		float_type const * matrix_composite_ptr(matrix_composite.get());
		uint8_t const * choice_column_ptr(choice_column.get());

		coefficient_metadata_type const * const etavar(coefficients + x_size);

		/* { NOTE -- reason for caches being recalculated on every loop iteration
		 TODO -- once again, a lot of the following caches could be allocated to take care of ALL respondents at once (thus not having to recalculate it at all). This way the only time recalculation would take place is when relevant coefficient is changed. YET -- I am not doing this a the moment (not for the vast majority of caches anyway). 
		 
		 This is mainly due to considerations for memory consumption -- having 1000 repsondents (not impossible thing to have), one would increase memory consumption also by a huuuuuuge factor indeed... and then there are 15 or so choicesets... 
		 
		 Keeping in mind that the wholel point is not to overtake many of the end-users computers (users and their apps take priority indeed)... 
		 
		 In future though, when users computers get more RAM -- will refactor and evolve this code...

		To this extent, the majority of thefollowing caches are not so much caches w.r.t. computed values, but rather to save on possible new/delete when matricies get re-assigned/re-set in eigen2

		The above statement is not really true -- the computations are re-used but only within a given loop iteration (i.e. not saved for future log_likelihood iterations).

		ALSO -- must consider effects of larger memory amounts being shuffled to/from RAM (not just cache) and speed-implications for that
		} */

		for (size_type i(0); i != respondents_choice_sets.size(); ++i) {

			size_type const i_modulo(i % draws_sets_size);
			size_type const eta_cache_respondent_column_begin(i_modulo * x_size);

#ifdef __WIN32__
			if (!(i % 100) && netcpu::should_i_sleep == true) 
				return result.flag_incomplete_processing();
#endif

			assert(tau.value() != censoc::largish<float_type>());
			if (sigma_bar_tau_cached_main[i_modulo] == tau.value())
				sigma_cache = sigma_cache_main;
			else if (sigma_bar_tau_cached_tmp[i_modulo] == tau.value())
				sigma_cache = sigma_cache_tmp;
			else {
				float_type * sigma_bar_tau_cached;
				if (tau.value_modified() == false) {
					sigma_bar_tau_cached = sigma_bar_tau_cached_main;
					sigma_cache = sigma_cache_main;
				} else {
					sigma_bar_tau_cached = sigma_bar_tau_cached_tmp;
					sigma_cache = sigma_cache_tmp;
				}

				{
					// calculate scaled distribution draws
					float_type const tau_value(tau.value());
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, matrix_haltons_sigma_, matrix_haltons_sigma + i_modulo * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, sigma_cache_, sigma_cache + i_modulo * float_repetitions_column_stride_size);
					for (size_type r(0); r != repetitions; ++r) {
						float_type const tmp(tau_value * matrix_haltons_sigma_[r]);
						sigma_cache_[r] = tmp < 0 ? 1 / (1 - tmp) : tmp + 1;
					}

					// experimental mean-normalisation
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(float_type, accumulator, 0);
					for (size_type r(0); r != repetitions; ++r)
						*accumulator += sigma_cache_[r];
					assert(*accumulator != 0);
					*accumulator = repetitions / *accumulator;
					for (size_type r(0); r != repetitions; ++r)
						sigma_cache_[r] *= *accumulator;
				}

				if (censoc::was_fpu_ok(sigma_cache, &censoc::init_fpu_check_anchor) == false) { // No point in caching -- otherwise may reuse it detrementally on subsequent iteration(s) [when such may not have fpu exceptions flag set then)
					sigma_bar_tau_cached[i_modulo] = censoc::largish<float_type>();
					return;
				} else
					sigma_bar_tau_cached[i_modulo] = tau.value();
			}

			assert(phi_covariance.value() != censoc::largish<float_type>());
			if (phi_covariance_cached_main[i_modulo] == phi_covariance.value())
				phi_covariance_draws_cache = phi_covariance_draws_cache_main;
			else if (phi_covariance_cached_tmp[i_modulo] == phi_covariance.value())
				phi_covariance_draws_cache = phi_covariance_draws_cache_tmp;
			else {
				float_type * phi_covariance_cached;
				if (phi_covariance.value_modified() == false) {
					phi_covariance_cached = phi_covariance_cached_main;
					phi_covariance_draws_cache = phi_covariance_draws_cache_main;
				} else {
					phi_covariance_cached = phi_covariance_cached_tmp;
					phi_covariance_draws_cache = phi_covariance_draws_cache_tmp;
				}

				{
					// calculate covariate distribution draws
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(float_type const, sigma_portion_value, phi_covariance.value());
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(float_type const, phi_portion_value, 1 - ::std::abs(phi_covariance.value()));
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, sigma_draws_, matrix_haltons_sigma + i_modulo * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, phi_draws_, phi_draws + i_modulo * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, phi_covariance_draws_cache_, phi_covariance_draws_cache + i_modulo * float_repetitions_column_stride_size);
					for (size_type r(0); r != repetitions; ++r)
						phi_covariance_draws_cache_[r] = *sigma_portion_value * sigma_draws_[r] + *phi_portion_value * phi_draws_[r];
				}

				if (censoc::was_fpu_ok(phi_covariance_draws_cache, &censoc::init_fpu_check_anchor) == false) { // No point in caching -- otherwise may reuse it detrementally on subsequent iteration(s) [when such may not have fpu exceptions flag set then)
					phi_covariance_cached[i_modulo] = censoc::largish<float_type>();
					return;
				} else
					phi_covariance_cached[i_modulo] = phi_covariance.value();
			}

			assert(phi_covariance.value() != censoc::largish<float_type>());
			assert(phi_variance.value() != censoc::largish<float_type>());
			if (phi_scaled_covariance_cached_main[i_modulo] == phi_covariance.value() && phi_variance_cached_main[i_modulo] == phi_variance.value())
				phi_scaled_draws_cache = phi_scaled_draws_cache_main;
			else if (phi_scaled_covariance_cached_tmp[i_modulo] == phi_covariance.value() && phi_variance_cached_tmp[i_modulo] == phi_variance.value())
				phi_scaled_draws_cache = phi_scaled_draws_cache_tmp;
			else {
				float_type * phi_scaled_covariance_cached;
				float_type * phi_variance_cached;
				if (phi_covariance.value_modified() == false && phi_variance.value_modified() == false) {
					phi_variance_cached = phi_variance_cached_main;
					phi_scaled_covariance_cached = phi_scaled_covariance_cached_main;
					phi_scaled_draws_cache = phi_scaled_draws_cache_main;
				} else {
					phi_variance_cached = phi_variance_cached_tmp;
					phi_scaled_covariance_cached = phi_scaled_covariance_cached_tmp;
					phi_scaled_draws_cache = phi_scaled_draws_cache_tmp;
				}

				{
					// calculate scaled distribution draws
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(float_type const, phi_variance_value, phi_variance.value());
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, phi_covariance_draws_cache_, phi_covariance_draws_cache + i_modulo * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, phi_scaled_draws_cache_, phi_scaled_draws_cache + i_modulo * float_repetitions_column_stride_size);
					for (size_type r(0); r != repetitions; ++r)
						phi_scaled_draws_cache_[r] = *phi_variance_value * phi_covariance_draws_cache_[r];
				}

				if (censoc::was_fpu_ok(phi_scaled_draws_cache, &censoc::init_fpu_check_anchor) == false) { // No point in caching -- otherwise may reuse it detrementally on subsequent iteration(s) [when such may not have fpu exceptions flag set then)
					phi_scaled_covariance_cached[i_modulo] = phi_variance_cached[i_modulo] = censoc::largish<float_type>();
					return;
				} else {
					phi_scaled_covariance_cached[i_modulo] = phi_covariance.value();
					phi_variance_cached[i_modulo] = phi_variance.value();
				}
			}

			coefficient_metadata_paramtype const phi_mean(coefficients[2 * x_size + 1]);
			assert(phi_variance.value() != censoc::largish<float_type>());
			assert(phi_mean.value() != censoc::largish<float_type>());
			if (complete_phi_mean_cached_main[i_modulo] == phi_mean.value() 
			&& complete_phi_variance_cached_main[i_modulo] == phi_variance.value()
			&& complete_phi_covariance_cached_main[i_modulo] == phi_covariance.value()
			)
				complete_phi_draws_cache = complete_phi_draws_cache_main;
			else if (complete_phi_mean_cached_tmp[i_modulo] == phi_mean.value() 
			&& complete_phi_variance_cached_tmp[i_modulo] == phi_variance.value()
			&& complete_phi_covariance_cached_tmp[i_modulo] == phi_covariance.value()
			)
				complete_phi_draws_cache = complete_phi_draws_cache_tmp;
			else {
				float_type * complete_phi_mean_cached;
				float_type * complete_phi_variance_cached;
				float_type * complete_phi_covariance_cached;
				if (phi_mean.value_modified() == false && phi_variance.value_modified() == false && phi_covariance.value_modified() == false) {
					complete_phi_mean_cached = complete_phi_mean_cached_main;
					complete_phi_variance_cached = complete_phi_variance_cached_main;
					complete_phi_covariance_cached = complete_phi_covariance_cached_main;
					complete_phi_draws_cache = complete_phi_draws_cache_main;
				} else {
					complete_phi_mean_cached = complete_phi_mean_cached_tmp;
					complete_phi_variance_cached = complete_phi_variance_cached_tmp;
					complete_phi_covariance_cached = complete_phi_covariance_cached_tmp;
					complete_phi_draws_cache = complete_phi_draws_cache_tmp;
				}

				{
					// calculate offset distribution draws
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, phi_scaled_draws_cache_, phi_scaled_draws_cache + i_modulo * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(float_type const, phi_mean_value, phi_mean.value());
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, complete_phi_draws_cache_, complete_phi_draws_cache + i_modulo * float_repetitions_column_stride_size);
					for (size_type r(0); r != repetitions; ++r)
							complete_phi_draws_cache_[r] = *phi_mean_value + phi_scaled_draws_cache_[r];
				}

				if (censoc::was_fpu_ok(complete_phi_draws_cache, &censoc::init_fpu_check_anchor) == false) { // No point in caching -- otherwise may reuse it detrementally on subsequent iteration(s) [when such may not have fpu exceptions flag set then)
					complete_phi_mean_cached[i_modulo] = complete_phi_variance_cached[i_modulo] = complete_phi_covariance_cached[i_modulo] = censoc::largish<float_type>();
					return;
				} else {
					complete_phi_mean_cached[i_modulo] = phi_mean.value();
					complete_phi_variance_cached[i_modulo] = phi_variance.value();
					complete_phi_covariance_cached[i_modulo] = phi_covariance.value();
				}
			}

			for (size_type set_ones_i(0); set_ones_i != repetitions; ++set_ones_i)
				respondent_probability_cache[set_ones_i] = 1;

			// TODO -- ideally, in addition to this 'cumulative formula' caching, could *also* cache at a finer level (etavar, betas -- separately). (albeit this would blow-out the memory requirements)
			// TODO -- ALSO ABOUT MAIN VS TMP CACHES THEN...  (once again -- will need to think about memory usage).

			// TODO/NOTE (=old!!! possibly outdated) -- if memory usage is too large, can take out this caching, and derpecate ev_cache, just use eta_cache (but reduced in size of course) for both (and subsequently clobber on every repspondent pass)

			// AN EVEN BETTER TODO (==old!!! possibly outdated) -- try not using separate draws for each person (in other words -- use the same set of halton draws for every person -- should drop memory usag a LOT)

			for (size_type j(0); j != x_size; ++j) {
				if (etavars_cache[eta_cache_respondent_column_begin + j] != etavar[j].value()) {
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, eta_cache_x_, eta_cache_x + (eta_cache_respondent_column_begin + j) * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, matrix_haltons_nonsigma_, matrix_haltons_nonsigma + (eta_cache_respondent_column_begin + j) * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(float_type const, etavar_value, etavar[j].value());
					for (size_type r(0); r != repetitions; ++r)
						eta_cache_x_[r] = matrix_haltons_nonsigma_[r] * *etavar_value;

					if (censoc::was_fpu_ok(eta_cache_x, &censoc::init_fpu_check_anchor) == false) { // No point in caching -- otherwise may reuse it detrementally on subsequent iteration(s) [when such may not have fpu exceptions flag set then)
						etavars_cache[eta_cache_respondent_column_begin + j] = censoc::largish<float_type>();
						// note -- not resetting 'betavars_cache' because this code should automatically re-run on next iteration. BUT this is only due to the fact that there is no 'main/transient' cache variants for etavars_ and betavars_ caches!!!
						return;
					} else {
						// TODO -- later introduce main/transient (or a buffer/multiple of) caches!!! (no time for right now and the caches are rather huge already...)
						etavars_cache[eta_cache_respondent_column_begin + j] = etavar[j].value();
					}
				}
			}

			// now, one could further break-up the sigma * (eta + coeff) into "eta + coeff" cache and then into "sigma * xxx"... (i.e. have "stage3_composite_equation_tau_cached" multiplication be processed separately/later) but each of such caches is rather huge... will leave for further considerations... especially given that only thing which will be saved is the addition (in face of the already-needed multiplication)
			for (size_type j(0); j != x_size; ++j) {
				size_type const coefficient_column(eta_cache_respondent_column_begin + j);
				if (stage3_composite_equation_tau_cached[coefficient_column] != tau.value() 
						|| stage3_composite_equation_phi_mean_cached[coefficient_column] != phi_mean.value() 
						|| stage3_composite_equation_phi_variance_cached[coefficient_column] != phi_variance.value() 
						|| stage3_composite_equation_phi_covariance_cached[coefficient_column] != phi_covariance.value() 
						|| betavars_cache[coefficient_column] != coefficients[j].value()
						|| stage3_etavars_cache[coefficient_column] != etavar[j].value()
				) {
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, eta_cache_, eta_cache + coefficient_column * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, eta_cache_x_, eta_cache_x + coefficient_column * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, sigma_cache_, sigma_cache + i_modulo * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, complete_phi_draws_cache_, complete_phi_draws_cache + i_modulo * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(float_type const, coefficient_value, coefficients[j].value()); 
					for (size_type r(0); r != repetitions; ++r)
						eta_cache_[r] = sigma_cache_[r] * (eta_cache_x_[r] + *coefficient_value) + complete_phi_draws_cache_[r];

					if (censoc::was_fpu_ok(eta_cache, &censoc::init_fpu_check_anchor) == false) { // No point in caching -- otherwise may reuse it detrementally on subsequent iteration(s) [when such may not have fpu exceptions flag set then)
						stage3_composite_equation_tau_cached[coefficient_column] = stage3_composite_equation_phi_mean_cached[coefficient_column] = stage3_composite_equation_phi_variance_cached[coefficient_column] = stage3_composite_equation_phi_covariance_cached[coefficient_column] = betavars_cache[coefficient_column] = stage3_etavars_cache[coefficient_column] = censoc::largish<float_type>();
						return;
					} else {
						// TODO -- later introduce main/transient (or a buffer/multiple of) caches!!! (no time for right now)
						stage3_composite_equation_tau_cached[coefficient_column] = tau.value();
						stage3_composite_equation_phi_mean_cached[coefficient_column] = phi_mean.value();
						stage3_composite_equation_phi_variance_cached[coefficient_column] = phi_variance.value();
						stage3_composite_equation_phi_covariance_cached[coefficient_column] = phi_covariance.value();
						betavars_cache[coefficient_column] = coefficients[j].value();
						stage3_etavars_cache[coefficient_column] = etavar[j].value();
					}
				}
			}

			for (size_type t(0); t != respondents_choice_sets[i]; ++t) {

				assert(choice_sets_alternatives_ptr < debug_choice_sets_alternatives_end);
				uint8_t const alternatives(*choice_sets_alternatives_ptr++);
				assert(alternatives <= max_alternatives);
				assert(x_size);
#ifndef NDEBUG
				float_type const * const debug_end_of_row_in_composite_matrix(matrix_composite_ptr + alternatives * x_size);
				assert(debug_end_of_row_in_composite_matrix <= debug_matrix_composite_end);
				assert(choice_column_ptr <= debug_choice_column_end);
#endif
				// x == 0 (first pass)
				for (size_type a(0); a != alternatives; ++a) {
					assert(matrix_composite_ptr < debug_end_of_row_in_composite_matrix);
					float_type const control(*matrix_composite_ptr++);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, dst, ev_cache_tmp_or_censor_cache_tmp + a * float_repetitions_column_stride_size);
					CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src, eta_cache + eta_cache_respondent_column_begin * float_repetitions_column_stride_size);

					if (control == .0)
						for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
							dst[tmp_i] = .0;
					else if (control == 1.)
						for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
							dst[tmp_i] = src[tmp_i];
					else if (control == -1.)
						for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
							dst[tmp_i] = -src[tmp_i];
					else if (control == 2.)
						for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
							dst[tmp_i] = src[tmp_i] + src[tmp_i];
					else if (control == -2.)
						for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
							dst[tmp_i] = -(src[tmp_i] + src[tmp_i]);
					else
						for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
							dst[tmp_i] = src[tmp_i] * control;

				}

				// now to the rest of passes ( 1 <= x < x_size)
				for (size_type x(1); x != x_size; ++x) {
					for (size_type a(0); a != alternatives; ++a) {
						assert(matrix_composite_ptr < debug_end_of_row_in_composite_matrix);
						float_type const control(*matrix_composite_ptr++);

						if (control != .0) {

							CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type, dst, ev_cache_tmp_or_censor_cache_tmp + a * float_repetitions_column_stride_size);
							CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src, eta_cache + (eta_cache_respondent_column_begin + x) * float_repetitions_column_stride_size);

							if (control == 1.)
								for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
									dst[tmp_i] += src[tmp_i];
							else if (control == -1.)
								for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
									dst[tmp_i] -= src[tmp_i];
							else if (control == 2.)
								for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
									dst[tmp_i] += src[tmp_i] + src[tmp_i];
							else if (control == -2.)
								for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
									dst[tmp_i] -= src[tmp_i] + src[tmp_i];
							else
								for (size_type tmp_i(0); tmp_i != repetitions; ++tmp_i) 
									dst[tmp_i] += src[tmp_i] * control;
						}

					}
				}

#ifndef NDEBUG
				for (size_type a(0); a != alternatives; ++a) 
					for (size_type r(0); r != repetitions; ++r) 
						assert(::std::isfinite(ev_cache_tmp_or_censor_cache_tmp[a * float_repetitions_column_stride_size + r]));
#endif

				// move to the last column in row -- a chosen alternative
				assert(matrix_composite_ptr == debug_end_of_row_in_composite_matrix);
				size_type const chosen_alternative(*choice_column_ptr++);
				assert(chosen_alternative < alternatives);
				// todo some sanity-checking assertions

				{
					if (reduce_exp_complexity == false) {

						// TODO -- consider fixing this, the way it is now is rather stupid wrt cpu-caching

						// calculate 'exp' and aggregate across rows/alternatives
						CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type, accum, repetitions_cache_rowwise_tmp);
						{ // a = 0, first alternative
						CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type, dst, ev_cache);
						CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src, ev_cache_tmp_or_censor_cache_tmp);
						for (size_type r(0); r != repetitions; ++r) {
							extended_float_type const tmp(exponent_evaluation_choice_base::eval(src[r]));
							accum[r] = tmp;
							if (!chosen_alternative)
								dst[r] = tmp; 
						}
						}
						for (size_type a(1); a != alternatives; ++a) {
							CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type, dst, ev_cache);
							CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src, ev_cache_tmp_or_censor_cache_tmp + a * float_repetitions_column_stride_size);
							for (size_type r(0); r != repetitions; ++r) {
								extended_float_type const tmp(exponent_evaluation_choice_base::eval(src[r]));
								accum[r] += tmp;
								if (a == chosen_alternative)
									dst[r] = tmp;
							}
						}

						{
						CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type, dst, respondent_probability_cache);
						CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type const, src, ev_cache);
						for (size_type r(0); r != repetitions; ++r)
							dst[r] *=  src[r] / accum[r];
						}

					} else {

						CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type, accum, repetitions_cache_rowwise_tmp);
						{ // a =0, first alternative
							if (chosen_alternative) {
								CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src_a, ev_cache_tmp_or_censor_cache_tmp);
								CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src_b, ev_cache_tmp_or_censor_cache_tmp + chosen_alternative * float_repetitions_column_stride_size);
								for (size_type r(0); r != repetitions; ++r)
									accum[r] = exponent_evaluation_choice_base::eval(src_a[r] - src_b[r]); 
							} else for (size_type r(0); r != repetitions; ++r)
								accum[r] = 
									//dst[r] = 
									1;
						}
						for (size_type a(1); a != alternatives; ++a) {
							if (a != chosen_alternative) {
								CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src_a, ev_cache_tmp_or_censor_cache_tmp + a * float_repetitions_column_stride_size);
								CENSOC_ALIGNED_RESTRICTED_CONST_PTR(float_type const, src_b, ev_cache_tmp_or_censor_cache_tmp + chosen_alternative * float_repetitions_column_stride_size);
								for (size_type r(0); r != repetitions; ++r)
									accum[r] += exponent_evaluation_choice_base::eval(src_a[r] - src_b[r]); 
							} else for (size_type r(0); r != repetitions; ++r)
								accum[r] += 1;
						}

						{
						CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type, dst, respondent_probability_cache);
						for (size_type r(0); r != repetitions; ++r)
							dst[r] /=  accum[r];
						}
					}
				}

			} // end loop for each choiceset, for a given respondent

			{
				CENSOC_ALIGNED_RESTRICTED_CONST_PTR(extended_float_type const, src, respondent_probability_cache);
				CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(extended_float_type, sum, 0);
				for (size_type r(0); r != repetitions; ++r)
					*sum += src[r];
				accumulated_probability += accumulated_probability_log_repetitions_offset - ::std::log(*sum);
			}

			// note -- using the log of likelihood (instead of 'running products' way of the raw likelihood) in order to programmatically prevent underflow in cases of large number of respondents/observations being present in dataset.
			// e.g. -- 2000 observation with prob of .5 for each (for the ease of illustration)
			// then in raw likelihood "runnig product" we have .5 ^ 2000 which is numeric underflow pretty much...
			// in log-likelihood we, on the other hand, have 2000 * ln(.5) which is perfectly ok indeed

			if (censoc::was_fpu_ok(&accumulated_probability, &censoc::init_fpu_check_anchor) == false || accumulated_probability > target) 
				return;

			assert(censoc::isfinite(accumulated_probability) == true);

		} // loop for all respondents

		// quick-n-nasty
		censoc::llog() << "ll end proper resulting value: [" << accumulated_probability << "]\n";

		result.value(accumulated_probability);
	}

	bool static
	on_meta_read(meta_msg_type const & meta_msg)
	{

		// todo verify meta parameters here (if processing size is too larg for RAM on this peer, then write 'bad' msg and issue 'read' -- the server should send 'end_process' when it feels like it, then can call in this prog 'delete this' and the underlying processor.h (peer_connection) in this prog will write a new 'ready to process' msg)

		size_type const max_alternatives(meta_msg.model.dataset.max_alternatives());
		size_type const repetitions(meta_msg.model.repetitions());
		uint_fast64_t const matrix_composite_elements(meta_msg.model.dataset.matrix_composite_elements()); 
		uint_fast64_t const x_size(meta_msg.model.dataset.x_size()); 
		uint_fast64_t const draws_sets_size(meta_msg.model.draws_sets_size());

		// todo -- make a proper calculations (currently not taking into account the stride alignment effects... but that should not be too much...)
		uint_fast64_t anticipated_ram_utilization(
			sizeof(float_type) * (
				draws_sets_size * 2 // sigma_bar_tau_cached (main and tmp)
				+ draws_sets_size * 2 // phi_covariance_cached_{main,tmp}
				+ draws_sets_size * 2 // phi_scaled_covariance_cached_{main,tmp}
				+ draws_sets_size * 2 // phi_variance_cached_{main,tmp}
				+ draws_sets_size * 2 // complete_phi_mean_cached_{main,tmp}
				+ draws_sets_size * 2 // complete_phi_variance_cached_{main,tmp}
				+ draws_sets_size * 2 // complete_phi_covariance_cached_{main,tmp}
				+ draws_sets_size * repetitions // sigma haltons (now a vector)
				+ draws_sets_size * repetitions // phi haltons (a vector)
				+ repetitions * x_size * draws_sets_size // non sigma haltons 
				+ x_size * draws_sets_size // stage3_composite_equation_tau_cached
				+ x_size * draws_sets_size // stage3_composite_equation_phi_mean_cached
				+ x_size * draws_sets_size // stage3_composite_equation_phi_variance_cached
				+ x_size * draws_sets_size // stage3_composite_equation_phi_covariance_cached
				+ x_size * draws_sets_size // etavars_cache
				+ x_size * draws_sets_size // betavars_cache
				+ 2 * repetitions * draws_sets_size // sigma_cache {main, tmp}
				+ 2 * repetitions * draws_sets_size // phi_covariance_draws_cache {main, tmp}
				+ 2 * repetitions * draws_sets_size // phi_scaled_draws_cache {main, tmp}
				+ 2 * repetitions * draws_sets_size // complete_phi_draws_cache_{main, tmp}
				+ 2 * repetitions * x_size * draws_sets_size // eta_cache and eta_cache_x
				+ repetitions * max_alternatives // ev_cache_tmp_or_censor_cache_tmp
				+ matrix_composite_elements // matrix_composite
			) 
			+ sizeof(extended_float_type) * (
				+ repetitions // ev_cache
				+ repetitions // respondent_probability_cache
				+ repetitions // repetitions_cache_rowwise_tmp
			)
			// TODO -- put choice_columns calculation here!!!
		); 

		return netcpu::test_anticipated_ram_utilization(anticipated_ram_utilization);
	}

};

netcpu::converger_1::model_factory<gmnl_2a::task_processor, netcpu::models_ids::gmnl_2a> static factory;

}}}

#endif
