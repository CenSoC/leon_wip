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

// NOTE -- currently don't have internet connection so can't install boost. when will get some time -- will use boost :-) In the meantime, writing this code only takes a few minutes anyway...

#include <iostream>
#include <iomanip>

#include <stdexcept>
#include <string>
#include <sstream>

#include <boost/integer_traits.hpp>

#include "type_traits.h"

#ifndef CENSOC_LEXICAST_H
#define CENSOC_LEXICAST_H

namespace censoc {

// quick'n'nasty (will do for the time-being). 
	
/**
	@note -- only supporting numeric-string representations. thusly, '(un)signed char' (or rather (u)int8_t is treated as a number, not an ascii char), so values of '1' will not be converted to char's 1 ascii value, nor will they be printed as '1' ascii (rather as (int)1)

	@note -- on the other hand, given that C++ has 3 types for a char -- i.e. there is such as "char" != "signed char" && "char" != "unsigned char", any explicit typing of ']' 'a' etc. should be propagated unchanged...
	*/

template <typename Rv>
Rv const & 
eightbit_helper(Rv const & x)
{
	return x;
}
int16_t  
eightbit_helper(int8_t const & x)
{
	return x;
}
uint16_t  
eightbit_helper(uint8_t const & x)
{
	return x;
}

enum printstream_type {out, log, err};

template <censoc::printstream_type T> ::std::ostream static & stdstream();

template <>
::std::ostream & 
stdstream<censoc::out>() 
{
	return ::std::cout;
}

template <>
::std::ostream & 
stdstream<censoc::log>() 
{
	return ::std::clog;
}

template <>
::std::ostream & 
stdstream<censoc::err>() 
{
	return ::std::cerr;
}

template <printstream_type T>
struct lexiprint {
	template <typename From>
	lexiprint & operator << (From const & 
#ifndef __WIN32__
			x
#endif
			)
	{
#ifndef __WIN32__
		censoc::stdstream<T>() << eightbit_helper(x);
#endif
		return *this;
	}
	// for ::std::endl et al (which are manipulators)
	lexiprint & operator << (::std::ostream & (* 
#ifndef __WIN32__
				x
#endif
				) (::std::ostream &))
	{
#ifndef __WIN32__
		censoc::stdstream<T>() << x;
#endif
		return *this;
	}
};

typedef lexiprint<censoc::out> lout;
typedef lexiprint<censoc::log> llog;
typedef lexiprint<censoc::err> lerr;

template <typename> class lexicast;

template <>
class lexicast< ::std::string> {
	::std::stringstream engine;
public:
	void
	set_precision(unsigned const precision)
	{
		engine << ::std::setprecision(precision);
	}
	lexicast()
	{
	}
	void
	set_locale(::std::locale const & x)
	{
		engine.imbue(x);
	}
	template <typename From>
	lexicast(From const & x)
	{
		engine << eightbit_helper(x);
	}
	template <typename From>
	lexicast< ::std::string> & operator << (From const & x)
	{
		engine << eightbit_helper(x);
		return *this;
	}
	operator ::std::string ()
	{
		return engine.str();
	}
};

template <typename Rv>
void 
eightbit_helper(::std::stringstream & ss, Rv & rv)
{
	ss >> rv;
	if (!ss)
		throw ::std::runtime_error(lexicast< ::std::string>("failed in lexical numeric conversion from: [") << ss.str() << "] to: [" << rv << "]");
}
void 
eightbit_helper(::std::stringstream & ss, int8_t & rv)
{
	int16_t tmp;
	ss >> tmp;
	if (!ss || tmp > ::boost::integer_traits<int8_t>::const_max || tmp < ::boost::integer_traits<int8_t>::const_min)
		throw ::std::runtime_error(lexicast< ::std::string>("failed in lexical numeric conversion from: [") << ss.str() << "] to: [" << rv << "]");
	rv = static_cast<int8_t>(tmp);
}
void 
eightbit_helper(::std::stringstream & ss, uint8_t & rv)
{
	uint16_t tmp;
	ss >> tmp;
	if (!ss || tmp > ::boost::integer_traits<int8_t>::const_max)
		throw ::std::runtime_error(lexicast< ::std::string>("failed in lexical numeric conversion from: [") << ss.str() << "] to: [" << rv << "]");
	rv = static_cast<uint8_t>(tmp);
}

template <typename To>
class lexicast {
	typedef typename censoc::strip_const<typename censoc::strip_ref<To>::type>::type type; 
	type rv;
public:
	template <typename From>
	lexicast(From const & from)
	: rv(from) {
	}
	lexicast(char const * const from)
	{
		fromstr(from);
	}
	lexicast(char * const from)
	{
		fromstr(from);
	}
	lexicast(::std::string const & from)
	{
		fromstr(from);
	}
	operator type const & ()
	{
		return rv;
	}
private:
	template <typename From>
	void
	fromstr(From const & from)
	{
		::std::stringstream ss;
		ss << from;
		eightbit_helper(ss, rv);
	}
};

// todo -- continuing in a spirit of quick-hack thingies, later on provide a more appropriate placing of the formatting code (and it's reuse of the relevant locale objects)
struct coma_separated_thousands : ::std::numpunct<char> {
	char 
		do_thousands_sep() const
		{
			return ',';
		}
	std::string 
		do_grouping() const
		{
			return "\03";
		}
};
::std::locale const static coma_separated_thousands_locale(::std::locale(""), new coma_separated_thousands);

}
#endif
