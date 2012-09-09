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

// TODO!!! -- when the codebase/apps get more stable/concrete, then try to REDUCE THE CODEBASE based on COMMONALITY WITH processor.CC FILE/PROGRAM!!! (e.g. define commonly shared classes such as peer_connection et al)... (doing the whole inter-programmatic generalisation now will be a bit too soon and is more likely to result in a messy code rather than an elegant structure of information models).

#include <string>

#include <cstdlib>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <censoc/lexicast.h>

namespace censoc { namespace netcpu {
::boost::asio::io_service static io;
::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);
::boost::system::error_code static ec;
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

struct model_meta {
	netcpu::message::id_type id;
	enum action {task_offer, task_status};
	action what_to_do;
	typedef ::std::list<char const *> args_type;
	args_type args;
};
::std::list<model_meta> static model_metas;

struct peer_connection;

typedef censoc::lexicast< ::std::string> xxx;

struct interface {
	interface(int argc, char * * argv)
	: endpoints(io) {
		::std::string server_ssl_certificate;
		::std::string client_ssl_certificate;
		::std::string client_ssl_key;
		for (int i(1); i != argc; ++i) {
			if (i == argc - 1)
				throw ::std::runtime_error(xxx() << "every argument must have a value: [" << argv[i] << "]");

			if (!::strcmp(argv[i], "--server_at")) {
				char const * const host(::strtok(argv[++i], ":"));
				if (host == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value: [" << argv[i] << "] need 'host:port'");
				char const * const port = ::strtok(NULL, ":");
				if (port == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value: [" << argv[i] << "] need 'host:port'");
				endpoints_i = endpoints.resolve(::boost::asio::ip::tcp::resolver::query(
							::boost::asio::ip::tcp::v4(), // to generate faster DNS resolution (possibly), but lesser compatible (no IP6): todo -- comment out when IP4 gets deprecated :-)
							host, port));
			} else if (!::strcmp(argv[i], "--server_cert")) 
				server_ssl_certificate = argv[++i];
			else if (!::strcmp(argv[i], "--client_cert")) 
				client_ssl_certificate = argv[++i];
			else if (!::strcmp(argv[i], "--client_key")) 
				client_ssl_key = argv[++i];
			else if (!::strcmp(argv[i], "--model")) {
				char const * const id(::strtok(argv[++i], ":"));
				if (id == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --model value: [" << argv[i] << "] need 'id:action'");
				char const * const action = ::strtok(NULL, ":");
				if (action == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --model value: [" << argv[i] << "] need 'id:action'");

				model_metas.push_back(netcpu::model_meta());
				model_metas.back().id = ::censoc::lexicast<unsigned>(id);

				if (!::strcmp(action, "offer")) 
					model_metas.back().what_to_do = model_meta::task_offer;
				else if (!::strcmp(action, "status")) 
					model_metas.back().what_to_do = model_meta::task_status;
				else
					throw ::std::runtime_error(xxx() << "incorrect --model subvalue for action: [" << action << "]");

			} else if (model_metas.empty() == false) {
				model_metas.back().args.push_back(argv[i]);
				model_metas.back().args.push_back(argv[++i]);
			} else 
				throw ::std::runtime_error(xxx() << "unknown option: [" << argv[i] << "]");
		}

		if (model_metas.empty() == true)
			throw ::std::runtime_error("must supply at least one --model option");

		if (endpoints_i == ::boost::asio::ip::tcp::resolver::iterator())
			throw ::std::runtime_error("must supply --server_at option with correct host:port values");

		if (server_ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply server ssl certificate file (via --server_cert option)");

		if (client_ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply client ssl certificate file (via --client_cert option)");

		if (client_ssl_key.empty() == true)
			throw ::std::runtime_error("must supply client ssl key file (via --client_key option)");

		ssl.set_verify_mode(::boost::asio::ssl::context::verify_peer | ::boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		ssl.load_verify_file(server_ssl_certificate);

		ssl.use_certificate_file(client_ssl_certificate, boost::asio::ssl::context::pem);
		ssl.use_private_key_file(client_ssl_key, boost::asio::ssl::context::pem);
	}
	::boost::asio::ip::tcp::resolver endpoints;
	::boost::asio::ip::tcp::resolver::iterator endpoints_i;
};

struct peer_connection : public netcpu::io_wrapper<netcpu::message::async_driver> {

	peer_connection(netcpu::message::async_driver & io)
	: netcpu::io_wrapper<netcpu::message::async_driver>(io) {
		// TODO -- some of the below comments may be outdated!
		// not calling 'run' code here because 'write' will implicitly call 'shared_from_this' and external onwership by shared pointer is not yet setup (ctor for shared pointer will wait for the ctor of peer_con to complete first)...
		// still, however, it is more elegant than having shared_ptr(this) here in ctor an then worrying/making sure that nothing that follows it in ctor will ever throw
	}

	// TODO -- currently deprecated... (i.e. loading multiple jobs in one invocation)
	void
	on_new_in_chain()
	{
		assert(model_metas.empty() == false);

		model_metas.pop_front();
		if (model_metas.empty() == false)
			apply();
	}

	void
	run()
	{
		netcpu::message::version msg;
		msg.value(1);
		io().write(msg, &peer_connection::apply, this);
	}

	void 
	on_error()
	{
		// todo later just test if on_error then call it otherwise skip altogether... (and let asio take things out of scope)
	}

	void 
	apply()
	{
		assert(model_metas.empty() == false);

		model_meta const & current(model_metas.front());

		censoc::llog() << "total models(=" << netcpu::models.size() << ")\n";
		netcpu::models_type::iterator implementation(netcpu::models.find(current.id));
		if (implementation == netcpu::models.end()) 
			throw ::std::runtime_error(netcpu::xxx("model is not supported: [") << current.id << ']');
		implementation->second->new_task(io());
	}

	void 
	on_write()
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
			default:
				throw netcpu::message::exception("inappropriate message");
		}
		io().read();
	}
	~peer_connection() throw();
};

void static
run(int argc, char * * argv) 
{
	::boost::scoped_ptr<interface> ui(new interface(argc, argv));
	
	::boost::shared_ptr<netcpu::message::async_driver> new_driver(new netcpu::message::async_driver);
	netcpu::peer_connection * new_peer_connection(new peer_connection(*new_driver)); // another way would be to do shared_ptr(this) in ctor of peer_connection (but then one must take extra bit of care for anything that follows this in ctor, and potentially derived types, not to throw at all)... this way, howere, can throw all one wants...

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
	new_peer_connection->run();
	new_driver.reset();
	io.run();
}

peer_connection::~peer_connection() throw()
{
	censoc::llog() << "dtor in peer_connection" << ::std::endl;
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

#include "gmnl_2a/controller.h"
#include "gmnl_2/controller.h"
#include "logit/controller.h"
#include "mixed_logit/controller.h"
