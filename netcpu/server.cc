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

#include <censoc/empty.h>
#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/arrayptr.h>
#include <censoc/opendir.h>
#include <censoc/scoped_membership_iterator.h>

#include "big_int_context.h"
namespace censoc { namespace netcpu { 
big_int_context_type static big_int_context;
}}
#include "big_int.h"

#include "types.h"
//}

namespace censoc { namespace netcpu { 

namespace message {
	class async_driver;
}
	
typedef uint_fast16_t tasks_size_type;
typedef censoc::param<tasks_size_type>::type tasks_size_paramtype;
typedef netcpu::task_id_typepair::ram task_id_type;
typedef censoc::param<task_id_type>::type task_id_paramtype;
typedef censoc::lexicast< ::std::string> xxx;

::boost::system::error_code static ec;

char const pending_index_filename[] = "pending.txt";
char const completed_index_filename[] = "completed.txt";

time_t static task_oldage(0);
time_t static task_oldage_check(0);

::boost::asio::io_service static io;
::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);
::std::string static root_path;
tasks_size_type static max_taskid;


bool static 
verify_callback(bool preverified, ::boost::asio::ssl::verify_context& ctx) 
{
	if (preverified == false)
		return false;
	else {
		::std::cerr << "CERTIFICATE NAME: (=" << ctx.native_handle()->cert->name << ")\n";
		for (unsigned i(0); i != SHA_DIGEST_LENGTH; ++i) {
			::std::cerr << (int)ctx.native_handle()->cert->sha1_hash[i];
		}
		::std::cerr << ::std::endl;
		return true;
	}
}


struct task;
::std::multimap<time_t, netcpu::task *> static aged_tasks;
::std::list<netcpu::task *> static pending_tasks;
::std::list<netcpu::task *> static completed_tasks;

// containing explicitly via a ::boost::shared_ptr does not give any nicer syntax to the user-code, and it wastes more memory/cpu-cycles (as 'shared' context is replicated 1:element of the list. Instead will have a ~dtor thingy
struct named_tasks_type : ::std::map< ::std::string, netcpu::task *> {
#ifndef NDEBUG
	~named_tasks_type();
#endif
} static named_tasks;

struct aged_tasks_iterator_traits {
	typedef censoc::empty ctor_type;
	typedef ::std::multimap<time_t, netcpu::task *> container_type;
	container_type static &
	container()
	{
		return aged_tasks;
	}
};

struct named_tasks_iterator_traits {
	typedef censoc::empty ctor_type;
	typedef ::std::map< ::std::string, netcpu::task *> container_type;
	container_type static &
	container()
	{
		return named_tasks;
	}
};

struct pending_tasks_iterator_traits {
	typedef censoc::empty ctor_type;
	typedef ::std::list<netcpu::task *> container_type;
	container_type static &
	container()
	{
		return pending_tasks;
	}
};

struct completed_tasks_iterator_traits {
	typedef censoc::empty ctor_type;
	typedef ::std::list<netcpu::task *> container_type;
	container_type static &
	container()
	{
		return completed_tasks;
	}
};

struct peer_connection;
typedef ::std::list<peer_connection *> ready_to_process_peers_type;
ready_to_process_peers_type static ready_to_process_peers;
struct ready_to_process_peers_iterator_traits {
	typedef censoc::empty ctor_type;
	typedef ready_to_process_peers_type container_type;
	container_type static &
	container()
	{
		return ready_to_process_peers;
	}
};

typedef censoc::scoped_membership_iterator<named_tasks_iterator_traits> named_tasks_scoped_membership_iterator;
typedef censoc::scoped_membership_iterator<aged_tasks_iterator_traits> aged_tasks_scoped_membership_iterator;
typedef censoc::scoped_membership_iterator<pending_tasks_iterator_traits> pending_tasks_scoped_membership_iterator;
typedef censoc::scoped_membership_iterator<completed_tasks_iterator_traits> completed_tasks_scoped_membership_iterator;
typedef censoc::scoped_membership_iterator<ready_to_process_peers_iterator_traits> ready_to_process_peers_scoped_membership_iterator;

void static
on_pending_tasks_update();

void static
on_completed_tasks_update();

struct task {
	named_tasks_scoped_membership_iterator name_i;
	aged_tasks_scoped_membership_iterator age_i;
	pending_tasks_scoped_membership_iterator pending_i;
	completed_tasks_scoped_membership_iterator completed_i; // TODO -- may not even really need this one at all...

