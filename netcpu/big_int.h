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

#include <gmp.h>

#include <memory>

#include <boost/integer_traits.hpp>
#include <boost/static_assert.hpp>

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

/**
	@NOTE -- hack: currently using copy-on-write implementation to save on redundant copying (i.e. ::BN_copy, ::mpz_init_set, etc.) when passing by value (e.g. std::make_pair, etc.) 
	
	@TODO -- Later, however, would be better to implement this using explicit "move" semantics (either via template specialisation or, better yet, using new/upcoming c++ standard). This way some things could be even faster based on compile-time determination of whether the move or copy-based assignment is taking place in the code.

	For the time-being, however, it would be quicker (i.e. less development time) to deploy the copy-on-write semantics w/o any noticeable performance-related bottlenecks as big_uint is not expected to be used in the actual opaque method whose terrain is being minimized.
	*/
	::std::shared_ptr< ::mpz_t> num;

	typedef ::std::hash<big_uint<size_type> > hasher_type;

public:

	big_uint()
	: num(netcpu::big_int_context.get_bn()) {
		assert(num.unique() == true); 
	}

	big_uint(big_uint const & x)
	: num(x.num) {
		assert(num.unique() == false); 
	}

	big_uint(size_paramtype x)
	: num(netcpu::big_int_context.get_bn()) {
		assert(num.unique() == true); 
		// TODO -- perhaps specialize better for 64bit ints etc. using numeric-lib-native calls et al (if present)
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		::mpz_set_ui(*num, x);
	}

	~big_uint()
	{
		if (num.unique() == true) 
			netcpu::big_int_context.return_bn(num);
	}

	// TODO -- specialize better for 64bit ints etc. using BN_bin2bn et al
	operator size_type () const
	{
		unsigned long const tmp(::mpz_get_ui(*num));
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
		::mpz_set_ui(*num, x);
	}

	big_uint 
	operator + (size_paramtype x) const
	{
		big_uint tmp;
		::mpz_add_ui(*tmp.num, *num, x);
		return tmp;
	}

	big_uint
	operator + (big_uint const & x) const
	{
		big_uint tmp;
		::mpz_add(*tmp.num, *num, *x.num);
		return tmp;
	}

	big_uint &
	operator += (big_uint const & x) 
	{
		netcpu::big_int_context.copy_on_write(num);
		::mpz_add(*num, *num, *x.num);
		return *this;
	}

	big_uint &
	operator += (size_paramtype x) 
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::mpz_add_ui(*num, *num, x);
		return *this;
	}

	big_uint
	operator-(big_uint const & x) const
	{
		big_uint tmp;
		::mpz_sub(*tmp.num, *num, *x.num);
		return tmp;
	}

	big_uint &
	operator-=(big_uint const & x) 
	{
		netcpu::big_int_context.copy_on_write(num);
		::mpz_sub(*num, *num, *x.num);
		return *this;
	}

	big_uint
	operator*(size_paramtype x) const
	{
		big_uint tmp;
		::mpz_mul_ui(*tmp.num, *num, x);
		return tmp;
	}

	big_uint &
	operator*=(big_uint const & x) 
	{
		netcpu::big_int_context.copy_on_write(num);
		::mpz_mul(*num, *num, *x.num);
		return *this;
	}

	big_uint &
	operator*=(size_paramtype x) 
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::mpz_mul_ui(*num, *num, x);
		return *this;
	}

	big_uint 
	operator/(size_paramtype x) const
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		big_uint tmp;
		::mpz_tdiv_q_ui(*tmp.num, *num, x);
		return tmp;
	}

	big_uint &
	operator/=(size_paramtype x)
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::mpz_tdiv_q_ui(*num, *num, x);
		return *this;
	}

	big_uint
	operator%(big_uint const & x) const
	{
		big_uint tmp;
		::mpz_tdiv_r(*tmp.num, *num, *x.num);
		return tmp;
	}

	big_uint &
	operator%=(big_uint const & x)
	{
		netcpu::big_int_context.copy_on_write(num);
		::mpz_tdiv_r(*num, *num, *x.num);
		return *this;
	}

	/*
		 y += x * z
		 */
	big_uint &
	addmul(big_uint const & x)
	{
		netcpu::big_int_context.copy_on_write(num);
		::mpz_addmul(*num, *num, *x.num);
		return *this;
	}
	big_uint &
	addmul(size_paramtype x)
	{
		assert(x <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::mpz_addmul_ui(*num, *num, x);
		return *this;
	}
	big_uint &
	addmul(big_uint const & x, big_uint const & y)
	{
		netcpu::big_int_context.copy_on_write(num);
		::mpz_addmul(*num, *x.num, *y.num);
		return *this;
	}
	big_uint &
	addmul(big_uint const & x, size_paramtype y)
	{
		assert(y <= ::std::numeric_limits<unsigned long>::max());
		netcpu::big_int_context.copy_on_write(num);
		::mpz_addmul_ui(*num, *x.num, y);
		return *this;
	}

	int
	operator>(big_uint const & x) const
	{
		return ::mpz_cmp(*num, *x.num) > 0 ? !0 : 0;
	}

	int
	operator>=(big_uint const & x) const
	{
		return ::mpz_cmp(*num, *x.num) >= 0 ? !0 : 0;
	}

	int
	operator<(big_uint const & x) const
	{
		return ::mpz_cmp(*num, *x.num) < 0 ? !0 : 0;
	}

	int
	operator<=(big_uint const & x) const
	{
		return  ::mpz_cmp(*num, *x.num) <= 0 ? !0 : 0;
	}

	int
	operator==(big_uint const & x) const
	{
		return !::mpz_cmp(*num, *x.num) ? !0 : 0;
	}

	int
	operator!=(big_uint const & x) const
	{
		return ::mpz_cmp(*num, *x.num) ? !0 : 0;
	}

	size_type // ret by val because vector.size returns by val
	size() const noexcept
	{
		return (::mpz_sizeinbase(*num, 2) + 7) / 8;
	}

	void
	store(unsigned char *to) const noexcept
	{
		assert(to != NULL);
		to[0] = 0; // export may not write anything at all actually...
		::mpz_export(to, NULL, 1, 1, -1, 0, *num);
	}

	void
	load(unsigned char const * from, size_paramtype from_size) 
	{
		netcpu::big_int_context.isolate_on_write(num);
		::mpz_import(*num, from_size, 1, 1, -1, 0, from);
	}

	::std::string
	to_string() const
	{
		::std::vector<char> chars(::mpz_sizeinbase(*num, 10) + 1);
		return ::mpz_get_str(chars.data(), 10, *num);
	}

	size_type
	hash() const 
	{
		assert(num.get() != NULL);
		return netcpu::big_int_context.hash_bn<size_type>(num);
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
