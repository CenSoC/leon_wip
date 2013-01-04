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

//{ includes...
#include <censoc/type_traits.h>
#include <netcpu/value_on_grid.h>
//}

#ifndef CENSOC_NETCPU_CONVERGER_1_COEFFICIENT_METADATA_BASE_H
#define CENSOC_NETCPU_CONVERGER_1_COEFFICIENT_METADATA_BASE_H

namespace censoc { namespace netcpu { namespace converger_1 { 

/**
	@NOTE -- a hack of commonly-shared utils w.r.t. coeff metadata between server and processor code
 */

template <typename N, typename F>
struct coefficient_metadata_base {
	typedef typename netcpu::message::typepair<N>::ram size_type;
	typedef typename censoc::param<size_type>::type size_paramtype;

	typedef F float_type;
	typedef typename censoc::param<float_type>::type float_paramtype; 
	typedef netcpu::value_on_grid<size_type, float_type> value_on_grid_type;

	::std::vector<size_type> grid_resolutions; // in calculated-complexity terms (not model-wide)
	value_on_grid_type now_value_;
	value_on_grid_type tmp_value_; 
	float_type value_from_;
	float_type rand_range_;

protected:

#ifndef NDEBUG
	coefficient_metadata_base() 
	: rand_range_ (0) {
	}
#endif

public:

#ifndef NDEBUG
	typename censoc::strip_const<typename censoc::param<value_on_grid_type>::type>::type
	tmp_value_testonly() const
	{
		return tmp_value_;
	}
#endif

	typename censoc::strip_const<float_paramtype>::type
	value_from() const
	{
		return value_from_;
	}

	typename censoc::strip_const<float_paramtype>::type
	rand_range() const
	{
		return rand_range_;
	}

	void
	value_reset()
	{
		tmp_value_ = now_value_;
	}

	void
	value_save()
	{
		now_value_ = tmp_value_;
	}

	bool 
	value_modified() const
	{
		return tmp_value_ == now_value_ ? false : true;
	}

	typename censoc::strip_const<float_paramtype>::type
	saved_value() const
	{
		return now_value_.value();
	}

	typename censoc::strip_const<float_paramtype>::type
	value() const
	{
		return tmp_value_.value();
	}

	typename censoc::strip_const<size_paramtype>::type
	saved_index() const
	{
		return now_value_.index();
	}

	typename censoc::strip_const<size_paramtype>::type
	index() const
	{
		return tmp_value_.index();
	}

	void
	value_from_grid(size_paramtype grid_i, size_paramtype grid_res)
	{
		assert(grid_i < grid_res);
		assert(rand_range_ != 0);
		assert(grid_resolutions[0] >= grid_res);

		tmp_value_.index((500 + uint_fast64_t(1000) * grid_i * grid_resolutions[0] / grid_res) / 1000, grid_resolutions[0], value_from_, rand_range_);
	}

	template <typename Vector>
	void
	set_grid_resolutions(Vector const & grid_resolutions)
	{
		::std::cout << "setting grid resolutions:\n";
		this->grid_resolutions.resize(grid_resolutions.size());
		for (size_type i(0); i != this->grid_resolutions.size(); ++i) {
			this->grid_resolutions[i] = grid_resolutions[i];
			::std::cout << this->grid_resolutions[i] << ' ';
		}
		::std::cout << ::std::endl;
	}

};

}}}

#endif