	censoc::strip_const<censoc::param< ::std::string>::type>::type 
	name()
	{
		return name_i.i->first;
	}

	void virtual cancel() = 0;
	void virtual suspend() = 0;
	void virtual activate() = 0;
	bool virtual active() = 0;
	void virtual new_processing_peer(netcpu::message::async_driver & io) = 0;

	task(::std::string const & name, time_t birthday = ::time(NULL))
	: name_i(named_tasks.insert(::std::make_pair(name, this)).first), age_i(aged_tasks.insert(::std::make_pair(birthday, this))) {
		::std::clog << "ctor for task: time: " << birthday << ::std::endl;
	}

	void
	markpending()
	{
		assert(completed_i == completed_tasks_scoped_membership_iterator::iterator_type());
		assert(pending_i == pending_tasks_scoped_membership_iterator::iterator_type());

		pending_i = pending_tasks.insert(pending_tasks.end(), this);

		on_pending_tasks_update();
	}
	void
	swapinqueue(task * other)
	{
		assert(other != NULL);
		assert(other != this);
		*pending_i.i = other;
		*other->pending_i.i = this;
		::std::list<netcpu::task *>::iterator tmp(pending_i.i);
		pending_i.i = other->pending_i.i;
		other->pending_i.i = tmp;
		assert(this == *pending_i.i);
	}
	void
	movefrontinqueue() 
	{
		assert(pending_tasks.size() > 1);
		assert(pending_tasks.front() != this);
		pending_i = pending_tasks.insert(pending_tasks.begin(), this);
	}
	void
	markcompleted()
	{
		assert(completed_i == completed_tasks_scoped_membership_iterator::iterator_type());
		pending_i.reset();

		completed_i = completed_tasks.insert(completed_tasks.end(), this);

		on_pending_tasks_update();
		on_completed_tasks_update();
	}

};

// for simple hacking-like solution for the time-being
bool static completed_existing_tasks_parsing_during_load(false);

void static
activate_front_pending_task()
{
	if (completed_existing_tasks_parsing_during_load == true && pending_tasks.empty() == false && pending_tasks.front()->active() == false)
		pending_tasks.front()->activate();
}

void static
on_completed_tasks_update()
{ 
	// write the latest index file
	::std::string const index_stream_filepath(netcpu::root_path + netcpu::completed_index_filename);
	{
		::std::ofstream index_stream((index_stream_filepath + ".tmp").c_str(), ::std::ios::trunc);
		if (index_stream.is_open()) {
			for (::std::list<netcpu::task *>::iterator i(pending_tasks.begin()); i != pending_tasks.end(); ++i) 
				index_stream << (*i)->name() << ::std::endl;
		}
	}
	::rename((index_stream_filepath + ".tmp").c_str(), index_stream_filepath.c_str());
}

#ifndef NDEBUG
named_tasks_type::~named_tasks_type()
{
	::std::cerr << "dtor in named_tasks start\n";
	while(size()) 
		delete begin()->second; // task will delete the element anyway
	::std::cerr << "dtor in named_tasks end\n";
}
#endif

::std::string
generate_taskdir(::std::string const & postfix)
{
	netcpu::task_id_type tmp(netcpu::max_taskid++);
	while (!0) {
		::std::string const name(::std::string(netcpu::xxx(netcpu::max_taskid)) + "_" + postfix);
		::std::string const path(netcpu::root_path + name);
		int const ec(::mkdir(path.c_str(), 0700));
		if (ec) {
			if (errno != EEXIST)
				throw ::std::runtime_error(xxx("could not mkdir: [") << path << "] with error: [" << ::strerror(errno) << ']' );
			else if (++netcpu::max_taskid == tmp)
				throw ::std::runtime_error(xxx("could not mkdir: [") << path << "] all options exhausted due to maximum possible tasks being already stored on the drive (consider cleaning or changing typedef for tasks_size_type)");
		} else 
			return name;
	}
}
}}

