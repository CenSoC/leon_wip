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

#include <stdlib.h>

#include <iostream>

#include <censoc/type_traits.h>

#include "combos_builder.h"

// temporary hack for auxiliary projects
#if 1
#include <fstream>

#include <set>
#include <boost/lexical_cast.hpp>

#endif

//typedef unsigned size_type;
typedef uint64_t size_type;
typedef ::censoc::param<size_type>::type size_paramtype;

size_type const static coefficients_size = 30;

class blah {
	size_type value_;
public:

	// policy compliance
	::std::vector<size_type> grid_resolutions;

	void
	value_from_grid(size_paramtype grid, size_paramtype grid_res) 
	{
		(void)grid_res;
		value_ = grid;
	}

	void
	value_reset()
	{
		value_ = 0;
	}

	// just playing-around/examplifying
	::censoc::strip_const<size_paramtype>::type
	value() const 
	{
		return value_;
	}
};
typedef blah blah_vector_type[coefficients_size];
blah_vector_type coefficients_metadata;

// temporary hack for auxiliary projects
#if 1
struct boo {
	size_type index;
};
void
output_stata_like(::std::string const & title, ::std::vector< ::std::string> & bam) 
{
	::std::ofstream file(("./output_stata_like/" + title + ".txt").c_str(), ::std::ios::trunc);
	file << "* " << title << ::std::endl << ::std::endl;
	::std::vector<blah> coefficients_metadata(bam.size());
	for (size_type i(0); i != bam.size(); ++i) {
		coefficients_metadata[i].grid_resolutions.resize(bam.size());
		for (size_type j(0); j != bam.size(); ++j)
			coefficients_metadata[i].grid_resolutions[j] = 2;
	}
	::censoc::netcpu::combos_builder<size_type, ::std::vector<blah> > cb;
	size_type complexity_size(3000000000);
	size_type actual_bam_size(bam.size());
	cb.build(bam.size(), complexity_size, actual_bam_size, coefficients_metadata);

	::std::list< ::std::list<boo> > booboo;
	cb.load_all_combos(booboo);

	::std::cerr << '\n';
	::std::map<size_type, ::std::vector<size_type> > const & blahx(cb.metadata());
	for (::std::map<size_type, ::std::vector<size_type> >::const_iterator i(blahx.begin()); i != blahx.end(); ++i) {
		::std::cerr << i->first << ", ";
	}

	::std::set< ::std::string> aoeu;
	for (::std::list< ::std::list<boo> >::const_iterator j(booboo.begin()); j != booboo.end(); ++j) {
		if (j->size() == 8) {
			::std::string iii(title);
			for (::std::list<boo>::const_iterator k(j->begin()); k != j->end(); ++k) {
				if (k != j->begin()) 
					iii += " & ";
				iii += ::boost::lexical_cast< ::std::string>(1 + k->index);
			}
			aoeu.insert(iii);
		}
	}
	if (aoeu.empty() == false)
		::std::cerr << "\n size: " << aoeu.size() << '\n';

#if 1
	for (::std::list< ::std::list<boo> >::const_iterator j(booboo.begin()); j != booboo.end(); ++j) {
		file << "* " << title << ' ';
		for (::std::list<boo>::const_iterator k(j->begin()); k != j->end(); ++k) {
			if (k != j->begin()) 
				file << " & ";
			file << 1 + k->index;
		}
		file << ::std::endl;
		file << "xi: regress sumBW ";
		for (::std::list<boo>::const_iterator k(j->begin()); k != j->end(); ++k) {
			if (bam[k->index] != "gender_o"  &&  bam[k->index] != "faith") 
				file << "i.";
			file << bam[k->index] << ' ';
		}
		file << ::std::endl;
		for (::std::list<boo>::const_iterator k(j->begin()); k != j->end(); ++k) {
			if (bam[k->index] != "gender_o"  &&  bam[k->index] != "faith") 
				file << "testparm _I" << bam[k->index].substr(0, 9) << "_*\n";
		}
		file << ::std::endl;
	}
#endif



}
#endif


