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

#include "aligned_alloc.h"
#include "cdfinvn.h"

#ifndef CENSOC_HALTON_H
#define CENSOC_HALTON_H

namespace censoc {

template <typename N, typename F, typename Base>
class halton : public Base {

	typedef Base base;

	struct nonsigma_postcdfinvn {

		N nonsigma_column_i;

		nonsigma_postcdfinvn(halton & h) 
		: h(h) {
		}

		void inline 
		hook(F const * const x, N const x_size) 
		{
			assert(h.respondents * h.repetitions + 10 == x_size);

#ifndef NDEBUG
			{
				N const x_size_(h.respondents * h.repetitions);
				for (N i(0); i != x_size_; ++i) {
					assert(x[10 + i] < 10);
					assert(x[10 + i] > -10);
				}
			}
#endif
			for (N i(0); i != h.respondents; ++i) {
				CENSOC_RESTRICTED_CONST_PTR(F, dst, h.nonsigma() + h.repetitions_column_stride_size * (nonsigma_column_i + i * h.attributes_size));
				CENSOC_RESTRICTED_CONST_PTR(F const, src, x + 10 + i * h.repetitions);
				for (N j(0); j != h.repetitions; ++j)
					dst[j] = src[j];
			}
		}

	private:
		halton & h;
	};

	struct sigma_postcdfinvn {

		sigma_postcdfinvn(halton & h) 
		: h(h) {
		}

		void inline 
		hook(F const * const x, N const x_size) 
		{
			assert(h.respondents * h.repetitions + 10 == x_size);
#ifndef NDEBUG
			{
				N const x_size_(h.respondents * h.repetitions);
				for (N i(0); i != x_size_; ++i) {
					assert(x[10 + i] < 10);
					assert(x[10 + i] > -10);
				}
			}
#endif

			for (N i(0); i != h.respondents; ++i) {
				CENSOC_RESTRICTED_CONST_PTR(F, dst, h.sigma() + h.repetitions_column_stride_size * i);
				CENSOC_RESTRICTED_CONST_PTR(F const, src, x + 10 + i * h.repetitions);
				for (N j(0); j != h.repetitions; ++j)
					dst[j] = src[j];
			}
		}

	private:
		halton & h;
	};


	// TODO when the main app class gets broken to app-cli and app-engine, move these back into a const-qualified status
	N respondents; 
	N repetitions; 
	N attributes_size;
	N repetitions_column_stride_size;

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
	halton (typename base::ctor_paramtype base_ctor)
	: base(base_ctor) {
	}

	inline 
	halton ()
	{
	}

	inline 
	void init (N const & respondents, N const & repetitions, N const & attributes_size, N const & repetitions_column_stride_size)
	{
		// TODO when the main app class gets broken to app-cli and app-engine, move this back into a ctor-initialisation stage
	  this->respondents = respondents;
		this->repetitions = repetitions; 
		this->attributes_size = attributes_size; 
		this->repetitions_column_stride_size = repetitions_column_stride_size;

		N const static primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113};

		if (attributes_size > sizeof(primes) / sizeof(N))
			throw ::std::runtime_error("not enough primes to generate halton table");

		BOOST_STATIC_ASSERT(censoc::DefaultAlign > 11); // 1 to start the prime-number based sequence (hence discarded); 10 is to skip first ten primes;
		BOOST_STATIC_ASSERT(censoc::DefaultAlign > sizeof(F));
		BOOST_STATIC_ASSERT(!(censoc::DefaultAlign % sizeof(F)));
		N const raw_size(censoc::DefaultAlign + respondents * repetitions);
		censoc::aligned_alloc<F> raw;
		F * const raw_tmp(raw.alloc(raw_size));

		// 1 to start the prime-number based sequence (hence discarded); 10 is to skip first ten primes;
		N const raw_start(censoc::DefaultAlign - 11);
		N const raw_ptr_size(raw_size - raw_start);
		F * const raw_ptr(raw_tmp + raw_start);
		raw_ptr[0] = 0;

		F * const raw_ptr_plus_one(raw_ptr + 1);
		N const raw_ptr_size_minus_one(raw_ptr_size - 1);

		// nonsigma
		{
			censoc::cdfinvn<F, N, nonsigma_postcdfinvn> cdfinvn(*this); 

			for (N i(0); i != attributes_size; ++i) {
				sequence(raw_ptr, raw_ptr_size, primes[i]);
				cdfinvn.nonsigma_column_i = i;

				for (N j(0); j != raw_ptr_size_minus_one; ++j) 
					raw_ptr_plus_one[j] = non_sigma_min() + raw_ptr_plus_one[j] * (non_sigma_max() - non_sigma_min());

				cdfinvn.eval(raw_ptr_plus_one, raw_ptr_size_minus_one);  
			}
		}

		// sigma... (note -- inverted version when compared to gauss code representation in gmnl_x.prg)
		if (base::sigma() != NULL) {
			censoc::cdfinvn<F, N, sigma_postcdfinvn> cdfinvn(*this);  
			sequence(raw_ptr, raw_ptr_size, primes[attributes_size]);
			for (N j(0); j != raw_ptr_size_minus_one; ++j) 
				raw_ptr_plus_one[j] = non_sigma_min() + raw_ptr_plus_one[j] * (non_sigma_max() - non_sigma_min());
			cdfinvn.eval(raw_ptr_plus_one, raw_ptr_size_minus_one);  
		}

	}

private:
	void static inline 
	sequence(F * const vector, N const vector_size, N const & prime)
	{
		assert(prime > 1);
		assert(vector_size > 1);
		assert(vector[0] == 0);
		assert(!((uintptr_t)vector % censoc::DefaultAlign));

		N i(1), segment_size(1);
		do {
			N const segment_size_cached(segment_size);
			for (N j(1); j != prime; ++j) {
				N const growth_size(::std::min(vector_size - segment_size, segment_size_cached));

				// TODO -- in the following, do check that the original/theoretical/verbose design (j/s^i) did indeed group as (j / (s^i)); otherwise replacing it with (j * s^(-1)) will not work...
				// if so, then re-calculate another array of the static inverted primes (this way can also replace the division by mult).
				CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(F const, tmp, j * ::std::pow(static_cast<F>(prime), -static_cast<F>(i)));
				CENSOC_RESTRICTED_CONST_PTR(F, dst, vector + segment_size);
				CENSOC_ALIGNED_RESTRICTED_CONST_PTR(F const, src, vector);
				for (N k(0); k != growth_size; ++k)
					dst[k] = src[k] + *tmp;

				if ((segment_size += growth_size) == static_cast<N>(vector_size))
					return;
			}
			++i;
			assert(i); // N-width range must not wraparound...
		} while (!0);
	}
};

}

#endif
