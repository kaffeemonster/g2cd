# Defines a constant that is used later by the spec file.
%define shortname g2cd
%define ver_real 0.0.00.11
%define ver_revno r428

# Name, brief description, and version 
Summary: A G2 hub only implementation
Name: %{shortname}
Version: %{ver_real}.%{ver_revno}
Release: 1
License: GPLv3
%define fullname %{name}-%{ver_real}-%{ver_revno}
# norootforbuild

# Define buildroot
BuildRoot: %{_tmppath}/%{fullname}

# Software group
Group: Productivity/Networking/File-Sharing

# Source tar ball.
Source: %{fullname}.tar.gz

# Location of the project's home page.
URL: http://g2cd.sourceforge.net

# Owner of the product.
Vendor: Jan Seiffert

# Boolean that specifies if you want to automatically determine some
# dependencies.
AutoReq: yes

# Mandriva 2009.1 is broken, it can not decide which db-devel to install by default
%if 0%{?mdkversion}
%if 0%{?mdkversion} == 200910
%ifarch x86_64
BuildRequires: gcc zlib-devel db-devel lib64db4.7-devel dbus-devel libdbus-1-devel libpcap-devel python pkgconfig ncurses-devel
%else
BuildRequires: gcc zlib-devel db-devel libdb4.7-devel dbus-devel libdbus-1-devel libpcap-devel python pkgconfig ncurses-devel
# endif arch
%endif
%else
BuildRequires: gcc zlib-devel db-devel dbus-devel libdbus-1-devel libpcap-devel python pkgconfig ncurses-devel
# endif mdkversion ==
%endif
# else mdkversion
%else
%if 0%{?suse_version}
%if 0%{?suse_version} >= 1000
BuildRequires: gcc zlib-devel db-devel dbus-1-devel libpcap-devel python pkg-config ncurses-devel
%else
BuildRequires: gcc zlib-devel db-devel libpcap-devel python ncurses-devel
%endif
# else suse_version
%else
%if 0%{?rhel_version}
%if 0%{?rhel_version} >= 500 && 0%{?rhel_version} < 600
BuildRequires: gcc zlib-devel db-devel dbus-devel libpcap-devel python pkgconfig ncurses-devel
%else
BuildRequires: gcc zlib-devel db-devel dbus-devel python pkgconfig ncurses-devel
%endif
%else
BuildRequires: gcc zlib-devel db-devel dbus-devel libpcap-devel python pkgconfig ncurses-devel
%endif
# endif suse_version
%endif
# endif mdkversion
%endif

# In-depth description.
%description
This is a server-only implementation for the Gnutella 2 P2P-protocol. Gnutella and Gnutella 2 are self-organizing, "server-less" P2P-networks. But, to maintain network-connectivity, some network nodes become, dynamicaly, special nodes (Ultra-peers/Hubs).  The main idea behind this piece of software is to be able to set up a dedicated Gnutella 2 Hub to maintain the G2 network connectivity.  With its ability to run on a typical server grade machine (UNIX® system / 24-7 / static IP / high Bandwidth / SMP), it will hopefully be able to improve the overall Gnutella 2 performance.

%prep
%setup -q -n "%{fullname}"

%build
# Mandriva... -Werror=format-security, srsly?
# If you guys really think that makes your distro safer
# how about hiring a guy who pimps the detection logic fixing the false positives?
# and they even fuck it up more...
# lowering the strict aliasing warning level raises warnings in the IPv6 test makros
# make the compiler better before pulling such stunts!
RPM_OPT_FLAGS=`echo "$RPM_OPT_FLAGS" | sed -e 's|-Werror=format-security||' | sed -e 's|-Wstrict-aliasing=2||'`
# Configure
./configure --enable-release --prefix=/usr --bindir="%{_bindir}" --sysconfdir="%{_sysconfdir}" --localstatedir="%{_localstatedir}" --mandir="%{_mandir}" CFLAGS="$RPM_OPT_FLAGS"

make %{?jobs:-j %{jobs}}

%install
# rpms strip themself
make DESTDIR="%{buildroot}" install_striped

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc README COPYING
%{_bindir}/g2cd
%config %{_sysconfdir}/g2cd.conf
%attr(755,nobody,nobody) %dir %{_localstatedir}/cache/g2cd
%doc %{_mandir}/man1/*

%changelog
* Thu Jan 24 2010 Jan Seiffert <kaffeemonster@googlemail.com>
- 0.0.00.11.20100624 initial version
