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
	@NOTE -- currently largely a hack to be able to represent unevrsally-unique position in the solution space (total solution space, not just iterated-one via the combos_builder)
 */

#include <stdlib.h>
#include <gmp.h>

#include <memory>
#include <vector>

#include <censoc/aligned_alloc.h>

// @TODO -- as with other bits of code (e.g. range_utils) later will move to 'outside-of-netcpu-centric' scope...
#ifndef CENSOC_NETCPU_BIG_INT_CONTEXT_H
#define CENSOC_NETCPU_BIG_INT_CONTEXT_H

namespace censoc { namespace netcpu { 

template <typename T> struct fnv_1a_hash_traits { };
template <>
struct fnv_1a_hash_traits<uint32_t> {
	uint32_t const static base = 2166136261;
	uint32_t const static prime = 16777619;
};
template <>
struct fnv_1a_hash_traits<uint64_t> {
	uint64_t const static base = 14695981039346656037ull;
	uint64_t const static prime = 1099511628211ull;
};

// hacky singleton -- don't want to have a separate translation unit for classic version just yet... (later if this wrapper of bignum happens to be of use -- move to outer scope and perhaps make the context to be a classic singleton)
struct big_int_context_type {
	::std::vector< ::std::shared_ptr< ::mpz_t> > cached_bns;
	uint_fast32_t hash_data_size;
	censoc::aligned_alloc<uint8_t> hash_data;
	uint8_t * hash_data_ptr;

	big_int_context_type()
	: hash_data_size(16), hash_data_ptr(hash_data.alloc(hash_data_size)) {
		unsigned const static cache_size(7000000);
		cached_bns.reserve(cache_size);
		for (unsigned i(0); i != cache_size; ++i)
			cached_bns.push_back(get_new_bn());
	}

	static ::mpz_t * 
	mpz_type_alloc() noexcept
	{
		::mpz_t * rv(static_cast< ::mpz_t *>(::malloc(sizeof(::mpz_t))));
		assert(rv != NULL);
		return rv;
	}

	void static
	mpz_type_free(mpz_t * x)
	{
		assert(x != NULL);
		::mpz_clear(*x);
		::free(x);
	}

	::std::shared_ptr< ::mpz_t> 
	get_new_bn()
	{
		::std::shared_ptr< ::mpz_t> rv(mpz_type_alloc(), mpz_type_free);
		::mpz_init2(*rv, hash_data_size * 8);
		return rv;
	}

	::std::shared_ptr< ::mpz_t>
	get_cached_bn()
	{
		assert(cached_bns.back().unique() == true);
		::std::shared_ptr< ::mpz_t> rv(cached_bns.back());
		cached_bns.pop_back();
		return rv;
	}

	::std::shared_ptr< ::mpz_t>
	get_bn() noexcept 
	{
		return cached_bns.empty() == true ? get_new_bn() : get_cached_bn(); 
	}

	void
	copy_on_write(::std::shared_ptr< ::mpz_t> & num)
	{
		if (num.unique() == false) { 
			::mpz_t & old(*num);
			if (cached_bns.empty() == true)
				num.reset(mpz_type_alloc(), mpz_type_free);
			else {
				assert(cached_bns.back().unique() == true);
				num = cached_bns.back();
				cached_bns.pop_back();
			}
			::mpz_init_set(*num, old);
		}
		assert(num.unique() == true);
	}

	void
	isolate_on_write(::std::shared_ptr< ::mpz_t> & num) 
	{
		if (num.unique() == false)
			num = get_bn();
		assert(num.unique() == true);
	}

	void
	return_bn(::std::shared_ptr< ::mpz_t> & x) noexcept
	{
		assert(x.unique() == true);
		cached_bns.push_back(x);
	}

#if 0
	void
	reset()
	{
#ifndef NDEBUG
		for (::std::vector< ::std::shared_ptr< ::mpz_t> >::const_iterator i(cached_bns.begin()); i != cached_bns.end(); ++i)
			assert(i->unique() == true);
#endif
		cached_bns.clear();
	}
#endif

	// not really needed -- expected to be static scope anyways
	~big_int_context_type()
	{
	}


	template <typename SizeType>
	SizeType
	hash_bn(::std::shared_ptr< ::mpz_t> const & x)
	{
		// todo -- consider whether hash-caching of the 'unchanged' number is a possibility (depends on whether there are frequent 'hash' calls on the same number, or if there are 'rehash' use-cases on things like unoredered containers)
		unsigned const x_size((::mpz_sizeinbase(*x, 2) + 7) / 8);
		if (hash_data_size < x_size)
			hash_data_ptr = hash_data.alloc(hash_data_size = x_size * 1.25);
		hash_data_ptr[0] = 0; // export may not write anything at all actually...
		::mpz_export(hash_data_ptr, NULL, 1, 1, 0, 0, *x);


		CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(SizeType, hash_rv_ptr, fnv_1a_hash_traits<SizeType>::base);
		CENSOC_ALIGNED_RESTRICTED_CONST_PTR(uint8_t const, hash_data_ptr_, hash_data_ptr);
		for (unsigned i(0); i != x_size; ++i) {
			*hash_rv_ptr ^= hash_data_ptr_[i];
			*hash_rv_ptr *= fnv_1a_hash_traits<SizeType>::prime;
		}
		return *hash_rv_ptr;
	}

};
// not instantiating -- to make client app more aware by doing it explicitly
}

}

#endif
