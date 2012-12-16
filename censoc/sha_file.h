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

#include <string.h>

#include <openssl/sha.h>

#include <fstream>

#include <boost/filesystem.hpp>

#include "lexicast.h"
#include "arrayptr.h"

#ifndef CENSOC_SHA_FILE_H
#define CENSOC_SHA_FILE_H

namespace censoc { 

template <typename size_type>
struct sha {
	enum {hushlen = 20};
	typedef typename censoc::param<size_type>::type size_paramtype;
	void static
	calculate(void const * data, size_paramtype data_size, uint8_t * hush)
	{
		assert(data_size);

		::SHA_CTX sha_ctx;
		::SHA1_Init(&sha_ctx);
		::SHA1_Update(&sha_ctx, data, static_cast<unsigned long>(data_size));
		::SHA1_Final(hush, &sha_ctx);
	}
};

template <typename size_type>
struct sha_buf {
	enum {hushlen = censoc::sha<size_type>::hushlen};
	typedef typename censoc::param<size_type>::type size_paramtype;
private: 
	uint8_t hush[hushlen]; 

public:
	uint8_t const *
	get() const
	{
		return hush;
	}

	uint8_t *
	get() 
	{
		return hush;
	}

	void
	calculate(void const * data, size_paramtype data_size)
	{
		assert(data_size);
		sha<size_type>::calculate(data, data_size, hush);
	}

	void
	operator = (char const * x) 
	{
		::memcpy(hush, x, hushlen);
	}

};

// note -- it would appear (although further research is needed) that some/all versions/distros openssl have no: 'SHA1_File()', now given the portability requirements, shall do 'lowe-level' stuff...
template <typename size_type>
struct sha_file {
	typedef censoc::lexicast< ::std::string> xxx;

	censoc::sha_buf<size_type> buf;

public:

	enum {hushlen = censoc::sha_buf<size_type>::hushlen};

	uint8_t const *
	get() const
	{
		return buf.get();
	}

	// calculates based on contents of the specified file
	uint8_t const *
	calculate(::std::string const & filepath)
	{
		size_type const filesize(::boost::filesystem::file_size(filepath));
		if (!filesize) // nasty hack for the time-being 
			::memset(buf.get(), -1, hushlen);
		else {
			::std::ifstream file(filepath.c_str(), ::std::ios::binary);
			censoc::array<char, size_type> filedata(filesize);
			file.read(filedata, static_cast< ::std::streamsize>(filesize));
			buf.calculate(filedata, filesize);
		}
		return buf.get();
	}

	// loads from stored file (i.e. file is sha data itself)
	uint8_t const *
	load(::std::string const & filepath)
	{
		size_type const filesize(::boost::filesystem::file_size(filepath));
		if (filesize != hushlen) // nasty hack for the time-being
			::memset(buf.get(), 0, hushlen);
		else {
			::std::ifstream file(filepath.c_str(), ::std::ios::binary);
			file.read(reinterpret_cast<char *>(buf.get()), hushlen);
		}
		return buf.get();
	}

	// saves hush value to file (i.e. file shall have sha data itself)
	void
	store(::std::string const & filepath) const
	{
		::std::ofstream file(filepath.c_str(), ::std::ios::binary | ::std::ios::trunc);
		file.write(reinterpret_cast<char const *>(buf.get()), hushlen);
	}

	void
	operator = (char const * x) 
	{
		buf = x;
	}


};

}

#endif
