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

// TODO!!! -- when the codebase/apps get more stable/concrete, then try to REDUCE THE CODEBASE based on COMMONALITY WITH controller.CC FILE/PROGRAM!!! (e.g. define commonly shared classes such as peer_connection et al)... (doing the whole inter-programmatic generalisation now will be a bit too soon and is more likely to result in a messy code rather than an elegant structure of information models).

#ifdef __WIN32__
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// for should_sleep primitive hacking (later, if the overall idea works, will hack-in pdh support in mingw)
#include <stdint.h>
#endif

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#endif

#include <string>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem.hpp>

#include <censoc/lexicast.h>
#include <censoc/arrayptr.h>
#include <censoc/scheduler.h>
#include <censoc/sysinfo.h>
#include <censoc/finite_math.h>
#include <censoc/rand.h>

#ifdef __WIN32__
// mingw may have ZwQuerySystemInformation in libntdll.a (otherwise, if using different compiler, comment this out and re-enable the GetProcAddress et al code).
extern "C"
{
long __stdcall ZwQuerySystemInformation(int, void*, unsigned long, unsigned long *); 
}
#endif

namespace censoc { namespace netcpu {
	
::boost::filesystem::path static ownpath;
::boost::asio::io_service static io;
::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);
::boost::system::error_code static ec;
unsigned static cpu_affinity_index(0);
unsigned static clone_instance_index(0);

uintptr_t static free_ram_low_watermark;
uintptr_t static free_ram_high_watermark;
bool static
test_anticipated_ram_utilization(double x)
{
	x = x * 1.1 + 
		// todo -- define the guard for app and it's mem structs more correctly as opposed to this ugly hack
		21000000;
	double free_to_play_with(censoc::sysinfo::ram_size() * .8); // todo: make parametric so that may easily utilise all of the memeroy if/when running on a dedicated box.
	double const cumulative_memory_consumption((netcpu::clone_instance_index + 1.) * x);
	if (cumulative_memory_consumption > free_to_play_with)
		return false;
	else {
		free_ram_high_watermark = censoc::sysinfo::ram_size() * .1;
		free_ram_low_watermark = free_ram_high_watermark - x * .777;
		return true;
	}
}

//{ windooze insanity
#ifdef __WIN32__

void static
ping_exit()
{
	::MSG msg;
	while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		if (msg.message == WM_QUIT)
			::ExitProcess(0);
}

// SwitchToThread'n'Sleep(x) is rather an overly primitive hack and was shown (much like Sleep(0) alternatives) not to work particularly well (although much better than Sleep(0)). Instead a semantic equivalent to GetProcessorTimes (note thats 'processor' not 'process' -- leverage of the cpu-affinity :-) and GetThreadTimes is being attempted... 

// NOTE -- should later use PDH performance data helping lib (like PdhAddCounter et al), but this is quicker to hack to roughly test the concept (no time now to hack relevant declarations/structs support within mingw32 which currently does not have explicit headers/lib for it's PDH query/counters... although mingw64, which can be made to build 32bit runtime as well, does apparently support pdh... will need to migrate later on...)

// and let us not forget that the whole windoooze thing (and its scheduler wrt higher-prio, page-faulting matalab et al) is a freaking compromise in the first place (if I had a choice I would not need to write any such windooooze-related garbage... I mean code... in the first place -- so if this approach of cpu-measured "scheduler assisting" happens to show that it is of some use then will TODO more elegant/proper PDH-based fixes as needs arise).

// at the end of the day -- user may be able to set higher WorkingSetSize for matlab et al processes -- but the background process such as this one cannot rely on this at all!

// also will need to consider that, at least semantically, the queries and counters need start/stop mechanisms... whereas {Nt,Zw}Query... appears to just pull the data out of 'prestructured' and 'preaccounted-for' structs... so it may actually be faster... not that it would matter a great deal.

struct system_processor_performance_information {
	LARGE_INTEGER idle;
	LARGE_INTEGER kernel;
	LARGE_INTEGER user;
	LARGE_INTEGER reserved_1[2];
	ULONG reserved_2;
};

class {

public:

	void
	init()
	{
		systime_stamp = ping_systime();

		am_sleeping = false;
		time_atom = 500;

		HMODULE const module(::GetModuleHandle("ntdll.dll"));
		if (module == NULL) 
			throw ::std::runtime_error("getmodule failed for ntdll.dll");

// mingw may have ZwQuerySystemInformation in libntdll.a
#if 0
		queryhack = reinterpret_cast<queryhack_t>(::GetProcAddress(module, "ZwQuerySystemInformation")); 
		if (queryhack == NULL) 
			throw ::std::runtime_error("getprocaddress failed for ZwQuerySystemInformation");
#endif

		ping();
	}

