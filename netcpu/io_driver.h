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

//{ #includes
#include <stdint.h>

#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>

#include <censoc/arrayptr.h>

#include "message.h"

//}

#ifndef CENSOC_NETCPU_IO_DRIVER_H
#define CENSOC_NETCPU_IO_DRIVER_H

namespace censoc { namespace netcpu { 
	
template <typename> class io_wrapper;
	
namespace message { // todo -- possibly migrate out of the "message" namespace scope

// quick and dirty... (todo -- move into outside the message namespace, and into a different file)
struct async_timer_detail : ::boost::enable_shared_from_this<async_timer_detail> {
	typedef ::boost::asio::deadline_timer timer_type; 

	timer_type timer; 
	::boost::function<void()> timeout_callback;
	::boost::function<void()> error_callback;

	int unsigned idle_wait;
	int unsigned const static idle_wait_watermark = 5; // TODO make more parametric/better

	bool is_pending;

private:
	// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
	bool canceled_;
public:

	template <typename T>
	async_timer_detail(void (T::* meth)(), T * obj)
	: timer(netcpu::io), error_callback(::boost::bind(meth, obj)), idle_wait(idle_wait_watermark), is_pending(false), canceled_(false) {
	}

	async_timer_detail(void (* meth)())
	: timer(netcpu::io), error_callback(meth), idle_wait(idle_wait_watermark), is_pending(false), canceled_(false) {
	}

	async_timer_detail()
	: timer(netcpu::io), idle_wait(idle_wait_watermark), is_pending(false), canceled_(false) {
	}

	void
	on_timeout(::boost::system::error_code const & error)
	{
		// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
		if (canceled_ == true)
			return;

		is_pending = false;
		if (!error) { 
			try {
				if (timeout_callback) 
					timeout_callback();
			} catch (netcpu::message::exception const & e) { 
				censoc::llog() << "message-related exception in timer: [" << e.what() << "]\n"; 
				if (error_callback)
					error_callback();
				else
					throw ::std::runtime_error("generic timer error");
			} catch (...) { 
				censoc::llog() << "unexpected exception in timer.\n"; 
				throw; 
			} 
		} else if (error != ::boost::asio::error::operation_aborted) { 
			censoc::llog() << "'error' var in timer's async callback is non-zero: [" << error << "]\n"; 
			if (error_callback)
				error_callback();
			else
				throw ::std::runtime_error("generic timer error");
		} 
	}

	void
	on_idlecycle(::boost::system::error_code const & error)
	{
		// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
		if (canceled_ == true)
			return;

		if (!error) { 
			if (!--idle_wait) {
				idle_wait = idle_wait_watermark;
				on_timeout(error);
			} else {
				// censoc::llog() << "idleness efficacy skip\n";
				timeout();
			}
		} else
			is_pending = false;
	}

	void
	timeout()
	{
		::boost::system::error_code const static e;
		netcpu::io.post(::boost::bind(&async_timer_detail::on_idlecycle, shared_from_this(), ::boost::ref(e)));
	}

	/**
		@note -- cancelling is final (much like in io/socket async_driver_detail 'cancel'), does not allow restarting timeouts later. if such restarting is needed -- re-create the timer object completely
		*/
	void
	cancel()
	{
		// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
		canceled_ = true;
		is_pending = false;
		timer.cancel(netcpu::ec);
	}
};

struct async_timer {
	typedef ::boost::asio::deadline_timer timer_type; 
	::boost::shared_ptr<async_timer_detail> detail;

	template <typename T>
	async_timer(void (T::* meth)(), T * obj)
	: detail(new async_timer_detail(meth, obj)) {
	}

	async_timer(void (* meth)())
	: detail(new async_timer_detail(meth)) {
	}

	async_timer()
	: detail(new async_timer_detail) {
	}

	~async_timer()
	{
		detail->error_callback = ::boost::function<void()>();
		detail->timeout_callback = ::boost::function<void()>();
		detail->cancel();
	}

	template <typename T>
	void
	timeout_callback(void (T::* meth)(), T * obj)
	{
		detail->timeout_callback = ::boost::bind(meth, obj);
	}

	void
	timeout_callback(void (* meth)())
	{
		detail->timeout_callback = meth;
	}

	/**
		@param 'x' is something like '::boost::posix_time::seconds(5)'
		*/
	void
	timeout(timer_type::duration_type const & x)
	{
		assert(is_pending() == false);
		detail->is_pending = true;
		detail->timer.expires_from_now(x);
		detail->timer.async_wait(::boost::bind(&async_timer_detail::on_timeout, detail, ::boost::asio::placeholders::error));
	}

