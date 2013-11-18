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

#include <censoc/exp_lookup.h>

#include <netcpu/converger_1/server.h>
#include <netcpu/dataset_1/message/dataset_meta.h>

#include "message/meta.h"
#include "message/bulk.h"

// includes }

#ifndef CENSOC_NETCPU_MULTI_LOGIT_SERVER_H
#define CENSOC_NETCPU_MULTI_LOGIT_SERVER_H

//{ for the time-being model_factory_interface will be automatically declared, no need to include explicitly...}

namespace censoc { namespace netcpu { namespace multi_logit { 

template <typename N, typename F, typename Model, netcpu::models_ids::val ModelId> struct task;
template <typename N, typename F, typename Model>
struct task<N, F, Model, netcpu::models_ids::multi_logit> : netcpu::converger_1::task<N, F, Model, netcpu::models_ids::multi_logit> {
	typedef netcpu::converger_1::task<N, F, Model, netcpu::models_ids::multi_logit> base_type; 

	using typename base_type::float_type;
	using typename base_type::size_type;
	using base_type::active_task_processor;
	using base_type::markcompleted;

	::std::vector<float_type> converged_coefficients;

	::std::string dataset_filepath;

	task(netcpu::tasks_size_type const & id, ::std::string const & name, time_t birthday = ::time(NULL))
	: base_type(id, name, birthday), dataset_filepath(netcpu::root_path + name + "/dataset.csv") {
	}
	using base_type::name;

	void 
	set_task_info_user_data(converger_1::message::meta<N, F, multi_logit::message::meta<N> > const & meta_msg) override
	{
		base_type::task_info_user_data.reset(1);
		*base_type::task_info_user_data.data() = meta_msg.model.betas_sets_size();
	}

	void 
	verify(converger_1::message::meta<N, F, multi_logit::message::meta<N> > const & meta_msg, converger_1::message::bulk<N, F, multi_logit::message::bulk<N> > const &) override
	{
		if (!meta_msg.model.betas_sets_size())
			throw netcpu::message::exception("betas_sets_size must be > 0");
	}

	// todo -- for the time-being doing it here directly (at the server end), but in future may indeed distribute this computation across multiple processing peers! This architecture is therefore applicable to other sub-tasks such as Hessian matrix calculation, etc. (although some additional methods such as 'active' may need to be implemented).
	void
	generate_classes_probability()
	{
		netcpu::message::read_wrapper wrapper;

		// read res_msg
		::std::ifstream res_file((netcpu::root_path + name() + "/res.msg").c_str(), ::std::ios::binary);
		if (!res_file)
			throw ::std::runtime_error("missing res message during load");

		netcpu::message::fstream_to_wrapper(res_file, wrapper);

		converger_1::message::res res_msg;
		res_msg.from_wire(wrapper);

		// explicit because no need really to have compiler go through "<double, float>" arrangement...
		if (res_msg.extended_float_res() == converger_1::message::float_res<float>::value)
			generate_classes_probability_sub_1<float>(res_msg, wrapper);
		else if (res_msg.extended_float_res() == converger_1::message::float_res<double>::value)
			generate_classes_probability_sub_1<double>(res_msg, wrapper);
		else 
			throw ::std::runtime_error(xxx("unsupported floating resolutions: [") << res_msg.float_res() << "], [" << res_msg.extended_float_res() << ']');

	}

	template <typename EF>
	void
	generate_classes_probability_sub_1(converger_1::message::res const & res_msg, netcpu::message::read_wrapper & wrapper)
	{
		if (res_msg.approximate_exponents())
			generate_classes_probability_sub_2<EF, true>(wrapper);
		else
			generate_classes_probability_sub_2<EF, false>(wrapper);
	}

