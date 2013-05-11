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

#ifndef CENSOC_NETCPU_RANGE_UTILS_H
#define CENSOC_NETCPU_RANGE_UTILS_H

namespace censoc { namespace netcpu { namespace range_utils {


#ifndef NDEBUG
	template <typename T>
	void
	test_container(T const & a) 
	{
		if (a.size() < 2)
			return;
		else {
			typedef typename censoc::strip_const<typename T::value_type::first_type>::type size_type;
			typedef typename censoc::param<size_type>::type size_paramtype;
			typename T::const_iterator j(a.begin());
			size_type i_begin(j->first);
			size_type i_end(i_begin + j->second);
			assert(i_end > i_begin);
			for (++j; j != a.end(); ++j) {
				size_paramtype j_begin(j->first);
				size_paramtype j_end(j_begin + j->second);
				assert(j_end > j_begin);
				assert(i_end < j_begin);
				i_begin = j_begin;
				i_end = j_end;
			}
		}
	}
#endif

	/**
		@pre -- 'a' is ordered and have non-overlapping ranges w.r.t. their own contents; moreover range's elements are not contiguous (i.e. end of 1st element is not the beginnig of the second element -- rather there is a gap, or the 1st element should simply be longer thereby consuming the 2nd element).
		@note -- if need be, shall write the variant of this (but NOT a replacement) which will perform under more generalized predicates but, naturally, at a price of greater number of code complexity, cpu cycyles and larger code-section related memory usage.

		@post -- range represented by 'b' (or any portion thereof which is implicitly present in 'a') is taken out from 'a'. If need be, ranges in 'a' are deleted, top- tail-adjusted, or refactored into multiple ranges. 
	 */
	template <typename MAP, typename A, typename B>
	void
	subtract(MAP & a, ::std::pair<A, B> const & i) 
	{
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
		test_container(a);
#endif
#endif
		// TODO -- make it into an assertion and have a 'slower'/'generic' method which will test it...
		if (a.empty() == true) 
			return;

		//typedef typename MAP::value_type::second_type size_type;
		typedef typename censoc::strip_const<typename MAP::value_type::first_type>::type size_type;
		typedef typename censoc::param<size_type>::type size_paramtype;

		size_paramtype i_begin(i.first);
		size_paramtype i_end(i.first + i.second);
		assert(i_begin != i_end);

		typename MAP::iterator j(a.lower_bound(i_begin));
		size_paramtype j_begin(j->first);

		if (j == a.end() || j_begin > i_begin && j != a.begin())
			--j;

		while (j != a.end()) {

			// @NOTE watch out for relevant map element not to be erased before any referencing to the given temporary is made!!! (it is referenced because the code may be used with BIGNUM, etc. where the types are not primitive)
			size_paramtype j_begin(j->first);
			size_paramtype j_end(j->first + j->second);
			assert(j_begin != j_end);

			// quick-basic hacking for the time-being

			if (i_end <= j_begin) { // nothing to do (range to subtract does not exist)
				return;
			} else if (i_begin <= j_begin && i_end >= j_end) { // delete (range to subtract consumes the given "existing" range)
				a.erase(j++);
			} else if (i_begin > j_begin && i_end < j_end) { // split 
				j->second = i_begin - j_begin;
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
#ifndef NDEBUG
				typename MAP::iterator j_ = 
#endif
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				a.insert(++j, ::std::make_pair(i_end, j_end - i_end));
#ifndef NDEBUG
				assert(j_->second != static_cast<typename MAP::value_type::second_type>(0));
				assert(++j_ == j);
#endif
				return;
			} else if (i_begin < j_end && i_begin > j_begin && i_end >= j_end) { // shorten from the right
				j->second = i_begin - j_begin;
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
				++j;
			} else if (i_begin <= j_begin && i_end < j_end) { // shorten from left
				typename MAP::iterator j_(j);
#ifndef NDEBUG
				typename MAP::iterator j_inc_(j);
				++j_inc_;
#endif
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				j = a.insert(++j, ::std::make_pair(i_end, j_end - i_end));
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
#ifndef NDEBUG
				typename MAP::iterator tmp_(j_);
				assert(++tmp_ == j);
				assert(++tmp_ == j_inc_);
#endif
				a.erase(j_);
			} else  
				++j;
		}
	}