	template <typename T>
	void
	timeout(timer_type::duration_type const & x, void (T::* meth)(), T * obj)
	{
		timeout_callback(meth, obj);
		timeout(x);
	}

	void
	timeout(timer_type::duration_type const & x, void (* meth)())
	{
		timeout_callback(meth);
		timeout(x);
	}

	bool
	is_pending() const
	{
		return detail->is_pending;
	}

	void
	timeout()
	{
		assert(is_pending() == false);
		detail->is_pending = true;
		detail->timeout();
	}

	template <typename T>
	void
	timeout(void (T::* meth)(), T * obj)
	{
		timeout_callback(meth, obj);
		timeout();
	}
	void
	timeout(void (* meth)())
	{
		timeout_callback(meth);
		timeout();
	}

	/**
		@note -- cancelling is final (much like in io/socket async_driver_detail 'cancel'), does not allow restarting timeouts later. if such restarting is needed -- re-create the timer object completely
		*/
	void
	cancel()
	{
		assert(is_pending() == true);
		detail->cancel();
	}
};

//{ quick and dirty... (todo -- move into outside the message namespace, and into a different file, and more encapsulation w.r.t. public/private/protected access qualifiers) }//
template <typename SharedFromThisProvider>
struct async_driver_detail : SharedFromThisProvider, ::boost::noncopyable {

	netcpu::message::read_wrapper read_raw; 
	netcpu::message::write_wrapper write_raw; 
	typedef ::boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_type;
	socket_type socket;

	void
	set_keepalive()
	{
		// TODO later reduce this -- currently kept so that the long-executing debug-based server build will not trigger disconnection(s)
		assert(keepalive_timer.is_pending() == false);
		keepalive_timer.timeout(boost::posix_time::seconds(700));
	}

private:
	netcpu::message::write_wrapper write_raw_keepalive; 
	bool keepalive_write_is_pending;
	bool data_written_inplaceof_keepalive_between_timeouts;
	uint_fast8_t keepalive_wait;
	message::async_timer keepalive_timer;

