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

// allows to be compiled in a separate translation unit with different optimisations (e.g. -fno-finite-math-only)


#include <stdexcept>

#include "type_traits.h"

#ifndef CENSOC_FINITE_MATH_H
#define CENSOC_FINITE_MATH_H

#ifdef __GNUC__

// optimization controlling thingies

#define CENSOC_OPTIMIZE_PUSH_OPTIONS _Pragma("GCC push_options")

#define CENSOC_OPTIMIZE_NO_ASSOCIATIVE_RECIPROCAL_MATH _Pragma("GCC optimize (\"-fno-associative-math\", \"-fno-reciprocal-math\")")

#define CENSOC_OPTIMIZE_POP_OPTIONS _Pragma("GCC pop_options")

// tip thingy: can use something like gcc -E main.cc to test...


#else
#error "optimization pragmas have not yet been implemented for this compiler... TODO... (not to worry though -- most of them should follow similar syntatic structure :-)"
#endif

namespace censoc {

	bool isfinite(censoc::param<float>::type x) noexcept;
	bool isfinite(censoc::param<double>::type x) noexcept;
	bool isfinite(censoc::param<double long>::type x) noexcept;

// TODO --  on some computers 'was_fpu_ok' can be made static (non-extern and defined right here in this file which is subjected to 'ffast-math' etc optimizations) and still work absolutely fine... on other (e.g. slightly newer cpus) it would appear that I have to move it to finite_math.pi (with some optimizations disabled)... may be has something to do with march=native flag... Anyways -- TODO FIND OUT MORE LATER.
	bool was_fpu_ok(...) noexcept;
	bool was_fpu_ok_x(float *, int *) noexcept;

	void opacify(...) noexcept;

	int init_fpu_check_anchor(true);

	void static
	init_fpu_check()
	{
#ifdef __GNUC__
#ifdef __x86_64__
		// sometimes some gcc builds are broken (do not include crtfastmath.o on x86_64)...
		__builtin_ia32_ldmxcsr (__builtin_ia32_stmxcsr() | 1 << 6 | 1 << 15);
#endif
#endif

		float x;
		was_fpu_ok(&x, &init_fpu_check_anchor); // reset (in case the program calls it after some unrelated exception is throw and even though this 'init' is called from the 'start' of the main, the process's fpu state is still lingering.
		if (was_fpu_ok(&x, &init_fpu_check_anchor) == false) 
			throw ::std::runtime_error("'init_fpu_check' expects fpu state to be 'clearable'");

		x = ::std::exp(100.f);
		if (censoc::isfinite(x) == true)
			throw ::std::runtime_error("'censoc::isfinite' failed to detect an issue in exp");
		if (was_fpu_ok(&x, &init_fpu_check_anchor) == true) 
			throw ::std::runtime_error("'init_fpu_check' failed to set the fpu exception flags in exp");

		x = ::std::numeric_limits<float>::max();
		was_fpu_ok(&x);
		x = x * 2;
		if (censoc::isfinite(x) == true)
			throw ::std::runtime_error("'censoc::isfinite' failed to detect an issue in max * 2");
		if (was_fpu_ok(&x, &init_fpu_check_anchor) == true) 
			throw ::std::runtime_error("'init_fpu_check' failed to set the fpu exception flags in max * 2");

		x = ::std::numeric_limits<float>::max();
		was_fpu_ok(&x);
		x = x * 2;
		if (censoc::isfinite(x) == true)
			throw ::std::runtime_error("'censoc::isfinite' failed to detect an issue in inf * 2");
		if (was_fpu_ok(&x, &init_fpu_check_anchor) == true) 
			throw ::std::runtime_error("'init_fpu_check' failed to detect an issue in inf * 2");
	}

}
#endif