int main()
{
	size_type coeffs_atonce_size(30);
	for (size_type i(0); i != coefficients_size; ++i) {
		coefficients_metadata[i].grid_resolutions.resize(coeffs_atonce_size);
		for (size_type j(0); j != coeffs_atonce_size; ++j)
			coefficients_metadata[i].grid_resolutions[j] = ::rand() % 5 + 3; // = 10
	}

	::censoc::netcpu::combos_builder<size_type, blah_vector_type> cb;
	size_type complexity_size(1000000);
	cb.build(coefficients_size, complexity_size, coeffs_atonce_size, coefficients_metadata);

	//cb.print();
	::std::clog << "resulting complexity: " << complexity_size << ::std::endl;
	::std::clog << "resulting coeffs_at_once: " << coeffs_atonce_size << ::std::endl;

	// @NOTE -- complexity is zero-based
	for (size_type i(0); i != complexity_size; ++i) {
		cb.demodulate(i, coefficients_metadata, coefficients_size);
	}

	cb.build(coefficients_size, complexity_size, coeffs_atonce_size, coefficients_metadata);

	::std::clog << "resulting complexity: " << complexity_size << ::std::endl;
	::std::clog << "resulting coeffs_at_once: " << coeffs_atonce_size << ::std::endl;

// temporary hack for auxiliary projects
#if 1
	{
		{
			::std::vector< ::std::string> boo;
			boo.push_back("lifesatisfaction_1");
			boo.push_back("o_attachment");
			boo.push_back("o_security");
			boo.push_back("o_role");
			boo.push_back("o_enjoyment");
			boo.push_back("o_control");
			output_stata_like("Own QoL", boo);
		}
		{
			::std::vector< ::std::string> boo;
			boo.push_back("gender_o");
			boo.push_back("age_o");
			boo.push_back("marital");
			boo.push_back("family");
			output_stata_like("Individual demographics", boo);
		}
		{
			::std::vector< ::std::string> boo;
			boo.push_back("location"); 
			boo.push_back("vehicles");
			boo.push_back("dwelling");    
			boo.push_back("dwelling2");       
			output_stata_like("Residence demographics", boo);
		}
		{
			::std::vector< ::std::string> boo;
			boo.push_back("faith"); 
			boo.push_back("school");         
			boo.push_back("nonschool");
			boo.push_back("personal");
			boo.push_back("work");           
			boo.push_back("occupation");
			boo.push_back("household");
			output_stata_like("Sociographics", boo);
		}
		{
			::std::vector< ::std::string> boo;
			boo.push_back("health");         
			boo.push_back("sleep");           
			output_stata_like("Health", boo);
		}
		{
			::std::vector< ::std::string> boo;
			boo.push_back("influence");      
			boo.push_back("trust");          
			boo.push_back("crime");           
			output_stata_like("Social empowerment", boo);
		}
		{
			::std::vector< ::std::string> boo;
			boo.push_back("family");
			boo.push_back("vehicles");
			boo.push_back("dwelling");       
			boo.push_back("dwelling2");       
			output_stata_like("Demographics combined A", boo);
		}
		{
			::std::vector< ::std::string> boo;
			boo.push_back("family");         
			boo.push_back("marital");
			boo.push_back("dwelling ");      
			boo.push_back("dwelling2");       
			output_stata_like("Demographics combined B", boo);
		}
#if 0
		{
			::std::vector< ::std::string> boo;
			boo.push_back("1");         
			boo.push_back("2");
			boo.push_back("3 ");      
			boo.push_back("4");       
			boo.push_back("5");       
			boo.push_back("6");       
			boo.push_back("7");       
			boo.push_back("8");       
			boo.push_back("9");       
			boo.push_back("10");       
			boo.push_back("11");       
			boo.push_back("12");       
			boo.push_back("13");       
			boo.push_back("14");       
			boo.push_back("15");       
			boo.push_back("1");         
			boo.push_back("2");
			boo.push_back("3 ");      
			boo.push_back("4");       
			boo.push_back("5");       
			boo.push_back("6");       
			boo.push_back("7");       
			boo.push_back("8");       
			boo.push_back("9");       
			boo.push_back("10");       
			boo.push_back("11");       
			boo.push_back("12");       
			boo.push_back("13");       
			boo.push_back("14");       
			boo.push_back("15");       
			output_stata_like("leon", boo);
		}
#endif
	}
#endif

	return 0;
}

