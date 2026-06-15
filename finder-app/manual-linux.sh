#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # 'deep clean' the kernel build tree - removing the .config file with any existing configurations
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper

    # configure for virt ARM dev board to simulate in QEMU
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig

    # build a kernel image for booting with QEMU
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all

    # build a kernel modules
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

    # build the devicetree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
    # --- END TODO: Add your kernel build steps here ---
fi

echo "Adding the Image in outdir"
cp -v ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

# next step - creating a folder tree
# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
mkdir -p ${OUTDIR}/rootfs/bin ${OUTDIR}/rootfs/dev ${OUTDIR}/rootfs/etc ${OUTDIR}/rootfs/home ${OUTDIR}/rootfs/lib ${OUTDIR}/rootfs/lib64 ${OUTDIR}/rootfs/proc ${OUTDIR}/rootfs/sbin ${OUTDIR}/rootfs/sys ${OUTDIR}/rootfs/tmp ${OUTDIR}/rootfs/usr ${OUTDIR}/rootfs/var
mkdir -p ${OUTDIR}/rootfs/usr/lib ${OUTDIR}/rootfs/usr/lib64 ${OUTDIR}/rootfs/usr/bin ${OUTDIR}/rootfs/usr/sbin
mkdir -p ${OUTDIR}/rootfs/var/log
# --- END TODO: Create necessary base directories ---

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    # to make manual configuration
    #make menuconfig
    # use defconfig option to simplify and create predefined default configuration
    make defconfig
    # --- END TODO:  Configure busybox ---
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- install
# --- END TODO: Make and install busybox ---

cd "$OUTDIR"
# check library dependencies for our application (writer)
echo "Library dependencies"
${CROSS_COMPILE}readelf -a rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp -v ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
cp -v ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp -v ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp -v ${SYSROOT}/usr/lib64/libc.so ${OUTDIR}/rootfs/usr/lib64
cp -v ${SYSROOT}/usr/lib64/libm.so  ${OUTDIR}/rootfs/usr/lib64
cp -v ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp -v ${SYSROOT}/usr/bin/ld.so ${OUTDIR}/rootfs/usr/bin
# static libraries
#cp -v ${SYSROOT}/usr/lib64/libc.a ${OUTDIR}/rootfs/usr/lib64
#cp -v ${SYSROOT}/usr/lib64/libresolv.a ${OUTDIR}/rootfs/usr/lib64


# TODO: Make device nodes
cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd "$FINDER_APP_DIR"
make CROSS_COMPILE= clean
make CROSS_COMPILE=aarch64-none-linux-gnu- all

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
${CROSS_COMPILE}readelf -a writer | grep "program interpreter"
${CROSS_COMPILE}readelf -a writer | grep "Shared library"
cp -v writer ${OUTDIR}/rootfs/home
cp -v finder.sh ${OUTDIR}/rootfs/home
cp -v finder-test.sh ${OUTDIR}/rootfs/home
cp -v writer.sh ${OUTDIR}/rootfs/home
cp -R -v -L ./conf ${OUTDIR}/rootfs/home
cp -v autorun-qemu.sh ${OUTDIR}/rootfs/home
# --- END TODO: Copy the finder related scripts and executables ---

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
#cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
