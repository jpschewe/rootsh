# Upstream: Corey Henderson <corman@cormander.com>

%define _without_syslog 1

Summary: Shell wrapper to log activity
Name: rootsh
Version: 1.6.4
Release: 1
License: GPL
Group: System/Shells
URL: https://github.com/jpschewe/rootsh
Source: rootsh-%{version}.tar.gz

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
%doc AUTHORS ChangeLog COPYING README THANKS
%{_mandir}/man1/rootsh.1*
%{_bindir}/rootsh

%defattr(1777, root, root)
%{_localstatedir}/log/rootsh/