	void
	on_keepalive_timeout()
	{
		if (keepalive_write_is_pending == true)
			return;

#if 0
		size_type const avail(socket.in_avail(netcpu::ec));
		if (netcpu::ec &&  avail)
			keepalive_wait = 0;
		else 
#endif
			if (++keepalive_wait > 3) 
				return cancel();

		if (write_is_pending == false && data_written_inplaceof_keepalive_between_timeouts == false) {
			keepalive_write_is_pending = true;
			::boost::asio::async_write(socket, ::boost::asio::buffer(write_raw_keepalive.head(), static_cast< ::std::size_t>(write_raw_keepalive.size())), ::boost::bind(&async_driver_detail::on_write_keepalive, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
		} else {
			data_written_inplaceof_keepalive_between_timeouts = false;
			keepalive_timer.timeout(boost::posix_time::seconds(75));
		}
	}

	void
	on_write_keepalive(::boost::system::error_code const & error)
	{
		if (error && error != ::boost::asio::error::operation_aborted) 
			cancel();
		else {
			keepalive_write_is_pending = false;
			if (write_is_pending == true) {
				// todo -- make commonality-wise w.r.t. 'write' call (but no assertions, etc.), also keep in mind the 'xxx_pending' flags
				::boost::asio::async_write(socket, ::boost::asio::buffer(write_raw.head(), static_cast< ::std::size_t>(write_raw.size())), ::boost::bind(&async_driver_detail::on_write, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
			}
			keepalive_timer.timeout(boost::posix_time::seconds(75));
		}
	}

	::boost::function<void()> read_callback_;
	::boost::function<void()> write_callback_;
	::boost::function<void(::std::string const &)> error_callback_;
	::boost::function<void()> handshake_callback_;
	bool read_is_pending;

#ifndef NDEBUG
	bool on_peek_is_pending;
#endif

	bool write_is_pending;

	// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
	bool canceled_;

public:

	bool
	is_read_pending() const
	{
		return read_is_pending;
	}

	bool
	is_write_pending() const
	{
		return write_is_pending;
	}

	template <typename T>
	void
	error_callback(void (T::* meth)(::std::string const &), T * obj)
	{
		error_callback_ = ::boost::bind(meth, obj, _1);
	}

	// reading
	template <typename T>
	void
	read_callback(void (T::* meth)(), T * obj)
	{
		read_callback_ = ::boost::bind(meth, obj);
	}

	/*
	 @NOTE -- current implementation will invalidate previous contents of the 'read_raw' (message). This has implication of not calling 'read' from callback whilst still parsing/using the message which was delivered to the callback.

	 @TODO -- future refactoring may well be easily done in 2 directions:
	 i. see below 'TODO': do not action reading immediately, but rather issue a boost::asio::post event (making sure it will be actuated after the 'callback' for current 'read' returns.
	 ii. the aforementioned way will cause lesser efficiency (not that it matters that much I suppose)... but in any case -- one could, instead, simply provide a variant of the 'read' call: 'posted_read' or something similar. Then this 'posted_read' could be used by client code to issue read request whilst still reading current message. Having said this -- such an approach may well be coded-by the client code (upper level of design) anyway...
	 */
	void
	read()
	{
		assert(read_callback_ != ::boost::bind(&async_driver_detail::on_read, this));
		assert(read_is_pending == false);

		read_is_pending = true;

		// TODO -- may issue a posted message instead -- this way client code may call 'read' (for the next message) whilst still looking-at currently parsed message in on read-callback
		action_reading();
	}

private:

	void
	action_reading()
	{
		assert(read_is_pending == true);

#ifndef NDEBUG
		assert(on_peek_is_pending == false);
		on_peek_is_pending = true;
#endif

		read_raw.reset();
		::boost::asio::async_read(socket, ::boost::asio::buffer(read_raw.head(), read_raw.peek_size), ::boost::bind(&async_driver_detail::on_peek, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
	}

public:

	template <typename T>
	void
	handshake_callback(void (T::* meth)(), T * obj)
	{
		handshake_callback_ = ::boost::bind(meth, obj);
	}

	void
	handshake()
	{
		assert(handshake_callback_ != ::boost::bind(&async_driver_detail::on_handshake, this));
		socket.async_handshake(::boost::asio::ssl::stream_base::server, ::boost::bind(&async_driver_detail::on_handshake, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
	}

	void 
	on_handshake(::boost::system::error_code const & error)
	{
		// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
		if (canceled_ == true)
			return;

		if (!error) { 
			try {
				handshake_callback_();
			} catch (netcpu::message::exception const & e) { 
				censoc::llog() << "ssl handshake-related exception: [" << e.what() << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n";
				error_callback_(e.what());
			} catch (...) { 
				censoc::llog() << "unexpected exception during ssl handshake. peer at a time was: [" << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				throw; 
			} 
		} else { 
			censoc::llog() << "'error' var in async callback for ssl handshake is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
		} 
	}

	template <typename T>
	void
	read(void (T::* meth)(), T * obj)
	{
		read_callback(meth, obj);
		read();
	}

	void
	on_peek(::boost::system::error_code const & error)
	{
		// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
		if (canceled_ == true)
			return;

		if (!error) { 
#ifndef NDEBUG
			assert(on_peek_is_pending == true);
			on_peek_is_pending = false;
#endif
			assert(read_is_pending == true);
			keepalive_wait = 0;
			try {
				read_raw.from_wire_peek();
				if (read_raw.postpeek_size()) {
					::boost::asio::async_read(socket, ::boost::asio::buffer(read_raw.head() + read_raw.peek_size, static_cast< ::std::size_t>(read_raw.postpeek_size())), ::boost::bind(&async_driver_detail::on_read, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
				} else {
					if (message::keepalive::myid == read_raw.id()) {
						action_reading();
					} else {
						read_is_pending = false;
						read_callback_();
					}
				}
			} catch (netcpu::message::exception const & e) { 
				censoc::llog() << "message-related exception: [" << e.what() << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				error_callback_(e.what());
			} catch (...) { 
				censoc::llog() << "unexpected exception. peer at a time was: [" << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				throw; 
			} 
		} else if (error != ::boost::asio::error::operation_aborted) { 
			censoc::llog() << "'error' var in async callback ('on_peek') is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
		} 
	}

	void 
	on_read(::boost::system::error_code const & error)
	{
		// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
		if (canceled_ == true)
			return;

		if (!error) { 
			assert(read_is_pending == true);
			assert(message::keepalive::myid != read_raw.id());
			try {
				read_is_pending = false;
				read_callback_();
			} catch (netcpu::message::exception const & e) { 
				censoc::llog() << "message-related exception: [" << e.what() << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				error_callback_(e.what());
			} catch (...) { 
				censoc::llog() << "unexpected exception. peer at a time was: [" << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				throw; 
			} 
		} else if (error != ::boost::asio::error::operation_aborted) { 
			censoc::llog() << "'error' var in async callback ('on_read') is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
		} 
	}

	void
	write()
	{
		assert(write_callback_ != ::boost::bind(&async_driver_detail::on_write, this));
		assert(write_is_pending == false);
		write_is_pending = true;
		if (keepalive_write_is_pending == false) {
			::boost::asio::async_write(socket, ::boost::asio::buffer(write_raw.head(), static_cast< ::std::size_t>(write_raw.size())), ::boost::bind(&async_driver_detail::on_write, SharedFromThisProvider::shared_from_this(), ::boost::asio::placeholders::error));
		}
	}

	template <typename Msg>
	void
	write(Msg const & msg)
	{
		write_raw.reset();
		msg.to_wire(write_raw);
		write();
	}

	template <typename T>
	void
	write(::std::vector<uint8_t> const & wire, void (T::* meth)(), T * obj)
	{
		write_callback(meth, obj);
		write_raw.reset(wire.size());
		::memcpy(write_raw.head(), wire.data(), wire.size());
		write();
	}

	template <typename T>
	void
	write_callback(void (T::* meth)(), T * obj)
	{
		write_callback_ = ::boost::bind(meth, obj);
	}

	template <typename Msg, typename T>
	void
	write(Msg const & msg, void (T::* meth)(), T * obj)
	{
		write_callback(meth, obj);
		write(msg);
	}

	void 
	on_write(::boost::system::error_code const & error)
	{
		// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
		if (canceled_ == true)
			return;

		if (!error) { 
			data_written_inplaceof_keepalive_between_timeouts = true;
			try {
				write_is_pending = false;
				write_callback_();
			} catch (netcpu::message::exception const & e) { 
				censoc::llog() << "message-related exception: [" << e.what() << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				error_callback_(e.what());
			} catch (...) { 
				censoc::llog() << "unexpected exception. peer at a time was: [" << socket.lowest_layer().remote_endpoint(netcpu::ec) << "]\n"; 
				throw; 
			} 
		} else if (error != ::boost::asio::error::operation_aborted) { 
			censoc::llog() << "'error' var in async callback ('on_write') is non-zero: [" << error << ", " << socket.lowest_layer().remote_endpoint(netcpu::ec) << ", this: " << this << "]\n"; 
		} 
	}

	async_driver_detail()
	:	socket(netcpu::io, netcpu::ssl)
	, write_raw_keepalive(0, 0), keepalive_write_is_pending(false), data_written_inplaceof_keepalive_between_timeouts(false), keepalive_wait(0), 
	read_is_pending(false), 
#ifndef NDEBUG
	on_peek_is_pending(false),
#endif
	write_is_pending(false), 
	canceled_(false)
	{
		reset_callbacks();
		message::keepalive msg;
		msg.to_wire(write_raw_keepalive);
		keepalive_timer.timeout_callback(&async_driver_detail::on_keepalive_timeout, this);
	}

	~async_driver_detail()
	{
		canceled_ = true;
		censoc::llog() << "dtor in async_driver_detail: " << this << '\n';
	}

	// TODO -- possibly deprecate 'virtual' qualifier on these...
	void virtual on_read() {}
	void virtual on_write() {}
	void virtual on_error(::std::string const &) {}
	void virtual on_handshake() {}

	void
	reset_callbacks()
	{
		read_callback_ = ::boost::bind(&async_driver_detail::on_read, this);
		write_callback_ = ::boost::bind(&async_driver_detail::on_write, this);
		error_callback_ = ::boost::bind(&async_driver_detail::on_error, this, _1);		
		handshake_callback_ = ::boost::bind(&async_driver_detail::on_handshake, this);
		assert(write_callback_ == ::boost::bind(&async_driver_detail::on_write, this));
	}

	/**
		@note -- cancelling is final, does not allow restarting io later. if such restarting is needed -- re-create the socket object completely'
		*/
	void
	cancel()
	{
		if (canceled_ == false) {
			read_is_pending = write_is_pending = false;
#ifndef NDEBUG
			on_peek_is_pending = false;
#endif

			// this is needed due to a likely bug in currently-installed boost::asio (some callbacks, even after an OK close call, get called with !error as opposed to error == canceled)
			canceled_ = true;

			//socket.lowest_layer().cancel(netcpu::ec);

			socket.next_layer().close(netcpu::ec);
			socket.lowest_layer().close(netcpu::ec);

			if (keepalive_timer.is_pending() == true)
				keepalive_timer.cancel();
			censoc::llog() << "cancelling in async_driver_detail: " << this << ", status: " << netcpu::ec << '\n';
		}
	}

};

struct async_driver : async_driver_detail< ::boost::enable_shared_from_this<async_driver> >
{
	typedef async_driver native_protocol;
	censoc::unique_ptr<netcpu::io_wrapper<async_driver> > user_key;
};

}}}


#endif
