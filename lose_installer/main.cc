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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdint.h>
#include <windows.h>
#include <ntsecapi.h>
#include <ntdef.h>
#include <shellapi.h>
#include <objbase.h>
#include <stdexcept>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <string>
#include <errno.h>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp> // because need custom dtor supplied in the constructor
#include <mstask.h> // copied to wingw32 from mingw64 headers!!!
#include <lmaccess.h>
#include <lmerr.h>
#include <limits>

const GUID IID_ITaskScheduler GUID_SECT = {0x148BD527L,0xA2AB,0x11CE,{0xB1,0x1F,0x00,0xAA,0x00,0x53,0x05,0x03}};
const GUID CLSID_CTaskScheduler GUID_SECT = {0x148BD52A,0xA2AB,0x11CE,{0xB1,0x1F,0x00,0xAA,0x00,0x53,0x05,0x03}};
const GUID IID_ITask GUID_SECT = {0x148BD524L,0xA2AB,0x11CE,{0xB1,0x1F,0x00,0xAA,0x00,0x53,0x05,0x03}};
const GUID CLSID_CTask GUID_SECT = {0x148BD520,0xA2AB,0x11CE,{0xB1,0x1F,0x00,0xAA,0x00,0x53,0x05,0x03}};
const GUID IID_IPersistFile GUID_SECT = {0x10b,0,0,{0xc0,0,0,0,0,0,0,0x46}};

// for mingw32 (not defined there)
#ifndef UF_NOT_DELEGATED
#define UF_NOT_DELEGATED 0x100000
#endif

template <class Instance>
class CComPtr
{
public:
	CComPtr ()
	: inst(NULL) {
	}
	Instance * 
	operator = (Instance * x)
	{
		if (inst != NULL)
			inst->Release ();
		inst = x;
		inst->AddRef ();
		return inst;
	}
	Instance * 
	operator -> ()
	{
		assert (inst != NULL);
		return inst;
	}
	operator Instance * ()
	{
		return inst;
	}
	~CComPtr ()
	{
		if (inst != NULL)
			inst->Release ();
	}
	void 
	Release ()
	{
		assert (inst != NULL);
		inst->Release ();
		inst = NULL;
	}
private:
	Instance *inst;
};

template <class T> class allocator;

template <> struct 
allocator<void> {
	typedef void*       pointer;
	typedef const void* const_pointer;
	typedef void value_type;
	template <class U> struct rebind { typedef allocator<U> other; };
};


template <class T> 
class allocator {
public:
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T *        pointer;
	typedef const T *  const_pointer;
	typedef T &        reference;
	typedef const T &  const_reference;
	typedef T         value_type;
	template <class U> struct rebind { typedef allocator<U> other; };

	allocator() throw() { }
	allocator(allocator const &) throw() { }
	template <class U> allocator(allocator<U> const &) throw() { }
	~allocator() throw() { }

	pointer 
	allocate(size_type n, allocator<void>::const_pointer hint = 0) // todo what is the hint for?
	{
		pointer p(new value_type[n]);
		if (!::VirtualLock(p, n * sizeof(value_type)))
			throw ::std::runtime_error("VirtualLock");
	}

	void 
	deallocate(pointer p, size_type n)
	{
		::RAND_pseudo_bytes(reinterpret_cast<char unsigned *>(p), n * sizeof(value_type));
		if (!::VirtualUnlock(p, n * sizeof(value_type)))
			throw ::std::runtime_error("VirtualUnlock");
		delete [] p;
	}

	size_type max_size() const throw()
	{
		return ::std::numeric_limits<size_type>::max();
	}

	void 
	construct(pointer p, T const & val)
	{
		new (p) T(val);
	}

	void 
	destroy(pointer p)
	{
		p->~T();
	}
};

::std::basic_string<wchar_t, std::char_traits<wchar_t>, allocator<wchar_t> > hushstr;

/**
	@todo -- more error checking et al
	*/

void 
set_lsa_string(PLSA_UNICODE_STRING lsa_str, LPWSTR str)
{
	DWORD str_size;
	str_size = ::wcslen(str);
	lsa_str->Buffer = str;
	lsa_str->Length = str_size * sizeof(WCHAR);
	lsa_str->MaximumLength = (str_size + 1) * sizeof(WCHAR);
}

