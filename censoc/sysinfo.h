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

#ifdef __WIN32__
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winbase.h>
#endif

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifndef CENSOC_SYSINFO_H
#define CENSOC_SYSINFO_H

namespace censoc { namespace sysinfo {

// find out how many cpus I have...
// NOTE/TODO: these will invoke extra ::GetSystemInfo call in loser-os, but that's ok (not a bottle-neck)... later will write a variant of the call to get more info types in one call (if such becomes needed w.r.t. bottle-neck/performance-related standpoints)...
inline unsigned static
cpus_size() 
{
#ifdef __FreeBSD__

	// TODO also/instead cat try to use the libpmc, pmc_ncpu call et al!!!

	// e.g. hw.ncpu: 2
	int mib[4];
	size_t mib_len(4);
	if (::sysctlnametomib("hw.ncpu", mib, &mib_len))
		throw ::std::runtime_error("could not sysctlnametomib");
	size_t len;
	if (::sysctl(mib, mib_len, NULL, &len, NULL, 0)) 
		throw ::std::runtime_error("could not sysctl");
	censoc::array<char, unsigned> data(len);
	if (::sysctl(mib, mib_len, data, &len, NULL, 0))
		throw ::std::runtime_error("could not sysctl");

	return *reinterpret_cast<unsigned const *>(static_cast<char const *>(data));

#elif defined __WIN32__

	// TODO -- NOTE, if there is difference between values returned from GetSystemInfo and ZwQuerySystemInformation w.r.t. number of logical CPUs, then must use ZwQuerySystemInformation!!! -- will need to test on bi-modal CPUs.
	::SYSTEM_INFO sysinfo;
	::GetSystemInfo(&sysinfo);

#if 0 
	// not in mingw32 and the aobve appears to do the job ok(ish)...
	//{ TODO -- more advanced process-counting option... if needed

	typedef enum _PROCESSOR_CACHE_TYPE {
		CacheUnified,
		CacheInstruction,
		CacheData,
		CacheTrace 
	} PROCESSOR_CACHE_TYPE;

	typedef struct _CACHE_DESCRIPTOR {
		BYTE                 Level;
		BYTE                 Associativity;
		WORD                 LineSize;
		DWORD                Size;
		PROCESSOR_CACHE_TYPE Type;
	} CACHE_DESCRIPTOR, *PCACHE_DESCRIPTOR;

	typedef struct _SYSTEM_LOGICAL_PROCESSOR_INFORMATION {
		ULONG_PTR                      ProcessorMask;
		LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
		union {
			struct {
				BYTE Flags;
			} ProcessorCore;
			struct {
				DWORD NodeNumber;
			} NumaNode;
			CACHE_DESCRIPTOR Cache;
			ULONGLONG        Reserved[2];
		} ;
	} SYSTEM_LOGICAL_PROCESSOR_INFORMATION, *PSYSTEM_LOGICAL_PROCESSOR_INFORMATION;

	typedef BOOL (WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

	LPFN_GLPI glpi = (LPFN_GLPI) GetProcAddress(::GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");
	if (glpi != NULL) {
		DWORD buflen(0);
		if (::GetLogicalProcessorInformation(NULL, &buflen) != FALSE)
			throw ::std::runtime_error("first call to ::GetLogicalProcessorInformation expected to fail...");
		else if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
			throw ::std::runtime_error("after first call to ::GetLogicalProcessorInformation the ::GetLastError expected to return ERROR_INSUFFICIENT_BUFFER");
		return buflen / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
	}

	//}
#endif
	return sysinfo.dwNumberOfProcessors;
#endif
}

inline uint64_t static
ram_size()
{
#ifdef __FreeBSD__

	// e.g. hw.ncpu: 2
	int mib[4];
	size_t mib_len(4);
	if (::sysctlnametomib("hw.physmem", mib, &mib_len))
		throw ::std::runtime_error("could not sysctlnametomib");
	size_t len;
	if (::sysctl(mib, mib_len, NULL, &len, NULL, 0)) 
		throw ::std::runtime_error("could not sysctl");
	censoc::array<char, unsigned> data(len);
	if (::sysctl(mib, mib_len, data, &len, NULL, 0))
		throw ::std::runtime_error("could not sysctl");

	return *reinterpret_cast<uint64_t const *>(static_cast<char const *>(data));

#elif defined __WIN32__

	::MEMORYSTATUSEX meminfo;
	meminfo.dwLength = sizeof(meminfo);
	if (!::GlobalMemoryStatusEx(&meminfo))
		throw ::std::runtime_error("could not GlobalMemoryStatusEx");

	return meminfo.ullTotalPhys;
#endif
}

}}

#endif
