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

#include <limits>

#include <boost/type_traits.hpp>

#ifndef CENSOC_TYPE_TRAITS_H
#define CENSOC_TYPE_TRAITS_H

namespace censoc {

#if 0

	template <typename T>
	class isprimitive {
		template <typename X>
		char static magic(void (X::*)());
		template <typename X>
		double static magic(X);
	public:
		bool const static eval = sizeof(magic<T>(0)) == 1 ? false : true;
	};

#endif

// quick and dirty (later involve more of ::boost)

#if 0
NOTE -- THIS IS SIMILAR TO BOOST, but given my bias towards modern compilers (e.g. gcc 4.5 et al), this "pass by value" instead of "pass by ref" is possible with compilation option(s):

-fipa-sra
			Perform interprocedural scalar replacement of aggregates, removal of unused parameters and REPLACEMENT OF PARAMETERS PASSED BY REFERENCE BY PARAMETERS PASSED BY VALUE.
			Enabled at levels -O2, -O3 and -Os. 

So, TODO in future, will simply discard this and pass by 'ref', 'ref const' and let the compiler figure the right replacements when needed. This may not be so easy though -- if inter-proc analysis must go on even if inlining has stopped -- as one needs to find out if IMIPLEMENTATION prints/uses the address of the arg passed by ref (if so -- then cannot pass by val)... so in cases of some libs/hidden translation objs. this may not work... but then again -- this semantically is not addressed by below code anyway... so there :-) Although by explicitly specifying 'param::type' one does make it more obvious as to the restrictive expectations "by design" not to be printing/using the address of the arg as it makes its way through potentially multiple method calls.

#endif

template <typename T, bool ByVal = 
	(::boost::is_pod<T>::value == false || sizeof(T) > sizeof(T *) ? false : true)
	> struct param;
template <typename T> struct param<T, true> { typedef T const type; };
template <typename T> struct param<T, false> { typedef T const & type; };

// todo -- deprecate and use ::std::remove_reference et al in future

template <typename T> struct strip_ref { typedef T type; };
template <typename T> struct strip_ref<T &> { typedef T type; };

template <typename T> struct strip_const { typedef T type; };
template <typename T> struct strip_const<T const> { typedef T type; };

// consistency across platforms (when required)

template <typename T> T static smallish();
template <> float smallish<float>() { return 1e-38f; }
template <> double smallish<double>() { return 1e-308; }

template <typename T> T static largish();
template <> float largish<float>() { return 1e+38f; }
template <> double largish<double>() { return 1e+308; }

unsigned const static DefaultAlign = 16;

#ifdef __GNUC__

#define CENSOC_RESTRICTED_PTR(t, name, init_expr) t * __restrict__ name(init_expr)

#define CENSOC_RESTRICTED_CONST_PTR(t, name, init_expr) t * const __restrict__ name(init_expr)

#define CENSOC_ALIGNED_RESTRICTED_CONST_PTR(t, name, init_expr) t * const __restrict__ name((t *) __builtin_assume_aligned((init_expr), 16)) ; assert(!((uintptr_t)name % DefaultAlign))

#define CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR__(t, name, init_expr, line) t __attribute__ ((aligned(16))) censoc_aligned_restricted_ptr_from_local_scalar_ ## name ## _ ## line (init_expr) ; t * const __restrict__ name((t *) __builtin_assume_aligned(& censoc_aligned_restricted_ptr_from_local_scalar_ ## name ## _ ## line, 16)) ; assert(!((uintptr_t)name % DefaultAlign))

#define CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR_(t, name, init_expr, line) CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR__(t, name, init_expr, line)

#define CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR(t, name, init_expr) CENSOC_ALIGNED_RESTRICTED_CONST_PTR_FROM_LOCAL_SCALAR_(t, name, init_expr, __LINE__)


#else
#error "aligned and restricted semantics have not yet been implemented for this compiler... TODO..."
#endif

}

#endif
