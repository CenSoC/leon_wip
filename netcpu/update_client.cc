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

#include <windows.h>
#include <shellapi.h>
#include <objbase.h>

#endif

#include <sys/stat.h>
#include <sys/types.h>


#ifdef __FreeBSD__
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#endif

#include <openssl/sha.h>

#include <string>

#include <cstdlib>
#include <iostream>
#include <set>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <censoc/lexicast.h>
#include <censoc/arrayptr.h>
#include <censoc/sha_file.h>
#include <censoc/sysinfo.h>
#include <censoc/scheduler.h>

namespace censoc { namespace netcpu { 
	typedef uint_fast64_t size_type;
	::boost::asio::io_service static io;
	::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);
	::boost::system::error_code static ec;
	::boost::filesystem::path static ownpath;
	::std::string static cpu_description;

	// @note 'sys' is currently mainly needed for trouble-shooting (e.g. NtQuerySysInfo not being implemented as expected due to its "do not use" advise, etc.) -- will be easy to tell what system the client processor is running (linux, freebsd, xp, 7, vista, etc. etc. etc.)
	// @note also later -- this may prove to be of great benefit w.r.t. migrating updating the "update_client.exe" itself -- e.g. if user had d/l(ed) 32bit install and used it on 64bit system -- so this string could be used to automatically update the "update_client.exe" to the 64 bit version -- TODO for later (i.e. see __SIZEOF_POINTER__ bit of code below)! Just have to 100% sure when parsing sys_description and deciding to "upgrate" the updatie_client to 64 bit -- if such decision is wrong then the whole client will no longer be usable!
	::std::string static sys_description;

	bool static quit_flag(false);
	int static ncpus(0);
	::std::set<unsigned> static excluded_cpus;

#ifndef __WIN32__
	pid_t nosleep;
	::std::vector<pid_t> processors;
#else
	::PROCESS_INFORMATION nosleep;
	::std::vector< ::PROCESS_INFORMATION> processors;
#endif

	censoc::sha_file<size_type> static sha;
	censoc::sha_file<size_type> sha_tmp;

}}

#include "message.h"
#include "io_driver.h"

#include "io_wrapper.h"

#include "update_common.h"

