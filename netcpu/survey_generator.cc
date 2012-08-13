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
#include <stdint.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

uint_fast32_t const static respondents_size(500);
uint_fast32_t const static oversample_scale(3);

::std::string const static separator(",\t");

int
main()
{
	::std::cout << "resp" << separator << "obs" << separator << "alt" << separator << "choice";
	for (uint_fast8_t i(0); i != 2; ++i)
		::std::cout << separator << "atr" << i + 1;
	::std::cout << '\n';

	::std::vector<uint_fast8_t> choicesets;
	choicesets.reserve(16);
	for (uint_fast8_t i(0); i != 16; ++i) {
		uint_fast8_t const low(i & 3);
		uint_fast8_t const high(i >> 2);
		if (low != high)
			choicesets.push_back(i);
	}

	for (uint_fast32_t r_i(0); r_i != respondents_size; ++r_i) {
		for (uint_fast32_t oversample_i(0); oversample_i != oversample_scale; ++oversample_i) {
			::std::random_shuffle(choicesets.begin(), choicesets.end());
			for (uint_fast8_t c_i(0); c_i != choicesets.size(); ++c_i) {
				uint_fast8_t choice(choicesets[c_i]);
				for (unsigned a_i(0); a_i != 2; ++a_i) {

					::std::cout << r_i + 1  << separator << c_i + choicesets.size() * oversample_i + 1 << separator << a_i + 1 << separator;

					// fake choice (irrelevant really), just to pass controller validity-check (expected to be overwritten by simulator.exe)
					::std::cout << a_i;

					for (unsigned j(0); j != 2; ++j) {
						::std::cout << separator << (1 & choice ? 1 : -1); 
						choice >>= 1;
					}
					::std::cout << '\n';
				}
				assert(!choice);
			}
		}
	}
	return 0;
}
