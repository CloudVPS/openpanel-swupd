%define version 0.8.12

%define libpath /usr/lib
%ifarch x86_64
  %define libpath /usr/lib64
%endif

Summary: swupd
Name: openpanel-swupd
Version: %version
Release: 1
License: GPLv2
Group: Development
Source: http://packages.openpanel.com/archive/openpanel-swupd-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-buildroot
Requires: openpanel-core

%description
swupd

%prep
%setup -q -n openpanel-swupd-%version

%build
BUILD_ROOT=$RPM_BUILD_ROOT
./configure
make

%install
BUILD_ROOT=$RPM_BUILD_ROOT
rm -rf ${BUILD_ROOT}
mkdir -p ${BUILD_ROOT}/var/opencore
mkdir -p ${BUILD_ROOT}/var/opencore/bin
install -m 750 -d ${BUILD_ROOT}/var/opencore/log
install -m 755 -d ${BUILD_ROOT}/var/opencore/sockets
install -m 700 -d ${BUILD_ROOT}/var/opencore/sockets/swupd
cp -rf swupd.app ${BUILD_ROOT}/var/opencore/bin/
install -m 755 swupd ${BUILD_ROOT}/var/opencore/bin/swupd
mkdir -p ${BUILD_ROOT}/etc/rc.d/init.d
install -m 755 contrib/redhat.init ${BUILD_ROOT}/etc/rc.d/init.d/openpanel-swupd

%post
chown -R root:authd /var/opencore/sockets
chmod -R 775 /var/opencore/sockets
chmod 755 /var/opencore/tools
chkconfig --level 2345 openpanel-swupd on

%files
%defattr(-,root,root)
/
