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

#include <stdlib.h>
#include <zlib.h>

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
// #include <boost/xpressive/xpressive.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

#include <censoc/empty.h>
#include <censoc/type_traits.h>
#include <censoc/lexicast.h>

//}

#ifndef CENSOC_NETCPU_HTTP_IO_DRIVER_H
#define CENSOC_NETCPU_HTTP_IO_DRIVER_H

namespace censoc { namespace netcpu { namespace http { 

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

}}}

namespace boost { namespace asio {
template <> 
struct is_match_condition<censoc::netcpu::http::match_boundaries> : ::boost::true_type {};
}}

#include "../io_driver.h"
#include "../io_wrapper.h"

namespace censoc { namespace netcpu { namespace http { 

template <typename SharedFromThisProvider>
struct http_async_driver_detail : SharedFromThisProvider, ::boost::noncopyable {

	::boost::asio::streambuf read_raw;
	::boost::asio::streambuf write_raw;
	char unsigned const * pending_deflated_payload;
	unsigned pending_deflated_payload_size;

	typedef ::boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_type;
	socket_type socket;

private:
	bool keepalive_wait;
	message::async_timer keepalive_timer;


	bool read_is_pending;
	bool write_is_pending;

protected:
	// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
	bool canceled_;
private:

	unsigned total_bytes_read;
public:
	unsigned const static max_read_capacity = 1024 * 1024 * 64;
private:

	unsigned header_size;
	unsigned body_size;

	::std::string origin;
public:
	::std::string content;
	::std::string cache_control;
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
		pending_deflated_payload_size = 0;
		cache_control = "no-cache";
	}


	void
	handshake()
	{
		assert(keepalive_timer.is_pending() == false);
		keepalive_timer.timeout(::boost::posix_time::seconds(75));
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
		} else { 
			censoc::llog() << "'error' var in http driver's async callback for ssl handshake is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << ']';
			if (error.category() == boost::asio::error::get_ssl_category()) {
				char const * const str(::ERR_error_string(error.value(), NULL));
				assert(str != NULL);
				censoc::llog() << " additional ssl caterogy description: [" << str << ']';
			}
			censoc::llog() << '\n'; 
		}
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

				::boost::posix_time::ptime t;
				bool if_modified_since_found(false);
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
						::std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
						::std::string header_value_lc(header_value);
						::std::transform(header_value_lc.begin(), header_value_lc.end(), header_value_lc.begin(), ::tolower);
						if (header_name == "origin") {
							origin = header_value;
						} else if (header_name == "connection") {
							if (header_value_lc == "keep-alive")
								keepalive_found = true;
						} else if (header_name == "content-length")
							body_size = ::boost::lexical_cast<unsigned>(header_value);
						else if (header_name == "content-type") {
							if (header_value_lc.find("multipart/form-data") != ::std::string::npos) {
#if 1
								::boost::regex e("\\W*boundary=([^\"]+)", ::boost::regex::perl | boost::regex::icase);
								::boost::match_results< ::std::string::const_iterator> m;
								if (::boost::regex_search(header_value, m, e) == true && m[1].matched == true) {
									body_boundary = "--" + m[1].str();
									::boost::trim(body_boundary);
									::std::cerr << "found boundary(=" << body_boundary << ")\n";
								}
#else
								// TODO/NOTE -- xpressive (albeit with the benefit of templated processing) is not compiling on gcc 4.7.2 from boost 1_52 due to the fact that RTTI needs to be enabled, and I really want to continue using -fno-rtti for as much as I can...
								// todo -- regex search vs match in terms of efficiency/preformance!!!
								::boost::xpressive::sregex e(::boost::xpressive::sregex::compile("\\W*boundary=([^\"]+)", ::boost::regex::perl | boost::regex::icase));
								::boost::xpressive::smatch m;
								if(::boost::xpressive::regex_search(header_value, m, e)) {
									body_boundary = "--" + m[1];
									::boost::trim(body_boundary);
									::std::cerr << "found boundary(=" << body_boundary << ")\n";
								}
#endif
							}
						} else if (header_name == "if-modified-since") {
							if_modified_since_found = true;
							// todo -- perhaps a pre-allocation -init -caching of stringstream, locale and facets et. al.
							::std::stringstream ss;
							::std::locale loc(ss.getloc());
							ss.imbue(::std::locale(loc, new ::boost::posix_time::time_input_facet("%a, %d %b %Y %H:%M:%S GMT")));
							ss << header_value;
							ss >> t;
						}
					}
				}

