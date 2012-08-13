Written and contributed by Leonid Zadorin at the Centre for the Study of Choice
(CenSoC), the University of Technology Sydney (UTS).

The open-source license for the work contained herein is as follows:

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

-------------------------------------------------------------------------------

Currently (August, 2012) the source code and other materials contained herein
are WORK IN PROGRESS.

Various parts and their completion stages vary from experimental/gestational to
proof-of-concept. 

Some (or rather a lot) of the technical documentation, comments, configuration,
installation information is FAR FROM COMPLETE at this stage -- it will be added
as the implementation state of the software solidifies and more time is
allocated for the relevant processes. 

For example, premature formal documentation -- e.g.  "design by contract"
documentation of pre/post conditions, pedantic arguments description, UML class
diagrams, etc. etc. etc. -- for the fluctuating and rather "unsettled"
code/design/architecture would yield enormous amounts of time spent on
updating/re-writing of the documentation in a "double-up" effort to keep it in
sync with the aforementioned dynamic state of the code. Timewise this is
practically infeasible at this stage.

The provisioning of this work is done for the transparency purposes -- putting
it in the public view, etc. etc. 

It is invisaged that in later stages of the repository's lifecycle the
structure may be decoupled further leading to multiple, smaller repositories
being created.

Moreover, currently-present duplication of, some, of the code will be
refactored into a commonly-used library code. For example, the FPU-state
checking and management code will probably be shared between the 'suncreen' and
'censoc/netcpu' directories.

Most of the text files are in UNIX text format (has to do with how the lines of
text are separated from each other).

Most of the code is in C++ (utilising some of the 2011 standard-compliant
syntax, so will not compile/build on earlier compiler versions which do not
support C++ 2011 standard).

Some files contain shell scripting.

The absolute majority of the development is done in FreeBSD environment. This
is intended to continue. Exceptional situations will be evaluated on a
case-by-case basis.

-------------------------------------------------------------------------------

An outline of the currently-present directories is as follows:

'info_etc' contains various high-level documentation, information and howto
files.

'censoc' and 'netcpu' are the main directories containing implementation of: 
	* various maximum likelihood models such as mixed-logit, GMNL2, etc.;
	* Variable Complexity Grid-Walk convergence algorithm (as per VCGW.pdf in
		the aforementioned 'info_etc' directory);
	* NetCPU parallel processing infrastructure (once again, see VCGW.pdf for
		more, albeit high-level description) and associated helper utilities.

Most likely (in future), the 'censoc' directory will be renamed to a 'lib' or
something -- as it is intended to host the code that is commonly shared by
'netcpu' and other projects/subdirectories... 

'autoinput' is a very quick hack of the mouse and keyboard event automation on
Windows (from here on 'lose') systems. Was primarily designed to help Chelsea,
Luke et al. in their coarse, very coarse, sync. automation of the eye-tracker and
the EEG-scanner utilities for the neuro-science projects/surveys.

'sunscreen' contains the code for performing of the Gaussian blur (smearing) as
per requirements of the sunscreen survey-project by Amanda Barnard, Jordan
Louviere, et al.

'lose_installer' is yet another helper/transient utility in relation to installing NetCPU
processors/clients on the lose-OS systems. It basically represents a quick hack
and it is highly-likely that if/when the project evolves from the
proof-of-concept to a more robust deployment across multiple sites than
relevant system admins will use their own installation methods. Such a
'system-admin' customization is envisaged to be present not only for lose-OS
installers but for other systems (e.g. Linux-based Ubuntu vs Slackware vs
Fedora etc. -- each of such systems will most likely have their own 'binary
installation' package strategy/procedure/utilities). 

-------------------------------------------------------------------------------
