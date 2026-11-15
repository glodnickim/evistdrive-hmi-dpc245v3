# SPDX-License-Identifier: GPL-3.0-or-later
# Build settings of the DPC245 V2 target.
BOARD_LABEL := DPC245_V2

# V2 settings-memory persistence is ENABLED. The PROBE image read the SPI NOR
# JEDEC id on the device as 85 60 15, byte-identical to V3, so the shared SPI
# driver and persistence behave exactly as on the V3 part. V2 differs from V3
# only in board_config.h.
BOARD_DEFS :=
