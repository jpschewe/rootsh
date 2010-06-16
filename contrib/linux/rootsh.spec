# Upstream: Corey Henderson <corman@cormander.com>

%define _without_syslog 1

Summary: Shell wrapper to log activity
Name: rootsh
Version: 1.5.3.1
Release: 1
License: GPL
Group: System Environment/Base
URL: http://sourceforge.net/projects/rootsh/
Packager: Corey Henderson <corman@cormander.com>
Source: http://dl.sf.net/rootsh/rootsh-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Rootsh is a wrapper for shells which logs all echoed keystrokes and terminal
output to a file and/or to syslog. It's main purpose is the auditing of users
who need a shell with root privileges. They start rootsh through the sudo
mechanism.

%prep
%setup

%build
%configure \
%{!?_without_syslog:--enable-syslog="local5.notice"} \
%{?_without_syslog:--disable-syslog} \
	--with-logdir="%{_localstatedir}/log/rootsh"
%{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%makeinstall
%{__install} -d -m0700 %{buildroot}%{_localstatedir}/log/rootsh/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc AUTHORS ChangeLog COPYING INSTALL README THANKS
%{_mandir}/man1/rootsh.1*
%{_bindir}/rootsh

%defattr(1777, root, root)
%{_localstatedir}/log/rootsh/

%changelog
* Tue Jun 15 2010 Jon Schewe <jpschewe@mtu.net>
- Released 1.5.3.1 with bugs from SF fixed and patches applied

* Wed May 14 2008 Corey Henderson <corman@cormander.com>
- Re-released as 1.5.3 with 4 patches

* Fri Mar 14 2008 Corey Henderson <corman@cormander.com>
- added rootsh-append.patch
- added rootsh-gcc4-sentinel-compile.patch
- added rootsh-scp.patch
- added rootsh-sigwinch.patch

* Thu Mar 24 2005 Dag Wieers <dag@wieers.com> - 1.5.2-1 - 2964+/dag
- Updated to release 1.5.2.

* Sat Feb 12 2005 Dag Wieers <dag@wieers.com> - 1.5.1-1
- Updated to release 1.5.1.

* Sun Dec 19 2004 Dries Verachtert <dries@ulyssis.org> - 1.5-1
- Updated to release 1.5.

* Thu Dec 09 2004 Dries Verachtert <dries@ulyssis.org> - 1.4.1-1
- Updated to release 1.4.1.

* Fri Sep 14 2004 Dag Wieers <dag@wieers.com> - 0.2-1
- Initial package. (using DAR)
