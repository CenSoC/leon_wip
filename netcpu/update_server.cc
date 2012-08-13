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

//{ lot of  includes...
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include <readpassphrase.h>

#include <string>
#include <cstdlib>
#include <iostream>
#include <set>
#include <map>
#include <list>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/regex.hpp>

#include <censoc/empty.h>
#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/opendir.h>
#include <censoc/scoped_membership_iterator.h>
#include <censoc/sha_file.h>

#include "types.h"
//}

namespace censoc { namespace netcpu { 

namespace message {
	class async_driver;
}
	
typedef uint_fast32_t size_type;
typedef censoc::lexicast< ::std::string> xxx;

::boost::system::error_code static ec;

::boost::asio::io_service static io;
::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);

::std::string static root_path;

size_type static total_clients(0);

}}

#include "message.h"
#include "io_driver.h"
#include "io_wrapper.h"
#include "update_common.h"

namespace censoc { namespace netcpu { 

struct interface {
	// TODO -- not very secure (but better than nothing for the time being). may leave data in process's address space if ::std::string is not reference-counted... (will need to work more on it later)...
	::std::string
	passwd() const
	{
		char buffer[100];
		char const * const rv(::readpassphrase("server password please...", buffer, sizeof(buffer), RPP_REQUIRE_TTY));

		if (rv == NULL)
			throw ::std::runtime_error("Could not read password...");

		::std::string const cp(rv);
		::memset(buffer, ::arc4random() & 255, sizeof(buffer));
		return cp;
	}

	::boost::asio::ip::tcp::resolver endpoints;
	::boost::asio::ip::tcp::resolver::iterator endpoints_i;

