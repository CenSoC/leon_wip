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

/**
	@TODO -- this code is outdated (wrt eigen is being really deprecated)
 */

#include <fstream>
#include <vector>

#include <boost/numeric/conversion/bounds.hpp>
#include <boost/limits.hpp>


#include <censoc/lexicast.h>
#include <censoc/rand.h>
#include <censoc/halton.h>
#include <censoc/rnd_mirror_grid.h>
#include <censoc/finite_math.h>

#include <netcpu/message.h>
#include <netcpu/fstream_to_wrapper.h>

#include <netcpu/converger_1/message/res.h>
#include <netcpu/converger_1/message/meta.h>
#include <netcpu/converger_1/message/bulk.h>
#include "message/meta.h"
#include "message/bulk.h"
#include <netcpu/dataset_1/composite_matrix.h>

namespace censoc { namespace netcpu { namespace gmnl_2 {

typedef censoc::lexicast< ::std::string> xxx;

// TODO -- deprecate this feature
size_t const static error_injected_choicsets(0);

template <typename N, typename F>
struct halton_matrix_provider {

//{ typedefs...

	typedef F float_type;
	typedef ::Eigen::Array<float_type, ::Eigen::Dynamic, 1> vector_column_type;
	typedef ::Eigen::Array<float_type, ::Eigen::Dynamic, ::Eigen::Dynamic> matrix_columnmajor_type;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	struct ctor_type {
		vector_column_type * sigma;
		matrix_columnmajor_type * nonsigma;
	};
	typedef typename censoc::param<ctor_type>::type ctor_paramtype;

//}

	halton_matrix_provider(ctor_paramtype ctor)
	: sigma_(*ctor.sigma), nonsigma_(*ctor.nonsigma){
	}

	matrix_columnmajor_type & 
	nonsigma() 
	{
		return nonsigma_;
	}

	vector_column_type & 
	sigma() 
	{
		return sigma_;
	}

	matrix_columnmajor_type const & 
	nonsigma() const
	{
		return nonsigma_;
	}

	vector_column_type const & 
	sigma() const
	{
		return sigma_;
	}

	void
	nonsigma_resize(size_paramtype rows, size_paramtype columns)
	{
		nonsigma_.resize(rows, columns);
	}

	void
	sigma_resize(size_paramtype rows)
	{
		sigma_.resize(rows);
	}
protected:

	vector_column_type & sigma_;
	matrix_columnmajor_type & nonsigma_;
};

template <typename N, typename F>
struct cdfinvn_hook {

	typedef N size_type;
	typedef F float_type;
	typedef ::Eigen::Array<float_type, ::Eigen::Dynamic, 1> vector_column_type;
	typedef ::Eigen::Array<float_type, ::Eigen::Dynamic, ::Eigen::Dynamic> matrix_columnmajor_type;

	typedef typename censoc::param<size_type>::type size_paramtype;

