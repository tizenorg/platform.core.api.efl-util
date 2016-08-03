
Name:       capi-ui-efl-util
Summary:    An EFL utility library in Tizen C API
Version:    0.3.5
Release:    1
Group:      Graphics & UI Framework/API
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: 	capi-ui-efl-util.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-tbm-client)
BuildRequires:  pkgconfig(screenshooter-client)
BuildRequires:  pkgconfig(tizen-extension-client)
BuildRequires:  pkgconfig(ecore-wayland)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(libtbm)

%global TZ_SYS_RO_SHARE  %{?TZ_SYS_RO_SHARE:%TZ_SYS_RO_SHARE}%{!?TZ_SYS_RO_SHARE:/usr/share}

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

cp -a include/efl_util.h.in include/efl_util.h
%if "%{profile}" != "wearable"
   sed -i 's/\$TZ_CFG_VER_24_OR_30\$/2.4/g'    include/efl_util.h
   sed -i 's/\$TZ_CFG_VER_24_OR_231\$/2.4/g'   include/efl_util.h
   sed -i '/\$TZ_CFG_KEEP_BEGIN\$/d'           include/efl_util.h
   sed -i '/\$TZ_CFG_KEEP_END\$/d'             include/efl_util.h
%else
   sed -i 's/\$TZ_CFG_VER_24_OR_30\$/3.0/g'    include/efl_util.h
   sed -i 's/\$TZ_CFG_VER_24_OR_231\$/2.3.1/g' include/efl_util.h
   sed -ie '/\$TZ_CFG_KEEP_BEGIN\$/,/\$TZ_CFG_KEEP_END\$/{s/\$TZ_CFG_KEEP_BEGIN\$//p;d}' include/efl_util.h
%endif


%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%cmake . -DFULLVER=%{version} -DMAJORVER=${MAJORVER} \
%if "%{profile}" == "wearable"
    -DTIZEN_WEARABLE=YES \
%endif

make %{?jobs:-j%jobs}

%install
%make_install

# for license notification
mkdir -p %{buildroot}/%{TZ_SYS_RO_SHARE}/license
cp -a LICENSE.APLv2 %{buildroot}/%{TZ_SYS_RO_SHARE}/license/%{name}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest %{name}.manifest
%{_libdir}/libcapi-ui-efl-util.so.*
%{TZ_SYS_RO_SHARE}/license/%{name}

%files devel
%manifest %{name}.manifest
%{_includedir}/ui/efl_util.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/libcapi-ui-efl-util.so
%exclude %{_includedir}/ui/config.h
