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

#include <string>

#include <censoc/lexicast.h>

#include <censoc/spreadsheet/util.h>
//#include <censoc/spreadsheet/rawgrid.h>
#include <censoc/spreadsheet/csv.h>
#include <censoc/compare.h>
#include <censoc/stl_container_utils.h>

#include <netcpu/message.h>

#include "composite_matrix.h"
#include "message/meta.h"
#include "message/bulk.h"

#ifndef CENSOC_NETCPU_DATASET_1_H
#define CENSOC_NETCPU_DATASET_1_H

namespace censoc { namespace netcpu { namespace dataset_1 { 

template <typename N, typename F>
struct composite_matrix_loader {

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype;

	typedef typename netcpu::message::typepair<N>::wire size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	/*
	Rethinking the whole row-major layout. Mainly because the composite matrix (z ~ x ~ y) appears to be used only '1 row at a time' anyway -- thus even a row-based matrix should not cause performance issues when it is only used to generate 'row views'.
	*/
	//typedef ::Eigen::Matrix<float_type, ::Eigen::Dynamic, ::Eigen::Dynamic> matrix_columnmajor_type;
	//typedef ::Eigen::Matrix<int8_t, ::Eigen::Dynamic, ::Eigen::Dynamic, ::Eigen::RowMajor> int_matrix_rowmajor_type;
	typedef ::Eigen::Matrix<float_type, ::Eigen::Dynamic, 1> vector_column;
	typedef ::Eigen::Matrix<float_type, 1, ::Eigen::Dynamic> vector_row;

	typedef censoc::lexicast< ::std::string> xxx;

	typedef censoc::spreadsheet::util<size_type, xxx> ute_type;
	typedef typename censoc::param<censoc::spreadsheet::util<size_type, xxx> >::type ute_paramtype;

	struct grid_base {
		struct ctor {
			ctor (ute_type & ute)
			: ute(ute) {
			}
			ute_type & ute;
		};
		grid_base(ctor const & c)
		: ute(c.ute) {
		}
		ute_type & ute;
		ute_type & 
		utils() const
		{
			return ute;
		}
	};

	//typedef censoc::spreadsheet::rawgrid<size_type, xxx, grid_base> rawgrid_type;
	typedef censoc::spreadsheet::csv<size_type, xxx, grid_base> rawgrid_type;

	template <typename T>
	void 
	cli_parse_column_i(char const firstc, T const & str, ute_paramtype ute, size_type & x)
	{
		if (::isalpha(firstc)) 
			x = ute.alpha_tofrom_numeric(str);
		else 
			x = censoc::lexicast<size_type>(str) - 1;
	}

	void
	cli_parse_columns(::std::list<size_type> & columns, ::std::set<size_type> & unique, char const * arg, char const * const text_id, ute_type & ute)
	{
		assert(arg != NULL);

		bool next_is_range(false);
		char const * tmp = arg;
		size_t tmp_size;
		while ((tmp_size = ::strcspn(tmp, ",:"))) {

			::std::string const str(tmp, tmp_size);

			size_type index;
			cli_parse_column_i(*tmp, str, ute, index);

			if (next_is_range == true) {
				assert(columns.size());

				if (index <= columns.back())
					throw ::std::runtime_error(xxx() << "incorrect column range ending with: [" << str << "] for " << text_id << ": [" << arg << "]");

				for (size_type i(columns.back() + 1); i <= index; ++i) {
					if (unique.find(i) != unique.end())
						throw ::std::runtime_error(xxx() << "cannot imply duplicate column name: [" << ute.alpha_tofrom_numeric(i) << "] no: [" << i + 1 << "] for " << text_id << ": [" << arg << "]");
					unique.insert(i);
					columns.push_back(i);
				}

			} else  {
				if (unique.find(index) != unique.end())
					throw ::std::runtime_error(xxx() << "cannot imply duplicate column name: [" << str << "] for " << text_id << ": [" << arg << "]");
				unique.insert(index);
				columns.push_back(index);
			}

			switch (tmp[tmp_size]) {
				case ':' :
					if (next_is_range == true)
						throw ::std::runtime_error(xxx() << "syntax error, cannot specify range(s) like this: [" << arg << "] for " << text_id);
					next_is_range = true;
				break;
				case '\0' :
					return;
				default :
					next_is_range = false;
			}
			tmp += tmp_size + 1;
		}
	}