	template <typename extended_float_type, bool approximate_exponents>
	void
	generate_classes_probability_sub_2(netcpu::message::read_wrapper & wrapper)
	{
		assert(converged_coefficients.empty() == false);


		// read meta_msg
		::std::ifstream meta_file((netcpu::root_path + name() + "/meta.msg").c_str(), ::std::ios::binary);
		if (!meta_file)
			throw ::std::runtime_error("missing meta message during load");

		netcpu::message::fstream_to_wrapper(meta_file, wrapper);

		converger_1::message::meta<N, F, multi_logit::message::meta<N> > meta_msg;
		meta_msg.from_wire(wrapper);

		struct exp_traits_type : censoc::exp_lookup::exponent_evaluation_choice<extended_float_type, typename netcpu::message::typepair<N>::ram, approximate_exponents> {
			bool const reduce_exp_complexity;
			exp_traits_type(bool const x) : reduce_exp_complexity(x) {}
		} exp_traits(meta_msg.model.reduce_exp_complexity());

		size_type const x_size(meta_msg.model.dataset.x_size());
		size_type const betas_sets_size(meta_msg.model.betas_sets_size());
		size_type max_alternatives(meta_msg.model.dataset.max_alternatives());
		::std::vector<float_type> betas_mult_choice(max_alternatives);

		// read bulk_msg
		::std::ifstream bulk_file((netcpu::root_path + name() + "/bulk.msg").c_str(), ::std::ios::binary);
		if (!bulk_file)
			throw ::std::runtime_error("missing bulk message during load");
		netcpu::message::fstream_to_wrapper(bulk_file, wrapper);
		converger_1::message::bulk<N, F, typename Model::bulk_msg_type> bulk_msg;
		bulk_msg.from_wire(wrapper);

		assert(bulk_msg.coeffs.size() == converged_coefficients.size());

		::std::vector<size_type> respondents_choice_sets;
		::std::vector<size_type> respondents_modal_class;
		::std::vector<size_type> classes_membership_count(betas_sets_size);
		::boost::scoped_array<uint8_t> choice_sets_alternatives;
		::boost::scoped_array<float_type> matrix_composite; 
		::boost::scoped_array<uint8_t> choice_column; 

		size_type const respondents_size(bulk_msg.model.dataset.respondents_choice_sets.size());
		respondents_choice_sets.reserve(respondents_size);
		for (unsigned i(0); i != respondents_size; ++i)
			respondents_choice_sets.push_back(bulk_msg.model.dataset.respondents_choice_sets(i));
		respondents_modal_class.reserve(respondents_size);

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
		uint8_t * debug_choice_sets_alternatives_end;
		float_type * debug_matrix_composite_end;
		uint8_t * debug_choice_column_end;
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

		::std::vector<extended_float_type> respondents_classes_probability; 
		respondents_classes_probability.reserve(respondents_size * betas_sets_size);

		uint8_t const * choice_sets_alternatives_ptr_outer(choice_sets_alternatives.get());
		assert(matrix_composite.get() != NULL);
		float_type const * matrix_composite_ptr_outer(matrix_composite.get());
		float_type const * matrix_composite_ptr_inner;
		uint8_t const * choice_column_ptr_outer(choice_column.get());

		float_type classes_probability_normaliser_inv(0);
		for (size_type betas_set_i(0); betas_set_i != betas_sets_size; ++betas_set_i) {
			float_type const tmp_value(converged_coefficients[betas_set_i]);
			float_type const tmp_value_transformed(tmp_value > 0 ? 1 + tmp_value : 1 / (1 - tmp_value));
			classes_probability_normaliser_inv += converged_coefficients[betas_set_i] = tmp_value_transformed;
		}
		classes_probability_normaliser_inv = 1 / classes_probability_normaliser_inv;

		::std::vector<float_type> classes_prior_probability(betas_sets_size);
		for (size_type betas_set_i(0); betas_set_i != betas_sets_size; ++betas_set_i)
			classes_prior_probability[betas_set_i] = converged_coefficients[betas_set_i] * classes_probability_normaliser_inv;

		::std::vector<extended_float_type> respondent_classes_posterior_probability(betas_sets_size);

		assert(respondents_choice_sets.empty() == false);
		for (size_type i(0); i != respondents_choice_sets.size(); ++i) {

			// todo -- this is rather a vvvvery vvvvery quick hack: later on should consider moving this pass/loop into the innermost loops via vectorized array/vector -- just like 'repetititons' in models such as mixed logit et. al. get calculated
			uint8_t const * choice_sets_alternatives_ptr_inner;
			float_type const * matrix_composite_ptr_inner;
			uint8_t const * choice_column_ptr_inner;
			size_type current_coefficient_i_outer(betas_sets_size);

			extended_float_type modal_max(0);
			size_type modal_i;

			assert(betas_sets_size);
			for (size_type betas_set_i(0); betas_set_i != betas_sets_size; ++betas_set_i) {

				choice_sets_alternatives_ptr_inner = choice_sets_alternatives_ptr_outer;
				matrix_composite_ptr_inner = matrix_composite_ptr_outer;
				choice_column_ptr_inner = choice_column_ptr_outer;
				size_type current_coefficient_i_inner;

				extended_float_type respondent_probability(1);

				assert(respondents_choice_sets[i]);
				for (size_type t(0); t != respondents_choice_sets[i]; ++t) {

					current_coefficient_i_inner = current_coefficient_i_outer;

					assert(choice_sets_alternatives_ptr_inner < debug_choice_sets_alternatives_end);
					uint8_t const alternatives(*choice_sets_alternatives_ptr_inner++);
					assert(alternatives <= max_alternatives);

					// todo -- may be aligned foo...

					// setting first pass explicitly as opposed to additional call to 'set to zero' initially and then += ones...
					assert(x_size);
#ifndef NDEBUG
					float_type const * const debug_end_of_row_in_composite_matrix(matrix_composite_ptr_inner + alternatives * x_size);
					assert(debug_end_of_row_in_composite_matrix <= debug_matrix_composite_end);
					assert(matrix_composite_ptr_inner < debug_end_of_row_in_composite_matrix);
					assert(choice_column_ptr_inner < debug_choice_column_end);
#endif
					// x == 0 (first pass)
					for (size_type a(0); a != alternatives; ++a) {
						assert(matrix_composite_ptr_inner < debug_end_of_row_in_composite_matrix);
						float_type const control(*matrix_composite_ptr_inner++);

						if (control == .0)
							betas_mult_choice[a] = .0;
						else {
							float_type const tmp(converged_coefficients[current_coefficient_i_inner]);
							if (control == 1.)
								betas_mult_choice[a] = tmp;
							else if (control == -1.)
								betas_mult_choice[a] = -tmp;
							else if (control == 2.)
								betas_mult_choice[a] = tmp + tmp;
							else if (control == -2.)
								betas_mult_choice[a] = -(tmp + tmp);
							else
								betas_mult_choice[a] = tmp * control;
						}

					}
					// now to the rest of passes ( 1 <= x < x_size)
					++current_coefficient_i_inner;
					assert(current_coefficient_i_inner < betas_sets_size * (x_size + 1));
					for (size_type x(1), alternatives_columns_begin(alternatives); x != x_size; ++x, ++current_coefficient_i_inner) {
						assert(current_coefficient_i_inner < betas_sets_size * (x_size + 1));
						for (size_type a(0); a != alternatives; ++a, ++alternatives_columns_begin) {
							assert(matrix_composite_ptr_inner < debug_end_of_row_in_composite_matrix);
							float_type const control(*matrix_composite_ptr_inner++);
							if (control != .0) {
								float_type const tmp(converged_coefficients[current_coefficient_i_inner]);
								if (control == 1.)
									betas_mult_choice[a] += tmp;
								else if (control == -1.)
									betas_mult_choice[a] -= tmp;
								else if (control == 2.)
									betas_mult_choice[a] += tmp + tmp;
								else if (control == -2.)
									betas_mult_choice[a] -= tmp + tmp;
								else
									betas_mult_choice[a] += tmp * control;
							}
						}
					}
					// move to the last column in row -- a chosen alternative
					size_type const chosen_alternative(*choice_column_ptr_inner++);
					assert(chosen_alternative < alternatives);
					// todo some sanity-checking assertions

					{
						extended_float_type accum;
						if (exp_traits.reduce_exp_complexity == false) {
							extended_float_type exp_alts;

							// TODO -- consider fixing this, the way it is now is rather stupid wrt cpu-caching

							// calculate 'exp' and aggregate across rows/alternatives
							{ // a = 0, first alternative
								accum = exp_traits.eval(betas_mult_choice[0]);
								if (!chosen_alternative)
									exp_alts = accum;
							}
							for (size_type a(1); a != alternatives; ++a) {
								extended_float_type const tmp(exp_traits.eval(betas_mult_choice[a]));
								accum += tmp;
								if (a == chosen_alternative)
									exp_alts = tmp;
							}
							respondent_probability *=  exp_alts / accum;
						} else {
							{ // a =0, first alternative
								if (chosen_alternative) {
									accum = exp_traits.eval(betas_mult_choice[0] - betas_mult_choice[chosen_alternative]);
								} else 
									accum = 1;
							}
							for (size_type a(1); a != alternatives; ++a) {
								if (a != chosen_alternative) {
									accum += exp_traits.eval(betas_mult_choice[a] - betas_mult_choice[chosen_alternative]);
								} else
									accum += 1;
							}
							respondent_probability /=  accum;
						}
					}

				} // end loop for each choiceset, for a given respondent

				current_coefficient_i_outer = current_coefficient_i_inner;

				extended_float_type const tmp_prob(classes_prior_probability[betas_set_i] * respondent_probability);

				respondent_classes_posterior_probability[betas_set_i] = tmp_prob;

				if (modal_max < tmp_prob) {
					modal_max = tmp_prob;
					modal_i = betas_set_i;
				}

			} // end for the betas_set

			extended_float_type respondent_classes_posterior_probability_sum_inv(0);
			for (size_type betas_set_i(0); betas_set_i != betas_sets_size; ++betas_set_i)
				respondent_classes_posterior_probability_sum_inv += respondent_classes_posterior_probability[betas_set_i];
			respondent_classes_posterior_probability_sum_inv = 1 / respondent_classes_posterior_probability_sum_inv;
			for (size_type betas_set_i(0); betas_set_i != betas_sets_size; ++betas_set_i)
				respondents_classes_probability.push_back(::std::log(respondent_classes_posterior_probability[betas_set_i] * respondent_classes_posterior_probability_sum_inv));

			respondents_modal_class.push_back(modal_i);
			++classes_membership_count[modal_i];

			choice_sets_alternatives_ptr_outer = choice_sets_alternatives_ptr_inner;
			matrix_composite_ptr_outer = matrix_composite_ptr_inner;
			choice_column_ptr_outer = choice_column_ptr_inner;

		} // loop for all respondents

		// write out the dataset...
		::std::ofstream dataset_file((dataset_filepath + ".tmp").c_str());
		if (!dataset_file)
			throw ::std::runtime_error("cannot write dataset.csv");

		if (censoc::was_fpu_ok(respondents_classes_probability.data()) == false) {
			dataset_file << "posterior probabilities can't be calculated due to numeric issues\n";
		} else { 

			// first read dataset_meta_msg
			::std::ifstream dataset_meta_file((netcpu::root_path + name() + "/dataset_meta.msg").c_str(), ::std::ios::binary);
			if (!dataset_meta_file)
				throw ::std::runtime_error("missing dataset_meta message during load");

			netcpu::message::fstream_to_wrapper(dataset_meta_file, wrapper);

			netcpu::dataset_1::message::dataset_meta<converger_1::message::messages_ids::dataset_meta> dataset_meta_msg;
			dataset_meta_msg.from_wire(wrapper);

			dataset_file << "respondent,choice_set,alternative,choice";
			for (size_type i(0); i != x_size; ++i)
				dataset_file << ",attribute_" << i + 1;
			dataset_file << ",choice_sets_per_respondent,alternatives_per_choice_set";
			float_type const inv_respondents_size_mul_100(100. / respondents_choice_sets.size());
			for (size_type i(0); i != betas_sets_size; ++i)
				dataset_file << ",LL_class_" << i + 1;
			dataset_file << ",modal_class";

			dataset_file << "\n,,,,,";
			for (size_type i(0); i != x_size; ++i)
				dataset_file << ',';
			for (size_type i(0); i != betas_sets_size; ++i)
				dataset_file << ",members(count=" << classes_membership_count[i] << " %=" << classes_membership_count[i] * inv_respondents_size_mul_100 << ')';

			dataset_file << "\n,,,,,";
			for (size_type i(0); i != x_size; ++i)
				dataset_file << ',';
			for (size_type i(0); i != betas_sets_size; ++i)
				dataset_file << ",Pprior(=" << classes_prior_probability[i] << ')'; 
			dataset_file << '\n';

			matrix_composite_ptr_outer = matrix_composite.get();
			choice_sets_alternatives_ptr_outer = choice_sets_alternatives.get();
			choice_column_ptr_outer = choice_column.get();

			extended_float_type * respondents_classes_probability_outer = respondents_classes_probability.data();
			extended_float_type * respondents_classes_probability_inner;

			for (size_type i(0), alt_id_i(0); i != respondents_choice_sets.size(); ++i) { // for every respondent
				for (size_type t(0); t != respondents_choice_sets[i]; ++t, ++choice_column_ptr_outer) { // for every choice-set
					uint8_t const alternatives(*choice_sets_alternatives_ptr_outer++);
					for (size_type alt_i(0); alt_i != alternatives; ++alt_i, ++matrix_composite_ptr_outer, ++alt_id_i) { // for every alternative

						assert(i < dataset_meta_msg.respondents.size());
						assert(static_cast<uintptr_t>(choice_column_ptr_outer - choice_column.get()) < dataset_meta_msg.choice_sets.size());
						assert(alt_id_i < dataset_meta_msg.alternatives.size());

						// write out columns... a quick hack for the time being...
						auto const & respondent_id(dataset_meta_msg.respondents[i]);
						auto const & choice_set_id(dataset_meta_msg.choice_sets[choice_column_ptr_outer - choice_column.get()]);
						auto const & alternative_id(dataset_meta_msg.alternatives[alt_id_i]);
						dataset_file 
						<< ::std::string(reinterpret_cast<char const *>(respondent_id.data()), respondent_id.size()) << ',' 
						<< ::std::string(reinterpret_cast<char const *>(choice_set_id.data()), choice_set_id.size()) << ',' 
						<< ::std::string(reinterpret_cast<char const *>(alternative_id.data()), alternative_id.size()) << ',' 
						<< (*choice_column_ptr_outer == alt_i ? 1 : 0); 

						matrix_composite_ptr_inner = matrix_composite_ptr_outer;
						for (size_type x_i(0); x_i != x_size; ++x_i, matrix_composite_ptr_inner += alternatives) // attributes
							dataset_file << ',' << *matrix_composite_ptr_inner;

						// choice_set and alternative sizes
						dataset_file << ',' << respondents_choice_sets[i] << ',' << static_cast<unsigned>(alternatives);

						respondents_classes_probability_inner  = respondents_classes_probability_outer;
						for (size_type betas_set_i(0); betas_set_i != betas_sets_size; ++betas_set_i, ++respondents_classes_probability_inner) // classes
							dataset_file << ',' << *respondents_classes_probability_inner;
						// modal class
						dataset_file << ',' << respondents_modal_class[i] + 1 << '\n';
					}
					matrix_composite_ptr_outer = matrix_composite_ptr_inner - alternatives + 1;
				}
				respondents_classes_probability_outer = respondents_classes_probability_inner;
			}
		}

		if (::rename((dataset_filepath + ".tmp").c_str(), dataset_filepath.c_str()))
			throw ::std::runtime_error("could not rename/mv latest-class-calculated-dataset csv file");

	}

