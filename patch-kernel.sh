#!/bin/bash

MORE=1
INSTALL=0
RUN_MENUCONFIG=0
WORKDIR="$HOME/bt-kernel-patch"

if [[ "$@" == *"--install"* ]]; then
    INSTALL=1
fi

if [[ "$@" == *"--configure"* ]]; then
    RUN_MENUCONFIG=1
fi

defconfig=""
generic_defconfig=0
is_pi=0


if ! which apt &> /dev/null; then
    echo " - Error: This script works only with Debian-based distros, at least for now." 1>&2
    exit 1
fi

if ! grep -q "^deb-src" /etc/apt/sources.list /etc/apt/sources.list.d/*; then
    echo " - Error: Source repositories are disabled or not configured in your /etc/apt/sources.list. You need to enable them for this script to work." 1>&2
    exit 1
fi

if [[ $MORE > 1 ]]; then
    echo " - Warning: You set \$MORE to $MORE, which is greater than 1, and this is unsafe. I recommend you to set it to 0 or 1." 1>&2
fi


JOBS=$(($(nproc --all) + $MORE))

abort() {
    echo " - Aborting." 1>&2
    exit 2
}

run() {
    echo " - Running $*" 1>&2
    $*
}

copy_with_backup() {
    source="$1"
    dest="$2"
    if [[ $3 == 1 ]]; then
        maybe_sudo="sudo"
    else
        maybe_sudo=""
    fi


    if [[ ! -e "$source" ]]; then
        return 1
    fi

    if [ -e "$dest" ]; then
        if [ -e "$dest.old" ]; then
            counter=1
            while [ -e "$dest.old.$counter" ]; do
                counter=$((counter + 1))
            done
            $maybe_sudo mv "$dest" "$dest.old.$counter"
        else
            $maybe_sudo mv "$dest" "$dest.old"
        fi
    fi

    $maybe_sudo cp "$source" "$dest"
}

if [ -f /proc/device-tree/model ]; then
    read -r -d '' model < /proc/device-tree/model

    case $model in
        "Raspberry Pi 4"*)
            defconfig="bcm2711"
            is_pi=1
            ;;
        "Raspberry Pi 3"*)
            defconfig="bcmrpi3"
            is_pi=1
            ;;
        "Raspberry Pi 2"*)
            defconfig="bcm2709"
            is_pi=1
            ;;
        "Raspberry Pi Zero"* | "Raspberry Pi 1"*)
            defconfig="bcmrpi"
            is_pi=1
            ;;
        *)
            if [ -f "/proc/cpuinfo" ]; then
                tempconfig=$(cat /proc/cpuinfo | grep "Hardware" | awk '{print tolower($3)}')
                if [ -n $tempconfig ]; then
                    defconfig=$tempconfig
                else
                    defconfig=$(uname -m)
                    generic_defconfig=1
                fi
            else
                defconfig=$(uname -m)
                generic_defconfig=1
            fi
            ;;
    esac
else
    defconfig=$(uname -m)
    generic_defconfig=1
fi

if [ -e $WORKDIR ]; then
    rm -r $WORKDIR
fi

mkdir $WORKDIR
cd $WORKDIR

echo " - Preparing the system to compile the kernel..."
sudo apt update
sudo apt install build-essential libncurses-dev bison flex libssl-dev xz-utils libelf-dev fakeroot patch || abort

MAKE="make -j$JOBS"

echo " - Downloading the kernel source package..."
if [[ $is_pi == 1 ]]; then
    apt source raspberrypi-firmware || abort
    cd raspberrypi-firmware-*/linux
else
    apt source linux-source || abort
    cd linux-*
fi


if [[ $generic_defconfig == 1 ]]; then
    echo " - Warning: Couldn't get information for defconfig. Falling back to the generic defconfig." 1>&2
fi

echo " - Defconfig: $defconfig"

PATCH=$(cat <<EOF
diff --git a/net/bluetooth/l2cap_sock.c b/net/bluetooth/l2cap_sock.c
index eebe25610..64db1db3f 100644
--- a/net/bluetooth/l2cap_sock.c
+++ b/net/bluetooth/l2cap_sock.c
@@ -1825,7 +1825,7 @@ static void l2cap_sock_init(struct sock *sk, struct sock *parent)
                        break;
                }
 
-               chan->imtu = L2CAP_DEFAULT_MTU;
+               chan->imtu = 0;
                chan->omtu = 0;
                if (!disable_ertm && sk->sk_type == SOCK_STREAM) {
                        chan->mode = L2CAP_MODE_ERTM;
EOF
)

if echo $PATCH | patch -sfp0 --ignore-whitespace --dry-run net/bluetooth/l2cap_sock.c &>/dev/null; then
    if echo $PATCH | patch -s --ignore-whitespace net/bluetooth/l2cap_sock.c &>/dev/null; then
        echo " - Applied patch to net/bluetooth/l2cap_sock.c."
    else
        echo " - Could not apply patch to net/bluetooth/l2cap_sock.c. Aborting." 1>&2
        exit 1
    fi
else
    echo " - Patch already applied."
fi

run $MAKE clean
run $MAKE mrproper
run $MAKE ${defconfig}_defconfig || run $MAKE defconfig
if [[ $RUN_MENUCONFIG == 1 ]]; then
    run $MAKE menuconfig
fi

run $MAKE || abort

if [[ $is_pi == 1 ]]; then
    cd ..
    run fakeroot debian/rules -j$JOBS binary || abort
    cd -
fi


if [[ -e "/boot/kernel8.img" ]]; then
    kn=8
else if [[ -e "/boot/kernel7.img" ]]; then
    kn=7
fi

case $(uname -m) in
    aarch64) arch="arm64" ;;
    armv7l) arch="arm" ;;
    i386|x86_64|amd64) arch="x86" ;;
    *) arch="<INSERT_ARCH_HERE>" ;;
esac

if [[ $INSTALL == 1 ]]; then
    if [[ $is_pi == 1 ]]; then
        run sudo dpkg -i ../../*.deb || abort
        if [[ arch == "<INSERT_ARCH_HERE>" ]]; then
            echo " - Error: Could not detect the architecture ($(uname -m)) - you have to copy the image to /boot yourself." 1>&2
            exit 1
        fi

        echo -n " - Copying the Linux image to /boot..."
        copy_with_backup arch/$arch/boot/Image /boot/kernel$kn.img 1 || abort
        echo "done.\n - To restore the original kernel, copy /boot/kernel$kn.img.old back to /boot/kernel$kn.img"
    else
        run sudo $MAKE install || abort
    fi
    echo " - Done, the patched kernel has been successfully installed!"
    echo " - Reboot to run the new kernel."
else
    echo -n " - Successfully built the patched kernel. To install it, just "
    if [[ $is_pi == 1 ]]; then
        echo "copy $(readlink -f arch/$arch/boot/Image) to /boot/kernel$kn.img."
    else
        echo "run \"sudo $MAKE install\" inside the directory $WORKDIR."
    fi
fi