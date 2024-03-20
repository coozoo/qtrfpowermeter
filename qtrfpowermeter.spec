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
Url: https://github.com/coozoo/qtrfpowermeter

%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: qt5-qtbase-devel >= 5.9
BuildRequires: qt5-linguist >= 5.9
BuildRequires: qt5-qtserialport-devel >= 5.9
BuildRequires: qt5-qtcharts-devel >= 5.9
%endif
%if 0%{?suse_version} || 0%{?sle_version}
Group:          Electronics
BuildRequires:  pkgconfig(Qt5Widgets)
BuildRequires:  libqt5-qtbase-devel
BuildRequires:  libqt5-linguist
BuildRequires:  libqt5-qtserialport-devel
BuildRequires:  libqt5-qtcharts-devel
BuildRequires:  update-desktop-files
Requires(post): update-desktop-files
Requires(postun): update-desktop-files
%endif
%if 0%{?mageia}
BuildRequires: lib64qt5base5-devel >= 5.9
BuildRequires: lib64qt5help-devel >= 5.9
BuildRequires: lib64qt5serialport-devel >= 5.9
BuildRequires: lib64qt5charts-devel >= 5.9
%endif


# Requires: qt5 >= 5.5

%description

Application to visualize measured RF power and work with Chinese serial port RF power meters,
labeled as RF-Power500, RF-Power3000, RF-Power8000 (RF-500, RF-3000, RF-8000)
Allows on fly visualization, build charts of measured power and log data in csv format. 


%global debug_package %{nil}

%prep
#copr build
#%setup -q -n %{name}-%{version}
#local build
%setup -q -n %{reponame}-main

%build
# don't know maybe it's stupid me but lrelease in qt looks like runs after make file generation as result automatic file list inside qmake doesn't work
# so what I need just run it twice...
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
    qmake-qt5
    make
    qmake-qt5
    make
%endif
%if 0%{?mageia} || 0%{?suse_version} || 0%{?sle_version}
    %qmake5
    %make_build
    %qmake5
    %make_build
%endif

%install
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
    make INSTALL_ROOT=%{buildroot} -j$(nproc) install
%endif
%if 0%{?mageia} || 0%{?suse_version} || 0%{?sle_version}
    %qmake5_install
    %suse_update_desktop_file -G "QT RF Power Meter" -r qtrfpowermeter Electronics
%endif

%post
%if 0%{?suse_version} ||  0%{?sle_version}
    %desktop_database_post
%endif

%postun
%if 0%{?suse_version} || 0%{?sle_version}
    %desktop_database_postun
%endif

%files
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version} || 0%{?mageia}
    %{_bindir}/*
    %{_datadir}/*
%endif
%if 0%{?suse_version} || 0%{?sle_version}
    %license LICENSE
    %doc README.md
    %{_bindir}/*
    %{_datadir}/*
    %{_datadir}/applications/qtrfpowermeter.desktop
%endif

%changelog
