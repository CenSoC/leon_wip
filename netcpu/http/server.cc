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

#include <readpassphrase.h>
#include <stdlib.h>
#include <zlib.h>
#include <archive.h>
#include <archive_entry.h>

#include <string>
#include <iostream>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

#include <censoc/empty.h>
#include <censoc/type_traits.h>
#include <censoc/lexicast.h>

#include <netcpu/models_ids.h>

//}

namespace censoc { namespace netcpu { 

typedef censoc::lexicast< ::std::string> xxx;

::boost::posix_time::ptime static time_started;
::std::string static time_started_as_string;

::boost::system::error_code static ec;
::boost::asio::io_service static io;
namespace http { ::boost::asio::ssl::context static ssl_server(netcpu::io, ::boost::asio::ssl::context::sslv23_server); }
unsigned char const static as_server_ssl_session_id(1);
::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);

struct model_meta {
	enum action {task_offer, task_status};
	action what_to_do;
	typedef ::std::list< ::std::pair< ::std::string, ::std::string> > args_type;
	args_type args;
};

struct http_adapter_driver;
struct model_factory_interface {
	void virtual new_task(
		netcpu::http_adapter_driver &, 
		::boost::shared_ptr<model_meta> // should not pass by reference to shared_ptr, ctor in the upcoming objects will delete caller (most likely) via the base-class (io_wrapper) ctor *before* the top-level ctor takes onwership of the 'mm'
	) = 0;
#ifndef NDEBUG
	virtual ~model_factory_interface()
	{
	}
#endif
};

}}

#include "../models.h"
#include "../io_driver.h"
#include "../io_wrapper.h"
#include "io_driver.h"

namespace censoc { namespace netcpu { 

::boost::asio::ip::tcp::resolver::iterator static native_protocol_server_endpoints_i;

template <typename T>
struct virtual_enable_shared_from_this : virtual ::boost::enable_shared_from_this<T> {
};

struct http_adapter_driver : netcpu::message::async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> >, netcpu::http::http_async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> > {

	typedef netcpu::message::async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> > native_protocol;
	typedef netcpu::http::http_async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> > http_protocol;

	::std::unique_ptr<netcpu::io_wrapper<http_adapter_driver> > user_key;

	void
	on_native_protocol_exception(::std::string const & x) 
	{
		http_protocol::abandon_cancel();
		http_protocol::write_json_error(x);
	}

	void
	on_http_handshake()
	{
		::boost::asio::ip::tcp::resolver::iterator endpoints_i = native_protocol_server_endpoints_i;
		while (endpoints_i != ::boost::asio::ip::tcp::resolver::iterator()) {
			::std::cerr << "BAM\n";
			native_protocol::socket.lowest_layer().connect(*endpoints_i, netcpu::ec);
			if (!ec) 
				break;
			else {
				native_protocol::socket.lowest_layer().close();
				++endpoints_i;
			}
		}
		if (endpoints_i == ::boost::asio::ip::tcp::resolver::iterator())
			throw netcpu::message::exception(xxx() << "Could not connect to NetCPU server.");
		native_protocol::socket.handshake(::boost::asio::ssl::stream_base::client, netcpu::ec);
		if (ec) 
			throw netcpu::message::exception(xxx() << "Although being connected, could not do a SSL handshake with NetCPU server.");
		native_protocol::set_keepalive();
		netcpu::message::version msg;
		msg.value(1);
		native_protocol::write(msg, &http_adapter_driver::on_native_protocol_version_written, this);
	}

	void
	on_native_protocol_version_written()
	{
		http_protocol::read_header_line();
	}

	http_adapter_driver()
	{
		native_protocol::error_callback(&http_adapter_driver::on_native_protocol_exception, this);
		http_protocol::handshake_callback = ::boost::bind(&http_adapter_driver::on_http_handshake, this);
	}

	~http_adapter_driver()
	{
		http_protocol::canceled_ = true;
	}

	void
	reset_callbacks()
	{
		native_protocol::reset_callbacks();
		http_protocol::reset_callbacks();
		// todo -- overwriting hack for the time-being
		native_protocol::error_callback(&http_adapter_driver::on_native_protocol_exception, this);
	}

