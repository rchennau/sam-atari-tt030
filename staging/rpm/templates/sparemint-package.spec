#
# SpareMiNT RPM Spec File Template for Atari TT030 (M68030/FreeMiNT)
#

Summary:        RPM package template for Atari TT030 FreeMiNT userland
Name:           sample-package
Version:        1.0.0
Release:        1
License:        GPLv2+
Group:          Applications/System
URL:            https://freemint.github.io/sparemint/
Source0:        %{name}-%{version}.tar.gz
# Patch0:         %{name}-%{version}-mint.patch

BuildRoot:      %{_tmppath}/%{name}-%{version}-root
Prefix:         /usr

# Target architecture for Atari TT030 (Motorola 68030)
BuildArch:      m68kmint
Requires:       mintlib >= 0.59.1, freemint >= 1.18.0
BuildRequires:  mintlib-devel, gcc-m68k

%description
This is a standard RPM spec template configured for building native or
cross-compiled packages targeted at the Atari TT030 (M68030 / FreeMiNT)
running the SpareMiNT package distribution.

%prep
%setup -q
# %patch0 -p1

%build
# Set optimization flags for Motorola 68030
export CFLAGS="${RPM_OPT_FLAGS:- -O2 -fomit-frame-pointer} -m68030"
export LDFLAGS="-Wl,-stack,64k"

if [ -f ./configure ]; then
    ./configure \
        --prefix=%{_prefix} \
        --exec-prefix=%{_prefix} \
        --bindir=%{_bindir} \
        --sbindir=%{_sbindir} \
        --libexecdir=%{_libexecdir} \
        --datadir=%{_datadir} \
        --sysconfdir=/etc \
        --sharedstatedir=%{_prefix}/com \
        --localstatedir=/var \
        --libdir=%{_libdir} \
        --includedir=%{_includedir} \
        --infodir=%{_infodir} \
        --mandir=%{_mandir}
fi

make %{?_smp_mflags}

%install
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

make install DESTDIR=%{buildroot} prefix=%{buildroot}%{_prefix}

# Strip binaries for m68k MiNT environment
if command -v m68k-atari-mint-strip >/dev/null 2>&1; then
    find %{buildroot}%{_bindir} %{buildroot}%{_sbindir} -type f -exec m68k-atari-mint-strip {} + 2>/dev/null || true
fi

%clean
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc README COPYING ChangeLog
%{_bindir}/*
# %{_mandir}/man1/*

%changelog
* Tue Sep 15 2026 Maintainer <maintainer@atari.local> - 1.0.0-1
- Initial SpareMiNT RPM spec template release for Atari TT030 M68030/FreeMiNT.
