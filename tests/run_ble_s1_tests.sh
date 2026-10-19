#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host
"${CC:-cc}" -std=c11 -DBLE_HOST_TEST=1 -O1 -g -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    tests/ble_s1_test.c firmware/ble/ble_logic.c firmware/ble/ble_controller.c firmware/ble/ble_link_logic.c firmware/ble/ble_link.c \
    -o build/host/ble_s1_test
build/host/ble_s1_test

"${CC:-cc}" -std=c11 -DBLE_HOST_TEST=1 -DFELUCCA_BLE_TX_DBM=-20 -O1 -g -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    tests/ble_s1_test.c firmware/ble/ble_logic.c firmware/ble/ble_controller.c firmware/ble/ble_link_logic.c firmware/ble/ble_link.c \
    -o build/host/ble_s1_measured_power_test
build/host/ble_s1_measured_power_test
