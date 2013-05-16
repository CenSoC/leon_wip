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

/** { @note early days still...

	@note for the time-being a very generic/consistent way of parsing the messages (later on will optimize on the basis of fixed-size messages and known-exchange sequences given a further developed protocol)

	@note the overall messaging API will abstract boost API via polymorphic interfaces. Also, the message structs/classes will explicitly and always reference the underlying "wire" struct via reference (as opposed to template-based inheritence where one could choose to either have a 'reference-wrapped' message or the one which actually has the wire object as data member 'by value'.

	This (generic message format, polymorphism, reference-defficient interfacing with the underlying wire object) was decided because the model here (i.e. the whole of netcpu) is NOT IO-bound, but rather CPU-bound. So, whilst the 'on the wire' message representation does care about reducing bandwidth (precisely because the model does not presume heavy IO-capabilities), the CPU-related message parsing and processing is expected not to care too much about how many cycles it takes to process a message -- precisely because such messages are not expected to be communicatied all the time (i.e. BY FAR the MOST amount of time will be spent by CPU calculating the likelihoods, etc. etc. etc.)

	So even though data member 'by value' is, indeed, more efficient (just as the 'client-to-base' non-polymorphic code layout -- as compared to 'base-to-client' virtual method propagation) -- it is not expected to make any of the practically-feasible difference due to the overall model being defined as CPU-bound, not IO-bound.

	So, really not concerned with speed/performance here at all...
	} */

//{ #includes
#include <stdint.h>
#include <string.h>

#include <zlib.h>

#include <list>
#include <vector>

#include <boost/static_assert.hpp>
#include <boost/integer_traits.hpp>
#include <boost/type_traits.hpp>
#include <boost/noncopyable.hpp>

#include <censoc/type_traits.h>
#include <censoc/arrayptr.h>

#include "types.h"
//}

#ifndef CENSOC_NETCPU_MESSAGE_H
#define CENSOC_NETCPU_MESSAGE_H

namespace censoc { namespace netcpu { 
	
namespace message {

namespace messages_ids {
	enum {
		version = 1,
		ready_to_process,
		task_offer, // todo -- later on perform universe-upgrade and move this message into controller_specific domain
		new_taskname, // todo -- later on perform universe-upgrade and move this message into controller_specific domain
		end_process,
		good,
		keepalive,
		bad,
		end_id // must be last
	};
}

namespace controller_messages_ids {
	enum {
		get_tasks_list = message::messages_ids::end_id,
		tasks_list,
		move_task_in_list,
		delete_task
	};
}

struct exception : public ::std::runtime_error {
	exception(char const * const x)
	: ::std::runtime_error(x) {
	}
	exception(::std::string const & x)
	: ::std::runtime_error(x) {
	}
};
	
// TODO put in a separate file

#if defined(LITTLE_ENDIAN)

template <typename Ram, int Size = sizeof(Ram)>
struct from_wire {
	void static
	eval(uint8_t const * wire, Ram & ram)
	{
		::memcpy(&ram, wire, Size);
	}
};

template <typename Ram, int Size = sizeof(Ram)>
struct to_wire {
	void static
	eval(uint8_t * wire, Ram const & ram)
	{
		::memcpy(wire, &ram, Size);
	}
};

#elif defined(BIG_ENDIAN)

template <typename Ram, int Size = sizeof(Ram)>
struct from_wire {
	void static
	eval(uint8_t const * wire, Ram & ram)
	{
		BOOST_STATIC_ASSERT(::boost::integer_traits<uint_fast8_t>::const_max >= Size);
		wire = wire + Size;
		for (uint_fast8_t i(0); i != Size; ++i)
			reinterpret_cast<uint8_t *>(&ram)[i] = *--wire;
	}
};

template <typename Ram, int Size = sizeof(Ram)>
struct to_wire {
	void static
	eval(uint8_t * wire, Ram const & ram)
	{
		BOOST_STATIC_ASSERT(::boost::integer_traits<uint_fast8_t>::const_max >= Size);
		wire = wire + Size;
		for (uint_fast8_t i(0); i != Size; ++i)
			*--wire = reinterpret_cast<uint8_t *>(&ram)[i];
	}
};

#else
#error "SPECIFY ENDIAN ORIENTATION IN THE MAKEFILE PLEASE"
#endif

template <typename Ram>
struct from_wire<Ram, 1> {
	void static
	eval(uint8_t const *wire, Ram & ram)
	{
		ram = *wire;
	}
};

template <typename Ram>
struct to_wire<Ram, 1> {
	void static
	eval(uint8_t * wire, Ram const & ram)
	{
		*wire = ram;
	}
};

typedef uint64_t size_type;

/** { @note below will use slow(ish) uint8_t (as opposed to uint_fast8_t et al). mainly due to the observation that this part is not about the speed (and, if anything, it would be good to reduce the memory print of these classes as much as possible so as to reduce the cache-hit effect on the other, performance-critical, code).
	} */
typedef uint8_t id_type;

class wrapper_base : ::boost::noncopyable {
protected:

