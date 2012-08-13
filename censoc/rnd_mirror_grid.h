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

#include <cmath>
#include <stdexcept>
#include <algorithm>

#include <Eigen/Core>

#include "cdfinvn.h"
#include "rand.h"
#include "aligned_alloc.h"

#ifndef CENSOC_RND_MIRROR_GRID_H
#define CENSOC_RND_MIRROR_GRID_H

namespace censoc {

template <typename N, typename F, typename Base>
class rnd_mirror_grid : public Base {

	typedef Base base;

	struct nonsigma_postcdfinvn {

		N nonsigma_column_i;

		nonsigma_postcdfinvn(rnd_mirror_grid & h) 
		: h(h) {
		}

		void inline 
		hook(F const * const x, N const x_size) 
		{
			assert(x_size > 1);
			assert(h.repetitions == 2 * x_size);
			assert(!(h.repetitions % 2));
			//assert(static_cast<N>(h.nonsigma().col(nonsigma_column_i).size()) == h.repetitions);

#ifndef NDEBUG
			// x > 10 ? 10 : x < -10 ? -10 : x;
			for (N i(0); i != x_size; ++i) {
				assert(x[i] < 10);
				assert(x[i] > -10);
			}
#endif

			N const column_offset(h.repetitions_column_stride_size * nonsigma_column_i);
			for (N i(0); i != x_size; ++i) {
				h.nonsigma()[column_offset + i] = x[i];
				h.nonsigma()[column_offset + i + x_size] = -x[i];
			}

			// aka ::std::random_shuffle
			for (N i(1); i != h.repetitions; ++i) 
				::std::swap(h.nonsigma()[column_offset + i], h.nonsigma()[column_offset + censoc::rand_nobias<censoc::mt19937_ref>(h.rng).eval(i + 1)]);

		}

	private:
		rnd_mirror_grid & h;
	};

	// todo -- refactor, as there is too much common code with nonsigma_postcbdfinvn in this new, non-eigen, version...
	struct sigma_postcdfinvn {

		N sigma_column_i;

		sigma_postcdfinvn(rnd_mirror_grid & h) 
		: h(h) {
		}

		void inline 
		hook(F const * const x, N const x_size) 
		{
			assert(x_size > 1);
			assert(h.repetitions == 2 * x_size);
			assert(!(h.repetitions % 2));

#ifndef NDEBUG
			for (N i(0); i != x_size; ++i) {
				assert(x[10] < 10);
				assert(x[10] > -10);
			}
#endif

			N const column_offset(h.repetitions_column_stride_size * sigma_column_i);
			for (N i(0); i != x_size; ++i) {
				h.sigma()[column_offset + i] = x[i];
				h.sigma()[column_offset + i + x_size] = -x[i];
			}

			// aka ::std::random_shuffle
			for (N i(1); i != h.repetitions; ++i) 
				::std::swap(h.sigma()[column_offset + i], h.sigma()[column_offset + censoc::rand_nobias<censoc::mt19937_ref>(h.rng).eval(i + 1)]);
		}

	private:
		rnd_mirror_grid & h;
	};

	// TODO when the main app class gets broken to app-cli and app-engine, move these back into a const-qualified status
	N respondents; 
	N repetitions; 
	N attributes_size;
	N repetitions_column_stride_size;

	/**
		@note -- produces predictable and reproducable accross diff. architectures uncorrelated sequence (good for having consistent "random-like" things for shared "random" draws accross different processing modules across different network nodes for example). As per C++0x required behavior: 10000th consecutive invocation of a default-constructed object of type mt19937 shall produce the value 4123659995
		*/
	::boost::mt19937 rng;


public:

	int const static sigma_trunc = 3;
	int const static non_sigma_trunc = 4;

	F static 
	sigma_min()
	{
		return censoc::cdfn<F, -sigma_trunc>::eval();
	}
	F static 
	sigma_max()
	{
		return censoc::cdfn<F, sigma_trunc>::eval();
	}
	F static 
	non_sigma_min()
	{
		return censoc::cdfn<F, -non_sigma_trunc>::eval();
	}
	F static 
	non_sigma_max()
	{
		return censoc::cdfn<F, non_sigma_trunc>::eval();
	}

	inline 
	rnd_mirror_grid (typename base::ctor_paramtype base_ctor)
	: base(base_ctor) {
		assert(rng.max() == (uint32_t)-1);
	}

	inline 
	rnd_mirror_grid ()
	{
	}

	inline 
	void 
	init(N const & respondents, N const & repetitions, N const & attributes_size, N const & repetitions_column_stride_size)
	{
		assert(!(repetitions % 2));

		// TODO when the main app class gets broken to app-cli and app-engine, move this back into a ctor-initialisation stage
	  this->respondents = respondents;
		this->repetitions = repetitions; 
		this->attributes_size = attributes_size; 
		this->repetitions_column_stride_size = repetitions_column_stride_size;

		N const nonsigma_columns(attributes_size * respondents);
		N const repetitions_half(repetitions >> 1);

		censoc::aligned_alloc<F> raw;
		F * const raw_ptr(raw.alloc(repetitions_half));

		// nonsigma
		if (base::nonsigma() != NULL) {
			censoc::cdfinvn_half<F, N, nonsigma_postcdfinvn> cdfinvn(*this);  
			for (cdfinvn.nonsigma_column_i = 0; cdfinvn.nonsigma_column_i != nonsigma_columns; ++cdfinvn.nonsigma_column_i) {
				sequence(raw_ptr, non_sigma_min(), repetitions_half);
				cdfinvn.eval(raw_ptr, repetitions_half);  
			}
		}

		// sigma... (note -- inverted version when compared to gauss code representation in gmnl_x.prg)
		if (base::sigma() != NULL) {
			censoc::cdfinvn_half<F, N, sigma_postcdfinvn> cdfinvn(*this);  
			for (cdfinvn.sigma_column_i = 0; cdfinvn.sigma_column_i != respondents; ++cdfinvn.sigma_column_i) {
				sequence(raw_ptr, sigma_min(), repetitions_half);
				cdfinvn.eval(raw_ptr, repetitions_half);  
			}
		}

	}

private:
	void inline 
	sequence(F * vector, F const & from, N const & steps)
	{
		assert(from < .5 && from > 0);

		F const to(.5 - censoc::smallish<F>());
		F const step_size((to - from) / steps);

		for (N i(0); i != steps; ++i)
			vector[i] = to - (step_size * i) - ::censoc::rand_half<censoc::mt19937_ref>(rng).eval(step_size);
	}
};

}

#endif
