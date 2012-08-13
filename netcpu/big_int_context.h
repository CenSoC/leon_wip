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

/**
	@NOTE -- currently largely a hack for the range_utils to be able to represent unevrsally-unique position in the solution space (total solution space, not just iterated-one via the combos_builder)
 */

#include <openssl/bn.h>

// @TODO -- as with other bits of code (e.g. range_utils) later will move to 'outside-of-netcpu-centric' scope...
#ifndef CENSOC_NETCPU_BIG_INT_CONTEXT_H
#define CENSOC_NETCPU_BIG_INT_CONTEXT_H

namespace censoc { namespace netcpu { 

// hacky singleton -- don't want to have a separate translation unit for classic version just yet... (later if this wrapper of bignum happens to be of use -- move to outer scope and perhaps make the context to be a classic singleton)
struct big_int_context_type {
	BN_CTX * context; // BN_CTX is not externally declared in current installation of openssl -- meaning that initialization of statically declared BN_CTX is not possible
	big_int_context_type()
	: context(::BN_CTX_new()) {
		//::BN_CTX_init(context);
	}
	void
	reset()
	{
		::BN_CTX_free(context);
		context = ::BN_CTX_new();
		//::BN_CTX_init(&context);
	}
	// not really needed -- expected to be static scope anyways
	~big_int_context_type()
	{
		::BN_CTX_free(context);
	}
};
// not instantiating -- to make client app more aware by doing it explicitly
}

}

#endif
