#!/bin/bash

MORE=0
INSTALL=0
RUN_MENUCONFIG=0
DEB_BUILD=1
WORKDIR="$HOME/bluetooth/kernel-patch"
PATCH="$PWD/kernel.patch"

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

if [[ "$MORE" -gt 1 ]]; then
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

backup() {
    dest="$1"
    if [[ $2 == 1 ]]; then
        maybe_sudo="sudo"
    else
        maybe_sudo=""
    fi

    if [[ ! -e "$dest" ]]; then return 1; fi

    if [ -e "$dest.old" ]; then
        counter=1
        while [ -e "$dest.old.$counter" ]; do
            counter=$((counter + 1))
        done
        $maybe_sudo mv "$dest" "$dest.old.$counter"
    else
        $maybe_sudo mv "$dest" "$dest.old"
    fi
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
        backup "$dest" $3
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
                if [ -n "$tempconfig" ]; then
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

if [ -e "$WORKDIR" ]; then
    rm -r "$WORKDIR"
fi

mkdir "$WORKDIR" || abort
cd "$WORKDIR" || abort

echo " - Preparing the system to compile the kernel..."
sudo apt update
sudo apt install build-essential libncurses-dev bison flex libssl-dev xz-utils libelf-dev fakeroot patch devscripts || abort

MAKE="make -j$JOBS"

echo " - Downloading the kernel source package..."
apt source linux || abort

if [[ "$DEB_BUILD" != 1 ]]; then
    if [[ "$generic_defconfig" == 1 ]]; then
        echo " - Warning: Couldn't get information for defconfig. Falling back to the generic defconfig." 1>&2
    fi

    echo " - Defconfig: $defconfig"
fi

cd linux-*

if [[ "$DEB_BUILD" == 1 ]]; then
	out=$(dpkg-checkbuilddeps 2>&1 >/dev/null || true)

	if [[ ! -z "$output" ]]; then
		depln=$(echo "$out" | sed -n 's/.*Unmet build dependencies: //p')

		if [[ ! -z "$deps_line" ]]; then
			deps=$(echo "$depln" \
			  | sed -E 's/\([^)]*\)//g' \
			  | sed -E 's/([[:alnum:]._:+-]+)\s*\|\s*[[:alnum:]._:+-]+/\1/g')

			echo " - Installing missing build dependencies."
			sudo apt install -y $deps_clean
		fi
	fi
fi

if patch -sfp0 --ignore-whitespace --dry-run net/bluetooth/l2cap_sock.c < "$PATCH" &>/dev/null; then
    if patch -s --ignore-whitespace net/bluetooth/l2cap_sock.c < "$PATCH" &>/dev/null; then
        echo " - Applied patch to net/bluetooth/l2cap_sock.c."
    else
        echo " - Could not apply patch to net/bluetooth/l2cap_sock.c. Aborting." 1>&2
        exit 1
    fi
else
    echo " - Patch already applied."
fi

if [[ "$DEB_BUILD" == 1 ]]; then
    export DEB_BUILD_OPTIONS="parallel=$JOBS"
    export DEB_BUILD_PROFILES="nodoc"
    run fakeroot debian/rules binary -j$JOBS || abort
else
    run $MAKE clean
    run $MAKE mrproper
    run $MAKE ${defconfig}_defconfig || run $MAKE defconfig
    if [[ "$RUN_MENUCONFIG" == 1 ]]; then
        run $MAKE menuconfig
    fi
    if [[ "$is_pi" == 1 ]]; then
        run $MAKE Image.gz modules dtbs || abort
    else
        run $MAKE || abort
    fi
fi

if [[ -e "/boot/firmware/kernel8.img" ]]; then
    kn=8
elif [[ -e "/boot/firmware/kernel7.img" ]]; then
    kn=7
fi

case $(uname -m) in
    aarch64) arch="arm64" ;;
    armv7l) arch="arm" ;;
    i386|x86_64|amd64) arch="x86" ;;
    *) arch="<INSERT_ARCH_HERE>" ;;
esac

if [[ "$INSTALL" == 1 ]]; then
    if [[ "$DEB_BUILD" == 1 ]]; then
        run sudo dpkg -i ../*.deb || abort
    else
        run sudo $MAKE modules_install || abort
        if [[ "$is_pi" == 1 ]]; then
            if [[ "$arch" == "<INSERT_ARCH_HERE>" ]]; then
                echo " - Error: Could not detect the architecture ($(uname -m)) - you have to copy the image to /boot yourself." 1>&2
                exit 1
            fi

            echo -n " - Copying the Linux image to /boot..."
            copy_with_backup arch/$arch/boot/Image /boot/firmware/kernel$kn.img 1 || abort
            backup /boot/firmware/overlays 1
            run sudo mkdir /boot/firmware/overlays
            run sudo cp arch/arm64/boot/dts/broadcom/*.dtb /boot/firmware/
            run sudo cp arch/arm64/boot/dts/overlays/*.dtb* /boot/firmware/overlays/
            run sudo cp arch/arm64/boot/dts/overlays/README /boot/firmware/overlays/
            echo "done.\n - To restore the original kernel, copy /boot/firmware/kernel$kn.img.old back to /boot/firmware/kernel$kn.img"
        else
            run sudo $MAKE install || abort
        fi
    fi
    echo " - Done, the patched kernel has been successfully installed!"
    echo " - Reboot to run the new kernel."
else
    echo -n " - Successfully built the patched kernel. To install it, just "
    if [[ "$DEB_BUILD" == 1 ]]; then
        echo "install the .deb packages inside the $WORKDIR directory:"
        echo " - sudo apt install $WORKDIR/*.deb"
    elif [[ "$is_pi" == 1 ]]; then
        echo "copy $(readlink -f arch/$arch/boot/Image) to /boot/firmware/kernel$kn.img."
    else
        echo "run \"sudo $MAKE install\" inside the directory $WORKDIR."
    fi
fi
