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

#ifndef CENSOC_STL_CONTAINERS_UTILS_H
#define CENSOC_STL_CONTAINERS_UTILS_H

namespace censoc { namespace stl {

	template <typename Container, typename Size>
	typename Container::iterator
	accessor_subscript_via_iteration(Container & container, Size subscript)
	{
		assert(container.empty() == false);
		typename Container::iterator i(container.begin());
		while (subscript--) {
			++i; 
			assert(i != container.end());
		}
		return i;
	}

	template <typename Value, typename Container>
	bool
	find_value_via_iteration(typename censoc::param<Value>::type value, Container & container)
	{
		for (typename Container::iterator i(container.begin()); i != container.end(); ++i) 
			if (*i == value)
				return true;
		return false;
	}

	template <typename Container>
	bool
	find_iterator(typename Container::iterator & iterator, Container & container)
	{
		for (typename Container::iterator i(container.begin()); i != container.end(); ++i) 
			if (i == iterator)
				return true;
		return false;
	}

	// NOTE -- unfinished, TODO -- finish!
	template <typename Container, typename Size>
	struct fastsize : protected Container {
		Size size_;
		fastsize()
		: size_(0) {
		}

#ifndef NDEBUG
		~fastsize()
		{
			assert(size_ == Container::size());
		}
#endif

		using Container::value_type;
		using Container::const_iterator;
		using Container::iterator;
		using Container::begin;
		using Container::end;
		using Container::empty;
		using Container::front;
		using Container::back;

		void
		clear()
		{
			size_ = 0;
			Container::clear();
			assert(size_ == Container::size());
		}

		void
		erase(typename Container::iterator i)
		{
			Container::erase(i);
			--size_;
			assert(size_ == Container::size());
		}
		typename Container::iterator
		insert(typename censoc::param<typename Container::iterator>::type i, typename censoc::param<typename Container::value_type>::type x)
		{
			++size_;
#ifndef NDEBUG
			return Container::insert(i, x);
#else
			typename Container::iterator rv(Container::insert(i, x));
			assert(size_ == Container::size());
			return rv;
#endif
		}
		void
		push_back(typename censoc::param<typename Container::value_type>::type x)
		{
			Container::push_back(x);

			++size_;
			assert(size_ == Container::size());
		}
		void
		push_front(typename censoc::param<typename Container::value_type>::type x)
		{
			Container::push_front(x);

			++size_;
			assert(size_ == Container::size());
		}
		Size
		size() const
		{
			assert(size_ == Container::size());
			return size_;
		}
	};


}}

#endif