#include "message.h"
#include "io_driver.h"

namespace censoc { namespace netcpu { 

// this must be a very very small class (it will exist through the whole of the lifespan of the server)
struct model_factory_interface {
	void virtual new_task(netcpu::message::async_driver &, netcpu::message::task_offer & x) = 0;
	void virtual new_task(::std::string const & name, bool pending, time_t birthday) = 0;
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

void static
purge_old_tasks_error()
{
	throw ::std::runtime_error("purging timer error");
}

netcpu::message::async_timer static purge_old_tasks_timer(&purge_old_tasks_error);

void static
purge_old_tasks()
{
	::std::clog << "purge check\n";
	// TODO -- convert pending and completed tasks from list to fastsize<list> for faster access of 'size' property
	size_t const pending_task_size(pending_tasks.size());
	size_t const completed_task_size(completed_tasks.size());
	while (aged_tasks.size()) { 
		netcpu::task * tmp(aged_tasks.begin()->second);
		if (::time(NULL) - aged_tasks.begin()->first > netcpu::task_oldage) {

			if (pending_tasks.front() == tmp) {
				// TODO -- if calculation-model is non-NULL -- dismantle it!!!
			}

			::std::string const path(netcpu::root_path + tmp->name());
			censoc::llog() << "purging: [" << path << "]\n";
			// short-term hack (later use ::boost::filesystem libs)
			if (::system(("rm -fr '" + path + "'").c_str()))
				throw ::std::runtime_error(xxx("could not rm -fr: [") << path << ']');

			delete tmp;
		} else 
			break;
	}

	if (pending_tasks.size() != pending_task_size)
		on_pending_tasks_update();
	if (completed_tasks.size() != completed_task_size)
		on_completed_tasks_update();

	purge_old_tasks_timer.timeout(boost::posix_time::seconds(task_oldage_check), &purge_old_tasks);
}

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

		ssl.set_options(::boost::asio::ssl::context::default_workarounds | ::boost::asio::ssl::context::no_sslv2 
				// | ::boost::asio::ssl::context::single_dh_use
				);
		ssl.set_password_callback(::boost::bind(&interface::passwd, this));
		ssl.set_verify_callback(verify_callback);

		::std::string ssl_key;
		::std::string ssl_certificate;
		::std::string ssl_clients_certificates;
		//::std::string ssl_dh;

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
			//else if (!::strcmp(argv[i], "--dh")) 
			//	ssl_dh = argv[++i];
			else if (!::strcmp(argv[i], "--clients_certificates")) 
				ssl_clients_certificates = argv[++i];
			else if (!::strcmp(argv[i], "--task_oldage")) 
				task_oldage = censoc::lexicast<time_t>(argv[++i]) * 60 * 60 * 24;
			else if (!::strcmp(argv[i], "--task_oldage_check")) 
				task_oldage_check = censoc::lexicast<time_t>(argv[++i]) * 60 * 60 * 24;
			else
				throw ::std::runtime_error(xxx() << "unknown option: [" << argv[i] << ']');
		}

		if (ssl_key.empty() == true)
			throw ::std::runtime_error("must supply ssl private key file");
		else if (ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply ssl certificate file");
		else if (ssl_clients_certificates.empty() == true)
			throw ::std::runtime_error("must supply directory for certificates used to verify peers/clients");
		//else if (ssl_dh.empty() == true)
		//	throw ::std::runtime_error("must supply ssl dh file");
		else if (root_path.empty() == true)
			throw ::std::runtime_error("must supply --root path (where all of the tasks subdirectories will be written/stored)");
		else if (!task_oldage)
			throw ::std::runtime_error("must supply --task_oldage option (in days)");
		else if (!task_oldage_check)
			throw ::std::runtime_error("must supply --task_oldage_check option (in days)");


		//ssl.use_certificate_chain_file(ssl_certificate);
		ssl.use_certificate_file(ssl_certificate, boost::asio::ssl::context::pem);
		ssl.use_private_key_file(ssl_key, boost::asio::ssl::context::pem);
		//ssl.use_tmp_dh_file(ssl_dh);

		ssl.set_verify_mode(::boost::asio::ssl::context::verify_peer | ::boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		//ssl.load_verify_file("client_certificate.pem");
		ssl.add_verify_path(ssl_clients_certificates);
	}
};

class peer_connection : public netcpu::io_wrapper { 

