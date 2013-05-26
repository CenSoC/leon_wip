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

#include <boost/lexical_cast.hpp>

#include <censoc/lexicast.h>
#include <censoc/rand.h>
#include <censoc/cdfinvn.h>
#include <censoc/finite_math.h>

#include <netcpu/types.h>
#include <netcpu/message.h>
#include <netcpu/dataset_1/controller.h>
#include <netcpu/dataset_1/message/meta.h>
#include <netcpu/dataset_1/message/bulk.h>


namespace censoc { namespace netcpu { namespace gmnl_2a { 

struct cdfinvn_hook {
	template <typename TMP, typename N>
	void inline 
	hook(TMP const &, N) 
	{
		// noop -- in this case must remember to pass data as a pointer to 'eval' of the normals obj. otherwise template type inference rules will make type T to be the actualy type (not the reference as per lvalue expression) and then the object will be passed by value (copy-ctor) and then not assigned in this hook and thusly the value modifications will be lost. If passing by pointer, then pointer will be copied (refenece to pointer then gets inferred to just a pointer) which is fine and then 'inplace' modifications will occure even with this empty hook.
	}
};

struct response_generator {

	// hacks for futurte templatisation (if need be) and for legacy ease of copy-n-pasing the code which came from previous design of controller/simulator and 2csv code blocks...
	typedef double F;
	typedef uint64_t N; 

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef typename netcpu::message::typepair<N>::wire size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef censoc::lexicast< ::std::string> xxx;

	struct messages_container_hack : netcpu::message::message_base<1> {
		netcpu::dataset_1::message::meta<N> meta_msg; 
		netcpu::dataset_1::message::bulk<N> bulk_msg;
	};

