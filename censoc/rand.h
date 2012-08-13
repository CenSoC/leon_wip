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

#include <assert.h>
#include <boost/static_assert.hpp>

#include <boost/random/mersenne_twister.hpp>
#ifdef HAVE_MT19937INT_C
#error should be using boost-specific MT19937INT implementation
#endif

#include <openssl/rand.h>

#ifndef CENSOC_RAND_H
#define CENSOC_RAND_H

namespace censoc {

/**
	@desc -- generic, default algo to produce auto-seeded sequences 
	*/
template <typename T>
struct rand {
	typedef T result_type;
	typedef struct{} ctor_type;
	BOOST_STATIC_ASSERT(T(0) < T(-1));

	result_type static
	max() noexcept
	{
		return -1;
	}

	result_type static
	eval() noexcept
	{
		result_type buf;
#ifndef NDEBUG
		int const rv(
#endif
				::RAND_pseudo_bytes(reinterpret_cast<char unsigned*>(&buf), sizeof(buf))
#ifndef NDEBUG
				)
#endif
			;
		assert(rv != -1);
		return buf;
	}
};

template <>
struct rand<uint32_t> {
	typedef uint32_t result_type;
	typedef struct {} ctor_type;
	uint32_t static
	max() noexcept
	{
		return -1;
	}
	uint32_t static
	eval() noexcept
	{
#ifndef __WIN32__
		return ::arc4random();
#else
		uint32_t buf;
#ifndef NDEBUG
		int const rv(
#endif
				::RAND_pseudo_bytes(reinterpret_cast<char unsigned*>(&buf), sizeof(buf))
#ifndef NDEBUG
				)
#endif
			;
		assert(rv != -1);
		return buf;
#endif
	}
};

struct mt19937_ref {
	typedef ::boost::mt19937 rng_type;
	typedef rng_type::result_type result_type;
	typedef rng_type & ctor_type;
	rng_type & rng;
	mt19937_ref(ctor_type rng) noexcept
	: rng(rng) {
	}
	result_type
	max() const noexcept
	{
		return rng.max();
	}
	result_type
	eval() noexcept
	{
		return rng();
	}
};

/**
	@post returns '-abs(range_high)' to 'abs(range_high)' (inclusive) centred around 0. 'range' can be either positive or negative
	*/
//  uint32_t (* rng) () = censoc::rand
template <typename Rng>
struct rand_full : Rng {
	rand_full() noexcept
	{
	}
	rand_full(typename Rng::ctor_type rng_ctor) noexcept
	: Rng(rng_ctor) {
	}
	template <typename R>
	R
	eval(R range_high) noexcept
	{
		assert((range_high + range_high) / Rng::max() != 0);
		return Rng::eval() * ((range_high + range_high) / Rng::max()) - range_high;
		//return (::arc4random() & 1) * (range_high + range_high) - range_high;
		//return (::arc4random() & 7) * ((range_high + range_high) / 7) - range_high;
	}
};

/**
	@post returns 0 to 'range' (inclusive). 'range' can be either positive or negative
	*/
template <typename Rng>
struct rand_half : Rng {
	rand_half() noexcept
	{
	}
	rand_half(typename Rng::ctor_type rng_ctor) noexcept
	: Rng(rng_ctor) {
	}
	template <typename R>
	R
	eval(R range) noexcept
	{
		assert(range / Rng::max() != 0);

		//return Rng::eval() * (range / (double)Rng::max());
		return Rng::eval() * (range / Rng::max());

		//return (::arc4random() & 1) * range;
		//return (::arc4random() & 7) * (range / 7);
	}
};

// rane [0, range)
template <typename Rng>
struct rand_nobias_raw : Rng {
	typedef typename Rng::result_type result_type;
	rand_nobias_raw() noexcept
	{
	}
	rand_nobias_raw(typename Rng::ctor_type rng_ctor) noexcept
	: Rng(rng_ctor) {
	}
	result_type
	eval(result_type const & range) noexcept
	{
		assert(range);
		result_type const nobias(Rng::max() - 1 - Rng::max() % range);
		result_type rv;
		while((rv = Rng::eval()) > nobias);
		return rv % range;
	}
};

template <typename T>
struct stupid_hack : T {
	stupid_hack()
	{
	}
	stupid_hack(typename T::ctor_type ctor)
	: T(ctor) {
	}
};

// rane [0, range)
template <typename Rng>
struct rand_nobias : stupid_hack<Rng>, // direct Rng specification here hides/ambiguates the rand_nobias_raw<Rng> base... even with full class-resolution path specification... mmm...
	rand_nobias_raw<Rng> {
	typedef typename Rng::result_type result_type;
	rand_nobias() noexcept
	{
	}
	rand_nobias(typename Rng::ctor_type rng_ctor) noexcept
	: stupid_hack<Rng>(rng_ctor), rand_nobias_raw<Rng>(rng_ctor) {
	}
	result_type
	eval(result_type const & range) noexcept
	{
		assert(range);
		result_type const range_less_one(range - 1);
		if (!(range_less_one & range)) 
			return stupid_hack<Rng>::eval() & range_less_one;
		else 
			return rand_nobias_raw<Rng>::eval(range);
	}
};

}

#endif
