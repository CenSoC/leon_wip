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

#include <boost/tokenizer.hpp>

// does not currently (boost v.1_52) work with -fno-rtti switch (calls typeid), but I really dont wanna enable the damn thing, so will currently write an alternative code
// #include <boost/iostreams/device/array.hpp>
// #include <boost/iostreams/stream.hpp>

#include <censoc/lexicast.h>

#include <censoc/spreadsheet/util.h>
//#include <censoc/spreadsheet/rawgrid.h>
#include <censoc/spreadsheet/csv.h>
#include <censoc/compare.h>
#include <censoc/stl_container_utils.h>

#include <netcpu/message.h>

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
	cli_parse_columns(::std::list<size_type> & columns, ::std::set<size_type> & unique, ::std::string const & arg, char const * const text_id, ute_type & ute)
	{
		assert(arg.empty() == false);

		bool next_is_range(false);
		char const * tmp = arg.c_str();
		size_t tmp_size;
		while ((tmp_size = ::strcspn(tmp, ",:"))) {

			::std::string const str(tmp, tmp_size);

			size_type index;
			cli_parse_column_i(*tmp, str, ute, index);

			if (next_is_range == true) {
				assert(columns.size());

				if (index <= columns.back())
					throw netcpu::message::exception(xxx() << "incorrect column range ending with: [" << str << "] for " << text_id << ": [" << arg << "]");

				for (size_type i(columns.back() + 1); i <= index; ++i) {
					if (unique.find(i) != unique.end())
						throw netcpu::message::exception(xxx() << "cannot imply duplicate column name: [" << ute.alpha_tofrom_numeric(i) << "] no: [" << i + 1 << "] for " << text_id << ": [" << arg << "]");
					unique.insert(i);
					columns.push_back(i);
				}

			} else  {
				if (unique.find(index) != unique.end())
					throw netcpu::message::exception(xxx() << "cannot imply duplicate column name: [" << str << "] for " << text_id << ": [" << arg << "]");
				unique.insert(index);
				columns.push_back(index);
			}

			switch (tmp[tmp_size]) {
				case ':' :
					if (next_is_range == true)
						throw netcpu::message::exception(xxx() << "syntax error, cannot specify range(s) like this: [" << arg << "] for " << text_id);
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
			throw netcpu::message::exception(xxx() << "must supply some attributes for the " << text_id);

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
	censoc::vector<char, size_type> filedata; // not string because need a 'data()' pointer to non-const array as per requirement of a workaround in 'setg' call
	//size_type sheet_i;
	size_type respondent_i;
	size_type choice_set_i;
	size_type alternative_i;
	size_type row_start_i;
	size_type best_i;

	::std::list< ::std::list<size_type> > x_elements; 

	ute_type ute;

	::std::list<size_type> sortby;
	::std::set<size_type> sortby_unique;

	// TODO refactor to use fastsize 
	::std::list<size_type> sorted;
	::std::list<delby_metadata> delby;
	censoc::stl::fastsize< ::std::list<size_type>, size_type> respondents_choice_sets;
	::std::set<size_type> x_unique;

	composite_matrix_loader()
	: 
		//sheet_i(-1), 
		respondent_i(-1), choice_set_i(-1), alternative_i(-1), row_start_i(-1), best_i(-1) {
	}

	bool
	parse_arg(::std::string const & name, ::std::string const & value, dataset_1::message::meta<N> &, dataset_1::message::bulk<N> & /* bulk_msg */ ) 
	{
		// (void)bulk_msg; // no 'unused param' warning from the compiler thanks... ('bulk_msg' is passed as a matter of policy for the time being)
		if (name == "--filepath") {
			filepath = value;
			censoc::llog() << "Loading file: " << filepath << '\n';
		//} else if (!::strcmp(name, "--sheet")) {
		//	sheet_i = censoc::lexicast<size_type>(value);
		//	censoc::llog() << "Loading sheet: " << sheet_i << '\n';
		//	--sheet_i; // user talks in 1-based terms
		} else if (name == "--filedata") {
			filedata.reset(value.size());
			::memcpy(filedata.data(),  value.data(), value.size());
			censoc::llog() << "received actual csv data: " << filedata.size() << " bytes\n";
		} else if (name == "--x") {
			x_elements.push_back(::std::list<size_type>());
			cli_parse_columns(x_elements.back(), x_unique, value, "x submatrix", ute);
		} else if (name == "--sort") {
			sortby.clear();
			cli_parse_columns(sortby, sortby_unique, value, "sortby list of columns", ute);
		} else if (name == "--respondent") {
			cli_parse_column_i(value[0], value, ute, respondent_i);
			// quick-n-dirty
			censoc::llog() << "Respondent id column: [" << ute.alpha_tofrom_numeric(respondent_i) << "] no: [" << respondent_i + 1 << "]\n";
		} else if (name == "--choice_set") {
			cli_parse_column_i(value[0], value, ute, choice_set_i);
			// quick-n-dirty
			censoc::llog() << "Choice set id column: [" << ute.alpha_tofrom_numeric(choice_set_i) << "] no: [" << choice_set_i + 1 << "]\n";
		} else if (name == "--alternative") {
			cli_parse_column_i(value[0], value, ute, alternative_i);
			// quick-n-dirty
			censoc::llog() << "Alternative id column: [" << ute.alpha_tofrom_numeric(alternative_i) << "] no: [" << alternative_i + 1 << "]\n";
		} else if (name == "--fromrow") {
			row_start_i = censoc::lexicast<size_type>(value);
			censoc::llog() << "Numeric data starts from row: [" << row_start_i << "]\n";
			--row_start_i;
		} else if (name == "--best") { 
			cli_parse_column_i(value[0], value, ute, best_i);
			// quick-n-dirty
			censoc::llog() << "Best column: [" << ute.alpha_tofrom_numeric(best_i) << "] no: [" << best_i + 1 << "]\n";
		} else if (name == "--del") {

			typedef ::boost::tokenizer< ::boost::char_separator<char> > tokenizer_type;
			tokenizer_type tokens(value, ::boost::char_separator<char>(":"));
			tokenizer_type::const_iterator token_i(tokens.begin()); 

			if (token_i == tokens.end())
				throw netcpu::message::exception(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			size_type delby_i;
			if ((*token_i)[0] != '*')
				cli_parse_column_i((*token_i)[0], *token_i, ute, delby_i);
			else
				delby_i = -1;

			if (++token_i == tokens.end())
				throw netcpu::message::exception(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			censoc::compared delby_op;
			::std::string const delby_op_string(*token_i);
			if (delby_op_string == "lt") {
				delby_op = censoc::lt;
			} else if (delby_op_string == "le") {
				delby_op = censoc::le;
			} else if (delby_op_string == "eq") {
				delby_op = censoc::eq;
			} else if (delby_op_string == "ge") {
				delby_op = censoc::ge;
			} else if (delby_op_string == "gt") {
				delby_op = censoc::gt;
			} else
				throw netcpu::message::exception(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			if (++token_i == tokens.end())
				throw netcpu::message::exception(xxx() << "incorrect --del value: [" << value << "] need 'column:op:value'");

			float_type const delby_value = censoc::lexicast<float_type>(*token_i);

			if (delby_i != static_cast<size_type>(-1))
				censoc::llog() << "Will delete rows based on values from column: [" << ute.alpha_tofrom_numeric(delby_i) << "] no: [" << delby_i + 1 << "]. The comparison for deletion criteria: [" << delby_op_string << "]. The scalar value: [" << delby_value << "]\n";
			else
				censoc::llog() << "Will delete rows based on values from any of the attribute columns. The comparison for deletion criteria: [" << delby_op_string << "]. The scalar value: [" << delby_value << "]\n";

			delby.push_back(delby_metadata(delby_i, delby_op, delby_value));
		} else 
			return false;
		return true;
	}

private:
	bool
	is_excluded(censoc::compared const meta_op, float_type const meta_value, float_type const value)
	{
		if (meta_op == censoc::lt) {
			if (value < meta_value) 
				return true;
		} else if (meta_op == censoc::le) {
			if (value <= meta_value)
				return true;
		} else if (meta_op == censoc::eq) {
			if (value == meta_value)
				return true;
		} else if (meta_op == censoc::ge) {
			if (value >= meta_value)
				return true;
		} else if (meta_op == censoc::gt) {
			if (value > meta_value)
				return true;
		} 
		return false;
	}
	void
	verify_args_sub(rawgrid_type & grid_obj, dataset_1::message::meta<N> & meta_msg, dataset_1::message::bulk<N> & bulk_msg) 
	{
		//if (sheet_i == static_cast<size_type>(-1))
		//	throw netcpu::message::exception("must supply which sheet in the excel file contains relevant data");

		if (respondent_i == static_cast<size_type>(-1))
			throw netcpu::message::exception("must supply colmun for the respondent's id");

		if (choice_set_i == static_cast<size_type>(-1))
			throw netcpu::message::exception("must supply colmun for the choice_set id");

		if (alternative_i == static_cast<size_type>(-1))
			throw netcpu::message::exception("must supply colmun for the alternative id");

		if (best_i == static_cast<size_type>(-1))
			throw netcpu::message::exception("must supply colmun for the best (whatever that means :-)");

		if (row_start_i == static_cast<size_type>(-1))
			throw netcpu::message::exception("must supply starting row for the numeric data");

		if (x_unique.empty() == true)
			throw netcpu::message::exception("Must supply at least one --x option");

		meta_msg.x_size(x_unique.size());

		censoc::llog() << "Availability column in this model is ALWAYS presumed to be a ficticious column containing only values of '1'\n";

		if (sortby.empty() == false) {
			censoc::llog() << "Sorting by columns: ";
			for (typename ::std::list<size_type>::iterator i(sortby.begin()); i != sortby.end(); ++i) 
				censoc::llog() << *i + 1 << '(' << ute.alpha_tofrom_numeric(*i) << ") ";
			censoc::llog() << '\n';
		} else
			censoc::llog() << "Not performing any sorting/reordering of rows in the supplied dataset";

		if (grid_obj.rows() <= row_start_i)
			throw netcpu::message::exception(xxx() << "not enough rows in spreadsheet: [" << grid_obj.rows() << "] given the starting row: [" << row_start_i + 1 << "]");

		censoc::llog() << "Total rows in the spreadsheet: [" << grid_obj.rows() << "]\n";

		// load sorting index whilst culling any to-be-deleted rows
		size_type excluded(0);
		for (size_type i(row_start_i); i != grid_obj.rows(); ++i) {

			// exclude rows here 
			bool exclude(false);
			grid_obj.torow(i);
			for (typename ::std::list<delby_metadata>::iterator d(delby.begin()); d != delby.end() && exclude == false; ++d) {
				delby_metadata const & meta(*d);
				if (meta.i != static_cast<size_type>(-1))
					exclude = is_excluded(meta.op, meta.value, grid_obj.template column<float_type>(meta.i));
				else for (typename ::std::set<size_type>::const_iterator i(x_unique.begin()); i != x_unique.end() && exclude == false; ++i)
					exclude = is_excluded(meta.op, meta.value, grid_obj.template column<float_type>(*i));
			}

			if (exclude == false)
				sorted.push_back(i);
			else
				++excluded;
		}
		if (sorted.empty() == true)
			throw netcpu::message::exception("no data is left after culling the rows");

		censoc::llog() << "Total deleted rows: [" << excluded << "], remainin rows (before turning alternatives into columns): [" << sorted.size() << "]\n";

		if (sortby.empty() == false)
			sorted.sort(grid_sorter(sortby, grid_obj));

		censoc::stl::fastsize< ::std::list<uint8_t>, size_type> choice_sets_alternatives;
		uint8_t max_alternatives(0);
		size_type matrix_composite_elements_size(0);
		size_type old_respondent(-1);
		size_type old_choice_set(-1);
		size_type old_alternative(-1);
		for (typename ::std::list<size_type>::const_iterator sorted_i(sorted.begin()); sorted_i != sorted.end(); ++sorted_i) {
			grid_obj.torow(*sorted_i);
			size_type const respondent(grid_obj.template column<size_type>(respondent_i));
			size_type const choice_set(grid_obj.template column<size_type>(choice_set_i));
			size_type const alternative(grid_obj.template column<size_type>(alternative_i));

			unsigned stage;
			if (old_respondent != respondent)
				stage = 1;
			else if (old_choice_set != choice_set)
				stage = 2;
			else if (old_alternative != alternative)
				stage = 3;
			else
				throw netcpu::message::exception("design has duplicate rows w.r.t. tuple of {respondent, choice set, alternative} ");
			switch (stage) { // falling through on purpose...
				case 1:
					old_respondent = respondent;
					respondents_choice_sets.push_back(0);
				case 2:
					old_choice_set = choice_set;
					++respondents_choice_sets.back();
					choice_sets_alternatives.push_back(0);
					++matrix_composite_elements_size; // for y vector horizontal concatenation
				case 3:
					old_alternative =  alternative;
					if (++choice_sets_alternatives.back() > max_alternatives) {
						max_alternatives = choice_sets_alternatives.back();
						if (::boost::integer_traits<uint8_t>::const_max == max_alternatives)
							throw netcpu::message::exception(xxx("too many alternatives. any given choiceset must have less than ") << ::boost::integer_traits<uint8_t>::const_max << " alternatives.");
					}
					matrix_composite_elements_size += meta_msg.x_size();
			}
		}

		meta_msg.max_alternatives(max_alternatives);
		meta_msg.matrix_composite_elements(matrix_composite_elements_size);

#ifndef NDEBUG
		{
			unsigned elements_count(0);
			for (::std::list<uint8_t>::const_iterator i(choice_sets_alternatives.begin()); i != choice_sets_alternatives.end(); elements_count += (*i * meta_msg.x_size() + 1), ++i)
				assert(*i <= meta_msg.max_alternatives());
			assert(elements_count == meta_msg.matrix_composite_elements());
			unsigned choice_sets_count(0);
			for (typename ::std::list<size_type>::const_iterator i(respondents_choice_sets.begin()); i != respondents_choice_sets.end(); choice_sets_count += *i, ++i);
			assert(choice_sets_count == choice_sets_alternatives.size());
		}
#endif
	
		censoc::llog() << "Composite matrix (attributes/levels wich a choice cell) yields total elements: [" << matrix_composite_elements_size << "], max_alternatives: [" << max_alternatives << "], rows: [" << choice_sets_alternatives.size() << "Individual respondents: [" << respondents_choice_sets.size() << "]\n";


		bulk_msg.respondents_choice_sets.resize(respondents_choice_sets.size());
		size_type bulk_msg_respondent_i(0);
		for (typename ::std::list<size_type>::const_iterator i(respondents_choice_sets.begin()); i != respondents_choice_sets.end(); ++i)
			bulk_msg.respondents_choice_sets(bulk_msg_respondent_i++, *i);
		/*
		// debug for choice-sets
		for (::std::vector<size_type>::iterator i(respondents_choice_sets.begin()); i != respondents_choice_sets.end(); ++i) {
			censoc::llog() << *i << ", ";
		}
		*/


		// transposition of alternatives into columns
		typename ::std::list<size_type>::const_iterator sorted_i(sorted.begin()); 
		assert(sorted_i != sorted.end());

		bulk_msg.choice_sets_alternatives.resize(choice_sets_alternatives.size());
		bulk_msg.matrix_composite.resize(matrix_composite_elements_size);
		uint8_t * composite_matrix_data_ptr(bulk_msg.matrix_composite.data());
		size_type bulk_msg_choice_sets_alternatives_i(0);
		// in every choice set...
		for (::std::list<uint8_t>::const_iterator r(choice_sets_alternatives.begin()); r != choice_sets_alternatives.end(); ++r, ++bulk_msg_choice_sets_alternatives_i) { 
			assert(*r > 0);
			bulk_msg.choice_sets_alternatives(bulk_msg_choice_sets_alternatives_i, *r);

			uint8_t * composite_matrix_data_chosen_alternative_ptr = composite_matrix_data_ptr  + meta_msg.x_size() * *r;
			*composite_matrix_data_chosen_alternative_ptr = -1;

			// for every 'alternative' row...
			for (uint8_t a(0); a != *r; ++a) {
				assert(sorted_i != sorted.end());
				grid_obj.torow(*sorted_i++);

				// pack grouping by attributes
				uint8_t * tmp(composite_matrix_data_ptr + a);
				for (typename ::std::set<size_type>::const_iterator i(x_unique.begin()); i != x_unique.end(); ++i, tmp += *r) {
					if (grid_obj.template column<F>(*i) != grid_obj.template column<int>(*i))
						throw netcpu::message::exception("attribute columns can only contain whole numbers");
					*tmp = grid_obj.template column<int8_t>(*i);
				}

				int const best_i_value(grid_obj.template column<int>(best_i));
				if (best_i_value && best_i_value != 1 || best_i_value != grid_obj.template column<float_type>(best_i))
					throw netcpu::message::exception("choice column can only contain values of 0 or 1");
				if (best_i_value) {
					if (*composite_matrix_data_chosen_alternative_ptr != static_cast<uint8_t>(-1))
						throw netcpu::message::exception("design is not excepted to have multiple 'choices' present for a given choiceset -- only one alternative can be indicated as chosen");
					*composite_matrix_data_chosen_alternative_ptr = a;
				}
			}
			composite_matrix_data_ptr = composite_matrix_data_chosen_alternative_ptr + 1;
		}
		assert(sorted_i == sorted.end());
	}

public:
	void
	verify_args(dataset_1::message::meta<N> & meta_msg, dataset_1::message::bulk<N> & bulk_msg)
	{
		cli_check_matrix_elements(x_elements, "x submatrix", ute);
		if (filepath.empty() == false) {
			// xls_debug = 1;
			//rawgrid_type grid_obj(filepath.c_str(), "ASCII", sheet_i, typename grid_base::ctor(ute));
			::std::ifstream is(filepath);
			if (!is.good())
				throw netcpu::message::exception(xxx() << "Failed to open csv file(=" << filepath << ")");
			rawgrid_type grid_obj(is, typename grid_base::ctor(ute));
			verify_args_sub(grid_obj, meta_msg, bulk_msg);
		} else if (filedata.size()) {
			// does not currently (boost v.1_52) work with -fno-rtti switch (calls typeid), but I really dont wanna enable the damn thing, so there
			//::boost::iostreams::stream< ::boost::iostreams::array_source> data(filedata.data(), filedata.size());
			// instead doing this:
			struct : ::std::streambuf {
				using ::std::streambuf::setg;
		  } buf;
			buf.setg(filedata.data(), filedata.data(), filedata.data() + filedata.size()); 
			::std::istream data(&buf);

			rawgrid_type grid_obj(data, typename grid_base::ctor(ute));
			verify_args_sub(grid_obj, meta_msg, bulk_msg);
		} else if (filepath.empty() == false && filedata.size() || filepath.empty() == true && !filedata.size())
			throw netcpu::message::exception("must either supply the filepath for the csv file or the filedata itself");
	}
};

}}}

#endif
