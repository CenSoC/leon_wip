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

#include <windows.h>
#include <winbase.h>

#else

#include <unistd.h>

#endif

#ifdef __FreeBSD__
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifndef CENSOC_SCHEDULER_H
#define CENSOC_SCHEDULER_H

namespace censoc { namespace scheduler {

void static
idle()
{
#ifndef __WIN32__

	::setpriority(PRIO_PROCESS, 0, 20);
	// TODO -- for freebsd also checkout ::rtprio call (and idprio command)

#else

	if (!::SetPriorityClass(::GetCurrentProcess(), IDLE_PRIORITY_CLASS))
		::std::clog << "failed to set IDLE_PRIORITY_CLASS\n";
	//::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_LOWEST);
	// ::SetThreadPriority(::GetCurrentThread(), -3);
	if (!::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_IDLE))
		::std::clog << "failed to set THREAD_PRIORITY_IDLE\n";

	// being extra conservative here...
	if (!::SetProcessPriorityBoost(::GetCurrentProcess(), TRUE))
		::std::clog << "failed to SetProcessPriorityBoost\n";
	if (!::SetThreadPriorityBoost(::GetCurrentThread(), TRUE))
		::std::clog << "failed to SetThreadPriorityBoost\n";

	// not in mingw headers:
#define PROCESS_MODE_BACKGROUND_BEGIN 0x00100000
#define THREAD_MODE_BACKGROUND_BEGIN 0x00010000

	// TODO -- cludge, calling for all windows but it is not supported on XP (and so will not implicitly reduce working set size)
	::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
	::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);

	{
	// ... calling this explicitly for the sake of XP because the 'background mode' setting (which sets working set size implicitly, e.g. on W7) is not supported on XP
	// (TODO -- later will do if/else thingy based on what system it is built-for or which system has been detected at runtime... if need be)
	SIZE_T min, max;
	if (::GetProcessWorkingSetSize(::GetCurrentProcess(), &min, &max)) {
		enum {target_max = 1024 * 1024 * 24};
		if (max > target_max) {
			// SetProcessWorkingSetSizeEx *is* available in mingw64 (but not in mingw32) -- TODO its about time I move my x-compiler from mingw32 to mingw64 even for the 32bit targets, mingw64 should support it...
			if (!::SetProcessWorkingSetSize(::GetCurrentProcess(), min, target_max))
				::std::clog << "failed to SetProcessWorkingSetSize\n";
		}
	} else
		::std::clog << "failed to GetProcessWorkingSetSize\n";
	}

	// TODO -- ONE WILL NEED TO DO FURTHER RESEARCH ON WHEN/IF the above CODE CAN CAUSE OTHER (MORE "IMPORTANT") APPS TO QUIT/CRASH -- AS THEY DECIDE TO ERROR WITH 'could not set working set of wanted size' error if they choose to set their own working size. will need to investigate under which conditions setting of the process working set size will fail: e.g. when other apps have grabbed a lot for themselves and current apps request will not be satisfied if total RAM is insufficient, even though others may be paged out...)

	// NOTE -- if *using* the above -- then *must* call/enable-code in other places such as: ::SetProcessWorkingSetSize(::GetCurrentProcess(), ...) in order to reduce soft-pagefaults in W7. Alternatively may play with defined or not __win64__ thingies etc.
	// WILL USE THE ABOVE, HOWEVER, RIGHT NOW AND SET THE LIMIT ON WORKING SET OF THE PROCESS -- TO TEST THE "CLOSE-TO-WORST" CASES ON PERFORMANCE.


	//{ TODO -- io priority (vista and greater only)

#if 0

	// this is a temp collection of info from net

	The IoSetIoPriorityHint routine sets the priority hint value for an IRP.
		Syntax
		Copy

		NTSTATUS IoSetIoPriorityHint(
				__in  PIRP Irp,
				__in  IO_PRIORITY_HINT PriorityHint
				);


	Parameters

		Irp [in]

		Specifies the IRP to set the priority hint value for.
		PriorityHint [in]

		Specifies the IO_PRIORITY_HINT value that indicates the new priority hint.

		Return Value

		IoSetIoPriorityHint returns STATUS_SUCCESS if the operation succeeds and the appropriate NTSTATUS value if the operation fails.



		IO_PRIORITY_HINT Enumeration

		The IO_PRIORITY_HINT enumeration type specifies the priority hint for an IRP.
		Syntax
		Copy

		typedef enum _IO_PRIORITY_HINT {
			IoPriorityVeryLow    = 0,
			IoPriorityLow        = 1,
			IoPriorityNormal     = 2,
			IoPriorityHigh       = 3,
			IoPriorityCritical   = 4,
			MaxIoPriorityTypes   = 5 
		} IO_PRIORITY_HINT;


	Constants

		IoPriorityVeryLow

		Specifies the lowest possible priority hint level. The system uses this value for background I/O operations.
		IoPriorityLow

		Specifies a low-priority hint level. 
		IoPriorityNormal

		Specifies a normal-priority hint level. This value is the default setting for an IRP.
		IoPriorityHigh

		Specifies a high-priority hint level. This value is reserved for use by the system.
		IoPriorityCritical

		Specifies the highest-priority hint level. This value is reserved for use by the system.
		MaxIoPriorityTypes

		Marks the limit for priority hints. Any priority hint value must be less than MaxIoPriorityTypes.

		Remarks

		For more information about priority hints, see Using IRP Priority Hints.

		Contained in Ntoskrnl.lib.

#endif

		//}

#endif
}

// hacks indeed
void static 
sleep_s(unsigned secs)
{
#ifdef __WIN32__
	::Sleep(secs * 1000);
#else
	::sleep(secs);
#endif
}

}}

#endif