	// should probably call this 'bool op' only with a few sec interval, not any sooner! but for the time-being will just use internal protection at the expense of extra syscall (normally would have done this via async timer in the calling code or similar... but this is only a windoooze-related hack for the time-being anyway so no need to complicate outer architecture too much)
	operator bool ()
	{
		uint_fast64_t const systime_stamp_tmp(ping_systime());
		uint_fast64_t const systime_delta_msec((systime_stamp_tmp - systime_stamp) / 10000);

		if (systime_delta_msec < time_atom)
			return am_sleeping;

		// TODO -- also, alternatively, just try with cpu_delta / 25 but with / 30 even...
		// although this code will indeed force windows scheduler to schedule high-prio thread on "my" core
		static uint_fast64_t long_mode(0);
		if ((long_mode += systime_delta_msec) > 6000) {
			long_mode = 0;
			ping_exit();

			// extra case for ram availability w.r.t. other, non-NetCPU, user-based running programs etc. 

			unsigned static ncpus(censoc::sysinfo::cpus_size());
			if (censoc::sysinfo::free_ram_size() < (am_sleeping == true ? free_ram_low_watermark : free_ram_high_watermark)) {
				sleep();
				// random value for sleep is similar to CSMACD strategy -- multilpe instances of processors are unlinkely to come back at once so they will gradually "fill" the ram capacity as permitted by free space and the last one to wake-up will go back to sleep...
				// an alternative strategy could have been for each of the processor's instances to create a "i am aware of the ram depletion and have gone to sleep" file (made unique by the filename containing the clone_index from the processor's instance).
				// then instances with 'lower' clone index would not immediately go into sleep upon sensing the ram issue, but rather wait for higher-indexed "I am aware and sleeping" index to become present in the file system. there would have to be an additional time-out sensitivity in case the highe-indexed instance has crashed, so that the lower-indexed instance can proceed with own sleeping anyway. but, for the time-being, shall address the issue in a "kiss" manner -- keep it simple stupid :-)
				time_atom = 120000 + (censoc::rand<uint32_t>::eval() % ncpus) * 15000;
				systime_stamp = systime_stamp_tmp;
				return true;
			} else {
				// extra case for battery-charge level (if/when applicable)
				::SYSTEM_POWER_STATUS sps;
				// Rather optimistic, w/o taking care of 'unknown', error-returns, etc. use-cases all over the place, quick hack.
				if (::GetSystemPowerStatus(&sps)) {
					// TODO -- make it more robust if need be later on...
					if (!sps.ACLineStatus || sps.BatteryFlag != 128 && sps.BatteryLifePercent < 95) {
						sleep();
						time_atom = 7 * 60 * 1000; // check in about 7 minutes
						systime_stamp = systime_stamp_tmp;
						return true;
					}
				}
			}

			if (am_sleeping == false) {
				ping();
				if (::SwitchToThread())
					::Sleep(230);
				else
					::Sleep(120);
			}
		}

		uint_fast64_t const cpu_idlestamp_tmp(cpu_idlestamp);
		uint_fast64_t const cpu_stamp_tmp(cpu_stamp);
		uint_fast64_t const thread_stamp_tmp(thread_stamp);
		ping();

		// not much optimization atm -- just semantic-level coding

		uint_fast64_t const cpu_idle_delta(cpu_idlestamp - cpu_idlestamp_tmp);
		// ::censoc::llog() << "cpu_idle_delta: " << cpu_idle_delta << ::std::endl;

		uint_fast64_t const cpu_delta((cpu_stamp - cpu_stamp_tmp) - cpu_idle_delta); // semantically, idle appears to be included in 'kern + user'
		// ::censoc::llog() << "cpu_delta: " << cpu_delta << ::std::endl;

		uint_fast64_t const thread_delta(thread_stamp - thread_stamp_tmp);
		// ::censoc::llog() << "thread_delta: " << thread_delta << ::std::endl;

		systime_stamp = systime_stamp_tmp;
		// if the processor is determined to have been sufficiently busy and; if this thread used < ~96% of processor load then something else must have been running... need to sleep and let that something else have full freedom (thanks to nasty scheduling issues in win scheduler -- low-piro cpu-loop prevents various high-prio threads from running on SMP cases load-balancing... at the end of the day -- user may be able to set higher WorkingSetSize for matlab et al processes -- but the background process such as this one cannot rely on this at all!
		if (cpu_delta > cpu_idle_delta / 33 && thread_delta < cpu_delta - cpu_delta / 25) 
			return sleep();
		else // or cpu was idle or; cpu was not idle and this thread utilisation was > ~95%, so no need to sleep
			return awake();

#if 0
		// oldish code for extra-condition of when not to call awake
		if (systime_delta_sec < 5) { // do not awake too soon...
			cpu_idlestamp = cpu_idlestamp_tmp;
			cpu_stamp = cpu_stamp_tmp;
			thread_stamp = thread_stamp_tmp;
			return am_sleeping;
		} else {
			// or cpu was idle or; cpu was not idle and this thread utilisation was > ~95%, so no need to sleep
			systime_stamp = systime_stamp_tmp;
			return awake();
		}
#endif
	}

