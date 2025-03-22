-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA512

Format: 1.0
Source: linux
Binary: linux-headers-6.8.0-55, linux-tools-6.8.0-55, linux-cloud-tools-6.8.0-55, linux-libc-dev, linux-tools-common, linux-cloud-tools-common, linux-tools-host, linux-source-6.8.0, linux-doc, linux-bpf-dev, linux-image-unsigned-6.8.0-55-generic, linux-modules-6.8.0-55-generic, linux-modules-extra-6.8.0-55-generic, linux-headers-6.8.0-55-generic, linux-lib-rust-6.8.0-55-generic, linux-image-unsigned-6.8.0-55-generic-dbgsym, linux-tools-6.8.0-55-generic, linux-cloud-tools-6.8.0-55-generic, linux-buildinfo-6.8.0-55-generic, linux-modules-ipu6-6.8.0-55-generic, linux-modules-iwlwifi-6.8.0-55-generic, linux-modules-usbio-6.8.0-55-generic, linux-image-unsigned-6.8.0-55-generic-64k, linux-modules-6.8.0-55-generic-64k, linux-modules-extra-6.8.0-55-generic-64k, linux-headers-6.8.0-55-generic-64k, linux-lib-rust-6.8.0-55-generic-64k, linux-image-unsigned-6.8.0-55-generic-64k-dbgsym, linux-tools-6.8.0-55-generic-64k, linux-cloud-tools-6.8.0-55-generic-64k,
 linux-buildinfo-6.8.0-55-generic-64k, linux-modules-ipu6-6.8.0-55-generic-64k, linux-modules-iwlwifi-6.8.0-55-generic-64k, linux-modules-usbio-6.8.0-55-generic-64k, linux-image-unsigned-6.8.0-55-generic-lpae, linux-modules-6.8.0-55-generic-lpae, linux-modules-extra-6.8.0-55-generic-lpae, linux-headers-6.8.0-55-generic-lpae, linux-lib-rust-6.8.0-55-generic-lpae, linux-image-unsigned-6.8.0-55-generic-lpae-dbgsym, linux-tools-6.8.0-55-generic-lpae, linux-cloud-tools-6.8.0-55-generic-lpae, linux-buildinfo-6.8.0-55-generic-lpae, linux-modules-ipu6-6.8.0-55-generic-lpae, linux-modules-iwlwifi-6.8.0-55-generic-lpae,
 linux-modules-usbio-6.8.0-55-generic-lpae
Architecture: all amd64 armhf arm64 ppc64el s390x i386 riscv64
Version: 6.8.0-55.57
Maintainer: Ubuntu Kernel Team <kernel-team@lists.ubuntu.com>
Standards-Version: 3.9.4.0
Vcs-Git: git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/noble
Testsuite: autopkgtest
Testsuite-Triggers: @builddeps@, build-essential, fakeroot, gcc-multilib, gdb, git, python3
Build-Depends: gcc-13, gcc-13-aarch64-linux-gnu [arm64] <cross>, gcc-13-arm-linux-gnueabihf [armhf] <cross>, gcc-13-powerpc64le-linux-gnu [ppc64el] <cross>, gcc-13-riscv64-linux-gnu [riscv64] <cross>, gcc-13-s390x-linux-gnu [s390x] <cross>, gcc-13-x86-64-linux-gnu [amd64] <cross>, autoconf <!stage1>, automake <!stage1>, bc <!stage1>, bindgen-0.65 [amd64 arm64 armhf ppc64el riscv64 s390x], bison <!stage1>, clang-18 [amd64 arm64 armhf ppc64el riscv64 s390x], cpio, curl <!stage1>, debhelper-compat (= 10), default-jdk-headless <!stage1>, dkms <!stage1>, dwarfdump <!stage1>, flex <!stage1>, gawk <!stage1>, java-common <!stage1>, kmod <!stage1>, libaudit-dev <!stage1>, libcap-dev <!stage1>, libdw-dev <!stage1>, libelf-dev <!stage1>, libiberty-dev <!stage1>, liblzma-dev <!stage1>, libnewt-dev <!stage1>, libnuma-dev [amd64 arm64 ppc64el s390x] <!stage1>, libpci-dev <!stage1>, libssl-dev <!stage1>, libstdc++-dev, libtool <!stage1>, libtraceevent-dev [amd64 arm64 armhf ppc64el s390x riscv64] <!stage1>, libtracefs-dev [amd64 arm64 armhf ppc64el s390x riscv64] <!stage1>, libudev-dev <!stage1>, libunwind8-dev [amd64 arm64 armhf ppc64el] <!stage1>, makedumpfile [amd64] <!stage1>, openssl <!stage1>, pahole [amd64 arm64 armhf ppc64el s390x riscv64] | dwarves (>= 1.21) [amd64 arm64 armhf ppc64el s390x riscv64] <!stage1>, pkg-config <!stage1>, python3 <!stage1>, python3-dev <!stage1>, python3-setuptools <!stage1>, rsync [!i386] <!stage1>, rust-src [amd64 arm64 armhf ppc64el riscv64 s390x], rustc [amd64 arm64 armhf ppc64el riscv64 s390x], rustfmt [amd64 arm64 armhf ppc64el riscv64 s390x], uuid-dev <!stage1>, zstd <!stage1>
Build-Depends-Indep: asciidoc <!stage1>, bzip2 <!stage1>, python3-docutils <!stage1>, sharutils <!stage1>, xmlto <!stage1>
Package-List:
 linux-bpf-dev deb devel optional arch=amd64,armhf,arm64,i386,ppc64el,riscv64,s390x
 linux-buildinfo-6.8.0-55-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-buildinfo-6.8.0-55-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-buildinfo-6.8.0-55-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-cloud-tools-6.8.0-55 deb devel optional arch=amd64,armhf profile=!stage1
 linux-cloud-tools-6.8.0-55-generic deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-cloud-tools-6.8.0-55-generic-64k deb devel optional arch=arm64 profile=!stage1
 linux-cloud-tools-6.8.0-55-generic-lpae deb devel optional arch=armhf profile=!stage1
 linux-cloud-tools-common deb kernel optional arch=all profile=!stage1
 linux-doc deb doc optional arch=all profile=!stage1
 linux-headers-6.8.0-55 deb devel optional arch=all profile=!stage1
 linux-headers-6.8.0-55-generic deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-headers-6.8.0-55-generic-64k deb devel optional arch=arm64 profile=!stage1
 linux-headers-6.8.0-55-generic-lpae deb devel optional arch=armhf profile=!stage1
 linux-image-unsigned-6.8.0-55-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-image-unsigned-6.8.0-55-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-image-unsigned-6.8.0-55-generic-64k-dbgsym deb devel optional arch=arm64 profile=!stage1
 linux-image-unsigned-6.8.0-55-generic-dbgsym deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-image-unsigned-6.8.0-55-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-image-unsigned-6.8.0-55-generic-lpae-dbgsym deb devel optional arch=armhf profile=!stage1
 linux-lib-rust-6.8.0-55-generic deb devel optional arch=amd64 profile=!stage1
 linux-lib-rust-6.8.0-55-generic-64k deb devel optional arch=amd64 profile=!stage1
 linux-lib-rust-6.8.0-55-generic-lpae deb devel optional arch=amd64 profile=!stage1
 linux-libc-dev deb devel optional arch=amd64,armhf,arm64,i386,ppc64el,riscv64,s390x
 linux-modules-6.8.0-55-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-6.8.0-55-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-6.8.0-55-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-extra-6.8.0-55-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-extra-6.8.0-55-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-extra-6.8.0-55-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-ipu6-6.8.0-55-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-ipu6-6.8.0-55-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-ipu6-6.8.0-55-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-iwlwifi-6.8.0-55-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-iwlwifi-6.8.0-55-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-iwlwifi-6.8.0-55-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-usbio-6.8.0-55-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-usbio-6.8.0-55-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-usbio-6.8.0-55-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-source-6.8.0 deb devel optional arch=all profile=!stage1
 linux-tools-6.8.0-55 deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-tools-6.8.0-55-generic deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-tools-6.8.0-55-generic-64k deb devel optional arch=arm64 profile=!stage1
 linux-tools-6.8.0-55-generic-lpae deb devel optional arch=armhf profile=!stage1
 linux-tools-common deb kernel optional arch=all profile=!stage1
 linux-tools-host deb kernel optional arch=all profile=!stage1
