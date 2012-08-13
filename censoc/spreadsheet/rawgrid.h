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

#include <assert.h>

#include <ctype.h>

extern "C" {
// TODO -- LATER WILL JUST WRITE OWN CODE, AS THIS LIB IS NOT THAT GOOD (IT SHOULD DO FOR THE TIME-BEING)
#include <libxls/xls.h>
}

#include <stdexcept>
#include <string>

#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/compare.h>


#include "util.h"

#ifndef CENSOC_SPREADSHEET_RAWGRID_H
#define CENSOC_SPREADSHEET_RAWGRID_H


namespace censoc { namespace spreadsheet {


template <typename N, typename xxx, typename Base>
class rawgrid : public Base {

	typedef typename censoc::param<N>::type n_param;

	//typedef typename Types::N N;

	// for verbosity
	//typedef typename Types::xxx xxx;


public:

	struct autobook {
		::xlsWorkBook * book;
		autobook(char const * path, char const * encoding)
		: book(
		// the lib is not "nice" wrt consts
		::xls_open(const_cast<char *>(path), const_cast<char *>(encoding))
		) {
			if (book == NULL)
				throw ::std::runtime_error(xxx() << "Failed to open the xls file: [" << path << "]");
		}
		~autobook()
		{
			::xls_close(book);
		}
		operator ::xlsWorkBook * ()
		{
			return book;
		}
		operator ::xlsWorkBook const * () const
		{
			return book;
		}
	};
	autobook book;

	::xlsWorkSheet * sheet;
	::st_row::st_row_data * row;
public:

	/**
		@param 'path' location of xls file
		@param 'encoding' "ASCII", "KOI8-R", etc.
		@param 'sheet_i' is 0-based
	*/
	rawgrid(char const * path, char const * encoding, n_param sheet_i, typename Base::ctor const & base_ctor)
	: Base(base_ctor), book(path, encoding), row(NULL) {
		sheet = ::xls_getWorkSheet(book, sheet_i);
		if (sheet == NULL)
			throw ::std::runtime_error(xxx() << "Failed to get worksheet [" << sheet_i << "] from the xls file: [" << path << "]");
		::xls_parseWorkSheet(sheet);
	}

	/**
		@desc positions the internal semantic "read pointer" to a given row
		@param "i" is 0-based
	*/
	void
	torow(n_param i)
	{
		assert(book != NULL);
		assert(sheet != NULL);

		if (i > sheet->rows.lastrow)
			throw ::std::runtime_error(xxx() << "the 1-based row: [" << i + 1 << "] is out of range: [" << sheet->rows.lastrow + 1 << "]");

		row = &sheet->rows.row[i];
		if (row == NULL)
			throw ::std::runtime_error(xxx() << "Failed to get the 1-based row: [" << i + 1 << "]");
	}

	/**
		@desc using current internal semantic row "read pointer", returns data in a given column
		@pre column data must be present (i.e. non-blank, etc.), and it must not be hidden, and column-span together with row-span for the cell must be 1 (i.e. keeping it simple)
		@param "i" is 0-based
		@todo CURRENTLY libxsl which only works in Little-endian orientation, if/when doing a more robust calculations (or fixing the lib) -- will need to determine the xls standard (how the actual document is storing the code -- currently presuming network order, but could be rather very wrong).
	*/
	template <typename T>
	T 
	column(n_param i) const
	{
		assert(assert_common() == true);

		if (i > sheet->rows.lastcol)
			throw ::std::runtime_error(xxx() << "in the 1-based row: [" << row->index  + 1 << "] the 1-based column: [" << i + 1 << "] (alpha: [" << Base::utils().alpha_tofrom_numeric(i) << "]) is out of range: [" << sheet->rows.lastcol + 1 << "]");

		if (row->fcell)
			throw ::std::runtime_error(xxx() << "in the 1-based row: [" << row->index  + 1 << "] the grid does not begin on the first column (not allowed). first cell: [" << row->fcell + 1 << "]");

		if (row->lcell != sheet->rows.lastcol)
			throw ::std::runtime_error(xxx() << "in the 1-based row: [" << row->index  + 1 << "] the grid does not end on the last column (not allowed). last cell: [" << row->lcell + 1 << "] last column: [" << sheet->rows.lastcol + 1 << "]");

		if (row->cells.cell[i].colspan)
			throw ::std::runtime_error(xxx() << "in the 1-based row: [" << row->index + 1 << "] the 1-based column: [" << i + 1 << "] (alpha: [" << Base::utils().alpha_tofrom_numeric(i) << "]) has incorrect (non-1) column-span: [" << row->cells.cell[i].colspan + 1 << "]");

		if (row->cells.cell[i].rowspan)
			throw ::std::runtime_error(xxx() << "in the 1-based row: [" << row->index + 1 << "] the 1-based column: [" << i + 1 << "] (alpha: [" << Base::utils().alpha_tofrom_numeric(i) << "]) has incorrect (non-1) row-span: [" << row->cells.cell[i].rowspan + 1 << "]");

		if (row->cells.cell[i].ishiden)
			throw ::std::runtime_error(xxx() << "in the 1-based row: [" << row->index + 1 << "] the 1-based column: [" << i + 1 << "] (alpha: [" << Base::utils().alpha_tofrom_numeric(i) << "]) is hidden (not allowed)");

		T rv;
		uint16_t const id(row->cells.cell[i].id);
		try {
			if (id == 0x27E || id == 0x203 || id == 0xBD || id == 0x06) // TODO the last one should probably be taken out!
				rv = censoc::lexicast<T>(row->cells.cell[i].d);
			else if (id == 0xFD)
				rv = censoc::lexicast<T>(row->cells.cell[i].l);
			else 
				rv = censoc::lexicast<T>(const_cast<char const *>(row->cells.cell[i].str));
		} catch (::std::exception const & e) {
			 throw ::std::runtime_error(xxx() << "Type extraction/cast in the 1-based row: [" << row->index + 1 << "] the 1-based column: [" << i + 1 << "] (alpha: [" << Base::utils().alpha_tofrom_numeric(i) << "]) has failed with: [" << e.what() << "]; cell's typecode: [0x" << ::std::hex << id << "]");
		}
		return rv;
	}