	void
	cancel()
	{
		native_protocol::cancel();
		http_protocol::cancel();
	}
};

namespace http { using io_driver_type = netcpu::http_adapter_driver; }

}}

#include "resources_typedef.h"
namespace censoc { namespace netcpu { namespace http { http::resources_type static resources; }}}

#include "utils.h"

namespace censoc { namespace netcpu { 

// cludge for converger_1/controller.h -- it expects netcpu::peer_connection for the time being
typedef netcpu::http::peer_connection peer_connection;

struct new_task : io_wrapper<http_adapter_driver> {

	unsigned model_id;
	::boost::shared_ptr<model_meta> mm;

	::std::string static
	get_id()
	{
		return "post /new_task http/1.1";
	}

	struct scoped_zip_archive {
		::archive * impl;
		scoped_zip_archive()
		: impl(::archive_read_new())
		{
			if (impl == NULL)
				throw netcpu::message::exception("::archive_read_new");
		}
		~scoped_zip_archive()
		{
			::archive_read_free(impl);
		}
	};

	void
	on_process(::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
	{
		if (resource_id == get_id()) {
			// ::std::ofstream f("deleteme.bin", ::std::ios::binary);
			// f.write(body_fields.back().second.data(), body_fields.back().second.size());

			mm.reset(new model_meta);
			mm->what_to_do = model_meta::task_offer;

			model_id = -1; // will send http error (in on_native_protocol_version_written) if no model was specified in args
			for (::std::list< ::std::pair< ::std::string, ::std::string> >::iterator i(body_fields.begin()); i != body_fields.end(); ++i) {

				::std::cerr << "body name: " << i->first << ::std::endl;
				::std::cerr << "basename: " << ::boost::filesystem::basename(i->first) << ::std::endl;
				::std::cerr << "extension: " << ::boost::filesystem::extension(i->first) << ::std::endl;

				if (::boost::filesystem::basename(i->first) == "--filedata") {
					::std::string const extension(::boost::filesystem::extension(i->first));
					if (extension == ".zip") {
						scoped_zip_archive a;
						::archive_entry *ae;
						if (::archive_read_support_filter_compress(a.impl) == ARCHIVE_OK && ::archive_read_support_format_zip(a.impl) == ARCHIVE_OK && 
							// me thinks libarchive has an interface bug... at least w.r.t. doco/implementation, the buf data argument (no. 2) should have been declared const... alas an ugly cast for the time being... if later versions of libarchive become more explicit w.r.t. whether the buf data is modified during call then will adjust/re-factor later...
							::archive_read_open_memory(a.impl, const_cast<char *>(i->second.data()), i->second.size()) == ARCHIVE_OK && ::archive_read_next_header(a.impl, &ae) == ARCHIVE_OK && ae != NULL
						) {
							::boost::scoped_array<char> d(new char[http_adapter_driver::http_protocol::max_read_capacity]);
							int const bytes_read(::archive_read_data(a.impl, d.get(), http_adapter_driver::http_protocol::max_read_capacity));
							if (bytes_read <= 0)
								throw netcpu::message::exception("Could not read some of the zip data.");
							else if (static_cast<unsigned>(bytes_read) < http_adapter_driver::http_protocol::max_read_capacity) {
								mm->args.push_back(::std::pair< ::std::string, ::std::string>("--filedata", ::std::string(d.get(), bytes_read)));
								// ::boost::stream< ::boost::array_source> stream(d.get(), file_size);
							} else
								throw netcpu::message::exception(xxx("The uncompressed CSV dataset file is still too large... maximum allowed dataset size is: ") << unsigned(http_adapter_driver::http_protocol::max_read_capacity) << "bytes.");
						} else
							throw netcpu::message::exception("Could not open zip archive, etc.");
					} else if (extension == ".csv") {
						i->first.erase(i->first.size() - 4);
						mm->args.push_back(*i);
					} else
						throw netcpu::message::exception("invalid file format upload (only csv and zip files supported)");
				} else if (i->first == "--model_id") { 
					model_id = ::boost::lexical_cast<unsigned>(i->second);
				} else
					mm->args.push_back(*i);
			}

			netcpu::models_type::iterator model_i(netcpu::models.find(model_id));
			if (model_i != netcpu::models.end()) {
				model_i->second->new_task(io(), mm);
			} else
				throw netcpu::message::exception(xxx() << "unsupported computational model(=" << model_id << ')');

		} else 
			netcpu::http::apply_resource_processor(io(), resource_id, body_fields);
	}


	new_task(http_adapter_driver & io)
	: io_wrapper<http_adapter_driver>(io) {
		io.http_protocol::process_callback = ::boost::bind(&new_task::on_process, this, _1, _2);
		io.http_protocol::content = "application/json";
		//io.http_protocol::content = "plain/text";
	}

#ifndef NDEBUG
	~new_task()
	{
		::std::cerr << "dtor of new_task " << this << ::std::endl;
	}
#endif
};

struct get_tasks_list : io_wrapper<http_adapter_driver> {

