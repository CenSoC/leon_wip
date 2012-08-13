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

#include <stdexcept>

#include <Eigen/Core>

#include "lexicast.h"

#ifndef CENSOC_CDFINVN_H
#define CENSOC_CDFINVN_H

namespace censoc {

// temporary deactivation of 'self-assigning default hook' -- until further research/learning w.r.t. lazy evaluations in Eigen and aliasing side-effects is done.
// at the moment the user-code is taxed with explicit type-provisioning for hooking
#if 0
struct cdfinvn_hook_default {
	void hook(F * const x, N x_size) 
	{
		// x is the final value passed to client code
	}
};
#endif

template <typename F, typename N, typename BaseHook>
struct cdfinvn : public BaseHook {

	cdfinvn()
	{
	}

	template <typename BaseInit>
	cdfinvn(BaseInit & base_init) // TODO -- quick hack for the time-being (wrt ref being non-const)
	: BaseHook(base_init) {
	}

private:
	typedef censoc::lexicast< ::std::string> xxx;

public:

	/**
		@post 'BaseHook::hook' is called for further chaining
		*/
	template <typename TMP>
	void inline 
	eval(TMP x, N const x_size)
	{
		// todo move into pre-condition and refactor this to 'assert' level (instead of execption-throwing)
		for (N i(0); i != x_size; ++i)
			if (x[i] >= 1)
				throw ::std::runtime_error(xxx() << "supplied probability is >= 1 [\n" << x[i] << "]");
			else if (x[i] <= 0)
				throw ::std::runtime_error(xxx() << "supplied probability is <= 0 [\n" << x[i] << "\n]");

		F const static p0(-.322232431088);
		F const static p1(-1);
		F const static p2(-.342242088547);
		F const static p3(-.0204231210245);
		F const static p4(-.453642210148e-4);

		F const static q0(.0993484626060);
		F const static q1(.588581570495);
		F const static q2(.531103462366);
		F const static q3(.103537752850);
		F const static q4(.38560700634e-2);

		for (N i(0); i != x_size; ++i) {
			assert(x[i] < 1 && x[i] > 0);
			F const sqrt_cache(::std::sqrt(::std::log(x[i] > static_cast<F>(.5) ? 1 - x[i] : x[i]) * -2));
			F const norms_cache(sqrt_cache + 
				((((((((sqrt_cache * p4) + p3) * sqrt_cache) + p2) * sqrt_cache) + p1) * sqrt_cache) + p0) / 
				((((((((sqrt_cache * q4) + q3) * sqrt_cache) + q2) * sqrt_cache) + q1) * sqrt_cache) + q0)
			);
			x[i] = x[i] > static_cast<F>(.5) ? norms_cache : -norms_cache;
		}

		BaseHook::hook(x, x_size);
	}
};

template <typename F, typename N, typename BaseHook>
struct cdfinvn_half : public BaseHook {

	cdfinvn_half()
	{
	}

	template <typename BaseInit>
	cdfinvn_half(BaseInit & base_init) // TODO -- quick hack for the time-being (wrt ref being non-const)
	: BaseHook(base_init) {
	}

private:
	typedef censoc::lexicast< ::std::string> xxx;

public:

	/**
		@pre x elements are within 0 < x < .5
		@post normal distribution but for the positive range (as if x was .5 < x < 1)
		@post 'BaseHook::hook' is called for further chaining
		*/
	template <typename TMP>
	void inline 
	eval(TMP x, N const x_size)
	{
		// todo move into pre-condition and refactor this to 'assert' level (instead of execption-throwing)
		for (N i(0); i != x_size; ++i)
			if (x[i] >= .5)
				throw ::std::runtime_error(xxx() << "supplied probability is >= .5 [\n" << x[i] << "]");
			else if (x[i] <= 0)
				throw ::std::runtime_error(xxx() << "supplied probability is <= 0 [\n" << x[i] << "\n]");

		F const static p0(-.322232431088);
		F const static p1(-1);
		F const static p2(-.342242088547);
		F const static p3(-.0204231210245);
		F const static p4(-.453642210148e-4);

		F const static q0(.0993484626060);
		F const static q1(.588581570495);
		F const static q2(.531103462366);
		F const static q3(.103537752850);
		F const static q4(.38560700634e-2);

		for (N i(0); i != x_size; ++i) {
			assert(x[i] < .5 && x[i] > 0);
			F const sqrt_cache(::std::sqrt(::std::log(x[i]) * -2));
			F const norms_cache(sqrt_cache + 
				((((((((sqrt_cache * p4) + p3) * sqrt_cache) + p2) * sqrt_cache) + p1) * sqrt_cache) + p0) / 
				((((((((sqrt_cache * q4) + q3) * sqrt_cache) + q2) * sqrt_cache) + q1) * sqrt_cache) + q0)
			);
			x[i] = norms_cache;
		}

		BaseHook::hook(x, x_size);
	}
};

template <typename F, int N>
struct cdfn { };

template <typename F>
struct cdfn<F, 2> { 
	F static inline 
	eval()
	{
		return .97725;
	}
};

template <typename F>
struct cdfn<F, -2> { 
	F static inline 
	eval()
	{
		return .02275;
	}
};

template <typename F>
struct cdfn<F, 3> { 
	F static inline 
	eval()
	{
		return 0.99865;
	}
};

template <typename F>
struct cdfn<F, -3> { 
	F static inline 
	eval()
	{
		return 0.00135;
	}
};

template <typename F>
struct cdfn<F, 4> { 
	F static inline 
	eval()
	{
		return 0.999968;
	}
};

template <typename F>
struct cdfn<F, -4> { 
	F static inline 
	eval()
	{
		return 0.000032;
	}
};


}

#endif
