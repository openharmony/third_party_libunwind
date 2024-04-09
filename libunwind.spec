Name: libunwind
Epoch: 2
Version: 1.6.2
Release: 7
Summary: Libunwind provides a C ABI to determine the call-chain of a program
License: BSD
URL: http://savannah.nongnu.org/projects/libunwind
Source: http://download-mirror.savannah.gnu.org/releases/libunwind/libunwind-%{version}.tar.gz

Patch0001: 0001-fix-byte_order_is_valid-function-logic.patch
Patch2: backport-check-namespace.sh-adjust-aarch64-symbols.patch
Patch3: backport-tests-run-coredump-unwind-Skip-test-if-no-coredump-h.patch
Patch4: backport-aarch64-unw_step-validates-address-before-calling-dwarf_get.patch
Patch5: backport-avoid-calling-printf-because-OE-glibc-2.34-used-mno-.patch
Patch6: backport-fix-run-ptrace-mapper-test-case-failed.patch

ExclusiveArch: aarch64 %{ix86} x86_64

BuildRequires: automake libtool autoconf texlive-latex2man gcc-c++

%description
Libunwind defines a portable and efficient C programming interface (API) to
determine the call-chain of a program. The API additionally provides the means
to manipulate the preserved (callee-saved) state of each call-frame and to
resume execution at any point in the call-chain (non-local goto). The API
supports both local (same-process) and remote (across-process) operation.

%package devel
Summary: Development files for libunwind
Requires: libunwind = %{epoch}:%{version}-%{release}

%description devel
This package contains development files for libunwind.

%package_help

%prep
%autosetup -n %{name}-%{version} -p1

%build
aclocal
libtoolize --force
autoheader
automake --add-missing
autoconf
%configure --enable-static --enable-shared --enable-setjmp=no
%make_build

%install
%make_install
%delete_la

# /usr/include/libunwind-ptrace.h
# [...] aren't really part of the libunwind API.  They are implemented in
# a archive library called libunwind-ptrace.a.
mv -f $RPM_BUILD_ROOT%{_libdir}/libunwind-ptrace.a $RPM_BUILD_ROOT%{_libdir}/libunwind-ptrace.a-save
rm -f $RPM_BUILD_ROOT%{_libdir}/libunwind*.a
mv -f $RPM_BUILD_ROOT%{_libdir}/libunwind-ptrace.a-save $RPM_BUILD_ROOT%{_libdir}/libunwind-ptrace.a
rm -f $RPM_BUILD_ROOT%{_libdir}/libunwind-ptrace*.so*

# fix multilib conflicts
touch -r NEWS $RPM_BUILD_ROOT%{_includedir}/libunwind.h

%check
make check || true

%pre

%preun

%ldconfig_scriptlets

%files
%defattr(-,root,root)
%license COPYING
%{_libdir}/libunwind*.so.*

%files devel
%defattr(-,root,root)
%{_libdir}/pkgconfig/libunwind*.pc
# <unwind.h> does not get installed for REMOTE_ONLY targets - check it.
%{_includedir}/unwind.h
%{_includedir}/libunwind*.h
%{_libdir}/libunwind-ptrace.a
%{_libdir}/libunwind*.so

%files help
%defattr(-,root,root)
%doc README NEWS
%{_mandir}/*/*

%changelog
* Tue Jul 11 2023 chenziyang <chenziyang4@huawei.com> - 2:1.6.2-7
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:backport upstream patch to fix failed run-ptrace-mapper testcase 

* Mon Jul 03 2023 chenziyang <chenziyang4@huawei.com> - 2:1.6.2-6
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:fix failed testcase by editing test-ptrace testcase

* Tue Jun 20 2023 wangjiang <wangjiang37@h-partners.com> - 2:1.6.2-5
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:fix failed testcase

* Mon Feb 20 2023 shixuantong <shixuantong1@huawei.com> - 2:1.6.2-4
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:fix some test fail

* Fri Dec 16 2022 Bin Hu <hubin73@huawei.com> - 2:1.6.2-2
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:fix byte_order_is_valid function logic

* Mon Nov 7 2022 Bin Hu <hubin73@huawei.com> - 2:1.6.2-1
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:update version 1.6.2

* Tue Oct 25 2022 zhangruifang <zhangruifang1@h-partners.com> - 2:1.5.0-3
- Rebuild for next release
- fix bogus date in changelog

* Tue Apr 26 2022 renhongxun<renhongxun@h-partners.com> - 2:1.5.0-2
- add epoch

* Wed Dec 22 2021 shangyibin<shangyibin1@huawei.com> - 1.5.0-1
- Upgrade to version 1.5.0

* Fri Jul 30 2021 liudabo<liudabo1@huawei.com> - 1.4.0-2
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:Fix gcc build

* Mon Mar 30 2020 wenzhanli<wenzhanli2@huawei.com> - 1.4.0-1
- Type:bugfix
- ID:NA
- SUG:NA
- DESC:update version 1.4.0

* Sun Jan 12 2020 openEuler Buildteam <buildteam@openeuler.org> - 1.3.1-3
- remove useless patch

* Fri Sep 27 2019 openEuler Buildteam <buildteam@openeuler.org> - 1.3.1-2
- del unnecessary statement

* Mon Sep 2 2019 openEuler Buildteam <buildteam@openeuler.org> - 1.3.1-1
- Package init
