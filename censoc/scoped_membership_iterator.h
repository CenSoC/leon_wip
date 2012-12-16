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

#include "type_traits.h"

#ifndef CENSOC_SCOPED_MEMBERSHIP_ITERATOR_H
#define CENSOC_SCOPED_MEMBERSHIP_ITERATOR_H

namespace censoc {

// quick hack indeed
template <typename T>
struct scoped_membership_iterator : T {
	typedef typename T::container_type::iterator iterator_type;
	typedef typename censoc::param<iterator_type>::type iterator_paramtype;

	iterator_type i;

	scoped_membership_iterator(typename T::ctor_type base_ctor, iterator_paramtype i = iterator_type())
	: T(base_ctor), i(i) {
		if (i == T::container().end())
			throw ::std::runtime_error("invalid interator w.r.t. given container");
	}

	scoped_membership_iterator(iterator_paramtype i = iterator_type())
	: i(i) {
		if (i == T::container().end())
			throw ::std::runtime_error("invalid interator w.r.t. given container");
	}

	~scoped_membership_iterator()
	{
		reset();
	}

	void
	operator = (iterator_paramtype i) throw()
	{
		if (this->i != iterator_type())
			// TODO - if 'erase' is likely to throw, then save tmp copy of this->i first, then assign this->i, then erase the tmp copy of old this->i
			T::container().erase(this->i);
		this->i = i;
	}
	int
	operator == (iterator_paramtype i)
	{
		return this->i == i;
	}
	int
	operator != (iterator_paramtype i)
	{
		return this->i != i;
	}
	void
	reset()
	{
		if (i != iterator_type()) {
			T::container().erase(i);
			i = iterator_type();
		}
	}

	bool 
	nulled() const
	{
		return i == iterator_type() ? true : false;
	}
};

}

#endif
