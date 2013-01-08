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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdint.h>
#include <windows.h>
#include <iprtrmib.h>
#include <iphlpapi.h>
#include <winioctl.h>
#if __SIZEOF_POINTER__ == 8
#include <winsock2.h>
#endif

#include <stdexcept>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <censoc/scheduler.h>
#include <censoc/sysinfo.h>

unsigned static cpus_size;
unsigned static idx;
typedef uint_fast64_t * counters_type[2];
counters_type static idle_counts;
counters_type static cpu_counts;

uint_fast64_t static ip_io_count;
uint_fast64_t static disk_io_count;

void static
ping_exit()
{
	::MSG msg;
	while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		if (msg.message == WM_QUIT)
			::ExitProcess(0);
}

// NOTE -- SHOULD LATER USE PDH PERFORMANCE DATA HELPING LIB (LIKE PDHADDCOUNTER ET AL), BUT THIS IS QUICKER TO HACK TO ROUGHLY TEST THE CONCEPT (NO TIME NOW TO HACK RELEVANT DECLARATIONS/STRUCTS SUPPORT WITHIN MINGW32 WHICH CURRENTLY DOES NOT HAVE EXPLICIT HEADERS/LIB FOR IT'S PDH QUERY/COUNTERS... ALTHOUGH MINGW64, WHICH CAN BE MADE TO BUILD 32BIT RUNTIME AS WELL, DOES APPARENTLY SUPPORT PDH... WILL NEED TO MIGRATE LATER ON...)
// AND LET US NOT FORGET THAT THE WHOLE WINDOOOZE THING/PORT IS A FREAKING COMPROMISE IN THE FIRST PLACE (IF I HAD A CHOICE I WOULD NOT NEED TO WRITE ANY SUCH WINDOOOOZE-RELATED GARBAGE... I MEAN CODE... IN THE FIRST PLACE).

#if 0
typedef long (__stdcall * queryhack_t)(int, void*, unsigned long, unsigned long *); 
queryhack_t static queryhack;
#else
// mingw may have it in libntdll.a
extern "C"
{
long __stdcall ZwQuerySystemInformation(int, void*, unsigned long, unsigned long *); 
}
#endif

void static
ping_cpu() 
{
	static struct system_processor_performance_information {
		LARGE_INTEGER idle;
		LARGE_INTEGER kernel;
		LARGE_INTEGER user;
		LARGE_INTEGER reserved_1[2];
		ULONG reserved_2;
	} * const cpus(new system_processor_performance_information[cpus_size]);
	ULONG res;
	if (
			//queryhack
			::ZwQuerySystemInformation
			(8, cpus, sizeof(system_processor_performance_information) * cpus_size, &res) < 0) 
		throw ::std::runtime_error("ZwQuerySystemInformation for processor-specific info failed");
	idx = idx ? 0u : 1u;
	for (unsigned i(0u); i != cpus_size; ++i) {
		idle_counts[idx][i] = cpus[i].idle.QuadPart;
		cpu_counts[idx][i] = cpus[i].kernel.QuadPart + cpus[i].user.QuadPart;
	}
}


#if 0
NetStatisticsGet (not good -- only for the service {workstation,server} but not the whole computre/system)

or may be RegQueryValueEx(HKEY_PERFORMANCE_DATA, TEXT("Global")) -- may be can use the registry (since not doing things all that often)?
or may be GetIpStatisticsEx (no good -- mingw32 does not appear to include it at the time of this observation)

but there is GetIpStatistics though (meaing only IP4)

NOTE -- TODO: at the end of the day, when migrating to mingw64 altogether, will just convert to PDH (performance data helpers).

#endif

/**
	@note -- quick and nasty hack atm -- i.e. far from correct/perfect, but whatever is the quickest way to get over the line on this nightmare that is windoooze systems.
	@todo -- as per ping_cpu, do use PDH lib later on when/if time allows to do proper coding (or when 32bit mingw is no longer needed altogether)
	*/
