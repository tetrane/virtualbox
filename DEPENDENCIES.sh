#!/bin/bash

# reven-virtualbox
STRETCH_DEPENDENCIES="$STRETCH_DEPENDENCIES
libqt5opengl5
libqt5x11extras5
procps
psmisc
kmod
"

function echo_error() {
    echo -ne '\033[0;31m'
    echo $@
    echo -ne '\033[0m'
}

function run() {
    $@ >/tmp/reven_dependencies.log 2>&1
    exit_code=$?
    if [ "$exit_code" != "0" ]; then
        echo "======================= BEGIN APT LOG ======================="
        cat /tmp/reven_dependencies.log
        echo "=======================  END APT LOG  ======================="
        echo_error -e "\nThere was a problem installing dependencies. Please check-out the logs above."
        exit $exit_code
    fi
}

HEADERS_PACKAGE="linux-headers-$(uname -r)"
echo "Updating package list"
run apt update
echo "Installing the Linux headers for VirtualBox"
apt install -y "$HEADERS_PACKAGE" >/dev/null 2>&1
exit_code=$?
if [ "$exit_code" != "0" ]; then
    echo -ne '\033[0;33m'
    echo "Unable to install '$HEADERS_PACKAGE'. Please install the correct headers for your running Linux version, then try again."
    echo -ne '\033[0m'
fi


if [ -z "${MAIN_SCRIPT+x}" ]; then
    echo "Updating package list"
    run apt update
    echo "Installing dependencies. This may take a while."
    DEBIAN_VERSION=$(lsb_release -sc)
    if [ "$DEBIAN_VERSION" = "stretch" ]; then
        run apt install -y $STRETCH_DEPENDENCIES
    elif [ "$DEBIAN_VERSION" = "buster" ]; then
        run apt install -y $BUSTER_DEPENDENCIES
    fi
    export MAIN_SCRIPT=1
fi

