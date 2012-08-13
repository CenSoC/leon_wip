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

#include <netcpu/message.h>

#ifndef CENSOC_NETCPU_DATASET_1_COMPOSITE_MATRIX_H
#define CENSOC_NETCPU_DATASET_1_COMPOSITE_MATRIX_H

namespace censoc { namespace netcpu { namespace dataset_1 { 

// throughput
template <typename From, typename To, bool is_from_unsigned = ::boost::is_unsigned<From>::value, bool is_to_unsigned = ::boost::is_unsigned<To>::value>
struct	raw_copy_helper {
	From static
	eval(From from)
	{
		return from;
	}
};

// custom conversion... leveraging the inference of unsigned containing converted signed (earlier on)
template <typename From, typename To>
struct	raw_copy_helper<From, To, true, false> {
	To static
	eval(From from)
	{
		return netcpu::message::deserialise_from_unsigned_to_signed_integral(from);
	}
};

// rowmajor matrix
template <typename Size, typename Data>
struct composite_matrix_map {

	typedef Size size_type;
	typedef Data data_type;

	BOOST_STATIC_ASSERT(::boost::is_integral<data_type>::value == true);

protected:
	data_type * data_;
	size_type rows_;
	size_type columns_;
public:

	composite_matrix_map()
	: data_(NULL) {
	}
	composite_matrix_map(data_type * const data, size_type rows, size_type columns)
	{
		data_ = data;
		rows_ = rows;
		columns_ = columns;
	}
	data_type const *
	data() const
	{
		return data_;
	}
	data_type &
	operator()(size_type row, size_type column)
	{
		assert(row < rows_);
		assert(column < columns_);
		return *(data_ + row * columns_ + column);
	}
	data_type const &
	operator()(size_type row, size_type column) const
	{
		assert(row < rows_);
		assert(column < columns_);
		return *(data_ + row * columns_ + column);
	}
	size_type
	cols() const
	{
		return columns_;
	}
	size_type 
	rows() const
	{
		return rows_;
	}
	template <typename T>
	void
	copy_raw(T const * const data, size_type elements)
	{
		assert(data_ != NULL);
		assert(data != NULL);
		assert(elements == rows_ * columns_);

		// TODO specialize for T == data_type
		for (size_type i(0); i != elements; ++i)
			data_[i] = dataset_1::raw_copy_helper<T, data_type>::eval(data[i]);
	}
	template <typename T>
	void
	copy_raw_row(T const & rhs, size_type src, size_type dst)
	{
		assert(src < rhs.rows());
		assert(dst < rows_);
		assert(rhs.data() != NULL);
		assert(data_ != NULL);
		assert(static_cast<void const *>(rhs.data()) != static_cast<void const *>(data_));
		assert(rhs.cols() == columns_);

		// TODO specialize for T::data_type == this->data_type
		typename T::data_type const * src_offset(rhs.data() + src * columns_);
		data_type * dst_offset(data_ + dst * columns_);
		for (data_type * const end(dst_offset + columns_); dst_offset != end; ++dst_offset, ++src_offset)
			*dst_offset = dataset_1::raw_copy_helper<typename T::data_type, data_type>::eval(*src_offset);
	}
};
template <typename size_type, typename data_type>
struct composite_matrix : composite_matrix_map<size_type, data_type> {

	typedef composite_matrix_map<size_type, data_type> base_type;

	~composite_matrix()
	{
		delete [] base_type::data_;
	}
	template <typename T>
	void
	cast(T const * const data, size_type rows, size_type columns)
	{
		base_type::rows_ = rows;
		base_type::columns_ = columns;
		size_type const end_i(rows * columns);
		delete [] base_type::data_;
		base_type::data_ = new data_type[end_i];
		base_type::copy_raw(data, end_i);
	}
};

}}}

#endif
