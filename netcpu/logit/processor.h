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

#include <netcpu/dataset_1/composite_matrix.h>
#include <netcpu/converger_1/message/bulk.h>

#include "message/meta.h"
#include "message/bulk.h"
//}

#ifndef CENSOC_NETCPU_LOGIT_PROCESSOR_H
#define CENSOC_NETCPU_LOGIT_PROCESSOR_H

// for the time-being model_factory_interface will be automatically declared, no need to include explicitly...
namespace censoc { namespace netcpu { namespace logit { 

template <typename N, typename F, typename EF>
struct task_processor {

	//{ typedefs...

	typedef netcpu::converger_1::message::meta<N, F, logit::message::meta<N> > meta_msg_type;
	typedef netcpu::converger_1::message::bulk<N, F, logit::message::bulk> bulk_msg_type;

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

	//}

	//{ data members

	size_type const alternatives;

	size_type const matrix_composite_rows; 
	size_type const matrix_composite_columns; 
	size_type const x_size;
	::std::vector<size_type> respondents_choice_sets;
	::std::vector<float_type> betas_mult_choice;

	netcpu::dataset_1::composite_matrix<size_type, int> matrix_composite; 

	bool reduce_exp_complexity;

	censoc::exp_lookup::linear_interpolation_linear_spacing<EF, size_type, 100> my_exp;

	//}

	// TODO -- later deprecate message.h typedef size_type and pass it via the template arg!!!
	task_processor(meta_msg_type const & meta_msg)	
	: alternatives(meta_msg.model.dataset.alternatives()), matrix_composite_rows(meta_msg.model.dataset.matrix_composite_rows()), matrix_composite_columns(meta_msg.model.dataset.matrix_composite_columns()), x_size(meta_msg.model.dataset.x_size()), 
	betas_mult_choice(alternatives),
	reduce_exp_complexity(meta_msg.model.reduce_exp_complexity()),
	my_exp(1)// .99999)
	{
		size_type const respondents_size(meta_msg.model.dataset.respondents_choice_sets.size());
		respondents_choice_sets.reserve(respondents_size);
		for (unsigned i(0); i != respondents_size; ++i)
			respondents_choice_sets.push_back(meta_msg.model.dataset.respondents_choice_sets(i));

		censoc::llog() << "ctor in logit::task_processor\n";
	}

public:

	size_type
	get_coefficients_size() const noexcept
	{	
		return x_size;
	}

	void
	on_bulk_read(bulk_msg_type const & bulk_msg)
	{
		assert(bulk_msg.coeffs.size() == get_coefficients_size());
		matrix_composite.cast(bulk_msg.model.dataset.matrix_composite.data(), matrix_composite_rows, matrix_composite_columns);
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

		for (size_type i(0), matrix_composite_row_i(0); i != respondents_choice_sets.size(); ++i) {

#ifdef __WIN32__
			if (!(i % 100) && netcpu::should_i_sleep == true) 
				return result.flag_incomplete_processing();
#endif

			extended_float_type respondent_probability(1);

			for (size_type t(0); t != respondents_choice_sets[i]; ++t, ++matrix_composite_row_i) {

				size_type const matrix_composite_thisrow_last_col_i(matrix_composite(matrix_composite_row_i, matrix_composite.cols() - 1));

				// setting first pass explicitly as opposed to additional call to 'set to zero' initially and then += ones...
				assert(x_size);
				// x == 0 (first pass)
				for (size_type a(0); a != alternatives; ++a) {
					int const control(matrix_composite(matrix_composite_row_i, a));
					switch (control) {
					case 0: 
						betas_mult_choice[a] = 0;
					break;
					case 1:
						betas_mult_choice[a] = coefficients[0].value();
					break;
					case -1:
						betas_mult_choice[a] = -coefficients[0].value();
					break;
					default:
						betas_mult_choice[a] = control * coefficients[0].value();
					}
				}
				// now to the rest of passes ( 1 <= x < x_size)
				for (size_type x(1), alternatives_columns_begin(alternatives); x != x_size; ++x) {
					for (size_type a(0); a != alternatives; ++a, ++alternatives_columns_begin) {
						int const control(matrix_composite(matrix_composite_row_i, alternatives_columns_begin));
						switch (control) {
						case 1:
							betas_mult_choice[a] += coefficients[x].value();
						break;
						case -1:
							betas_mult_choice[a] -= coefficients[x].value();
						break;
						default:
							betas_mult_choice[a] += coefficients[x].value() * control;
						}
					}
				}


				{
					extended_float_type accum;
					if (reduce_exp_complexity == false) {
						extended_float_type exp_alts;

						// TODO -- consider fixing this, the way it is now is rather stupid wrt cpu-caching

						// calculate 'exp' and aggregate across rows/alternatives
						{ // a = 0, first alternative
							accum =  
								//my_exp.eval(betas_mult_choice[0]); 
								::std::exp(betas_mult_choice[0]);
							if (!matrix_composite_thisrow_last_col_i)
								exp_alts = accum;
						}
						for (size_type a(1); a != alternatives; ++a) {
							extended_float_type const tmp(
								//my_exp.eval(betas_mult_choice[a])
								::std::exp(betas_mult_choice[a])
							);
							accum += tmp;
							if (a == matrix_composite_thisrow_last_col_i)
								exp_alts = tmp;
						}
						respondent_probability *=  exp_alts / accum;
						//accumulated_probability -=  ::std::log(exp_alts / accum);

					} else {

						{ // a =0, first alternative
							if (matrix_composite_thisrow_last_col_i) {
								accum = 
									//my_exp.eval(betas_mult_choice[0] - betas_mult_choice[matrix_composite_thisrow_last_col_i]); 
									::std::exp(betas_mult_choice[0] - betas_mult_choice[matrix_composite_thisrow_last_col_i]);
							} else 
								accum = 1;
						}
						for (size_type a(1); a != alternatives; ++a) {
							if (a != matrix_composite_thisrow_last_col_i) {
									accum += 
										//my_exp.eval(betas_mult_choice[a] - betas_mult_choice[matrix_composite_thisrow_last_col_i]); 
										::std::exp(betas_mult_choice[a] - betas_mult_choice[matrix_composite_thisrow_last_col_i]);
							} else
								accum += 1;
						}
						respondent_probability /=  accum;
						//accumulated_probability += ::std::log(accum);
					}
				}

			} // end loop for each choiceset, for a given respondent

			// prob should be between 0 and 1, so log is expected to be no greater than 0 (e.g. negative). so it is OK to use this value for early-exit test below...
			accumulated_probability -= ::std::log(respondent_probability);

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

		uint_fast64_t const matrix_composite_rows(meta_msg.model.dataset.matrix_composite_rows()); 
		uint_fast64_t const matrix_composite_columns(meta_msg.model.dataset.matrix_composite_columns()); 

		uint_fast64_t anticipated_ram_utilization(
			sizeof(int) * (
				matrix_composite_rows * matrix_composite_columns
			)
		); 

		// todo -- define the guard for app and it's mem structs more correctly as opposed to this ugly hack
		anticipated_ram_utilization += 17000000;
		
		if ((netcpu::cpu_affinity_index + 1) * anticipated_ram_utilization > static_cast<float_type>(censoc::sysinfo::ram_size()) * .8) 
			return false;
		else 
			return true;
	}

#ifndef NDEBUG
	// as a matter of debugging policy compliance, for the time-being
	void invalidate_caches_1() {}
#endif

};

netcpu::converger_1::model_factory<logit::task_processor, netcpu::models_ids::logit> static factory;

}}}

#endif
