Name: %NAME
Version: %VERSION
Release:	1%{?dist}
Summary: Common Thrift Client and asynchronized library

Group: Development/Libraries		
License: BSD	
Source: %{NAME}-%{VERSION}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Requires:gflags
Requires:glog
Requires:protobuf >= 2.5.0
Requires:libev

%description

%package devel
Summary: Common Thrift Client and asynchronized library
Group: Development/Libraries
Requires: %{NAME} = %{VERSION}

%description devel

%prep
%setup -q


%build
%configure
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)

%{_libdir}/*.so.*

%files devel
%defattr(-,root,root)

%{_includedir}/thrift
%{_libdir}/*.a
%{_libdir}/*.la
%{_libdir}/*.so

%changelog

