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

//{ includes...

#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/finite_math.h>
#include <censoc/sysinfo.h>
#include <censoc/exp_lookup.h>

#include <netcpu/converger_1/processor.h>

#include <netcpu/converger_1/message/bulk.h>

#include "message/meta.h"
#include "message/bulk.h"
//}

#ifndef CENSOC_NETCPU_MULTI_LOGIT_PROCESSOR_H
#define CENSOC_NETCPU_MULTI_LOGIT_PROCESSOR_H

// for the time-being model_factory_interface will be automatically declared, no need to include explicitly...
namespace censoc { namespace netcpu { namespace multi_logit { 

template <typename N, typename F, typename EF, bool approximate_exponents>
struct task_processor : censoc::exp_lookup::exponent_evaluation_choice<EF, typename netcpu::message::typepair<N>::ram, approximate_exponents> {

	//{ typedefs...

	typedef netcpu::converger_1::message::meta<N, F, multi_logit::message::meta<N> > meta_msg_type;
	typedef netcpu::converger_1::message::bulk<N, F, multi_logit::message::bulk<N> > bulk_msg_type;

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef EF extended_float_type;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef censoc::lexicast< ::std::string> xxx;

	typedef netcpu::converger_1::coefficient_metadata<N, F> coefficient_metadata_type;
	typedef typename censoc::param<coefficient_metadata_type>::type coefficient_metadata_paramtype;
	typedef censoc::array<coefficient_metadata_type, size_type> coefficients_metadata_type;
	typedef typename censoc::param<coefficients_metadata_type>::type coefficients_metadata_paramtype;

	typedef censoc::exp_lookup::exponent_evaluation_choice<EF, size_type, approximate_exponents> exponent_evaluation_choice_base;

	//}

	//{ data members

#ifndef NDEBUG
	size_type const max_alternatives;
#endif
	size_type const x_size;
	::std::vector<size_type> respondents_choice_sets;
	::std::vector<float_type> betas_mult_choice;

#ifndef NDEBUG
	uint8_t * debug_choice_sets_alternatives_end;
	float_type * debug_matrix_composite_end;
	uint8_t * debug_choice_column_end;
#endif
	::boost::scoped_array<uint8_t> choice_sets_alternatives;
	::boost::scoped_array<float_type> matrix_composite; 
	::boost::scoped_array<uint8_t> choice_column; 

	bool reduce_exp_complexity;

	size_type const betas_sets_size;
	float_type const accumulated_probability_log_betas_sets_size_offset;

	//}

	// TODO -- later deprecate message.h typedef size_type and pass it via the template arg!!!
	task_processor(meta_msg_type const & meta_msg)	
	: 
#ifndef NDEBUG
	max_alternatives(meta_msg.model.dataset.max_alternatives()),  
#endif
	x_size(meta_msg.model.dataset.x_size()), 
	betas_mult_choice(meta_msg.model.dataset.max_alternatives()),
	reduce_exp_complexity(meta_msg.model.reduce_exp_complexity()),
	betas_sets_size(meta_msg.model.betas_sets_size()), accumulated_probability_log_betas_sets_size_offset(::std::log(static_cast<float_type>(betas_sets_size)))
	{
		assert(betas_sets_size);
		censoc::llog() << "ctor in multi_logit::task_processor\n";
	}

public:

