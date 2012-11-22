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
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/xpressive/xpressive.hpp>

#include <censoc/empty.h>
#include <censoc/type_traits.h>
#include <censoc/lexicast.h>

//}

namespace censoc { namespace netcpu { 

typedef censoc::lexicast< ::std::string> xxx;

::boost::system::error_code static ec;
::boost::asio::io_service static io;
::boost::asio::ssl::context static as_server_ssl(io, ::boost::asio::ssl::context::sslv23_server);
::boost::asio::ssl::context static ssl(io, ::boost::asio::ssl::context::sslv23);

struct match_boundaries_result {
	bool last;
	unsigned boundary_size;
};

struct match_boundaries {


	::std::string boundary;
	::std::string boundary_last;

	match_boundaries_result & result_status;

	match_boundaries(::std::string const & x, match_boundaries_result & result_status) 
	: boundary("\r\n" + x + "\r\n"), boundary_last("\r\n" + x + "--\r\n"), result_status(result_status) {
	}

	template <typename Iterator>
	::std::pair<Iterator, bool> 
	operator()(Iterator begin, Iterator end) const
	{
		::std::string const x(begin, end);
		::std::size_t i(x.find(boundary)); 
		if (i != ::std::string::npos) {
			::std::cerr << "found bondary\n";
			result_status.last = false;
			result_status.boundary_size = boundary.size();
			return ::std::make_pair(begin + i + boundary.size(), true);
		} else if ((i = x.find(boundary_last)) != ::std::string::npos) {
			::std::cerr << "found bondary last\n";
			result_status.last = true;
			result_status.boundary_size = boundary_last.size();
			return ::std::make_pair(begin + i + boundary_last.size(), true);
		} else if (x.size() > boundary_last.size())
			return ::std::make_pair(end - boundary_last.size(), false); 
		else
			return ::std::make_pair(begin, false);
	}
};

}}

namespace boost { namespace asio {
template <> 
struct is_match_condition<censoc::netcpu::match_boundaries> : ::boost::true_type {};
}}

namespace censoc { namespace netcpu { 

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

namespace censoc { namespace netcpu { 

::boost::asio::ip::tcp::resolver::iterator static native_protocol_server_endpoints_i;

template <typename SharedFromThisProvider>
struct http_async_driver_detail : SharedFromThisProvider, ::boost::noncopyable {

	::boost::asio::streambuf read_raw;
	::boost::asio::streambuf write_raw;

	typedef ::boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_type;
	socket_type socket;

private:
	bool keepalive_wait;
	message::async_timer keepalive_timer;


	bool read_is_pending;
	bool write_is_pending;

	// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
	bool canceled_;

	unsigned total_bytes_read;
	unsigned const static max_read_capacity = 1024 * 1024 * 30;

	unsigned header_size;
	unsigned body_size;

	::std::string origin;
public:
	::std::string content;
private:

	::std::list< ::std::string> headers;

public:
	::std::list< ::std::pair< ::std::string, ::std::string > > body_fields;

private:
	::std::string body_boundary; // todo -- for the time-being only supporting the upload of a single file (e.g. survey dataset)... besides as zippng shall be supported, one could submit multiple thingies as such...

	::std::string body_name;

public:
	::boost::function<void(::std::string const &, ::std::list< ::std::pair< ::std::string, ::std::string> > &)> process_callback;
	::boost::function<void()> handshake_callback;

private:

	void
	on_keepalive_timeout()
	{
		if (keepalive_wait == true) 
			return cancel();
		else {
			keepalive_wait = true;
			keepalive_timer.timeout(::boost::posix_time::seconds(75));
		}
	}

	template <void (http_async_driver_detail::*async_callback) (unsigned)>
	void
	read()
	{
		assert(read_is_pending == false);
		read_is_pending = true;
		::boost::asio::async_read_until(socket, read_raw, "\r\n", ::boost::bind(&http_async_driver_detail::on_read<async_callback>, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error, ::boost::asio::placeholders::bytes_transferred)); 
	}

	void 
	on_write(::boost::system::error_code const & error)
	{
		if (canceled_ == true)
			return;
		if (!error) { 

			body_boundary.clear();
			body_name.clear();
			total_bytes_read = header_size = body_size = 0;

			headers.clear();
			body_fields.clear();

			keepalive_wait = false;
			assert(write_is_pending == true);
			write_is_pending = false;
			read_header_line();

		} else if (error != ::boost::asio::error::operation_aborted) { 
			censoc::llog() << "'error' var in async callback ('on_write') is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
		} 
	}

