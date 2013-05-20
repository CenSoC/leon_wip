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
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>

#include <censoc/empty.h>
#include <censoc/type_traits.h>
#include <censoc/lexicast.h>
#include <censoc/opendir.h>
#include <censoc/scoped_membership_iterator.h>
#include <censoc/stl_container_utils.h>

#include "big_int_context.h"
namespace censoc { namespace netcpu { 
big_int_context_type static big_int_context;
}}
#include "big_int.h"

#include "types.h"
#include "message.h"
//}


namespace censoc { namespace netcpu { 

namespace message {
	class async_driver;
}
	
typedef netcpu::message::typepair<uint64_t>::ram tasks_size_type;
typedef censoc::param<tasks_size_type>::type tasks_size_paramtype;
typedef netcpu::task_id_typepair::ram task_id_type;
typedef censoc::param<task_id_type>::type task_id_paramtype;
typedef censoc::lexicast< ::std::string> xxx;

::boost::system::error_code static ec;

char const pending_index_filename[] = "pending.txt";
char const completed_index_filename[] = "completed.txt";

time_t static task_oldage(0);
time_t static task_oldage_check(0);
time_t static wakeonlan_check(30); // in minutes, todo, a quick hack, perhaps a runtime parametric CLI option for the value, don't make it too short -- see git commit history logs for reasoning.

::boost::asio::io_service static io;
::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);
::std::string static root_path;
tasks_size_type static taskid_begin;
tasks_size_type static taskid_end;

void static
do_wakeonlan()
{
	// todo -- use boost filesystem exists to see if script exists and only then run it, or perhaps use a command-line option supplied by the user to determine what (and if) to run.
	if (::system("./wakeonlan.sh")) // todo -- later make it more configurable and sensible
		throw ::std::runtime_error("could not run wakeonlan.sh");
}

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
::censoc::stl::fastsize< ::std::list<netcpu::task *>, uint_fast32_t> static pending_tasks;
::std::multimap< ::time_t, netcpu::task *> static completed_tasks;

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
	typedef ::censoc::stl::fastsize< ::std::list<netcpu::task *>, uint_fast32_t> container_type;
	container_type static &
	container()
	{
		return pending_tasks;
	}
};

struct completed_tasks_iterator_traits {
	typedef censoc::empty ctor_type;
	typedef ::std::multimap< ::time_t, netcpu::task *> container_type;
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


::std::set<netcpu::tasks_size_type> static tasks_ids;
struct tasks_ids_iterator_traits {
	typedef censoc::empty ctor_type;
	typedef ::std::set<netcpu::tasks_size_type> container_type;
	container_type static &
	container()
	{
		return tasks_ids;
	}
};

struct tasks_ids_scoped_membership_iterator : censoc::scoped_membership_iterator<tasks_ids_iterator_traits> {

	tasks_ids_scoped_membership_iterator(iterator_paramtype i)
	: censoc::scoped_membership_iterator<tasks_ids_iterator_traits>(i) {
	}