	typedef message::size_type size_type;
	typedef censoc::param<size_type>::type size_paramtype;
	typedef message::id_type id_type;
	typedef censoc::param<id_type>::type id_paramtype;

	size_type max_cached;
	size_type max_allowed;
	censoc::array<uint8_t, size_type> buffer; 
	::std::vector<uint8_t> inflated_buffer;

	size_type message_size;
	id_type message_id;

	size_type inflated_payload_size_;

public:
	typedef censoc::lexicast< ::std::string> xxx;

	enum {peek_size = sizeof(size_type) + sizeof(id_type)};

	wrapper_base(size_paramtype max_cached = 1024, size_paramtype max_allowed = 1024 * 1024 * 32)
	: max_cached(peek_size + max_cached), max_allowed(::std::max<size_type>(peek_size, max_allowed)), buffer(this->max_cached), message_size(0), message_id(0) {
	}

	uint8_t const *
	head() const
	{
		return buffer;
	}

	// even read_wrapper needs it -- for boost::asio to populate it's contents
	uint8_t *
	head()
	{
		return buffer;
	}

	censoc::strip_const<size_paramtype>::type
	size() const
	{
		return message_size;
	}

	void
	to_wire()
	{
		message::to_wire<size_type>::eval(buffer, message_size);
		message::to_wire<id_type>::eval(buffer + sizeof(size_type), message_id);
	}

	size_type
	inflated_payload_size() const
	{
		return inflated_payload_size_;
	}

	uint8_t *
	inflated_payload_head()
	{
		return inflated_buffer.data();
	}

	uint8_t const *
	inflated_payload_head() const
	{
		return inflated_buffer.data();
	}
};

class read_wrapper : public wrapper_base {

public:

	void
	reset()
	{
		if (message_size > max_cached)
			buffer.reset(max_cached);
		message_size = 0;
	}

	void
	from_wire_peek()
	{
		message::from_wire<size_type>::eval(buffer, message_size);

		if (message_size > max_allowed || message_size < peek_size)
			throw message::exception(xxx("incorrect message size: [") << message_size << ']');

		message::from_wire<id_type>::eval(buffer + sizeof(size_type), message_id);

		if (message_size > max_cached) {
			buffer.reset(message_size);
			to_wire();
		}
	}

	size_type
	postpeek_size() const
	{
		assert(message_size >= peek_size);
		return message_size - peek_size;
	}

	censoc::strip_const<id_paramtype>::type
	id() const
	{
		return message_id;
	}

	void
	inflate()
	{
		assert(size());
		assert(postpeek_size() > sizeof(size_type));

		message::from_wire<size_type>::eval(buffer.operator+(peek_size), inflated_payload_size_);
		uLongf target_inflated_buffer_size(inflated_payload_size_ * 1.03 + 12);

		if (target_inflated_buffer_size > inflated_buffer.size())
			inflated_buffer.resize(target_inflated_buffer_size);

		if (::uncompress(inflated_buffer.data(), &target_inflated_buffer_size, buffer + (peek_size + sizeof(size_type)), size() - (peek_size + sizeof(size_type))) != Z_OK || target_inflated_buffer_size != inflated_payload_size_)
			throw message::exception("failed to inflate the message");
	}
};

class write_wrapper : public wrapper_base {

	size_type allocated_buffer_size;

public:

	using wrapper_base::inflated_payload_size;

	write_wrapper(size_paramtype max_cached, size_paramtype max_allowed)
	: wrapper_base(max_cached, max_allowed), allocated_buffer_size(wrapper_base::max_cached) {
	}

	write_wrapper()
	: allocated_buffer_size(wrapper_base::max_cached) {
	}

