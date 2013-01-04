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
	@NOTE -- currently largely a hack for the range_utils to be able to represent unevrsally-unique position in the solution space (total solution space, not just iterated-one via the combos_builder)
 */

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include <boost/integer_traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/shared_ptr.hpp>

#include <censoc/type_traits.h>

#include "big_int_context.h"

// @TODO -- as with other bits of code (e.g. range_utils) later will move to 'outside-of-netcpu-centric' scope...
#ifndef CENSOC_NETCPU_BIG_UINT_H
#define CENSOC_NETCPU_BIG_UINT_H

namespace censoc { namespace netcpu { 

template <typename size_type>
class big_uint {
	typedef typename censoc::param<size_type>::type size_paramtype;

	BOOST_STATIC_ASSERT(::boost::integer_traits<size_type>::const_max <= ::boost::integer_traits<unsigned long>::const_max);
	BOOST_STATIC_ASSERT(::boost::integer_traits<size_type>::const_max <= ::boost::integer_traits<BN_ULONG>::const_max);

/**
	@NOTE -- hack: currently using copy-on-write implementation to save on redundant copying (i.e. ::BN_copy(num, x.num)) when passing by value (e.g. std::make_pair, etc.) 
	
	@TODO -- Later, however, would be better to implement this using explicit "move" semantics (either via template specialisation or, better yet, using new/upcoming c++ standard). This way some things could be even faster based on compile-time determination of whether the move or copy-based assignment is taking place in the code.

	For the time-being, however, it would be quicker (i.e. less development time) to deploy the copy-on-write semantics w/o any noticeable performance-related bottlenecks as big_uint is not expected to be used in the actual opaque method whose terrain is being minimized.
	*/
	::boost::shared_ptr< ::BIGNUM> num;

	typedef ::std::hash<big_uint<size_type> > hasher_type;

public:

	big_uint()
	: num(netcpu::big_int_context.get_bn()) {
		assert(num.use_count() == 1); 
	}

	big_uint(big_uint const & x)
	: num(x.num) {
		assert(num.use_count() > 1); 
	}

	big_uint(size_paramtype x)
	: num(netcpu::big_int_context.get_bn()) {
		assert(num.use_count() == 1); 
		// TODO -- specialize better for 64bit ints etc. using BN_bin2bn et al
		assert(x <= ::std::numeric_limits<unsigned long>::max());
#ifndef NDEBUG
		int rv =
#endif
		::BN_set_word(num.get(), x);
		assert(rv);
	}

	~big_uint()
	{
		if (num.use_count() == 1) 
			netcpu::big_int_context.return_bn(num);
	}

	void
	zero()
	{
		netcpu::big_int_context.isolate_on_write(num);
#ifndef NDEBUG
		int rv = 
#endif
		BN_zero(num.get());
		assert(rv);
	}

	void
	one()
	{
		netcpu::big_int_context.isolate_on_write(num);
#ifndef NDEBUG
		int rv = 
#endif
		BN_one(num.get());
		assert(rv);
	}

	void
	rand(big_uint const & range)
	{
		netcpu::big_int_context.isolate_on_write(num);
#ifndef NDEBUG
		int rv = 
#endif
		::BN_pseudo_rand_range(num.get(), range.num.get());
#ifndef NDEBUG
	 if (!rv) {
			::std::cerr << range.to_string() << ::std::endl;
			::std::cerr << ::ERR_error_string(::ERR_get_error(), NULL) << ::std::endl;
		}
		assert(rv);
#endif
	}

	// TODO -- specialize better for 64bit ints etc. using BN_bin2bn et al
	operator size_type () const
	{
		unsigned long const tmp(::BN_get_word(num.get()));
		assert(0xffffffffL != tmp);
		assert(tmp <= ::std::numeric_limits<size_type>::max());
		return tmp;
	}

	big_uint &
	operator = (big_uint const & x) 
	{
		num = x.num;
		return *this;
	}

	void 
	operator = (size_paramtype x) 
	{
		netcpu::big_int_context.isolate_on_write(num);
#ifndef NDEBUG
		int rv =
#endif
		::BN_set_word(num.get(), x);
		assert(rv);
	}

	big_uint 
	operator + (size_paramtype x) const
	{
		big_uint tmp(x);
		::BN_add(tmp.num.get(), tmp.num.get(), num.get());
		return tmp;
	}

	big_uint
	operator + (big_uint const & x) const
	{
		big_uint tmp;
		::BN_add(tmp.num.get(), num.get(), x.num.get());
		return tmp;
	}

	big_uint &
	operator += (big_uint const & x) 
	{
		netcpu::big_int_context.copy_on_write(num);
		::BN_add(num.get(), num.get(), x.num.get());
		return *this;
	}

