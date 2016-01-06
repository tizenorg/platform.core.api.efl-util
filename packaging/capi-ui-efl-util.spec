%bcond_with x
%bcond_with wayland

Name:       capi-ui-efl-util
Summary:    An EFL utility library in Tizen C API
Version:    0.1.7
Release:    1
Group:      Graphics & UI Framework/API
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: 	capi-ui-efl-util.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
%if %{with x}
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(xtst)
BuildRequires:  pkgconfig(libdri2)
BuildRequires:  pkgconfig(dri2proto)
BuildRequires:  pkgconfig(xext)
BuildRequires:  pkgconfig(xv)
BuildRequires:  pkgconfig(xdamage)
%endif
%if %{with wayland}
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-tbm-client)
BuildRequires:  pkgconfig(screenshooter-client)
BuildRequires:  pkgconfig(tizen-extension-client)
BuildRequires:  pkgconfig(ecore-wayland)
%endif
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(libtbm)

%description
An EFL utility library in SLP C API.


%package devel
Summary:  An EFL utility library in Tizen C API (Development)
Requires: %{name} = %{version}-%{release}

%description devel
%devel_desc

%prep
%setup -q
cp %{SOURCE1001} .


%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%cmake . -DFULLVER=%{version} -DMAJORVER=${MAJORVER} \
%if %{with wayland}
-DWITH_WAYLAND=TRUE
%endif
%if %{with x}
-DWITH_X11=TRUE
%endif

make %{?jobs:-j%jobs}

%install
%make_install

# for license notification
mkdir -p %{buildroot}/usr/share/license
cp -a LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest %{name}.manifest
%license LICENSE.APLv2
%{_libdir}/libcapi-ui-efl-util.so.*
/usr/share/license/%{name}

%files devel
%manifest %{name}.manifest
%{_includedir}/ui/efl_util.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/libcapi-ui-efl-util.so
%exclude %{_includedir}/ui/config.h