	void
	inflated_payload_size(size_paramtype x) 
	{
		if ((inflated_payload_size_ = x) > inflated_buffer.size())
			inflated_buffer.resize(inflated_payload_size_);
	}

	void
	zero_inflated_payload_size()
	{
		message_size = peek_size;
	}

	void
	id(id_paramtype x) 
	{
		message_id = x;
	}

	void
	deflate()
	{
		assert(allocated_buffer_size >= message_size);

		uLongf deflated_payload_size(inflated_payload_size_ * 1.03 + 12);

		size_type const target_buffer_size(deflated_payload_size + (peek_size + sizeof(size_type)));
		if (target_buffer_size > allocated_buffer_size)
			buffer.reset(allocated_buffer_size = target_buffer_size);

		if (::compress2(head() + (peek_size + sizeof(size_type)), &deflated_payload_size, inflated_buffer.data(), inflated_payload_size_, 9) != Z_OK)
			throw message::exception("failed to compress the message");

		message_size = peek_size + sizeof(size_type) + deflated_payload_size;

		message::to_wire<size_type>::eval(buffer.operator+(peek_size), inflated_payload_size_);
	}

	void
	reset()
	{
		if (allocated_buffer_size > max_cached)
			buffer.reset(allocated_buffer_size = max_cached);

		message_size = 0;
	}

	void
	reset(size_paramtype new_message_size)
	{
		if (allocated_buffer_size < new_message_size)
			buffer.reset(allocated_buffer_size = new_message_size);
		else if (allocated_buffer_size > max_cached)
			buffer.reset(allocated_buffer_size = max_cached);

		message_size = new_message_size; // 'write_raw.size()' is often called by the client code...
	}

};

/** { @note the following code (e.g. field typing and restructuring) could well have been more efficient (memory-wise, pointer-dereferencing-wise, etc.) if templates were used:
	typedef field<empty, type> field_a_type;
	field_a_type field_a;

	typedef field<field_a_type, type> field_b_type; // where field_a_type is used for determining compile-time offset of the field from the head of the wire
	field_a_type field_b;

	etc.

	But it has syntax issues:

	two lines to type (typedef as well as the variable) for every field;

	more explicit daisy-chaining of the offsets... which may further cause the wrong offsets like:

	  typedef field<empty, type> field_a_type;
		typedef field<field_a_type, type> field_b_type;
		typedef field<field_a_type, type> field_c_type; // should have used offset from field_b_type

		field_a_type field_a;
		field_b_type field_b;
		field_c_type field_c; 

	Given the aforementioned 'non IO-bound' nature of the model, the following more compact (but slower -- which is acceptable for non IO-bound solutions) method:

	field<int> field_a;
	field<char> field_b;
	field<int> field_c;

	works more intuitively, requires less typing (thus causing less risk of manual error) and makes it easier to layout the structure of the message 'on the wire'.

	Moreover, given the single-threaded nature of my model, all fields may use a static common variable offset propagation. Also, given that virtual and polymorphic calls are ok(ish) in messaging here, one can have automatic 'to_wire' being applied in some base class, so that the final message structure is rather very very simple and easy to write up.

	TODO -- will make it more encapsulated w.r.t. protected access rights in various structure members...

	} */


// quick'n'simple for the time-being 
template <typename FieldInterface>
struct fields_master_type {
	typedef ::std::list<FieldInterface *> fields_type;
	fields_type * fields;
	typename fields_type::iterator i; // must point 1 past 'current'
};

struct field_interface;
message::fields_master_type<field_interface> static fields_master;
struct fields_master_traits {
	typedef typename message::fields_master_type<message::field_interface>::fields_type fields_type;
	message::fields_master_type<message::field_interface> static & 
	get_fields_master() 
	{
		return message::fields_master;
	}
};

/**
	@note -- noncopyable is just a quick quality-assurance hack. later on will write appropriate copy ctor et al.
	*/
template <typename FieldsMasterTraits = message::fields_master_traits>
struct message_base_noid : ::boost::noncopyable  {
	typename FieldsMasterTraits::fields_type fields;
	message_base_noid()
	{
		FieldsMasterTraits::get_fields_master().fields = &fields;
		FieldsMasterTraits::get_fields_master().i = fields.end();
	}
	~message_base_noid()
	{
	}
};

/**
	@note -- noncopyable is just a quick quality-assurance hack. later on will write appropriate copy ctor et al.
 */
template <typename FieldsMasterTraits = message::fields_master_traits>
struct field_interface_base : ::boost::noncopyable {

