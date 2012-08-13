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
 */

#include <iostream>
#include <iomanip>

#include <boost/lexical_cast.hpp>

#include <censoc/exp_lookup.h>
#include <censoc/finite_math.h>
#include <censoc/rand.h>

typedef unsigned size_type;

template <typename E>
void
run()
{
	typedef typename E::float_type float_type;
	E my_exp(.99999);
	if (censoc::was_fpu_ok(&my_exp) == false) 
		throw ::std::runtime_error("fpu exception taken place during initialization of exp_lookup");

	::std::cout << my_exp.calculate_max_exponent() << ::std::endl;

	float_type f(0);
	for (; f < my_exp.calculate_max_exponent(); f += .0001) {
		float_type ratio(my_exp.eval(f) / ::std::exp(f));
		::std::cout << f << ',' << ratio << ',';
		if (censoc::was_fpu_ok(&ratio, &f, &::std::cout) == false) 
			throw ::std::runtime_error("fpu exception taken place whilst iterating through range, f(=" + ::boost::lexical_cast< ::std::string>(f) + "), ratio(=" + ::boost::lexical_cast< ::std::string>(ratio) + ')');
		//if (ratio < .999)
		//	throw ::std::runtime_error("exp ratio approx/real is always expected to be >= 1");

		ratio = my_exp.eval(-f) / ::std::exp(-f);
		::std::cout << ratio << '\n';
		if (censoc::was_fpu_ok(&ratio, &f, &::std::cout) == false) 
			throw ::std::runtime_error("fpu exception taken place whilst iterating through range, f(=" + ::boost::lexical_cast< ::std::string>(-f) + "), ratio(=" + ::boost::lexical_cast< ::std::string>(ratio) + ')');
		//if (ratio < .999)
		//	throw ::std::runtime_error("exp ratio approx/real is always expected to be >= 1");
	}

	if (censoc::was_fpu_ok(&f, &::std::cout) == false) 
		throw ::std::runtime_error("fpu should have taken place but did not");

	f = my_exp.eval(f);
	if (censoc::was_fpu_ok(&f, &::std::cout) == true) 
		throw ::std::runtime_error("fpu exception failed to be generated");

}

int
main()
{
	try {
		::censoc::init_fpu_check();
		::std::cout << ::std::setprecision(10);
		::std::clog << ::std::setprecision(10);

		::std::cout << "running double, linear spacing\n";
		run< ::censoc::exp_lookup::linear_interpolation_linear_spacing<double, size_type, 100> >();
		::std::cout << "running float, linear spacing\n";
		run< ::censoc::exp_lookup::linear_interpolation_linear_spacing<float, size_type, 100> >();

		// NOTE -- UNFINISHED
#if 0
		::std::cout << "running double, exp spacing\n";
		run< ::censoc::exp_lookup::linear_interpolation_exp_spacing<double, size_type, 2000> >();
		::std::cout << "running float, linear spacing\n";
		run< ::censoc::exp_lookup::linear_interpolation_exp_spacing<float, size_type, 1000> >();
#endif
		return 0;
	} catch (::std::exception const & e) {
		::std::cout.flush();
		::std::cerr << "Runtime error: [" << e.what() << "]\n";
		return -1;
	} catch (...) {
		::std::cout.flush();
		::std::cerr << "Unknown runtime error.\n";
		return -1;
	}
}