	void empty_on_process(::std::string const &, ::std::list< ::std::pair< ::std::string, ::std::string> > const &) { }

public:

	void
	read_header_line()
	{
		read<&http_async_driver_detail::on_header_line>();
	}

	void
	reset_callbacks()
	{
		process_callback = ::boost::bind(&http_async_driver_detail::empty_on_process, this, _1, _2);
	}


	void
	handshake()
	{
		socket.async_handshake(::boost::asio::ssl::stream_base::server, ::boost::bind(&http_async_driver_detail::on_handshake, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
	}

private:

	void 
	on_handshake(::boost::system::error_code const & error)
	{
		if (canceled_ == true)
			return;
		else if (!error) {
			try{
				assert(handshake_callback);
				handshake_callback();
			} catch (netcpu::message::exception const & e) { 
				abandon_cancel();
				write_json_error(e.what());
				censoc::llog() << "message-related exception: [" << e.what() << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
			} catch (...) { 
				censoc::llog() << "unexpected exception. peer at a time was: [" << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				throw; 
			} 
		} else 
			censoc::llog() << "'error' var in async callback for ssl handshake is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
	}

	::std::string 
	get_line(unsigned bytes) 
	{
		//read_raw.commit(bytes);
		::std::string line(::boost::asio::buffer_cast<char const *>(read_raw.data()), bytes);
		read_raw.consume(bytes);
#if 0
		// old way
		::std::istream is(&read_raw);
		::std::string line;
		::std::getline(is, line);
#endif
		::std::cerr << line;
		::boost::trim(line);
		::std::transform(line.begin(), line.end(), line.begin(), ::tolower);
		return line;
	}

	void
	on_header_line(unsigned bytes)
	{
		::std::string line(get_line(bytes));
		if (line.empty() == false) {
			headers.push_back(line);
			read_header_line();
		} else {
			if (headers.empty() == false) {

				bool keepalive_found(false);
				::std::list< ::std::string>::const_iterator i(headers.begin());
				assert(i != headers.end());
				assert(i->empty() == false);
				while (++i != headers.end()) {
					unsigned const delim_pos(i->find_first_of(':'));
					if (delim_pos < i->size() - 1) {
						::std::string header_name(i->substr(0, delim_pos));
						::boost::trim(header_name);
						::std::string header_value(i->substr(delim_pos + 1));
						::boost::trim(header_value);
						if (header_name == "origin") {
							origin = header_value;
						} else if (header_name == "connection") {
							if (header_value == "keep-alive")
								keepalive_found = true;
						} else if (header_name == "content-length")
							body_size = ::boost::lexical_cast<unsigned>(header_value);
						else if (header_name == "content-type") {
							if (header_value.find("multipart/form-data") != ::std::string::npos) {
#if 0
								::boost::regex e("\\W*boundary=([^\"]+)");
								::boost::match_results< ::std::string::const_iterator> m;
								if (::boost::regex_search(header_value, m, e) == true && m[1].matched == true) {
									body_boundary = "--" + m[1].str();
									::boost::trim(body_boundary);
									::std::cerr << "found boundary(=" << body_boundary << ")\n";
								}
#else
								// todo -- regex search vs match in terms of efficiency/preformance!!!
								::boost::xpressive::sregex e(::boost::xpressive::sregex::compile("\\W*boundary=([^\"]+)"));
								::boost::xpressive::smatch m;
								if(::boost::xpressive::regex_search(header_value, m, e)) {
									body_boundary = "--" + m[1];
									::boost::trim(body_boundary);
									::std::cerr << "found boundary(=" << body_boundary << ")\n";
								}
#endif
							}
						}
					}
				}

				if (keepalive_found == false)
					censoc::llog() <<  "I am not 100% compliant with HTTP 1.1 -- web client must request Connection: keep-alive. ";
				else if (body_size && body_boundary.empty() == false) {
					::std::cerr << "finished reading header, will start on body...\n";
					read<&http_async_driver_detail::on_body_boundary_line>();
					header_size = total_bytes_read;
				} else
					process_callback(headers.front(), body_fields);
			}
		}
	}

public:

	void
	write_not_found()
	{
		assert(write_is_pending == false);
		write_is_pending = true;
		::std::ostream os(&write_raw);
		os << "HTTP/1.1 404 Not Found\r\n\r\n";
		::boost::asio::async_write(socket, write_raw, ::boost::bind(&http_async_driver_detail::on_write, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
	}

private:
	void
	write_common(::std::string const & x)
	{
		assert(write_is_pending == false);
		assert(origin.empty() == false);
		assert(content.empty() == false);
		write_is_pending = true;
		::std::ostream os(&write_raw);
		os << "HTTP/1.1 200 OK\r\n"
		<< "Content-Type: " << content << "; charset=UTF-8\r\n"
		<< "Access-Control-Allow-Origin: " << origin << " \r\n"
		// << "Access-Control-Allow-Origin: * \r\n";
		<< "Access-Control-Allow-Cookies: true\r\n"
		<< "Access-Control-Allow-Credentials: true\r\n"
		<< "Connection: Keep-Alive\r\n"
		<< "Content-Length: " << x.size()  << "\r\n"
		<< "\r\n"
		<< x;
	}

	void
	empty_on_write(::boost::system::error_code const &)
	{
		cancel();
	}

public:

	void
	write(::std::string const & x)
	{
		write_common(x);
		::boost::asio::async_write(socket, write_raw, ::boost::bind(&http_async_driver_detail::on_write, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
	}

protected:
	void
	write_json_error(::std::string const & x)
	{
		write_common("{\"error\":\"" + x + "\"}");
		::boost::asio::async_write(socket, write_raw, ::boost::bind(&http_async_driver_detail::empty_on_write, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
	}

private:
	void
	on_body_boundary_line(unsigned bytes)
	{
		::std::cerr << "on body boundary line\n";
		::std::string const l(get_line(bytes));
		if (l == body_boundary) {
			::std::cerr << "issuing on body name line\n";
			read<&http_async_driver_detail::on_body_name_line>();
		} else {
			::std::cerr << "issuing on body boundary line\n";
			read<&http_async_driver_detail::on_body_boundary_line>();
		}
	}

	void
	on_wind_out_the_rest_of_contents(unsigned)
	{
		process_callback(headers.front(), body_fields);
	}

	match_boundaries_result matcher_status;

	void
	on_body_name_line(unsigned bytes)
	{
		::std::cerr << "on body name line\n";
		::std::string const l(get_line(bytes));
		// todo -- basic hack for the time-being, also later use boost xpressive (as per code which does the boundary discovery)!
		::boost::regex e("\\W*name=\"([^\"]+)\"");
		::boost::match_results< ::std::string::const_iterator> m;
		if (::boost::regex_search(l, m, e) == true && m[1].matched == true) {
			body_name = m[1].str();

			e.assign("\\W*filename=\"([^\"]+)\"");
			if (::boost::regex_search(l, m, e) == true && m[1].matched == true)
				body_name += ::boost::filesystem::extension(m[1].str());

			read<&http_async_driver_detail::on_body_name_line>();
		} else if (l.empty() == false)
			read<&http_async_driver_detail::on_body_name_line>();
		else {
			::std::cerr << "issuing on body value line\n";
			assert(read_is_pending == false);
			read_is_pending = true;
			::boost::asio::async_read_until(socket, read_raw, match_boundaries(body_boundary, matcher_status), ::boost::bind(&http_async_driver_detail::on_read<(&http_async_driver_detail::on_body_value_line)>, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error, ::boost::asio::placeholders::bytes_transferred)); 
		}
	}

	void
	on_body_value_line(unsigned bytes)
	{
		::std::cerr << "on body value line\n";
		//::std::istream is(&read_raw);

		//read_raw.commit(bytes);

		// todo in the specialized code -- write file from stream...  if (zipped -- use libarchive) { }

		::std::cerr << "body value size: " << bytes - matcher_status.boundary_size << ::std::endl;

		assert(bytes >= matcher_status.boundary_size);
		body_fields.push_back(::std::make_pair(body_name, ::std::string(::boost::asio::buffer_cast<char const *>(read_raw.data()), bytes - matcher_status.boundary_size)));

		read_raw.consume(bytes);

		unsigned const body_bytes_read(total_bytes_read - header_size);

		if (matcher_status.last == false)
			read<&http_async_driver_detail::on_body_name_line>();
		else if (body_bytes_read == body_size) {
			::std::cerr << "issing porecssing callback\n";
			process_callback(headers.front(), body_fields);
		} else if (body_size > body_bytes_read) {
			::std::cerr << "winding out... body_size= " << body_size << ", body_bytes_read " << body_bytes_read << "\n";
			assert(read_is_pending == false);
			read_is_pending = true;
			::boost::asio::async_read(socket, read_raw.prepare(body_size - body_bytes_read), ::boost::bind(&http_async_driver_detail::on_read<(&http_async_driver_detail::on_wind_out_the_rest_of_contents)>, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error, ::boost::asio::placeholders::bytes_transferred)); 
		}
	}

	template <void (http_async_driver_detail::*callback) (unsigned)>
	void 
	on_read(::boost::system::error_code const & error, ::std::size_t bytes)
	{
		if (canceled_ == true)
			return;
		if (!error) { 
			if ((total_bytes_read += bytes) < max_read_capacity) {
				keepalive_wait = false;
				assert(read_is_pending == true);
				read_is_pending = false;
				try {
					(this->*callback)(bytes);
				} catch (netcpu::message::exception const & e) { 
					abandon_cancel();
					write_json_error(e.what());
					censoc::llog() << "message-related exception: [" << e.what() << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				} catch (...) { 
					censoc::llog() << "unexpected exception. peer at a time was: [" << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
					throw; 
				} 
			} else {
				write_json_error("too many bytes in request...");
				censoc::llog() << "too many bytes in request..."; 
			}
		} else if (error != ::boost::asio::error::operation_aborted) { 
			censoc::llog() << "'error' var in async callback ('on_read') is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
		} 
	}

	bool cancel_pending;

public:
	http_async_driver_detail()
	:	read_raw(max_read_capacity), socket(netcpu::io, netcpu::as_server_ssl), keepalive_wait(false), read_is_pending(false), write_is_pending(false), canceled_(false), total_bytes_read(0), origin("*"), content("text/plain"), cancel_pending(false)
	{
		assert(keepalive_timer.is_pending() == false);
		keepalive_timer.timeout(::boost::posix_time::seconds(75));
		keepalive_timer.timeout_callback(&http_async_driver_detail::on_keepalive_timeout, this);
		reset_callbacks();
	}

	~http_async_driver_detail()
	{
		canceled_ = true;
		censoc::llog() << "dtor in http_async_driver_detail: " << this << '\n';
	}

	void
	on_cancel()
	{
		if (cancel_pending == true) {
			cancel_pending = false;
			read_is_pending = write_is_pending = false;
			canceled_ = true;
			socket.next_layer().close(netcpu::ec);
			socket.lowest_layer().close(netcpu::ec);
			if (keepalive_timer.is_pending() == true)
				keepalive_timer.cancel();
			censoc::llog() << "cancelling in http_async_driver_detail: " << this << ", status: " << netcpu::ec << '\n';
		}
	}

public:

	/**
		@note -- cancelling is final, does not allow restarting io later. if such restarting is needed -- re-create the socket object completely'
		*/
	void
	cancel()
	{
		if (canceled_ == false) {
			cancel_pending = true;
			netcpu::io.post(::boost::bind(&http_async_driver_detail::on_cancel, SharedFromThisProvider::shared_from_this()));
		}
	}

	void
	abandon_cancel() 
	{
		cancel_pending = false;
	}

};

template <typename T>
struct virtual_enable_shared_from_this : virtual ::boost::enable_shared_from_this<T> {
};

struct http_adapter_driver : netcpu::message::async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> >, http_async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> > {

	typedef netcpu::message::async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> > native_protocol;
	typedef http_async_driver_detail<virtual_enable_shared_from_this<http_adapter_driver> > http_protocol;

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

	bool
	canceled() const 
	{
		return false;
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
		if (native_protocol::canceled() == false)
			native_protocol::cancel();
		http_protocol::cancel();
	}
};

typedef ::std::map< ::std::string, void (*) (http_adapter_driver &, ::std::string const &, ::std::list< ::std::pair< ::std::string, ::std::string> > &) > resources_type;
resources_type static resources;
void static
apply_resource_processor(http_adapter_driver & io, ::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
{
	::std::cerr << "finding :" << resource_id << ::std::endl;
	resources_type::const_iterator i(resources.find(resource_id));
	if (i != resources.end())
		(i->second)(io, resource_id, body_fields);
	else
		io.http_protocol::write_not_found();
}

struct new_task : io_wrapper<http_adapter_driver> {

	unsigned model_id;
	::boost::shared_ptr<model_meta> mm;

	::std::string static
	get_id()
	{
		return "post /new_task http/1.1";
	}

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
						::archive * a(::archive_read_new());
						if (a != NULL) {
							::archive_entry *ae;
							if (::archive_read_support_compression_compress(a) == ARCHIVE_OK && ::archive_read_support_format_zip(a) == ARCHIVE_OK && 
									// me thinks libarchive has an interface bug... at least w.r.t. doco/implementation, the buf data argument (no. 2) should have been declared const... alas an ugly cast for the time being... if later versions of libarchive become more explicit w.r.t. whether the buf data is modified during call then will adjust/re-factor later...
									::archive_read_open_memory(a, const_cast<char *>(i->second.data()), i->second.size()) == ARCHIVE_OK 
									&& ::archive_read_next_header(a, &ae) == ARCHIVE_OK && ae != NULL) {
								unsigned file_size(::archive_entry_size(ae));
								if (file_size) {
									::boost::scoped_ptr<char> d(new char[file_size]);
									if (::archive_read_data(a, d.get(), file_size) == file_size) {
										mm->args.push_back(::std::pair< ::std::string, ::std::string>("--filedata", ::std::string(d.get(), file_size)));
										// ::boost::stream< ::boost::array_source> stream(d.get(), file_size);
									}
								}
							}
							::archive_read_finish(a);
						}
					} else if (extension == ".csv") {
						i->first.erase(i->first.size() - 4);
						mm->args.push_back(*i);
					} else
						throw netcpu::message::exception(xxx() << "invalid file format upload (only csv and zip files supported)");
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
			apply_resource_processor(io(), resource_id, body_fields);
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

// NOTE -- cludge only, this is really a continuation for 'new_task' struct only (after the native protocol is done with loading the job)... will fix/refactor later when there's more time...
struct peer_connection : io_wrapper<http_adapter_driver> {
	peer_connection(http_adapter_driver & io) : io_wrapper<http_adapter_driver>(io)
	{
		io.http_protocol::process_callback = ::boost::bind(&peer_connection::on_process, this, _1, _2);
	}

	void
	on_process(::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
	{
		apply_resource_processor(io(), resource_id, body_fields);
	}

	void
	on_new_in_chain(::std::string const & feedback_string)
	{
		io().http_protocol::write(feedback_string);
	}
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
			apply_resource_processor(io(), resource_id, body_fields);
	}

	void
	on_native_protocol_get_tasks_list_written()
	{
		io().native_protocol::read(&get_tasks_list::on_native_protocol_tasks_list_read, this);
	}

	::std::string
	task_info_2_json(netcpu::message::task_info const & task)
	{
		::std::string rv("{\"name\":\"" + ::std::string(task.name.data(), task.name.size()) + "\",\"state\":\"" + (task.state() == netcpu::message::task_info::state_type::pending ? "pending" : "complete") + "\"");
		if (task.coefficients.size()) {
			rv += ",\"coefficients\":[";
			for (uint_fast32_t i(0); i != task.coefficients.size(); ++i) {
				rv += "{\"value\":\"" + 
					::boost::lexical_cast< ::std::string>(netcpu::message::deserialise_from_decomposed_floating<double>(task.coefficients[i].value))
					+ "\",\"from\":\"" + 
					::boost::lexical_cast< ::std::string>(netcpu::message::deserialise_from_decomposed_floating<double>(task.coefficients[i].from))
					+ "\",\"range\":\"" + 
					::boost::lexical_cast< ::std::string>(netcpu::message::deserialise_from_decomposed_floating<double>(task.coefficients[i].range))
					+ "\"}"; 
				if (i < task.coefficients.size())
					rv += ',';
			}
			rv += ']';
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
				::std::string result("{\"tasks\":[" + task_info_2_json(msg.tasks[0]));
				for (uint_fast32_t i(1); i != msg.tasks.size(); ++i)
					result += "," + task_info_2_json(msg.tasks[i]);
				result += ("]}");
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
					msg.steps_to_move_by(::boost::lexical_cast<int>(i->second));
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
			apply_resource_processor(io(), resource_id, body_fields);
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
			apply_resource_processor(io(), resource_id, body_fields);
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
			::std::ifstream f("./index.html", ::std::ios::binary);
			//::std::stringstream ss << f.rdbuf();
			io().http_protocol::write(std::string(::std::istreambuf_iterator<char>(f), ::std::istreambuf_iterator<char>()));
		} else 
			apply_resource_processor(io(), resource_id, body_fields);
	}

	root(http_adapter_driver & io)
	: io_wrapper<http_adapter_driver>(io) {
		io.http_protocol::process_callback = ::boost::bind(&root::on_process, this, _1, _2);
		io.http_protocol::content = "text/html";
	}
};


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

		as_server_ssl.set_options(::boost::asio::ssl::context::default_workarounds | ::boost::asio::ssl::context::no_sslv2 |  ::boost::asio::ssl::context::no_tlsv1);
		as_server_ssl.set_password_callback(::boost::bind(&interface::passwd, this));
		as_server_ssl.set_verify_callback(verify_callback);

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

		as_server_ssl.use_certificate_file(as_server_ssl_certificate, boost::asio::ssl::context::pem);
		as_server_ssl.use_private_key_file(as_server_ssl_key, boost::asio::ssl::context::pem);

		as_server_ssl.set_verify_mode( ::boost::asio::ssl::context::verify_peer | ::boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		as_server_ssl.add_verify_path(as_server_ssl_clients_certificates);

		ssl.set_verify_mode(::boost::asio::ssl::context::verify_peer | ::boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		ssl.load_verify_file(server_ssl_certificate);

		ssl.use_certificate_file(client_ssl_certificate, boost::asio::ssl::context::pem);
		ssl.use_private_key_file(client_ssl_key, boost::asio::ssl::context::pem);
	}
};

struct acceptor : ::boost::enable_shared_from_this<acceptor> {
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
		::boost::shared_ptr<netcpu::http_adapter_driver> new_driver(new netcpu::http_adapter_driver);
		new peer_connection(*new_driver);
		newcon.async_accept(new_driver->http_protocol::socket.lowest_layer(), ::boost::bind(&acceptor::on_accept, shared_from_this(), new_driver, ::boost::asio::placeholders::error));
	}

	void 
	on_accept(::boost::shared_ptr<netcpu::http_adapter_driver> driver, ::boost::system::error_code const & error) throw()
	{
		if (!error) {
			censoc::llog() << "ON_ACCEPT\n";
			driver->http_protocol::handshake();

			::boost::shared_ptr<netcpu::http_adapter_driver> new_driver(new netcpu::http_adapter_driver);
			new peer_connection(*new_driver);
			newcon.async_accept(new_driver->http_protocol::socket.lowest_layer(), ::boost::bind(&acceptor::on_accept, shared_from_this(), new_driver, ::boost::asio::placeholders::error));
		}
	}

	::boost::asio::ip::tcp::acceptor newcon;
};

template <typename T>
void static
delegate(http_adapter_driver & xxx, ::std::string const & resource_id, ::std::list< ::std::pair< ::std::string, ::std::string> > & body_fields)
{
	typedef typename http_adapter_driver::http_protocol http_protocol;
	(new T(xxx))->io().http_protocol::process_callback(resource_id, body_fields);
}

template <typename T>
void static
resources_insert()
{
	resources.insert(::std::make_pair(T::get_id(), &delegate<T>));
}

void static
run(int argc, char * * argv)
{
	resources_insert<root>();
	resources_insert<new_task>();
	resources_insert<get_tasks_list>();
	resources_insert<move_task_in_list>();
	resources_insert<delete_task>();

	::boost::scoped_ptr<interface> ui(new interface(argc, argv));
	::boost::shared_ptr<netcpu::acceptor> acceptor_ptr(new acceptor(ui->endpoints_i)); 
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

#include "../gmnl_2a/controller.h"
::censoc::netcpu::converger_1::model_factory< ::censoc::netcpu::http_adapter_driver, ::censoc::netcpu::gmnl_2a::model_traits, ::censoc::netcpu::models_ids::gmnl_2a> static gmnl_2a_factory;

