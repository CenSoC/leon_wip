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

#include <stdint.h>

#include <vector>
#include <map>

#include <boost/scoped_array.hpp>

#include "type_traits.h"

#ifndef CENSOC_EXP_LOOKUP_H
#define CENSOC_EXP_LOOKUP_H

namespace censoc { namespace exp_lookup {

//template <typename F, typename N> struct traits {};
//template <typename N> struct traits<double, N> { N const static max_exponent = 696; };
//template <typename N> struct traits<float, N> { N const static max_exponent = 81; };

template <typename F, typename N, N steps>
class linear_interpolation_linear_spacing {
public:
	typedef F float_type;
	typedef N size_type;
	typedef typename censoc::param<float_type>::type float_paramtype;
	typedef typename censoc::param<size_type>::type size_paramtype;

private:
	size_type const last_array_i;
	size_type const array_size;
	::boost::scoped_array<float_type> pos_array;
	::boost::scoped_array<float_type> neg_array;

public:

	float_type
	calculate_max_exponent() const
	{
		return last_array_i / steps;
	}

	linear_interpolation_linear_spacing(float_paramtype offset_scale)
	//: last_array_i(steps * exp_lookup::traits<F, N>::max_exponent), array_size(last_array_i + 1) {
	: last_array_i(steps * static_cast<size_type>(::std::log(::std::numeric_limits<F>::max()))), array_size(last_array_i + 1) {
		pos_array.reset(new float_type[array_size]);
		neg_array.reset(new float_type[array_size]);
		float_type * const __restrict pos(pos_array.get());
		float_type * const __restrict neg(neg_array.get());
		pos[0] = neg[0] = 1;
		for (size_type i(1); i != array_size; ++i) {
			float_type tmp(::std::exp(static_cast<float_type>(i) / steps));
			pos[i] = offset_scale * tmp;
			neg[i] = offset_scale / tmp;
		}
	}

	float_type
	eval(float_type x) const
	{
		BOOST_STATIC_ASSERT(steps > 2);

		float_type const * array;
		if (x > 0)
			array = pos_array.get();
		else {
			x = -x;
			array = neg_array.get();
		}

		// todo -- one the one hand could use fistpl faster float to int conversion in assembly, on the other hand may leave as is so that 'march=native' could take advantage of vector intrinsicts (e.g. sse et al) or other h/w assembly calls... 
		size_type const i_from(x *= steps);
		if (i_from < last_array_i) {
			size_type const i_to(i_from + 1);

			assert(x >= 0);
			assert(x >= i_from);
			assert(x <= i_to);
			assert(i_from < array_size);
			assert(i_to < array_size);
			assert(last_array_i + 1 == array_size);

			float_type const y_from(array[i_from]);
			float_type const y_to(array[i_to]);

			return (x - i_from) * (y_to - y_from) + y_from;
		} else 
			return x *= ::std::numeric_limits<float_type>::max();
	}

};

template <typename float_type, typename, bool>
struct exponent_evaluation_choice {
	float_type inline static
	eval(float_type x) 
	{
		return ::std::exp(x);
	}
};

template <typename float_type, typename size_type>
struct exponent_evaluation_choice<float_type, size_type, true> {
	exp_lookup::linear_interpolation_linear_spacing<float_type, size_type, 100> my_exp;
	exponent_evaluation_choice()
	:	my_exp(1)// .99999) 
	{
	}
	float_type inline
	eval(float_type x) const
	{
		return my_exp.eval(x);
	}
};

}}

#endif