	// TODO -- this is a cludge, later will refactor 'peer_connection' with other types specific to controller, processor and update_client !!!
	ready_to_process_peers_scoped_membership_iterator processing_peers_i;

public:

	void
	on_new_in_chain()
	{
		io().read();
	}

	peer_connection(netcpu::message::async_driver & io) 
	: io_wrapper(io) {
		censoc::llog() << "ctor in peer_connection, driver addr: " << &io << ::std::endl;
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
			io().read(&peer_connection::on_read, this);
		}
	}

	void
	on_task_offer()
	{
		netcpu::message::task_offer msg;
		msg.from_wire(io().read_raw);
		censoc::llog() << "a task is on offer: [" << msg.task_id() << "], from: [" << io().socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n";
		netcpu::models_type::iterator implementation(netcpu::models.find(msg.task_id()));
		if (implementation != netcpu::models.end()) 
			implementation->second->new_task(io(), msg);
		else 
			io().write(netcpu::message::bad());
	}

	void 
	on_read()
	{
		netcpu::message::id_type const id(io().read_raw.id());
		switch (id) {
		case netcpu::message::ready_to_process::myid :
		{
			netcpu::message::ready_to_process msg;
			msg.from_wire(io().read_raw);
			censoc::llog() << "peer is ready to process: [" << io().socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n";
			if (pending_tasks.empty() == false && pending_tasks.front()->active() == true 
				// because don't want to feed next 'ready_to_process' message from the same peer back to the current task (given that this peer may already have rejected the task during the 1st offering).
				&& io().never_replaced == true
				) {
				censoc::llog() << "starting new_processing_peer from active task\n";
				return pending_tasks.front()->new_processing_peer(io());
			} else {
			// { todo -- may be deprecate the check (or just make it into an assertion) -- currently making this robust w.r.t. client sending mulitple (e.g. consecutive) 'ready to process' messages (later should probably drop the client connection as extra irrelevant messages yield incorrect usage and extra b/w utilisation). }
				if (processing_peers_i.nulled() == true) 
					processing_peers_i = netcpu::ready_to_process_peers.insert(ready_to_process_peers.end(), this);
			}
		}
		break;

		case netcpu::message::task_offer::myid : 
		return on_task_offer();

		default:
		censoc::llog() << "unexpected message: [" << id << "], ignoring.\n";
		}
		io().read();
	}

