/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVISTDRIVE_APP_CONFIG_H
#define EVISTDRIVE_APP_CONFIG_H

/* Product identity */
#define EVISTDRIVE_HMI_NAME "EVistDrive HMI"
/* Release number of this application. */
#define EVISTDRIVE_HMI_VERSION "10.23"

/* Production feature set. Override with -D for diagnostic builds. */
#ifndef EVIST_ENABLE_CAN_TX
#define EVIST_ENABLE_CAN_TX 1
#endif
#ifndef EVIST_ENABLE_SERVICE_TX
#define EVIST_ENABLE_SERVICE_TX 1
#endif
#ifndef EVIST_ENABLE_PERSISTENCE
#define EVIST_ENABLE_PERSISTENCE 1
#endif
#ifndef EVIST_ENABLE_BLE_TX
#define EVIST_ENABLE_BLE_TX 1
#endif
/* WHEEL/ASSIST and phone P0/P1 writes (verified by controller readback). */
#ifndef EVIST_ENABLE_CONFIG_WRITES
#define EVIST_ENABLE_CONFIG_WRITES 1
#endif
/* Enable the RIDE MODE 0x3203 speed write. */
#ifndef EVIST_ENABLE_RIDE_SPEED_WRITE
#define EVIST_ENABLE_RIDE_SPEED_WRITE 1
#endif

/* Safety policy: walk assist is valid only while DOWN remains physically held. */
#define EVIST_WALK_DEADMAN 1

/* Build profiles.
 *   PRODUCT         normal application.
 *   RECOVERY_FAULT  preflight recovery, then a deliberate stop before LCD/UI;
 *                   simulates a hung application with a valid marker.
 *   PROBE           one-shot diagnostic image: clears its own marker first,
 *                   then shows read-only hardware information. */
#define EVIST_PROFILE_PRODUCT        0
#define EVIST_PROFILE_RECOVERY_FAULT 1
#define EVIST_PROFILE_PROBE          2
#ifndef EVIST_BUILD_PROFILE
#define EVIST_BUILD_PROFILE EVIST_PROFILE_PRODUCT
#endif

/* Length of the pre-runtime recovery window. Updaters repeat the CAN request
 * every ~60 ms, so a short window suffices for CAN; qualification images use a
 * long window so a phone can connect over BLE. */
#ifndef EVIST_PREFLIGHT_WINDOW_MS
#if EVIST_BUILD_PROFILE == EVIST_PROFILE_PRODUCT
#define EVIST_PREFLIGHT_WINDOW_MS 1000u
#elif EVIST_BUILD_PROFILE == EVIST_PROFILE_PROBE
/* Marker already cleared; the probe screen keeps serving requests. */
#define EVIST_PREFLIGHT_WINDOW_MS 0u
#else
#define EVIST_PREFLIGHT_WINDOW_MS 10000u
#endif
#endif

/* Source identity, passed in by the Makefile (-DEVIST_BUILD_GIT). */
#ifndef EVIST_BUILD_GIT
#define EVIST_BUILD_GIT "UNKNOWN"
#endif

#endif