	uint_fast32_t
	sleep_for() const
	{
		return time_atom;
	}

private:
// mingw may have ZwQuerySystemInformation in libntdll.a
#if 0
	typedef long (__stdcall * queryhack_t)(int, void*, unsigned long, unsigned long *); 
	queryhack_t queryhack;
#endif
	uint_fast64_t cpu_idlestamp;
	uint_fast64_t cpu_stamp;
	uint_fast64_t thread_stamp;

	// tmp hack only...
	uint_fast64_t systime_stamp;

	bool am_sleeping;
	uint_fast32_t time_atom;

	bool
	awake()
	{
		if (am_sleeping == true) {
			// WILL USE THE FOLLOWING RIGHT NOW AND SET THE LIMIT ON WORKING SET (explicitly in XP, and implicitly in W7 due to 'background' nature) OF THE PROCESS -- TO TEST THE "CLOSE-TO-WORST" CASES ON PERFORMANCE.

			// ON THE OTHER HAND, IF SOFT-PAGEFAULTS ARE TO BE MINIMIZED -- CONSIDER PASSING A LARGER AMOUNT WHICH WILL ACTUALLY BE USED BY THE APP: MOST LIKELY THIS WILL HAVE TO COME FROM the model-specific context something ::SetProcessWorkingSetSize(::GetCurrentProcess(), anticipated_ram_utilization >> 1, anticipated_ram_utilization);

			// SetProcessWorkingSetSizeEx *is* available in mingw64 (but not in mingw32) -- TODO its about time I move my x-compiler from mingw32 to mingw64 even for the 32bit targets, mingw64 should support it...
			// ::SetProcessWorkingSetSize(::GetCurrentProcess(), 1024 * 1024 * 8, 1024 * 1024 * 32);


			// NOTE ALTHOUGH CURRENTLY NOT TESTING THE ABOVE (setting limits of working set for close-to-worst-case performance scenario) -- BECAUSE ONE WILL NEED TO DO FURTHER RESEARCH ON WHEN/IF THIS CODE CAN CAUSE OTHER (MORE "IMPORTANT") APPS TO QUIT/CRASH -- AS THEY DECIDE TO ERROR WITH 'could not set working set of wanted size' error. will need to investigate under which conditions setting of the process working set size will fail: e.g. when other apps have grabbed a lot for themselves and current apps request will not be satisfied if total RAM is insufficient, even though others may be paged out...)

			am_sleeping = false;
			time_atom = 500;

		} else if (time_atom < 700)
			time_atom <<= 1;
		return false;
	}

	bool 
	sleep()
	{
		if (am_sleeping == false) {
			::SetProcessWorkingSetSize(::GetCurrentProcess(), -1, -1);
			am_sleeping = true;
			time_atom = 500;
		} else if (time_atom < 9000)
			time_atom <<= 1;
		return true;
	}

