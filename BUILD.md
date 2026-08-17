# Building Heliod

This document describes how to build Heliod from source on a current Linux distribution.

## Requirements

### Build host

The build has been verified on:

- Ubuntu 26.04 LTS (x86_64)
- gcc / g++ 15.2.0
- GNU Make 4.4.1

### System packages

The following packages are required on a Debian or Ubuntu system:

```bash
sudo apt-get install \
  build-essential \
  libnspr4-dev libnss3-dev \
  libsasl2-dev \
  libxerces-c-dev libxalan-c-dev \
  libicu-dev
```

### SBC dependency tree

Heliod needs a pre-built `ldapsdk` in the `SBC` tree (the dependency tree used by the project). The source repository includes build helpers in `sbc/` that build and install MozLDAP into that tree.

#### Build the SBC `ldapsdk`

1. Download the MozLDAP C-SDK 6.0.5 source tarball into `SBC_SRC`:

   ```bash
   export SBC_SRC=/opt/sbc-sources
   mkdir -p $SBC_SRC
   cd $SBC_SRC
   wget https://ftp.mozilla.org/pub/directory/c-sdk/releases/v6.0.5/src/LDAPCSDK_6_0_5_RTM.tar.gz
   ```

2. Set `SBC` and run the platform build helper:

   ```bash
   export SBC=/opt/heliod-sbc
   make -C sbc SBC_SRC=/opt/sbc-sources SBC=/opt/heliod-sbc linux64
   ```

   For a 32-bit build, use `linux32` instead.

The build helpers are in `sbc/ldapsdk/Makefile.linux` and are not run automatically by the top-level `make`. The exact `OBJDIR` name is selected by the platform `pinfo` utility based on the host OS and the `BUILD64` variable.

## Build

```bash
cd <source-root>
make SBC=/opt/heliod-sbc
```

This will build the C/C++ server, support libraries, and plugins. The Java/XSD code-generation steps are also run by the top-level `make`.

To build only the Java components:

```bash
ant -f src/java/build.xml
```

## Common environment variables

- `SBC` — root of the dependency tree (required).
- `BUILD64` — set to `1` to build 64-bit binaries. Omit to use the platform default.
- `LDAPSDK_VERSION` — version of the MozLDAP SDK to use (default is `6_0_5`).
- `BASE_DEFINES_NAME` — normally auto-detected by `pinfo`; can be overridden for cross-platform builds.

## Smoke test

The `test/` directory contains a minimal test harness:

```bash
make -C test
```

## Platform notes

### Debian / Ubuntu

The platform definitions are in `make/defines_Linux_debian*.mk`. On current distributions the `ldap_ssl.h` header is provided by the MozLDAP source tree, so `LDAPSDK_INC` includes the `work/mozldap-$(subst _,.,$(LDAPSDK_VERSION))/mozilla/dist/public/ldap` directory.

### SunOS / illumos

SunOS-style mapfiles are still supported on those platforms. On Linux the mapfile workflow is disabled and `-fPIC` is forced for all shared-library and plugin components.

## Known issues