	~tasks_ids_scoped_membership_iterator()
	{
		assert(nulled() == false);
		if (*i == taskid_begin) {
			iterator_type j(i);
			++j;
			if (j == container().end())
				taskid_begin = taskid_end;
			else
				taskid_begin = *j;

			// store taskid_begin (even though throwing in dtor is not cool -- termination of the whole process is ok and expected here)
			::std::ofstream f((netcpu::root_path + "taskid_begin.tmp").c_str(), ::std::ios::trunc);
			if (f.is_open() == false) 
				throw ::std::runtime_error("could not create taskid_begin.tmp temporary file");

			netcpu::message::typepair<uint64_t>::wire w(taskid_begin);
			uint8_t raw[sizeof(w)];
			netcpu::message::to_wire<netcpu::message::typepair<uint64_t>::wire>::eval(raw, w);
			if (!f.write(reinterpret_cast<char const *>(raw), sizeof(w)))
				throw ::std::runtime_error("could not write taskid_begin");

			if (::rename((netcpu::root_path + "taskid_begin.tmp").c_str(), (netcpu::root_path + "taskid_begin").c_str()))
				throw ::std::runtime_error("could not rename/mv taskid_begin temporary file");
		}
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
	tasks_ids_scoped_membership_iterator id_i;
	named_tasks_scoped_membership_iterator name_i;
	aged_tasks_scoped_membership_iterator age_i;
	pending_tasks_scoped_membership_iterator pending_i;
	completed_tasks_scoped_membership_iterator completed_i; // TODO -- may not even really need this one at all...

	censoc::strip_const<censoc::param< ::std::string>::type>::type 
	name() const
	{
		return name_i.i->first;
	}

	void virtual deactivate() = 0;
	void virtual activate() = 0;
	bool virtual active() = 0;
	void virtual new_processing_peer(netcpu::message::async_driver & io) = 0;
	

	// NOTE -- a temporary, quick hack; this is because the deriving task already has floating-point and integral specialisations (needed for reading convergence_state message); moreover convergence_state is specific to the converger_1 algorithm which may, later on, not be the only algo used and so the awareness of the mapping/translation of algo-native message structure to a more universal/common format may be needed anyway. Just about the only choice is to whether make each algo write-out the common-format convergence when it writes its native convergence_state message (then just reading the file upon every controller request); or, alternatively calculate the mapping on-the-run: the way it is done now... 
	// TODO -- consider (in more detail) the former alternative...
	// TODO -- it may even be plausible that different models will not have a common representational structure (coefficient values, etc.) in the first place, and so the message-building -cooking process will need to be re-factored to perhaps generate either a dynamically-structured messages or a multi-message sequences during io-communication with the peer... this, however, is rather in a very distant future to consider...
	virtual void load_coefficients_info_et_al(netcpu::message::tasks_list & tasks_list, netcpu::message::task_info &) = 0;

	task(netcpu::tasks_size_type const & id, ::std::string const & name, time_t birthday = ::time(NULL))
	: id_i(tasks_ids.insert(id).first), name_i(named_tasks.insert(::std::make_pair(name, this)).first), age_i(aged_tasks.insert(::std::make_pair(birthday, this))) {
		::std::clog << "ctor for task: time: " << birthday << ::std::endl;
	}

	virtual 
	~task()
	{
		// NOTE no need to test for deactivation, should really be a part of dtor code in the deriving class
		::std::string const path(netcpu::root_path + name());
		censoc::llog() << "purging: [" << path << "]\n";
		// short-term hack (later use ::boost::filesystem libs)
		if (::system(("rm -fr '" + path + "'").c_str()))
			throw ::std::runtime_error(xxx("could not rm -fr: [") << path << ']');
	}

	void
	markpending()
	{
		assert(completed_i == completed_tasks_scoped_membership_iterator::iterator_type());
		assert(pending_i == pending_tasks_scoped_membership_iterator::iterator_type());

		pending_i = pending_tasks.insert(pending_tasks.end(), this);

		on_pending_tasks_update();
	}
	bool
	move_in_list_of_pending_tasks(int steps_to_move_by)
	{
		assert(steps_to_move_by);
		assert(pending_i != pending_tasks_scoped_membership_iterator::iterator_type());

		::std::list<netcpu::task *>::iterator i(pending_i.i);
		assert(this == *i);
		if (steps_to_move_by > 0)
			for (; steps_to_move_by > -1 && i != pending_tasks.end(); ++i, --steps_to_move_by);
		else 
			for (; steps_to_move_by && i != pending_tasks.begin(); --i, ++steps_to_move_by);

		uint_fast32_t const pending_tasks_size(pending_tasks.size());
		if (i != pending_i.i && i != ::std::next(pending_i.i)) {
			if (active() == true)
				deactivate();
			else if (i == pending_tasks.begin() && (*i)->active() == true)
				(*i)->deactivate();
			if (pending_tasks_size != pending_tasks.size()) // if any of the above deactivation caused 'on_sync_timeout' to deem the task as complete...
				return false;
			pending_i = pending_tasks.insert(i, this);
		}
		
		// not calling activate (if needed) because the caller is expected to eventually call 'on_pending_tasks_update' (which will not only activate front pending task but will also serialize the pending list to file/disk) 

		return true;
	}
	void
	markcompleted()
	{
		assert(completed_i == completed_tasks_scoped_membership_iterator::iterator_type());
		assert(age_i != aged_tasks_scoped_membership_iterator::iterator_type());

		deactivate();
		pending_i.reset();

		completed_i = completed_tasks.insert(*age_i.i);

		on_pending_tasks_update();
		on_completed_tasks_update();
	}

};

// for simple hacking-like solution for the time-being
bool static completed_existing_tasks_parsing_during_load(false);

void static
activate_front_pending_task()
{
	if (completed_existing_tasks_parsing_during_load == true && pending_tasks.empty() == false && pending_tasks.front()->active() == false) {
		pending_tasks.front()->activate();
		do_wakeonlan();
	}
}

void static
on_completed_tasks_update()
{ 
	if (completed_existing_tasks_parsing_during_load == false)
		return;
	// write the latest index file
	::std::string const index_stream_filepath(netcpu::root_path + netcpu::completed_index_filename);
	{
		::std::ofstream index_stream((index_stream_filepath + ".tmp").c_str(), ::std::ios::trunc);
		if (index_stream.is_open() == false)
			throw ::std::runtime_error("could not create completed tasks temporary file");

		for (::std::multimap< ::time_t, netcpu::task *>::iterator i(completed_tasks.begin()); i != completed_tasks.end(); ++i) 
			index_stream << i->second->name() << ::std::endl;

		if (!index_stream)
			throw ::std::runtime_error("could not write completed tasks temporary file");
	}
	if (::rename((index_stream_filepath + ".tmp").c_str(), index_stream_filepath.c_str()))
		throw ::std::runtime_error("could not rename/mv completed tasks temporary file");
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

void
generate_taskdir(::std::string const & postfix, netcpu::tasks_size_type & id, ::std::string & name)
{
		id = netcpu::taskid_end;
		name = ::std::string(netcpu::xxx(netcpu::taskid_end)) + "_" + postfix;
		::std::string const path(netcpu::root_path + name);
		if (::mkdir(path.c_str(), 0700))
			throw ::std::runtime_error(xxx("could not mkdir: [") << path << "] with error: [" << ::strerror(errno) << ']' );

		netcpu::taskid_end = (netcpu::taskid_end + 1) % 100000;
		if (netcpu::taskid_begin - netcpu::taskid_end < 300)
			throw ::std::runtime_error("too many existing tasks to allow generation of a new one");

		// store taskid_end 
		::std::ofstream f((netcpu::root_path + "taskid_end.tmp").c_str(), ::std::ios::trunc);
		if (f.is_open() == false)
			throw ::std::runtime_error("could not create taskid_end.tmp temporary file");

		netcpu::message::typepair<uint64_t>::wire w(taskid_end);
		uint8_t raw[sizeof(w)];

		netcpu::message::to_wire<netcpu::message::typepair<uint64_t>::wire>::eval(raw, w);
		if (!f.write(reinterpret_cast<char const *>(raw), sizeof(w)))
			throw ::std::runtime_error("could not write taskid_end");

		if (::rename((netcpu::root_path + "taskid_end.tmp").c_str(), (netcpu::root_path + "taskid_end").c_str()))
			throw ::std::runtime_error("could not rename/mv taskid_end temporary file");
}

}}

#include "io_driver.h"

namespace censoc { namespace netcpu { 

// this must be a very very small class (it will exist through the whole of the lifespan of the server)
struct model_factory_interface {
	void virtual new_task(netcpu::message::async_driver &, netcpu::message::task_offer & x) = 0;
	void virtual new_task(netcpu::tasks_size_type const & id, ::std::string const & name, bool pending, time_t birthday) = 0;
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
netcpu::message::async_timer static wakeonlan_timer;

void static
purge_old_tasks()
{
	::std::clog << "purge check\n";
	size_t const pending_task_size(pending_tasks.size());
	size_t const completed_task_size(completed_tasks.size());
	while (aged_tasks.size()) { 
		netcpu::task * tmp(aged_tasks.begin()->second);
		assert(tmp != NULL);
		if (::time(NULL) - aged_tasks.begin()->first > netcpu::task_oldage)
			delete tmp;
		else 
			break;
	}

	if (pending_tasks.size() != pending_task_size)
		on_pending_tasks_update();
	if (completed_tasks.size() != completed_task_size)
		on_completed_tasks_update();

	purge_old_tasks_timer.timeout(boost::posix_time::seconds(task_oldage_check), &purge_old_tasks);
}

void static
wakeonlan()
{
	if (pending_tasks.empty() == false && pending_tasks.front()->active() == true)
		do_wakeonlan();
	wakeonlan_timer.timeout(boost::posix_time::minutes(wakeonlan_check));
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

class peer_connection : public netcpu::io_wrapper<netcpu::message::async_driver> { 


	// TODO -- this is a cludge, later will refactor 'peer_connection' with other types specific to controller, processor and update_client !!!
	ready_to_process_peers_scoped_membership_iterator processing_peers_i;

public:

	peer_connection(netcpu::message::async_driver & io) 
	: netcpu::io_wrapper<netcpu::message::async_driver>(io) {
		censoc::llog() << "ctor in peer_connection, driver addr: " << &io << ::std::endl;
		assert(io.is_write_pending() == false);
		this->io().handshake_callback(&peer_connection::on_handshake, this);
		io.write_callback(&peer_connection::on_write, this);
		// todo -- a reasonable cludge this one: end_process_writer et al in converger_1/server.h et al will new-up the peer_connection but without any need to do a handshake -- basically jumping rigth into the 'on_read' state... by that stage the 'on_read' callback had better be set... so just doing it here... later on shall re-factor for more explicit specialisations of the code-use -- as new-in-chain or not...
		io.read_callback(&peer_connection::on_read, this);
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
	on_get_tasks_list()
	{
		netcpu::message::tasks_list msg;
		uint_fast32_t task_i(0);
		msg.tasks.resize(pending_tasks.size() + completed_tasks.size());
		for (censoc::stl::fastsize< ::std::list<netcpu::task *>, uint_fast32_t>::const_iterator i(pending_tasks.begin()); i != pending_tasks.end(); ++i, ++task_i) {
#ifndef NDEBUG
			if (!task_i)
				assert((*i)->active() == true);
#endif
			netcpu::message::task_info & tsk(msg.tasks[task_i]);
			tsk.name.resize((*i)->name().size());
			::memcpy(tsk.name.data(), (*i)->name().c_str(), (*i)->name().size());
			tsk.state(netcpu::message::task_info::state_type::pending);
			(*i)->load_coefficients_info_et_al(msg, tsk);
		}
		for (::std::multimap< ::time_t, netcpu::task *>::const_iterator i(completed_tasks.begin()); i != completed_tasks.end(); ++i, ++task_i) {
			netcpu::message::task_info & tsk(msg.tasks[task_i]);
			tsk.name.resize(i->second->name().size());
			::memcpy(tsk.name.data(), i->second->name().c_str(), i->second->name().size());
			tsk.state(netcpu::message::task_info::state_type::completed);
			i->second->load_coefficients_info_et_al(msg, tsk);
		}
		io().write(msg);
	}

	void
	on_move_task_in_list()
	{
		netcpu::message::move_task_in_list msg;
		msg.from_wire(io().read_raw);
		int const steps_to_move_by(netcpu::message::deserialise_from_unsigned_to_signed_integral(msg.steps_to_move_by()));
		if (steps_to_move_by) {
			::std::string task_name(::std::string(msg.task_name.data(), msg.task_name.size()));
			::std::map< ::std::string, netcpu::task * >::iterator i(named_tasks.find(task_name));
			if (i != named_tasks.end()) {
				while (i->second->pending_i != pending_tasks_scoped_membership_iterator::iterator_type()) {
					assert(pending_tasks.empty() == false);
					assert(pending_tasks.front()->active() == true);
					assert(pending_tasks.front() != NULL);
					if (i->second->move_in_list_of_pending_tasks(steps_to_move_by) == false)
						continue;
					on_pending_tasks_update();
					io().write(netcpu::message::good());
					return;
				} 
			}
			censoc::llog() << "controller is asking to move a task which does not exist, or is no longer pending [" << task_name << "]\n";
			io().write(netcpu::message::bad());
		} else {
			censoc::llog() << "controller is asking to move a task by zero-steps (not allowed)\n";
			io().write(netcpu::message::bad());
		}
	}

	void
	on_delete_task()
	{
		netcpu::message::delete_task msg;
		msg.from_wire(io().read_raw);
		::std::string task_name(::std::string(msg.task_name.data(), msg.task_name.size()));
		::std::cerr << "deleting a task " << task_name << '\n';
		::std::map< ::std::string, netcpu::task * >::iterator i(named_tasks.find(task_name));
		if (i != named_tasks.end()) {
			size_t const pending_task_size(pending_tasks.size());
			size_t const completed_task_size(completed_tasks.size());
			delete i->second;
			if (pending_tasks.size() != pending_task_size)
				on_pending_tasks_update();
			if (completed_tasks.size() != completed_task_size)
				on_completed_tasks_update();
			io().write(netcpu::message::good());
		} else {
			censoc::llog() << "controller is asking to delete a task which does not exist [" << task_name << "]\n";
			io().write(netcpu::message::bad());
		}
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
			if (pending_tasks.empty() == false && pending_tasks.front()->active() == true) {
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

		case netcpu::message::get_tasks_list::myid : 
		return on_get_tasks_list();

		case netcpu::message::move_task_in_list::myid : 
		return on_move_task_in_list();

		case netcpu::message::delete_task::myid : 
		return on_delete_task();

		default:
		censoc::llog() << "unexpected message: [" << id << "], ignoring.\n";
		}
		io().read();
	}

	void 
	on_write()
	{
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
	::boost::scoped_ptr<interface> ui(new interface(argc, argv));

	{
		netcpu::message::typepair<uint64_t>::wire w;
		uint8_t raw[sizeof(w)];

		// load taskid_begin
		{
			::std::ifstream f((netcpu::root_path + "taskid_begin").c_str());
			if (f.read(reinterpret_cast<char *>(raw), sizeof(w))) {
				netcpu::message::from_wire<netcpu::message::typepair<uint64_t>::wire>::eval(raw, w);
				taskid_begin = w;
				::std::cerr << "taskid_begin cached: " << taskid_begin << ::std::endl;
			}
		}

		// load taskid_end
		{
			::std::ifstream f((netcpu::root_path + "taskid_end").c_str());
			if (f.read(reinterpret_cast<char *>(raw), sizeof(w))) {
				netcpu::message::from_wire<netcpu::message::typepair<uint64_t>::wire>::eval(raw, w);
				taskid_end = w;
				::std::cerr << "taskid_end cached: " << taskid_end << ::std::endl;
			}
		}

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

				::std::string::size_type begin_pos(name.find_first_of('_'));

				netcpu::tasks_size_type const taskid(::boost::lexical_cast<netcpu::tasks_size_type>(name.substr(0, begin_pos)));

				if (begin_pos == ::std::string::npos || name.size() <= ++begin_pos)
					throw ::std::runtime_error(xxx("incorrect path found: [") << ent_path << ']' );

				::std::string::size_type end_pos(name.find_first_of('-'));
				if (end_pos == ::std::string::npos)
					throw ::std::runtime_error(xxx("incorrect path found: [") << ent_path << ']' );

				netcpu::models_type::iterator implementation(netcpu::models.find(censoc::lexicast<netcpu::task_id_paramtype>(name.substr(begin_pos, end_pos))));
				if (implementation == netcpu::models.end()) 
					throw ::std::runtime_error("corrupt directory -- unsupported model found");
				if (completed_index.find(name) != completed_index.end()) {
					assert(pending_index.find(name) == pending_index.end());
					implementation->second->new_task(taskid, name, false, s.st_birthtime);
				} else if (pending_index.find(name) != pending_index.end()) {
					assert(completed_index.find(name) == completed_index.end());
					implementation->second->new_task(taskid, name, true, s.st_birthtime);
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
				j->second->pending_i = pending_tasks.insert(pending_tasks.begin(), j->second);
		}

#ifndef NDEBUG
		censoc::llog() << "after shuffle:\n";
		for (::std::list<netcpu::task *>::iterator i(pending_tasks.begin()); i != pending_tasks.end(); ++i) 
			censoc::llog() << (*i)->name() << ::std::endl;
#endif

		completed_existing_tasks_parsing_during_load = true;
		on_completed_tasks_update();
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
	wakeonlan_timer.timeout(boost::posix_time::minutes(wakeonlan_check), &wakeonlan);

	::boost::shared_ptr<netcpu::acceptor> acceptor_ptr(new acceptor(ui->endpoints_i)); 
	ui.reset();
	acceptor_ptr->run();
	io.run();
	acceptor_ptr.reset();
}

void static
on_pending_tasks_update()
{ 
	if (completed_existing_tasks_parsing_during_load == false)
		return;
	// write the latest index file
	::std::string const index_stream_filepath(netcpu::root_path + netcpu::pending_index_filename);
	{
		::std::ofstream index_stream((index_stream_filepath + ".tmp").c_str(), ::std::ios::trunc);
		if (index_stream.is_open() == false)
			throw ::std::runtime_error("could not create pending tasks temporary file");

		for (::std::list<netcpu::task *>::iterator i(pending_tasks.begin()); i != pending_tasks.end(); ++i) 
			index_stream << (*i)->name() << ::std::endl;

		if (!index_stream)
			throw ::std::runtime_error("could not write pending tasks temporary file");
	}
	if (::rename((index_stream_filepath + ".tmp").c_str(), index_stream_filepath.c_str()))
		throw ::std::runtime_error("could not rename/mv pending tasks temporary file");

	assert(pending_tasks.empty() == true || pending_tasks.front() != NULL);
	activate_front_pending_task();
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

#include "gmnl_2a/server.h"
#include "gmnl_2/server.h"
#include "logit/server.h"
#include "mixed_logit/server.h"
