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

#include <stdexcept>
#include <list>
#include <map>
#include <iostream>

#include <censoc/stl_container_utils.h>
#include "range_utils.h"

// TODO -- an unfinished hack, as pretty much all of the current unit-tests are... when there's more time -- complete gradually.

#include "big_int_context.h"
namespace censoc { namespace netcpu { 
big_int_context_type static big_int_context;
}}
#include "big_int.h"

template <typename size_type_a, typename size_type_b>
struct suite {

	void static
	print_premap(::std::map<size_type_a, size_type_b> const & map)
	{
		::std::clog << "pre map. total size: " << map.size() << '\n';
		for (typename ::std::map<size_type_a, size_type_b>::const_iterator i(map.begin()); i != map.end(); ++i) 
			::std::clog << "range start: " << i->first << ", range size: " << i->second << ::std::endl;
	}
	void static
	print_postmap(::std::map<size_type_a, size_type_b> const & map, ::std::pair<size_type_a, size_type_b> const & b)
	{
		::std::clog << "post map. total size: " << map.size() << '\n';
		for (typename ::std::map<size_type_a, size_type_b>::const_iterator i(map.begin()); i != map.end(); ++i) 
			::std::clog << "range start: " << i->first << ", range size: " << i->second << ::std::endl;
		::std::clog << "Done range tested: " << b.first << ", " << b.second << ::std::endl;
	}
	template <typename T>
	void static
	print_postmaps(::std::map<size_type_a, size_type_b> const & map, T const & b)
	{
		::std::clog << "post map. total size: " << map.size() << '\n';
		for (typename ::std::map<size_type_a, size_type_b>::const_iterator i(map.begin()); i != map.end(); ++i) 
			::std::clog << "range start: " << i->first << ", range size: " << i->second << ::std::endl;
		::std::clog << "Done ranges tested: \n";
		for (typename T::const_iterator i(b.begin()); i != b.end(); ++i) 
			::std::clog << "range start: " << i->first << ", range size: " << i->second << ::std::endl;
	}

	void static 
	test_contained_range_post(::std::map<size_type_a, size_type_b> const & pre, ::std::map<size_type_a, size_type_b> const & a, ::std::pair<size_type_a, size_type_b> const & b)
	{
		if (pre != a) {
			::std::clog << "test failed. diagnostic follows...\n";
			::std::clog << "Pre state:\n";
			print_premap(pre);
			print_postmap(a, b);
			throw ::std::runtime_error("adding inner 'done' range to the already existing outer range should pretty much do nothing.");
		}
	}

	void static
	test_contained_range(::std::map<size_type_a, size_type_b> & a, ::std::pair<size_type_a, size_type_b> const & b)
	{
		::std::map<size_type_a, size_type_b> pre(a);
		::censoc::netcpu::range_utils::add(a, ::std::make_pair(b.first, b.second));
		test_contained_range_post(pre, a, b);
		::censoc::netcpu::range_utils::add(a, ::std::make_pair(b.first + static_cast<size_type_b>(1), b.second - static_cast<size_type_b>(2)));
		test_contained_range_post(pre, a, b);
		::censoc::netcpu::range_utils::add(a, ::std::make_pair(b.first, b.second - static_cast<size_type_b>(1)));
		test_contained_range_post(pre, a, b);
		::censoc::netcpu::range_utils::add(a, ::std::make_pair(b.first + static_cast<size_type_b>(1), b.second - static_cast<size_type_b>(1)));
		test_contained_range_post(pre, a, b);
	}

	void static
	test_glued_range(::std::map<size_type_a, size_type_b> & a, ::std::pair<size_type_a, size_type_b> const & b, size_type_b expected_postsize)
	{
		::std::string error;
		::std::map<size_type_a, size_type_b> pre(a);
		::censoc::netcpu::range_utils::add(a, b);

		typename ::std::map<size_type_a, size_type_b>::const_iterator i(a.lower_bound(b.first));
		if (i != a.end() && i->first == b.first || i == a.end()) {
			if (i == a.begin())
				error += "[gluing of the elements is missing something at the start] ";
			else 
				--i;
		}  

		if (error.empty() == true) {
			if (i->first > b.first || i->first + i->second < b.first + b.second)
				error += "[gluing of the elements caused original range to be smaller] ";
			if (static_cast<size_type_b>(a.size()) != expected_postsize)
				error += "[final size is incorrect] ";
		}

		if (error.empty() == false) {
			::std::clog << "'glued ranges on addition' test failed. diagnostic follows...\n";
			print_premap(pre);
			print_postmap(a, b);
			throw ::std::runtime_error(error);
		}
	}