char unsigned static 
fourbit(char unsigned c)
{
	if (c < 10)
		return c + '0';
	else 
		return c - 10 + 'A';
}

int 
main()
{
	if (FAILED(::CoInitialize(0)))
		throw ::std::runtime_error("CoInitialize");

	try {

		::boost::shared_ptr<TCHAR> scoped_dummy;
		DWORD dummy_size(0);

		SID_NAME_USE sid_type;

		// check that user is not already present...
		DWORD sid_size(0);
		if (::LookupAccountName(NULL, "censoc_netcpu", NULL, &sid_size, NULL, &dummy_size, &sid_type)) 
			throw ::std::runtime_error("expecting LookupAccountName to fail on 1st pass (for memory allocation).");
		else if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			::boost::shared_ptr<SID> scoped_sid(static_cast<SID *>(::malloc(sid_size)), ::free); 
			scoped_dummy.reset(static_cast<TCHAR *>(::malloc(dummy_size * sizeof(TCHAR))), ::free); 
			if (::LookupAccountName(NULL, "censoc_netcpu", scoped_sid.get(), &sid_size, scoped_dummy.get(), &dummy_size, &sid_type))
				throw ::std::runtime_error("sensed already present/installed censoc_netcpu user -- if this is the result of previous installation of this softwore then deinstall first. Otherwise the installation is incompatible with this system.");
		}

		try {
			char unsigned data[15];
			::RAND_pseudo_bytes(data, sizeof(data));

			::SHA_CTX sha_ctx;
			::SHA1_Init(&sha_ctx);
			::SHA1_Update(&sha_ctx, data, sizeof(data));

			// no SHA1_End in some openssl distros...
			char unsigned hush_bin[20];
			::SHA1_Final(hush_bin, &sha_ctx);
			::std::wstring hush; // todo -- later customize the whole thing to lock the memory and zero-out!!!
			for (int i(0); i != 20; ++i) {
				hush += fourbit(hush_bin[i] & 0xf);
				hush += fourbit(hush_bin[i] >> 4);
			}
			hush = hush.substr(0, 14);

			//if (::system(("net user censoc_netcpu " + hush + " /add").c_str()) || errno == ENOENT) 
			//	throw ::std::runtime_error("net user add");

			USER_INFO_1 user_info;
			::memset(&user_info, 0, sizeof(USER_INFO_1));
			user_info.usri1_name = const_cast<wchar_t *>(L"censoc_netcpu");
			user_info.usri1_password = const_cast<wchar_t *>(hush.c_str()); 
			user_info.usri1_priv = USER_PRIV_USER; 
			user_info.usri1_home_dir = NULL;
			user_info.usri1_comment = NULL;
			user_info.usri1_flags = UF_SCRIPT | UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD
			| UF_NOT_DELEGATED // -- TODO!!! may be if delegatable is set, then dont need password for runas thath scheduled task will use, so no need for password? hmmm to research when tehre is more time
			| UF_NORMAL_ACCOUNT;
			user_info.usri1_script_path = NULL;
			if (::NetUserAdd(NULL, 1, reinterpret_cast<BYTE *>(&user_info), NULL) != NERR_Success)
				throw ::std::runtime_error("NetUserAdd");

			// if (::system("WMIC USERACCOUNT WHERE \"Name='censoc_netcpu'\" SET PasswordExpires=FALSE") || errno == ENOENT)
			// 	throw ::std::runtime_error("wmic setting passwordexpires to false");

			sid_size = 0;
			dummy_size = 0;
			if (::LookupAccountName(NULL, "censoc_netcpu", NULL, &sid_size, NULL, &dummy_size, &sid_type))
				throw ::std::runtime_error("expecting LookupAccountName to fail on 1st pass (for memory allocation).");
			else if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				throw ::std::runtime_error("LookupAccountName allocating buffer");
			::boost::shared_ptr<SID> scoped_sid(static_cast<SID *>(::malloc(sid_size)), ::free); 
			scoped_dummy.reset(static_cast<TCHAR *>(::malloc(dummy_size * sizeof(TCHAR))), ::free); 
			if (!::LookupAccountName(NULL, "censoc_netcpu", scoped_sid.get(), &sid_size, scoped_dummy.get(), &dummy_size, &sid_type))
				throw ::std::runtime_error("LookupAccountName");

			//if (::system("net localgroup Users censoc_netcpu /delete") || errno == ENOENT)
			//	throw ::std::runtime_error("net group delete from users");

			LOCALGROUP_MEMBERS_INFO_0 user_to_del;
			user_to_del.lgrmi0_sid = scoped_sid.get();
			NET_API_STATUS const rv(::NetLocalGroupDelMembers(NULL, L"Users", 0, reinterpret_cast<BYTE *>(&user_to_del), 1));
			if (rv != NERR_Success && rv != NERR_GroupNotFound && rv != ERROR_MEMBER_NOT_IN_ALIAS)
				throw ::std::runtime_error("NetLocalGroupDelMembers");

			LSA_HANDLE policy;
			::LSA_OBJECT_ATTRIBUTES ObjectAttributes;
			::ZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));
			if (::LsaOpenPolicy( NULL, &ObjectAttributes, 
			// GENERIC_READ | GENERIC_EXECUTE |
			POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES, &policy) != STATUS_SUCCESS)
				throw ::std::runtime_error("LsaOpenPolicy");

			try {
				LSA_UNICODE_STRING account_rights[4];

				::set_lsa_string(account_rights, const_cast<wchar_t *>(L"SeBatchLogonRight"));
				::set_lsa_string(account_rights + 1, const_cast<wchar_t *>(L"SeDenyInteractiveLogonRight"));
				::set_lsa_string(account_rights + 2, const_cast<wchar_t *>(L"SeDenyNetworkLogonRight"));
				::set_lsa_string(account_rights + 3, const_cast<wchar_t *>(L"SeDenyRemoteInteractiveLogonRight"));
				if (::LsaAddAccountRights(policy, scoped_sid.get(), account_rights,  4) != STATUS_SUCCESS)
					throw ::std::runtime_error("LsaAddAccountRights");

				::set_lsa_string(account_rights, const_cast<wchar_t *>(L"SeDebugPrivilege"));
				::set_lsa_string(account_rights + 1, const_cast<wchar_t *>(L"SeImpersonatePrivilege"));
				::set_lsa_string(account_rights + 2, const_cast<wchar_t *>(L"SeIncreaseBasePriorityPrivilege"));
				::set_lsa_string(account_rights + 3, const_cast<wchar_t *>(L"SeTakeOwnershipPrivilege"));
				if (::LsaRemoveAccountRights(policy, scoped_sid.get(), FALSE, account_rights, 4) != STATUS_SUCCESS)
					throw ::std::runtime_error("LsaRemoveAccountRights");

			} catch (::std::runtime_error const &) {
				::LsaClose(policy);
				throw;
			}
			::LsaClose(policy);

			{ // special access rights to c

			//  porting to c and/or do better error-checking!!!
			//if (::system("nasty_hack.vbs") || errno == ENOENT)
			//	throw ::std::runtime_error("nasty_hack.vbs");

			char path_name[] = "c:\\"; 
			DWORD sd_size;
			if (::GetFileSecurity(path_name, DACL_SECURITY_INFORMATION, NULL, 0, &sd_size) || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				throw ::std::runtime_error("GetFileSecurity when establishing size of buffer");
			// malloc should be DWORD aligned by default...
			::boost::shared_ptr<SECURITY_DESCRIPTOR> scoped_sd(static_cast<PSECURITY_DESCRIPTOR>(::malloc(sd_size)), ::free); // shared_ptr because of dtor arg
			if (!::GetFileSecurity(path_name, DACL_SECURITY_INFORMATION, scoped_sd.get(), sd_size, &sd_size))
				throw ::std::runtime_error("GetFileSecurity");

			PACL old_acl;
			BOOL dacl_present, tmp;
			if (!::GetSecurityDescriptorDacl(scoped_sd.get(), &dacl_present, &old_acl, &tmp) || dacl_present == FALSE || old_acl == NULL)
				throw ::std::runtime_error("GetSecurityDescriptorDacl");

			ACL_SIZE_INFORMATION AclInfo;
			if (!::GetAclInformation(old_acl, &AclInfo, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation)) 
				throw ::std::runtime_error("GetAclInformation");

			DWORD cbNewACL = AclInfo.AclBytesInUse + sizeof(ACCESS_DENIED_ACE) + ::GetLengthSid(scoped_sid.get()) - sizeof(DWORD);

			::boost::shared_ptr<ACL> scoped_new_acl(static_cast<PACL>(::malloc(cbNewACL)), ::free);

			if (!::InitializeAcl(scoped_new_acl.get(), cbNewACL, ACL_REVISION2)) 
				throw ::std::runtime_error("InitializeAcl");

			if (!::AddAccessDeniedAceEx(scoped_new_acl.get(), ACL_REVISION_DS, CONTAINER_INHERIT_ACE  | OBJECT_INHERIT_ACE, 
			// FILE_TRAVERSE | FILE_EXECUTE |
			FILE_READ_ATTRIBUTES |
			FILE_READ_EA |
			FILE_ADD_FILE | FILE_WRITE_DATA | 
			FILE_ADD_SUBDIRECTORY | FILE_APPEND_DATA | 
			FILE_WRITE_ATTRIBUTES | 
			FILE_WRITE_EA | 
			FILE_DELETE_CHILD | 
			DELETE |
			READ_CONTROL | 
			WRITE_DAC | 
			WRITE_OWNER
			, scoped_sid.get()))
				throw ::std::runtime_error("AddAccessDeniedAceEx");
			//bool last_ace_not_inherited(true);
			for (int unsigned i(0); i != AclInfo.AceCount; ++i) {
				void * ace;
				if (!::GetAce(old_acl, i, &ace)) 
					throw ::std::runtime_error("GetAce");

				//if (last_ace_not_inherited == true && 
				//(static_cast<PACE_HEADER>(ace)->AceFlags & INHERITED_ACE || i == AclInfo.AceCount - 1)) {
				//	last_ace_not_inherited = false;

				//}

				if (!::EqualSid(scoped_sid.get(),
				// ACCESS_ALLOWED_ACE vs ACCESS_DENIED_ACE et al are synonymous w.r.t. 'SidStart' field...
				&static_cast<ACCESS_DENIED_ACE *>(ace)->SidStart))
					if (!::AddAce(scoped_new_acl.get(), ACL_REVISION_DS, MAXDWORD, ace, static_cast<PACE_HEADER>(ace)->AceSize)) 
						throw ::std::runtime_error("AddAce");
			}

			// the bloody thing wants it in absolute format... no time to stuff-around, will do bulk allocation (even if not really needed -- no time to find out if call will contradict M$ documentation -- as per 'dummy' arg for 'LookupAccountName' even though it was documented there as being optional)
			DWORD new_sd_size(0);
			DWORD new_dacl_size(0);
			DWORD new_sacl_size(0);
			DWORD new_owner_size(0);
			DWORD new_group_size(0);
			if (::MakeAbsoluteSD(scoped_sd.get(), NULL, &new_sd_size, NULL, &new_dacl_size, NULL, &new_sacl_size, NULL, &new_owner_size, NULL, &new_group_size) || (::GetLastError() != 0xC0000023 && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER))
				throw ::std::runtime_error("MakeAbsoluteSD -- first call expected to fail with STATUS_BUFFER_TOO_SMALL or ERROR_INSUFFICIENT_BUFFER error.");

			::boost::shared_ptr<SECURITY_DESCRIPTOR> scoped_new_sd(static_cast<PSECURITY_DESCRIPTOR>(::malloc(new_sd_size)), ::free); // shared_ptr because of dtor arg
			::boost::shared_ptr<ACL> scoped_new_dacl(static_cast<PACL>(::malloc(new_dacl_size)), ::free); 
			::boost::shared_ptr<ACL> scoped_new_sacl(static_cast<PACL>(::malloc(new_sacl_size)), ::free); 
			::boost::shared_ptr<SID> scoped_new_owner(static_cast<SID *>(::malloc(new_owner_size)), ::free);
			::boost::shared_ptr<SID> scoped_new_group(static_cast<SID *>(::malloc(new_group_size)), ::free);
			if (!::MakeAbsoluteSD(scoped_sd.get(), scoped_new_sd.get(), &new_sd_size, scoped_new_dacl.get(), &new_dacl_size, scoped_new_sacl.get(), &new_sacl_size, scoped_new_owner.get(), &new_owner_size, scoped_new_group.get(), &new_group_size))
				throw ::std::runtime_error("MakeAbsoluteSD");

			if (!::SetSecurityDescriptorDacl(scoped_new_sd.get(), TRUE, scoped_new_acl.get(), FALSE)) 
				throw ::std::runtime_error("SetSecurityDescriptorDacl");

			if (!::SetFileSecurity(path_name, DACL_SECURITY_INFORMATION, scoped_new_sd.get())) 
				throw ::std::runtime_error("SetFileSecurity");

			} // special access rights to c

			if (::system("echo y| CACLS c:\\censoc\\netcpu /T /E /R censoc_netcpu") || errno == ENOENT)
				throw ::std::runtime_error("cacls revoke on censoc/netcpu");

			if (::system("echo y| CACLS c:\\censoc\\netcpu /T /E /G censoc_netcpu:F") || errno == ENOENT)
				throw ::std::runtime_error("cacls grant on censoc/netcpu");

			// TODO -- later do this programmatically so as not to expose the password too much!!!
			//if (::_wsystem((L"schtasks /create /tn censoc_netcpu_update_client /tr c:\\censoc\\netcpu\\censoc_netcpu_update_client.bat /sc ONSTART /ru censoc_netcpu /rp " + hush).c_str()) || errno == ENOENT)
			//	throw ::std::runtime_error("schktasks");

			MULTI_QI instance;
			::memset(&instance, 0, sizeof(MULTI_QI));
			instance.pIID = &IID_ITaskScheduler;
			if (FAILED(::CoCreateInstanceEx(CLSID_CTaskScheduler, NULL, CLSCTX_ALL, NULL, 1, &instance)))
				throw ::std::runtime_error("CoCreateInstanceEx(CLSID_CTaskScheduler...)");

			CComPtr<ITaskScheduler> ts;
			ts = reinterpret_cast<ITaskScheduler*>(instance.pItf);
			// ts->SetTargetComputer(NULL);

			ts->Delete(L"censoc_netcpu_update_client");
			CComPtr<ITask> t;
			if (FAILED(ts->NewWorkItem(L"censoc_netcpu_update_client", CLSID_CTask, IID_ITask, (IUnknown**)&t)))
				throw ::std::runtime_error("NewWorkItem");

			if (FAILED(t->SetApplicationName (L"c:\\censoc\\netcpu\\censoc_netcpu_update_client.bat")))
				throw ::std::runtime_error("SetApplicationName");
			if (FAILED(t->SetMaxRunTime(INFINITE)))
				throw ::std::runtime_error("SetMaxRunTime");
			if (FAILED(t->SetAccountInformation(L"censoc_netcpu", hush.c_str())))
				throw ::std::runtime_error("SetAccountInformation");
			if (FAILED(t->SetPriority(IDLE_PRIORITY_CLASS)))
				throw ::std::runtime_error("SetPriority");

			WORD wtrig(0);
			CComPtr<ITaskTrigger> trigger;
			if (FAILED(t->CreateTrigger(&wtrig, reinterpret_cast<ITaskTrigger **>(&trigger))))
				throw ::std::runtime_error("CreateTrigger");

			TASK_TRIGGER trigger_info;
			::memset(&trigger_info, 0, sizeof(TASK_TRIGGER));
			trigger_info.cbTriggerSize = sizeof(TASK_TRIGGER);
			trigger_info.wBeginYear = 2000;
			trigger_info.wBeginMonth = 1;
			trigger_info.wBeginDay = 1;
			trigger_info.TriggerType = TASK_EVENT_TRIGGER_AT_SYSTEMSTART;
			if (FAILED(trigger->SetTrigger(&trigger_info)))
				throw ::std::runtime_error("SetTrigger");

			CComPtr<IPersistFile> tosave;
			if (FAILED(t->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&tosave))))
				throw ::std::runtime_error("QueryInterface(IID_IPersistFile...");
			if (FAILED(tosave->Save(NULL, TRUE)))
				throw ::std::runtime_error("Save in IPersistFile");

			// TODO -- port to c and/or do better error-checking!!!
			if (::system("REGEDIT.EXE  /S  postinstall.reg") || errno == ENOENT)
				throw ::std::runtime_error("regedit");

		} catch (::std::runtime_error const &) {


			::system("deinstall.bat");
			throw;
		}

		//::InitiateSystemShutdown(NULL, NULL, 0, FALSE, TRUE);
		//::system("shutdown -r -t 0");
	} catch (...) {
		::CoUninitialize();
		throw;
	}
	::CoUninitialize();

	return 0;
}