	/**
		@pre -- both 'a' and 'b' are ordered containers and have non-overlapping ranges w.r.t. their own contents; moreover range's elements are not contiguous (i.e. end of 1st element is not the beginnig of the second element -- rather there is a gap, or the 1st element should simply be longer thereby consuming the 2nd element).

		@post -- for every element of 'b' a 'subtract' from 'a' is called. Semantically, this is equivalent to taking the intersecting areas of two sets and subtracting those from set 'a'...

		@post -- returns 'false' if 'a' was not changed, 'true' otherwise
	 */
	template <typename MAP, typename T>
	bool
	subtract(MAP & a, T const & b) 
	{
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
		test_container(a);
		test_container(b);
#endif
#endif
		assert(b.empty() == false);

		// TODO -- make it into an assertion and have a 'slower'/'generic' method which will test it...
		if (a.empty() == true) 
			return false;

		//typedef typename MAP::value_type::second_type size_type;
		typedef typename censoc::strip_const<typename MAP::value_type::first_type>::type size_type;
		typedef typename censoc::param<size_type>::type size_paramtype;

		typename T::const_iterator i(b.begin());
		size_paramtype i_begin(i->first);

		typename MAP::iterator j(a.lower_bound(i->first));
		size_paramtype j_begin(j->first);

		if (j == a.end() || j_begin > i_begin && j != a.begin())
			--j;

		bool rv(false);
		while (i != b.end() && j != a.end()) {

			size_paramtype i_begin(i->first);
			size_paramtype i_end(i->first + i->second);
			assert(i_begin != i_end);

			// @NOTE watch out for relevant map element not to be erased before any referencing to the given temporary is made!!! (it is referenced because the code may be used with BIGNUM, etc. where the types are not primitive)
			size_paramtype j_begin(j->first);
			size_paramtype j_end(j->first + j->second);
			assert(j_begin != j_end);

			// quick-basic hacking for the time-being

			if (i_end <= j_begin) { // nothing to do (range to subtract does not exist)
				++i;
			} else if (i_begin <= j_begin && i_end >= j_end) { // delete (range to subtract consumes the given "existing" range)
				a.erase(j++);
				rv = true;
			} else if (i_begin > j_begin && i_end < j_end) { // split
				j->second = i_begin - j_begin;
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				j = a.insert(++j, ::std::make_pair(i_end, j_end - i_end));
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
				++i;
				rv = true;
			} else if (i_begin < j_end && i_begin > j_begin && i_end >= j_end) { // shorten from the right 
				j->second = i_begin - j_begin;
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
				++j;
				rv = true;
			} else if (i_begin <= j_begin && i_end < j_end) { // shorten from left
				typename MAP::iterator j_(j);
#ifndef NDEBUG
				typename MAP::iterator j_inc_(j);
				++j_inc_;
#endif
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				j = a.insert(++j, ::std::make_pair(i_end, j_end - i_end));
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
#ifndef NDEBUG
				typename MAP::iterator tmp_(j_);
				assert(++tmp_ == j);
				assert(++tmp_ == j_inc_);
#endif
				a.erase(j_);
				rv = true;
			} else  
				++j;
		}
		return rv;
	}

	/**
		@post -- adds a new range member to the container. moreover, if newely introduced range is simply an extension (i.e. contiguous) w.r.t. any existing ranges -- then instead of adding a new range, the already-existing one is extended.
		*/
	template <typename MAP, typename A, typename B>
	void
	add(MAP & a, ::std::pair<A, B> const & i) 
	{
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
		test_container(a);
#endif
#endif
		//typedef typename MAP::value_type::second_type size_type;
		typedef typename censoc::strip_const<typename MAP::value_type::first_type>::type size_type;
		typedef typename censoc::param<size_type>::type size_paramtype;

		// TODO -- make it into an assertion and have a 'slower'/'generic' method which will test it...
		if (a.empty() == true) {
			a.insert(i);
			return;
		}

		size_paramtype i_begin(i.first);
		size_paramtype i_end(i.first + i.second);
		assert(i_begin != i_end);

		typename MAP::iterator j(a.lower_bound(i.first));
		size_paramtype j_begin(j->first);

		if (j == a.end() || j_begin > i_begin && j != a.begin())
			--j;

		while (j != a.end()) {

			// @NOTE watch out for relevant map element not to be erased before any referencing to the given temporary is made!!! (it is referenced because the code may be used with BIGNUM, etc. where the types are not primitive)
			size_paramtype j_begin(j->first);
			size_paramtype j_end(j->first + j->second);
			assert(j_begin != j_end);

			// quick-basic hacking for the time-being

			if (i_end < j_begin) { // add new (nothing in common with existing)
#ifndef NDEBUG
				typename MAP::iterator j_ = 
#endif
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				a.insert(j, i);
				assert(++j_ == j);
				return;
			} else if (i_begin >= j_begin && i_end <= j_end) { // do nothing (all of the to-add range is already present) 
				return;
			} else if (i_begin <= j_end && i_end > j_end) { // at least partially will need to extend to the right 
					j->second = i_end - j_begin; 
					typename MAP::iterator k(j);
					++k;
					while (k != a.end()) {
						if (k->first + k->second <= i_end) {
							a.erase(k++);
						} else if (k->first > i_end)
							break;
						else {
							assert(k->first <= i_end);
							assert(k->first + k->second > i_end);
							j->second += k->first + k->second - i_end;
							a.erase(k);
							break;
						}
					}
			} else if (i_begin < j_begin) { // at least partially will need to extend to the left 
				typename MAP::iterator j_(j);
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				j = a.insert(j, ::std::make_pair(i_begin, j->second + j_begin - i_begin));
#ifndef NDEBUG
				typename MAP::iterator tmp_(j);
				assert(++tmp_ == j_);
#endif
				a.erase(j_);
			} else  
				++j;
		}

		assert(i_begin != i_end);
		assert(j == a.end());
		assert(a.empty() == false);

		if (a.rbegin()->first + a.rbegin()->second < i_begin)
			// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
			a.insert(a.end(), i);

	}