	typedef message::size_type size_type;
	typedef censoc::param<size_type>::type size_paramtype;

	// array-specific hacks -- for arrays which contain user-defined struct which may contain scalars/custom-types. when such arrays are reset/resized/etc. they will need to know where to delete themselves from the right place in the fields list...
	typename FieldsMasterTraits::fields_type * fields;
	typename FieldsMasterTraits::fields_type::iterator my_i;
	// end of array-specific hacks

	typename FieldsMasterTraits::fields_type::value_type const deriving_this;

	field_interface_base(typename FieldsMasterTraits::fields_type::value_type deriving_this)
	: deriving_this(deriving_this) {
		assert(FieldsMasterTraits::get_fields_master().fields != NULL);
		fields = FieldsMasterTraits::get_fields_master().fields; 
		if (FieldsMasterTraits::get_fields_master().i != fields->end())
			++FieldsMasterTraits::get_fields_master().i;
		FieldsMasterTraits::get_fields_master().i = my_i = fields->insert(FieldsMasterTraits::get_fields_master().i, deriving_this);
	}
	~field_interface_base()
	{
#ifndef NDEBUG
#ifndef NDEBUG_XSLOW
		bool found(false);
		for (message::fields_master_type::fields_type::iterator i(fields->begin()); i != fields->end(); ++i) {
			if (*i == deriving_this) {
				if (found == false)
					found = true;
				else {
					found = false;
					break;
				}
			}
		}
		assert(found == true);
#endif
#endif
		assert(*my_i == deriving_this);
		assert(my_i != fields->end());

		fields->erase(my_i);
	}
};

struct field_interface : field_interface_base<> {
	field_interface()
	: field_interface_base<>(this) {
	}
	void virtual from_wire(censoc::param<message::read_wrapper>::type, size_type & offset) = 0;
	size_type virtual to_wire_size() const = 0;
	void virtual to_wire(message::write_wrapper &, size_type & offset) const = 0;
	// for some message fields (like array) -- to allow to retain the message object in scope (thus saving on re-running all those 'self-registering' contructors), but at the same time to save on memory allocated for keeping large arrays of data...
	void virtual reset() {};
};

template <unsigned Id>
struct message_base : message_base_noid<> {
	typedef censoc::lexicast< ::std::string> xxx;

	typedef message::size_type size_type;

	enum {myid = Id};

	BOOST_STATIC_ASSERT(myid);

	void virtual
	from_wire(message::read_wrapper & wrapper)
	{
		assert(wrapper.size());
		assert(wrapper.id() == myid);

		// todo -- make this a template-based arg variant (message-deriving class should know if it has any fields at compile-time)
		if (fields.empty() == false) {
			assert(wrapper.postpeek_size() > sizeof(size_type));

			wrapper.inflate();

			size_type offset(0); 
			for (typename message::fields_master_type<message::field_interface>::fields_type::iterator i(fields.begin()); i != fields.end(); ++i) 
				(*i)->from_wire(wrapper, offset);
		}
	}

	void virtual
	to_wire(message::write_wrapper & wrapper) const
	{
		// make sure have enough space
		wrapper.id(myid);

		// todo -- make this a template-based arg variant (message-deriving class should know if it has any fields at compile-time)
		if (fields.empty() == false) {
			size_type tmp(0);
			for (typename message::fields_master_type<message::field_interface>::fields_type::const_iterator i(fields.begin()); i != fields.end(); ++i) 
				tmp += (*i)->to_wire_size();

			wrapper.inflated_payload_size(tmp);

			// write-out the rest of the fields
			tmp = 0; 
			for (typename message::fields_master_type<message::field_interface>::fields_type::const_iterator i(fields.begin()); i != fields.end(); ++i) 
				(*i)->to_wire(wrapper, tmp);

			wrapper.deflate();
		} else
			wrapper.zero_inflated_payload_size();

		wrapper.to_wire();
	}
	void
	reset() 
	{
		for (typename message::fields_master_type<message::field_interface>::fields_type::iterator i(fields.begin()); i != fields.end(); ++i) 
			(*i)->reset();
	}
};

template <typename T> 
struct scalar : field_interface {
	typedef censoc::lexicast< ::std::string> xxx;

	typedef message::size_type size_type;
	typedef censoc::param<size_type>::type size_paramtype;

	typedef T data_type;
	typedef typename censoc::param<data_type>::type data_paramtype;

