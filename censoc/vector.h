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

#ifndef CENSOC_VECTOR_H
#define CENSOC_VECTOR_H

#if 0

namespace censoc {

template <typename T, typename N, N Alignment>
class vector {
	vector(vector<T, N, Alignment> const &);
	T * x;
	// not using new/delete because there is no way to shrink (ala realloc with smaller new size) the memory, which may be more elegant in some cases. Such as reserve vector, populate some, free remaining.
	void *
	allocate(N const & size) const
	{
		void * rv;
		if (::posix_memalign(&rv, Alignment, size))
			throw ::std::runtime_error("posix_memalign"); // todo make it more informative
		return rv;
	}
public:
	vector()
	: x(NULL) {
	}
	vector(vector<T, N, Alignment> &x)
	: x(x.x) {
		x.x = NULL;
	}
	vector(N const & size)
	{
		assert(size > 0);
		x = new (allocate(size)) T;
	}
	template <typename I>
	vector(N const & size, I const & i)
	{
		assert(size > 0);
		x = new (allocate(size)) T(i);
	}
	~vector()
	{
		::std::cerr << "freeing : " << x << ::std::endl;
		::free(x);
	}
	void
	clamp(N const & size)
	{
		x = ::reallocf(x, size);
	}
	T &
	construct(N const & index)
	{
		return new(x + index) T;
	}
	template <typename I>
	T &
	construct(N const & index, I const & i)
	{
		return new(x + index) T(i);
	}
	T const &
	construct(N const & index) const
	{
		return new(x + index) T;
	}
	template <typename I>
	T const &
	construct(N const & index, I const & i) const
	{
		return new(x + index) T(i);
	}
	T & get (N const & index)
	{
		assert(x != NULL);
		return x[index];
	}
	T const & get (N const & index) const
	{
		assert(x != NULL);
		return x[index];
	}
};

}

#endif

#endif