	response_generator(int argc, char * * argv)
	{
		//{ load csv 

		messages_container_hack messages;

		::std::vector<float_type> coefficients_values;
		coefficients_values.reserve(100);

		netcpu::dataset_1::composite_matrix_loader<N, F> dataset_loader;

		float_type gumbel_deviation(1);

		for (int i(1); i != argc; i += 2)
			if (i == argc - 1)
				throw ::std::runtime_error(xxx() << "every argument must have a value: [" << argv[i] << "]");
			else if (!::strcmp(argv[i], "--coefficients_values"))
				for (char const * value(::strtok(argv[i + 1], ",")); value != NULL; value = ::strtok(NULL, ","))
					coefficients_values.push_back(::boost::lexical_cast<float_type>(value));
			else if (!::strcmp(argv[i], "--gumbel_deviation"))
				gumbel_deviation = ::boost::lexical_cast<float_type>(argv[i + 1]);
			else if (dataset_loader.parse_arg(argv[i], argv[i + 1], messages.meta_msg, messages.bulk_msg) == false)
				throw netcpu::message::exception(xxx() << "unknown argument: [" << argv[i] << ']');

		if (coefficients_values.empty() == true)
			throw ::std::runtime_error("must supply --coefficients_values option");

		dataset_loader.verify_args(messages.meta_msg, messages.bulk_msg); // should throw
		//}

		//{ simulate
		::std::vector<uint8_t> choice_sets_alternatives(messages.bulk_msg.choice_sets_alternatives.size());
		::memcpy(choice_sets_alternatives.data(), messages.bulk_msg.choice_sets_alternatives.data(), choice_sets_alternatives.size());
		uint8_t * choice_sets_alternatives_ptr(choice_sets_alternatives.data());

		size_type const x_size(messages.meta_msg.x_size()); 
		::std::vector<float_type> coefficients(x_size * 2 + 4);

		for (size_type i(0); i != coefficients.size() && i != coefficients_values.size(); ++i)
			coefficients[i] = coefficients_values[i];;

		::boost::mt19937 rng;

		::std::vector<size_type> respondents_choice_sets;
		respondents_choice_sets.reserve(static_cast< ::std::size_t>(messages.bulk_msg.respondents_choice_sets.size()));
		for (size_type i(0); i != messages.bulk_msg.respondents_choice_sets.size(); ++i) 
			respondents_choice_sets.push_back(messages.bulk_msg.respondents_choice_sets(i));

		::std::vector<int8_t> matrix_composite(messages.bulk_msg.matrix_composite.size());
		for (size_type i(0); i != matrix_composite.size(); ++i)
			matrix_composite[i] = netcpu::message::deserialise_from_unsigned_to_signed_integral(messages.bulk_msg.matrix_composite(i));
		int8_t * matrix_composite_ptr(matrix_composite.data());

		::std::vector<float_type> vector_haltons_sigma(respondents_choice_sets.size());
		for (size_type i(0); i != respondents_choice_sets.size(); ++i)
			vector_haltons_sigma[i] = censoc::cdfn<float_type, -3>::eval() + censoc::rand_half<censoc::mt19937_ref>(rng).eval(censoc::cdfn<float_type, 3>::eval() - censoc::cdfn<float_type, -3>::eval());

		::std::vector<float_type> matrix_haltons_nonsigma(respondents_choice_sets.size() * (x_size + 1));
		for (size_type column(0); column != (x_size + 1); ++column)
			for (size_type row(0); row != respondents_choice_sets.size(); ++row)
				matrix_haltons_nonsigma[row + column * respondents_choice_sets.size()] = censoc::cdfn<float_type, -4>::eval() + censoc::rand_half<censoc::mt19937_ref>(rng).eval(censoc::cdfn<float_type, 4>::eval() - censoc::cdfn<float_type, -4>::eval());

		censoc::cdfinvn<float_type, size_type, cdfinvn_hook> normals_generator;  
		normals_generator.eval(vector_haltons_sigma.data(), vector_haltons_sigma.size());
		for (size_type column(0); column != (x_size + 1); ++column)
			normals_generator.eval(matrix_haltons_nonsigma.data() + column * respondents_choice_sets.size(), respondents_choice_sets.size());

		float_type tau(coefficients[2 * x_size]);
		float_type phi_mean(coefficients[2 * x_size + 1]);
		float_type phi_deviation(coefficients[2 * x_size + 2]);
		float_type phi_tau_corr(coefficients[2 * x_size + 2]);

		float_type const * const etavar(&coefficients[x_size]);

		// normalised sigma draws
		::std::vector<float_type> sigma_draws(respondents_choice_sets.size());
		float_type scaling_distribution_mean_normalisation(0);
		for (size_type i(0); i != respondents_choice_sets.size(); ++i) {
			float_type const sigma_tmp(tau * vector_haltons_sigma[i]);
			scaling_distribution_mean_normalisation += sigma_draws[i] = sigma_tmp < 0 ? 1 / (1 - sigma_tmp) : sigma_tmp + 1;
		}
		scaling_distribution_mean_normalisation = respondents_choice_sets.size() / scaling_distribution_mean_normalisation;
		for (size_type i(0); i != respondents_choice_sets.size(); ++i)
			sigma_draws[i] *= scaling_distribution_mean_normalisation;

		for (size_type i(0); i != respondents_choice_sets.size(); ++i) {

			assert(x_size);
			::std::vector<float_type> attributes_weights(x_size);
			for (size_type x(0); x != x_size; ++x) {
				float_type const tmp_tmp(etavar[x] * matrix_haltons_nonsigma[i + x * respondents_choice_sets.size()]);
				float_type const tmp(sigma_draws[i] * (coefficients[x] + tmp_tmp)
					+ phi_mean + phi_deviation * (phi_tau_corr * vector_haltons_sigma[i] + (1 - ::std::abs(phi_tau_corr)) * matrix_haltons_nonsigma[i + x_size * respondents_choice_sets.size()])
					);
				attributes_weights[x] = tmp;
			}

			for (size_type t(0); t != respondents_choice_sets[i]; ++t) {
				uint8_t const alternatives(*choice_sets_alternatives_ptr++);
				::std::vector<float_type> ev_cache_tmp_or_censor_cache_tmp(alternatives);
				float_type highest_probability(::boost::numeric::bounds<float_type>::lowest());
				for (size_type x(0); x != x_size; ++x) {
					for (size_type a(0); a != alternatives; ++a)
						ev_cache_tmp_or_censor_cache_tmp[a] += attributes_weights[x] * *matrix_composite_ptr+++ 
						// Gumbel 
						// Given a random variate U drawn from the uniform distribution in the interval (0, 1), the variate
						// X = mu - beta * ln(-ln(U)) 
						// The mean is mu + constant * beta; where constant = Euler-Mascheroni constant ~ 0.5772156649015328606
						-gumbel_deviation * (.5772156649015328606 + ::std::log(-::std::log(::std::numeric_limits<float_type>::min() + censoc::rand_half<censoc::mt19937_ref>(rng).eval(1 - ::std::numeric_limits<float_type>::min() * 2)))); 
				}
				size_type chosen_alternative;
				for (size_type a(0); a != alternatives; ++a) {
					if (ev_cache_tmp_or_censor_cache_tmp[a] <= ::boost::numeric::bounds<float_type>::lowest())
						throw ::std::runtime_error("'this_prob' is too low (min expressable in the format)");
					if (ev_cache_tmp_or_censor_cache_tmp[a] > highest_probability) {
						highest_probability = ev_cache_tmp_or_censor_cache_tmp[a];
						chosen_alternative = a;
					} 
				}
				*matrix_composite_ptr++ = chosen_alternative;

				if (censoc::was_fpu_ok(ev_cache_tmp_or_censor_cache_tmp.data()) == false) 
					throw ::std::runtime_error("fpu exception taken place for the chosen coefficients");
			}

		} // loop for all respondents
		//}

		//{ print out simulated csv 
		::std::cout << "respondent" << ',' << "choice_set" << ',' << "alternative" << ',' << "choice";
		for (size_type i(0); i != x_size; ++i)
			::std::cout << ',' << "attribute" << i + 1;
		::std::cout << '\n';

		choice_sets_alternatives_ptr = choice_sets_alternatives.data();
		matrix_composite_ptr = matrix_composite.data();

		for (size_type i(0); i != respondents_choice_sets.size(); ++i) {
			for (size_type t(0); t != respondents_choice_sets[i]; ++t) {
				uint8_t const alternatives(*choice_sets_alternatives_ptr++);
				int8_t const choice(matrix_composite_ptr[alternatives * x_size]);
				for (uint8_t a(0); a != alternatives; ++a) {
					::std::cout << i + 1 << ',' << t + 1 << ',' << a + 1 << ',';
					if (a == choice)
						::std::cout << 1;
					else
						::std::cout << 0;
					for (size_type x(0); x != x_size; ++x)
						::std::cout << ',' << static_cast<int>(matrix_composite_ptr[x * alternatives + a]);
					::std::cout << '\n';
				}
				matrix_composite_ptr += alternatives *  x_size + 1;
			}
		} 
		//}
	}

};

}}}

int 
main(int argc, char * * argv)
{
	::censoc::init_fpu_check();

	try {
		::censoc::netcpu::gmnl_2a::response_generator(argc, argv);
	} catch (::std::exception const & e) {
		::std::cout.flush();
		::censoc::llog() << ::std::endl;
		::std::cerr << "Runtime error: [" << e.what() << "]\n";
		return -1;
	} catch (...) {
		::std::cout.flush();
		::censoc::llog() << ::std::endl;
		::std::cerr << "Unknown runtime error.\n";
		return -1;
	}
	return 0;
}