	BOOST_STATIC_ASSERT(::boost::is_pod<T>::value == true);
	BOOST_STATIC_ASSERT(::boost::is_unsigned<T>::value == true);

	data_type x;

	scalar()
	{
	}
	scalar(data_paramtype x)
	: x(x) {
	}
	data_paramtype
	operator()() const
	{
		return x;
	}
	void 
	operator()(data_paramtype x) 
	{
		this->x = x;
	}
	void
	from_wire(censoc::param<message::read_wrapper>::type wrapper, size_type & offset)
	{
		if (wrapper.inflated_payload_size() < offset + sizeof(data_type))
			throw message::exception(xxx("incorrect message size: [") << wrapper.inflated_payload_size() << ']');

		message::from_wire<data_type>::eval(wrapper.inflated_payload_head() + offset, x);
		offset += sizeof(data_type);
	}
	size_type
	to_wire_size() const
	{
		return sizeof(data_type);
	}
	void
	to_wire(message::write_wrapper & wrapper, size_type & offset) const
	{
		assert(wrapper.inflated_payload_size() >= offset + sizeof(data_type));
		message::to_wire<data_type>::eval(wrapper.inflated_payload_head() + offset, x);
		offset += sizeof(data_type);
	}
};

template <typename T>
typename ::boost::make_signed<T>::type 
deserialise_from_unsigned_to_signed_integral(T x)
{
	BOOST_STATIC_ASSERT(::boost::is_unsigned<T>::value == true);
	typedef typename ::boost::make_signed<T>::type signed_type;
	if (x > static_cast<T>(::std::numeric_limits<signed_type>::max())) 
		return -static_cast<signed_type>(::std::numeric_limits<T>::max() - x) - 1;
	else
		return x;
}  

template <template <typename T> class ScalarType = message::scalar>
struct decomposed_floating {
	typedef uint64_t capacity_type;
	typedef ScalarType<capacity_type> size_type;

	decomposed_floating()
	: fractional(0), exponent(0) {
	}

	size_type fractional;
	size_type exponent; // signed semantics
};

// not terribly precise, but will do for now...

float static inline frexp(float x, int * const e) { return ::frexpf(x, e); }
double static inline frexp(double x, int * const e) { return ::frexp(x, e); }

float static inline ldexp(float x, int e) { return ::ldexpf(x, e); } 
double static inline ldexp(double x, int e) { return ::ldexp(x, e); } 

template <typename F, template <typename T> class ScalarType>
void 
serialise_to_decomposed_floating(F const & from, decomposed_floating<ScalarType> & to)
{
	typedef typename ::boost::make_signed<typename decomposed_floating<ScalarType>::capacity_type>::type exponent_type;

	int exponent;
	F const fractional(message::frexp(from, &exponent));

	to.fractional(fractional * ::std::numeric_limits<exponent_type>::max());
	to.exponent(exponent);
}  

template <typename F, template <typename T> class ScalarType>
F 
deserialise_from_decomposed_floating(decomposed_floating<ScalarType> const & from)
{
	typedef typename decomposed_floating<ScalarType>::capacity_type fractional_type;
	typedef typename ::boost::make_signed<typename decomposed_floating<ScalarType>::capacity_type>::type exponent_type;

	F const static inverted_max(static_cast<F>(1) / ::std::numeric_limits<exponent_type>::max());

	return message::ldexp(deserialise_from_unsigned_to_signed_integral(from.fractional()) * inverted_max, deserialise_from_unsigned_to_signed_integral(from.exponent()));
}  


template <typename T, bool IsPrimitive = ::boost::is_class<T>::value == false ? true : false>
struct array;

template <typename T>
struct array<T, true> : field_interface {
	typedef censoc::lexicast< ::std::string> xxx;

	typedef message::size_type size_type;
	typedef censoc::param<size_type>::type size_paramtype;

	typedef T data_type;
	typedef typename censoc::param<data_type>::type data_paramtype;

	// TODO -- later-on make 'size_type' an explicit template param to array class so that it can be customized by the application which defines specific messages...
	censoc::array<data_type, size_type> buffer; 

	size_type buffer_size;
	size_type size_;
	size_type max_cached;

	array(size_paramtype max_cached = 128)
	: buffer_size(0), size_(0), max_cached(max_cached) {
	}
	data_type const &
	operator()(size_paramtype i) const
	{
		assert(i < size_);
		return buffer[i];
	}
	data_type &
	operator()(size_paramtype i) 
	{
		assert(i < size_);
		return buffer[i];
	}