	::std::string static
	get_id()
	{
		return "get /get_tasks_list http/1.1";
	}

	void
	on_process(::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
	{
		if (resource_id == get_id()) {
			assert(body_fields.empty() == true);

			netcpu::message::get_tasks_list msg;
			io().native_protocol::write(msg, &get_tasks_list::on_native_protocol_get_tasks_list_written, this);

		} else 
			netcpu::http::apply_resource_processor(io(), resource_id, body_fields);
	}

	void
	on_native_protocol_get_tasks_list_written()
	{
		io().native_protocol::read(&get_tasks_list::on_native_protocol_tasks_list_read, this);
	}

	::std::string
	task_info_2_json(netcpu::message::task_info const & task)
	{
		::std::string task_state;
		switch (task.state()) {
		case netcpu::message::task_info::state_type::active:
		task_state = "active";
		break;
		case netcpu::message::task_info::state_type::pending:
		task_state = "pending";
		break;
		case netcpu::message::task_info::state_type::completed:
		task_state = "completed";
		break;
		}
		::std::string rv("{\"name\":\"" + ::std::string(task.name.data(), task.name.size()) + "\",\"short_description\":\"" + ::std::string(task.short_description.data(), task.short_description.size()) + "\",\"state\":\"" + task_state + "\",\"value\":" + ::boost::lexical_cast< ::std::string>(netcpu::message::deserialise_from_decomposed_floating<double>(task.value)));
		if (task.coefficients.size()) {
			rv += ",\"coefficients\":[";
			for (uint_fast32_t i(0); i != task.coefficients.size(); ++i) {
				rv += "{\"value\":" + 
					::boost::lexical_cast< ::std::string>(netcpu::message::deserialise_from_decomposed_floating<double>(task.coefficients[i].value))
					+ ",\"from\":" + 
					::boost::lexical_cast< ::std::string>(netcpu::message::deserialise_from_decomposed_floating<double>(task.coefficients[i].from))
					+ ",\"range\":" + 
					::boost::lexical_cast< ::std::string>(netcpu::message::deserialise_from_decomposed_floating<double>(task.coefficients[i].range))
					+ "}"; 
				if (i < task.coefficients.size() - 1)
					rv += ',';
			}
			rv += ']';
		}

		// store user_data (currently a hack for multi_logit only)
		if (task.model_id() == netcpu::models_ids::multi_logit) {
			assert(task.user_data.size());
			rv += ",\"user_data\":{\"betas_sets_size\":" + ::boost::lexical_cast< ::std::string>(static_cast<unsigned>(task.user_data(0))) + '}';
		}

		rv += '}';
		return rv;
	}

	void
	on_native_protocol_tasks_list_read()
	{
		if (netcpu::message::tasks_list::myid == io().native_protocol::read_raw.id()) {
			netcpu::message::tasks_list msg(io().native_protocol::read_raw);
			if (msg.tasks.size()) {
				::std::string result("{");
				if (msg.meta_text.size())
					result += "\"meta_text\":\"" + ::std::string(msg.meta_text.data(), msg.meta_text.size()) + "\",";
				result += "\"tasks\":[" + task_info_2_json(msg.tasks[0]);
				for (uint_fast32_t i(1); i != msg.tasks.size(); ++i)
					result += ',' + task_info_2_json(msg.tasks[i]);
				result += "]}";
				io().http_protocol::write(result);
			} else
				throw netcpu::message::exception("no tasks present yet (why dont you add some, sunshine?)");
		} else
			throw netcpu::message::exception(xxx() << "incorrect message type returned from NetCPU server. wanted(=" << netcpu::message::tasks_list::myid << "), got(=" << io().native_protocol::read_raw.id() << ')');
	}

	get_tasks_list(http_adapter_driver & io)
	: io_wrapper<http_adapter_driver>(io) {
		::std::cerr << "get_tasks_list ctor \n";
		io.http_protocol::process_callback = ::boost::bind(&get_tasks_list::on_process, this, _1, _2);
		io.http_protocol::content = "application/json";
	}

#ifndef NDEBUG
	~get_tasks_list()
	{
		::std::cerr << "dtor of get_tasks_list " << this << ::std::endl;
	}
#endif
};

struct move_task_in_list : io_wrapper<http_adapter_driver> {