	void
	cli_check_matrix_elements(::std::list< ::std::list<size_type> > const & attributes, ::std::string const & text_id, ute_type & ute)
	{
		if (attributes.empty() == true)
			throw ::std::runtime_error(xxx() << "must supply some attributes for the " << text_id);

		censoc::llog() << "Elements in " << text_id << ':';
		size_type total_count(0);
		for (typename ::std::list< ::std::list<size_type> >::const_iterator i(attributes.begin()); i != attributes.end(); ++i) {
			::std::list<size_type> const & columns(*i);
			for (typename ::std::list<size_type>::const_iterator j(columns.begin()); j != columns.end(); ++j, ++total_count) 
				censoc::llog() << ' ' << *j + 1 << '(' << ute.alpha_tofrom_numeric(*j) << ')';
			censoc::llog() << "; ";
		}
		censoc::llog() << "Total of: [" << total_count << "].\n";
	}

	/**
		@todo -- deprecate (only used in 1 place anyway)
		*/
	template <typename M>
	void
	row_append_attributes(M & matrix, ::std::list< ::std::list<size_type> > & attributes, size_paramtype r, size_paramtype c_offset, size_type & c, rawgrid_type & grid, size_paramtype alternatives)
	{
		for (typename ::std::list< ::std::list<size_type> >::const_iterator i(attributes.begin()); i != attributes.end(); ++i) {
			::std::list<size_type> const & columns(*i);
			for (typename ::std::list<size_type>::const_iterator j(columns.begin()); j != columns.end(); ++j) {
				assert(grid.template column<F>(*j) == grid.template column<int>(*j));
				matrix(r, c_offset + (c += alternatives)) = grid.template column<int>(*j);
			}
		}
	}

	struct delby_metadata {
		delby_metadata(size_paramtype i, censoc::compared const op, float_paramtype value)
		: i(i), op(op), value(value) {
		}
		size_type const i;
		censoc::compared const op;
		float_type const value;
	};

	struct grid_sorter {
		::std::list<size_type> const & sortby;
		rawgrid_type & grid;
		grid_sorter(::std::list<size_type> const & sortby, rawgrid_type & grid)
		: sortby(sortby), grid(grid) {
		}
		bool 
		operator () (size_type lhs, size_type rhs) const
		{
			for (typename ::std::list<size_type>::const_iterator i(sortby.begin()); i != sortby.end(); ++i) {
				censoc::compared const tmp(grid.template compare<size_type>(lhs, rhs, *i));
				if (tmp == lt)
					return true;
				else if (tmp == gt) 
					return false;
			}
			return false;
		}
	};

	// CLI -- quick-n-dirty
	::std::string filepath; 
	//size_type sheet_i;
	size_type respondent_i;
	size_type row_start_i;
	size_type best_i;

	::std::list< ::std::list<size_type> > x_elements; 

	ute_type ute;

	::std::list<size_type> sortby;
	::std::set<size_type> sortby_unique;

	// TODO refactor to use fastsize 
	::std::list<size_type> sorted;
	::std::list<delby_metadata> delby;
	::std::vector<size_type> respondents_choice_sets;
	::std::set<size_type> x_unique;

	composite_matrix_loader()
	: 
		//sheet_i(-1), 
		respondent_i(-1), row_start_i(-1), best_i(-1) {
	}

