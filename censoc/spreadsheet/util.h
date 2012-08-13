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
#include <math.h>

#include <stdexcept>
#include <string>

#include <censoc/arrayptr.h>
#include <censoc/lexicast.h>

#ifndef CENSOC_SPREADSHEET_UTIL_H
#define CENSOC_SPREADSHEET_UTIL_H

namespace censoc { namespace spreadsheet {

template <typename N, typename xxx>
class util {
	//typedef typename Types::N N;

	// for verbosity
	//typedef typename Types::xxx xxx;

	// buffer for utility calls (almost insignificant)
	censoc::array<char, N> alpha;
	N alpha_size;

	// because, currently, alpha is not copyable... TODO -- fill re-factor later on...
	util(util const &) {}

public:
	util()
	: alpha_size(0) {
	}

	/**
		@param 'alpha' column name "AA", "A", "BS", etc.
		@post returns column's 0-based index
	*/
	N
	alpha_tofrom_numeric(::std::string const & alpha) const
	{
		assert(alpha.size());

		N numeric(0);
		N scale(1);
		for (N i(alpha.size() - 1); i != static_cast<N>(-1); --i) {
			assert(i < alpha.size());
			int const c(alpha.at(i));

			if (!::isalpha(c))
				throw ::std::runtime_error(xxx() << "not an alpha character: [" << c << "]");

			// "aaa" is not 0, "a" is not 0, so 'a' cannot be == index 0
			numeric += (::tolower(c) - ('a' - 1)) * scale;
			scale *= 'z' - 'a' + 1;
		}

		// 0-based
		return numeric - 1;
	}

	/**
		@param 'numeric' column's 0-based index
		@post returns lower-case column name "aa", "a", "bs", etc.
	*/
	::std::string
	alpha_tofrom_numeric(N numeric)
	{
		N const base('z' - 'a' + 1);
		// numeric + 1 so no logf(0), +1 is for 1 extra place due to rounding 
		N const alpha_size_new(logf(numeric + 1) / logf(25) + 2);
		assert(alpha_size_new);
		if (alpha_size < alpha_size_new)
			alpha.reset(alpha_size = alpha_size_new);

		++numeric;
		char * end(alpha + alpha_size);
		char * tmp(end);
		do {
			--numeric;
			N const mod(numeric % base);
			*--tmp = static_cast<char>(mod + 'a');
			assert(tmp >= alpha);
			assert(::isalpha(mod + 'a'));
			assert(::islower(mod + 'a'));
		} while (numeric /= base);

		return ::std::string(tmp, end - tmp);
	}
};

}}
#endif