	vector_column_type normal;
	template <typename TMP>
	void inline 
	hook(TMP const & x, N x_size) 
	{
#ifndef NDEBUG
		assert(static_cast<size_type>(x.size()) == x_size);
		for (size_type i(0); i != x_size; ++i) {
			assert(x[i] < 10);
			assert(x[i] > -10);
		}
#endif
		normal = x;
	}
};

template <typename float_type>
struct exponent_replacement {
	typedef typename censoc::param<float_type>::type float_paramtype;
	float_type 
	operator()(float_paramtype x) const 
	{ 
		if (x < 0)
			return 1 / (1 - x);
		else
			return x + 1;
	}
};

/**
	TODO -- MUST USE LONGEST DOUBLES ANYWAY (irrespective of the meta_msg specs)
	*/
template <typename N, typename F>
void static
simulate()
{
	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef ::Eigen::Array<float_type, ::Eigen::Dynamic, ::Eigen::Dynamic> matrix_columnmajor_type;
	typedef ::Eigen::Array<float_type, ::Eigen::Dynamic, 1> vector_column_type;

	//typedef censoc::halton<size_type, float_type, halton_matrix_provider<N, F> > haltons_type;
	typedef censoc::rnd_mirror_grid<size_type, float_type, halton_matrix_provider<N, F> > haltons_type;

	typedef netcpu::converger_1::message::meta<N, F, gmnl_2::message::meta<N> > meta_msg_type;
	typedef netcpu::converger_1::message::bulk<N, F, gmnl_2::message::bulk> bulk_msg_type;


#if 0

	0.3054, 1.0000, 0.1095, 0.6456, 0.0538, 0.6971, 0.1952, 0.7779, 
	0.7557, 0.7065, 0.5271, 0.6312, 0.6084, 0.0098, 0.2723, 0.6815, 
	0.2172, 1.7000

	0.279321, 0.999486, 0.0550412, 0.874486, -0.0236626, 0.998457, 0.299897, 0.998971, 
	1.37354, 1.49854, 0.995605, 1.20166, 1.16748, 0.132324, 0.344727, 1.31104, 
	0.998264, 0.436214 

	0.332305 0.999486 -0.0221193 0.97428 -0.104424 0.998457 0.310185 0.776749 
	1.43701 1.49902 1.09717 1.43408 1.28809 0.00146484 0.11377 1.49951 
	0.998843 0.317387

	with 6 steps and 1.9 range for variances
	todo -- do try with 7 steps?
	0.282407 0.998971 0.0519547 0.89249 -0.0396091 0.998971 0.307613 0.998971 1.43045 1.59222 1.02513 1.23355 1.20871 0.139955 0.362915 1.3626 0.999349 0.430041
	float_type const coefficients[] = { 
	0.282407, 0.998971, 0.0519547, 0.89249, -0.0396091, 0.998971, 0.307613, 0.998971, 
	1.43045, 1.59222, 1.02513, 1.23355, 1.20871, 0.139955, 0.362915, 1.3626, 
	0.999349, 0.430041
	};


	float_type const coefficients[] = { 
	0.3054, 1.0000, 0.1095, 0.6456, 0.0538, 0.6971, 0.1952, 0.7779, 
	0.7557 / 2, 0.7065 / 2, 0.5271 / 2, 0.6312 / 2, 0.6084 / 2, 0.0098 / 2, 0.2723 / 2, 0.6815 / 2, 
	0.2172, 1.7000
	};

	float_type const coefficients[] = { 
0.311728, 0.998457, 0.103395, 0.998457, -0.0329218, 0.884259, 0.301955, 0.998971, 0.562207, 0.822437, 0.319604, 0.351611, 0.305688, 0.000463867, 0.0194824, 0.500977, 0.572338, 0.556584
	};

	float_type const coefficients[] = { 
0.325617, 0.998971, 0.112654, 0.998971, -0.0715021, 0.998457, 0.352366, 0.998457, 0.582153, 1.05159, 0.200854, 0.415625, 0.190649, 0.0213379, 0.000463867, 0.583081, 0.225694, 1.49486
	};
	float_type const coefficients[] = { 
0.320988, 0.998971, 0.114198, 0.998457, -0.141461, 0.998971, 0.41358, 0.998971, 
0.610913, 1.28398, 0.000463867, 0.513501, 0.000463867, 0.0013916, 0.0013916, 0.630396, 0.204282, 1.2963
	};

	float_type const coefficients[] = { 
	0.3054, 1.0000, 0.1095, 0.6456, 0.0538, 0.6971, 0.1952, 0.7779, 
	0.7557, 0.7065, 0.5271, 0.6312, 0.6084, 0.0098, 0.2723, 0.6815, 
	0.2172, 1.7000
	};

0.3054, 1.0000, 0.1095, 0.6456, 0.0538,  
0.7557, 0.7065, 0.5271, 0.6312, 0.6084,  
0.2172, 1.7000

#endif

#if 0
	float_type const coefficients[] = { 
0.3054, 1.0000, 0.1095, 0.6456,   
0.7557, 0.7065, 0.5271, 0.6312,   
1.7000
	};
#endif

#if 0
	float_type const coefficients[] = { 
0.3054, 1.0000, 0.1095, 0.6456,   
0, 0, 0, 0,   
1.7000
	};
#endif
#if 0
	float_type const coefficients[] = { 
0.3054, 1.0000, 0.1095, 0.6456,   
0.7557, 0.7065, 0.5271, 0.6312,   
0
	};
#endif

	float_type const coefficients[] = { 
1 * 0.3054, 1 * 1.0000,   
1 * 0.7557, 1 * 0.7065,   
0 * 1.7000
	};


	netcpu::message::read_wrapper msg_wire;

	::std::ifstream meta_msg_file("meta.msg", ::std::ios::binary);
	if (meta_msg_file == false)
		throw ::std::runtime_error("missing meta message during load");
	netcpu::message::fstream_to_wrapper(meta_msg_file, msg_wire);
	meta_msg_type meta_msg;
	meta_msg.from_wire(msg_wire);
	meta_msg.print();

	size_type const alternatives(meta_msg.model.dataset.alternatives());
	size_type const repetitions(meta_msg.model.repetitions()); 
	size_type const matrix_composite_rows(meta_msg.model.dataset.matrix_composite_rows()); 
	size_type const matrix_composite_columns(meta_msg.model.dataset.matrix_composite_columns()); 
	size_type const x_size(meta_msg.model.dataset.x_size()); 
	assert(x_size == ((sizeof(coefficients) / sizeof(float_type)) - 1) / 2);
	::std::vector<size_type> respondents_choice_sets;
	respondents_choice_sets.reserve(static_cast< ::std::size_t>(meta_msg.model.dataset.respondents_choice_sets.size()));
	for (size_type i(0); i != meta_msg.model.dataset.respondents_choice_sets.size(); ++i) 
		respondents_choice_sets.push_back(meta_msg.model.dataset.respondents_choice_sets(i));

	::std::ifstream bulk_msg_file("bulk.msg", ::std::ios::binary);
	if (bulk_msg_file == false)
		throw ::std::runtime_error("missing bulk message during load");
	netcpu::message::fstream_to_wrapper(bulk_msg_file, msg_wire);
	bulk_msg_type bulk_msg;
	bulk_msg.from_wire(msg_wire);


	netcpu::dataset_1::composite_matrix<size_type, int> matrix_composite; 
	matrix_composite.cast(bulk_msg.model.dataset.matrix_composite.data(), matrix_composite_rows, matrix_composite_columns);


	size_type const expanded_rows(matrix_composite_rows + respondents_choice_sets.size() * error_injected_choicsets);
	bulk_msg.model.dataset.matrix_composite.resize(expanded_rows * matrix_composite_columns);
	netcpu::dataset_1::composite_matrix_map<size_type, uint8_t> matrix_composite_dst(bulk_msg.model.dataset.matrix_composite.data(), expanded_rows, matrix_composite_columns);

	vector_column_type vector_haltons_sigma;
	matrix_columnmajor_type matrix_haltons_nonsigma;



	//
	// now using 'true' randomness instead, so no haltons-like stuff for the time-being...
	//
#if 0
	typename halton_matrix_provider<N, F>::ctor_type haltons_ctor;
	haltons_ctor.sigma = &vector_haltons_sigma;
	haltons_ctor.nonsigma = &matrix_haltons_nonsigma;
	haltons_type haltons(haltons_ctor);
	haltons.init(1, respondents_choice_sets.size(), x_size);
	//
#else
	censoc::cdfinvn<float_type, size_type, cdfinvn_hook<size_type, float_type> > normals_generator;  
	// ... and so here comes the random stuff ...
	// sigma
	vector_haltons_sigma.resize(respondents_choice_sets.size());
	for (size_type i(0); i != respondents_choice_sets.size(); ++i)
		vector_haltons_sigma[i] = haltons_type::sigma_min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(haltons_type::sigma_max() - haltons_type::sigma_min());
	normals_generator.eval(vector_haltons_sigma, vector_haltons_sigma.size());
	vector_haltons_sigma = normals_generator.normal;
	// non sigma...
	matrix_haltons_nonsigma.resize(respondents_choice_sets.size(), x_size);
	for (size_type i(0); i != x_size; ++i) {
		for (size_type j(0); j != respondents_choice_sets.size(); ++j)
			matrix_haltons_nonsigma(j, i) = haltons_type::non_sigma_min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(haltons_type::non_sigma_max() - haltons_type::non_sigma_min());
		normals_generator.eval(matrix_haltons_nonsigma.col(i), matrix_haltons_nonsigma.col(i).size());
		matrix_haltons_nonsigma.col(i) = normals_generator.normal;
	}
	// ... done with "trully" random stuff
#endif

	float_paramtype tau(coefficients[2 * x_size]);

	float_type const * const etavar(coefficients + x_size);

	censoc::rand_half<censoc::rand<uint32_t> > rnd;

	//float_type const mean_normalisation(-::std::log((vector_haltons_sigma * tau).cwise().exp().sum() / respondents_choice_sets.size()));
	//float_type const mean_normalisation(respondents_choice_sets.size() / (vector_haltons_sigma * tau).unaryExpr(exponent_replacement<float_type>()).sum());

	//float_type const mean_normalisation(::std::exp(-haltons_type::sigma_trunc * tau) + haltons_type::sigma_trunc);

#if 0
	float_type mean_normalisation;
	{
		vector_column_type uniform(repetitions);

		float_type const from(haltons_type::sigma_min());
		float_type const to(haltons_type::sigma_max());

		float_type const step_size((to - from) / repetitions);
		float_type const step_half_size(step_size * .5);

		for (size_type i(0); i != repetitions; ++i) {
			uniform[i] = from + (step_size * i) + step_half_size;
			// fp paranoia...
			if (uniform[i] >= to)
				uniform[i] = to;
		}

		censoc::cdfinvn<float_type, size_type, cdfinvn_hook<size_type, float_type> > surveyside_postnormalisation_cdfinvn;  
		surveyside_postnormalisation_cdfinvn.eval(uniform);
		assert(surveyside_postnormalisation_cdfinvn.normal.size() == uniform.size() && static_cast<size_type>(uniform.size()) == repetitions);

		mean_normalisation = repetitions / (surveyside_postnormalisation_cdfinvn.normal * tau).unaryExpr(exponent_replacement<float_type>()).sum();
		//mean_normalisation = repetitions / (surveyside_postnormalisation_cdfinvn.normal * tau).cwise().exp().sum();
	}
#endif

	for (size_type i(0), matrix_composite_row_i(0), matrix_composite_dst_row_i(0); i != respondents_choice_sets.size(); ++i) {

#if 0
		float_type const sigma(
			//1
			::std::exp(tau * vector_haltons_sigma[i] 
				//- tau * tau * .5
				// + mean_normalisation
			)
		);
#else
		float_type const sigma_tmp(
				tau * vector_haltons_sigma[i]
				);
		float_type const sigma(
				(sigma_tmp < 0 ? 1 / (1 - sigma_tmp) : sigma_tmp + 1) 
				//::std::exp(sigma_tmp)
				//* mean_normalisation
		);
		//float_type const sigma(
		//		(sigma_tmp  + mean_normalisation) / mean_normalisation
		//);
#endif

		assert(x_size);
		matrix_columnmajor_type ev_cache_tmp_or_censor_cache_tmp(static_cast<int>(respondents_choice_sets[i]), static_cast<int>(alternatives));
		ev_cache_tmp_or_censor_cache_tmp.setZero();
		for (size_type x(0); x != x_size; ++x) {
			size_type const alternatives_columns_begin(x * alternatives);
			float_type const tmp_tmp(etavar[x] * matrix_haltons_nonsigma(i, x));
			float_type const tmp(
					sigma * 
					(
					coefficients[x] + 
					tmp_tmp
					)
					);
			for (size_type a(0); a != alternatives; ++a)
				for (size_type t(0), matrix_composite_row_tmp(matrix_composite_row_i); t != respondents_choice_sets[i]; ++t, ++matrix_composite_row_tmp)
						ev_cache_tmp_or_censor_cache_tmp(t, a) += tmp * matrix_composite(matrix_composite_row_tmp, alternatives_columns_begin + a);
		}

		matrix_columnmajor_type matrix_error(static_cast<int>(respondents_choice_sets[i]), static_cast<int>(alternatives));
		for (size_type a(0); a != alternatives; ++a) {

#if 0
			// Laplace
			// Given a random variable U drawn from the uniform distribution in the interval [-1/2, 1/2), the random variable
			// X = mu - beta * sgn(U) * ln(1 - 2|U|) 
			// ... not yet implemented a possible todo for future
#endif

#if 1
			// Gumbel 
			// Given a random variate U drawn from the uniform distribution in the interval (0, 1), the variate
			// X = mu - beta * ln(-ln(U)) 
			// The mean is mu + constant * beta; where constant = Euler-Mascheroni constant ~ 0.5772156649015328606
			for (size_type t(0); t != respondents_choice_sets[i]; ++t)
				//matrix_error(t, a) = -sigma * (.5772156649015328606 + ::std::log(-::std::log(::std::numeric_limits<float_type>::min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(1 - ::std::numeric_limits<float_type>::min() * 2))));
				//matrix_error(t, a) =  -1.0 * ::std::log(-::std::log(::std::numeric_limits<float_type>::min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(1 - ::std::numeric_limits<float_type>::min() * 2)));
				matrix_error(t, a) =  -(.5772156649015328606 + ::std::log(-::std::log(::std::numeric_limits<float_type>::min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(1 - ::std::numeric_limits<float_type>::min() * 2))));
				//matrix_error(t, a) =  -(.5772156649015328606 + ::std::log(-::std::log(.2 + censoc::rand_half<censoc::rand<uint32_t> >().eval(.6))));
#endif

#if 0
			// Uniform -- ignorant hacking atm
			for (size_type t(0); t != respondents_choice_sets[i]; ++t)
				matrix_error(t, a) = censoc::rand_full<censoc::rand<uint32_t> >().eval(static_cast<float_type>(.5));
#endif

		}
		ev_cache_tmp_or_censor_cache_tmp += matrix_error;

		if (censoc::was_fpu_ok(ev_cache_tmp_or_censor_cache_tmp.data()) == false) 
			throw ::std::runtime_error("fpu exception taken place for the chosen coefficients");

		size_type tmp_start_i(matrix_composite_row_i);
		for (size_type t(0); t != respondents_choice_sets[i]; ++t, ++matrix_composite_row_i, ++matrix_composite_dst_row_i) {

			matrix_composite_dst.copy_raw_row(matrix_composite, matrix_composite_row_i, matrix_composite_dst_row_i);

			size_type matrix_composite_thisrow_last_col_i(matrix_composite(matrix_composite_row_i, matrix_composite.cols() - 1));

			float_type highest_probability(::boost::numeric::bounds<float_type>::lowest());
			for (size_type a(0); a != alternatives; ++a) {
				float_type const this_prob(ev_cache_tmp_or_censor_cache_tmp(t, a));

				if (this_prob <= ::boost::numeric::bounds<float_type>::lowest())
					throw ::std::runtime_error("'this_prob' is too low (min expressable in the format)");

				if (this_prob > highest_probability) {
					highest_probability = this_prob;
					//matrix_composite(matrix_composite_row_i, matrix_composite.cols() - 1) = a;
					matrix_composite_dst(matrix_composite_dst_row_i, matrix_composite.cols() - 1) = a;
				} 
			}

		}

		// here add the error injected thingies
		for (size_type e(0); e != error_injected_choicsets; ++e, ++matrix_composite_dst_row_i) {
			size_type const tmp_row(tmp_start_i + censoc::rand_nobias<censoc::rand<uint32_t> >().eval(respondents_choice_sets[i]));
			matrix_composite_dst.copy_raw_row(matrix_composite, tmp_row, matrix_composite_dst_row_i);
			while (!0) {
				size_type const tmp_alternative(censoc::rand_nobias<censoc::rand<uint32_t> >().eval(alternatives));
				matrix_composite_dst(matrix_composite_dst_row_i, matrix_composite.cols() - 1) = tmp_alternative;
				break;
			}
		}

	} // loop for all respondents

	netcpu::message::write_wrapper write_raw; 
	bulk_msg.to_wire(write_raw);
	::std::ofstream new_bulk_msg_file("new_bulk.msg", ::std::ios::binary | ::std::ios::trunc);
	new_bulk_msg_file.write(reinterpret_cast<char const *>(write_raw.head()), write_raw.size());
	if (new_bulk_msg_file == false)
		throw ::std::runtime_error("could not write new_bulk.msg");

	for (size_type i(0); i != meta_msg.model.dataset.respondents_choice_sets.size(); ++i) 
		meta_msg.model.dataset.respondents_choice_sets(i) += error_injected_choicsets;
}

template <typename IntRes>
void 
simulate(netcpu::converger_1::message::res const & msg)
{
	// todo -- single floats should, theoretically, be also tested with double -- ie simulate with double, but estimate with floats!
	// but the message itself is already "conditioned" to deal with "float_res" types! so even if, internally, such float_res is represented as uint64_t, the code here should be robust- correct-enough to pass "processing F in simulation" type explicitly to "simulate" method...
	if (msg.float_res() == netcpu::converger_1::message::float_res<float>::value)
		simulate<IntRes, float>(); 
	else if (msg.float_res() == netcpu::converger_1::message::float_res<double>::value)
		simulate<IntRes, double>();
	else // TODO -- may be as per 'on_read' -- write a bad message to client (more verbose)
		throw ::std::runtime_error(xxx("unsupported floating resolution: [") << msg.float_res() << ']');
}

void
main()
{
		censoc::init_fpu_check();

		::std::ifstream msg_file("res.msg", ::std::ios::binary);
		if (msg_file == false)
			throw ::std::runtime_error("missing res message during load");

		netcpu::message::read_wrapper msg_wire;
		netcpu::message::fstream_to_wrapper(msg_file, msg_wire);
		netcpu::converger_1::message::res msg;
		msg.from_wire(msg_wire);
		switch (msg.int_res()) {
			case netcpu::converger_1::message::int_res<uint32_t>::value :
			simulate<uint32_t>(msg);
		break;
			case netcpu::converger_1::message::int_res<uint64_t>::value :
			simulate<uint64_t>(msg);
		break;
			throw ::std::runtime_error(xxx("unsupported resolution(=") << msg.int_res() << ")");
		}
}

}}}

int 
main()
{
	::censoc::netcpu::gmnl_2::main();
	return 0;
}