	big_uint &
	operator += (size_paramtype x) 
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::BN_add_word(num.get(), x);
		return *this;
	}

	big_uint
	operator-(big_uint const & x) const
	{
		big_uint tmp;
		::BN_sub(tmp.num.get(), num.get(), x.num.get());
		return tmp;
	}

	big_uint &
	operator-=(big_uint const & x) 
	{
		netcpu::big_int_context.copy_on_write(num);
		::BN_sub(num.get(), num.get(), x.num.get());
		return *this;
	}

	big_uint
	operator*(size_paramtype x) const
	{
		//big_uint tmp(x);
		//::BN_mul(tmp.num.get(), tmp.num.get(), num.get(), netcpu::big_int_context.context);
		big_uint tmp(*this);
		netcpu::big_int_context.copy_on_write(tmp.num);
		::BN_mul_word(tmp.num.get(), x);
		return tmp;
	}

	big_uint &
	operator*=(big_uint const & x) 
	{
		netcpu::big_int_context.copy_on_write(num);
		::BN_mul(num.get(), num.get(), x.num.get(), netcpu::big_int_context.context);
		return *this;
	}

	big_uint &
	operator*=(size_paramtype x) 
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::BN_mul_word(num.get(), x);
		return *this;
	}

	big_uint 
	operator/(size_paramtype x) const
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		big_uint tmp(*this);
		netcpu::big_int_context.copy_on_write(tmp.num);
		::BN_div_word(tmp.num.get(), x);
		return tmp;
	}

	big_uint &
	operator/=(size_paramtype x)
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::BN_div_word(num.get(), x);
		return *this;
	}

	big_uint
	operator%(big_uint const & x) const
	{
		big_uint tmp;
		::BN_mod(tmp.num.get(), num.get(), x.num.get(), netcpu::big_int_context.context);
		return tmp;
	}

	big_uint &
	operator%=(big_uint const & x)
	{
		// doco suggests that result may not be the same as any of the sources...
		big_uint tmp;
		::BN_mod(tmp.num.get(), num.get(), x.num.get(), netcpu::big_int_context.context);
		return *this = tmp;
	}


	int
	operator>(big_uint const & x) const
	{
		return ::BN_ucmp(num.get(), x.num.get()) == 1 ? !0 : 0;
	}

	int
	operator>=(big_uint const & x) const
	{
		int const rv(::BN_ucmp(num.get(), x.num.get()));
		return rv == 1 || !rv ? !0 : 0;
	}

	int
	operator<(big_uint const & x) const
	{
		return ::BN_ucmp(num.get(), x.num.get()) == -1 ? !0 : 0;
	}

	int
	operator<=(big_uint const & x) const
	{
		int const rv(::BN_ucmp(num.get(), x.num.get()));
		return  rv == -1 || !rv ? !0 : 0;
	}

	int
	operator==(big_uint const & x) const
	{
		return !::BN_ucmp(num.get(), x.num.get());
	}

	int
	operator!=(big_uint const & x) const
	{
		return ::BN_ucmp(num.get(), x.num.get());
	}

	size_type // ret by val because vector.size returns by val
	size() const
	{
		return BN_num_bytes(num.get());
	}

	void
	store(unsigned char *to) const
	{
		::BN_bn2bin(num.get(), to);
	}

	void
	load(unsigned char const * from, size_paramtype from_size) 
	{
		netcpu::big_int_context.isolate_on_write(num);
		::BN_bin2bn(from, from_size, num.get());
	}

	::std::string
	to_string() const
	{
		char * const tmp(::BN_bn2dec(num.get()));
		::std::string rv(tmp);
		OPENSSL_free(tmp);
		return rv;
	}

	size_type
	hash() const 
	{
		assert(num.get() != NULL);
		return netcpu::big_int_context.hash_bn<size_type>(num.get());
	}
};

}}

namespace std {
template<typename SizeType>
struct hash< ::censoc::netcpu::big_uint<SizeType> > {
	SizeType
	operator()(::censoc::netcpu::big_uint<SizeType> const & x) const noexcept
	{ 
		return x.hash();
	}
};
}


template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator+(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs + lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator-(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs - lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator*(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs * lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator>(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs < lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator>=(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs <= lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator<(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs > lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator<=(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs >= lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator==(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs == lhs;
}

template <typename T, typename BigIntSubtype>
::censoc::netcpu::big_uint<BigIntSubtype>
operator!=(T const & lhs, ::censoc::netcpu::big_uint<BigIntSubtype> const & rhs)
{
	return rhs != lhs;
}

#endif
