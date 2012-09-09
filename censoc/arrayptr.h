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

// NOTE -- currently don't have internet connection so can't install boost. when will get some time -- will use boost :-) In the meantime, writing this code only takes a few minutes anyway...

#include <stdlib.h>

#ifndef CENSOC_ARRAYPTR_H
#define CENSOC_ARRAYPTR_H

namespace censoc {

template <typename T, typename N>
struct array_base {
	operator T * ()
	{
		assert(x != NULL);
		return x;
	}
	operator T const * () const
	{
		assert(x != NULL);
		return x;
	}
	T * operator + (N const & n)
	{
		assert(x != NULL);
		assert(n <= size); // the <= is not really safe, but allowing for 'end' semantics
		return x + n;
	}
	T const * operator + (N const & n) const
	{
		assert(x != NULL);
		assert(n <= size); // the <= is not really safe, but allowing for 'end' semantics
		return x + n;
	}
protected:
	array_base()
	: x(NULL) 
#ifndef NDEBUG
		, size(0)
#endif
	{
	}
	array_base(T * x)
	: x(x) {
		assert(x != NULL);
	}
	T * x;

#ifndef NDEBUG
	N size;
#endif

private:
	array_base(array_base const &);
	void operator = (array_base const &);
};

template <typename T, typename N>
struct array : public array_base<T, N> {
	array()
	{
	}
	typedef array_base<T, N> base;
	array(N const & n)
	: base(new T[n]) {
#ifndef NDEBUG
		base::size = n;;
#endif
	}
	~array()
	{
		delete []base::x;
	}

	void
	reset(N const & n)
	{
		delete []base::x;

		base::x = new T[n];
#ifndef NDEBUG
		base::size = n;;
#endif
	}
};

template <typename N>
struct array<char, N> : public array_base<char, N> {
	typedef array_base<char, N> base;
	array()
	{
	}
	array(N const & n)
	: base(static_cast<char *>(::malloc(static_cast< ::size_t>(n)))) {
		if (base::x == NULL)
			throw ::std::runtime_error("::malloc failed in array<char> ctor");
#ifndef NDEBUG
		base::size = n;;
#endif
	}
	~array()
	{
		::free(base::x);
	}
	void
	reset(N const & n)
	{
		::free(base::x);

		base::x = static_cast<char *>(::malloc(static_cast< ::size_t>(n)));
		// C++ 'new' vs C's 'malloc' differ in their standardization for zero-sized requests (non-error conditions should return non-NULL pointer in C++, yet 'malloc' may indeed return a NULL pointer -- as, indeed, possible with runtime-tuning as per 'man malloc' the V option under FreeBSD).
		if (base::x == NULL && n)
			throw ::std::runtime_error("::malloc failed in array<char> reset");

#ifndef NDEBUG
		base::size = n;
#endif
	}
	void
	resize(N const & n)
	{
		if (base::x == NULL)
			reset(n);
		else {
			char * new_x(static_cast<char *>(::realloc(base::x, n)));
			::free(base::x);
			base::x = new_x;
			// C++ 'new' vs C's 'malloc' differ in their standardization for zero-sized requests (non-error conditions should return non-NULL pointer in C++, yet 'malloc' may indeed return a NULL pointer -- as, indeed, possible with runtime-tuning as per 'man malloc' the V option under FreeBSD).
			if (base::x == NULL && !n)
				throw ::std::runtime_error("::realloc failed in array<char> resize");
#ifndef NDEBUG
			base::size = n;;
#endif
		}
	}
};

}

#endif
