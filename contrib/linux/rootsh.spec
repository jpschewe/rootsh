%define ver 0.1
%define rel 1

Summary: logging shell wrapper bla
Name: rootsh
Version: %{ver}
Release: %{rel}
License: GNU
Group: Applications/Shells
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

# Where do we expect the logfiles
%define logdir /var/log/rootsh

# Do we want syslog support (1=yes 0=no)
%define syslog 1

# if yes, which priority
%define sysloglevel local5
%define syslogfacility notice

%description
bla

%prep
%setup -q

%build
aclocal
autoheader
autoconf

%configure \
        --prefix=/ \
        --with-logdir=%{logdir} \
%if %{syslog}
        --enable-syslog=%{sysloglevel}.%{syslogfacility}
%else
        --disable-syslog
%endif
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p -m755 $RPM_BUILD_ROOT%{logdir}

make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%pre
mkdir %{logdir}
chmod 700 %{logdir}
chown root:root  %{logdir}

%postun
rm -rf /usr/bin/rootsh
if [ -d /tmp/rootsh ]; then
  # there is already an old backup directory. move just the logfiles
  mv /var/adm/rootsh/* /tmp/rootsh
  rm -rf /var/adm/rootsh
else
  # copy the entire log directory to tmp
  mv /var/adm/rootsh /tmp/rootsh
fi

%files
%defattr(-,root,root)
%doc CREDITS ChangeLog INSTALL LICENCE OVERVIEW README* RFC* TODO WARNING*
%attr(0500,bin,bin) %{_bindir}/rootsh
%attr(0700,root,root) %{logdir}

%changelog
* Mon Jul 12 2004 Gerhard Lausser <lausser@sourceforge.net>
- initial release
