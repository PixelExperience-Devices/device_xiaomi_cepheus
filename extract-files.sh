#!/bin/bash
#
# Copyright (C) 2019 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

DEVICE=cepheus
VENDOR=xiaomi

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$MY_DIR" ]]; then MY_DIR="$PWD"; fi

AOSP_ROOT="$MY_DIR"/../../..

HELPER="$AOSP_ROOT"/vendor/aosp/build/tools/extract_utils.sh
if [ ! -f "$HELPER" ]; then
    echo "Unable to find helper script at $HELPER"
    exit 1
fi
. "$HELPER"

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

while [ "$1" != "" ]; do
    case $1 in
        -n | --no-cleanup )     CLEAN_VENDOR=false
                                ;;
        -s | --section )        shift
                                SECTION=$1
                                CLEAN_VENDOR=false
                                ;;
        * )                     SRC=$1
                                ;;
    esac
    shift
done

if [ -z "$SRC" ]; then
    SRC=adb
fi

# Initialize the helper for common device
setup_vendor "$DEVICE" "$VENDOR" "$AOSP_ROOT" true "$CLEAN_VENDOR"

extract "$MY_DIR"/proprietary-files.txt "$SRC" "$SECTION"

BLOB_ROOT="$AOSP_ROOT"/vendor/"$VENDOR"/"$DEVICE"/proprietary

patchelf --remove-needed vendor.xiaomi.hardware.mtdservice@1.0.so "$BLOB_ROOT"/vendor/bin/mlipayd@1.1
patchelf --remove-needed vendor.xiaomi.hardware.mtdservice@1.0.so "$BLOB_ROOT"/vendor/lib64/libmlipay.so
patchelf --remove-needed vendor.xiaomi.hardware.mtdservice@1.0.so "$BLOB_ROOT"/vendor/lib64/libmlipay@1.1.so

"$MY_DIR"/setup-makefiles.sh