	/**
		@desc comparator for a given column in 2 rows
		@past returns if 'lhs' is < than 'rhs'
	*/
	template <typename T>
	censoc::compared
	compare(n_param lhs, n_param rhs, n_param i) const
	{
		assert(book != NULL);
		assert(sheet != NULL);

		::st_row::st_row_data const & row_lhs(sheet->rows.row[lhs]);
		::st_row::st_row_data const & row_rhs(sheet->rows.row[rhs]);
		uint16_t const id_lhs(row_lhs.cells.cell[i].id);
		uint16_t const id_rhs(row_rhs.cells.cell[i].id);
		if (id_lhs != id_rhs)
			 throw ::std::runtime_error(xxx() << "columns do not share common type in comparasion. row: [" << lhs + 1 << "] column's type: [0x" << ::std::hex << id_lhs << ::std::dec << "] vs row: [" << rhs + 1 << "] column's type: [0x" << ::std::hex << id_rhs << ::std::dec << "] column's index: [" << i + 1 << "(" << Base::utils().alpha_tofrom_numeric(i) << ")");

		// TODO -- refactor for commonality with 'column' method
		if (id_lhs == 0x27E || id_lhs == 0x203 || id_lhs == 0xBD || id_lhs == 0x06) 
			return compare_sub<T>(censoc::lexicast<T>(row_lhs.cells.cell[i].d), censoc::lexicast<T>(row_rhs.cells.cell[i].d));
		else if (id_lhs == 0xFD)
			return compare_sub<T>(censoc::lexicast<T>(row_lhs.cells.cell[i].l), censoc::lexicast<T>(row_rhs.cells.cell[i].l));
		else
			return compare_sub<T>(censoc::lexicast<T>(row_lhs.cells.cell[i].str), censoc::lexicast<T>(row_rhs.cells.cell[i].str));
	}

private:
	template <typename T>
	censoc::compared
	compare_sub(T const & lhs, T const & rhs) const
	{
		return lhs < rhs ? lt : lhs > rhs ? gt : eq;
	}

	bool
	assert_common() const
	{
		if (book == NULL)
			return false;
		if (sheet == NULL)
			return false;
		if (row == NULL)
			return false;
		return true;
	}
public:
	/**
		@note not so fast, just for informative purposes. If needed to be used often -- cache in the client code.
	 */
	N
	rows() const
	{
		assert(book != NULL);
		assert(sheet != NULL);

		return sheet->rows.lastrow + 1;
	}

	/**
		@note not so fast, just for informative purposes. If needed to be used often -- cache in the client code.
	 */
	N
	columns() const
	{
		assert(book != NULL);
		assert(sheet != NULL);

		// strange, on my xls sample file, quite in contradiction to the test.c code in libxls, the total columns is NOT inclusive of 'lastcol' (as 'lastrow' appears to be).
		return sheet->rows.lastcol;
	}


};

}}
#endif
