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

// @note -- quick hack at the moment (empty cells are allowed, but empty rows will be quietly discarded)
// @note -- any whitespace that is not ensclosed by double quotes is not guaranteed to be preserved.
// @note -- the double quote itself (as a part of the fields value) must be encoded as two consecutive double quotes
// @note -- a number may not need to be enclosed in double quotes.
// @note -- if an encoded double-quote character appears in the cell value, then such cell value must be enclosed in double quotes. for example to have a cell value consisting of nothing but a double-quote character, the following must be present: """"

#include <assert.h>
#include <stdint.h>

#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>

#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/compare.h>

#include "util.h"

#ifndef CENSOC_SPREADSHEET_CSV_H
#define CENSOC_SPREADSHEET_CSV_H

namespace censoc { namespace spreadsheet {


template <typename size_type, typename xxx, typename Base>
class csv : public Base {

	typedef typename censoc::param<size_type>::type size_paramtype;

	::std::vector< ::std::vector< ::std::string> > grid;

	size_type current_row_i;

	size_type const static buf_size = 10 * 1024 * 1024; // todo -- make tunable
	char buf[buf_size]; 

	size_type const static grid_grow_rows_by = 200;
	size_type const static grid_grow_columns_by = 15;

public:
	csv(::std::istream & is, typename Base::ctor const & base_ctor)
	: Base(base_ctor), current_row_i(-1) {

		// TODO -- find out a more standard-guaranteed way. may not be so easy (whilst it is possible to generate own streambuf inheriting types which have access to setg and setp calls, some streams like ofstream/istream have pre-defined filebuf objects which have setg/setp as protected...) 
	  is.rdbuf()->pubsetbuf(buf, buf_size);

		size_type rows_reservations_size(1000); // todo -- make tunable
		size_type columns_reservations_size(75); // todo -- make tunable

		grid.reserve(rows_reservations_size); 

		bool newrow_pending(true);
		bool unmatched_quotes(false);

		while (!0) {

			char c(is.get());
			if (!is.good())
				return;

			if (!::isspace(c)) {
				if (newrow_pending == true) {
					newrow_pending = false;
					if (grid.size() == rows_reservations_size) {
						size_type const new_rows_reservations_size(rows_reservations_size + grid_grow_rows_by);
						if (::std::numeric_limits<size_t>::max() < new_rows_reservations_size || new_rows_reservations_size < rows_reservations_size)
							throw ::std::runtime_error(xxx() << "Failed to grow the rows buffer for csv parser(=" << this << ")");
						grid.reserve(rows_reservations_size = new_rows_reservations_size);
					}
					assert(is.tellg() > 0);
					grid.push_back(::std::vector< ::std::string>());
					grid.back().reserve(columns_reservations_size);
					grid.back().push_back(::std::string());

					while (!0) {
						if (c == ',') {
							if (unmatched_quotes == false) {
								if (grid.back().size() == columns_reservations_size) {
									size_type const new_columns_reservations_size(columns_reservations_size + grid_grow_columns_by);
									if (::std::numeric_limits<size_t>::max() < new_columns_reservations_size || new_columns_reservations_size <= columns_reservations_size)
										throw ::std::runtime_error(xxx() << "Failed to grow the columns buffer for csv parser(=" << this << ")");
									grid.back().reserve(columns_reservations_size = new_columns_reservations_size);
								}
								grid.back().push_back(::std::string());
							} else 
								grid.back().back() += c;
						} else if (isnewline(c) == true) {
							newrow_pending = true;
							break;
						} else if (c == '"') {
							if (unmatched_quotes == true) {
								if (is.peek() == '"') {
									grid.back().back() += c;
									is.get();
									assert(is.good());
								} else
									unmatched_quotes = false;
							} else
								unmatched_quotes = true;
						} else if (!::isspace(c) || unmatched_quotes == true)
							grid.back().back() += c;
						c = is.get();
						if (!is.good())
							return;
					}
				} 
			} 
		}
	}

	/**
		@desc positions the internal semantic "read pointer" to a given row
		@param "i" is 0-based
		@todo -- deprecate, this is largely due to compatibility with the xls lib (no longer to be used)
	*/
	void
	torow(size_paramtype i)
	{
		assert(assert_common() == true);
		if (i >= grid.size())
			throw ::std::runtime_error(xxx() << "the 1-based row(=" << i  + 1 << ") is out of range(=" << grid.size() << ")");

		current_row_i = i;
	}

	/**
		@desc using current internal semantic row "read pointer", returns data in a given column
		@param "i" is 0-based
	*/
	template <typename T>
	T 
	column(size_paramtype x)
	{
		assert(assert_common() == true);
		if (x >= grid[current_row_i].size())
				throw ::std::runtime_error(xxx() << "in the 1-based row: [" << current_row_i  + 1 << "] the 1-based column: [" << x + 1 << "] (alpha: [" << Base::utils().alpha_tofrom_numeric(x) << "]) is out of range. Total rows: [" << grid.size() << "] and columns if that row: [" << grid[current_row_i].size() << ']');

#if 0
		if (grid[current_row_i][x].empty() == true)
			throw ::std::runtime_error(xxx() << "in the 1-based row: [" << current_row_i  + 1 << "] the 1-based column: [" << x + 1 << "] (alpha: [" << Base::utils().alpha_tofrom_numeric(x) << "]) is rather empty (currently not allowed).");
#endif

		return censoc::lexicast<T>(grid[current_row_i][x]);
	}

	/**
		@desc comparator for a given column in 2 rows
		@past returns if 'lhs' is < than 'rhs'
	*/
	template <typename T>
	censoc::compared
	compare(size_paramtype lhs, size_paramtype rhs, size_paramtype i) 
	{
		return compare_sub<T>(censoc::lexicast<T>(grid[lhs][i]), censoc::lexicast<T>(grid[rhs][i]));
	}

	size_type
	rows() const
	{
		return grid.size();
	}

private:
	bool static
	isnewline(char c)
	{
		switch (c) {
			case 0x0a :
			case 0x0c :
			case 0x0d :
			return true;
		}
		return false;
	}

	template <typename T>
	censoc::compared
	compare_sub(T const & lhs, T const & rhs) const
	{
		return lhs < rhs ? lt : lhs > rhs ? gt : eq;
	}

	bool
	assert_common() const
	{
		// TODO
		return true;
	}


};

}}
#endif