	void 
	on_converged() override
	{
		size_type const coefficients_size(active_task_processor.get()->coefficients_size);
		converged_coefficients.reserve(coefficients_size);
		assert(converged_coefficients.empty() == true);
		for (size_type i(0); i != coefficients_size; ++i)
			converged_coefficients.push_back(active_task_processor.get()->coefficients_metadata[i].saved_value());
		assert(converged_coefficients.empty() == false);

		auto * const tmp(active_task_processor.get());
		active_task_processor.release();
		delete tmp;

		generate_classes_probability();
		base_type::on_converged();
	}

	void 
	activate() override
	{
		if (::boost::filesystem::exists(dataset_filepath) == false) {
			::std::string const convergence_state_filepath(netcpu::root_path + name() + "/convergence_state.bin");
			if (::boost::filesystem::exists(convergence_state_filepath) == true) {
				converger_1::fstreamer::convergence_state_ifstreamer<N> convergence_state;
				convergence_state.load_header(convergence_state_filepath);
				size_type coefficients_size = convergence_state.get_header().coefficients_size();
				size_type coefficients_rand_range_ended_wait = convergence_state.get_header().coefficients_rand_range_ended_wait();
				if (!coefficients_rand_range_ended_wait) {
					converged_coefficients.reserve(coefficients_size);
					for (size_type i(0); i != coefficients_size; ++i) {
						convergence_state.load_coefficient();
						converged_coefficients.push_back(netcpu::message::deserialise_from_decomposed_floating<float_type>(convergence_state.get_coefficient().value_f));
					}
					generate_classes_probability();
					assert(netcpu::pending_tasks.front() == this);
					markcompleted();
					return;
				}
			} 
		} 
		base_type::activate();
	}
};

template <typename N, typename F>
struct model_traits {
	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef multi_logit::message::meta<N> meta_msg_type;
	typedef multi_logit::message::bulk<N> bulk_msg_type;

	template <netcpu::models_ids::val ModelId>
	using task_type = task<N, F, model_traits<N, F>, ModelId>; // specialisation of template aliases with ModelId = netcpu::models_ids::multi_logit is not allowed, todo -- find a more elegant solution later on, time permitting...

};

netcpu::converger_1::model_factory<multi_logit::model_traits, netcpu::models_ids::multi_logit> static factory;

}}}

#endif
