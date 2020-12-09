Building VirtualGL
==================


Build Requirements
------------------

- [CMake](http://www.cmake.org) v2.8.12 or later
  * CMake v3.1 or later is required if building the VirtualGL Faker with the
    OpenCL interposer enabled (this is the default on Linux and FreeBSD but can
    be changed using the `VGL_FAKEOPENCL` CMake variable.)

- If building SSL support:
  * [OpenSSL](http://www.OpenSSL.org) -- see "Building SSL Support" below

- libjpeg-turbo SDK v1.2 or later
  * The libjpeg-turbo SDK binary packages can be downloaded from the "Files"
    area of <http://sourceforge.net/projects/libjpeg-turbo>.
  * The VirtualGL build system will search for the TurboJPEG header and
    library under **/opt/libjpeg-turbo** on Un*x, but you can override this by
    setting the `TJPEG_INCLUDE_DIR` CMake variable to the directory containing
    turbojpeg.h and the `TJPEG_LIBRARY` CMake variable to either the full path
    of the TurboJPEG library against which you want to link or a set of link
    flags needed to link with the TurboJPEG library (for instance,
    `-DTJPEG_LIBRARY="-L/opt/libjpeg-turbo/lib64 -lturbojpeg"`.)

- G++ or Clang
  * On Mac platforms, these can be obtained from
    [MacPorts](http://www.macports.org/) or [Homebrew](http://brew.sh/) or by
    installing [Xcode](http://developer.apple.com/tools/xcode).

- X11 and OpenGL development libraries:
  * libX11, libXext, libXtst, libGL, and libGLU
      - libEGL, libxcb, and libxcb-keysyms (if building the VirtualGL Faker)
      - libXv (if building VirtualGL with X Video support)
  * On Mac platforms, these are distributed with
    [XQuartz](http://xquartz.macosforge.org).
  * The OpenGL and GLX headers must be reasonably modern (generally Mesa 9 or
    later, or the equivalent.)

- OpenCL development libraries [if building the VirtualGL Faker with the OpenCL
  interposer enabled]
  * On Red Hat Enterprise Linux 6-7 (and work-alikes), these are located in the
    [EPEL](https://fedoraproject.org/wiki/EPEL) repository.

- Some requisite development libraries are located in the CRB (Code Ready
  Builder) repository on Red Hat Enterprise Linux 8 and in the PowerTools
  repository on CentOS 8, which is not enabled by default.


32-bit VirtualGL Builds on x86-64 Linux Distributions
-----------------------------------------------------

In order to run 32-bit OpenGL applications using VirtualGL on a 64-bit Linux
platform, it is necessary to build both 32-bit and 64-bit versions of the
VirtualGL server components.  However, most recent x86-64 Linux distributions
do not ship with the 32-bit libraries necessary to build a 32-bit version of
VirtualGL.  To build 32-bit VirtualGL components on an x86-64 Linux system, you
will need, at minimum, the following 32-bit development libraries:

- glibc and libstdc++
- X11 and OpenGL (see above)

Instructions for installing these on specific distributions:

### Red Hat Enterprise Linux 6+ (and work-alikes), Recent Fedora Releases

- Execute the following command as root:

        yum install libXv-devel.i686 libXext-devel.i686 libXtst-devel.i686 libX11-devel.i686 libxcb-devel.i686 xcb-util-keysyms-devel.i686 mesa-libGLU-devel.i686 mesa-libGL-devel.i686 mesa-libEGL-devel.i686 glibc-devel.i686 libstdc++-devel.i686 libstdc++-static.i686

  * Replace `yum` with `dnf` on Fedora 23+ or RHEL 8+.
  * Add `ocl-icd-devel.i686` to the command line if building the VirtualGL
    Faker with the OpenCL interposer enabled.

### Ubuntu

- Execute the following command as root:

        apt-get install g++-multilib libxv-dev:i386 libxtst-dev:i386 libx11-xcb-dev:i386 libxcb-keysyms1-dev:i386 libegl1-mesa-dev:i386 libglu1-mesa-dev:i386

  * Add `ocl-icd-opencl-dev:i386` to the command line if building the VirtualGL
    Faker with the OpenCL interposer enabled.

Out-of-Tree Builds
------------------

Binary objects, libraries, and executables are generated in the directory from
which CMake is executed (the "binary directory"), and this directory need not
necessarily be the same as the VirtualGL source directory.  You can create
multiple independent binary directories, in which different versions of
VirtualGL can be built from the same source tree using different compilers or
settings.  In the sections below, *{build_directory}* refers to the binary
directory, whereas *{source_directory}* refers to the VirtualGL source
directory.  For in-tree builds, these directories are the same.


Build Procedure
---------------

The following procedure will build the VirtualGL Client and, on Linux and other
Un*x variants (except Mac), the VirtualGL Server components.  On most 64-bit
systems (Solaris being a notable exception), this will build a 64-bit version
of VirtualGL.  See "Build Recipes" for specific instructions on how to build a
32-bit or 64-bit version of VirtualGL on systems that support both.

    cd {build_directory}
    cmake -G"Unix Makefiles" [additional CMake flags] {source_directory}
    make

Replace `make` with `ninja` and `Unix Makefiles` with `Ninja` if using Ninja.


Debug Build
-----------

Add `-DCMAKE_BUILD_TYPE=Debug` to the CMake command line.


Building Secure Sockets Layer (SSL) Support
-------------------------------------------

If built with SSL support, VirtualGL can use OpenSSL to encrypt the traffic it
sends and receives via the VGL Transport.  This is only a marginally useful
feature, however, since VirtualGL can also tunnel the VGL Transport through
SSH.  To enable SSL support, set the `VGL_USESSL` CMake variable to `1`.

In general, if you are building on a Unix-ish platform that has the OpenSSL
link libraries and include files installed in the standard system locations,
then the VirtualGL build system should detect the system version of OpenSSL
automatically and link against it.  However, this produces a version of
VirtualGL that depends on the OpenSSL dynamic libraries, and thus the VirtualGL
binaries are not necessarily portable.  Thus, to build a fully portable,
cross-compatible version of VirtualGL with SSL support, it is necessary on most
systems to link against the OpenSSL static libraries.  The following sections
describe how to do that on various platforms.


### Linux

There is generally no sane way to statically link with OpenSSL on Linux without
building OpenSSL from source.  Some distributions of Linux ship with the
OpenSSL static libraries, but these usually depend on Kerberos, which
introduces a long list of dependencies, some of which aren't available in
static library form.  To build OpenSSL from source:

* Download the latest OpenSSL source tarball from <http://www.OpenSSL.org>
* Extract the tarball
* cd to the OpenSSL source directory and issue one of the following commands to
  configure the OpenSSL build:

    **64-bit:**

      `./Configure linux-x86_64 shared no-krb5 no-dso`

    **32-bit:**

      `./Configure -m32 linux-generic32 shared no-krb5 no-dso`

* `make`

You can then manipulate the `OPENSSL_INCLUDE_DIR`, `OPENSSL_SSL_LIBRARY`, and
`OPENSSL_CRYPTO_LIBRARY` CMake variables to link VirtualGL against your custom
OpenSSL build.  For instance, adding

    -DVGL_USESSL=1 -DOPENSSL_INCLUDE_DIR=~/openssl/include \
      -DOPENSSL_SSL_LIBRARY=~/openssl/libssl.a \
      -DOPENSSL_CRYPTO_LIBRARY=~/openssl/libcrypto.a

to the CMake command line will cause VirtualGL to be statically linked against
a custom build of OpenSSL that resides under **~/openssl**.


### Mac

Linking with the OpenSSL dynamic libraries is generally not a concern on OS X,
since Apple ships several versions of these in order to retain backward
compatibility with prior versions of OS X.


### Solaris

The easiest approach on Solaris is to install the OpenSSL development libraries
from [OpenCSW](http://www.OpenCSW.org).  You can then add one of the following
to the CMake command line to statically link VirtualGL with OpenSSL:

  **64-bit:**

    -DVGL_USESSL=1 -DOPENSSL_INCLUDE_DIR=/opt/csw/include \
      -DOPENSSL_SSL_LIBRARY=/opt/csw/lib/64/libssl.a \
      -DOPENSSL_CRYPTO_LIBRARY=/opt/csw/lib/64/libcrypto.a

  **32-bit:**

    -DVGL_USESSL=1 -DOPENSSL_INCLUDE_DIR=/opt/csw/include \
      -DOPENSSL_SSL_LIBRARY=/opt/csw/lib/libssl.a \
      -DOPENSSL_CRYPTO_LIBRARY=/opt/csw/lib/libcrypto.a


Build Recipes
-------------


### 32-bit Build on 64-bit Linux/Unix

Use export/setenv to set the following environment variables before running
CMake:

    CFLAGS=-m32
    CXXFLAGS=-m32
    LDFLAGS=-m32


### 64-bit Build on Solaris

Use export/setenv to set the following environment variables before running
CMake:

    CFLAGS=-m64
    CXXFLAGS=-m64
    LDFLAGS=-m64


### Other Compilers

On Un*x systems, prior to running CMake, you can set the `CC` environment
variable to the command used to invoke the C compiler and the `CXX` environment
variable to the command used to invoke the C++ compiler.


Advanced CMake Options
----------------------

To list and configure other CMake options not specifically mentioned in this
guide, run

    ccmake {source_directory}

or

    cmake-gui {source_directory}

from the build directory after initially configuring the build.  CCMake is a
text-based interactive version of CMake, and CMake-GUI is a GUI version.  Both
will display all variables that are relevant to the VirtualGL build, their
current values, and a help string describing what they do.


Installing VirtualGL
====================

You can use the build system to install VirtualGL (as opposed to creating an
installer package.)  To do this, run `make install` or `ninja install`.
Running `make uninstall` or `ninja uninstall` will uninstall VirtualGL.

The `CMAKE_INSTALL_PREFIX` CMake variable can be modified in order to install
VirtualGL into a directory of your choosing.  If you don't specify
`CMAKE_INSTALL_PREFIX`, then the default is **/opt/VirtualGL**.  The default
value of `CMAKE_INSTALL_PREFIX` causes the VirtualGL files to be installed with
a directory structure resembling that of the official VirtualGL binary
packages.  Changing the value of `CMAKE_INSTALL_PREFIX` (for instance, to
**/usr/local**) causes the VirtualGL files to be installed with a directory
structure that conforms to GNU standards.

The `CMAKE_INSTALL_BINDIR`, `CMAKE_INSTALL_DATAROOTDIR`,
`CMAKE_INSTALL_DOCDIR`, `CMAKE_INSTALL_INCLUDEDIR`, and `CMAKE_INSTALL_LIBDIR`
CMake variables allow a finer degree of control over where specific files in
the VirtualGL distribution should be installed.  These directory variables can
either be specified as absolute paths or as paths relative to
`CMAKE_INSTALL_PREFIX` (for instance, setting `CMAKE_INSTALL_DOCDIR` to **doc**
would cause the documentation to be installed in
**${CMAKE\_INSTALL\_PREFIX}/doc**.)  If a directory variable contains the name
of another directory variable in angle brackets, then its final value will
depend on the final value of that other variable.  For instance, the default
value of `CMAKE_INSTALL_DOCDIR` if installing under **/opt/VirtualGL** is
**\<CMAKE\_INSTALL\_DATAROOTDIR\>/doc**.

NOTE: If setting one of these directory variables to a relative path using the
CMake command line, you must specify that the variable is of type `PATH`. For
example:

    cmake -G"{generator type}" -DCMAKE_INSTALL_LIBDIR:PATH=lib {source_directory}

Otherwise, CMake will assume that the path is relative to the build directory
rather than the install directory.


Creating Distribution Packages
==============================

The following commands can be used to create various types of distribution
packages (replace `make` with `ninja` if using Ninja):


Linux
-----

    make rpm

Create Red Hat-style binary RPM package.  Requires RPM v4 or later.

    make deb

Create Debian-style binary package.  Requires dpkg.


Mac
---

    make dmg

Create Mac package/disk image.  This requires pkgbuild and productbuild, which
are installed by default on OS X 10.7 and later.