void static
ping_io()
{
	ip_io_count = 0u;
	disk_io_count = 0u;


#if 1
	// TODO -- use GetIfTable and GetIpEntry instead
	::boost::shared_ptr<MIB_IFTABLE> scoped_iftable(static_cast<MIB_IFTABLE *>(::malloc(sizeof(MIB_IFTABLE))), ::free); 
	DWORD dwSize = sizeof(MIB_IFTABLE);
	if (::GetIfTable(scoped_iftable.get(), &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
		scoped_iftable.reset(static_cast<MIB_IFTABLE *>(::malloc(dwSize)), ::free); 
	} else
		throw ::std::runtime_error("GetIfTable -- 1st call");

	if (::GetIfTable(scoped_iftable.get(), &dwSize, 0) == NO_ERROR) {
		MIB_IFROW ifrow;
		for (unsigned i(0); i != scoped_iftable->dwNumEntries; ++i) {
			ifrow.dwIndex = scoped_iftable->table[i].dwIndex;
			if (::GetIfEntry(&ifrow) == NO_ERROR) {
				if (ifrow.dwType != IF_TYPE_SOFTWARE_LOOPBACK)
					ip_io_count += ifrow.dwInOctets;
			} else
				throw ::std::runtime_error("GetIfEntry");
		}
	} else
		throw ::std::runtime_error("GetIfTable -- 2nd call");

#else

	::MIB_IPSTATS s;
	if (::GetIpStatistics(&s) != NO_ERROR) // use PDH later (although, there ought to be as "simple of an interface" for byte-related count on system network IO as it is for the ip packets stats)
		throw ::std::runtime_error("GetIpStatistics");
	ip_io_count += s.dwInReceives;
	// io_count += s.dwOutRequests;

	// currently mingw64 is only used for 64 builds and it has IPv6 support call (todo -- later will use mingw64 distro to build 32 bit binaries)
#if __SIZEOF_POINTER__ == 8
	if (::GetIpStatisticsEx(&s, AF_INET6) != NO_ERROR)
		throw ::std::runtime_error("GetIpStatisticsEx");
	ip_io_count += s.dwInReceives;
#endif

#endif


	// disks (later will re-implement via pdh lib) as this needs 'diskperf -y' command (todo -- find out if this is permanent command or not, also what are the performance issues if any?)
	unsigned i(0u);
	do {
		::std::string drive_filepath("\\\\.\\PhysicalDrive" + ::boost::lexical_cast< ::std::string, unsigned>(i++));
		HANDLE h(::CreateFile(drive_filepath.c_str(), 0, FILE_SHARE_READ 
				// | FILE_SHARE_WRITE
				,
				NULL, OPEN_EXISTING, 0, NULL));
		if (h == INVALID_HANDLE_VALUE)
			break;
		else {
			DISK_PERFORMANCE info[2]; // x2 is simply because wingw32 winioctl.h header for DISK_PERFORMANCE is too short (unlinke ddk/ntdddisk.h or indeed in mingw64 winioctl.h)
			DWORD tmp(0);
			if (::DeviceIoControl(h, IOCTL_DISK_PERFORMANCE, NULL, 0, &info, sizeof(DISK_PERFORMANCE) << 1, &tmp, NULL)) {
				disk_io_count += info[0].BytesRead.QuadPart;
				disk_io_count += info[0].BytesWritten.QuadPart;
			} 
			::CloseHandle(h);
		}
	} while (!0);

}

int
main()
{
#if 0
	HMODULE const module(::GetModuleHandle("ntdll.dll"));
	if (module == NULL) 
		throw ::std::runtime_error("getmodule failed for ntdll.dll");
	queryhack = reinterpret_cast<queryhack_t>(::GetProcAddress(module, "ZwQuerySystemInformation")); 
	if (queryhack == NULL) 
		throw ::std::runtime_error("getprocaddress failed for ZwQuerySystemInformation");
#endif

	::censoc::scheduler::idle();

	cpus_size = censoc::sysinfo::cpus_size();
	idle_counts[0] = new uint_fast64_t[cpus_size * 2];
	idle_counts[1] = idle_counts[0] + cpus_size;
	cpu_counts[0] = new uint_fast64_t[cpus_size * 2];
	cpu_counts[1] = cpu_counts[0] + cpus_size;

	unsigned non_idle_end(cpus_size >> 1);
	if (!non_idle_end)
		++non_idle_end;

	while (!0) {
		try {
			::SetProcessWorkingSetSize(::GetCurrentProcess(), -1, -1);

			for (unsigned i(0); i != 57 * 6; ++i) {
				::censoc::scheduler::sleep_s(10);
				ping_exit();
			}

			ping_cpu();
			::censoc::scheduler::sleep_s(10);
			ping_cpu();
			ping_exit();
			
			bool bail(false);
			for (unsigned j(0u), non_idle_size(-1); j != cpus_size && bail == false; ++j) {
				unsigned const last_idx(idx ? 0u : 1u);
				uint_fast64_t const cpu_diff(cpu_counts[idx][j] - cpu_counts[last_idx][j]);
				uint_fast64_t const idle_diff(idle_counts[idx][j] - idle_counts[last_idx][j]);
				if (idle_diff < cpu_diff - cpu_diff / 8u) // no core should have > 12%
					bail = true;
				else if (idle_diff < cpu_diff - cpu_diff / 12u) // at least ~50% of cores (if on hugely multicore box) should not have > 8%
					if (++non_idle_size == non_idle_end) 
						bail = true;
			}
			if (bail == true) {
				::SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
				continue;
			} 

			// one could have done with only 1 call to 'ping_io' (if calling GetSystemTime or similar and then calculating the amount if time spent in the sleep state)... but will leave for future todo.
			// mainly because:  the code would not get to this point unless there is no CPU activity anyway (i.e. there is plenty of cpu cycles available to do this additional code); and current model offers lesser additional code to write (meaning lesser issues to deal with -- a bonus w.r.t. lose(r)os environment).
			ping_io();
			uint_fast64_t const tmp_ip_io_count(ip_io_count);
			uint_fast64_t const tmp_disk_io_count(disk_io_count);
			::censoc::scheduler::sleep_s(10);
			ping_io();
			ping_exit();
			if (
#if 1
					// using GetIfEintry et al
					ip_io_count - tmp_ip_io_count > 12 * 10 * 1024 // 12kB per sec
#else
					// using GetIpStatistics
					ip_io_count - tmp_ip_io_count > 70 // 70 packets in 10 secs
#endif
					||
					disk_io_count - tmp_disk_io_count > 150 * 10 * 1024 // 150kB per sec
					) { 
				::SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
				continue;
			}
			::SetThreadExecutionState(ES_CONTINUOUS);
		} catch (::std::exception const & e) {
			::std::cerr << "Runtime error: [" << e.what() << "]\n";
		} catch (...) {
			::std::cerr << "Unknown runtime error.\n";
		}
	}
	return 0;
}