	size_type
	get_coefficients_size() const noexcept
	{	
		return (x_size + 1) * betas_sets_size;
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

	/** @note slightly verbose (thus not so optimised) implementation for the time-being
		@post shall not return value which evaluates to '::censoc::isfinite(x) == false' (i.e. it shall handle the cases of NAN/Inf and return 'censoc::largish<float_type>()' value (which, of coures is not really 'Inf' but is, indeed, '::censoc::isfinite(x) == true')
		*/
	void 
	log_likelihood(coefficients_metadata_paramtype coefficients, float_paramtype target, netcpu::converger_1::opaque_result<float_type> & result)
	{
		censoc::llog() << "ll start\n";

		assert(censoc::was_fpu_ok(&censoc::init_fpu_check_anchor));

		assert(result.is_valid() == false);

		float_type accumulated_probability(0); 

		uint8_t const * choice_sets_alternatives_ptr_outer(choice_sets_alternatives.get());
		assert(matrix_composite.get() != NULL);
		float_type const * matrix_composite_ptr_outer(matrix_composite.get());
		uint8_t const * choice_column_ptr_outer(choice_column.get());

		assert(respondents_choice_sets.empty() == false);
		for (size_type i(0); i != respondents_choice_sets.size(); ++i) {

#ifdef __WIN32__
			if (!(i % 100) && netcpu::should_i_sleep == true) 
				return result.flag_incomplete_processing();
#endif

			// todo -- this is rather a vvvvery vvvvery quick hack: later on should consider moving this pass/loop into the innermost loops via vectorized array/vector -- just like 'repetititons' in models such as mixed logit et. al. get calculated
			uint8_t const * choice_sets_alternatives_ptr_inner;
			float_type const * matrix_composite_ptr_inner;
			uint8_t const * choice_column_ptr_inner;
			size_type current_coefficient_i_outer(0);

			extended_float_type betas_respondent_probability(0);
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
					float_type const betas_set_scale(coefficients[current_coefficient_i_inner++].value());


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
							float_type const tmp(coefficients[current_coefficient_i_inner].value());
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
								float_type const tmp(coefficients[current_coefficient_i_inner].value());
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
						if (reduce_exp_complexity == false) {
							extended_float_type exp_alts;

							// TODO -- consider fixing this, the way it is now is rather stupid wrt cpu-caching

							// calculate 'exp' and aggregate across rows/alternatives
							{ // a = 0, first alternative
								accum = exponent_evaluation_choice_base::eval(betas_mult_choice[0]);
								if (!chosen_alternative)
									exp_alts = accum;
							}
							for (size_type a(1); a != alternatives; ++a) {
								extended_float_type const tmp(exponent_evaluation_choice_base::eval(betas_mult_choice[a]));
								accum += tmp;
								if (a == chosen_alternative)
									exp_alts = tmp;
							}
							respondent_probability *=  exp_alts / accum;
						} else {
							{ // a =0, first alternative
								if (chosen_alternative) {
									accum = exponent_evaluation_choice_base::eval(betas_mult_choice[0] - betas_mult_choice[chosen_alternative]);
								} else 
									accum = 1;
							}
							for (size_type a(1); a != alternatives; ++a) {
								if (a != chosen_alternative) {
										accum += exponent_evaluation_choice_base::eval(betas_mult_choice[a] - betas_mult_choice[chosen_alternative]);
								} else
									accum += 1;
							}
							respondent_probability /=  accum;
						}
					}

				} // end loop for each choiceset, for a given respondent
				
				current_coefficient_i_outer = current_coefficient_i_inner;

				betas_respondent_probability += respondent_probability;

			} // end for the betas_set

			choice_sets_alternatives_ptr_outer = choice_sets_alternatives_ptr_inner;
			matrix_composite_ptr_outer = matrix_composite_ptr_inner;
			choice_column_ptr_outer = choice_column_ptr_inner;

			accumulated_probability += accumulated_probability_log_betas_sets_size_offset - ::std::log(betas_respondent_probability);

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

		uint_fast64_t const matrix_composite_elements(meta_msg.model.dataset.matrix_composite_elements()); 

		uint_fast64_t anticipated_ram_utilization(
			matrix_composite_elements
			// TODO -- put choice_columns calculation here!!!
		); 

		return netcpu::test_anticipated_ram_utilization(anticipated_ram_utilization);
	}

#ifndef NDEBUG
	// as a matter of debugging policy compliance, for the time-being
	void invalidate_caches_1() {}
#endif

};

netcpu::converger_1::model_factory<multi_logit::task_processor, netcpu::models_ids::multi_logit> static factory;

}}}

#endif