	::std::string static
	get_id()
	{
		return "post /move_task_in_list http/1.1";
	}

	void
	on_process(::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
	{
		if (resource_id == get_id()) {
			assert(body_fields.empty() == false);

			netcpu::message::move_task_in_list msg;

			for (::std::list< ::std::pair< ::std::string, ::std::string> >::iterator i(body_fields.begin()); i != body_fields.end(); ++i) {
				if (i->first == "--task_name") {
					if (i->second.empty() == true)
						throw netcpu::message::exception("on move_task_in_list the task_name cannot be empty");
					msg.task_name.resize(i->second.size());
					::memcpy(msg.task_name.data(), i->second.c_str(), i->second.size());
				} else if (i->first == "--steps_to_move_by") {
					try {
						msg.steps_to_move_by(::boost::lexical_cast<int>(i->second));
					} 
					catch(::boost::bad_lexical_cast & e) {
						throw netcpu::message::exception(xxx("on move_task_in_list the steps_to_move_by argument is invalid (=") << i->second << ')');
					}
					if (!msg.steps_to_move_by())
						throw netcpu::message::exception("on move_task_in_list the steps_to_move_by argument is zero (not allowed)");
				}
			}

			if (!msg.task_name.size())
				throw netcpu::message::exception("on move_task_in_list the task_name argument is missing in the submitted http request");
			if (!msg.steps_to_move_by())
				throw netcpu::message::exception("on move_task_in_list the steps_to_move_by argument is missing");

			io().native_protocol::write(msg, &move_task_in_list::on_native_protocol_move_task_in_list_written, this);
		} else 
			netcpu::http::apply_resource_processor(io(), resource_id, body_fields);
	}

	void
	on_native_protocol_move_task_in_list_written()
	{
		io().native_protocol::read(&move_task_in_list::on_native_protocol_read, this);
	}

	void
	on_native_protocol_read()
	{
		if (netcpu::message::good::myid == io().native_protocol::read_raw.id())
			io().http_protocol::write(" { \"echo\" : \"OK\" } ");
		else
			throw netcpu::message::exception("NetCPU server didnt like the request");
	}

	move_task_in_list(http_adapter_driver & io)
	: io_wrapper<http_adapter_driver>(io) {
		::std::cerr << "move_task_in_list ctor \n";
		io.http_protocol::process_callback = ::boost::bind(&move_task_in_list::on_process, this, _1, _2);
		io.http_protocol::content = "application/json";
	}

#ifndef NDEBUG
	~move_task_in_list()
	{
		::std::cerr << "dtor of move_task_in_list ctor " << this << ::std::endl;
	}
#endif
};

struct delete_task : io_wrapper<http_adapter_driver> {

	::std::string static
	get_id()
	{
		return "post /delete_task http/1.1";
	}

