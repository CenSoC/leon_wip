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

#include <boost/scoped_array.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/type_traits.hpp>
#include <boost/optional.hpp>


namespace censoc {

	typedef ::boost::tokenizer< ::boost::escaped_list_separator<char> > tokenizer_type;

	struct filedata {
		unsigned rows;
		unsigned columns;
		::boost::scoped_array<uint8_t> data;

		filedata(::std::string const & filename)
		: rows(0) {
			if (::boost::filesystem::exists(::boost::filesystem::path(filename)) == false)
				throw ::std::runtime_error("file " + filename + " does not exist");
			::std::string line;
			{
				::std::ifstream file(filename.c_str());
				while (::std::getline(file, line)) {
					tokenizer_type tokens(line);
					unsigned tmp_columns(0);
					for (tokenizer_type::const_iterator i(tokens.begin()); i != tokens.end(); ++i, ++tmp_columns);
					if (!rows) {
						if (!tmp_columns)
							throw ::std::runtime_error("file must have columns");
						else
							columns = tmp_columns;
					} else if (columns != tmp_columns)
						throw ::std::runtime_error("every row in file must have the same number of columns. row=(" + ::boost::lexical_cast< ::std::string>(rows + 1) + "), file=(" + filename + ')');
					++rows;
				}
			}

			unsigned const elems(rows * columns);
			data.reset(new uint8_t[elems]);
			::std::ifstream file(filename.c_str());
			::std::string csv_token;
			for (unsigned i(0); ::std::getline(file, line);) {
				tokenizer_type tokens(line);
				for (tokenizer_type::const_iterator j(tokens.begin()); j != tokens.end(); ++j, ++i) {
					csv_token = *j;
					::boost::trim(csv_token);
					data[i] = ::boost::lexical_cast<unsigned>(csv_token);
				}
			}
		}
	}; 

	void
	run(::std::string const & filename_all, ::std::string const & filename_female, ::std::string const & filename_male)
	{
		filedata all(filename_all);
		filedata female(filename_female);
		filedata male(filename_male);

		if (all.rows != female.rows)
			throw ::std::runtime_error("file=(" + filename_all + ") rows=(" + ::boost::lexical_cast< ::std::string>(all.rows) + ") does not equate to file=(" + filename_female + ") rows =(" + ::boost::lexical_cast< ::std::string>(female.rows) + ')');
		if (all.columns != female.columns)
			throw ::std::runtime_error("file=(" + filename_all + ") columns=(" + ::boost::lexical_cast< ::std::string>(all.columns) + ") does not equate to file=(" + filename_female + ") columns =(" + ::boost::lexical_cast< ::std::string>(female.columns) + ')');
		if (all.rows != male.rows)
			throw ::std::runtime_error("file=(" + filename_all + ") rows=(" + ::boost::lexical_cast< ::std::string>(all.rows) + ") does not equate to file=(" + filename_male + ") rows =(" + ::boost::lexical_cast< ::std::string>(male.rows) + ')');
		if (all.columns != male.columns)
			throw ::std::runtime_error("file=(" + filename_all + ") columns=(" + ::boost::lexical_cast< ::std::string>(all.columns) + ") does not equate to file=(" + filename_male + ") columns =(" + ::boost::lexical_cast< ::std::string>(male.columns) + ')');

		for (unsigned row_i = 0; row_i < all.rows; ++row_i) {
			unsigned i(row_i * all.columns);
			for (unsigned column_i(0); column_i != all.columns; ++column_i, ++i) {
				if (all.data[i] != female.data[i] + male.data[i])
					throw ::std::runtime_error("row=(" + ::boost::lexical_cast< ::std::string>(row_i) + ") column=(" + ::boost::lexical_cast< ::std::string>(column_i) + ") has the following inequality: file=(" + filename_all + ") value=(" + ::boost::lexical_cast< ::std::string>(all.data[i]) + ") != file=(" + filename_female + ") value=(" + ::boost::lexical_cast< ::std::string>(female.data[i]) + ") + file=(" + filename_male + ") value=(" + ::boost::lexical_cast< ::std::string>(male.data[i]) + ')');
			}
		}
	}

}

int
main(int argc, char * * argv)
{
	try {
		::censoc::run("all_final.csv", "female_final.csv", "male_final.csv");
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