	template <typename MAP, typename T>
	void
	add(MAP & a, T const & b) 
	{
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
		test_container(a);
		test_container(b);
#endif
#endif
		assert(b.empty() == false);

		if (a.empty() == true) {
			for (typename T::const_iterator i(b.begin()); i != b.end(); ++i)
				a.insert(*i);
			return;
		}

		//typedef typename MAP::value_type::second_type size_type;
		typedef typename censoc::strip_const<typename MAP::value_type::first_type>::type size_type;
		typedef typename censoc::param<size_type>::type size_paramtype;

		typename T::const_iterator i(b.begin());
		size_paramtype i_begin(i->first);

		typename MAP::iterator j(a.lower_bound(i->first));
		size_paramtype j_begin(j->first);

		if (j == a.end() || j_begin > i_begin && j != a.begin())
			--j;

		while (i != b.end() && j != a.end()) {

			size_paramtype i_begin(i->first);
			size_paramtype i_end(i->first + i->second);
			assert(i_begin != i_end);

			// @NOTE watch out for relevant map element not to be erased before any referencing to the given temporary is made!!! (it is referenced because the code may be used with BIGNUM, etc. where the types are not primitive)
			size_paramtype j_begin(j->first);
			size_paramtype j_end(j->first + j->second);
			assert(j_begin != j_end);

			// quick-basic hacking for the time-being

			if (i_end < j_begin) { // add new (nothing in common with existing)
#ifndef NDEBUG
				typename MAP::iterator j_ = 
#endif
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				a.insert(j, *i++);
				assert(++j_ == j);
			} else if (i_begin >= j_begin && i_end <= j_end) { // do nothing (all of the to-add range is already present) 
				++i;
			} else if (i_begin <= j_end && i_end > j_end) { // at least partially will need to extend to the right 
					j->second = i_end - j_begin; 
					typename MAP::iterator k(j);
					++k;
					while (k != a.end()) {
						if (k->first + k->second <= i_end) {
							a.erase(k++);
						} else if (k->first > i_end)
							break;
						else {
							assert(k->first <= i_end);
							assert(k->first + k->second > i_end);
							j->second += k->first + k->second - i_end;
							a.erase(k);
							break;
						}
					}
			} else if (i_begin < j_begin) { // at least partially will need to extend to the left 
				typename MAP::iterator j_(j);
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				j = a.insert(j, ::std::make_pair(i_begin, j->second + j_begin - i_begin));
				assert(j->second != static_cast<typename MAP::value_type::second_type>(0));
#ifndef NDEBUG
				typename MAP::iterator tmp_(j);
				assert(++tmp_ == j_);
#endif
				a.erase(j_);
			} else  
				++j;
		}

		while (i != b.end()) {
			size_paramtype i_begin(i->first);

#ifndef NDEBUG
			size_paramtype i_end(i->first + i->second);
#endif
			assert(i_begin != i_end);
			assert(j == a.end());

			if (a.rbegin()->first + a.rbegin()->second < i_begin)
				// todo -- when gcc stdlibc++ compiler (or other, relevant ones) begin to support emplace_hint on ordered maps, then use emplace_hint instead of insert
				a.insert(a.end(), *i);

			++i;
		}
	}

	template <typename MAP>
	bool
	find(MAP & a, typename censoc::param<typename censoc::strip_const<typename MAP::value_type::first_type>::type>::type x) 
	{
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
		test_container(a);
#endif
#endif
		//typedef typename MAP::value_type::second_type size_type;
		typedef typename censoc::strip_const<typename MAP::value_type::first_type>::type size_type;
		typedef typename censoc::param<size_type>::type size_paramtype;

		// TODO -- make it into an assertion and have a 'slower'/'generic' method which will test it...
		if (a.empty() == true) 
			return false;

		typename MAP::iterator j(a.lower_bound(x));
		if (j == a.end())
			--j;
		else if (j->first != x) {
			assert(j->first > x);
			if (j != a.begin())
				--j;
			else 
				return false;
		}

		assert(j->first <= x);

		// explicit test for equality to prevent on 'add' op in cases where custom types are used (e.g. big_int)
		if (j->first == x || j->first + j->second > x)
			return true;
		else
			return false;
	}

}}}

#endif