	void
	on_process(::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
	{
		if (resource_id == get_id()) {
			if (body_fields.size() != 1 || body_fields.front().first != "--task_name" || body_fields.front().second.empty() == true)
				throw netcpu::message::exception("on delete_task bad fields in http request");

			netcpu::message::delete_task msg;
			msg.task_name.resize(body_fields.front().second.size());
			::memcpy(msg.task_name.data(), body_fields.front().second.c_str(), body_fields.front().second.size());

			io().native_protocol::write(msg, &delete_task::on_native_protocol_delete_task_written, this);
		} else 
			netcpu::http::apply_resource_processor(io(), resource_id, body_fields);
	}

	void
	on_native_protocol_delete_task_written()
	{
		io().native_protocol::read(&delete_task::on_native_protocol_read, this);
	}

	void
	on_native_protocol_read()
	{
		if (netcpu::message::good::myid == io().native_protocol::read_raw.id())
			io().http_protocol::write(" { \"echo\" : \"OK\" } ");
		else
			throw netcpu::message::exception("NetCPU server didnt like the request");
	}

	delete_task(http_adapter_driver & io)
	: io_wrapper<http_adapter_driver>(io) {
		::std::cerr << "delete_task ctor \n";
		io.http_protocol::process_callback = ::boost::bind(&delete_task::on_process, this, _1, _2);
		io.http_protocol::content = "application/json";
	}

#ifndef NDEBUG
	~delete_task()
	{
		::std::cerr << "dtor of delete_task " << this << ::std::endl;
	}
#endif
};

// todo -- a bit of a temporary, overy generic, hack... 
struct get_dataset_csv : io_wrapper<http_adapter_driver> {

	::std::string static
	get_id()
	{
		return "post /get_dataset_csv http/1.1";
	}

	void
	on_process(::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
	{
		if (resource_id == get_id()) {

			if (body_fields.size() != 1 || body_fields.front().first != "--task_name" || body_fields.front().second.empty() == true)
				throw netcpu::message::exception("on get_dataset_csv bad fields in http request");

			// todo -- when the http webgui delploys xmlhttpresquest (as opposed to current explicit-form sending hack) then can take the Content-Disposition setting out and let the javascript explicitly save the response int the background.
			::std::string const task_name(body_fields.front().second);

			io().http_protocol::content = "text/csv\r\nContent-Disposition: inline; filename=\"" + task_name + "_lc_dataset.csv" + "\"";

			netcpu::message::get_task_file msg;
			msg.task_name.resize(task_name.size());
			::memcpy(msg.task_name.data(), task_name.c_str(), task_name.size());

			char const static task_file_str[] = "dataset.csv";
			msg.task_file.resize(sizeof(task_file_str) - 1);
			::memcpy(msg.task_file.data(), task_file_str, sizeof(task_file_str) - 1);

			io().native_protocol::write(msg, &get_dataset_csv::on_native_protocol_get_task_file_written, this);

		} else 
			netcpu::http::apply_resource_processor(io(), resource_id, body_fields);
	}

	void
	on_native_protocol_get_task_file_written()
	{
		io().native_protocol::read(&get_dataset_csv::on_native_protocol_task_file_read, this);
	}

	void
	on_native_protocol_task_file_read()
	{
		if (netcpu::message::task_file::myid == io().native_protocol::read_raw.id()) {
			netcpu::message::task_file msg;
			msg.from_wire(io().native_protocol::read_raw);
			if (msg.bytes.size())
				return io().http_protocol::write(::std::string(reinterpret_cast<char const *>(msg.bytes.data()), msg.bytes.size()));
		} 
		io().http_protocol::content = "text/plain";
		throw netcpu::message::exception("Server refused to produce dataset.csv -- ask server admin as to why");
	}

	get_dataset_csv(http_adapter_driver & io)
	: io_wrapper<http_adapter_driver>(io) {
		io.http_protocol::process_callback = ::boost::bind(&get_dataset_csv::on_process, this, _1, _2);
	}
};

struct root : io_wrapper<http_adapter_driver> {
	::std::string static
	get_id()
	{
		return "get / http/1.1";
	}
	void
	on_process(::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
	{
		if (resource_id == get_id()) {
			//::boost::filesystem::file_size("./index.html");
			//::std::stringstream ss << f.rdbuf();
			//io().http_protocol::write(std::string(::std::istreambuf_iterator<char>(f), ::std::istreambuf_iterator<char>()));

			//file_cache static cache("./index.html");
			io().http_adapter_driver::http_protocol::write(cache.buffer.get(), cache.size);
		} else 
			netcpu::http::apply_resource_processor(io(), resource_id, body_fields);
	}
	root(http_adapter_driver & io)
	: io_wrapper<http_adapter_driver>(io) {
		io.http_protocol::process_callback = ::boost::bind(&root::on_process, this, _1, _2);
		io.http_protocol::content = "text/html";
		io.http_protocol::cache_control = "public, max-age=1800";
	}
	netcpu::http::file_cache static cache;
};
netcpu::http::file_cache root::cache("./index.html");

struct index_worker : netcpu::http::js_file<index_worker> { index_worker(http_adapter_driver & io) : js_file(io) { io.http_protocol::cache_control = "public, max-age=1800, must-revalidate"; } ::std::string static get_filepath() { return "index_worker.js"; } };

struct index_utils : netcpu::http::js_file<index_utils> { index_utils(http_adapter_driver & io) : js_file(io) { io.http_protocol::cache_control = "public, max-age=1800, must-revalidate"; } ::std::string static get_filepath() { return "index_utils.js"; } };

struct zip : netcpu::http::js_file<zip> { zip(http_adapter_driver & io) : js_file(io) { io.http_protocol::cache_control = "public, max-age=172800, must-revalidate"; } ::std::string static get_filepath() { return "third_party/zip/zip.js"; } };

struct zip_deflate : netcpu::http::js_file<zip_deflate> { zip_deflate(http_adapter_driver & io) : js_file(io) { io.http_protocol::cache_control = "public, max-age=172800, must-revalidate"; } ::std::string static get_filepath() { return "third_party/zip/deflate.js"; } };

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

