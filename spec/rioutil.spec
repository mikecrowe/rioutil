%define ver     1.4.4
%define rel     0
%define prefix  /sw
%define name    rioutil

Name:        %{name}
Summary:     Interface with Rio 600/800, psa[play
Version:     %{ver}
Release:     %{rel}
Copyright:   GPL
Group:       Development/Libraries
Source:      ftp://download.sourceforge.net/pub/sourceforge/%{name}/%{name}-%{ver}.tar.gz
URL:         http://rioutil.sourceforge.net
Packager:    Nathan Hjelm <hjelmn@users.sourceforge.net>
BuildRoot:   /tmp/%{name}-root

%description
rioutil is a program/library designed to interface with
diamond mm's rio 600/800 and nike psa[play.

%prep
%setup -n %{name}-%{ver}

%build
./configure --prefix=%{prefix}
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -rf $RPM_BUILD_ROOT; fi
make install-strip prefix=$RPM_BUILD_ROOT%{prefix}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{prefix}/bin/rioutil
%{prefix}/man/man1/rioutil.1
%{prefix}/include/rio.h
%{prefix}/lib/librioutil*.la
%{prefix}/lib/librioutil*.so*