	// NOTE -- yes start (cpu/thread) and stop (thread/cpu) would be better in terms of consistency of figures etc., but may need more syscalls... and given that this code is an ugly hack anyway (should use single query and performance counters etc. for pedantically-minded windoooze fans), this shall do for the time-being.
	void
	ping()
	{
		// processor...
		ULONG res;
		unsigned const static cpus_size(censoc::sysinfo::cpus_size());
		system_processor_performance_information static * const cpus(new system_processor_performance_information[cpus_size]);
		if (
				// mingw may have ZwQuerySystemInformation in libntdll.a
				// queryhack
				::ZwQuerySystemInformation
				(8, cpus, sizeof(system_processor_performance_information) * cpus_size, &res) < 0) 
			throw ::std::runtime_error("::ZwQuerySystemInformation for processor-specific info failed");

		// GetSystemInfo cor cpus_size does take into account hyper-threading..., will do sanity-check (for the time-being, TODO -- delete later after testing on HT bi-modal multi-core CPUs)
		if (res != sizeof(system_processor_performance_information) * cpus_size)
			throw ::std::runtime_error("::ZwQuerySystemInformation infers different cpus_size as compared to ::GetSystemInfo");

#if 0
		// nasty non-portable query hack
		::censoc::llog() << "----------------------------------------";
		for (unsigned i(0); i != cpus_size; ++i) {
			::censoc::llog() << "\nprinting proc struct no: " << i 
			<< " kernel: " << packint(cpus[i].kernel) 
			<< " user: " << packint(cpus[i].user)
			<< " idle: " << packint(cpus[i].idle)
			<< " reserved_1[0]: " << packint(cpus[i].reserved_1[0])
			<< " reserved_1[1]: " << packint(cpus[i].reserved_1[1])
			<< " reserved_2: " << cpus[i].reserved_2;
		}
		::censoc::llog() << ::std::endl;
		//
#endif

		cpu_stamp = packint(cpus[::censoc::netcpu::cpu_affinity_index].kernel) + packint(cpus[::censoc::netcpu::cpu_affinity_index].user);
		cpu_idlestamp = packint(cpus[::censoc::netcpu::cpu_affinity_index].idle);

		// thread
		FILETIME dummy;
		FILETIME kern;
		FILETIME usr;
		::GetThreadTimes(::GetCurrentThread(), &dummy, &dummy, &kern, &usr); // can pass NULL for generic?
		thread_stamp = packint(kern) + packint(usr);
	}

	uint_fast64_t
	ping_systime()
	{
		FILETIME systime;
		::GetSystemTimeAsFileTime(&systime);
		return packint(systime);
	}

	uint_fast64_t 
	packint(censoc::param<LARGE_INTEGER>::type x)
	{
#if __SIZEOF_POINTER__ == 8
		return x.QuadPart;
#else
		return (static_cast<uint64_t>(x.HighPart) << 32) | x.LowPart;
#endif
	}

	uint_fast64_t 
	packint(censoc::param<FILETIME>::type x)
	{
		return (static_cast<uint64_t>(x.dwHighDateTime) << 32) | x.dwLowDateTime;
	}
} static should_i_sleep;

#endif
//}


}}

#include "message.h"
#include "io_driver.h"

namespace censoc { namespace netcpu { 

// this must be a very very small class (it will exist through the whole of the lifespan of the server)
struct model_factory_interface {
	void virtual new_task(netcpu::message::async_driver &) = 0;
	// not really needed if all of the factories are static-time duration/lifespan
	// but could be of use when debugging for malloc issues/leaks and thereby freeing all factories at exit.
#ifndef NDEBUG
	virtual ~model_factory_interface()
	{
	}
#endif

};
}}

#include "models.h"
#include "io_wrapper.h"