	bool
	parse_arg(char const * const name, char const * const value, dataset_1::message::meta<N> & meta_msg, dataset_1::message::bulk & bulk_msg) 
	{
		(void)bulk_msg; // no 'unused param' warning from the compiler thanks... ('bulk_msg' is passed as a matter of policy for the time being)
		if (!::strcmp(name, "--file")) {
			filepath = value;
			censoc::llog() << "Loading file: " << filepath << '\n';
		//} else if (!::strcmp(name, "--sheet")) {
		//	sheet_i = censoc::lexicast<size_type>(value);
		//	censoc::llog() << "Loading sheet: " << sheet_i << '\n';
		//	--sheet_i; // user talks in 1-based terms
		} else if (!::strcmp(name, "--x")) {
			x_elements.push_back(::std::list<size_type>());
			cli_parse_columns(x_elements.back(), x_unique, value, "x submatrix", ute);
		} else if (!::strcmp(name, "--sort")) {
			sortby.clear();
			cli_parse_columns(sortby, sortby_unique, value, "sortby list of columns", ute);
		} else if (!::strcmp(name, "--resp")) {
			cli_parse_column_i(*value, value, ute, respondent_i);
			// quick-n-dirty
			censoc::llog() << "Respondent id column: [" << ute.alpha_tofrom_numeric(respondent_i) << "] no: [" << respondent_i + 1 << "]\n";
		} else if (!::strcmp(name, "--fromrow")) {
			row_start_i = censoc::lexicast<size_type>(value);
			censoc::llog() << "Numeric data starts from row: [" << row_start_i << "]\n";
			--row_start_i;
		} else if (!::strcmp(name, "--alts")) {
			meta_msg.alternatives(censoc::lexicast<size_type>(value));
			censoc::llog() << "Alternatives in a choice set: [" << meta_msg.alternatives() << "]\n";
		} else if (!::strcmp(name, "--best")) { 
			cli_parse_column_i(*value, value, ute, best_i);
			// quick-n-dirty
			censoc::llog() << "Best column: [" << ute.alpha_tofrom_numeric(best_i) << "] no: [" << best_i + 1 << "]\n";
		} else if (!::strcmp(name, "--del")) {
			char const * rv(::strtok(const_cast<char*>(value), ":"));
			if (rv == NULL)
				throw ::std::runtime_error(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			size_type delby_i;
			cli_parse_column_i(*rv, rv, ute, delby_i);

			rv = ::strtok(NULL, ":");
			if (rv == NULL)
				throw ::std::runtime_error(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			::std::string delby_op_str(rv);
			censoc::compared delby_op;
			if (delby_op_str == "lt") {
				delby_op = censoc::lt;
			} else if (delby_op_str == "le") {
				delby_op = censoc::le;
			} else if (delby_op_str == "eq") {
				delby_op = censoc::eq;
			} else if (delby_op_str == "ge") {
				delby_op = censoc::ge;
			} else if (delby_op_str == "gt") {
				delby_op = censoc::gt;
			} else
				throw ::std::runtime_error(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			rv = ::strtok(NULL, ":");
			if (rv == NULL)
				throw ::std::runtime_error(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			float_type const delby_value = censoc::lexicast<float_type>(rv);

			censoc::llog() << "Will delete rows based on values from column: [" << ute.alpha_tofrom_numeric(delby_i) << "] no: [" << delby_i + 1 << "]. The comparison for deletion criteria: [" << delby_op_str << "]. The scalar value: [" << delby_value << "]\n";

			delby.push_back(delby_metadata(delby_i, delby_op, delby_value));
		} else 
			return false;
		return true;
	}

	void
	verify_args(dataset_1::message::meta<N> & meta_msg, dataset_1::message::bulk & bulk_msg)
	{
		cli_check_matrix_elements(x_elements, "x submatrix", ute);

		if (filepath.empty() == true)
			throw ::std::runtime_error("must supply the filepath for the excel file");

		//if (sheet_i == static_cast<size_type>(-1))
		//	throw ::std::runtime_error("must supply which sheet in the excel file contains relevant data");

		if (respondent_i == static_cast<size_type>(-1))
			throw ::std::runtime_error("must supply colmun for the respondent's id");

		if (best_i == static_cast<size_type>(-1))
			throw ::std::runtime_error("must supply colmun for the best (whatever that means :-)");

		if (row_start_i == static_cast<size_type>(-1))
			throw ::std::runtime_error("must supply starting row for the numeric data");

		if (meta_msg.alternatives() == static_cast<typename netcpu::message::typepair<N>::wire>(-1))
			throw ::std::runtime_error("must supply number of alternatives in a choice set");

		if (x_unique.empty() == true)
			throw ::std::runtime_error("Must supply at least one --x option");

		meta_msg.x_size(x_unique.size());

		censoc::llog() << "Availability column in this model is ALWAYS presumed to be a ficticious column containing only values of '1'\n";

		if (sortby.empty() == true)
			throw ::std::runtime_error("must supply sortby columns");

		censoc::llog() << "Sorting by columns: ";
		for (typename ::std::list<size_type>::iterator i(sortby.begin()); i != sortby.end(); ++i) 
			censoc::llog() << *i + 1 << '(' << ute.alpha_tofrom_numeric(*i) << ") ";
		censoc::llog() << '\n';

		// load the raw grid
		// xls_debug = 1;
		//rawgrid_type grid_obj(filepath.c_str(), "ASCII", sheet_i, typename grid_base::ctor(ute));
		rawgrid_type grid_obj(filepath.c_str(), typename grid_base::ctor(ute));

		if (grid_obj.rows() <= row_start_i)
			throw ::std::runtime_error(xxx() << "not enough rows in spreadsheet: [" << grid_obj.rows() << "] given the starting row: [" << row_start_i + 1 << "]");

		censoc::llog() << "Total rows in the spreadsheet: [" << grid_obj.rows() << "]\n";

		// sorting index
		size_type excluded(0);
		for (size_type i(row_start_i); i != grid_obj.rows(); ++i) {

			// exclude rows here 
			bool exclude(false);
			grid_obj.torow(i);
			for (typename ::std::list<delby_metadata>::iterator d(delby.begin()); d != delby.end() && exclude == false; ++d) {
				delby_metadata const & meta(*d);

				float_type const value(grid_obj.template column<float_type>(meta.i));

				if (meta.op == censoc::lt) {
					if (value < meta.value) 
						exclude = true;
				} else if (meta.op == censoc::le) {
					if (value <= meta.value)
						exclude = true;
				} else if (meta.op == censoc::eq) {
					if (value == meta.value)
						exclude = true;
				} else if (meta.op == censoc::ge) {
					if (value >= meta.value)
						exclude = true;
				} else if (meta.op == censoc::gt) {
					if (value > meta.value)
						exclude = true;
				} 
			}

			if (exclude == false)
				sorted.push_back(i);
			else
				++excluded;
		}
		if (sorted.empty() == true)
			throw ::std::runtime_error("no data is left after culling the rows");

		censoc::llog() << "Total deleted rows: [" << excluded << "], remainin rows (before turning alternatives into columns): [" << sorted.size() << "]\n";

		sorted.sort(grid_sorter(sortby, grid_obj));

		meta_msg.matrix_composite_rows(sorted.size() / meta_msg.alternatives());
		meta_msg.matrix_composite_columns(meta_msg.x_size() * meta_msg.alternatives() + 1); // 'alternatives + 1' is for y vector horizontal concatenation

		censoc::llog() << "Composite matrix (z submatrix, x submatrix, y subvector) yields -- rows: [" << meta_msg.matrix_composite_rows() << "], columns: [" << meta_msg.matrix_composite_columns() << "]\n";

		bulk_msg.matrix_composite.resize(meta_msg.matrix_composite_rows() * meta_msg.matrix_composite_columns());
		//::Eigen::Map<int_matrix_rowmajor_type> matrix_composite(bulk_msg.matrix_composite.data(), meta_msg.matrix_composite_rows(), meta_msg.matrix_composite_columns());
		composite_matrix_map<size_type, uint8_t> matrix_composite(bulk_msg.matrix_composite.data(), meta_msg.matrix_composite_rows(), meta_msg.matrix_composite_columns());

		//matrix_columnmajor_type vector_y_tmp_m(static_cast<int>(meta_msg.matrix_composite_rows()), meta_msg.alternatives());

		// here (in the following loop) also perform the expansion of alternatives into columns
		typename ::std::list<size_type>::const_iterator sorted_i(sorted.begin()); 
		assert(meta_msg.matrix_composite_rows());
		assert(sorted_i != sorted.end());

		grid_obj.torow(*sorted_i);
		size_type respondent_id_tmp(grid_obj.template column<size_type>(respondent_i));
		respondents_choice_sets.push_back(0);

		for (size_type r(0); r != meta_msg.matrix_composite_rows(); ++r) { 
			matrix_composite(r, matrix_composite.cols() - 1) = -1;
			for (size_type i(0); i != meta_msg.alternatives(); ++i) {
				grid_obj.torow(*sorted_i);

				size_type const respondent_id_tmp_new(grid_obj.template column<size_type>(respondent_i));
				if (respondent_id_tmp != respondent_id_tmp_new) {
					if (respondents_choice_sets.back() % meta_msg.alternatives())
						throw ::std::runtime_error(xxx() << "Can't calculate exact number of choice-sets per respondent as the number is not completely divisible by alternatives. Respondent: [" << respondent_id_tmp << "], total responses: [" << respondents_choice_sets.back() << "]");
					respondent_id_tmp = respondent_id_tmp_new;
					respondents_choice_sets.back() /= meta_msg.alternatives();
					respondents_choice_sets.push_back(1);
				} else 
					++respondents_choice_sets.back();

				size_type c_composite(-static_cast<size_type>(meta_msg.alternatives()));
				row_append_attributes(matrix_composite, x_elements, r, i, c_composite, grid_obj, meta_msg.alternatives());

				//vector_y_tmp_m(r, i) = grid_obj.template column<float_type>(best_i);
				assert(grid_obj.template column<int>(best_i) == grid_obj.template column<float_type>(best_i));
				int const best_i_value(grid_obj.template column<int>(best_i));
				assert(best_i_value == 0 || best_i_value == 1);
				if (best_i_value) {
					if (matrix_composite(r, matrix_composite.cols() - 1) != static_cast<uint8_t>(-1))
						throw ::std::runtime_error("design is not excepted to have multiple 'choices' present for a given choiceset -- only one alternative can be indicated as chosen");
					matrix_composite(r, matrix_composite.cols() - 1) = i;
				}


#if 1
				// currently not emulating gauss in wraparound mode when reshaping... see below (else) for commented-out code if emulating gauss...
				if (sorted_i++ == sorted.end()) 
					throw ::std::runtime_error("design is not excepted to hit the case of gauss-like behavior of re-reading matrix from start during the 'reshaping' process");
#else
				// emulate gauss (TODO -- may not needed it as 'matrix_composite_rows' is already INTEGRALLY divided by 'altrenatives')
				// warnning -- if enabling -- must recode respondents-counting code!!!
				if (++sorted_i == sorted.end())
					sorted_i = sorted.begin();
#endif
			}
			if (matrix_composite(r, matrix_composite.cols() - 1) == static_cast<uint8_t>(-1))
				throw ::std::runtime_error("validity-check failed: after sorting, the y vector value exceeds number of alternatives which would blow the array-index access");

		}

		if (respondents_choice_sets.back() % meta_msg.alternatives())
			throw ::std::runtime_error(xxx() << "Can't calculate exact number of choice-sets per respondent as the number is not completely divisible by alternatives. Respondent: [" << respondent_id_tmp << "], total responses: [" << respondents_choice_sets.back() << "]");
		respondents_choice_sets.back() /= meta_msg.alternatives();

		censoc::llog() << "Individual respondents: [" << respondents_choice_sets.size() << "]\n";

		meta_msg.respondents_choice_sets.resize(respondents_choice_sets.size());
		for (size_type i(0); i != respondents_choice_sets.size(); ++i)
			meta_msg.respondents_choice_sets(i, respondents_choice_sets[i]);

		/*
		// debug for choice-sets
		for (::std::vector<size_type>::iterator i(respondents_choice_sets.begin()); i != respondents_choice_sets.end(); ++i) {
			censoc::llog() << *i << ", ";
		}
		*/

#if 0
		vector_column vector_y_tmp_v(meta_msg.alternatives());
		for (size_type i(0); i != meta_msg.alternatives(); ++i) // TODO -- see if there is a 'seqa' like 'populate' method in Eigen...
			vector_y_tmp_v[i] = i + 1;

		// calculate y vec ... 
		matrix_composite.col(matrix_composite.cols() - 1) = (vector_y_tmp_m * vector_y_tmp_v).cwise() - 1; // - 1 is because yvec's values are used for accessing another matrix columns -- so are 0 based
		if ((matrix_composite.col(matrix_composite.cols() - 1).cwise() > meta_msg.alternatives() - 1).any() == true || (matrix_composite.col(matrix_composite.cols() - 1).cwise() < 0).any() == true)
			throw ::std::runtime_error("validity-check failed: after sorting, the y vector value exceeds number of alternatives which would blow the array-index access");
#endif


		//censoc::llog() << "Composite matrix:\n" << matrix_composite << '\n';


	}
};

}}}

#endif