	// policy-compliance (e.g. for combos_builder as used in christine/controller.h)
	data_type const & 
	operator[](size_paramtype i) const
	{
		assert(i < size_);
		return buffer[i];
	}
	data_type  &
	operator[](size_paramtype i) 
	{
		assert(i < size_);
		return buffer[i];
	}
	//---

	void
	operator()(size_paramtype i, data_paramtype x)
	{
		assert(i < size_);
		buffer[i] = x;
	}
	void
	from_wire(censoc::param<message::read_wrapper>::type wrapper, size_type & offset)
	{
		if (wrapper.inflated_payload_size() < offset + sizeof(size_type))
			throw message::exception(xxx("incorrect message size: [") << wrapper.inflated_payload_size() << ']');

		message::from_wire<size_type>::eval(wrapper.inflated_payload_head() + offset, size_);
		offset += sizeof(size_type);

		if (wrapper.inflated_payload_size() < offset + size_ * sizeof(data_type))
			throw message::exception(xxx("incorrect message size: [") << wrapper.inflated_payload_size() << "], or field's array size: [" << size_ << ']');

		if (buffer_size < size_) 
			buffer.reset(buffer_size = size_);
		else if (buffer_size > size_ && buffer_size > max_cached) 
			buffer.reset(buffer_size = max_cached);
		for (unsigned i(0); i != size_; ++i, offset += sizeof(data_type)) 
			message::from_wire<data_type>::eval(wrapper.inflated_payload_head() + offset, buffer[i]);
	}
	size_type
	to_wire_size() const
	{
		return sizeof(size_type) + size_ * sizeof(data_type);
	}
	void
	to_wire(message::write_wrapper & wrapper, size_type & offset) const
	{
		assert(wrapper.inflated_payload_size() >= offset + sizeof(size_type) + size_ * sizeof(data_type));
		message::to_wire<size_type>::eval(wrapper.inflated_payload_head() + offset, size_);
		offset += sizeof(size_type);

		// TODO -- for char types -- specialize to just call memcpy (may be just specialise this array as scalar<::std::string>
		for (unsigned i(0); i != size_; ++i, offset += sizeof(data_type)) 
			message::to_wire<data_type>::eval(wrapper.inflated_payload_head() + offset, buffer[i]);
	}
	/**
		for memory conservation whilst retaining the object in scope
		*/
	void 
	reset() 
	{
		buffer.reset(buffer_size = size_ = 0);
	}
	/**
		for resizing in preparation to use the fields
		*/
	void
	resize(size_type size) 
	{
		size_ = size;
		if (buffer_size < size_) 
			buffer.reset(buffer_size = size_);
		else if (buffer_size > size_ && buffer_size > max_cached) 
			buffer.reset(buffer_size = max_cached);
	}
	/**
		@note -- mainly for purposes of mapping to eigen2 matricies
		*/
	data_type *
	data() 
	{
		return buffer;
	}
	data_type const *
	data() const
	{
		return buffer;
	}
	size_type
	size() const
	{
		return size_;
	}
};

template <typename T>
struct array<T, false> : field_interface {
	typedef censoc::lexicast< ::std::string> xxx;

	typedef message::size_type size_type;
	typedef censoc::param<size_type>::type size_paramtype;

	typedef T data_type;
	typedef typename censoc::param<data_type>::type data_paramtype;

	// TODO -- later-on make 'size_type' an explicit template param to array class so that it can be customized by the application which defines specific messages...
	censoc::array<data_type, size_type> buffer; 
	size_type size_;

	/**
		@note -- primitive array's semantics for caching of previously-allocated fields are not applicable here because all of the subfields are registered with the master message's base (not this struct). meaning that any cached elements would still be seen 'as valid' by the master message's base/container.

		it would be possible to fix this via something like 'always pass 'this' pointer when ctoring the struct', but this would increase the complexity of the message syntax in the end-code...

		given that this is really not about speed (i.e. speed takes second proirity), shall not use any concept of 'caching'...
		*/

	array()
	: size_(0) {
	}
	data_type const & 
	operator()(size_paramtype i) const
	{
		assert(i < size_);
		return buffer[i];
	}
	data_type  &
	operator()(size_paramtype i) 
	{
		assert(i < size_);
		return buffer[i];
	}

