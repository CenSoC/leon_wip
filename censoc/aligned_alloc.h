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
#include <assert.h>

#include <boost/noncopyable.hpp>

#include "type_traits.h"

#ifndef CENSOC_ALIGNED_ALLOC_H
#define CENSOC_ALIGNED_ALLOC_H

namespace censoc {

template <typename T, unsigned Align = censoc::DefaultAlign>
class aligned_alloc : ::boost::noncopyable {
	T * raw;
public:
	aligned_alloc() noexcept
	: raw(NULL) {
	}
	aligned_alloc(aligned_alloc && rval_ref) noexcept
	: raw(rval_ref.raw) {
		rval_ref.raw = NULL;
	}
	T *
	alloc(unsigned size) noexcept
	{ 
		BOOST_STATIC_ASSERT(Align >= sizeof(T));
		BOOST_STATIC_ASSERT(!(Align % sizeof(T)));

		raw = new T[size + Align / sizeof(T) - 1];
		uintptr_t const raw_int(reinterpret_cast<uintptr_t>(raw));
		assert(!(raw_int % sizeof(T)));
		uintptr_t const modulo(raw_int % Align);
		if (modulo) {
			assert(!(modulo % sizeof(T)));
			T * aligned = reinterpret_cast<T*>(Align - modulo + raw_int);
			assert(!((uintptr_t)aligned % Align));
			assert(!((uintptr_t)aligned % sizeof(T)));
			assert(aligned == raw + (Align - modulo) / sizeof(T));
			return aligned;
		} else
			return raw;
	}
	~aligned_alloc() noexcept
	{
		delete [] raw;
	}
};

}
#endif