namespace censoc { namespace netcpu { 

struct peer_connection;

typedef censoc::lexicast< ::std::string> xxx;

// underscore is because 'interface' is a macro in objbase.h->basetyps.h in windows
struct _interface {
	// note -- not using "(int argc, char * * argv)" because in win, it is very hard to have an initial batch file running w/o any console window being displayed... so reading from a config file... not ideal, but this is an exceptional situation more or less...
	// also --  things like filename for certificate(s) -- those on some systems may be relevant w.r.t. CWD (current working dir) which may or may not be the same as the dir for the binary itself... yet the cfg/args may not always impose the absolute path (or indeed know of it) -- so even if starting from a batch file things like 'processor.exe' -- the batch/shell stcript may have an 'indeterminate' CWD on the target processing machin (from the stapoint of the script-generating server).
	_interface()
	: endpoints(io) {
		::std::string ssl_certificate;

#if 0
		// now using a non-config_file setup. hardcoded values are just as good (even better in fact -- ensuring atomicity w.r.t. self-updates and possibility of different servers for different versions during selfupdate process) -- provided the git/svn/cvs source version control system is being delpoyed (or a similar process) -- which is should be...
		::boost::filesystem::path config_filepath(netcpu::ownpath.parent_path());
		config_filepath /= "censoc_netcpu_update_client.cfg";
		::std::ifstream config_file(config_filepath.string().c_str());

		if (config_file) {
#endif
		// todo -- later will have a series of servers ...
		// while (config_file) 

			::std::string const server_at("netcpu.zapto.org:8877"), cert_filename("certificate.pem"), alternative_cert_filename("new_certificate.pem");
			//::std::string const server_at("localhost:8889"), cert_filename("certificate.pem"), alternative_cert_filename("new_certificate.pem");
#if 0
			// see above for why non-config_file setup is being used now...
			config_file >> server_at;
			config_file >> cert_filename;
			config_file >> alternative_cert_filename;
			if (server_at.empty() == false && cert_filename.empty() == false) {
#endif


				::std::cerr << "server_at: " << server_at << ", cert: " << cert_filename << ::std::endl;

				censoc::array<char, size_type> server_at_c_str(server_at.size() + 1);
				::strcpy(server_at_c_str, server_at.c_str());

				char const * const host(::strtok(server_at_c_str, ":"));
				if (host == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value(=" << server_at << ") need 'host:port'");

				char const * const port = ::strtok(NULL, ":");
				if (port == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value: [" << server_at << "] need 'host:port'");

				::std::cerr << "host: " << host << ", port: " << port << ::std::endl;

				endpoints_i = endpoints.resolve(::boost::asio::ip::tcp::resolver::query(host, port));

				::boost::filesystem::path ssl_certificate_filepath(netcpu::ownpath.parent_path());
				ssl_certificate_filepath /= cert_filename;
				ssl_certificate = ssl_certificate_filepath.string();

#if 0
			// see above for why non-config_file setup is being used now...
				if (alternative_cert_filename.empty() == false) {
#endif
					::boost::filesystem::path alternative_ssl_certificate_filepath(netcpu::ownpath.parent_path());
					alternative_ssl_certificate_filepath /= alternative_cert_filename;
					alternative_ssl_certificate = alternative_ssl_certificate_filepath.string();
#if 0
				}
			// see above for why non-config_file setup is being used now...
			}
		}
#endif

		if (endpoints_i == ::boost::asio::ip::tcp::resolver::iterator() || ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply server_at (in a host:port form) followed by certificate_filename (relevant to the bin's dir)");

		ssl.set_verify_mode(::boost::asio::ssl::context::verify_peer);
		ssl.load_verify_file(ssl_certificate);
	}
	::boost::asio::ip::tcp::resolver endpoints;
	::boost::asio::ip::tcp::resolver::iterator endpoints_i;

	// tmp hack...
	::std::string alternative_ssl_certificate;
};

/**
	@NOTE -- a bit of a hack indeed (better to find existing processes, get their pids/handles and then nuke them explicitly)
	*/
void static
nuke_app(
#ifndef __WIN32__
	pid_t & meta
#else
	::PROCESS_INFORMATION & meta
#endif
)
{
#ifndef __WIN32__
	if (meta == static_cast<pid_t>(0))
		return;
	else while (::kill(meta, SIGKILL)) {
		::std::cerr << "failed to terminate process, nothing much could be done -- will try again...";
		censoc::scheduler::sleep_s(3);
	}
	censoc::scheduler::sleep_s(3);
	int tmp;
	while (meta != ::waitpid(meta, &tmp, 0)) {
		::std::cerr << "failed to waitpid, nothing much could be done -- will try again...";
		censoc::scheduler::sleep_s(3);
	}
	meta = 0;
#else
	if (!meta.hProcess)
		return;
	else {
		if (::WaitForSingleObject(meta.hProcess, 15000) != WAIT_OBJECT_0)
			while (!::TerminateProcess(meta.hProcess, 0) && ::WaitForSingleObject(meta.hProcess, 0) != WAIT_OBJECT_0) {
				::std::cerr << "failed to terminate process, nothing much could be done -- will try again...";
				censoc::scheduler::sleep_s(3);
			}
		::CloseHandle(meta.hThread);
		::CloseHandle(meta.hProcess);
		meta.hProcess = 0;
	}
#endif
}

void static
launch_app(
#ifndef __WIN32__
	pid_t & meta
#else
	::PROCESS_INFORMATION & meta
#endif
	, ::std::string const & app_filepath, ::std::string const & one_arg = "")
{
	if (::boost::filesystem::exists(app_filepath) == false) {
		//throw ::std::runtime_error("the app to be launched does not appear to exist on the filesystem: [" + app_filepath + ']');
		return;
	}
#ifndef __WIN32__
	if (meta)
		return;
	meta = ::fork();
	if (meta == static_cast<pid_t>(-1)) {
		meta = 0;
		throw ::std::runtime_error("could not fork [" + app_filepath + ']');
	} else if (meta == static_cast<pid_t>(0)) {
		char * args[3];
		args[0] = const_cast<char * const>(app_filepath.c_str());
		if (one_arg.empty() == false)
			args[1] = const_cast<char * const>(one_arg.c_str());
		else
			args[1] = NULL;
		args[2] = NULL;
		::execv(args[0], args);
		::exit(0);
	}
#else
	if (meta.hProcess)
		return;
	::STARTUPINFO tmp; ::memset(&tmp, 0, sizeof(tmp)); tmp.cb = sizeof(tmp); 
	if (!::CreateProcess(NULL, const_cast<CHAR *>(one_arg.empty() == true ? app_filepath.c_str() : (app_filepath + ' ' + one_arg).c_str()), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &tmp, &meta) || !meta.hProcess) {
		meta.hProcess = 0;
		throw ::std::runtime_error("could not CreateProcess [" + app_filepath + ']');
	}
#endif
}

#ifdef __WIN32__
void static
launch_nosleep()
{
	launch_app(nosleep, (netcpu::ownpath.parent_path() / "censoc_netcpu_nosleep.exe").string());
}
#endif

void static
launch_processors()
{
	assert(ncpus);

	::std::string const processor_filepath((netcpu::ownpath.parent_path() / "censoc_netcpu_processor.exe").string());
	for (int i(0); i != ncpus; ++i) 
		if (netcpu::excluded_cpus.find(i) == netcpu::excluded_cpus.end())
			launch_app(processors[i], processor_filepath, censoc::lexicast< ::std::string>(i));
}

// NOTE: largely copy from 'processor.cc'...
struct peer_connection : public netcpu::io_wrapper<netcpu::message::async_driver> {

	::boost::filesystem::path target_path;

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
		io().write(msg, &peer_connection::on_version_write, this);
	}
	
	void
	on_version_write()
	{
		// todo -- write own metadata (os, h/w arch, cpu type etc.)
		netcpu::update::message::processor_id msg;
		msg.cpuinfo.resize(cpu_description.size());
		::memcpy(msg.cpuinfo.data(), cpu_description.data(), cpu_description.size());
		msg.sysinfo.resize(sys_description.size());
		::memcpy(msg.sysinfo.data(), sys_description.data(), sys_description.size());
#ifdef __FreeBSD__
		msg.sys(netcpu::update::message::processor_id::sys_type::freebsd);
#elif defined(__WIN32__)
		msg.sys(netcpu::update::message::processor_id::sys_type::win);
#else
		msg.sys(netcpu::update::message::processor_id::sys_type::unknown);
#endif

		// @NOTE currently detecting this (64 vs 32 bit mem models) by compilation target. if the user will prove to be unable to d/l the correct version and subsequently will end up running 32bit app where 64bit is advised/possible, then will need to parse the system-info string
#if __SIZEOF_POINTER__ == 8
		msg.ptr_size(netcpu::update::message::processor_id::ptr_size_type::eight);
#else
		msg.ptr_size(netcpu::update::message::processor_id::ptr_size_type::four);
#endif
		io().write(msg, &peer_connection::on_write, this);
	}

	void 
	on_write()
	{
		io().read();
	}

	void 
	on_read()
	{
		netcpu::message::id_type const id(io().read_raw.id());
		switch (id) {
		case netcpu::message::bad::myid :
		{
			throw netcpu::message::exception(xxx() << "peer rejected last message: [" << io().socket.lowest_layer().remote_endpoint(netcpu::ec) << '\n');
		}
		break;
		case netcpu::update::message::meta::myid :
		{
			netcpu::update::message::meta msg;
			msg.from_wire(io().read_raw);

			// construct path to a target file
			::std::string const filename(msg.filename.data(), static_cast< ::std::size_t>(msg.filename.size()));
			target_path = netcpu::ownpath.parent_path();
			target_path /= filename;

			// determine if needs to be downloaded
			if (::boost::filesystem::exists(target_path) == true && !::memcmp(sha_tmp.calculate(target_path.string()), msg.hush.data(), sha_tmp.hushlen))
				return io().write(netcpu::message::bad());
			else {
				sha_tmp = msg.hush.data();
				return io().write(netcpu::message::good());
			}
		}
		break;
		case netcpu::update::message::bulk::myid :
		{
			assert(target_path.string().empty() == false);

			netcpu::update::message::bulk msg;
			msg.from_wire(io().read_raw);

			::std::string const filename(target_path.filename().string());

			::boost::filesystem::path tmp_target_path;
			bool self_update;
			if (filename == "censoc_netcpu_update_client.exe") {
				self_update = true;
				tmp_target_path = target_path.parent_path();
				tmp_target_path /= "tmp_" + filename;
			} else { 
				self_update = false;
				tmp_target_path = target_path;
			}

			while (!0) {
				::std::ofstream tmp_target_file(tmp_target_path.string().c_str(), ::std::ios::binary | ::std::ios::trunc);
				tmp_target_file.write(msg.data.data(), static_cast< ::std::streamsize>(msg.data.size()));
				tmp_target_file.flush();
				if (tmp_target_file) 
					break;
				else {
					::std::cerr << "failed to write target file: [" << tmp_target_path << "]\n";
					censoc::scheduler::sleep_s(3);
				}
			}

#ifndef __WIN32__
			mode_t tmp_target_filemode;
			if (tmp_target_path.extension() == ".exe")
				tmp_target_filemode = 0700;
			else
				tmp_target_filemode = 0600;
			if (::chmod(tmp_target_path.string().c_str(), tmp_target_filemode))
				throw ::std::runtime_error(xxx("unable to chmod on: [") << tmp_target_path << ']');
#endif

			if (self_update == true) {

				::boost::filesystem::path tmp_shatarget_path(tmp_target_path);
				tmp_shatarget_path.replace_extension(".sha");
				// @note -- sha storage is used (as opposed to 'move' semantics) because on loseros (before Vista, such as XP et al) there is no atomic file move that is documented and therefore guaranteed (as opposed to POSIX rename(2)). 'MoveFileTransacted' for example is only on Vista onwards...
				sha_tmp.store(tmp_shatarget_path.string());

				// hack -- re-using/clobbering nosleep handle/pid... but that's ok (process is going to exit anyways) (and nosleep should already be nuked...?)
				launch_app(nosleep, tmp_target_path.string());
				quit_flag = true;
				throw ::std::runtime_error("sensed self update, quitting the old self");
			} 
		}
		break;
		case netcpu::message::end_process::myid :
		{
			netcpu::launch_processors();
#ifdef __WIN32__
			netcpu::launch_nosleep();
#endif
		}
		break;
		default:
			throw netcpu::message::exception(xxx() << "inappropriate message: [" << id << ']');
		}
		io().read();
	}
	~peer_connection() throw();
};

int static
run(char const * ownpath) 
{
	assert(ownpath != NULL);

	netcpu::ownpath = ownpath;
	::std::string const filename(netcpu::ownpath.filename().string());

	sha.calculate(netcpu::ownpath.string());

	// if I am a tmp (being relpaced)...
	if (filename == "tmp_censoc_netcpu_update_client.exe") {

		// TODO -- delete this note: this var was outside the if statement due to non-tmp app writing its own sha to file... but this is slated for deprecation...
		::boost::filesystem::path ownshapath(netcpu::ownpath);
		ownshapath.replace_extension(".sha");

		// sha checksum for myself should exist
		if (::boost::filesystem::exists(ownshapath) == false) 
			return 0;

		// sha checksum for myself should match
		if (::memcmp(sha.get(), sha_tmp.load(ownshapath.string()), sha.hushlen)) 
			return 0;

		::boost::filesystem::path target_path(netcpu::ownpath.parent_path());
		target_path /= filename.substr(4);

		// if proper version is the same -- no need to do anything more... (TODO -- may be later do the actual binary comparison, but then -- if everything else is currently based so heavily on checksums/digests, then might as well reuse the api)
		if (!::memcmp(sha.get(), sha_tmp.calculate(target_path.string()), sha.hushlen)) 
			return 0;

		censoc::llog() << "I am a tmp_self -- must be from self_update... will start the proper version...\n";

		while (!0) {
			try {
				// not using 'rename' but rather 'copy' because in winapi open app locks it's exe file implicitly (won't allow move/rename to delete the 'from')
				// moreover some boost versions copy_file implementation is buggy as it does not truncate the file properly if target file exists and is smaller than source... so will delete first
				//::boost::filesystem::copy_file(netcpu::ownpath, target_path, ::boost::filesystem::copy_option::overwrite_if_exists);
				::boost::filesystem::remove(target_path);
				::boost::filesystem::copy_file(netcpu::ownpath, target_path);

#ifndef __WIN32__
				if (::chmod(target_path.string().c_str(), 0700))
					throw ::std::runtime_error(xxx("unable to chmod exe in self_update on: [") << target_path << ']');
#endif

			} catch (::boost::filesystem::filesystem_error const & e) {
				::std::cerr << "boost remove then copy failed: [" << e.what() << "]\n";
				censoc::scheduler::sleep_s(3);
				continue;
			}
			break;
		} 

		do {
			try {
				// no need to worry about nosleep pid/handle -- this process is going to exit anyways...
				launch_app(nosleep, target_path.string());
			} catch (...) {
				::std::cerr << "Failed to start the real update_client from tmp exec. No other option but to keep trying...\n";
				censoc::scheduler::sleep_s(3);
				continue;
			}
			return 0;
		} while (!0);

	} else { // i am the real app (not tmp)

		// if tmp exists and valid but is not the same as me -- stop runnnig...
		{
			::boost::filesystem::path tmp_target_path(netcpu::ownpath.parent_path());
			tmp_target_path /= "tmp_censoc_netcpu_update_client.exe";

			::boost::filesystem::path tmp_shatarget_path(tmp_target_path);
			tmp_shatarget_path.replace_extension(".sha");

			censoc::sha_file<size_type> sha_tmp_tmp;
			if (::boost::filesystem::exists(tmp_target_path) == true && ::boost::filesystem::exists(tmp_shatarget_path) == true && !::memcmp(sha_tmp.calculate(tmp_target_path.string()), sha_tmp_tmp.load(tmp_shatarget_path.string()), sha.hushlen) && ::memcmp(sha.get(), sha_tmp.get(), sha.hushlen))
				return 0;
		}

		// load excluded cpus if relevant
		::censoc::netcpu::excluded_cpus.clear();
		::boost::filesystem::path excluded_cpus_path(netcpu::ownpath.parent_path());
		excluded_cpus_path /= "excluded_cpus.txt";

		if (::boost::filesystem::exists(excluded_cpus_path) == true) {
			::std::ifstream excluded_cpus_file(excluded_cpus_path.string().c_str()); 
			if (excluded_cpus_file == false)
				throw ::std::runtime_error("cannot access file=(" + excluded_cpus_path.string() + ")");
			::std::string excluded_cpus_file_line;
			::std::string csv_token;
			while (::std::getline(excluded_cpus_file, excluded_cpus_file_line)) {
				typedef ::boost::tokenizer< ::boost::escaped_list_separator<char> > tokenizer_type;
				tokenizer_type tokens(excluded_cpus_file_line);
				for (tokenizer_type::const_iterator i(tokens.begin()); i != tokens.end(); ++i) {
					csv_token = *i;
					::boost::trim(csv_token);
					if (csv_token.empty() == false) { 
						::std::size_t range_index(csv_token.find(':'));
						if (range_index != ::std::string::npos) {
							unsigned from(::boost::lexical_cast<unsigned>(csv_token.substr(0, range_index)));
							unsigned to(::boost::lexical_cast<unsigned>(csv_token.substr(range_index + 1)));
							for (unsigned i(from); i <= to; ++i)
								netcpu::excluded_cpus.insert(i);
						} else 
							netcpu::excluded_cpus.insert(::boost::lexical_cast<unsigned>(csv_token));
					}
				}
			}
		}

		// debug
		::std::cout << "found excluded cpus: \n";
		for (::std::set<unsigned>::const_iterator i(netcpu::excluded_cpus.begin()); i != netcpu::excluded_cpus.end(); ++i) {
			::std::cout << *i << ::std::endl;
		}


#if 0
		// TODO -- delete this as I think there is no need for it any longer
		// if no sha -- write one.
		if (::boost::filesystem::exists(ownshapath) == false) 
			sha.store(ownshapath.string());
#endif

		// SAVE THE CPU-DESCRIPTION... etc.
		{
#ifdef __FreeBSD__

			// e.g. hw.model: Intel(R) Core(TM)2 Duo CPU     E8400  @ 3.00GHz
			int mib[4];
			size_t mib_len(4);
			if (::sysctlnametomib("hw.model", mib, &mib_len))
				throw ::std::runtime_error("could not sysctlnametomib");
			size_t len;
			if (::sysctl(mib, mib_len, NULL, &len, NULL, 0)) 
				throw ::std::runtime_error("could not sysctl");
			censoc::array<char, size_type> data(len);
			if (::sysctl(mib, 2, data, &len, NULL, 0))
				throw ::std::runtime_error("could not sysctl");
			netcpu::cpu_description = ::std::string(data, len);

			utsname sys_utsname;
			if (!::uname(&sys_utsname))
				netcpu::sys_description = ::std::string(sys_utsname.sysname) + " " + ::std::string(sys_utsname.release) + " " + ::std::string(sys_utsname.version) + " " + ::std::string(sys_utsname.machine); 
			else
				netcpu::sys_description = "failed to obtain"; 

#elif defined __WIN32__

			// with only the sysinfo.wProcessorArchitecture && sysinfo.wProcessorLevel && sysinfo.wProcessorRevision vars it is impossible to distinguish betwenn e8400 and q9400, other feature-preset register values may help, but will yield more platform-specific parsing, thusly may as well read the registry for it (string-textual representation is then 'common' w.r.t. other platforms using sysctl calls)...
#if 0

			::SYSTEM_INFO sysinfo;
			::GetSystemInfo(&sysinfo);

			netcpu::cpu_description = xxx("wProcessorArchitecture: [") << sysinfo.wProcessorArchitecture << "], wProcessorLevel: [" << sysinfo.wProcessorLevel << "] dwNumberOfProcessors: [" << sysinfo.dwNumberOfProcessors << "] wProcessorRevision: [" << sysinfo.wProcessorRevision << "] model: [" << ((sysinfo.wProcessorRevision & 0xff00) >> 8) << "] stepping: [" << (sysinfo.wProcessorRevision & 0xff) << ']'; 

			if (!sysinfo.wProcessorArchitecture && sysinfo.wProcessorLevel == 6 && sysinfo.wProcessorRevision == 5898) 
				netcpu::cpu_description += " composed one: intel core2 e8400";
			else if (!sysinfo.wProcessorArchitecture && sysinfo.wProcessorLevel == 6 && sysinfo.wProcessorRevision == 6661)
				netcpu::cpu_description += " composed one: intel xeon w3505";
#else
			netcpu::cpu_description = "unknown"; 
			HKEY key;
			DWORD buflen(0);
			DWORD type;
			// ::RegGetValue is not, currently, mingw32 api, although it is in mingw64... which could produce 32-bit runtime libs... so TODO -- convert to simpler api later on (i.e. after migrating my 32 win cross-compiler to mingw64)!
			if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
				if (::RegQueryValueEx(key, "ProcessorNameString", NULL, NULL, NULL, &buflen) == ERROR_SUCCESS) {
					if (buflen) {
						censoc::array<BYTE, size_type> buf(buflen);
						if (::RegQueryValueEx(key, "ProcessorNameString", NULL, &type, buf, &buflen) == ERROR_SUCCESS){
							if (type == REG_SZ) 
								netcpu::cpu_description = reinterpret_cast<char const *>(static_cast<BYTE const *>(buf));
						}
					}
				}
				::RegCloseKey(key);
			}
			netcpu::sys_description = " "; 
			if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
				if (::RegQueryValueEx(key, "ProductName", NULL, NULL, NULL, &buflen) == ERROR_SUCCESS) {
					if (buflen) {
						censoc::array<BYTE, size_type> buf(buflen);
						if (::RegQueryValueEx(key, "ProductName", NULL, &type, buf, &buflen) == ERROR_SUCCESS){
							if (type == REG_SZ) 
								netcpu::sys_description += reinterpret_cast<char const *>(static_cast<BYTE const *>(buf)) + ::std::string(" ");
						}
					}
				}
#if 0
				if (::RegQueryValueEx(key, "ProductId", NULL, NULL, NULL, &buflen) == ERROR_SUCCESS) {
					if (buflen) {
						censoc::array<BYTE, size_type> buf(buflen);
						if (::RegQueryValueEx(key, "ProductId", NULL, &type, buf, &buflen) == ERROR_SUCCESS){
							if (type == REG_SZ) 
								netcpu::sys_description += reinterpret_cast<char const *>(static_cast<BYTE const *>(buf)) + ::std::string(" ");
						}
					}
				}
#endif
				if (::RegQueryValueEx(key, "CurrentVersion", NULL, NULL, NULL, &buflen) == ERROR_SUCCESS) {
					if (buflen) {
						censoc::array<BYTE, size_type> buf(buflen);
						if (::RegQueryValueEx(key, "CurrentVersion", NULL, &type, buf, &buflen) == ERROR_SUCCESS){
							if (type == REG_SZ) 
								netcpu::sys_description += reinterpret_cast<char const *>(static_cast<BYTE const *>(buf)) + ::std::string(" ");
						}
					}
				}
				if (::RegQueryValueEx(key, "CurrentBuildNumber", NULL, NULL, NULL, &buflen) == ERROR_SUCCESS) {
					if (buflen) {
						censoc::array<BYTE, size_type> buf(buflen);
						if (::RegQueryValueEx(key, "CurrentBuildNumber", NULL, &type, buf, &buflen) == ERROR_SUCCESS){
							if (type == REG_SZ) 
								netcpu::sys_description += reinterpret_cast<char const *>(static_cast<BYTE const *>(buf)) + ::std::string(" ");
						}
					}
				}
				::RegCloseKey(key);
			}
			if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
				if (::RegQueryValueEx(key, "PROCESSOR_ARCHITECTURE", NULL, NULL, NULL, &buflen) == ERROR_SUCCESS) {
					if (buflen) {
						censoc::array<BYTE, size_type> buf(buflen);
						if (::RegQueryValueEx(key, "PROCESSOR_ARCHITECTURE", NULL, &type, buf, &buflen) == ERROR_SUCCESS){
							if (type == REG_SZ) 
								netcpu::sys_description += reinterpret_cast<char const *>(static_cast<BYTE const *>(buf));
						}
					}
				}
				::RegCloseKey(key);
			}
#endif

#endif
			{
				::boost::filesystem::path nickname_filepath(netcpu::ownpath.parent_path());
				nickname_filepath /= "nickname.txt";
				if (::boost::filesystem::exists(nickname_filepath) == true) {
					::std::ifstream nickname_file(nickname_filepath.string().c_str());
					if (nickname_file) {
						::std::string nickname;
						nickname_file >> nickname;
						if (nickname.empty() == false) {
							nickname = nickname.substr(0, 50);
						} else
							nickname = "undefined";
						netcpu::sys_description += " nickname(=" + nickname + ')';
					}
				}
			}

			netcpu::ncpus = censoc::sysinfo::cpus_size(); // will invoke extra ::GetSystemInfo call in loser-os, but that's ok (not a bottle-neck)... later will write a variant of the call to get more info at once...

#ifndef __WIN32__
			nosleep = 0;
			processors.resize(netcpu::ncpus, 0);
#else
			::memset(&nosleep, 0, sizeof(nosleep)); 
			::PROCESS_INFORMATION tmp; 
			::memset(&tmp, 0, sizeof(tmp)); 
			processors.resize(netcpu::ncpus, tmp);
#endif

			netcpu::cpu_description += xxx(" and cpus discovered: ") << netcpu::ncpus << " and ram discoreverd: " << censoc::sysinfo::ram_size() << " ";

			censoc::llog() << "cpu: " << netcpu::cpu_description << ::std::endl;
			censoc::llog() << "cpus: " << netcpu::ncpus << ::std::endl;

		}

		while (quit_flag == false) {
			try {

				censoc::scheduler::sleep_s(10);

				// nuke processors
#ifdef __WIN32__
				// ideally 'TerminateProcess' in loser-os should be as robust as signal/raise/kill as in Unix-like oses (inclusive of people not writing system-wide dll/shared-libs which are robust w.r.t. global data and unexpectedly terminating processes...) alas shall be "nice" to loser-os
				for (int i(0); i != ncpus; ++i) 
					if (processors[i].hProcess)
						::PostThreadMessage(processors[i].dwThreadId, WM_QUIT, 0, 0);
#endif
				for (int i(0); i != ncpus; ++i) 
					nuke_app(processors[i]);
#ifdef __WIN32__
				// @todo -- note that either nosleep.exe must be compiled with the exact (i.e. generic-32 -64 bit arch compatible) flags as this app itself happens to be(i.e. update_client) -- which is what is happening right now; OR one must not allow the exceptions to be propagated here. For instance, if/when user puts the drive into a different (slightly) architecture system (still same 32/64 bit etc, but different processor features) -- then heavily-optimized binary (e.g. processor.exe) may fail. Update_client must then be able to connect and re-download proper version/update; but this means the update_client should not get thrown exceptions before connecting from launching possibly-inocompatible apps.
				netcpu::launch_nosleep();
#endif

				censoc::scheduler::idle();


				::boost::scoped_ptr<_interface> ui(new _interface);

				::boost::shared_ptr<netcpu::message::async_driver> new_driver;
				netcpu::peer_connection * new_peer;
				for (bool alternative_tried(false);;) {
					new_driver.reset(new netcpu::message::async_driver);
					new_peer = new peer_connection(*new_driver); // another way would be to do shared_ptr(this) in ctor of peer_connection (but then one must take extra bit of care for anything that follows this in ctor, and potentially derived types, not to throw at all)... this way, however, can throw all one wants...

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
					if (ec) {
						if (alternative_tried == false && ui->alternative_ssl_certificate.empty() == false && ::boost::filesystem::exists(ui->alternative_ssl_certificate) == true) {
							new_driver->socket.lowest_layer().close();
							netcpu::ssl.load_verify_file(ui->alternative_ssl_certificate);
							alternative_tried = true;
						} else
							throw ::std::runtime_error("could not complete the SSL handshake");
					} else
						break;
				}

				assert(new_driver.get() != NULL);
				assert(new_peer != NULL);

				new_driver->set_keepalive();

				ui.reset(); 

#ifdef __WIN32__
				// ideally 'TerminateProcess' in loser-os should be as robust as signal/raise/kill as in Unix-like oses (inclusive of people not writing system-wide dll/shared-libs which are robust w.r.t. global data and unexpectedly terminating processes...) alas shall be "nice" to loser-os
				if (nosleep.hProcess)
					::PostThreadMessage(nosleep.dwThreadId, WM_QUIT, 0, 0);
				//::SetThreadExecutionState(ES_SYSTEM_REQUIRED);
				nuke_app(nosleep);
#endif

				new_peer->run();
				new_driver.reset();
				io.run();
				// TODO -- this may not be needed!!!
				io.reset();
				::std::cerr << "run ended normally\n";
			} catch (::std::exception const & e) {
				::std::cout.flush();
				censoc::llog() << ::std::endl;
				::std::cerr << "Runtime error: [" << e.what() << "]\n";
			} catch (...) {
				::std::cout.flush();
				censoc::llog() << ::std::endl;
				::std::cerr << "Unknown runtime error.\n";
			}
		}
		return 0;
	}
}

peer_connection::~peer_connection() throw()
{
	censoc::llog() << "dtor in peer_connection" << ::std::endl;
}

}}

int 
main(int argc, char * * argv)
{
	::censoc::scheduler::sleep_s(3);
#ifdef __WIN32__
	//::SetErrorMode(-1); // let us not automatically babyseat the SEM_NOALIGNMENTFAULTEXCEPT
	::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

	::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
#endif
	if (argc != 1)
		throw ::std::runtime_error("no longer using CLI for args, instead use .cfg file (implicitly of the same basename as the binary/exe)");
	return ::censoc::netcpu::run(*argv);
}