	interface(int argc, char * * argv)
	: endpoints(io) {

		ssl.set_options(::boost::asio::ssl::context::default_workarounds | ::boost::asio::ssl::context::no_sslv2 | ::boost::asio::ssl::context::single_dh_use);
		ssl.set_password_callback(::boost::bind(&interface::passwd, this));

		::std::string ssl_key;
		::std::string ssl_certificate;
		::std::string ssl_dh;

		for (int i(1); i != argc; ++i) {
			if (i == argc - 1)
				throw ::std::runtime_error(xxx() << "every argument must have a value: [" << argv[i] << ']');

			if (!::strcmp(argv[i], "--listen_at")) {
				char const * const host(::strtok(argv[++i], ":"));
				if (host == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --listen_at value: [" << argv[i] << "] need 'host:port'");
				char const * const port = ::strtok(NULL, ":");
				if (port == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --listen_at value: [" << argv[i] << "] need 'host:port'");
				endpoints_i = endpoints.resolve(::boost::asio::ip::tcp::resolver::query(
							::boost::asio::ip::tcp::v4(), // to generate faster DNS resolution (possibly), but lesser compatible (no IP6): todo -- comment out when IP4 gets deprecated :-)
							host, port));
			} else if (!::strcmp(argv[i], "--root")) {
				root_path = argv[++i];
				struct ::stat s;
				if (::stat(root_path.c_str(), &s) || !S_ISDIR(s.st_mode)) 
					throw ::std::runtime_error(xxx("directory is not valid: [") << root_path << ']');
				root_path += "/";
			} else if (!::strcmp(argv[i], "--key")) 
				ssl_key = argv[++i];
			else if (!::strcmp(argv[i], "--cert")) 
				ssl_certificate = argv[++i];
			else if (!::strcmp(argv[i], "--dh")) 
				ssl_dh = argv[++i];
			else
				throw ::std::runtime_error(xxx() << "unknown option: [" << argv[i] << ']');
		}

		if (ssl_key.empty() == true)
			throw ::std::runtime_error("must supply ssl private key file");
		else if (ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply ssl certificate file");
		else if (ssl_dh.empty() == true)
			throw ::std::runtime_error("must supply ssl dh file");
		else if (root_path.empty() == true)
			throw ::std::runtime_error("must supply --root path (where all of the tasks subdirectories will be written/stored)");

		ssl.use_certificate_chain_file(ssl_certificate);
		ssl.use_private_key_file(ssl_key, boost::asio::ssl::context::pem);
		ssl.use_tmp_dh_file(ssl_dh);
	}
};

template <netcpu::size_type Hushlen>
struct hushed_file {
	enum { hushlen = Hushlen };
	::std::string filepath;
	::std::string filename;
	uint8_t hush[hushlen];
	hushed_file(::std::string const & filepath, ::std::string const & filename, uint8_t const * hush)
	: filepath(filepath), filename(filename) {
		::memcpy(this->hush, hush, hushlen);
	}
};

struct peer_connection : public netcpu::io_wrapper { 

	typedef hushed_file<censoc::sha_file<netcpu::size_type>::hushlen> hushed_file_type;

	::std::list<hushed_file_type> tosend;

	peer_connection(netcpu::message::async_driver & io) 
	: io_wrapper(io) {
		censoc::llog() << "ctor in peer_connection, driver addr: " << &io << " Clients(=" << total_clients++ << ')' << ::std::endl;
	}

	~peer_connection() throw();

	void 
	on_handshake()
	{
		censoc::llog() << "on_handshake\n";
		io().set_keepalive();
		io().read(&peer_connection::on_version, this);
		censoc::llog() << "issued read request, pending read of io(): " << io().is_read_pending() << "\n";
	}

	void
	on_version()
	{
		if (io().read_raw.id() != netcpu::message::version::myid) 
			throw netcpu::message::exception("version message is missing ");
		else {
			netcpu::message::version msg;
			msg.from_wire(io().read_raw);
			if (msg.value() != 1) // TODO define symbolic const elsewhere...
				throw message::exception(xxx("incorrect version value: [") << msg.value() << ']');
			censoc::llog() << "peer connected: [" << io().socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n";
			io().read(&peer_connection::on_processor_id_read, this);
		}
	}

	void
	on_processor_id_read()
	{
		if (io().read_raw.id() != netcpu::update::message::processor_id::myid)
			throw message::exception("incorrect message (was expecting processor_id)");

		netcpu::update::message::processor_id msg;
		msg.from_wire(io().read_raw);

		// todo -- find the sys (freebsd, etc.) then cpu
		::std::string sysname;
		switch (msg.sys()) {
			case netcpu::update::message::processor_id::sys_type::freebsd :
				sysname = "freebsd";
			break;
			case netcpu::update::message::processor_id::sys_type::win :
				sysname = "lose";
			break;
		}

		if (msg.ptr_size() == netcpu::update::message::processor_id::ptr_size_type::eight)
				sysname += "_64";
		else 
				sysname += "_32";

		::std::string raw_cpuname(msg.cpuinfo.data(), msg.cpuinfo.size());
		::std::string raw_sysname(msg.sysinfo.data(), msg.sysinfo.size());

		::std::string cpuname("unknown");

#if 0
		"e8400"
		"e6550"
		"core2"
		"pentium4"
		"i386"
#endif


		::boost::regex e("intel", ::boost::regex_constants::icase);
#if 0
		... or if dont want to use case-insensitive (::boost::regex_constants::icase) grepping then do this:
		::std::transform(raw_cpuname.begin(), raw_cpuname.end(), raw_cpuname.begin(), ::tolower);
#endif
		if (::boost::regex_search(raw_cpuname, e) == true) {
			// cpuname = "i386";
			// this multi-"" breakage layout is for the sake of the vim's foldmethodi=marker ( { } ) (as syntax-based folding is too slow). w/o multi-line layout the foldmethod=marker is bearking
			e.assign("core(\\(tm\\))" "{" "0,1" "}" "2", ::boost::regex_constants::icase);
			if (::boost::regex_search(raw_cpuname, e) == true) { // core 2
				cpuname = "core2";
				e.assign("e6550", ::boost::regex_constants::icase);
				if (::boost::regex_search(raw_cpuname, e) == true) {
					cpuname = "e6550";
				} else if (
					e.assign("e8400", ::boost::regex_constants::icase),
					::boost::regex_search(raw_cpuname, e) == true
					) {
					cpuname = "e8400";
				} else if (
					e.assign("q9400", ::boost::regex_constants::icase),
					::boost::regex_search(raw_cpuname, e) == true
					) {
					cpuname = "q9400";
				}
			} else { // not core 2
				e.assign("xeon", ::boost::regex_constants::icase);
				if (::boost::regex_search(raw_cpuname, e) == true) { // xeon
					cpuname = "xeon";
					e.assign("w3505", ::boost::regex_constants::icase);
					if (::boost::regex_search(raw_cpuname, e) == true) {
						cpuname = "w3505";
					} else if (
						e.assign("e5504", ::boost::regex_constants::icase),
						::boost::regex_search(raw_cpuname, e) == true
						) {
						cpuname = "e5504";
					}
				}
			}
		}

		::std::cerr << "sys: " << sysname << ::std::endl;
		::std::cerr << "raw cpu: " << raw_cpuname << ::std::endl;
		::std::cerr << "cpu: " << cpuname << ::std::endl;
		::std::cerr << "raw sys: " << raw_sysname << ::std::endl;

		// load all of the cpu/sys relevant *files* into a queue thingy...
		::std::string const specific_path(netcpu::root_path + sysname + '_' + cpuname + '/');

		// load all of the top-level *files* into a queue thingy...
		censoc::autoclose_opendir topd(netcpu::root_path);
		for (struct ::dirent * i(::readdir(topd.dir)); i != NULL; i = ::readdir(topd.dir)) {

			::std::string const name(i->d_name);
			::std::string const ent_path(netcpu::root_path + name);

			censoc::llog() << "looking at: " << ent_path << ::std::endl;

			struct ::stat s;
			if (::stat(ent_path.c_str(), &s)) 
				throw ::std::runtime_error(xxx("could not stat: [") << ent_path << ']');
			else if (S_ISREG(s.st_mode)) {
				censoc::sha_file<netcpu::size_type> sha;
				tosend.push_back(hushed_file_type(ent_path, name, sha.calculate(ent_path)));
			}
		}

		size_type specific_files_count(0);
		if (::boost::filesystem::exists(specific_path) == true) {
			censoc::autoclose_opendir subd(specific_path);
			for (struct ::dirent * i(::readdir(subd.dir)); i != NULL; i = ::readdir(subd.dir)) {

				::std::string const name(i->d_name);
				::std::string const ent_path(specific_path + name);

				struct ::stat s;
				if (::stat(ent_path.c_str(), &s)) 
					throw ::std::runtime_error(xxx("could not stat: [") << ent_path << ']');
				else if (S_ISREG(s.st_mode)) {
					censoc::sha_file<netcpu::size_type> sha;
					tosend.push_back(hushed_file_type(ent_path, name, sha.calculate(ent_path)));
					++specific_files_count;
				}
			}
		}

		if (!specific_files_count) {
			censoc::llog() << "no repository files for the target platform: [" << specific_path << "] were found -- will not send any files.\n";
			io().read();
			return;
		}

		io().read_callback(&peer_connection::on_read, this);
		send_one_meta();
	}

	void 
	on_read()
	{
		netcpu::message::id_type const id(io().read_raw.id());
		switch (id) {
		case netcpu::message::good::myid :
		{
			assert(tosend.empty() == false);

			// send the file contents

			hushed_file_type & ram(tosend.front());
			netcpu::update::message::bulk bulk_msg;

			size_type const filesize(::boost::filesystem::file_size(ram.filepath));

			bulk_msg.data.resize(filesize);

			::std::ifstream file(ram.filepath.c_str(), ::std::ios::binary);
			file.read(bulk_msg.data.data(), filesize);

			io().write(bulk_msg, &peer_connection::on_bulk_write, this);

			tosend.pop_front();
		}
		break;
		case netcpu::message::bad::myid :
		{
			assert(tosend.empty() == false);
			tosend.pop_front();

			on_bulk_write();
		}
		break;

		default:
		censoc::llog() << "unexpected message: [" << id << "], ignoring.\n";
		}
	}

	void 
	on_bulk_write()
	{
		if (tosend.empty() == false) 
			send_one_meta();
		else
			io().write(netcpu::message::end_process(), &peer_connection::on_write, this);
	}

	void
	on_write()
	{
		io().read();
	}

private:
	void
	send_one_meta()
	{
		assert(tosend.empty() == false);

		hushed_file_type & ram(tosend.front());
		netcpu::update::message::meta meta_msg;

		meta_msg.filename.resize(ram.filename.size());
		::memcpy(meta_msg.filename.data(), ram.filename.data(), ram.filename.size());
		meta_msg.hush.resize(ram.hushlen);
		::memcpy(meta_msg.hush.data(), ram.hush, ram.hushlen);

		io().write(meta_msg, &peer_connection::on_write, this);
	}
};

struct acceptor : ::boost::enable_shared_from_this<netcpu::acceptor> {
	acceptor(::boost::asio::ip::tcp::resolver::iterator & endpoints_i)
	: newcon(io) {
		censoc::llog() << "ACCEPTOR CTOR begin\n";
		while (endpoints_i != ::boost::asio::ip::tcp::resolver::iterator()) {
			newcon.open(::boost::asio::ip::tcp::v4());
			newcon.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
			newcon.bind(*endpoints_i, netcpu::ec);
			if (!ec) 
				break;
			else {
				newcon.close();
				++endpoints_i;
			}
		}

		if (endpoints_i == ::boost::asio::ip::tcp::resolver::iterator())
			throw ::std::runtime_error(xxx() << "could not bind to address. must specify correct host:port in the '--listen_at' option");
		newcon.listen(300); // TODO!!!
	}

	void
	run()
	{
		censoc::llog() << "newing async driver\n";
		::boost::shared_ptr<netcpu::message::async_driver> new_driver(new netcpu::message::async_driver);
		censoc::llog() << "done newing async driver\n";
		censoc::llog() << "newing peer_connection\n";
		new peer_connection(*new_driver);
		censoc::llog() << "done newing peer_connection\n";
		newcon.async_accept(new_driver->socket.lowest_layer(), ::boost::bind(&acceptor::on_accept, shared_from_this(), new_driver, ::boost::asio::placeholders::error));
		censoc::llog() << "ACCEPTOR CTOR end\n";
	}

	void 
	on_accept(::boost::shared_ptr<netcpu::message::async_driver> driver, ::boost::system::error_code const & error) throw()
	{
		censoc::llog() << "ON_ACCEPT\n";
		if (!error) {
			driver->handshake();
			::boost::shared_ptr<netcpu::message::async_driver> new_driver(new netcpu::message::async_driver);
			new peer_connection(*new_driver);
			newcon.async_accept(new_driver->socket.lowest_layer(), ::boost::bind(&acceptor::on_accept, shared_from_this(), new_driver, ::boost::asio::placeholders::error));
		}
	}

	::boost::asio::ip::tcp::acceptor newcon;
};

void static
run(int argc, char * * argv)
{
	censoc::ptr<interface> ui(new interface(argc, argv));

	::boost::shared_ptr<netcpu::acceptor> acceptor_ptr(new acceptor(ui->endpoints_i)); 
	ui.reset(NULL);
	acceptor_ptr->run();
	io.run();
	acceptor_ptr.reset();
}

}}

#include "io_wrapper.pi"

namespace censoc { namespace netcpu { 

peer_connection::~peer_connection() throw()
{
	// if not being deleted by virtue of being taken-over
	if (io().being_taken_over == false) {
		io().being_taken_over = true; // to prevent re-newing of another peer_connection
		io().final_in_chain = true;
	}
	censoc::llog() << "dtor in peer_connection. Clients(=" << --total_clients - 1 << ')' << ::std::endl;
}

}}


int 
main(int argc, char * * argv)
{
	try {
		::censoc::netcpu::run(argc, argv);
	} catch (::std::exception const & e) {
		::std::cout.flush();
		::censoc::llog() << ::std::endl;
		::std::cerr << "Runtime error: [" << e.what() << "]\n";
		return -1;
	} catch (...) {
		::std::cout.flush();
		::censoc::llog() << ::std::endl;
		::std::cerr << "Unknown runtime error.\n";
		return -1;
	}
	return 0;
}

