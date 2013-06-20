Name:       capi-ui-efl-util
Summary:    An EFL utility library in SLP C API
Version:    0.1.0
Release:    1
Group:      Graphics & UI Framework/API
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(capi-base-common)

%description
An EFL utility library in SLP C API.


%package devel
Summary:  An EFL utility library in SLP C API (Development)
Requires: %{name} = %{version}-%{release}

%description devel
%devel_desc

%prep
%setup -q


%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%cmake . -DFULLVER=%{version} -DMAJORVER=${MAJORVER}
make %{?jobs:-j%jobs}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%license LICENSE.APLv2
%{_libdir}/libcapi-ui-efl-util.so.*

%files devel
%{_includedir}/ui/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/libcapi-ui-efl-util.so


