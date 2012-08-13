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

// quick hack...

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <fstream>

#include <boost/integer.hpp>
#include <boost/scoped_array.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/type_traits.hpp>
#include <boost/optional.hpp>


namespace censoc {

bool fpu_ping(...);

template <typename N> struct round_to_int; 
template <> 
struct round_to_int<int64_t> {
	long long static
	eval(float x)
	{
		return ::llroundf(x);
	}
	long long static
	eval(double x)
	{
		return ::llround(x);
	}
	long long static
	eval(long double x)
	{
		return ::llroundl(x);
	}
}; 
template <> 
struct round_to_int<int32_t> {
	long static
	eval(float x)
	{
		return ::lroundf(x);
	}
	long static
	eval(double x)
	{
		return ::lround(x);
	}
	long static
	eval(long double x)
	{
		return ::lroundl(x);
	}
}; 

template <typename F>
F static
to_gnuplot_image_range(F const x)
{
	return 255 * ::std::min(static_cast<F>(1), ::std::max(static_cast<F>(0), x));
}


template <typename F, typename N, typename D>
class gaussinator {

	typedef F float_type;
	typedef N int_type;
	typedef D counts_type;

	float_type pre_exp_scale_x;
	float_type pre_exp_scale_y;
	float_type x_pos_kernel;
	float_type x_neg_kernel;
	int_type y_kernel;

	::boost::scoped_array<counts_type> src;
	::boost::scoped_array<float_type> dst_elevation;
	::boost::scoped_array<float_type> dst_confidence;

	::boost::optional<float_type> 
	eval(int_type row_i, int_type row_p, int_type column_i, int_type column_p) const
	{
		if (row_p != row_i && column_p != column_i) {
			int_type const dx(column_p - column_i);
			int_type const dy(row_p - row_i);
			return ::std::exp(dx * dx * pre_exp_scale_x + dy * dy * pre_exp_scale_y);
		} else if (row_p == row_i && column_p == column_i)
			return ::boost::optional<float_type>();
		else if (row_p == row_i) {
			int_type const dx(column_p - column_i);
			return ::std::exp(dx * dx * pre_exp_scale_x);
		} else {
			int_type const dy(row_p - row_i);
			return ::std::exp(dy * dy * pre_exp_scale_y);
		}
	}

	float_type
	do_point(int_type const row_p, int_type const column_p, float_type & confidence) const
	{
		float_type accum(0);
		counts_type * const __restrict src_(src.get());

		// todo -- verify that column_p should be 1-based (for the purposes of x kernel configurations)

		int_type tmp_row_begin(row_p - y_kernel);
		assert(tmp_row_begin < rows);
		int_type tmp_row_end(row_p + y_kernel + 1);
		int_type tmp_column_begin(column_p - round_to_int<int_type>::eval((column_p + 1) * x_neg_kernel));
		int_type tmp_column_end(column_p + round_to_int<int_type>::eval((column_p + 1) * x_pos_kernel) + 1);

		int_type const total_points((tmp_row_end - tmp_row_begin) * (tmp_column_end - tmp_column_begin));

		if (tmp_row_begin < 0) 
			tmp_row_begin = 0;
		if (tmp_row_end > rows)
			tmp_row_end = rows;
		assert(tmp_row_end > tmp_row_begin);

		if (tmp_column_begin < 0)
			tmp_column_begin = 0;
		if (tmp_column_end > columns)
			tmp_column_end = columns;
		assert(tmp_column_end > tmp_column_begin);

		int_type const actual_points((tmp_row_end - tmp_row_begin) * (tmp_column_end - tmp_column_begin)); 
		assert(actual_points > 0);

		confidence = static_cast<float_type>(actual_points) / total_points;

		for (int_type row_i(tmp_row_begin); row_i != tmp_row_end; ++row_i) {
			for (int_type column_i(tmp_column_begin), i(row_i * columns + tmp_column_begin); column_i != tmp_column_end; ++column_i, ++i) 
				if (!src_[i])
					continue;
				else if (src_[i] == 1) {
					::boost::optional<float_type> const rv(eval(row_i, row_p, column_i, column_p));
					if (rv)
						accum += *rv;
					else
						accum += 1;
				} else {
					::boost::optional<float_type> const rv(eval(row_i, row_p, column_i, column_p));
					if (rv)
						accum += src_[i] * *rv;
					else
						accum += src_[i];
				}
		}
		return accum;
	}

