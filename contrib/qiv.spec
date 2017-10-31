# Should work on most rpm based distros. Package names might differ here and there.
# Tested on RHEL6/CentOS6, make sure epel repository is enabled.
Name:          qiv
Version:       2.3.2
Release:       1
Summary:       A quick image viewer for X
Source0:       http://spiegl.de/qiv/download/%{name}-%{version}.tar.gz
License:       GPL
Url:           http://spiegl.de/qiv/

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-buildroot

BuildRequires: gtk2-devel
BuildRequires: libexif-devel
BuildRequires: imlib2-devel
BuildRequires: file-devel
BuildRequires: pkgconfig
BuildRequires: lcms2-devel
BuildRequires: libtiff-devel
# This has most likely to be another jpeg package on distros other than RHEL/CentOS
BuildRequires: libjpeg-turbo-devel

Requires:      gtk2
Requires:      libexif
Requires:      imlib2
Requires:      file
Requires:      lcms2

Group:         Graphics

%description
Quick Image Viewer (qiv) is a very small and pretty fast GDK2.0/Imlib2 image
viewer. Features include zoom, maxpect, scale down, fullscreen,
brightness/contrast/gamma correction, slideshow, pan with keyboard and mouse,
rotate left/right, flip, delete (move to .qiv-trash/), jump to image x, jump
forward/backward x images, filename filter, and you can use qiv to set your
X11-Desktop background. With multi monitor support.

qiv also supports the assignment of external commands (like metacam) to unused
keys.

%prep
%setup -q -n %{name}-%{version}

%build
echo RPM_BUILD_ROOT ist $RPM_BUILD_ROOT
echo pwd ist
pwd
make CFLAGS="$RPM_OPT_FLAGS"

%install
echo pwd ist
pwd
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT/{%{_bindir},%{_mandir}/man1}

DISPLAY="" make PREFIX=$RPM_BUILD_ROOT%{_prefix} install
# Hmm. Any better ideas how to achieve this?
mkdir -p  %{_builddir}/%{name}-%{version}/examples/
cp -p %{_builddir}/%{name}-%{version}/contrib/gps2url.py %{_builddir}/%{name}-%{version}/examples
cp -p %{_builddir}/%{name}-%{version}/contrib/qiv-command.example %{_builddir}/%{name}-%{version}/examples/qiv-command

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc README Changelog README.COPYING README.INSTALL intro.jpg examples
%{_bindir}/qiv
%{_mandir}/man1/qiv.1*
%{_prefix}/share/applications/qiv.desktop
%{_prefix}/share/pixmaps/qiv.png

%changelog
* Thu Oct 10 2017 Thomas Wiegner <wiegner@gmx.de> 2.3.2
- qiv changes since v2.3.1:
  + [tw] fix: window sometimes recentered, when moved around with mouse
  + [as] fix Makefile for cross-compiling: Make pkg-config substitutable (Debian Bug#879108)
  + [tw] add specfile to build rpm for CentOS
  + [tw] add option --trashbin to use trash bin instead
         of .qiv-trash when deleting images
  + [as] add ability (and options) to display JPEG comments
  + [as] more sophisticated exiftool call in qiv-command example
  + [tw] fix inconsistent fixed-zoom behaviour
  + [tw] Don't try to rotate if file is not an image
  + [tw] get rid of some more XID collisions
  + [tw] Fix occasionally erratic behaviour of qiv statusbar
  + [tw] Fix xpm autodetection with libmagic
  + [tw] Fix small artefacts in pictures which might happen when
         running remote over slow link (patch by Derek Schrock)
  + [tw] update image after it is exposed in window mode
  + [js/tw] fix broken "-no-filter" option
            libmagic filter now works on symlinks
  + [tw] fix linking order in "make debug", did not
         work in some newer gcc versions
  + [as] turn on EXIF autorotation by default
  + [tw] Add option to sort files by modification time.
         (patch by Stefan Rüger)
  + [tw] In case of "watch" option, check image only every 1/10 sec,
         instead of every 200us. This was way too short for
         devices like RasPi etc. (Hamish)
  + [tw] Leave jumping mode on invalid input and process input key
         as if it was entered in non jumping mode (Sergey Pinaev)