Checksums-Sha1:
 27dce9d91afd9433620628fde58a686f5e53cebb 230060117 linux_6.8.0.orig.tar.gz
 0c59f05f3024a1873724700e60bd9e580fff32c8 4213273 linux_6.8.0-55.57.diff.gz
Checksums-Sha256:
 26512115972bdf017a4ac826cc7d3e9b0ba397d4f85cd330e4e4ff54c78061c8 230060117 linux_6.8.0.orig.tar.gz
 66fdeed1490d20163acf7215400dfdb530056d4e30aa597565ecc5f7a54c61b2 4213273 linux_6.8.0-55.57.diff.gz
Files:
 3ce3b99b065c7c0507b82ad0f330e2fd 230060117 linux_6.8.0.orig.tar.gz
 4e65e7f206e8238d86b9f767b7dbd859 4213273 linux_6.8.0-55.57.diff.gz
Ubuntu-Compatible-Signing: ubuntu/4 pro/3

-----BEGIN PGP SIGNATURE-----

iQJRBAEBCgA7FiEEfiwIK8Bxx+qw8pSyuvL3+R/lFUsFAmetHIEdHG1hbnVlbC5k
aWV3YWxkQGNhbm9uaWNhbC5jb20ACgkQuvL3+R/lFUt/3A/6A7AtxcHJQtqsb17R
k3XcIazV1qwTDxRjCuhVDLNgvcebYnp1N+qytXPbeS+gOSRy+Uh9NrBh3VQbxo2o
SoURxx6beQsVRLJ92tPw234Hh0Iy2k1BhDR/sWegq9fE+nG7n/7g1tJor+hd0g2o
SoNnAc6ortCDnkU7VEuufQt4igdPujvnzfWh8NnopxepSU64GxqZKDyT3CKDM8UB
v4wkSuQsJnqdrb5IEMzCWEIaM8L+rOsaiY8MTqx+OTlg4/JvvmuaBgkfX9DiCH5b
05kTI87WqCl8WyUt/VaDZ4EPjSdHLr9HnYCQtCtDnIY7kHXbVEgTVFtsy/JelZyg
GgFS5DOqZOMIt8JSKrDIGG/mEzJfMFYc8EBDxXJmRzpcHnvjWNGH831X0ccS9ebj
aJa1tYQXq2ahId/lIPTBNUcABT1bSLgp1GERCDWSPKoCK10rsqedzOJe57Hi536a
5MRpNUVpack7NCBsJ6fiw49OmVXcFI8CiM6uEKKnaJsE79vs2dOF6L4JhmmFm3xt
h0QKcgPoU6SblezcwulE9v4GIsv2MZiIqhT01LQzDUFCO/k/pKXjrejO5T+L8Tel
j/wH9TL9O5qnw1mdJKA00IwmT54zL7LjVQWRTvuwF/xHIZHraDcs8zVUdpx+llXk
V2Lt3JpoXzzQoCkEYoJ+Nh0UHxU=
=umcG
-----END PGP SIGNATURE-----