	// policy-compliance (e.g. for combos_builder as used in christine/controller.h)
	data_type const & 
	operator[](size_paramtype i) const
	{
		assert(i < size_);
		return buffer[i];
	}
	data_type  &
	operator[](size_paramtype i) 
	{
		assert(i < size_);
		return buffer[i];
	}
	//---

	void
	operator()(size_paramtype i, data_paramtype x)
	{
		assert(i < size_);
		buffer[i] = x;
	}
	void
	from_wire(censoc::param<message::read_wrapper>::type wrapper, size_type & offset)
	{
		if (wrapper.inflated_payload_size() < offset + sizeof(size_type))
			throw message::exception(xxx("incorrect message size: [") << wrapper.size() << ']');

		message::from_wire<size_type>::eval(wrapper.inflated_payload_head() + offset, size_);
		offset += sizeof(size_type);

		/**
			@note it is ok to re-allocate the array (via 'reset') as the standard guarantees the subscript order of elements initialization in an array:
			"When an array of class objects is initialized (either explicitly or implicitly), the constructor shall be called for each element of the array, following the subscript order"
			*/
		message::fields_master.fields = fields;
		message::fields_master.i = my_i;
		buffer.reset(size_);
	}

	size_type
	to_wire_size() const
	{
		return sizeof(size_type);
	}

	void
	to_wire(message::write_wrapper & wrapper, size_type & offset) const
	{
		assert(wrapper.inflated_payload_size() >= offset + to_wire_size());
		message::to_wire<size_type>::eval(wrapper.inflated_payload_head() + offset, size_);
		offset += sizeof(size_type);
	}
	void 
	reset() 
	{
		buffer.reset(size_ = 0);
	}
	/**
		for resizing in preparation to use the fields
		*/
	void
	resize(size_type size) 
	{
		if (size_ != size) {
			message::fields_master.fields = fields;
			message::fields_master.i = my_i;
			buffer.reset(size_ = size);
		}
	}
	size_type
	size() const
	{
		return size_;
	}

};

struct version : message_base<message::messages_ids::version> {
	message::scalar<uint64_t> value;
};

struct ready_to_process : message_base<message::messages_ids::ready_to_process>  {
};

struct task_offer : message_base<message::messages_ids::task_offer> {
	typedef netcpu::task_id_typepair::wire task_id_type;
	message::scalar<task_id_type> task_id;
};

struct new_taskname : netcpu::message::message_base<message::messages_ids::new_taskname> {

	netcpu::message::array<char> name;

	void 
	print()
	{
		censoc::llog() << ::std::string(name.data(), name.size())  << '\n';
	}

	new_taskname(netcpu::message::read_wrapper & raw)
	{
		from_wire(raw);
	}

	new_taskname(::std::string const & name)
	{
		// todo -- make ::std::string message scalar more elegant (as opposed to treating it as explicit array of chars)
		this->name.resize(name.size());
		::memcpy(this->name.data(), name.c_str(), name.size());
	}
};

struct bad : message_base<message::messages_ids::bad>  {
};

struct good : message_base<message::messages_ids::good>  {
};

struct end_process : message_base<message::messages_ids::end_process>  {
};

struct keepalive : message_base<message::messages_ids::keepalive>  {
};

struct get_tasks_list : message_base<message::controller_messages_ids::get_tasks_list>  {
};


struct task_coefficient_info {
	typedef message::decomposed_floating<> float_type; 
	float_type value;
	float_type from;
	float_type range;
};

struct task_info {
	message::array<char> name;
	message::array<char> short_description;
	enum state_type { pending, completed };
	message::scalar<uint8_t> state; 
	message::decomposed_floating<> value;
	message::array<message::task_coefficient_info> coefficients;
};

struct tasks_list : message_base<message::controller_messages_ids::tasks_list>  {
	message::array<char> meta_text;
	message::array<message::task_info> tasks;
	tasks_list(message::read_wrapper & raw)
	{
		from_wire(raw);
	}
	tasks_list()
	{
	}
};

struct move_task_in_list : message::message_base<message::controller_messages_ids::move_task_in_list> {
	message::array<char> task_name;
	message::scalar<uint32_t> steps_to_move_by;
	move_task_in_list()
	: steps_to_move_by(0) {
	}
};

struct delete_task : message::message_base<message::controller_messages_ids::delete_task> {
	message::array<char> task_name;
};

}}}


#endif
