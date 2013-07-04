Name:       qtcontacts-tracker
Summary:    QtContact tracker storage plugin
Version:    4.13.4.1
Release:    1
Group:      System/Libraries
License:    LGPL v2.1
URL:        http://gitorious.org/qtcontacts-tracker
Source0:    %{name}-%{version}.tar.bz2
#Patch0: 0001-Changes-Use-const-where-possible.patch
#Patch1: 0002-Changes-Delete-QSparqlResults-as-they-finish-in-save.patch
#Patch2: 0003-Changes-Add-virtual-destructor-in-QctSparqlResolverD.patch
#Patch3: 0004-Changes-Add-virtual-destructor-to-RelatedObjectInfoD.patch


BuildRequires:  pkgconfig(QtCore) >= 4.6.0
BuildRequires:  pkgconfig(QtDBus)
BuildRequires:  pkgconfig(QtGui)
BuildRequires:  libqtsparql-devel >= 0.0.18
BuildRequires:  qt-mobility-devel >= 1.2
BuildRequires:  libqtsparql-tracker-extensions-devel >= 0.0.5
BuildRequires:  fdupes
Requires: tracker >= 0.10.6
Requires: libqtsparql-tracker >= 0.0.18


%description
QtContact tracker storage plugin

%package tests
Summary:    Tests for QtContact tracker storage plugin
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description tests
Tests package for QtContact tracker storage plugin.

%package benchmarks
Summary:    Benchmarks for QtContact tracker storage plugin
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description benchmarks
Benchmarks package for QtContact tracker storage plugin.

%package -n libqtcontacts-tracker-extensions
Summary:    Specific QContact extensions from qtcontacts-tracker
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description -n libqtcontacts-tracker-extensions
The qtcontacts-tracker-extensions provides additionnal functions and
constants used by qtcontacts-tracker, but not part of the official QContact API.

%package -n libqtcontacts-tracker-extensions-devel
Summary:    Development files for libqtcontacts-tracker-extensions
Group:      Development/Libraries
Requires:   libqtcontacts-tracker-extensions = %{version}-%{release}

%description -n libqtcontacts-tracker-extensions-devel
The qtcontacts-tracker-extensions-devel package includes the header files
for qtcontacts-tracker-extensions.


%prep
%setup -q -n %{name}-%{version}

%patch0 -p1
%patch1 -p1

%build
./configure --prefix %{_prefix} --disable-builddirs-rpath --disable-schemalist

make # %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
make install INSTALL_ROOT=$RPM_BUILD_ROOT

%fdupes  %{buildroot}/%{_datadir}/libqtcontacts-tracker-tests/

%post -n libqtcontacts-tracker-extensions -p /sbin/ldconfig

%postun -n libqtcontacts-tracker-extensions -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/qt4/plugins/contacts/libqtcontacts_tracker.so


%files tests
%defattr(-,root,root,-)
%{_bindir}/ut_*
%{_datadir}/libqtcontacts-tracker-tests/*

%files benchmarks
%defattr(-,root,root,-)
%{_bindir}/bm_*

%files -n libqtcontacts-tracker-extensions
%defattr(-,root,root,-)
%{_libdir}/libqtcontacts_extensions_tracker.so.*

%files -n libqtcontacts-tracker-extensions-devel
%defattr(-,root,root,-)
%{_libdir}/libqtcontacts_extensions_tracker.so
%{_libdir}/libqtcontacts_extensions_tracker.prl
%{_includedir}/qtcontacts-tracker/*
%{_datadir}/qt4/mkspecs/features/*