				if (keepalive_found == false)
					censoc::llog() <<  "I am not 100% compliant with HTTP 1.1 -- web client must request Connection: keep-alive. ";
				else if (body_size && body_boundary.empty() == false) {
					::std::cerr << "finished reading header, will start on body...\n";
					read<&http_async_driver_detail::on_body_boundary_line>();
					header_size = total_bytes_read;
				} else if (if_modified_since_found == true && netcpu::time_started >= t && cache_control.find("no-cache") == ::std::string::npos) {

					// todo -- perhaps a pre-allocation -init -caching of stringstream, locale and facets et. al.
					::std::ostringstream ss;
					::std::locale loc(ss.getloc());
					ss.imbue(::std::locale(loc, new ::boost::posix_time::time_facet("%a, %d %b %Y %H:%M:%S GMT")));
					ss << ::boost::posix_time::second_clock::universal_time();

					assert(write_is_pending == false);
					write_is_pending = true;
					::std::ostream os(&write_raw);
					os << "HTTP/1.1 304 Not Modified\r\n"
					<< "Date: " << ss.str() << "\r\n"
					// todo -- perhaps cache the whole string as "Last-Modified: blah blah"
					<< "Last-Modified: " << netcpu::time_started_as_string << "\r\n"
					<< "Access-Control-Allow-Origin: " << origin << " \r\n"
					<< "Access-Control-Allow-Cookies: true\r\n"
					<< "Access-Control-Allow-Credentials: true\r\n"
					<< "Connection: Keep-Alive\r\n"
					<< "Cache-Control: " << cache_control << "\r\n"
					<< "\r\n";

					::boost::asio::async_write(socket, write_raw, ::boost::bind(&http_async_driver_detail::on_write, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
				} else {
					assert(body_fields.empty() == true);
					::std::string resource_id(headers.front());
					::std::transform(resource_id.begin(), resource_id.end(), resource_id.begin(), ::tolower);
					process_callback(resource_id, body_fields);
				}
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
	write_common_headers(::std::ostream & os, unsigned deflated_payload_size)
	{
		os << "HTTP/1.1 200 OK\r\n";
		if (cache_control.find("no-cache") == ::std::string::npos) 
			os << "Last-Modified: " << netcpu::time_started_as_string << "\r\n";
		os << "Content-Type: " << content << "; charset=UTF-8\r\n"
		<< "Access-Control-Allow-Origin: " << origin << " \r\n"
		// << "Access-Control-Allow-Origin: * \r\n";
		<< "Access-Control-Allow-Cookies: true\r\n"
		<< "Access-Control-Allow-Credentials: true\r\n"
		<< "Connection: Keep-Alive\r\n"
		<< "Cache-Control: " << cache_control << "\r\n"
		<< "Content-Encoding: deflate\r\n"
		<< "Content-Length: " << deflated_payload_size  << "\r\n"
		<< "\r\n";
	}
	censoc::vector<uint8_t, unsigned> buffer; 
	void
	write_common(::std::string const & x)
	{
		assert(write_is_pending == false);
		assert(origin.empty() == false);
		assert(content.empty() == false);

		uLongf deflated_payload_size(x.size() * 1.03 + 12);
		if (deflated_payload_size > buffer.size())
			buffer.reset(deflated_payload_size);
		if (::compress2(buffer.data(), &deflated_payload_size, reinterpret_cast<char unsigned const *>(x.data()), x.size(), 9) != Z_OK)
			throw message::exception("HTTP driver failed to compress message");

		write_is_pending = true;

		::std::ostream os(&write_raw);
		write_common_headers(os, deflated_payload_size);
		os.write(reinterpret_cast<char const *>(buffer.data()), deflated_payload_size);
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

	void
	write(char unsigned const * deflated_payload, unsigned deflated_payload_size)
	{
		assert(write_is_pending == false);
		assert(origin.empty() == false);
		assert(content.empty() == false);
		assert(cache_control.empty() == false);
		assert(deflated_payload != NULL);
		assert(deflated_payload_size);
		write_is_pending = true;
		::std::ostream os(&write_raw);
		write_common_headers(os, deflated_payload_size);
		pending_deflated_payload = deflated_payload;
		pending_deflated_payload_size = deflated_payload_size;
		::boost::asio::async_write(socket, write_raw, ::boost::bind(&http_async_driver_detail::on_write_common_headers, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
	}
private:
	void 
	on_write_common_headers(::boost::system::error_code const & error)
	{
		if (canceled_ == true)
			return;
		if (!error && pending_deflated_payload_size) { 
			::boost::asio::async_write(socket, ::boost::asio::buffer(pending_deflated_payload, pending_deflated_payload_size), ::boost::bind(&http_async_driver_detail::on_write, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
		} else if (error != ::boost::asio::error::operation_aborted) { 
			censoc::llog() << "'error' var in async callback ('on_write') is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
		} 
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
		::std::string resource_id(headers.front());
		::std::transform(resource_id.begin(), resource_id.end(), resource_id.begin(), ::tolower);
		process_callback(resource_id, body_fields);
	}

	match_boundaries_result matcher_status;

	void
	on_body_name_line(unsigned bytes)
	{
		::std::cerr << "on body name line\n";
		::std::string const l(get_line(bytes));
		// todo -- basic hack for the time-being, also later use boost xpressive (as per code which does the boundary discovery)! (although boost 1_52 seems to call typeid and I like to use "-fno-rtti"...
		::boost::regex e("\\W*name=\"([^\"]+)\"", ::boost::regex::perl | boost::regex::icase);
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
			::std::string resource_id(headers.front());
			::std::transform(resource_id.begin(), resource_id.end(), resource_id.begin(), ::tolower);
			process_callback(resource_id, body_fields);
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
	:	read_raw(max_read_capacity), pending_deflated_payload_size(0), socket(netcpu::io, netcpu::http::ssl_server), keepalive_wait(false), read_is_pending(false), write_is_pending(false), canceled_(false), total_bytes_read(0), origin("*"), content("text/plain"), cache_control("no-cache"), cancel_pending(false)
	{
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

}}}

#endif
