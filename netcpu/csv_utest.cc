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
#include <fstream>

#include <censoc/lexicast.h>
#include <censoc/type_traits.h>
#include <censoc/spreadsheet/csv.h>

typedef unsigned size_type;
typedef ::censoc::param<size_type>::type size_paramtype;
typedef censoc::lexicast< ::std::string> xxx;
typedef censoc::spreadsheet::util<size_type, xxx> ute_type;
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
typedef censoc::spreadsheet::csv<size_type, xxx, grid_base> rawgrid_type;

char const buffer[] = "\"test\" , , boo, \"test \",\n   01, 2, 3, 4, 15.15, \"\"\"a\"\n\n\tte\t st,\"\"\"boo\",goo\nbam,baa";
char const static filepath[] = "/tmp/csv_utest_1.tmp"; 


int main()
{
	{
		::std::ofstream tmp_file(filepath, ::std::ios::binary | ::std::ios::trunc);
		tmp_file.write(buffer, sizeof(buffer));
	}
	ute_type ute;
	rawgrid_type grid(filepath, grid_base::ctor(ute));
	grid.torow(1);
	assert(grid.column< ::std::string>(5) ==  "\"a");
	grid.torow(0);
	assert(grid.column< ::std::string>(1) == "");
	assert(grid.column< ::std::string>(3) == "test ");
	assert(grid.column< ::std::string>(4) == "");
	assert(grid.column< ::std::string>(0) == "test");
	grid.torow(2);
	assert(grid.column< ::std::string>(0) == "test");
	assert(grid.column< ::std::string>(1) == "\"boo");
	assert(grid.column< ::std::string>(2) == "goo");
	grid.torow(3);
	assert(grid.column< ::std::string>(0) == "bam");
	assert(grid.column< ::std::string>(1) == "baa");
	assert(grid.rows() == 4);
	// todo -- add test for empty fields enclosed in quotes...
	return 0;
}

