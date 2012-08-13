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

#include <assert.h>

#include <vector>
#include <list>
#include <map>

#include <censoc/type_traits.h>

#ifndef CENSOC_NETCPU_COMBOS_BUILDER_H
#define CENSOC_NETCPU_COMBOS_BUILDER_H


namespace censoc { namespace netcpu { 

template <typename size_type, typename CoeffMetadataVector>
class combos_builder {

	typedef typename censoc::param<size_type>::type size_paramtype;

	struct combo_instance {
		::std::vector<size_type> coeffs; 
		/**
			@inv -- offsets.size() == coeffs.size() - 1
			 */
		::std::vector<size_type> offsets; 
	};


	typedef ::std::map<size_type, combo_instance> combos_type;
	typedef typename combos_type::iterator combos_iterator_type;
	typedef typename combos_type::const_iterator combos_const_iterator_type;

	combos_type combos;
	/**
		@note -- firstly for the server-side to decide how to distribute complexities so that the lower combos are done as a matter of priority, even at the expense of "double-loading" the same compexlity range to different processoring nodes. Secondly, for the code (e.g. in demodulate) to know grid-res for a coeff in a given combo (as grid-res is subject to "simplification scaling" for further coeffs-at-once levels).
		*/
	::std::map<size_type, ::std::vector<size_type> > combos_metadata;

public:

	::std::map<size_type, ::std::vector<size_type> > const & 
	metadata() const 
	{
		return combos_metadata;
	}

	void
	build(size_paramtype coefficients_size, size_type & complexity_size, size_type & coeffs_atonce_size, typename censoc::param<CoeffMetadataVector>::type coefficients_metadata)
	{
		assert(complexity_size);

		combos.clear();
		combos_metadata.clear();

		size_type complexity_index(0);
		size_type i(-1);
		assert(coeffs_atonce_size <= coefficients_size);
		while (//i != coefficients_size && 
				++i != coeffs_atonce_size) { 
			if (build_combo(i + 1, coefficients_size, complexity_index, complexity_size, coefficients_metadata) == false) {
				if (!i) {
					combos.clear();
					complexity_size = coeffs_atonce_size = 0;
					return;
				} else {
					combos_iterator_type j(combos.lower_bound(combos_metadata.rbegin()->first));
					assert(j != combos.end());
					combos.erase(j, combos.end());
					--i;
					break;
				}
			}

			::std::pair<typename ::std::map<size_type, ::std::vector<size_type> >::iterator, bool> combos_metadata_i(
				combos_metadata.insert(::std::make_pair(complexity_index, ::std::vector<size_type>()))
			);
			assert(combos_metadata_i.second == true);
			assert(combos_metadata_i.first->first == complexity_index);
			combos_metadata_i.first->second.resize(coefficients_size);
			for (size_type k(0); k != coefficients_size; ++k) 
				combos_metadata_i.first->second[k] = ::std::max(coefficients_metadata[k].grid_resolutions[i], static_cast<size_type>(2));

		}

		complexity_size = combos_metadata.rbegin()->first;
		coeffs_atonce_size = combos_metadata.size();
	}

private:

	/**
		@arg 'combo_size' one-based, how many coeffs-at-once are in this combo
		@arg 'coefficients_size' one-based total number of coefficients in the model (not just the current combo)
		*/
	bool
	build_combo(size_paramtype combo_size, size_paramtype coefficients_size, size_type & complexity_index, size_paramtype complexity_size, typename censoc::param<CoeffMetadataVector>::type coefficients_metadata)
	{
		assert(complexity_index != complexity_size);

		::std::vector<size_type> transient_vector(combo_size);
		for (size_type i(0); i != combo_size; ++i) 
			transient_vector[i] = i;
		return build_combo_instance(combo_size, 0, coefficients_size, transient_vector, complexity_index, complexity_size, coefficients_metadata);
	}