	void static
	run()
	{

		// subtraction

		// 'a' covers 'b' completely, e.g.:

		// aaaaaaaaaaaaaaaaaaaaaaaaa
		//        bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 10));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 2);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 2 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(3) || a.rbegin()->first != static_cast<size_type_a>(5) || a.rbegin()->second != static_cast<size_type_b>(5)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' is completely consuming 'b' ");
			}
		}

		// minor variation: there is another element of a
		// aaaaaaaaaaaaaaaaaaaaaaaaa  aaaaaaaaaa
		//        bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 10));
			a.insert(::std::pair<size_type_a, size_type_b>(20, 10));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 2);
			::censoc::netcpu::range_utils::subtract(a, b);
			bool error(false);
			if (a.size() != 3 ) 
				error = true;
			else {
				typename ::std::map<size_type_a, size_type_b>::const_iterator i(a.begin());
				++i;
				if (a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(3) || i->first != static_cast<size_type_a>(5) || i->second != static_cast<size_type_b>(5) || a.rbegin()->first != static_cast<size_type_a>(20) || a.rbegin()->second != static_cast<size_type_b>(10)) 
					error = true;
			}

			if (error == true) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' is completely consuming 'b' ");
			}
		}

		// ... with a anb b starting at the same location
		// aaaaaaaaaaaaaaaaaaaaaaaaa
		// bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 10));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(1, 2);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(3) || a.begin()->second != static_cast<size_type_b>(8)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' is completely consuming 'b' ");
			}
		}

		// ... with a anb b ending at the same location
		// aaaaaaaaaaaaaaaaaaaaaaaaa
		//                   bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 10));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(9, 2);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(8)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' is completely consuming 'b' ");
			}
		}

		// 'a' and 'b' are 100% not exclusive, e.g.:
		// aaaa
		//        bbbbbbb

		// a leading b
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 3));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(5, 2);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a != pre) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' and 'b' are not correlated in any way");
			}
		}
		// b leading a 
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(0, 3);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a != pre) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' and 'b' are not correlated in any way");
			}
		}
		// b leading a and "just touching" (minor variant)
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(0, 5);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a != pre) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' and 'b' are not correlated in any way");
			}
		}

		// a and b are the same, e.g.:
		// aaaaa
		// bbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(5, 2);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size()) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' and 'b' are the same");
			}
		}
		// minor variation, some elements are preceeding in 'a'
		// aaa aaaaa
		//     bbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 1));
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(5, 2);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(1)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' and 'b' are the same");
			}
		}

		// b covers a ...

		//        aaaaa
		// bbbbbbbbbbbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(1, 10);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size()) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'b' covers 'a'");
			}
		}

		// ... and both start at the same location
		// aaaaa
		// bbbbbbbbbbbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(5, 10);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size()) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'b' covers 'a'");
			}
		}

		// ... and both end at the same location
		//                 aaaaa
		// bbbbbbbbbbbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(9, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(1, 10);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size()) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'b' covers 'a'");
			}
		}

		// a and b overlap

		// aaaaaaaaa
		//      bbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 5));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 5);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(2)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' and 'b' overlap");
			}
		}
		//    aaaaaaaaa
		// bbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 5));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 5);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(8) || a.begin()->second != static_cast<size_type_b>(2)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'a' and 'b' overlap");
			}
		}

		// b overlaps with different instaces of a
		// aaaaaaa       aaaaaaa
		//    bbbbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 5));
			a.insert(::std::pair<size_type_a, size_type_b>(10, 5));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 9);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 2 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(2) || a.rbegin()->first != static_cast<size_type_a>(12) || a.rbegin()->second != static_cast<size_type_b>(3)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'b' overlaps different instaces of 'a'");
			}
		}
		// aaaaaaa  aa   aaaaaaa
		//    bbbbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 5));
			a.insert(::std::pair<size_type_a, size_type_b>(7, 2));
			a.insert(::std::pair<size_type_a, size_type_b>(10, 5));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 9);
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 2 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(2) || a.rbegin()->first != static_cast<size_type_a>(12) || a.rbegin()->second != static_cast<size_type_b>(3)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed subtraction test when 'b' overlaps different instaces of 'a'");
			}
		}

		// minor variant two b overlap with two a
		// aaaaaaaaaa    aaaaaaaaaaaa
		//        bbbbbbbbbb     bbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 10));
			a.insert(::std::pair<size_type_a, size_type_b>(15, 10));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::list< ::std::pair<size_type_a, size_type_b> > b;
			b.push_back(::std::pair<size_type_a, size_type_b>(7, 10));
			b.push_back(::std::pair<size_type_a, size_type_b>(21, 10));
			::censoc::netcpu::range_utils::subtract(a, b);
			if (a.size() != 2 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(6) || a.rbegin()->first != static_cast<size_type_a>(17) || a.rbegin()->second != static_cast<size_type_b>(4)) {
				print_premap(pre);
				print_postmaps(a, b);
				throw ::std::runtime_error("failed subtraction test when two instances of 'b' overlap/interleave w.r.t. two instaces of 'a'");
			}
		}

		// addition

		// 'done' range is entirely contained by the existing range, e.g.:
		// aaaaaaaaaaaaaaaaaaaaaaaaa
		//        bbbbbbb

		{
			// with 1 element in a
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 10));
			test_contained_range(a, ::std::pair<size_type_a, size_type_b>(0, 10));
			// with some elements after the 'added' element
			a.clear();
			a.insert(::std::pair<size_type_a, size_type_b>(0, 10));
			a.insert(::std::pair<size_type_a, size_type_b>(12, 2));
			a.insert(::std::pair<size_type_a, size_type_b>(15, 21));
			test_contained_range(a, ::std::pair<size_type_a, size_type_b>(0, 10));
			// with some elements before the 'added' element
			a.clear();
			a.insert(::std::pair<size_type_a, size_type_b>(0, 1));
			a.insert(::std::pair<size_type_a, size_type_b>(2, 5));
			a.insert(::std::pair<size_type_a, size_type_b>(10, 10));
			test_contained_range(a, ::std::pair<size_type_a, size_type_b>(10, 10));
			// with some elements on both sides of the 'added' element
			a.clear();
			a.insert(::std::pair<size_type_a, size_type_b>(0, 1));
			a.insert(::std::pair<size_type_a, size_type_b>(2, 5));
			a.insert(::std::pair<size_type_a, size_type_b>(10, 3));
			a.insert(::std::pair<size_type_a, size_type_b>(15, 100));
			test_contained_range(a, ::std::pair<size_type_a, size_type_b>(10, 3));
		}

		// 'done' range acts as a glue, e.g.:
		// aaaaaaa       aaaaaaaaaaa
		//        bbbbbbb
		{

			// with 3 elements (glue being the middle one)
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 2));
			a.insert(::std::pair<size_type_a, size_type_b>(3, 1));
			test_glued_range(a, ::std::pair<size_type_a, size_type_b>(2, 1), 1);
			// with multiple elements being covered by the added one
			a.clear();
			a.insert(::std::pair<size_type_a, size_type_b>(0, 2));
			a.insert(::std::pair<size_type_a, size_type_b>(3, 1));
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			a.insert(::std::pair<size_type_a, size_type_b>(8, 3));
			a.insert(::std::pair<size_type_a, size_type_b>(12, 1));
			test_glued_range(a, ::std::pair<size_type_a, size_type_b>(2, 10), 1);
		}

		// aaaaa
		//         bbbbb

		// a leading b
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 3));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(5, 2);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 2 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(3) || a.rbegin()->first != static_cast<size_type_a>(5) || a.rbegin()->second != static_cast<size_type_b>(2)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' and 'b' are not correlated in any way");
			}
		}
		// b leading a 
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(0, 3);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 2 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(3) || a.rbegin()->first != static_cast<size_type_a>(5) || a.rbegin()->second != static_cast<size_type_b>(2)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' and 'b' are not correlated in any way");
			}
		}
		// b leading a and "just touching" (minor variant)
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(5, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(0, 5);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(7)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' and 'b' are not correlated in any way");
			}
		}

		// b containing a

		// aaa
		// bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 2));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(1, 5);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(5)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' is covered by 'b'");
			}
		}
		//     aaa
		// bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(3, 5));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(1, 7);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(7)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' is covered by 'b'");
			}
		}
		//   aaa
		// bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(2, 3));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(0, 7);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(7)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' is covered by 'b'");
			}
		}

		// a and b overlapping (although these can be done with a 'test_glue' method above... albeit, if need be, somewhat altered for greater expected precision...)

		// aaaaaaa
		//     bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 3));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(1, 7);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(8)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' overlaps w.r.t. 'b'");
			}
		}
		// minor glue variant:
		// aaaa
		//     bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 1));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(1, 7);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(8)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' is extended by 'b'");
			}
		}
		//     aaaaaaa
		// bbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 7));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(0, 3);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(8)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' overlaps w.r.t. 'b'");
			}
		}

		// one b overlapping two a
		// aaaaaaaa    aaaaaaaa
		//     bbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 5));
			a.insert(::std::pair<size_type_a, size_type_b>(10, 5));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 10);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(15)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' overlaps w.r.t. 'b'");
			}
		}

		// one b overlapping two outer a with one inner a
		// aaaaaaaa  aaa  aaaaaaaa
		//     bbbbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(0, 5));
			a.insert(::std::pair<size_type_a, size_type_b>(7, 2));
			a.insert(::std::pair<size_type_a, size_type_b>(10, 5));
			::std::map<size_type_a, size_type_b> pre(a);
			::std::pair<size_type_a, size_type_b> b(3, 10);
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(0) || a.begin()->second != static_cast<size_type_b>(15)) {
				print_premap(pre);
				print_postmap(a, b);
				throw ::std::runtime_error("failed addition test when 'a' overlaps w.r.t. 'b'");
			}
		}

		// minor variant two b overlap with two a
		// aaaaaaaaaa    aaaaaaaaaaaa
		//        bbbbbbbbbb     bbbbbbbbbbbb
		{
			::std::map<size_type_a, size_type_b> a;
			a.insert(::std::pair<size_type_a, size_type_b>(1, 10));
			a.insert(::std::pair<size_type_a, size_type_b>(15, 10));

			if (::censoc::netcpu::range_utils::find(a, size_type_a(1)) == false || ::censoc::netcpu::range_utils::find(a, size_type_a(2)) == false || ::censoc::netcpu::range_utils::find(a, size_type_a(9)) == false || ::censoc::netcpu::range_utils::find(a, size_type_a(11)) == true || ::censoc::netcpu::range_utils::find(a, size_type_a(15)) == false)
				throw ::std::runtime_error("failed find test");

			::std::map<size_type_a, size_type_b> pre(a);
			::std::list< ::std::pair<size_type_a, size_type_b> > b;
			b.push_back(::std::pair<size_type_a, size_type_b>(7, 10));
			b.push_back(::std::pair<size_type_a, size_type_b>(21, 10));
			::censoc::netcpu::range_utils::add(a, b);
			if (a.size() != 1 || a.begin()->first != static_cast<size_type_a>(1) || a.begin()->second != static_cast<size_type_b>(30)) {
				print_premap(pre);
				print_postmaps(a, b);
				throw ::std::runtime_error("failed addition test when two instances of 'b' overlap/interleave w.r.t. two instaces of 'a'");
			}
		}

	}
};

int
main()
{
	suite<unsigned, unsigned>::run();
	suite< ::censoc::netcpu::big_uint<unsigned>, unsigned>::run();
	suite< ::censoc::netcpu::big_uint<unsigned>, ::censoc::netcpu::big_uint<unsigned> >::run();
	return 0;
}

