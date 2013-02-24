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
	@note -- currently not really a unit test...

	@TODO -- this test is outdated (wrt eigen is being really deprecated)
 */

#include <iostream>
#include <vector>

#include <sys/types.h>

#include <boost/asio.hpp>

#include <censoc/finite_math.h>
#include <censoc/rand.h>
#include <censoc/cdfinvn.h>

typedef double float_type;

struct postcdfinvn {
	::std::vector<float_type> normal;
	template <typename TMP>
	void inline 
	hook(TMP const & x, unsigned /* x_size */) 
	{
#ifndef NDEBUG
		for (unsigned i(0); i != static_cast<unsigned>(x.size()); ++i) {
			assert(x[i] < 10);
			assert(x[i] > -10);
		}
#endif
		normal = x;
	}
};

void
append(float_type & norm, float_type & exp, unsigned & norm_size, unsigned & exp_size, float_type tmp)
{
	norm += tmp;
	exp += ::std::exp(tmp);
	++norm_size;
	++exp_size;
}

struct exponent_replacement {
	float_type 
	operator()(float_type x) const 
	{ 
		if (x < 0)
			return 1 / (1 - x);
		else
			return x + 1;
	}
};

void static
do_one(unsigned draws_size, float_type width, ::std::vector<float_type> const & uniform)
{

	::censoc::cdfinvn<float_type, unsigned, postcdfinvn> cdfinvn;  
	cdfinvn.eval(uniform, uniform.size());
	assert(cdfinvn.normal.size() == uniform.size() && static_cast<unsigned>(uniform.size()) == draws_size);

	float_type above(0), below(0), exp_above(0), exp_below(0);
	unsigned above_size(0), below_size(0), exp_above_size(0), exp_below_size(0);
	for (unsigned i(0); i != static_cast<unsigned>(uniform.size()); ++i) {
		float_type tmp(cdfinvn.normal[i] * width);
		if (tmp > 0) 
			append(above, exp_above, above_size, exp_above_size, tmp);
		else 
			append(below, exp_below, below_size, exp_below_size, tmp);
	}

	::std::cout << "norm above mean(=" << above / above_size << ")\n";
	::std::cout << "norm below mean(=" << below / below_size << ")\n";
	::std::cout << "exp above mean(=" << exp_above / exp_above_size << ")\n";
	::std::cout << "exp below mean(=" << exp_below / exp_below_size << ")\n";
	{
		float_type sum(0);
		for (unsigned i(0); i != cdfinvn.normal.size(); ++i)
			sum += ::std::exp(cdfinvn.normal[i] * width);
		sum /= draws_size;
		::std::cout << "exp_mean(=" << sum << ")\n\n";
	}
	{
		// short hack for the custom mean...
		exponent_replacement er;
		float_type sum(0);
		for (unsigned i(0); i != cdfinvn.normal.size(); ++i)
			sum += er(cdfinvn.normal[i] * width);
		sum /= draws_size;
		::std::cout << "custom_mean(=" << sum << ")\n\n";
	}
}

void static
mirror(::std::vector<float_type> & v, unsigned const draws_half_size)
{
	for (unsigned i(0); i != draws_half_size; ++i)
		v[i + draws_half_size] =  1 - v[draws_half_size];
}

void static
run(unsigned draws_size, float_type width)
{

	::std::cout << "=======\n\n";
	::std::cout << ">>> draws(=" << draws_size <<"), width(=" << width << ")\n\n";
	::std::vector<float_type> uniform(draws_size);
	assert(!(draws_size % 2));

	float_type const from(::censoc::cdfn<float_type, -4>::eval());
	assert(from < .5 && from > 0);

	float_type const to(.5 - ::std::numeric_limits<float_type>::epsilon());
	unsigned const draws_half_size(draws_size >> 1);
	float_type const step_size((to - from) / draws_half_size);

	::std::cout << "uniform/even coverage towards the outer grid boundary\n";
	for (unsigned i(0); i != draws_half_size; ++i)
		uniform[i] = to - (step_size * i) - step_size;
	mirror(uniform, draws_half_size);
	do_one(draws_size, width, uniform);

	::std::cout << "uniform/even coverage towards the inner grid boundary\n";
	for (unsigned i(0); i != draws_half_size; ++i)
		uniform[i] = to - (step_size * i);
	mirror(uniform, draws_half_size);
	do_one(draws_size, width, uniform);

	::std::cout << "uniform/even coverage balanced on the middle of the grid boundary\n";
	float_type const step_half_size(step_size *.5);
	for (unsigned i(0); i != draws_half_size; ++i)
		uniform[i] = to - (step_size * i) - step_half_size;
	mirror(uniform, draws_half_size);
	do_one(draws_size, width, uniform);

	::std::cout << "trully random\n";
	for (unsigned x(0); x != draws_size; ++x) 
		uniform[x] = ::censoc::cdfn<float_type, -4>::eval() + ::censoc::rand_half< ::censoc::rand<uint32_t> >().eval(censoc::cdfn<float_type, 4>::eval() - censoc::cdfn<float_type, -4>::eval());
	do_one(draws_size, width, uniform);
}

int
main()
{
	censoc::init_fpu_check();
	run(1000, 1.7);
	run(7000, 1.7);
	run(178, .7);
	run(1000, .7);
	run(1000, 2.79);
	if (censoc::was_fpu_ok(&::std::cout) == false) 
		throw ::std::runtime_error("fpu exception taken place for the chosen coefficients");
	return 0;
}
