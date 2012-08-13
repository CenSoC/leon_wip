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

#include <censoc/type_traits.h>

#ifndef CENSOC_NETCPU_VALUE_ON_GRID_H
#define CENSOC_NETCPU_VALUE_ON_GRID_H

namespace censoc { namespace netcpu { 

template <typename size_type, typename float_type>
class value_on_grid {
	typedef typename censoc::param<size_type>::type size_paramtype; 
	typedef typename censoc::param<float_type>::type float_paramtype; 

	size_type i;
	float_type f;

public:
#ifndef NDEBUG
	value_on_grid() 
	: i(-1) {
	}
#endif
	void
	index(size_paramtype grid_i, size_paramtype grid_steps, float_paramtype value_from, float_paramtype rand_range)
	{
		assert (grid_i <= grid_steps); // for the time being (i.e. as long as not setting 'max' to initial server state msg)
		f = value_from + ((i = grid_i) + 1) * (rand_range / (grid_steps + 1));
	}

	typename censoc::strip_const<size_paramtype>::type
	index() const
	{
		assert(i != static_cast<size_type>(-1));
		return i;
	}

	typename censoc::strip_const<float_paramtype>::type
	value() const
	{
		assert(i != static_cast<size_type>(-1));
		return f;
	}

	int
	operator == (value_on_grid const & x) const
	{
		assert(i != static_cast<size_type>(-1));
		return i == x.i;
	}

	int
	operator != (value_on_grid const & x) const
	{
		assert(i != static_cast<size_type>(-1));
		return i != x.i;
	}

	void
	clobber(size_paramtype i, float_paramtype f)
	{
		this->i = i;
		this->f = f;
	}

#ifndef NDEBUG
	void
	value_testonly(float_paramtype f)
	{
		uint64_t i_tmp(uint64_t(2000000000) + int64_t(100000000) * double(f));
		assert(i_tmp < ::std::numeric_limits<size_type>::max());
		this->i = i_tmp;
		this->f = f;
	}
#endif
};

}}

#endif