	int_type rows;
	int_type columns;

public:
	gaussinator(::std::string const & filename_base)
	: x_pos_kernel(.2), x_neg_kernel(.1), y_kernel(5), rows(0) {

		::std::string const filename_input_data(filename_base + ".csv");
		::std::string const filename_output_elevation(filename_base + "_elevation.csv"); 
		::std::string const filename_output_confidence(filename_base + "_confidence.csv");
		::std::string const filename_output_confidence_as_transparency(filename_base + "_confidence_as_transparency.csv");
		::std::string const filename_output_composite(filename_base + "_composite.csv");

		float_type normalize(1);
		//float_type cropped_value(-100);

		if (::boost::filesystem::exists(::boost::filesystem::path(filename_input_data)) == false)
			throw ::std::runtime_error("file " + filename_input_data + " does not exist");

		::std::ofstream out_elevation(filename_output_elevation.c_str(), ::std::ios_base::out | ::std::ios_base::trunc);
		::std::ofstream out_confidence(filename_output_confidence.c_str(), ::std::ios_base::out | ::std::ios_base::trunc);
		::std::ofstream out_confidence_as_transparency(filename_output_confidence_as_transparency.c_str(), ::std::ios_base::out | ::std::ios_base::trunc);
		::std::ofstream out_composite(filename_output_composite.c_str(), ::std::ios_base::out | ::std::ios_base::trunc);


		out_elevation << ::std::setprecision(10);
		out_confidence << ::std::setprecision(10);
		out_confidence_as_transparency << ::std::setprecision(10);
		out_composite << ::std::setprecision(10);

		typedef ::boost::tokenizer< ::boost::escaped_list_separator<char> > tokenizer_type;
		::std::string line;
		{
			::std::ifstream file(filename_input_data.c_str());
			while (::std::getline(file, line)) {
				tokenizer_type tokens(line);
				int_type tmp_columns(0);
				for (tokenizer_type::const_iterator i(tokens.begin()); i != tokens.end(); ++i, ++tmp_columns);
				if (!rows) {
					if (!tmp_columns)
						throw ::std::runtime_error("file must have columns");
					else
						columns = tmp_columns;
				} else if (columns != tmp_columns)
					throw ::std::runtime_error("every row in file must have the same number of columns. row no: " + ::boost::lexical_cast< ::std::string>(rows + 1));
				++rows;
			}
		}

		::std::clog << "Input File=(" << filename_input_data << ")\n";
		::std::clog << "Rows=(" << rows << ")\n";
		::std::clog << "Columns=(" << columns << ")\n";
		::std::clog << "Output Elevation File=(" << filename_output_elevation << ")\n";
		::std::clog << "Output Confidence File=(" << filename_output_confidence << ")\n";
		::std::clog << "Output Confidence(as transparency) File=(" << filename_output_confidence_as_transparency << ")\n";
		::std::clog << "Output Composite File=(" << filename_output_composite << ")\n";

		pre_exp_scale_x = -.5 / (29.01149 * 29.01149);
		pre_exp_scale_y = -.5 / (28.43413 * 28.43413);
		int_type const elems(rows * columns);

		src.reset(new counts_type[elems]);
		::std::ifstream file(filename_input_data.c_str());
		::std::string csv_token;
		for (int_type i(0); ::std::getline(file, line);) {
			tokenizer_type tokens(line);
			for (tokenizer_type::const_iterator j(tokens.begin()); j != tokens.end(); ++j, ++i) {
				csv_token = *j;
				::boost::trim(csv_token);
				int_type const tmp_count(::boost::lexical_cast<unsigned>(csv_token));
				if (tmp_count < 0 || ::boost::integer_traits<counts_type>::const_max < tmp_count)
					throw ::std::runtime_error("counts value in the source file is either negative or larger than compiled-for range capability in the program... rebuild the source code and use something like uint64_t instead of uint8_t for the gaussinator last template argument");
				src[i] = tmp_count;
			}
		}

	  dst_elevation.reset(new float_type[elems]);
	  dst_confidence.reset(new float_type[elems]);

		bool fpu_fail(false);

		float_type min(::std::numeric_limits<float_type>::max());
		// some boost system-libs have not the latest version installed
		//float_type max(::boost::numeric::bounds<float_type>::lowest());
		float_type max(-min);

		float_type * const __restrict dst_elevation_(dst_elevation.get());
		float_type * const __restrict dst_confidence_(dst_confidence.get());

		bool my_need_fpu_check(true);
		bool my_fpu_fail(false);
#pragma omp parallel for firstprivate(my_need_fpu_check) firstprivate(my_fpu_fail)
		for (int_type row_i = 0; row_i < rows; ++row_i) {
			if (my_need_fpu_check == true) {
				my_need_fpu_check = false;
				if (fpu_ping(&my_fpu_fail, dst_elevation_, dst_confidence_) == false)
					my_fpu_fail = true;
				else if (fpu_ping(::std::exp(100.f), &my_fpu_fail, dst_elevation_, dst_confidence_) == true) 
					my_fpu_fail = true;
			}
			float_type my_min(::std::numeric_limits<float_type>::max());
			float_type my_max(0);
			if (my_fpu_fail == false) {
				int_type i(row_i * columns);
				for (int_type column_i(0); column_i != columns; ++column_i, ++i) {
					float_type tmp_confidence;
					float_type const tmp_elevation(do_point(row_i, column_i, tmp_confidence));
					dst_confidence_[i] = tmp_confidence;
					dst_elevation_[i] = tmp_elevation;
					my_max = tmp_elevation > my_max ? tmp_elevation : my_max;
					my_min = tmp_elevation < my_min ? tmp_elevation : my_min;
				}
				if (fpu_ping(dst_elevation_, dst_confidence_) == false)
					my_fpu_fail = true;
			}
#pragma omp critical
			{
				if (my_fpu_fail == true)
					fpu_fail = true;
				else {
					if (my_min < min)
						min = my_min;
					if (my_max > max)
						max = my_max;
				}
			}
		}
		if (fpu_fail == true)
			throw ::std::runtime_error("fpu FAILED");

		int_type checked_grid_size(3);
		bool checked_grid_dark_row(true);

		float_type const scale(normalize / (max - min));
		int_type const last_col_i(columns - 1);
		for (int_type row_i(0), i(0); row_i != rows; ++row_i) {

			if (!(row_i % checked_grid_size))
				checked_grid_dark_row = !checked_grid_dark_row;

			bool checked_grid_dark_column(checked_grid_dark_row);

			for (int_type column_i(0); column_i != columns; ++column_i, ++i) {
				if (!(column_i % checked_grid_size))
					checked_grid_dark_column = !checked_grid_dark_column;

				//if (column_i >= columns_begin && column_i < columns_end && row_i >= rows_begin && row_i < rows_end)
					//out_elevation << (dst_elevation[i] - min) * scale;

					out_elevation << dst_elevation[i];
					out_confidence << dst_confidence[i];

					out_confidence_as_transparency << column_i + 1 << ',' << row_i + 1 
						<< ", 0, 0, 0, " 
						<< to_gnuplot_image_range(1 - dst_confidence[i])
						<< ::std::endl;


					float_type const x((dst_elevation[i] - min) * scale);
					float_type const checked_grid_value((checked_grid_dark_column == true ? .5 : 1) * (1 - dst_confidence[i]));

#if 0
					out_composite << column_i + 1 << ',' << row_i + 1 
						<< ',' << round_to_int<int_type>::eval((::std::sqrt(x) * dst_confidence[i] + checked_grid_value) * 255)
						<< ',' << round_to_int<int_type>::eval((x * x * x * dst_confidence[i] + checked_grid_value) * 255)
						<< ',' << round_to_int<int_type>::eval((::std::sin(x * (M_PI + M_PI)) * dst_confidence[i] + checked_grid_value) * 255)
						<< ::std::endl;
#else
					out_composite << column_i + 1 << ',' << row_i + 1 
						<< ',' << to_gnuplot_image_range(::std::sqrt(x) * dst_confidence[i] + checked_grid_value)
						<< ',' << to_gnuplot_image_range(x * x * x * dst_confidence[i] + checked_grid_value)
						<< ',' << to_gnuplot_image_range(::std::sin(x * (M_PI + M_PI)) * dst_confidence[i] + checked_grid_value)
						<< ::std::endl;
#endif

				//else
				//	::std::cout << cropped_value;
				if (column_i != last_col_i) {
					out_elevation << ", ";
					out_confidence << ", ";
				} else {
					out_elevation << '\n';
					out_confidence << '\n';
				}
			}
		}

		if (fpu_ping() == false || fpu_fail == true)
			throw ::std::runtime_error("fpu status is no good");
	}

};

}

int
main(int argc, char * * argv)
{
	try {
		if (argc != 2)
			throw ::std::runtime_error("must supply exactly one argument: filename for the data file (w/o extension). For example for the all-final.csv file type: all-final");
		::censoc::gaussinator<double, int64_t, uint8_t> t(::boost::lexical_cast< ::std::string>(argv[1]));
	} catch (::std::exception const & e) {
		::std::cout.flush();
		::std::cerr << "Runtime error... " << e.what() << '\n';
		return -1;
	} catch (...) {
		::std::cout.flush();
		::std::cerr << "Unknown runtime error.\n";
		return -1;
	}
	return 0;
}

// 98 elements std dev 28.43413; 100 elements std dev 29.01149