		netcpu::http::ssl_server.set_options(::boost::asio::ssl::context::default_workarounds | ::boost::asio::ssl::context::no_sslv2 |  ::boost::asio::ssl::context::no_tlsv1);
		netcpu::http::ssl_server.set_password_callback(::boost::bind(&interface::passwd, this));
		netcpu::http::ssl_server.set_verify_callback(verify_callback);

		::std::string as_server_ssl_key;
		::std::string as_server_ssl_certificate;
		::std::string as_server_ssl_clients_certificates;

		::std::string server_ssl_certificate;
		::std::string client_ssl_certificate;
		::std::string client_ssl_key;

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
			} else if (!::strcmp(argv[i], "--server_at")) {
				char const * const host(::strtok(argv[++i], ":"));
				if (host == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value: [" << argv[i] << "] need 'host:port'");
				char const * const port = ::strtok(NULL, ":");
				if (port == NULL)
					throw ::std::runtime_error(xxx() << "incorrect --server_at value: [" << argv[i] << "] need 'host:port'");
				native_protocol_server_endpoints_i = endpoints.resolve(::boost::asio::ip::tcp::resolver::query(
							::boost::asio::ip::tcp::v4(), // to generate faster DNS resolution (possibly), but lesser compatible (no IP6): todo -- comment out when IP4 gets deprecated :-)
							host, port));
			}	else if (!::strcmp(argv[i], "--key")) 
				as_server_ssl_key = argv[++i];
			else if (!::strcmp(argv[i], "--cert")) 
				as_server_ssl_certificate = argv[++i];
			else if (!::strcmp(argv[i], "--client_cert")) 
				client_ssl_certificate = argv[++i];
			else if (!::strcmp(argv[i], "--client_key")) 
				client_ssl_key = argv[++i];
			else if (!::strcmp(argv[i], "--server_cert")) 
				server_ssl_certificate = argv[++i];
			else if (!::strcmp(argv[i], "--clients_certificates")) 
				as_server_ssl_clients_certificates = argv[++i];
			else
				throw ::std::runtime_error(xxx() << "unknown option: [" << argv[i] << ']');
		}