	/**
		@arg 'combo_size' one-based, how many coeffs-at-once are in this combo
		@arg 'combo_i' zero-based index indicating currently built-for coefficient member in the current combo instance (e.g. is it 1st, 2nd, etc. in the N-at-once combo)
		@arg 'coefficients_size' one-based total number of coefficients in the model (not just the current combo)
	  @arg 'transient_vector' holds actual coefficients indicies for a currently built-for combo intsance
		*/
	bool
	build_combo_instance(size_paramtype combo_size, size_paramtype combo_i, size_paramtype coefficients_size, ::std::vector<size_type> & transient_vector, size_type & complexity_index, size_paramtype complexity_size, typename censoc::param<CoeffMetadataVector>::type coefficients_metadata)
	{
		assert(combo_size);
		assert(complexity_index != complexity_size);
		assert(transient_vector.size() == combo_size);

		size_type const combo_end_i(combo_size - 1);
		size_type start_i(transient_vector[combo_i]);
		size_type const end_i(coefficients_size - (combo_end_i - combo_i));

		assert(start_i != end_i);

		if (combo_i != combo_end_i) {
			for (size_type i(start_i); i != end_i; transient_vector[combo_i] = ++i) {
				assert(transient_vector[combo_i] < coefficients_size);
				if (build_combo_instance(combo_size, combo_i + 1, coefficients_size, transient_vector, complexity_index, complexity_size, coefficients_metadata) == false)
					return false;
			}
			for (size_type i(combo_i); i != combo_size; ++i) 
				transient_vector[i] = ++start_i;
			return true;
		} else {
			for (size_type i(start_i); i != end_i; transient_vector[combo_i] = ++i) {
				assert(transient_vector[combo_i] < coefficients_size);
				combos_iterator_type combos_i = combos.insert(::std::make_pair(complexity_index, combo_instance())).first;
				combos_i->second.coeffs = transient_vector;

				if (combo_size > 1)
					combos_i->second.offsets.resize(combo_size - 1);

				size_type accumulator(::std::max(coefficients_metadata[transient_vector[0]].grid_resolutions[combo_end_i], static_cast<size_type>(2)));
				for (size_type j(1); j != combo_size; ++j) {

					assert(combos_i->second.offsets.size() > j - 1);
					combos_i->second.offsets[j - 1] = accumulator;

					assert(transient_vector[j] < coefficients_size);

					size_type const new_accumulator(::std::max(coefficients_metadata[transient_vector[j]].grid_resolutions[combo_end_i], static_cast<size_type>(2)) * accumulator);
					// protect against wraparound
					if (new_accumulator < accumulator) {
						return false;
					} else 
						accumulator = new_accumulator;
				}

				size_type const new_complexity_index(complexity_index + accumulator);
				if (new_complexity_index < complexity_index // protect against wraparound
				|| new_complexity_index > complexity_size
				) { 
					return false;
				} else
					complexity_index = new_complexity_index;
			}
			transient_vector[combo_i] = start_i + 1;
			return true;
		}
	}

public:
	void
	print() const
	{
		for (combos_const_iterator_type j(combos.begin()); j != combos.end(); ++j) {
			for (size_type k(0); k != j->second.coeffs.size(); ++k)
				::std::clog << j->second.coeffs[k] << " ";
			::std::clog << ::std::endl;
		}
	}

	// temporary hack for auxiliary projects, basically loads all of the possible combinations of underlying coefficients indicies
#if 1
	template <typename T>
	void
	load_all_combos(::std::list< ::std::list<T> > & target) const
	{
		for (combos_const_iterator_type j(combos.begin()); j != combos.end(); ++j) {
			target.push_back(::std::list<T>());
			for (size_type k(0); k != j->second.coeffs.size(); ++k) {
				T tmp;
				tmp.index = j->second.coeffs[k];
				target.back().push_back(tmp);
			}
		}
	}
#endif

#ifndef NDEBUG
	size_type // ret by val because vector.size returns by val
	calculate_combo_size(size_paramtype complexity)
	{
		combos_const_iterator_type i(combos.lower_bound(complexity));

		if (i == combos.end() || i->first != complexity)
			--i;

		return i->second.coeffs.size();
	}
#endif

	void
	demodulate(size_paramtype complexity, CoeffMetadataVector & coefficients_metadata, size_paramtype coefficients_size)
	{
		combos_const_iterator_type i(combos.lower_bound(complexity));

		if (i == combos.end() || i->first != complexity)
			--i;


		combo_instance const & ci(i->second);

		assert(complexity >= i->first);
		size_type base_offset(complexity - i->first);

		assert(ci.coeffs.size());

#ifndef NDEBUG
		// driving on the fact that 'ci.coeffs' are sorted! 
		{
			size_type min(ci.coeffs[0]);
			for (size_type i(1); i != ci.coeffs.size(); ++i) {
				assert(ci.coeffs[i] > min);
				min = ci.coeffs[i];
			}
		}
#endif

		typename ::std::map<size_type, ::std::vector<size_type> >::const_iterator j(combos_metadata.upper_bound(complexity));
		// stored complexity in the metadata is actually the size of that combo category (thus 1 past last valid) of the given combo-category
		assert(j->first != complexity);
		assert(j != combos_metadata.end());
		::std::vector<size_type> const & grid_res(j->second);

		size_type unchanged_coeffs_i(coefficients_size - 1);
		for (size_type i(ci.coeffs.size() - 1); i; --i) {
			assert(ci.offsets.size() > i - 1);
			assert(ci.offsets[i - 1]);
			size_type const to_change_coeff_i(ci.coeffs[i]);
			while(to_change_coeff_i != unchanged_coeffs_i) 
				coefficients_metadata[unchanged_coeffs_i--].value_reset();
			--unchanged_coeffs_i;

			size_type const grid_val(base_offset / ci.offsets[i - 1]);

			coefficients_metadata[to_change_coeff_i].value_from_grid(grid_val, grid_res[to_change_coeff_i]);

			base_offset -= grid_val * ci.offsets[i - 1];
		}

		size_type const to_change_coeff_i(ci.coeffs[0]);
		while(to_change_coeff_i != unchanged_coeffs_i) 
			coefficients_metadata[unchanged_coeffs_i--].value_reset();

		coefficients_metadata[to_change_coeff_i].value_from_grid(base_offset, grid_res[to_change_coeff_i]);

		assert(::std::numeric_limits<size_type>::max() > coefficients_size);
		while(--unchanged_coeffs_i < coefficients_size) 
			coefficients_metadata[unchanged_coeffs_i].value_reset();

#if 0
		for (size_type i(0); i != ci.coeffs.size(); ++i) 
			::std::clog << coefficients_metadata[ci.coeffs[i]].value() << ", "; 
		::std::clog << ::std::endl;
#endif
	}
};

}}

#endif
