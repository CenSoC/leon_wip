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

#include <fstream>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/numeric/conversion/bounds.hpp>
#include <boost/limits.hpp>

#include <censoc/lexicast.h>
#include <censoc/rand.h>
#include <censoc/cdfinvn.h>
#include <censoc/finite_math.h>

#include <netcpu/models_ids.h>
#include <netcpu/message.h>
#include <netcpu/fstream_to_wrapper.h>

#include <netcpu/converger_1/message/res.h>
#include <netcpu/converger_1/message/meta.h>
#include <netcpu/converger_1/message/bulk.h>
#include <netcpu/gmnl_2/message/meta.h>
#include <netcpu/gmnl_2/message/bulk.h>

namespace censoc { namespace netcpu { namespace gmnl_2a {

typedef censoc::lexicast< ::std::string> xxx;

struct cdfinvn_hook {
	template <typename TMP, typename N>
	void inline 
	hook(TMP const &, N) 
	{
		// noop -- in this case must remember to pass data as a pointer to 'eval' of the normals obj. otherwise template type inference rules will make type T to be the actualy type (not the reference as per lvalue expression) and then the object will be passed by value (copy-ctor) and then not assigned in this hook and thusly the value modifications will be lost. If passing by pointer, then pointer will be copied (refenece to pointer then gets inferred to just a pointer) which is fine and then 'inplace' modifications will occure even with this empty hook.
	}
};

template <typename N, typename F>
void static
simulate()
{
	typedef F float_type;

	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef netcpu::converger_1::message::meta<N, F, gmnl_2::message::meta<N> > meta_msg_type;
	typedef netcpu::converger_1::message::bulk<N, F, gmnl_2::message::bulk<N> > bulk_msg_type;

	netcpu::message::read_wrapper msg_wire;

	::std::ifstream meta_msg_file("meta.msg", ::std::ios::binary);
	if (!meta_msg_file)
		throw ::std::runtime_error("missing meta message during load");
	netcpu::message::fstream_to_wrapper(meta_msg_file, msg_wire);
	meta_msg_type meta_msg;
	meta_msg.from_wire(msg_wire);

	::std::ifstream bulk_msg_file("bulk.msg", ::std::ios::binary);
	if (!bulk_msg_file)
		throw ::std::runtime_error("missing bulk message during load");
	netcpu::message::fstream_to_wrapper(bulk_msg_file, msg_wire);
	bulk_msg_type bulk_msg;
	bulk_msg.from_wire(msg_wire);

	::std::vector<uint8_t> choice_sets_alternatives(bulk_msg.model.dataset.choice_sets_alternatives.size());
	::memcpy(choice_sets_alternatives.data(), bulk_msg.model.dataset.choice_sets_alternatives.data(), choice_sets_alternatives.size());
	uint8_t * choice_sets_alternatives_ptr(choice_sets_alternatives.data());

#if 0
	size_type const repetitions(meta_msg.model.repetitions()); 
#endif
	size_type const x_size(meta_msg.model.dataset.x_size()); 

	::std::vector<double> coefficients(x_size * 2 + 2);

	//::std::string const task_name(::boost::filesystem::path(::boost::filesystem::canonical(::boost::filesystem::current_path()).branch_path()).leaf().string());
	// todo -- use this when installed boost version supports it...
	//::std::string const task_name(::boost::filesystem::canonical(::boost::filesystem::current_path()).leaf().string());
	// otherwise, using older, deprecated-but-still-present
	::std::string const task_name(::boost::filesystem::current_path().normalize().leaf().string());

	::std::string::size_type begin_pos(task_name.find_first_of('_'));
	if (begin_pos == ::std::string::npos || task_name.size() <= ++begin_pos)
		throw ::std::runtime_error(xxx("incorrect path found: [") << ::boost::filesystem::current_path() << ']' );
	::std::string::size_type end_pos(task_name.find_first_of('-'));
	if (end_pos == ::std::string::npos)
		throw ::std::runtime_error(xxx("incorrect path found: [") << ::boost::filesystem::current_path() << ']' );

	unsigned const model_id(censoc::lexicast<unsigned>(task_name.substr(begin_pos, end_pos)));

	switch (model_id) {
	case netcpu::models_ids::gmnl_2a:
	coefficients[x_size * 2 + 1] = -2;
	case netcpu::models_ids::gmnl_2:
	coefficients[x_size * 2] = 1.7;
	case netcpu::models_ids::mixed_logit:
	for (unsigned i(0); i != x_size; ++i)
		coefficients[i + x_size] = censoc::rand_half<censoc::rand<uint32_t> >().eval(3.);
	case netcpu::models_ids::logit:
	for (unsigned i(0); i != x_size; ++i)
		coefficients[i] = censoc::rand_full<censoc::rand<uint32_t> >().eval(5.);
	break;
	default:
		throw ::std::runtime_error("unsupported model");
	}

	::std::cerr << "simulated coefficients:\n";
	for (unsigned i(0); i != coefficients.size(); ++i)
		::std::cerr << coefficients[i] << ' ' ;
	::std::cerr << ::std::endl;

	::std::vector<size_type> respondents_choice_sets;
	respondents_choice_sets.reserve(static_cast< ::std::size_t>(bulk_msg.model.dataset.respondents_choice_sets.size()));
	for (size_type i(0); i != bulk_msg.model.dataset.respondents_choice_sets.size(); ++i) 
		respondents_choice_sets.push_back(bulk_msg.model.dataset.respondents_choice_sets(i));


	::std::vector<float_type> matrix_composite(bulk_msg.model.dataset.matrix_composite.size());
	for (size_type i(0); i != matrix_composite.size(); ++i)
		matrix_composite[i] = netcpu::message::deserialise_from_decomposed_floating<float_type>(bulk_msg.model.dataset.matrix_composite(i));
	float_type * matrix_composite_ptr(matrix_composite.data());

	::std::vector<uint8_t> choice_column(bulk_msg.model.dataset.choice_column.size());
	for (size_type i(0); i != choice_column.size(); ++i)
		choice_column[i] = bulk_msg.model.dataset.choice_column(i);
	uint8_t * choice_column_ptr(choice_column.data());

	::std::vector<double> vector_haltons_sigma(respondents_choice_sets.size());
	for (size_type i(0); i != respondents_choice_sets.size(); ++i)
		vector_haltons_sigma[i] = censoc::cdfn<double, -3>::eval() + censoc::rand_half<censoc::rand<uint32_t> >().eval(censoc::cdfn<double, 3>::eval() - censoc::cdfn<double, -3>::eval());

	::std::vector<double> matrix_haltons_nonsigma(respondents_choice_sets.size() * x_size);
	for (size_type column(0); column != x_size; ++column)
		for (size_type row(0); row != respondents_choice_sets.size(); ++row)
			matrix_haltons_nonsigma[row + column * respondents_choice_sets.size()] = censoc::cdfn<double, -4>::eval() + censoc::rand_half<censoc::rand<uint32_t> >().eval(censoc::cdfn<double, 4>::eval() - censoc::cdfn<double, -4>::eval());

	censoc::cdfinvn<double, size_type, cdfinvn_hook> normals_generator;  
	normals_generator.eval(vector_haltons_sigma.data(), vector_haltons_sigma.size());
	for (size_type column(0); column != x_size; ++column)
		normals_generator.eval(matrix_haltons_nonsigma.data() + column * respondents_choice_sets.size(), respondents_choice_sets.size());

	double phi(coefficients[2 * x_size + 1]);
	double tau(coefficients[2 * x_size]);

	double const * const etavar(&coefficients[x_size]);

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

	for (size_type i(0); i != respondents_choice_sets.size(); ++i) {

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

		::std::vector<double> attributes_weights(x_size);
		for (size_type x(0); x != x_size; ++x) {
			double const tmp_tmp(etavar[x] * matrix_haltons_nonsigma[i + x * respondents_choice_sets.size()]);
			double const tmp(
					sigma * 
					(
					coefficients[x] + 
					tmp_tmp
					)
					+ phi
					);
			attributes_weights[x] = tmp;
		}

		for (size_type t(0); t != respondents_choice_sets[i]; ++t) {
			uint8_t const alternatives(*choice_sets_alternatives_ptr++);
			::std::vector<double> ev_cache_tmp_or_censor_cache_tmp(alternatives);
			double highest_probability(::boost::numeric::bounds<double>::lowest());
			for (size_type x(0); x != x_size; ++x) {
				for (size_type a(0); a != alternatives; ++a)
					ev_cache_tmp_or_censor_cache_tmp[a] += attributes_weights[x] * *matrix_composite_ptr+++
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
						// -sigma * (.5772156649015328606 + ::std::log(-::std::log(::std::numeric_limits<double>::min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(1 - ::std::numeric_limits<double>::min() * 2))));
						// -1.0 * ::std::log(-::std::log(::std::numeric_limits<double>::min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(1 - ::std::numeric_limits<double>::min() * 2)));
						-(.5772156649015328606 + ::std::log(-::std::log(::std::numeric_limits<double>::min() + censoc::rand_half<censoc::rand<uint32_t> >().eval(1 - ::std::numeric_limits<double>::min() * 2))));
						// -(.5772156649015328606 + ::std::log(-::std::log(.2 + censoc::rand_half<censoc::rand<uint32_t> >().eval(.6))));
#endif
#if 0
						// Uniform -- ignorant hacking atm
						censoc::rand_full<censoc::rand<uint32_t> >().eval(static_cast<double>(.5));
#endif
			}
			size_type chosen_alternative;
			for (size_type a(0); a != alternatives; ++a) {
				if (ev_cache_tmp_or_censor_cache_tmp[a] <= ::boost::numeric::bounds<double>::lowest())
					throw ::std::runtime_error("'this_prob' is too low (min expressable in the format)");
				if (ev_cache_tmp_or_censor_cache_tmp[a] > highest_probability) {
					highest_probability = ev_cache_tmp_or_censor_cache_tmp[a];
					chosen_alternative = a;
				} 
			}
			*choice_column_ptr++ = chosen_alternative;

			if (censoc::was_fpu_ok(ev_cache_tmp_or_censor_cache_tmp.data()) == false) 
				throw ::std::runtime_error("fpu exception taken place for the chosen coefficients");
		}

	} // loop for all respondents

	// todo -- rather crude, only need to overwrite the chosen alternative value (last in the 'row').
	for (size_type i(0); i != matrix_composite.size(); ++i)
		netcpu::message::serialise_to_decomposed_floating(matrix_composite[i], bulk_msg.model.dataset.matrix_composite(i));

	for (size_type i(0); i != choice_column.size(); ++i)
		bulk_msg.model.dataset.choice_column(i, choice_column[i]);

	netcpu::message::write_wrapper write_raw; 
	bulk_msg.to_wire(write_raw);
	::std::ofstream new_bulk_msg_file("new_bulk.msg", ::std::ios::binary | ::std::ios::trunc);
	new_bulk_msg_file.write(reinterpret_cast<char const *>(write_raw.head()), write_raw.size());
	if (!new_bulk_msg_file)
		throw ::std::runtime_error("could not write new_bulk.msg");
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
		if (!msg_file)
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
	::censoc::netcpu::gmnl_2a::main();
	return 0;
}