namespace censoc { namespace netcpu { 

struct peer_connection;

typedef censoc::lexicast< ::std::string> xxx;

struct interface {
	// note --  things like filename for certificate(s) -- those on some systems may be relevant w.r.t. CWD (current working dir) which may or may not be the same as the dir for the binary itself... yet the cfg/args may not always impose the absolute path (or indeed know of it) -- so even if starting from a batch file things like 'processor.exe' -- the batch/shell stcript may have an 'indeterminate' CWD on the target processing machin (from the stapoint of the script-generating server).
	interface()
	: endpoints(io) {
		::std::string server_ssl_certificate;
		::std::string client_ssl_certificate;
		::std::string client_ssl_key;

		::boost::filesystem::path config_filepath(netcpu::ownpath);
		config_filepath.replace_extension(".cfg");
		::std::cerr << "FINDING: " << config_filepath.string() << ::std::endl;
		::std::ifstream config_file(config_filepath.string().c_str());

		if (config_file) {

			::std::string server_at, server_cert_filename, client_cert_filename, client_key_filename;
			config_file >> server_at;
			config_file >> server_cert_filename;
			config_file >> client_cert_filename;
			config_file >> client_key_filename;

			if (server_at.empty() == false && server_cert_filename.empty() == false && client_cert_filename.empty() == false && client_key_filename.empty() == false) {

				::std::cerr << "server_at: " << server_at << ", cert: " << server_cert_filename << ::std::endl;

				censoc::array<char, uint_fast8_t> server_at_c_str(static_cast<uint_fast8_t>(server_at.size() + 1));
				::strcpy(server_at_c_str, server_at.c_str());

				char const * const host(::strtok(server_at_c_str, ":"));
				if (host == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value: [" << server_at << "] need 'host:port'");

				char const * const port = ::strtok(NULL, ":");
				if (port == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value: [" << server_at << "] need 'host:port'");

				::std::cerr << "host: " << host << ", port: " << port << ::std::endl;

				endpoints_i = endpoints.resolve(::boost::asio::ip::tcp::resolver::query(
							::boost::asio::ip::tcp::v4(), // to generate faster DNS resolution (possibly), but lesser compatible (no IP6): todo -- comment out when IP4 gets deprecated :-)
							host, port));

				::boost::filesystem::path server_ssl_certificate_filepath = netcpu::ownpath.parent_path();
				server_ssl_certificate_filepath /= server_cert_filename;
				server_ssl_certificate = server_ssl_certificate_filepath.string();
				
				::boost::filesystem::path client_ssl_certificate_filepath = netcpu::ownpath.parent_path();
				client_ssl_certificate_filepath /= client_cert_filename;
				client_ssl_certificate = client_ssl_certificate_filepath.string();
				
				::boost::filesystem::path client_ssl_key_filepath = netcpu::ownpath.parent_path();
				client_ssl_key_filepath /= client_key_filename;
				client_ssl_key = client_ssl_key_filepath.string();
			}
		}

		if (
			endpoints_i == ::boost::asio::ip::tcp::resolver::iterator() 
			|| server_ssl_certificate.empty() == true
			|| client_ssl_certificate.empty() == true
			|| client_ssl_key.empty() == true
		)
			throw ::std::runtime_error("configuration files must supply server_at (in a host:port form) followed by server_certificate_filename followed by client_certificate_filename followed by private_key_filename (all filenames are to be located in the executable directory, i.e. where the program actually resides)");

		ssl.set_verify_mode(::boost::asio::ssl::context::verify_peer | ::boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		ssl.load_verify_file(server_ssl_certificate);

		ssl.use_certificate_file(client_ssl_certificate, boost::asio::ssl::context::pem);
		ssl.use_private_key_file(client_ssl_key, boost::asio::ssl::context::pem);
	}
	::boost::asio::ip::tcp::resolver endpoints;
	::boost::asio::ip::tcp::resolver::iterator endpoints_i;
};

struct peer_connection : public netcpu::io_wrapper<netcpu::message::async_driver> {

	peer_connection(netcpu::message::async_driver & x)
	: netcpu::io_wrapper<netcpu::message::async_driver>(x) {
		// not calling 'run' code here because 'write' will implicitly call 'shared_from_this' and external onwership by shared pointer is not yet setup (ctor for shared pointer will wait for the ctor of peer_con to complete first)...
		// still, however, it is more elegant than having shared_ptr(this) here in ctor an then worrying/making sure that nothing that follows it in ctor will ever throw
	}

	void
	run()
	{
		netcpu::message::version msg;
		msg.value(1);
		io().write(msg, &peer_connection::on_write, this);
	}

	void 
	on_error()
	{
		// todo later just test if on_error then call it otherwise skip altogether... (and let asio take things out of scope)
	}

	void 
	on_write()
	{
		io().write(netcpu::message::ready_to_process(), &peer_connection::on_ready_to_process_write, this);
	}

	void
	on_ready_to_process_write()
	{
		io().read(&peer_connection::on_read, this);
	}

	void 
	on_read()
	{
		netcpu::message::id_type const id(io().read_raw.id());
		switch (id) {
		case netcpu::message::bad::myid :
		{
			censoc::llog() << "peer rejected last message: [" << io().socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n";
		}
		break;
		case netcpu::message::task_offer::myid :
		{
			netcpu::message::task_offer msg;
			msg.from_wire(io().read_raw);
			censoc::llog() << "a task is on offer: [" << msg.task_id() << "], from: [" << io().socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n";
			netcpu::models_type::iterator implementation(netcpu::models.find(msg.task_id()));
			if (implementation != netcpu::models.end()) 
				implementation->second->new_task(io());
			else 
				io().write(netcpu::message::bad());
			}
		return;
		default:
			throw netcpu::message::exception(xxx() << "inappropriate message: [" << id << ']');
		}
		io().read();
	}
	~peer_connection() throw();
};

void static
run() 
{
	// TODO -- also add this to the server (inclusive of linking with finite_math.o)
	censoc::init_fpu_check();

	::boost::scoped_ptr<interface> ui(new interface);
	
	::boost::shared_ptr<netcpu::message::async_driver> new_driver(new netcpu::message::async_driver);
	netcpu::peer_connection * new_peer(new peer_connection(*new_driver)); // another way would be to do shared_ptr(this) in ctor of peer_connection (but then one must take extra bit of care for anything that follows this in ctor, and potentially derived types, not to throw at all)... this way, howere, can throw all one wants...

	while (ui->endpoints_i != ::boost::asio::ip::tcp::resolver::iterator()) {
		new_driver->socket.lowest_layer().connect(*ui->endpoints_i, netcpu::ec);
		if (!ec) 
			break;
		else {
			new_driver->socket.lowest_layer().close();
			++ui->endpoints_i;
		}
	}
	if (ui->endpoints_i == ::boost::asio::ip::tcp::resolver::iterator())
		throw ::std::runtime_error(xxx() << "could not connect to server");

	new_driver->socket.handshake(::boost::asio::ssl::stream_base::client, netcpu::ec);
	if (ec)
		throw ::std::runtime_error("could not do SSL handshake");

	new_driver->set_keepalive();

	ui.reset(); 
	new_peer->run();
	new_driver.reset();
	io.run();
	// TODO -- this may not be needed ideally -- as it is processor, in needs not exit gracefully when server's connection is terminated !!! ... although for the time-being at appears to do this anyway (i.e. graceful exiting).
	io.reset();
}

peer_connection::~peer_connection() throw()
{
	censoc::llog() << "dtor in peer_connection" << ::std::endl;
}

}}


int 
main(int argc, char * * argv)
{
#ifdef __WIN32__
	//::SetErrorMode(-1); // let us not automatically babyseat the SEM_NOALIGNMENTFAULTEXCEPT
	::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif

#if 0

#ifdef __SSSE3__
	::std::cout << "YES ssse3 \n";
#endif

#ifdef __SSE2__
	::std::cout << "YES sse2\n";
#endif

#ifdef EIGEN_VECTORIZE
	::std::cout << "YES eigen_vectorize\n";
#endif

#ifdef EIGEN_VECTORIZE_SSE
	::std::cout << "YES eigen_vectorize_sse\n";
#endif

#endif

	while (!0) {

		::censoc::scheduler::sleep_s(10);

#ifdef __WIN32__
		::censoc::netcpu::ping_exit();
#endif

		try {

			::censoc::scheduler::idle();

			if (argc > 3)
				throw ::std::runtime_error("no longer using CLI for args, instead use cfg file (implicitly of the same basename as the binary/exe). The only allowed arg now is the cpuaffinity expressed as zero-based number.");
			else if (argc == 3) {
				::censoc::netcpu::cpu_affinity_index = ::censoc::lexicast<unsigned>(argv[1]);
				::censoc::netcpu::clone_instance_index = ::censoc::lexicast<unsigned>(argv[2]);
#ifndef __WIN32__
				cpuset_t set;
				CPU_ZERO(&set);
				CPU_SET(::censoc::netcpu::cpu_affinity_index, &set);
				if (::cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(set), &set))
					::std::cerr << "FAILED TO SET CPU AFF: " << ::censoc::netcpu::cpu_affinity_index << ::std::endl;
#else
				::SetThreadAffinityMask(::GetCurrentThread(), (1 << ::censoc::netcpu::cpu_affinity_index));
				::SetProcessAffinityMask(::GetCurrentProcess(), (1 << ::censoc::netcpu::cpu_affinity_index));
#endif

			} else 
				throw ::std::runtime_error("processor *needs* cpu affinity and clone index specified on CLI indeed");

			::censoc::netcpu::ownpath = *argv;
#ifdef __WIN32__
			::censoc::netcpu::should_i_sleep.init();
#endif
			::censoc::netcpu::run();
		} catch (::std::exception const & e) {
			::std::cout.flush();
			::censoc::llog() << ::std::endl;
			::std::cerr << "Runtime error: [" << e.what() << "]\n";
			//return -1;
		} catch (...) {
			::std::cout.flush();
			::censoc::llog() << ::std::endl;
			::std::cerr << "Unknown runtime error.\n";
			//return -1;
		}
	}
	// return 0;
}

#include "multi_logit/processor.h"
#include "gmnl_2a/processor.h"
#include "gmnl_2/processor.h"
#include "logit/processor.h"
#include "mixed_logit/processor.h"