	void 
	on_write()
	{
		censoc::llog() << "written something" << ::std::endl;
		io().read();
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
		newcon.listen(500); // TODO!!!
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

	{
		// load pending index file if present (otherwise have empty index_list in RAM)
		::std::list< ::std::string> pending_order;
		::std::set< ::std::string> pending_index;
		{
			::std::ifstream index_stream((netcpu::root_path + netcpu::pending_index_filename).c_str());
			if (index_stream.is_open()) {
				while (!index_stream.eof()) {
					::std::string tmp;
					::std::getline(index_stream, tmp);
					if (tmp.empty() == false) {
						pending_order.push_back(tmp);
						pending_index.insert(tmp);
					}
				}
			}
		}

		// load completed index file if present (otherwise have empty index_list in RAM)
		::std::set< ::std::string> completed_index;
		{
			::std::ifstream index_stream((netcpu::root_path + netcpu::completed_index_filename).c_str());
			if (index_stream.is_open()) {
				while (!index_stream.eof()) {
					::std::string tmp;
					::std::getline(index_stream, tmp);
					if (tmp.empty() == false)
						completed_index.insert(tmp);
				}
			}
		}

		// load all dirs
		censoc::autoclose_opendir d(netcpu::root_path);
		for (struct ::dirent * i(::readdir(d.dir)); i != NULL; i = ::readdir(d.dir)) {

			::std::string const name(i->d_name);
			::std::string const ent_path(netcpu::root_path + name);

			struct ::stat s;
			if (::stat(ent_path.c_str(), &s)) 
				throw ::std::runtime_error(xxx("could not stat: [") << ent_path << ']');
			else if (S_ISDIR(s.st_mode) && name != "." && name != "..") {

				::std::string::size_type delim_pos(name.find_first_of('_'));
				if (delim_pos == ::std::string::npos || name.size() <= ++delim_pos)
					throw ::std::runtime_error(xxx("incorrect path found: [") << ent_path << ']' );

				netcpu::models_type::iterator implementation(netcpu::models.find(censoc::lexicast<netcpu::task_id_paramtype>(name.substr(delim_pos))));
				if (implementation == netcpu::models.end()) 
					throw ::std::runtime_error("corrupt directory -- unsupported model found");
				if (completed_index.find(name) != completed_index.end()) {
					assert(pending_index.find(name) == pending_index.end());
					implementation->second->new_task(name, false, s.st_birthtime);
				} else if (pending_index.find(name) != pending_index.end()) {
					assert(completed_index.find(name) == completed_index.end());
					implementation->second->new_task(name, true, s.st_birthtime);
				}
			}
		}

#ifndef NDEBUG
		censoc::llog() << "before shuffle:\n";
		for (::std::list<netcpu::task *>::iterator i(pending_tasks.begin()); i != pending_tasks.end(); ++i) 
			censoc::llog() << (*i)->name() << ::std::endl;
#endif

		// shuffle in the pending_tasks list according to the order in the above index
		for (::std::list< ::std::string>::reverse_iterator i(pending_order.rbegin()); i != pending_order.rend(); ++i) {
			::std::map< ::std::string, netcpu::task * >::iterator j(named_tasks.find(*i));
			if (j != named_tasks.end() && pending_tasks.front() != j->second)
				j->second->movefrontinqueue();
		}

#ifndef NDEBUG
		censoc::llog() << "after shuffle:\n";
		for (::std::list<netcpu::task *>::iterator i(pending_tasks.begin()); i != pending_tasks.end(); ++i) 
			censoc::llog() << (*i)->name() << ::std::endl;
#endif

		completed_existing_tasks_parsing_during_load = true;
		on_pending_tasks_update();

		//{ no need in newer (i.e. self-registering task class) design...
#if 0
		// copy what remains in the map to the list (i.e. what has not been found in the index -- robustness w.r.t. corrupt or missing index file)
		typedef ::std::map< ::std::string, ::boost::shared_ptr<netcpu::task> >::value_type pair_type;
		::std::transform(named_tasks.begin(), named_tasks.end(), ::std::back_inserter(pending_tasks), ::boost::bind(&pair_type::second, _1));
#endif
		//}

	}

	purge_old_tasks_timer.timeout(&purge_old_tasks);

	::boost::shared_ptr<netcpu::acceptor> acceptor_ptr(new acceptor(ui->endpoints_i)); 
	ui.reset(NULL);
	acceptor_ptr->run();
	io.run();
	acceptor_ptr.reset();
}

}}

#include "io_wrapper.pi"

namespace censoc { namespace netcpu { 

void static
on_pending_tasks_update()
{ 
	if (completed_existing_tasks_parsing_during_load == false)
		return;
	// write the latest index file
	::std::string const index_stream_filepath(netcpu::root_path + netcpu::pending_index_filename);
	{
		::std::ofstream index_stream((index_stream_filepath + ".tmp").c_str(), ::std::ios::trunc);
		if (index_stream.is_open()) {
			for (::std::list<netcpu::task *>::iterator i(pending_tasks.begin()); i != pending_tasks.end(); ++i) 
				index_stream << (*i)->name() << ::std::endl;
		}
	}
	::rename((index_stream_filepath + ".tmp").c_str(), index_stream_filepath.c_str());

	assert(pending_tasks.empty() == true || pending_tasks.front() != NULL);
	activate_front_pending_task();
}

peer_connection::~peer_connection() throw()
{
	// if not being deleted by virtue of being taken-over
	if (io().being_taken_over == false) {
		io().being_taken_over = true; // to prevent re-newing of another peer_connection
		io().final_in_chain = true;
	}
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

#include "gmnl_2a/server.h"
#include "gmnl_2/server.h"
#include "logit/server.h"
#include "mixed_logit/server.h"
