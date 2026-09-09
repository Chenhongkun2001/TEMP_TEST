# A simple guide to update libell

In order to update libell(embedded linux library), we need:
    1. download source codes;
    2. configure & build it;
    3. merge header files & lib* files into gateway source codes;
    4. deploy lib files into gateway.

Take ell0.72 as an example for instructions on how to update it as follow:

## download source codes

fetch libell source code from https://git.kernel.org/pub/scm/libs/ell/ell.git

### download the latest src
forlinx@ubuntu:~/tmp/ell$ git clone https://git.kernel.org/pub/scm/libs/ell/ell.git ;
### switch to the commit(ae0d7cd7515c68c1804da9dd4703a49249b0f987) for tag 0.72
forlinx@ubuntu:~/tmp/ell/ell$ git reset --hard ae0d7cd7515c68c1804da9dd4703a49249b0f987

## configure & build

### double check whether the dependancies are available as expected
    It depends on following packages: build-essential, autoconf, automake, libtool, and pkg-config.
    i: check whether the packages are installed:
        forlinx@ubuntu:~/tmp/ell/ell$ dpkg -l | grep -E 'build-essential|autoconf|automake|libtool|pkg-config'
        ii  autoconf                                   2.69-11                                         all          automatic configure script builder
        ii  automake                                   1:1.15.1-3ubuntu2                               all          Tool for generating GNU Standards-compliant Makefiles
        ii  build-essential                            12.4ubuntu1                                     amd64        Informational list of build-essential packages
        ii  libltdl-dev:amd64                          2.4.6-2                                         amd64        System independent dlopen wrapper for GNU libtool
        ii  libltdl7:amd64                             2.4.6-2                                         amd64        System independent dlopen wrapper for GNU libtool
        ii  libtool                                    2.4.6-2                                         all          Generic library support script
    ii: install the packages if missed(say pkg-config here):
        forlinx@ubuntu:~/tmp/ell/ell$ sudo apt install -y pkg-config
        [sudo] password for forlinx: 
        Reading package lists... Done
        Building dependency tree       
        Reading state information... Done
        The following NEW packages will be installed:
        pkg-config
        0 upgraded, 1 newly installed, 0 to remove and 57 not upgraded.
         ...
### generate configure file and auxiliary files missed
    forlinx@ubuntu:~/tmp/ell/ell$ mkdir -p build-aux
    forlinx@ubuntu:~/tmp/ell/ell$ aclocal
    forlinx@ubuntu:~/tmp/ell/ell$ autoconf
    forlinx@ubuntu:~/tmp/ell/ell$ autoheader
    forlinx@ubuntu:~/tmp/ell/ell$ automake --add-missing
        configure.ac:23: installing 'build-aux/compile'
        configure.ac:35: installing 'build-aux/config.guess'
        configure.ac:35: installing 'build-aux/config.sub'
        configure.ac:10: installing 'build-aux/install-sh'
        configure.ac:35: error: required file 'build-aux/ltmain.sh' not found
        configure.ac:10: installing 'build-aux/missing'
        configure.ac:8: installing 'build-aux/tap-driver.sh'
        Makefile.am: installing 'build-aux/depcomp'
        parallel-tests: installing 'build-aux/test-driver'
### configure and build
generate 'build-aux/ltmain.sh' file
    forlinx@ubuntu:~/tmp/ell/ell$ libtoolize --automake --copy --force
re-generate configure file with parameters
    forlinx@ubuntu:~/tmp/ell/ell$ autoreconf -ivf
    forlinx@ubuntu:~/tmp/ell/ell$ ./configure --target=aarch64-none-linux-gnu --host=aarch64-none-linux-gnu --prefix=/home/forlinx/work/OK62xx-linux-sdk/external-toolchain-dir/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- host_alias=aarch64-none-linux-gnu target_alias=aarch64-none-linux-gnu --no-recursion
    forlinx@ubuntu:~/tmp/ell/ell$ make

### update source code
once the build finished successfully, copy the header and lib files to replace ones:
    forlinx@ubuntu:~/tmp/ell/ell/ell$ cp *.h <src>/GW/skf_gw/source/libell_for_OK62xx/libell/inc/
and:
    forlinx@ubuntu:~/tmp/ell/ell/ell$cp libell.la <src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib/
    forlinx@ubuntu:~/tmp/ell/ell/ell$cp libell-private.la <src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib/
    forlinx@ubuntu:~/tmp/ell/ell/ell$cd .libs
    forlinx@ubuntu:~/tmp/ell/ell/ell/.libs$cp libell.lai <src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib/
    forlinx@ubuntu:~/tmp/ell/ell/ell/.libs$cp libell-private.a <src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib/
    forlinx@ubuntu:~/tmp/ell/ell/ell/.libs$cp libell.so.0.0.2 <src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib/
create 2 soft-link files as follow:
    forlinx@ubuntu:~/tmp/ell/ell/ell/.libs$cd <src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib/
    forlinx@ubuntu:~<src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib$ln -s libell.so.0.0.2 libell.so.0
    forlinx@ubuntu:~<src>/GW/skf_gw/source/libell_for_OK62xx/libell/lib$ln -s libell.so.0.0.2 libell.so
### update library files when deploying
when deploying in gateway, the ell library need to be updated accordingly as follow:
copy libell.la and libell.so.0.0.2 file into gateway(/usr/lib directory) to be deployed and create soft-link files
    root@OK62xx:/usr/lib#ln -s libell.so.0.0.2 libell.so.0
    root@OK62xx:/usr/lib#ln -s libell.so.0.0.2 libell.so
