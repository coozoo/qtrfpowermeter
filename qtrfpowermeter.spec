# spectool -g -R qtrfpowermeter.spec
# rpmbuild -ba qtrfpowermeter.spec

%define name qtrfpowermeter
%define reponame qtrfpowermeter
%define version %(echo "$(curl --silent 'https://raw.githubusercontent.com/coozoo/qtrfpowermeter/main/main.cpp'|grep 'QString APP_VERSION'| tr -d ' '|grep -oP '(?<=constQStringAPP_VERSION=").*(?=\";)')")
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}

Summary: QT Radio Frequency Power Meter application UI for logarithmic detector RF-8000
Name: %{name}
Version: %{version}
Release: %{build_timestamp}%{?dist}
Source0: https://github.com/coozoo/qtrfpowermeter/archive/main.zip#/%{name}-%{version}.tar.gz


License: MIT

BuildRequires: qt5-qtbase-devel >= 5.9
BuildRequires: qt5-linguist >= 5.9
BuildRequires: qt5-qtserialport >= 5.9
BuildRequires: qt5-qtcharts >= 5.9

# Requires: qt5 >= 5.5

Url: https://github.com/coozoo/qtrfpowermeter

%description

Application to visualize measured RF power and work with Chinese serial port RF power meters,
labeled as RF-Power500, RF-Power3000, RF-Power8000 (RF-500, RF-3000, RF-8000)
Allows on fly visualization, build charts of measured power and log data in csv format. 


%global debug_package %{nil}

%prep
#%setup -q -n %{name}-%{version}
%setup -q -n %{name}-main

%build
# don't know maybe it's stupid me but lrelease in qt looks like runs after make file generation as result automatic file list inside qmake doesn't work
# so what I need just run it twice...
qmake-qt5
make
qmake-qt5
make

%install
make INSTALL_ROOT=%{buildroot} -j$(nproc) install

%post

%postun

%files
%{_bindir}/*
%{_datadir}/*

%changelog