		if (native_protocol_server_endpoints_i == ::boost::asio::ip::tcp::resolver::iterator())
			throw ::std::runtime_error("must supply --server_at option with correct host:port values");
		if (as_server_ssl_key.empty() == true)
			throw ::std::runtime_error("must supply as_server_ssl private key file");
		else if (as_server_ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply as_server_ssl certificate file");
		else if (as_server_ssl_clients_certificates.empty() == true)
			throw ::std::runtime_error("must supply directory for certificates used to verify peers/clients");
		else if (client_ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply client ssl certificate file (via --client_cert option)");
		else if (client_ssl_key.empty() == true)
			throw ::std::runtime_error("must supply client ssl key file (via --client_key option)");
		else if (server_ssl_certificate.empty() == true)
			throw ::std::runtime_error("must supply server ssl certificate file (via --server_cert option)");

		netcpu::http::ssl_server.use_certificate_file(as_server_ssl_certificate, boost::asio::ssl::context::pem);
		netcpu::http::ssl_server.use_private_key_file(as_server_ssl_key, boost::asio::ssl::context::pem);

		netcpu::http::ssl_server.set_verify_mode( ::boost::asio::ssl::context::verify_peer | ::boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		netcpu::http::ssl_server.add_verify_path(as_server_ssl_clients_certificates);

		if (!::SSL_CTX_set_session_id_context(netcpu::http::ssl_server.impl(), &as_server_ssl_session_id, sizeof(as_server_ssl_session_id)))
			throw ::std::runtime_error(xxx() << "SSL_CTX_set_session_id_context");

		ssl.set_verify_mode(::boost::asio::ssl::context::verify_peer | ::boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		ssl.load_verify_file(server_ssl_certificate);

		ssl.use_certificate_file(client_ssl_certificate, boost::asio::ssl::context::pem);
		ssl.use_private_key_file(client_ssl_key, boost::asio::ssl::context::pem);
	}
};

void static
run(int argc, char * * argv)
{
	netcpu::http::resources_insert<root>();
	netcpu::http::resources_insert<index_worker>();
	netcpu::http::resources_insert<index_utils>();
	netcpu::http::resources_insert<zip>();
	netcpu::http::resources_insert<zip_deflate>();
	netcpu::http::resources_insert<new_task>();
	netcpu::http::resources_insert<get_tasks_list>();
	netcpu::http::resources_insert<move_task_in_list>();
	netcpu::http::resources_insert<delete_task>();
	netcpu::http::resources_insert<get_dataset_csv>();

	::boost::scoped_ptr<interface> ui(new interface(argc, argv));
	::boost::shared_ptr<netcpu::http::acceptor> acceptor_ptr(new netcpu::http::acceptor(ui->endpoints_i)); 
	ui.reset();
	acceptor_ptr->run();
	io.run();
	acceptor_ptr.reset();
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

#include "converger_1/controller.h"

#include "../multi_logit/controller.h"
#include "../gmnl_2a/controller.h"
#include "../gmnl_2/controller.h"
#include "../mixed_logit/controller.h"
#include "../logit/controller.h"

::censoc::netcpu::converger_1::model_factory< ::censoc::netcpu::http_adapter_driver, ::censoc::netcpu::multi_logit::model_traits, ::censoc::netcpu::models_ids::multi_logit> static multi_logit_factory;
::censoc::netcpu::converger_1::model_factory< ::censoc::netcpu::http_adapter_driver, ::censoc::netcpu::gmnl_2a::model_traits, ::censoc::netcpu::models_ids::gmnl_2a> static gmnl_2a_factory;
::censoc::netcpu::converger_1::model_factory< ::censoc::netcpu::http_adapter_driver, ::censoc::netcpu::gmnl_2::model_traits, ::censoc::netcpu::models_ids::gmnl_2> static gmnl2_factory;
::censoc::netcpu::converger_1::model_factory< ::censoc::netcpu::http_adapter_driver, ::censoc::netcpu::mixed_logit::model_traits, ::censoc::netcpu::models_ids::mixed_logit> static mixed_logit_factory;
::censoc::netcpu::converger_1::model_factory< ::censoc::netcpu::http_adapter_driver, ::censoc::netcpu::logit::model_traits, ::censoc::netcpu::models_ids::logit> static logit_factory;
