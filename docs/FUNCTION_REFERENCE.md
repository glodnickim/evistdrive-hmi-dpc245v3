# Complete Function Reference

This file is generated from the current source tree and is part of the release gate. Every C function definition, including static helpers, interrupt handlers and freestanding runtime functions, is listed here.

- Function definitions: **722**
- Source files: **53**

## `platform/dpc245/bxcan_raw.c`

Low-level CAN FIFO receive, acceptance filter and transmit mailbox driver.

Functions: **4**

### `dpc245_can_filter_accept_all`

- Location: `platform/dpc245/bxcan_raw.c:22`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Public function of the bxcan_raw module.
- Direct callers: —
- Direct callees: —

### `dpc245_can_receive_poll`

- Location: `platform/dpc245/bxcan_raw.c:28`
- Linkage: `global`
- Type: `_Bool (bafang_can_frame_t *)`
- Responsibility: Receive operation in the bxcan_raw module.
- Direct callers: `dpc245_preflight_poll`, `main`
- Direct callees: —

### `dpc245_can_transmit`

- Location: `platform/dpc245/bxcan_raw.c:49`
- Linkage: `global`
- Type: `_Bool (const bafang_can_frame_t *)`
- Responsibility: Transmit operation in the bxcan_raw module.
- Direct callers: `controller_config_service`, `handoff_after_ble_request`, `main`, `send_normal`
- Direct callees: —

### `dpc245_can_tx_idle`

- Location: `platform/dpc245/bxcan_raw.c:70`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Public function of the bxcan_raw module.
- Direct callers: `controller_config_service`, `handoff_after_ble_request`, `main`, `send_service_ack`
- Direct callees: —

## `platform/dpc245/dpc245_adc.c`

ADC initialization, ALS/Button Signal sampling and filtering.

Functions: **8**

### `short_delay`

- Location: `platform/dpc245/dpc245_adc.c:19`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_adc module.
- Direct callers: `dpc245_adc_init`
- Direct callees: —

### `dpc245_adc_init`

- Location: `platform/dpc245/dpc245_adc.c:23`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Initialization in the dpc245_adc module.
- Direct callers: `dpc245_probe_run`, `main`
- Direct callees: `short_delay`

### `dpc245_adc_read_channel`

- Location: `platform/dpc245/dpc245_adc.c:56`
- Linkage: `global`
- Type: `uint16_t (uint8_t)`
- Responsibility: Read operation in the dpc245_adc module.
- Direct callers: `dpc245_adc_sample32`, `dpc245_adc_window_filter_tick`
- Direct callees: —

### `dpc245_adc_sample32`

- Location: `platform/dpc245/dpc245_adc.c:68`
- Linkage: `global`
- Type: `void (dpc245_adc_snapshot_t *)`
- Responsibility: Public function of the dpc245_adc module.
- Direct callers: `dpc245_probe_run`, `main`
- Direct callees: `dpc245_adc_read_channel`

### `dpc245_adc_window_filter_init`

- Location: `platform/dpc245/dpc245_adc.c:100`
- Linkage: `global`
- Type: `void (dpc245_adc_window_filter_t *)`
- Responsibility: Initialization in the dpc245_adc module.
- Direct callers: `dpc245_adc_window_filter_tick`, `main`
- Direct callees: —

### `dpc245_adc_window_filter_tick`

- Location: `platform/dpc245/dpc245_adc.c:109`
- Linkage: `global`
- Type: `_Bool (dpc245_adc_window_filter_t *, dpc245_adc_snapshot_t *)`
- Responsibility: Periodic step in the dpc245_adc module.
- Direct callers: `main`
- Direct callees: `dpc245_adc_read_channel`, `dpc245_adc_window_filter_init`

### `dpc245_als_is_dark`

- Location: `platform/dpc245/dpc245_adc.c:145`
- Linkage: `global`
- Type: `_Bool (uint16_t, uint8_t)`
- Responsibility: Public function of the dpc245_adc module.
- Direct callers: —
- Direct callees: —

### `dpc245_button_signal_filter`

- Location: `platform/dpc245/dpc245_adc.c:150`
- Linkage: `global`
- Type: `_Bool (uint16_t, uint16_t *)`
- Responsibility: Public function of the dpc245_adc module.
- Direct callers: `main`
- Direct callees: —

## `platform/dpc245/dpc245_backlight.c`

Pulse-position backlight controller driver.

Functions: **10**

### `pin_high`

- Location: `platform/dpc245/dpc245_backlight.c:19`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_backlight module.
- Direct callers: `dpc245_backlight_init`, `emit_forward_pulse`, `set_position_internal`
- Direct callees: —

### `pin_low`

- Location: `platform/dpc245/dpc245_backlight.c:20`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_backlight module.
- Direct callers: `emit_forward_pulse`
- Direct callees: —

### `pulse_delay`

- Location: `platform/dpc245/dpc245_backlight.c:36`
- Linkage: `static`
- Type: `void (uint32_t)`
- Responsibility: Internal helper of the dpc245_backlight module.
- Direct callers: `emit_forward_pulse`, `set_position_internal`
- Direct callees: —

### `emit_forward_pulse`

- Location: `platform/dpc245/dpc245_backlight.c:41`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_backlight module.
- Direct callers: `set_position_internal`
- Direct callees: `pin_high`, `pin_low`, `pulse_delay`

### `set_position_internal`

- Location: `platform/dpc245/dpc245_backlight.c:48`
- Linkage: `static`
- Type: `void (uint8_t)`
- Responsibility: Internal helper of the dpc245_backlight module.
- Direct callers: `dpc245_backlight_set_level`, `dpc245_backlight_set_position`
- Direct callees: `dpc245_backlight_forward_steps`, `emit_forward_pulse`, `pin_high`, `pulse_delay`

### `dpc245_backlight_init`

- Location: `platform/dpc245/dpc245_backlight.c:72`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Initialization in the dpc245_backlight module.
- Direct callers: `dpc245_probe_run`, `main`
- Direct callees: `pin_high`

### `dpc245_backlight_set_level`

- Location: `platform/dpc245/dpc245_backlight.c:85`
- Linkage: `global`
- Type: `void (uint8_t)`
- Responsibility: Setter in the dpc245_backlight module.
- Direct callers: `dpc245_probe_run`, `main`
- Direct callees: `dpc245_backlight_level_to_position`, `set_position_internal`

### `dpc245_backlight_set_position`

- Location: `platform/dpc245/dpc245_backlight.c:91`
- Linkage: `global`
- Type: `void (uint8_t)`
- Responsibility: Setter in the dpc245_backlight module.
- Direct callers: —
- Direct callees: `set_position_internal`

### `dpc245_backlight_get_level`

- Location: `platform/dpc245/dpc245_backlight.c:95`
- Linkage: `global`
- Type: `uint8_t (void)`
- Responsibility: Getter in the dpc245_backlight module.
- Direct callers: —
- Direct callees: —

### `dpc245_backlight_get_position`

- Location: `platform/dpc245/dpc245_backlight.c:96`
- Linkage: `global`
- Type: `uint8_t (void)`
- Responsibility: Getter in the dpc245_backlight module.
- Direct callers: —
- Direct callees: —

## `platform/dpc245/dpc245_ble_uart.c`

USART transport for the external BLE module.

Functions: **10**

### `queue_rx`

- Location: `platform/dpc245/dpc245_ble_uart.c:29`
- Linkage: `static`
- Type: `void (uint8_t)`
- Responsibility: Internal helper of the dpc245_ble_uart module.
- Direct callers: `trace_rx`
- Direct callees: —

### `trace_rx`

- Location: `platform/dpc245/dpc245_ble_uart.c:40`
- Linkage: `static`
- Type: `void (uint8_t)`
- Responsibility: Internal helper of the dpc245_ble_uart module.
- Direct callers: `dpc245_ble_uart_poll`
- Direct callees: `queue_rx`

### `ble_uart_init_brr`

- Location: `platform/dpc245/dpc245_ble_uart.c:52`
- Linkage: `static`
- Type: `void (uint32_t)`
- Responsibility: Initialization in the dpc245_ble_uart module.
- Direct callers: `dpc245_ble_uart_init`, `dpc245_ble_uart_init_hsi`
- Direct callees: —

### `dpc245_ble_uart_init`

- Location: `platform/dpc245/dpc245_ble_uart.c:78`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Initialization in the dpc245_ble_uart module.
- Direct callers: `dpc245_preflight_begin`
- Direct callees: `ble_uart_init_brr`

### `dpc245_ble_uart_init_hsi`

- Location: `platform/dpc245/dpc245_ble_uart.c:83`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Initialization in the dpc245_ble_uart module.
- Direct callers: `dpc245_preflight_begin`
- Direct callees: `ble_uart_init_brr`

### `dpc245_ble_uart_poll`

- Location: `platform/dpc245/dpc245_ble_uart.c:88`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Public function of the dpc245_ble_uart module.
- Direct callers: `dpc245_preflight_poll`, `main`
- Direct callees: `trace_rx`

### `dpc245_ble_uart_read_byte`

- Location: `platform/dpc245/dpc245_ble_uart.c:103`
- Linkage: `global`
- Type: `_Bool (uint8_t *)`
- Responsibility: Read operation in the dpc245_ble_uart module.
- Direct callers: `dpc245_preflight_poll`, `main`
- Direct callees: —

### `dpc245_ble_uart_get_stats`

- Location: `platform/dpc245/dpc245_ble_uart.c:110`
- Linkage: `global`
- Type: `void (dpc245_ble_uart_stats_t *)`
- Responsibility: Getter in the dpc245_ble_uart module.
- Direct callers: —
- Direct callees: —

### `dpc245_ble_uart_write`

- Location: `platform/dpc245/dpc245_ble_uart.c:115`
- Linkage: `global`
- Type: `_Bool (const uint8_t *, uint16_t, uint32_t)`
- Responsibility: Write operation in the dpc245_ble_uart module.
- Direct callers: `dpc245_ble_uart_send_device_name_command`, `main`
- Direct callees: —

### `dpc245_ble_uart_send_device_name_command`

- Location: `platform/dpc245/dpc245_ble_uart.c:128`
- Linkage: `global`
- Type: `_Bool (uint32_t)`
- Responsibility: Public function of the dpc245_ble_uart module.
- Direct callers: `main`
- Direct callees: `dpc245_ble_uart_write`

## `platform/dpc245/dpc245_board.c`

Board-level debug/JTAG and reserved-input initialization.

Functions: **3**

### `dpc245_debug_keep_swd_release_jtag`

- Location: `platform/dpc245/dpc245_board.c:12`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Public function of the dpc245_board module.
- Direct callers: `dpc245_lcd_bus_init`
- Direct callees: —

### `dpc245_unknown_pa0_init_pullup`

- Location: `platform/dpc245/dpc245_board.c:22`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Initialization in the dpc245_board module.
- Direct callers: `dpc245_hwinfo_read`
- Direct callees: —

### `dpc245_unknown_pa0_high`

- Location: `platform/dpc245/dpc245_board.c:31`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Public function of the dpc245_board module.
- Direct callers: `dpc245_hwinfo_read`
- Direct callees: —

## `platform/dpc245/dpc245_buttons.c`

Physical key GPIO sampling and debounce helpers.

Functions: **12**

### `buttons_irq_save`

- Location: `platform/dpc245/dpc245_buttons.c:30`
- Linkage: `static`
- Type: `uint32_t (void)`
- Responsibility: Save operation in the dpc245_buttons module.
- Direct callers: `dpc245_buttons_key_event`, `dpc245_buttons_key_sampler_start`, `dpc245_buttons_take_pressed`, `dpc245_buttons_take_released`
- Direct callees: —

### `buttons_irq_restore`

- Location: `platform/dpc245/dpc245_buttons.c:36`
- Linkage: `static`
- Type: `void (uint32_t)`
- Responsibility: Internal helper of the dpc245_buttons module.
- Direct callers: `dpc245_buttons_key_event`, `dpc245_buttons_key_sampler_start`, `dpc245_buttons_take_pressed`, `dpc245_buttons_take_released`
- Direct callees: —

### `cfg_input_pullup`

- Location: `platform/dpc245/dpc245_buttons.c:39`
- Linkage: `static`
- Type: `void (unsigned int)`
- Responsibility: Internal helper of the dpc245_buttons module.
- Direct callers: `dpc245_buttons_init`
- Direct callees: —

### `dpc245_buttons_init`

- Location: `platform/dpc245/dpc245_buttons.c:46`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Initialization in the dpc245_buttons module.
- Direct callers: `clock_failure_safe_mode`, `main`
- Direct callees: `cfg_input_pullup`

### `dpc245_buttons_raw`

- Location: `platform/dpc245/dpc245_buttons.c:62`
- Linkage: `global`
- Type: `uint8_t (void)`
- Responsibility: Public function of the dpc245_buttons module.
- Direct callers: `dpc245_buttons_key_sampler_start`, `dpc245_buttons_tick_1ms`, `dpc245_preflight_poll`, `draw_dynamic`
- Direct callees: —

### `sample_one`

- Location: `platform/dpc245/dpc245_buttons.c:74`
- Linkage: `static`
- Type: `void (uint8_t, uint8_t, uint8_t *)`
- Responsibility: Internal helper of the dpc245_buttons module.
- Direct callers: `dpc245_buttons_tick_1ms`
- Direct callees: —

### `dpc245_buttons_tick_1ms`

- Location: `platform/dpc245/dpc245_buttons.c:90`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Periodic step in the dpc245_buttons module.
- Direct callers: `SysTick_Handler`
- Direct callees: `dpc245_buttons_raw`, `dpc245_key_sampler_tick`, `sample_one`

### `dpc245_buttons_key_sampler_start`

- Location: `platform/dpc245/dpc245_buttons.c:98`
- Linkage: `global`
- Type: `void (dpc245_key_sampler_t *, uint32_t)`
- Responsibility: Start operation in the dpc245_buttons module.
- Direct callers: —
- Direct callees: `buttons_irq_restore`, `buttons_irq_save`, `dpc245_buttons_raw`

### `dpc245_buttons_key_event`

- Location: `platform/dpc245/dpc245_buttons.c:105`
- Linkage: `global`
- Type: `_Bool (dpc245_key_sample_event_t *)`
- Responsibility: Public function of the dpc245_buttons module.
- Direct callers: `main`
- Direct callees: `buttons_irq_restore`, `buttons_irq_save`, `dpc245_key_sampler_pop`

### `dpc245_buttons_state`

- Location: `platform/dpc245/dpc245_buttons.c:112`
- Linkage: `global`
- Type: `uint8_t (void)`
- Responsibility: Public function of the dpc245_buttons module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_buttons_take_pressed`

- Location: `platform/dpc245/dpc245_buttons.c:114`
- Linkage: `global`
- Type: `uint8_t (void)`
- Responsibility: Consume-once accessor in the dpc245_buttons module.
- Direct callers: —
- Direct callees: `buttons_irq_restore`, `buttons_irq_save`

### `dpc245_buttons_take_released`

- Location: `platform/dpc245/dpc245_buttons.c:122`
- Linkage: `global`
- Type: `uint8_t (void)`
- Responsibility: Consume-once accessor in the dpc245_buttons module.
- Direct callers: `main`
- Direct callees: `buttons_irq_restore`, `buttons_irq_save`

## `platform/dpc245/dpc245_can_init.c`

CAN peripheral clock/timing initialization.

Functions: **4**

### `wait_bit`

- Location: `platform/dpc245/dpc245_can_init.c:20`
- Linkage: `static`
- Type: `_Bool (volatile uint32_t *, uint32_t, _Bool)`
- Responsibility: Bounded wait for a hardware or state condition in the dpc245_can_init module.
- Direct callers: `can_init_btr`
- Direct callees: —

### `can_init_btr`

- Location: `platform/dpc245/dpc245_can_init.c:30`
- Linkage: `static`
- Type: `_Bool (uint32_t)`
- Responsibility: Initialization in the dpc245_can_init module.
- Direct callers: `dpc245_can_init_250k`, `dpc245_can_init_250k_hsi`
- Direct callees: `wait_bit`

### `dpc245_can_init_250k`

- Location: `platform/dpc245/dpc245_can_init.c:55`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Initialization in the dpc245_can_init module.
- Direct callers: `dpc245_preflight_begin`
- Direct callees: `can_init_btr`

### `dpc245_can_init_250k_hsi`

- Location: `platform/dpc245/dpc245_can_init.c:59`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Initialization in the dpc245_can_init module.
- Direct callers: `dpc245_preflight_begin`
- Direct callees: `can_init_btr`

## `platform/dpc245/dpc245_clock.c`

120 MHz system clock and bus-clock initialization.

Functions: **4**

### `wait_mask`

- Location: `platform/dpc245/dpc245_clock.c:47`
- Linkage: `static`
- Type: `_Bool (volatile uint32_t *, uint32_t, uint32_t)`
- Responsibility: Bounded wait for a hardware or state condition in the dpc245_clock module.
- Direct callers: `dpc245_clock_init`
- Direct callees: —

### `dpc245_clock_tree_now`

- Location: `platform/dpc245/dpc245_clock.c:54`
- Linkage: `global`
- Type: `dpc245_clock_tree_t (void)`
- Responsibility: Public function of the dpc245_clock module.
- Direct callers: `start_clock`
- Direct callees: `dpc245_clock_classify`

### `dpc245_clock_fallback_hsi`

- Location: `platform/dpc245/dpc245_clock.c:58`
- Linkage: `global`
- Type: `dpc245_clock_tree_t (void)`
- Responsibility: Public function of the dpc245_clock module.
- Direct callers: `start_clock`
- Direct callees: `dpc245_clock_force_hsi`

### `dpc245_clock_init`

- Location: `platform/dpc245/dpc245_clock.c:63`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Initialization in the dpc245_clock module.
- Direct callers: `start_clock`
- Direct callees: `wait_mask`

## `platform/dpc245/dpc245_hwinfo.c`

MCU hardware-information readout.

Functions: **1**

### `dpc245_hwinfo_read`

- Location: `platform/dpc245/dpc245_hwinfo.c:10`
- Linkage: `global`
- Type: `void (dpc245_hwinfo_t *)`
- Responsibility: Read operation in the dpc245_hwinfo module.
- Direct callers: `dpc245_probe_run`, `main`
- Direct callees: `dpc245_unknown_pa0_high`, `dpc245_unknown_pa0_init_pullup`

## `platform/dpc245/dpc245_preflight.c`

Module implementation.

Functions: **4**

### `handoff_after_ble_request`

- Location: `platform/dpc245/dpc245_preflight.c:20`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_preflight module.
- Direct callers: `dpc245_preflight_poll`
- Direct callees: `dpc245_can_transmit`, `dpc245_can_tx_idle`, `dpc245_enter_resident_bootloader`, `dpc245_update_init`, `dpc245_update_next_can`, `dpc245_update_request`

### `dpc245_preflight_begin`

- Location: `platform/dpc245/dpc245_preflight.c:34`
- Linkage: `global`
- Type: `void (dpc245_preflight_clock_t, dpc245_preflight_stats_t *)`
- Responsibility: Public function of the dpc245_preflight module.
- Direct callers: `clock_failure_safe_mode`, `main`
- Direct callees: `dpc245_ble_parser_init`, `dpc245_ble_uart_init`, `dpc245_ble_uart_init_hsi`, `dpc245_can_init_250k`, `dpc245_can_init_250k_hsi`, `dpc245_recovery_chord_init`

### `dpc245_preflight_poll`

- Location: `platform/dpc245/dpc245_preflight.c:53`
- Linkage: `global`
- Type: `void (uint32_t, dpc245_preflight_stats_t *)`
- Responsibility: Public function of the dpc245_preflight module.
- Direct callers: `dpc245_preflight_run`, `dpc245_probe_run`
- Direct callees: `dpc245_ble_parser_feed`, `dpc245_ble_uart_poll`, `dpc245_ble_uart_read_byte`, `dpc245_buttons_raw`, `dpc245_can_receive_poll`, `dpc245_enter_resident_bootloader`, `dpc245_is_ble_update_request`, `dpc245_is_can_update_request`, `handoff_after_ble_request`

### `dpc245_preflight_run`

- Location: `platform/dpc245/dpc245_preflight.c:75`
- Linkage: `global`
- Type: `void (uint32_t, dpc245_millis_fn, dpc245_preflight_stats_t *)`
- Responsibility: Public function of the dpc245_preflight module.
- Direct callers: —
- Direct callees: `dpc245_preflight_poll`

## `platform/dpc245/dpc245_probe.c`

Module implementation.

Functions: **9**

### `marker_value`

- Location: `platform/dpc245/dpc245_probe.c:26`
- Linkage: `static`
- Type: `uint16_t (void)`
- Responsibility: Internal helper of the dpc245_probe module.
- Direct callers: `dpc245_probe_boot_init`, `dpc245_probe_run`
- Direct callees: —

### `dpc245_probe_boot_init`

- Location: `platform/dpc245/dpc245_probe.c:30`
- Linkage: `global`
- Type: `void (dpc245_probe_boot_t *)`
- Responsibility: Initialization in the dpc245_probe module.
- Direct callers: —
- Direct callees: `marker_value`

### `row_label`

- Location: `platform/dpc245/dpc245_probe.c:39`
- Linkage: `static`
- Type: `void (unsigned int, const char *)`
- Responsibility: Internal helper of the dpc245_probe module.
- Direct callers: `draw_static`
- Direct callees: `ui_text`

### `row_u32`

- Location: `platform/dpc245/dpc245_probe.c:43`
- Linkage: `static`
- Type: `void (unsigned int, uint32_t)`
- Responsibility: Internal helper of the dpc245_probe module.
- Direct callers: `draw_dynamic`, `draw_static`
- Direct callees: `dpc245_lcd_fill_rect`, `ui_u32`

### `row_hex`

- Location: `platform/dpc245/dpc245_probe.c:48`
- Linkage: `static`
- Type: `void (unsigned int, uint32_t, uint8_t)`
- Responsibility: Internal helper of the dpc245_probe module.
- Direct callers: `draw_static`
- Direct callees: `ui_hex32`

### `draw_marker`

- Location: `platform/dpc245/dpc245_probe.c:55`
- Linkage: `static`
- Type: `void (const dpc245_probe_boot_t *)`
- Responsibility: Drawing routine in the dpc245_probe module.
- Direct callers: `dpc245_probe_run`, `draw_static`
- Direct callees: `dpc245_lcd_fill_rect`, `ui_hex32`, `ui_text`, `ui_u32`

### `draw_static`

- Location: `platform/dpc245/dpc245_probe.c:70`
- Linkage: `static`
- Type: `void (const dpc245_probe_boot_t *, const dpc245_hwinfo_t *, const dpc245_preflight_stats_t *)`
- Responsibility: Drawing routine in the dpc245_probe module.
- Direct callers: `dpc245_probe_run`
- Direct callees: `dpc245_lcd_fill`, `draw_marker`, `row_hex`, `row_label`, `row_u32`, `ui_hex32`, `ui_hex8`, `ui_text`

### `draw_dynamic`

- Location: `platform/dpc245/dpc245_probe.c:103`
- Linkage: `static`
- Type: `void (const dpc245_preflight_stats_t *, const dpc245_adc_snapshot_t *, uint32_t)`
- Responsibility: Drawing routine in the dpc245_probe module.
- Direct callers: `dpc245_probe_run`
- Direct callees: `dpc245_buttons_raw`, `dpc245_lcd_fill_rect`, `row_u32`, `ui_u32`

### `dpc245_probe_run`

- Location: `platform/dpc245/dpc245_probe.c:124`
- Linkage: `global`
- Type: `void (dpc245_probe_boot_t *, dpc245_millis_fn, void (*)(uint32_t), dpc245_preflight_stats_t *) __attribute__((noreturn))`
- Responsibility: Public function of the dpc245_probe module.
- Direct callers: —
- Direct callees: `dpc245_adc_init`, `dpc245_adc_sample32`, `dpc245_backlight_init`, `dpc245_backlight_set_level`, `dpc245_boot_marker_clear`, `dpc245_hwinfo_read`, `dpc245_lcd_init`, `dpc245_preflight_poll`, `draw_dynamic`, `draw_marker`, `draw_static`, `marker_value`

## `platform/dpc245/dpc245_recovery.c`

APP marker invalidation and system reset entry path.

Functions: **5**

### `flash_wait_ready`

- Location: `platform/dpc245/dpc245_recovery.c:45`
- Linkage: `static`
- Type: `_Bool (void)`
- Responsibility: Read operation in the dpc245_recovery module.
- Direct callers: `dpc245_boot_marker_clear`
- Direct callees: —

### `dpc245_boot_marker_clear`

- Location: `platform/dpc245/dpc245_recovery.c:52`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Public function of the dpc245_recovery module.
- Direct callers: `dpc245_enter_resident_bootloader`, `dpc245_probe_run`, `early_recovery_guard`
- Direct callees: `flash_wait_ready`

### `fwdgt_start_reset`

- Location: `platform/dpc245/dpc245_recovery.c:76`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Start operation in the dpc245_recovery module.
- Direct callers: `dpc245_system_reset`
- Direct callees: —

### `dpc245_system_reset`

- Location: `platform/dpc245/dpc245_recovery.c:90`
- Linkage: `global`
- Type: `void (void) __attribute__((noreturn))`
- Responsibility: Public function of the dpc245_recovery module.
- Direct callers: `Default_Handler`, `Reset_Handler`, `dpc245_enter_resident_bootloader`, `early_recovery_guard`, `main`
- Direct callees: `fwdgt_start_reset`

### `dpc245_enter_resident_bootloader`

- Location: `platform/dpc245/dpc245_recovery.c:99`
- Linkage: `global`
- Type: `void (void) __attribute__((noreturn))`
- Responsibility: Public function of the dpc245_recovery module.
- Direct callers: `dpc245_preflight_poll`, `handoff_after_ble_request`, `main`
- Direct callees: `dpc245_boot_marker_clear`, `dpc245_system_reset`

## `platform/dpc245/dpc245_rtc_board.c`

Module implementation.

Functions: **7**

### `pc14_mode`

- Location: `platform/dpc245/dpc245_rtc_board.c:14`
- Linkage: `static`
- Type: `void (uint32_t)`
- Responsibility: Internal helper of the dpc245_rtc_board module.
- Direct callers: `pc14_input_pullup`, `pc14_output`
- Direct callees: —

### `pc14_output`

- Location: `platform/dpc245/dpc245_rtc_board.c:22`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_rtc_board module.
- Direct callers: `board_read`, `board_write`
- Direct callees: `pc14_mode`

### `pc14_input_pullup`

- Location: `platform/dpc245/dpc245_rtc_board.c:27`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_rtc_board module.
- Direct callers: `board_read`
- Direct callees: `pc14_mode`

### `shift_byte`

- Location: `platform/dpc245/dpc245_rtc_board.c:34`
- Linkage: `static`
- Type: `void (uint8_t)`
- Responsibility: Internal helper of the dpc245_rtc_board module.
- Direct callers: `board_read`, `board_write`
- Direct callees: —

### `board_write`

- Location: `platform/dpc245/dpc245_rtc_board.c:40`
- Linkage: `static`
- Type: `_Bool (void *, uint8_t, uint8_t)`
- Responsibility: Write operation in the dpc245_rtc_board module.
- Direct callers: —
- Direct callees: `pc14_output`, `shift_byte`

### `board_read`

- Location: `platform/dpc245/dpc245_rtc_board.c:48`
- Linkage: `static`
- Type: `_Bool (void *, uint8_t, uint8_t *)`
- Responsibility: Read operation in the dpc245_rtc_board module.
- Direct callers: —
- Direct callees: `pc14_input_pullup`, `pc14_output`, `shift_byte`

### `dpc245_rtc_board_io`

- Location: `platform/dpc245/dpc245_rtc_board.c:66`
- Linkage: `global`
- Type: `dpc245_rtc_io_t (void)`
- Responsibility: Public function of the dpc245_rtc_board module.
- Direct callers: `main`
- Direct callees: —

## `platform/dpc245/dpc245_spi_mem.c`

External SPI memory driver and persistence I/O adapter.

Functions: **24**

### `dpc245_spi_mem_set_limits`

- Location: `platform/dpc245/dpc245_spi_mem.c:25`
- Linkage: `global`
- Type: `void (uint32_t, uint32_t)`
- Responsibility: Setter in the dpc245_spi_mem module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_spi_mem_end_step`

- Location: `platform/dpc245/dpc245_spi_mem.c:30`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Public function of the dpc245_spi_mem module.
- Direct callers: `main`
- Direct callees: —

### `operation_begin`

- Location: `platform/dpc245/dpc245_spi_mem.c:31`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_page`, `dpc245_spi_mem_erase_start`, `dpc245_spi_mem_read`, `dpc245_spi_mem_read_jedec_id`, `dpc245_spi_mem_ready`, `dpc245_spi_mem_reset`, `dpc245_spi_mem_status`, `dpc245_spi_mem_write`
- Direct callees: —

### `cs_high`

- Location: `platform/dpc245/dpc245_spi_mem.c:38`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_start`, `dpc245_spi_mem_init`, `dpc245_spi_mem_read`, `dpc245_spi_mem_read_jedec_id`, `dpc245_spi_mem_ready`, `dpc245_spi_mem_reset`, `dpc245_spi_mem_status`, `program_chunk`, `wait_not_busy`, `write_enable`
- Direct callees: —

### `cs_low`

- Location: `platform/dpc245/dpc245_spi_mem.c:39`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_start`, `dpc245_spi_mem_read`, `dpc245_spi_mem_read_jedec_id`, `dpc245_spi_mem_ready`, `dpc245_spi_mem_reset`, `dpc245_spi_mem_status`, `program_chunk`, `wait_not_busy`, `write_enable`
- Direct callees: —

### `wait_flag`

- Location: `platform/dpc245/dpc245_spi_mem.c:41`
- Linkage: `static`
- Type: `_Bool (uint16_t, _Bool)`
- Responsibility: Bounded wait for a hardware or state condition in the dpc245_spi_mem module.
- Direct callers: `xfer`
- Direct callees: —

### `xfer`

- Location: `platform/dpc245/dpc245_spi_mem.c:53`
- Linkage: `static`
- Type: `uint8_t (uint8_t)`
- Responsibility: Internal helper of the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_start`, `dpc245_spi_mem_read`, `dpc245_spi_mem_read_jedec_id`, `dpc245_spi_mem_ready`, `dpc245_spi_mem_reset`, `dpc245_spi_mem_status`, `program_chunk`, `wait_not_busy`, `write_enable`
- Direct callees: `wait_flag`

### `wait_not_busy`

- Location: `platform/dpc245/dpc245_spi_mem.c:60`
- Linkage: `static`
- Type: `_Bool (void)`
- Responsibility: Bounded wait for a hardware or state condition in the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_page`, `dpc245_spi_mem_erase_start`, `dpc245_spi_mem_read`, `dpc245_spi_mem_reset`, `program_chunk`
- Direct callees: `cs_high`, `cs_low`, `xfer`

### `write_enable`

- Location: `platform/dpc245/dpc245_spi_mem.c:73`
- Linkage: `static`
- Type: `_Bool (void)`
- Responsibility: Internal helper of the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_start`, `program_chunk`
- Direct callees: `cs_high`, `cs_low`, `xfer`

### `dpc245_spi_mem_init`

- Location: `platform/dpc245/dpc245_spi_mem.c:80`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Initialization in the dpc245_spi_mem module.
- Direct callers: `main`
- Direct callees: `cs_high`

### `dpc245_spi_mem_reset`

- Location: `platform/dpc245/dpc245_spi_mem.c:102`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Public function of the dpc245_spi_mem module.
- Direct callers: —
- Direct callees: `cs_high`, `cs_low`, `operation_begin`, `wait_not_busy`, `xfer`

### `dpc245_spi_mem_status`

- Location: `platform/dpc245/dpc245_spi_mem.c:110`
- Linkage: `global`
- Type: `uint8_t (void)`
- Responsibility: Public function of the dpc245_spi_mem module.
- Direct callers: —
- Direct callees: `cs_high`, `cs_low`, `operation_begin`, `xfer`

### `dpc245_spi_mem_read_jedec_id`

- Location: `platform/dpc245/dpc245_spi_mem.c:116`
- Linkage: `global`
- Type: `_Bool (uint8_t *)`
- Responsibility: Read operation in the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_start`
- Direct callees: `cs_high`, `cs_low`, `operation_begin`, `xfer`

### `dpc245_spi_mem_read`

- Location: `platform/dpc245/dpc245_spi_mem.c:132`
- Linkage: `global`
- Type: `_Bool (uint32_t, uint8_t *, size_t)`
- Responsibility: Read operation in the dpc245_spi_mem module.
- Direct callers: `io_read`
- Direct callees: `cs_high`, `cs_low`, `operation_begin`, `wait_not_busy`, `xfer`

### `program_chunk`

- Location: `platform/dpc245/dpc245_spi_mem.c:149`
- Linkage: `static`
- Type: `_Bool (uint32_t, const uint8_t *, size_t)`
- Responsibility: Internal helper of the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_write`
- Direct callees: `cs_high`, `cs_low`, `wait_not_busy`, `write_enable`, `xfer`

### `dpc245_spi_mem_write`

- Location: `platform/dpc245/dpc245_spi_mem.c:163`
- Linkage: `global`
- Type: `_Bool (uint32_t, const uint8_t *, size_t)`
- Responsibility: Write operation in the dpc245_spi_mem module.
- Direct callers: `io_write`
- Direct callees: `operation_begin`, `program_chunk`

### `dpc245_spi_mem_erase_page`

- Location: `platform/dpc245/dpc245_spi_mem.c:177`
- Linkage: `global`
- Type: `_Bool (uint32_t)`
- Responsibility: Public function of the dpc245_spi_mem module.
- Direct callers: `io_erase`
- Direct callees: `dpc245_spi_mem_erase_start`, `operation_begin`, `wait_not_busy`

### `dpc245_spi_mem_erase_allowed`

- Location: `platform/dpc245/dpc245_spi_mem.c:184`
- Linkage: `global`
- Type: `_Bool (uint32_t)`
- Responsibility: Public function of the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_start`
- Direct callees: —

### `dpc245_spi_mem_erase_start`

- Location: `platform/dpc245/dpc245_spi_mem.c:191`
- Linkage: `global`
- Type: `_Bool (uint32_t)`
- Responsibility: Start operation in the dpc245_spi_mem module.
- Direct callers: `dpc245_spi_mem_erase_page`, `settings_erase_start`
- Direct callees: `cs_high`, `cs_low`, `dpc245_spi_mem_erase_allowed`, `dpc245_spi_mem_read_jedec_id`, `operation_begin`, `wait_not_busy`, `write_enable`, `xfer`

### `dpc245_spi_mem_ready`

- Location: `platform/dpc245/dpc245_spi_mem.c:208`
- Linkage: `global`
- Type: `int (void)`
- Responsibility: Read operation in the dpc245_spi_mem module.
- Direct callers: `settings_ready`
- Direct callees: `cs_high`, `cs_low`, `operation_begin`, `xfer`

### `io_read`

- Location: `platform/dpc245/dpc245_spi_mem.c:219`
- Linkage: `static`
- Type: `_Bool (void *, uint32_t, uint8_t *, size_t)`
- Responsibility: Read operation in the dpc245_spi_mem module.
- Direct callers: —
- Direct callees: `dpc245_spi_mem_read`

### `io_write`

- Location: `platform/dpc245/dpc245_spi_mem.c:220`
- Linkage: `static`
- Type: `_Bool (void *, uint32_t, const uint8_t *, size_t)`
- Responsibility: Write operation in the dpc245_spi_mem module.
- Direct callers: —
- Direct callees: `dpc245_spi_mem_write`

### `io_erase`

- Location: `platform/dpc245/dpc245_spi_mem.c:221`
- Linkage: `static`
- Type: `_Bool (void *, uint32_t)`
- Responsibility: Internal helper of the dpc245_spi_mem module.
- Direct callers: —
- Direct callees: `dpc245_spi_mem_erase_page`

### `dpc245_spi_mem_persist_io`

- Location: `platform/dpc245/dpc245_spi_mem.c:223`
- Linkage: `global`
- Type: `dpc245_persist_io_t (void)`
- Responsibility: Public function of the dpc245_spi_mem module.
- Direct callers: `main`
- Direct callees: —

## `platform/dpc245/minifont.c`

Minimal text/numeric renderer used by the UI.

Functions: **12**

### `glyph`

- Location: `platform/dpc245/minifont.c:22`
- Linkage: `static`
- Type: `const uint8_t *(char)`
- Responsibility: Internal helper of the minifont module.
- Direct callers: `ui_text`, `ui_text_big`, `ui_text_big_width`, `ui_text_box`
- Direct callees: —

### `ui_text_box`

- Location: `platform/dpc245/minifont.c:28`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, const char *, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: —
- Direct callees: `dpc245_lcd_blit_rgb565`, `dpc245_lcd_fill_rect`, `glyph`

### `ui_text`

- Location: `platform/dpc245/minifont.c:50`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, const char *, uint16_t, uint16_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: `draw_marker`, `draw_static`, `row_label`, `ui_fixed_u32`, `ui_hex32`, `ui_hex8`, `ui_i32`, `ui_u32`
- Direct callees: `dpc245_lcd_blit_rgb565`, `glyph`

### `ui_u32`

- Location: `platform/dpc245/minifont.c:70`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint32_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: `draw_dynamic`, `draw_marker`, `row_u32`, `ui_fixed_u32`, `ui_i32`
- Direct callees: `ui_text`

### `ui_i32`

- Location: `platform/dpc245/minifont.c:75`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, int32_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: —
- Direct callees: `ui_text`, `ui_u32`

### `ui_fixed_u32`

- Location: `platform/dpc245/minifont.c:79`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint32_t, uint8_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: —
- Direct callees: `ui_text`, `ui_u32`

### `hex_digit`

- Location: `platform/dpc245/minifont.c:93`
- Linkage: `static`
- Type: `char (uint8_t)`
- Responsibility: Internal helper of the minifont module.
- Direct callers: `dpc245_hmi_service_build_read_reply`, `ui_hex32`, `ui_hex8`
- Direct callees: —

### `ui_hex8`

- Location: `platform/dpc245/minifont.c:94`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint8_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: `draw_static`
- Direct callees: `hex_digit`, `ui_text`

### `ui_hex32`

- Location: `platform/dpc245/minifont.c:97`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint32_t, uint8_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: `draw_marker`, `draw_static`, `row_hex`
- Direct callees: `hex_digit`, `ui_text`

### `big_glyph_span`

- Location: `platform/dpc245/minifont.c:105`
- Linkage: `static`
- Type: `void (char, const uint8_t *, uint8_t *, uint8_t *)`
- Responsibility: Internal helper of the minifont module.
- Direct callers: `ui_text_big`
- Direct callees: —

### `ui_text_big_width`

- Location: `platform/dpc245/minifont.c:113`
- Linkage: `global`
- Type: `uint16_t (const char *, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: —
- Direct callees: `glyph`

### `ui_text_big`

- Location: `platform/dpc245/minifont.c:120`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, const char *, uint16_t, uint16_t, uint8_t)`
- Responsibility: Public function of the minifont module.
- Direct callers: —
- Direct callees: `big_glyph_span`, `dpc245_lcd_fill_rect`, `glyph`

## `platform/dpc245/runtime.c`

Freestanding C/AEABI runtime primitives required by the linker.

Functions: **17**

### `memcpy`

- Location: `platform/dpc245/runtime.c:0`
- Linkage: `global`
- Type: `void *(void *, const void *, unsigned long)`
- Responsibility: Public function of the runtime module.
- Direct callers: `__aeabi_memcpy`, `__aeabi_memcpy4`, `__aeabi_memcpy8`
- Direct callees: —

### `memmove`

- Location: `platform/dpc245/runtime.c:0`
- Linkage: `global`
- Type: `void *(void *, const void *, unsigned long)`
- Responsibility: Public function of the runtime module.
- Direct callers: `__aeabi_memmove`, `__aeabi_memmove4`, `__aeabi_memmove8`
- Direct callees: —

### `memset`

- Location: `platform/dpc245/runtime.c:0`
- Linkage: `global`
- Type: `void *(void *, int, unsigned long)`
- Responsibility: Public function of the runtime module.
- Direct callers: `__aeabi_memclr`, `__aeabi_memclr4`, `__aeabi_memclr8`, `bafang_build_besst_controller_config_write`, `bafang_build_hmi_heartbeat_v3`, `bafang_build_read_query`, `bafang_build_service_ack`, `bafang_service_rx_feed`, `bafang_service_rx_init`, `dpc245_hmi_control_build_read_reply`, `dpc245_hmi_control_process`, `dpc245_hmi_identity_records_init`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_init`, `dpc245_hmi_service_tx_start`, `evist_can_router_init`, `make_ack`
- Direct callees: —

### `udiv32`

- Location: `platform/dpc245/runtime.c:6`
- Linkage: `static`
- Type: `uint32_t (uint32_t, uint32_t, uint32_t *)`
- Responsibility: Internal helper of the runtime module.
- Direct callers: `__aeabi_idiv`, `__aeabi_uidiv`, `__aeabi_uidivmod`
- Direct callees: —

### `__aeabi_uidiv`

- Location: `platform/dpc245/runtime.c:12`
- Linkage: `global`
- Type: `uint32_t (uint32_t, uint32_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `udiv32`

### `__aeabi_uidivmod`

- Location: `platform/dpc245/runtime.c:14`
- Linkage: `global`
- Type: `udivmod_t (uint32_t, uint32_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `udiv32`

### `__aeabi_idiv`

- Location: `platform/dpc245/runtime.c:15`
- Linkage: `global`
- Type: `int32_t (int32_t, int32_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: `__aeabi_idivmod`
- Direct callees: `udiv32`

### `__aeabi_idivmod`

- Location: `platform/dpc245/runtime.c:20`
- Linkage: `global`
- Type: `idivmod_t (int32_t, int32_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `__aeabi_idiv`

### `__aeabi_memclr`

- Location: `platform/dpc245/runtime.c:21`
- Linkage: `global`
- Type: `void (void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memset`

### `__aeabi_memclr4`

- Location: `platform/dpc245/runtime.c:22`
- Linkage: `global`
- Type: `void (void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memset`

### `__aeabi_memclr8`

- Location: `platform/dpc245/runtime.c:23`
- Linkage: `global`
- Type: `void (void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memset`

### `__aeabi_memcpy`

- Location: `platform/dpc245/runtime.c:24`
- Linkage: `global`
- Type: `void (void *, const void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memcpy`

### `__aeabi_memcpy4`

- Location: `platform/dpc245/runtime.c:25`
- Linkage: `global`
- Type: `void (void *, const void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memcpy`

### `__aeabi_memcpy8`

- Location: `platform/dpc245/runtime.c:26`
- Linkage: `global`
- Type: `void (void *, const void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memcpy`

### `__aeabi_memmove`

- Location: `platform/dpc245/runtime.c:28`
- Linkage: `global`
- Type: `void (void *, const void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memmove`

### `__aeabi_memmove4`

- Location: `platform/dpc245/runtime.c:29`
- Linkage: `global`
- Type: `void (void *, const void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memmove`

### `__aeabi_memmove8`

- Location: `platform/dpc245/runtime.c:30`
- Linkage: `global`
- Type: `void (void *, const void *, size_t)`
- Responsibility: Public function of the runtime module.
- Direct callers: —
- Direct callees: `memmove`

## `platform/dpc245/st7789_8080.c`

8080-style parallel LCD bus and RGB565 drawing primitives.

Functions: **14**

### `pin_set`

- Location: `platform/dpc245/st7789_8080.c:27`
- Linkage: `static`
- Type: `void (uint32_t, unsigned int)`
- Responsibility: Internal helper of the st7789_8080 module.
- Direct callers: `dpc245_lcd_bus_init`, `dpc245_lcd_command`, `dpc245_lcd_init`, `write_byte`
- Direct callees: —

### `pin_clr`

- Location: `platform/dpc245/st7789_8080.c:28`
- Linkage: `static`
- Type: `void (uint32_t, unsigned int)`
- Responsibility: Internal helper of the st7789_8080 module.
- Direct callers: `dpc245_lcd_command`, `dpc245_lcd_init`, `write_byte`
- Direct callees: —

### `gpio_cfg_output_pp_50mhz`

- Location: `platform/dpc245/st7789_8080.c:30`
- Linkage: `static`
- Type: `void (uint32_t, unsigned int)`
- Responsibility: Internal helper of the st7789_8080 module.
- Direct callers: `dpc245_lcd_bus_init`
- Direct callees: —

### `dpc245_lcd_bus_init`

- Location: `platform/dpc245/st7789_8080.c:39`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Initialization in the st7789_8080 module.
- Direct callers: `dpc245_lcd_init`
- Direct callees: `dpc245_debug_keep_swd_release_jtag`, `gpio_cfg_output_pp_50mhz`, `pin_set`

### `write_byte`

- Location: `platform/dpc245/st7789_8080.c:58`
- Linkage: `static`
- Type: `void (uint8_t)`
- Responsibility: Internal helper of the st7789_8080 module.
- Direct callers: `dpc245_lcd_command`, `dpc245_lcd_data16`, `dpc245_lcd_data8`
- Direct callees: `pin_clr`, `pin_set`

### `dpc245_lcd_command`

- Location: `platform/dpc245/st7789_8080.c:66`
- Linkage: `global`
- Type: `void (uint8_t)`
- Responsibility: Public function of the st7789_8080 module.
- Direct callers: `cmd_data`, `dpc245_lcd_init`, `dpc245_lcd_set_window`
- Direct callees: `pin_clr`, `pin_set`, `write_byte`

### `dpc245_lcd_data8`

- Location: `platform/dpc245/st7789_8080.c:71`
- Linkage: `global`
- Type: `void (uint8_t)`
- Responsibility: Public function of the st7789_8080 module.
- Direct callers: `cmd_data`, `dpc245_lcd_set_window`
- Direct callees: `write_byte`

### `dpc245_lcd_data16`

- Location: `platform/dpc245/st7789_8080.c:72`
- Linkage: `global`
- Type: `void (uint16_t)`
- Responsibility: Public function of the st7789_8080 module.
- Direct callers: `dpc245_lcd_blit_rgb565`, `dpc245_lcd_fill`, `dpc245_lcd_fill_rect`, `screen_push`, `splash_emit`, `text_clipped`
- Direct callees: `ui_strip_capturing`, `ui_strip_data16`, `write_byte`

### `cmd_data`

- Location: `platform/dpc245/st7789_8080.c:77`
- Linkage: `static`
- Type: `void (uint8_t, const uint8_t *, size_t)`
- Responsibility: Internal helper of the st7789_8080 module.
- Direct callers: `dpc245_lcd_init`
- Direct callees: `dpc245_lcd_command`, `dpc245_lcd_data8`

### `dpc245_lcd_init`

- Location: `platform/dpc245/st7789_8080.c:82`
- Linkage: `global`
- Type: `void (void (*)(uint32_t))`
- Responsibility: Initialization in the st7789_8080 module.
- Direct callers: `dpc245_probe_run`
- Direct callees: `cmd_data`, `dpc245_lcd_bus_init`, `dpc245_lcd_command`, `dpc245_lcd_fill`, `pin_clr`, `pin_set`

### `dpc245_lcd_set_window`

- Location: `platform/dpc245/st7789_8080.c:114`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t)`
- Responsibility: Setter in the st7789_8080 module.
- Direct callers: `dpc245_lcd_blit_rgb565`, `dpc245_lcd_fill`, `dpc245_lcd_fill_rect`, `screen_push`, `text_clipped`, `ui_boot_splash_draw`
- Direct callees: `dpc245_lcd_command`, `dpc245_lcd_data8`, `ui_strip_capturing`, `ui_strip_set_window`

### `dpc245_lcd_fill`

- Location: `platform/dpc245/st7789_8080.c:126`
- Linkage: `global`
- Type: `void (uint16_t)`
- Responsibility: Fill routine in the st7789_8080 module.
- Direct callers: `clear`, `dpc245_lcd_init`, `draw_static`
- Direct callees: `dpc245_lcd_data16`, `dpc245_lcd_set_window`, `ui_strip_capturing`, `ui_strip_fill_rect`

### `dpc245_lcd_fill_rect`

- Location: `platform/dpc245/st7789_8080.c:132`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t, uint16_t)`
- Responsibility: Fill routine in the st7789_8080 module.
- Direct callers: `adv_row`, `arc_tile`, `draw_arc`, `draw_battery_bar`, `draw_battery_widget`, `draw_dashboard`, `draw_dynamic`, `draw_lower_panel`, `draw_marker`, `draw_menu`, `draw_power_graph`, `draw_status_glyph`, `draw_wedge`, `lower_cell`, `round_rect`, `row_u32`, `theme_banner`, `theme_clear_regions`, `ui_text_big`, `ui_text_box`
- Direct callees: `dpc245_lcd_data16`, `dpc245_lcd_set_window`, `ui_strip_capturing`, `ui_strip_fill_rect`

### `dpc245_lcd_blit_rgb565`

- Location: `platform/dpc245/st7789_8080.c:141`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t, const uint16_t *)`
- Responsibility: Public function of the st7789_8080 module.
- Direct callers: `ui_text`, `ui_text_box`
- Direct callees: `dpc245_lcd_data16`, `dpc245_lcd_set_window`, `ui_strip_blit`, `ui_strip_capturing`

## `platform/dpc245/startup.c`

Vector table, reset initialization and early recovery guard.

Functions: **8**

### `Default_Handler`

- Location: `platform/dpc245/startup.c:44`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Safe trap for unhandled interrupt vectors.
- Direct callers: —
- Direct callees: `dpc245_system_reset`

### `SysTick_Handler`

- Location: `platform/dpc245/startup.c:45`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Advances the 1 ms system time base.
- Direct callers: —
- Direct callees: `dpc245_buttons_tick_1ms`

### `clock_force_hsi8`

- Location: `platform/dpc245/startup.c:50`
- Linkage: `static`
- Type: `dpc245_clock_tree_t (void)`
- Responsibility: Internal helper of the startup module.
- Direct callers: `Reset_Handler`
- Direct callees: `dpc245_clock_force_hsi`

### `early_cfg_key_pullup`

- Location: `platform/dpc245/startup.c:58`
- Linkage: `static`
- Type: `void (unsigned int)`
- Responsibility: Internal helper of the startup module.
- Direct callers: `early_recovery_guard`
- Direct callees: —

### `early_all_keys_held`

- Location: `platform/dpc245/startup.c:67`
- Linkage: `static`
- Type: `int (void)`
- Responsibility: Internal helper of the startup module.
- Direct callers: `early_recovery_guard`
- Direct callees: —

### `early_delay_ms`

- Location: `platform/dpc245/startup.c:73`
- Linkage: `static`
- Type: `void (uint32_t, uint32_t)`
- Responsibility: Internal helper of the startup module.
- Direct callers: `early_recovery_guard`
- Direct callees: —

### `early_recovery_guard`

- Location: `platform/dpc245/startup.c:83`
- Linkage: `static`
- Type: `void (uint32_t)`
- Responsibility: Internal helper of the startup module.
- Direct callers: `Reset_Handler`
- Direct callees: `dpc245_boot_marker_clear`, `dpc245_system_reset`, `early_all_keys_held`, `early_cfg_key_pullup`, `early_delay_ms`

### `Reset_Handler`

- Location: `platform/dpc245/startup.c:97`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Initializes memory, executes the early recovery guard, then enters main().
- Direct callers: —
- Direct callees: `clock_force_hsi8`, `dpc245_clock_tree_core_hz`, `dpc245_system_reset`, `early_recovery_guard`, `main`

## `platform/dpc245/ui_font1.c`

Module implementation.

Functions: **5**

### `find`

- Location: `platform/dpc245/ui_font1.c:16`
- Linkage: `static`
- Type: `const ui_font1_glyph_t *(const ui_font1_t *, char)`
- Responsibility: Internal helper of the ui_font1 module.
- Direct callers: `text_clipped`, `ui_font1_width`
- Direct callees: —

### `ui_font1_width`

- Location: `platform/dpc245/ui_font1.c:22`
- Linkage: `global`
- Type: `uint16_t (const ui_font1_t *, const char *)`
- Responsibility: Public function of the ui_font1 module.
- Direct callers: `draw_battery_widget`, `draw_dashboard`, `draw_page_no`, `lower_cell`, `theme_right_text`, `value_text`, `value_x100_i`
- Direct callees: `find`

### `text_clipped`

- Location: `platform/dpc245/ui_font1.c:37`
- Linkage: `static`
- Type: `uint16_t (const ui_font1_t *, uint16_t, int, const char *, uint16_t, uint16_t, int, int)`
- Responsibility: Internal helper of the ui_font1 module.
- Direct callers: `ui_font1_text`, `ui_font1_text_clipped`
- Direct callees: `dpc245_lcd_data16`, `dpc245_lcd_set_window`, `find`, `ui_strip_rows`

### `ui_font1_text`

- Location: `platform/dpc245/ui_font1.c:94`
- Linkage: `global`
- Type: `uint16_t (const ui_font1_t *, uint16_t, int, const char *, uint16_t, uint16_t)`
- Responsibility: Public function of the ui_font1 module.
- Direct callers: `draw_advanced`, `draw_arc`, `draw_battery_widget`, `draw_dashboard`, `draw_edit_num`, `draw_mode_badge`, `draw_wedge`, `lower_cell`, `m_num`, `m_text`, `theme_right_text`
- Direct callees: `text_clipped`

### `ui_font1_text_clipped`

- Location: `platform/dpc245/ui_font1.c:99`
- Linkage: `global`
- Type: `uint16_t (const ui_font1_t *, uint16_t, int, const char *, uint16_t, uint16_t, uint16_t, uint16_t)`
- Responsibility: Public function of the ui_font1 module.
- Direct callers: `draw_speed`, `draw_speed_digit`, `theme_speed`, `theme_speed_arc`, `theme_speed_cell`
- Direct callees: `text_clipped`

## `src/advanced_settings.c`

Advanced settings state machine and controller-configuration editing.

Functions: **44**

### `wrap_up`

- Location: `src/advanced_settings.c:33`
- Linkage: `static`
- Type: `uint8_t (uint8_t, size_t)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `accel_key`, `assist_key`, `dpc245_app_key`, `evist_advanced_key`, `preference_key`, `wheel_key`
- Direct callees: —

### `wrap_down`

- Location: `src/advanced_settings.c:34`
- Linkage: `static`
- Type: `uint8_t (uint8_t, size_t)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `accel_key`, `assist_key`, `dpc245_app_key`, `evist_advanced_key`, `preference_key`, `wheel_key`
- Direct callees: —

### `cycle_u8`

- Location: `src/advanced_settings.c:36`
- Linkage: `static`
- Type: `void (uint8_t *, uint8_t, uint8_t, bool, bool)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `accel_key`, `preference_edit`
- Direct callees: —

### `cycle_u16`

- Location: `src/advanced_settings.c:40`
- Linkage: `static`
- Type: `void (uint16_t *, uint16_t, uint16_t, uint16_t, bool, bool)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `preference_edit`
- Direct callees: —

### `clamp_step_u16`

- Location: `src/advanced_settings.c:45`
- Linkage: `static`
- Type: `bool (uint16_t *, uint16_t, uint16_t, uint16_t, bool)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `preference_edit`, `wheel_edit`
- Direct callees: —

### `copy64`

- Location: `src/advanced_settings.c:51`
- Linkage: `static`
- Type: `void (uint8_t *, const uint8_t *)`
- Responsibility: Copy helper in the advanced_settings module.
- Direct callers: `assist_save`, `back_to_menu`, `evist_advanced_abandon`, `evist_advanced_on_result`
- Direct callees: —

### `accel_values_from_block`

- Location: `src/advanced_settings.c:52`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `back_to_menu`, `evist_advanced_abandon`, `evist_advanced_on_result`
- Direct callees: —

### `enter`

- Location: `src/advanced_settings.c:57`
- Linkage: `static`
- Type: `void (evist_advanced_t *, evist_adv_screen_t)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `back_to_menu`, `dpc245_app_event`, `dpc245_app_key`, `dpc245_app_tick`, `evist_advanced_key`, `evist_advanced_open_page`, `open_accel`, `open_advanced`, `open_assist`, `open_wheel`
- Direct callees: —

### `request`

- Location: `src/advanced_settings.c:66`
- Linkage: `static`
- Type: `void (evist_advanced_t *, evist_adv_request_t, const char *)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `accel_save`, `assist_save`, `evist_advanced_on_result`, `open_accel`, `open_assist`, `open_wheel`, `wheel_save`
- Direct callees: —

### `open_wheel`

- Location: `src/advanced_settings.c:71`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `evist_advanced_open_page`
- Direct callees: `enter`, `request`

### `open_assist`

- Location: `src/advanced_settings.c:79`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `evist_advanced_open_page`
- Direct callees: `enter`, `request`

### `open_accel`

- Location: `src/advanced_settings.c:86`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `evist_advanced_open_page`
- Direct callees: `enter`, `request`

### `evist_advanced_open_page`

- Location: `src/advanced_settings.c:94`
- Linkage: `global`
- Type: `void (evist_advanced_t *, evist_adv_screen_t)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `evist_advanced_key`, `open_advanced`
- Direct callees: `enter`, `open_accel`, `open_assist`, `open_wheel`

### `back_to_menu`

- Location: `src/advanced_settings.c:103`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `accel_key`, `assist_key`, `preference_key`, `wheel_key`
- Direct callees: `accel_values_from_block`, `copy64`, `enter`

### `evist_advanced_abandon`

- Location: `src/advanced_settings.c:121`
- Linkage: `global`
- Type: `void (evist_advanced_t *)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `dpc245_app_tick`
- Direct callees: `accel_values_from_block`, `copy64`

### `commit_preferences`

- Location: `src/advanced_settings.c:143`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `preference_key`
- Direct callees: `evist_preferences_validate`

### `evist_advanced_open`

- Location: `src/advanced_settings.c:152`
- Linkage: `global`
- Type: `void (evist_advanced_t *, const evist_preferences_t *, uint8_t)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `dpc245_app_apply_preferences`, `dpc245_app_init`, `open_advanced`
- Direct callees: `evist_assist_level_count_normalize`, `evist_preferences_defaults`, `evist_ride_defaults`

### `evist_advanced_assist_cells`

- Location: `src/advanced_settings.c:189`
- Linkage: `global`
- Type: `uint8_t (const evist_advanced_t *)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `assist_key`, `draw_adv_assist`, `evist_advanced_assist_cell_value`, `evist_advanced_row_count`
- Direct callees: `evist_assist_visible_levels`

### `evist_advanced_row_count`

- Location: `src/advanced_settings.c:193`
- Linkage: `global`
- Type: `size_t (const evist_advanced_t *)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `accel_key`, `draw_advanced`, `preference_key`
- Direct callees: `evist_advanced_assist_cells`, `evist_assist_visible_levels`

### `evist_advanced_menu_label`

- Location: `src/advanced_settings.c:214`
- Linkage: `global`
- Type: `const char *(size_t)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `draw_advanced`
- Direct callees: —

### `evist_advanced_screen_title`

- Location: `src/advanced_settings.c:216`
- Linkage: `global`
- Type: `const char *(evist_adv_screen_t)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `draw_advanced`
- Direct callees: —

### `evist_advanced_wheel_diameter_label`

- Location: `src/advanced_settings.c:234`
- Linkage: `global`
- Type: `const char *(const evist_advanced_t *)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `draw_adv_wheel`
- Direct callees: —

### `evist_advanced_assist_cell_value`

- Location: `src/advanced_settings.c:243`
- Linkage: `global`
- Type: `uint8_t (const evist_advanced_t *, uint8_t)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `draw_adv_assist`
- Direct callees: `evist_advanced_assist_cells`, `evist_assist_column_offset`, `evist_assist_level_slot`

### `wheel_edit`

- Location: `src/advanced_settings.c:252`
- Linkage: `static`
- Type: `void (evist_advanced_t *, bool)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `wheel_key`
- Direct callees: `clamp_step_u16`

### `wheel_save`

- Location: `src/advanced_settings.c:269`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Save operation in the advanced_settings module.
- Direct callers: `wheel_key`
- Direct callees: `request`

### `wheel_key`

- Location: `src/advanced_settings.c:288`
- Linkage: `static`
- Type: `void (evist_advanced_t *, evist_adv_key_t)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `evist_advanced_key`
- Direct callees: `back_to_menu`, `wheel_edit`, `wheel_save`, `wrap_down`, `wrap_up`

### `evist_advanced_wheel_target`

- Location: `src/advanced_settings.c:310`
- Linkage: `global`
- Type: `void (const evist_advanced_t *, evist_wheel_config_t *)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `controller_config_service`
- Direct callees: —

### `wheel_load`

- Location: `src/advanced_settings.c:319`
- Linkage: `static`
- Type: `void (evist_advanced_t *, const uint8_t *)`
- Responsibility: Load operation in the advanced_settings module.
- Direct callers: `evist_advanced_on_result`
- Direct callees: `evist_wheel_config_decode`

### `assist_save`

- Location: `src/advanced_settings.c:330`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Save operation in the advanced_settings module.
- Direct callers: `assist_key`
- Direct callees: `copy64`, `evist_assist_prepare_write`, `request`

### `assist_key`

- Location: `src/advanced_settings.c:343`
- Linkage: `static`
- Type: `void (evist_advanced_t *, evist_adv_key_t)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `evist_advanced_key`
- Direct callees: `assist_save`, `back_to_menu`, `evist_advanced_assist_cells`, `evist_assist_edit_step`, `wrap_down`, `wrap_up`

### `accel_save`

- Location: `src/advanced_settings.c:379`
- Linkage: `static`
- Type: `void (evist_advanced_t *)`
- Responsibility: Save operation in the advanced_settings module.
- Direct callers: `accel_key`
- Direct callees: `evist_assist_checksum`, `request`

### `accel_key`

- Location: `src/advanced_settings.c:394`
- Linkage: `static`
- Type: `void (evist_advanced_t *, evist_adv_key_t)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `evist_advanced_key`
- Direct callees: `accel_save`, `back_to_menu`, `cycle_u8`, `evist_advanced_row_count`, `evist_assist_level_slot`, `wrap_down`, `wrap_up`

### `preference_edit`

- Location: `src/advanced_settings.c:421`
- Linkage: `static`
- Type: `void (evist_advanced_t *, bool)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `preference_key`
- Direct callees: `clamp_step_u16`, `cycle_u16`, `cycle_u8`

### `preference_key`

- Location: `src/advanced_settings.c:484`
- Linkage: `static`
- Type: `void (evist_advanced_t *, evist_adv_key_t)`
- Responsibility: Internal helper of the advanced_settings module.
- Direct callers: `evist_advanced_key`
- Direct callees: `back_to_menu`, `commit_preferences`, `evist_advanced_row_count`, `evist_ride_cfg_valid`, `preference_edit`, `wrap_down`, `wrap_up`

### `evist_advanced_key`

- Location: `src/advanced_settings.c:516`
- Linkage: `global`
- Type: `void (evist_advanced_t *, evist_adv_key_t)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: `accel_key`, `assist_key`, `enter`, `evist_advanced_open_page`, `preference_key`, `wheel_key`, `wrap_down`, `wrap_up`

### `evist_advanced_take_request`

- Location: `src/advanced_settings.c:550`
- Linkage: `global`
- Type: `bool (evist_advanced_t *, evist_adv_request_t *)`
- Responsibility: Consume-once accessor in the advanced_settings module.
- Direct callers: `controller_config_service`
- Direct callees: —

### `evist_advanced_request_failed`

- Location: `src/advanced_settings.c:562`
- Linkage: `global`
- Type: `void (evist_advanced_t *, const char *)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `controller_config_service`
- Direct callees: —

### `evist_advanced_on_result`

- Location: `src/advanced_settings.c:569`
- Linkage: `global`
- Type: `void (evist_advanced_t *, evist_txn_kind_t, evist_txn_result_t, const uint8_t *)`
- Responsibility: Public function of the advanced_settings module.
- Direct callers: `controller_config_service`
- Direct callees: `accel_values_from_block`, `copy64`, `evist_assist_block_valid`, `evist_assist_level_slot`, `evist_assist_visible_levels`, `evist_txn_result_text`, `request`, `wheel_load`

### `evist_advanced_take_preferences`

- Location: `src/advanced_settings.c:674`
- Linkage: `global`
- Type: `bool (evist_advanced_t *, evist_preferences_t *)`
- Responsibility: Consume-once accessor in the advanced_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `evist_advanced_take_light_runtime`

- Location: `src/advanced_settings.c:681`
- Linkage: `global`
- Type: `bool (evist_advanced_t *, evist_preferences_t *)`
- Responsibility: Consume-once accessor in the advanced_settings module.
- Direct callers: `main`
- Direct callees: —

### `evist_advanced_take_exit`

- Location: `src/advanced_settings.c:688`
- Linkage: `global`
- Type: `bool (evist_advanced_t *)`
- Responsibility: Consume-once accessor in the advanced_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `evist_advanced_set_status`

- Location: `src/advanced_settings.c:695`
- Linkage: `global`
- Type: `void (evist_advanced_t *, const char *)`
- Responsibility: Setter in the advanced_settings module.
- Direct callers: `main`
- Direct callees: —

### `evist_advanced_set_ride`

- Location: `src/advanced_settings.c:699`
- Linkage: `global`
- Type: `void (evist_advanced_t *, const evist_ride_cfg_t *)`
- Responsibility: Setter in the advanced_settings module.
- Direct callers: `dpc245_app_apply_preferences`, `dpc245_app_set_ride`, `open_advanced`
- Direct callees: `evist_ride_cfg_valid`

### `evist_advanced_take_ride`

- Location: `src/advanced_settings.c:705`
- Linkage: `global`
- Type: `bool (evist_advanced_t *, evist_ride_cfg_t *)`
- Responsibility: Consume-once accessor in the advanced_settings module.
- Direct callers: `dpc245_app_key`, `main`
- Direct callees: —

## `src/assist_profile.c`

Module implementation.

Functions: **15**

### `evist_assist_level_count_normalize`

- Location: `src/assist_profile.c:4`
- Linkage: `global`
- Type: `uint8_t (uint8_t)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `evist_advanced_open`, `evist_assist_level_slot`, `evist_assist_visible_levels`
- Direct callees: —

### `evist_assist_visible_levels`

- Location: `src/assist_profile.c:9`
- Linkage: `global`
- Type: `uint8_t (uint8_t)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `evist_advanced_assist_cells`, `evist_advanced_on_result`, `evist_advanced_row_count`, `evist_assist_edit_bounds`, `evist_assist_edit_step`, `reconcile_hidden`, `visible_monotonic`
- Direct callees: `evist_assist_level_count_normalize`

### `evist_assist_level_slot`

- Location: `src/assist_profile.c:14`
- Linkage: `global`
- Type: `uint8_t (uint8_t, uint8_t)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `accel_key`, `draw_advanced`, `evist_advanced_assist_cell_value`, `evist_advanced_on_result`, `evist_assist_edit_step`, `level_value`, `reconcile_hidden`
- Direct callees: `evist_assist_level_count_normalize`

### `evist_assist_column_offset`

- Location: `src/assist_profile.c:24`
- Linkage: `global`
- Type: `uint8_t (evist_assist_column_t)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `evist_advanced_assist_cell_value`, `evist_assist_column_monotonic`, `evist_assist_edit_bounds`, `evist_assist_edit_step`
- Direct callees: —

### `evist_assist_checksum`

- Location: `src/assist_profile.c:28`
- Linkage: `global`
- Type: `uint8_t (const uint8_t *)`
- Responsibility: Checksum calculation or check in the assist_profile module.
- Direct callers: `accel_save`, `complete_read`, `evist_assist_block_valid`, `evist_assist_prepare_write`, `queue_motor_write`
- Direct callees: —

### `evist_assist_block_valid`

- Location: `src/assist_profile.c:34`
- Linkage: `global`
- Type: `_Bool (const uint8_t *)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `evist_advanced_on_result`, `evist_assist_write_allowed`, `evist_ctrl_txn_write_app_p2`
- Direct callees: `evist_assist_checksum`

### `assist_values_in_range`

- Location: `src/assist_profile.c:38`
- Linkage: `static`
- Type: `_Bool (const uint8_t *)`
- Responsibility: Internal helper of the assist_profile module.
- Direct callers: `evist_assist_write_allowed`
- Direct callees: —

### `level_value`

- Location: `src/assist_profile.c:46`
- Linkage: `static`
- Type: `uint8_t (const uint8_t *, uint8_t, uint8_t, uint8_t)`
- Responsibility: Internal helper of the assist_profile module.
- Direct callers: `evist_assist_edit_bounds`, `visible_monotonic`
- Direct callees: `evist_assist_level_slot`

### `visible_monotonic`

- Location: `src/assist_profile.c:50`
- Linkage: `static`
- Type: `_Bool (const uint8_t *, uint8_t, uint8_t)`
- Responsibility: Internal helper of the assist_profile module.
- Direct callers: `evist_assist_prepare_write`
- Direct callees: `evist_assist_visible_levels`, `level_value`

### `reconcile_hidden`

- Location: `src/assist_profile.c:61`
- Linkage: `static`
- Type: `void (uint8_t *, uint8_t, uint8_t)`
- Responsibility: Internal helper of the assist_profile module.
- Direct callers: `evist_assist_edit_step`, `evist_assist_prepare_write`
- Direct callees: `evist_assist_level_slot`, `evist_assist_visible_levels`

### `evist_assist_column_monotonic`

- Location: `src/assist_profile.c:92`
- Linkage: `global`
- Type: `_Bool (const uint8_t *, evist_assist_column_t)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `evist_assist_prepare_write`, `evist_assist_write_allowed`
- Direct callees: `evist_assist_column_offset`

### `evist_assist_edit_bounds`

- Location: `src/assist_profile.c:100`
- Linkage: `global`
- Type: `void (const uint8_t *, evist_firmware_profile_t, uint8_t, evist_assist_column_t, uint8_t, uint8_t *, uint8_t *)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `evist_assist_edit_step`
- Direct callees: `evist_assist_column_offset`, `evist_assist_visible_levels`, `level_value`

### `evist_assist_edit_step`

- Location: `src/assist_profile.c:116`
- Linkage: `global`
- Type: `_Bool (uint8_t *, evist_firmware_profile_t, uint8_t, evist_assist_column_t, uint8_t, _Bool)`
- Responsibility: Public function of the assist_profile module.
- Direct callers: `assist_key`
- Direct callees: `evist_assist_column_offset`, `evist_assist_edit_bounds`, `evist_assist_level_slot`, `evist_assist_visible_levels`, `reconcile_hidden`

### `evist_assist_prepare_write`

- Location: `src/assist_profile.c:133`
- Linkage: `global`
- Type: `_Bool (uint8_t *, evist_firmware_profile_t, uint8_t)`
- Responsibility: Write operation in the assist_profile module.
- Direct callers: `assist_save`
- Direct callees: `evist_assist_checksum`, `evist_assist_column_monotonic`, `reconcile_hidden`, `visible_monotonic`

### `evist_assist_write_allowed`

- Location: `src/assist_profile.c:148`
- Linkage: `global`
- Type: `_Bool (const uint8_t *, evist_firmware_profile_t)`
- Responsibility: Write operation in the assist_profile module.
- Direct callers: `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_assist`, `queue_motor_write`
- Direct callees: `assist_values_in_range`, `evist_assist_block_valid`, `evist_assist_column_monotonic`

## `src/bafang_can.c`

CAN identifier handling, frame classification, telemetry decode and common frame serialization.

Functions: **15**

### `u16le`

- Location: `src/bafang_can.c:6`
- Linkage: `static`
- Type: `uint16_t (const uint8_t *)`
- Responsibility: Internal helper of the bafang_can module.
- Direct callers: `bafang_decode_telemetry`
- Direct callees: —

### `bafang_temperature_raw_valid`

- Location: `src/bafang_can.c:10`
- Linkage: `global`
- Type: `bool (uint8_t)`
- Responsibility: Public function of the bafang_can module.
- Direct callers: `bafang_decode_telemetry`, `dpc245_app_temperature_sample`, `draw_adv_diag`, `draw_controller`, `refresh_a3`, `temperature_value`
- Direct callees: —

### `bafang_wheel_tenths_to_raw`

- Location: `src/bafang_can.c:14`
- Linkage: `global`
- Type: `uint16_t (uint16_t)`
- Responsibility: Public function of the bafang_can module.
- Direct callers: `bafang_build_besst_controller_config_write`
- Direct callees: —

### `bafang_wheel_raw_to_tenths`

- Location: `src/bafang_can.c:21`
- Linkage: `global`
- Type: `uint16_t (uint16_t)`
- Responsibility: Public function of the bafang_can module.
- Direct callers: `bafang_decode_telemetry`
- Direct callees: —

### `bafang_can_id_encode`

- Location: `src/bafang_can.c:27`
- Linkage: `global`
- Type: `uint32_t (bafang_can_id_t)`
- Responsibility: Encoding in the bafang_can module.
- Direct callers: `bafang_build_read_query`, `bafang_build_service_ack`, `controller_id`, `dpc245_hmi_control_build_read_reply`, `make_ack`, `make_id`
- Direct callees: —

### `bafang_can_id_decode`

- Location: `src/bafang_can.c:35`
- Linkage: `global`
- Type: `bafang_can_id_t (uint32_t)`
- Responsibility: Decoding in the bafang_can module.
- Direct callers: `bafang_decode_telemetry`, `bafang_service_rx_feed`, `dpc245_hmi_control_build_read_reply`, `dpc245_hmi_control_process`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_on_ack`, `dpc245_is_can_update_request`, `dpc245_runtime_on_can`, `evist_can_router_rx`, `evist_ctrl_txn_on_frame`, `is_hmi_service_reply`, `is_read_query`, `is_service_ack`, `main`
- Direct callees: —

### `bafang_frame_classify`

- Location: `src/bafang_can.c:45`
- Linkage: `global`
- Type: `bafang_frame_kind_t (uint32_t)`
- Responsibility: Public function of the bafang_can module.
- Direct callers: `bafang_decode_telemetry`, `evist_can_router_rx`
- Direct callees: —

### `bafang_decode_telemetry`

- Location: `src/bafang_can.c:62`
- Linkage: `global`
- Type: `bool (const bafang_can_frame_t *, bafang_telemetry_t *)`
- Responsibility: Decoding in the bafang_can module.
- Direct callers: `evist_can_router_rx`, `m820_on_frame`
- Direct callees: `bafang_can_id_decode`, `bafang_frame_classify`, `bafang_temperature_raw_valid`, `bafang_wheel_raw_to_tenths`, `u16le`

### `bafang_assist_code`

- Location: `src/bafang_can.c:177`
- Linkage: `global`
- Type: `uint8_t (uint8_t, uint8_t, bool)`
- Responsibility: Public function of the bafang_can module.
- Direct callers: `assist_wire`, `bafang_build_hmi_heartbeat_v3`
- Direct callees: —

### `bafang_build_hmi_heartbeat_v3`

- Location: `src/bafang_can.c:191`
- Linkage: `global`
- Type: `bool (bafang_can_frame_t *, uint8_t, uint8_t, bool, bool, uint8_t, uint8_t)`
- Responsibility: Frame or record construction in the bafang_can module.
- Direct callers: `bafang_build_hmi_heartbeat_v3_default`, `dpc245_build_periodic_frame`
- Direct callees: `bafang_assist_code`, `memset`

### `bafang_build_hmi_heartbeat_v3_default`

- Location: `src/bafang_can.c:214`
- Linkage: `global`
- Type: `bool (bafang_can_frame_t *, uint8_t, uint8_t, bool, bool)`
- Responsibility: Frame or record construction in the bafang_can module.
- Direct callers: —
- Direct callees: `bafang_build_hmi_heartbeat_v3`

### `bafang_build_read_query`

- Location: `src/bafang_can.c:223`
- Linkage: `global`
- Type: `bool (bafang_can_frame_t *, uint8_t, uint8_t, uint8_t)`
- Responsibility: Read operation in the bafang_can module.
- Direct callers: `dpc245_build_service_query`, `send_read`
- Direct callees: `bafang_can_id_encode`, `memset`

### `bafang_build_besst_controller_config_write`

- Location: `src/bafang_can.c:240`
- Linkage: `global`
- Type: `bool (bafang_can_frame_t *, uint16_t, uint16_t, uint16_t)`
- Responsibility: Write operation in the bafang_can module.
- Direct callers: `evist_bafang_config_build_write`
- Direct callees: `bafang_wheel_tenths_to_raw`, `memset`

### `bafang_power_w`

- Location: `src/bafang_can.c:258`
- Linkage: `global`
- Type: `int32_t (const bafang_telemetry_t *)`
- Responsibility: Public function of the bafang_can module.
- Direct callers: `draw_adv_diag`, `main`, `ui_dashboard_lower_pair_text`
- Direct callees: —

### `bafang_walk_bit_fresh`

- Location: `src/bafang_can.c:267`
- Linkage: `global`
- Type: `bool (const bafang_telemetry_t *, uint32_t)`
- Responsibility: Public function of the bafang_can module.
- Direct callers: `draw_dashboard`, `theme_mode_colour`
- Direct callees: —

## `src/bafang_config.c`

Controller wheel/speed configuration conversion and validation.

Functions: **3**

### `evist_bafang_config_from_telemetry`

- Location: `src/bafang_config.c:3`
- Linkage: `global`
- Type: `void (evist_bafang_config_t *, const bafang_telemetry_t *)`
- Responsibility: Public function of the bafang_config module.
- Direct callers: —
- Direct callees: —

### `evist_bafang_config_validate`

- Location: `src/bafang_config.c:10`
- Linkage: `global`
- Type: `_Bool (const evist_bafang_config_t *)`
- Responsibility: Validation in the bafang_config module.
- Direct callers: `evist_bafang_config_build_write`
- Direct callees: —

### `evist_bafang_config_build_write`

- Location: `src/bafang_config.c:18`
- Linkage: `global`
- Type: `_Bool (const evist_bafang_config_t *, const evist_can_router_t *, _Bool, bafang_can_frame_t *)`
- Responsibility: Write operation in the bafang_config module.
- Direct callers: —
- Direct callees: `bafang_build_besst_controller_config_write`, `evist_bafang_config_validate`, `evist_can_router_tx_allowed`

## `src/bafang_service.c`

Long-transfer service receive path and decoded identity/battery service data.

Functions: **9**

### `copy_bytes`

- Location: `src/bafang_service.c:6`
- Linkage: `static`
- Type: `void (uint8_t *, const uint8_t *, uint8_t)`
- Responsibility: Copy helper in the bafang_service module.
- Direct callers: `assist_write_frame`, `bafang_battery_service_apply`, `bafang_service_rx_feed`, `cache_block`, `complete_read`, `copy_record_field`, `dpc245_ble_app_apply_advanced_cache`, `dpc245_ble_frame_build`, `dpc245_ble_parser_feed`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_next`, `dpc245_hmi_service_tx_start`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `load_record`, `make_text_reply`, `queue_motor_write`, `send_write`, `write_block`
- Direct callees: —

### `append_bytes`

- Location: `src/bafang_service.c:10`
- Linkage: `static`
- Type: `uint8_t (uint8_t *, uint8_t, const uint8_t *, uint8_t)`
- Responsibility: Internal helper of the bafang_service module.
- Direct callers: `bafang_service_rx_feed`
- Direct callees: —

### `bafang_service_rx_init`

- Location: `src/bafang_service.c:18`
- Linkage: `global`
- Type: `void (bafang_service_rx_t *)`
- Responsibility: Initialization in the bafang_service module.
- Direct callers: `evist_can_router_init`
- Direct callees: `memset`

### `service_is_to_hmi`

- Location: `src/bafang_service.c:22`
- Linkage: `static`
- Type: `bool (const bafang_can_id_t *)`
- Responsibility: Internal helper of the bafang_service module.
- Direct callers: `bafang_service_rx_feed`
- Direct callees: —

### `bafang_service_rx_feed`

- Location: `src/bafang_service.c:26`
- Linkage: `global`
- Type: `bool (bafang_service_rx_t *, const bafang_can_frame_t *, bafang_service_message_t *, bool *)`
- Responsibility: Input handling in the bafang_service module.
- Direct callers: `evist_can_router_rx`
- Direct callees: `append_bytes`, `bafang_can_id_decode`, `copy_bytes`, `memset`, `service_is_to_hmi`

### `bafang_build_service_ack`

- Location: `src/bafang_service.c:83`
- Linkage: `global`
- Type: `bool (bafang_can_frame_t *, uint8_t, uint8_t, uint8_t)`
- Responsibility: Frame or record construction in the bafang_service module.
- Direct callers: `evist_can_router_rx`
- Direct callees: `bafang_can_id_encode`, `memset`

### `copy_text`

- Location: `src/bafang_service.c:100`
- Linkage: `static`
- Type: `void (char *, const uint8_t *, uint8_t)`
- Responsibility: Copy helper in the bafang_service module.
- Direct callers: `bafang_identity_apply_service`
- Direct callees: —

### `bafang_identity_apply_service`

- Location: `src/bafang_service.c:111`
- Linkage: `global`
- Type: `bool (bafang_device_identity_t *, const bafang_service_message_t *)`
- Responsibility: Apply operation in the bafang_service module.
- Direct callers: `evist_can_router_rx`
- Direct callees: `copy_text`

### `bafang_battery_service_apply`

- Location: `src/bafang_service.c:129`
- Linkage: `global`
- Type: `bool (bafang_battery_service_info_t *, const bafang_service_message_t *)`
- Responsibility: Apply operation in the bafang_service module.
- Direct callers: `evist_can_router_rx`
- Direct callees: `copy_bytes`

## `src/boot_splash.c`

Module implementation.

Functions: **2**

### `boot_splash_decode`

- Location: `src/boot_splash.c:6`
- Linkage: `global`
- Type: `_Bool (const uint8_t *, size_t, boot_splash_emit_fn, void *)`
- Responsibility: Decoding in the boot_splash module.
- Direct callers: `boot_splash_emit_all`
- Direct callees: —

### `boot_splash_emit_all`

- Location: `src/boot_splash.c:21`
- Linkage: `global`
- Type: `_Bool (boot_splash_emit_fn, void *)`
- Responsibility: Public function of the boot_splash module.
- Direct callers: —
- Direct callees: `boot_splash_decode`

## `src/can_router.c`

Central CAN receive routing, telemetry ownership and service ACK arbitration.

Functions: **11**

### `evist_can_router_init`

- Location: `src/can_router.c:6`
- Linkage: `global`
- Type: `void (evist_can_router_t *)`
- Responsibility: Initialization in the can_router module.
- Direct callers: `main`
- Direct callees: `bafang_service_rx_init`, `memset`

### `evist_can_router_rx`

- Location: `src/can_router.c:13`
- Linkage: `global`
- Type: `void (evist_can_router_t *, const bafang_can_frame_t *, uint32_t)`
- Responsibility: Public function of the can_router module.
- Direct callers: `main`
- Direct callees: `bafang_battery_service_apply`, `bafang_build_service_ack`, `bafang_can_id_decode`, `bafang_decode_telemetry`, `bafang_frame_classify`, `bafang_identity_apply_service`, `bafang_service_rx_feed`

### `evist_can_router_log_newest`

- Location: `src/can_router.c:72`
- Linkage: `global`
- Type: `const evist_can_log_entry_t *(const evist_can_router_t *, size_t)`
- Responsibility: Public function of the can_router module.
- Direct callers: —
- Direct callees: —

### `is_read_query`

- Location: `src/can_router.c:80`
- Linkage: `static`
- Type: `_Bool (const bafang_can_frame_t *)`
- Responsibility: Read operation in the can_router module.
- Direct callers: `evist_can_router_tx_allowed`
- Direct callees: `bafang_can_id_decode`

### `is_service_ack`

- Location: `src/can_router.c:85`
- Linkage: `static`
- Type: `_Bool (const bafang_can_frame_t *)`
- Responsibility: Internal helper of the can_router module.
- Direct callers: `evist_can_router_tx_allowed`
- Direct callees: `bafang_can_id_decode`

### `is_hmi_service_reply`

- Location: `src/can_router.c:90`
- Linkage: `static`
- Type: `_Bool (const bafang_can_frame_t *)`
- Responsibility: Internal helper of the can_router module.
- Direct callers: `evist_can_router_tx_allowed`
- Direct callees: `bafang_can_id_decode`

### `is_periodic_hmi`

- Location: `src/can_router.c:99`
- Linkage: `static`
- Type: `_Bool (const bafang_can_frame_t *)`
- Responsibility: Internal helper of the can_router module.
- Direct callers: `evist_can_router_tx_allowed`
- Direct callees: —

### `evist_can_router_tx_allowed`

- Location: `src/can_router.c:108`
- Linkage: `global`
- Type: `_Bool (const evist_can_router_t *, const bafang_can_frame_t *, _Bool)`
- Responsibility: Public function of the can_router module.
- Direct callers: `evist_bafang_config_build_write`, `send_normal`
- Direct callees: `is_hmi_service_reply`, `is_periodic_hmi`, `is_read_query`, `is_service_ack`

### `evist_can_router_take_service_ack`

- Location: `src/can_router.c:120`
- Linkage: `global`
- Type: `_Bool (evist_can_router_t *, bafang_can_frame_t *)`
- Responsibility: Consume-once accessor in the can_router module.
- Direct callers: —
- Direct callees: `evist_can_router_peek_service_ack`, `evist_can_router_pop_service_ack`

### `evist_can_router_peek_service_ack`

- Location: `src/can_router.c:127`
- Linkage: `global`
- Type: `_Bool (const evist_can_router_t *, bafang_can_frame_t *)`
- Responsibility: Public function of the can_router module.
- Direct callers: `evist_can_router_take_service_ack`, `send_service_ack`
- Direct callees: —

### `evist_can_router_pop_service_ack`

- Location: `src/can_router.c:134`
- Linkage: `global`
- Type: `void (evist_can_router_t *)`
- Responsibility: Public function of the can_router module.
- Direct callers: `evist_can_router_take_service_ack`, `send_service_ack`
- Direct callees: —

## `src/controller_write.c`

ACK-gated controller profile write transaction state machine.

Functions: **45**

### `expired`

- Location: `src/controller_write.c:25`
- Linkage: `static`
- Type: `bool (uint32_t, uint32_t)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `evist_ctrl_txn_tick`, `window_closed`
- Direct callees: —

### `copy_bytes`

- Location: `src/controller_write.c:27`
- Linkage: `static`
- Type: `void (uint8_t *, const uint8_t *, uint8_t)`
- Responsibility: Copy helper in the controller_write module.
- Direct callers: `assist_write_frame`, `bafang_battery_service_apply`, `bafang_service_rx_feed`, `cache_block`, `complete_read`, `copy_record_field`, `dpc245_ble_app_apply_advanced_cache`, `dpc245_ble_frame_build`, `dpc245_ble_parser_feed`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_next`, `dpc245_hmi_service_tx_start`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `load_record`, `make_text_reply`, `queue_motor_write`, `send_write`, `write_block`
- Direct callees: —

### `same_bytes`

- Location: `src/controller_write.c:31`
- Linkage: `static`
- Type: `bool (const uint8_t *, const uint8_t *, uint8_t)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `complete_read`
- Direct callees: —

### `is_wheel`

- Location: `src/controller_write.c:36`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `begin`, `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`, `evist_ctrl_txn_tick`, `payload_len`, `send_read`, `send_write`
- Direct callees: —

### `is_start_angle`

- Location: `src/controller_write.c:40`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Start operation in the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`, `payload_len`, `read_sub`, `send_read`
- Direct callees: —

### `is_accel_block`

- Location: `src/controller_write.c:44`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_tick`, `send_read`
- Direct callees: —

### `is_app_write`

- Location: `src/controller_write.c:49`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `evist_ctrl_txn_on_service`, `is_block_write`, `is_write`
- Direct callees: —

### `is_block_write`

- Location: `src/controller_write.c:54`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_tick`
- Direct callees: `is_app_write`

### `read_sub`

- Location: `src/controller_write.c:58`
- Linkage: `static`
- Type: `uint8_t (const evist_ctrl_txn_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `assist_write_frame`, `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`, `send_read`
- Direct callees: `is_start_angle`

### `is_write`

- Location: `src/controller_write.c:65`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `begin`, `complete_read`
- Direct callees: `is_app_write`

### `payload_len`

- Location: `src/controller_write.c:69`
- Linkage: `static`
- Type: `uint8_t (const evist_ctrl_txn_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `complete_read`, `evist_ctrl_txn_take_result`
- Direct callees: `is_start_angle`, `is_wheel`

### `finish`

- Location: `src/controller_write.c:73`
- Linkage: `static`
- Type: `void (evist_ctrl_txn_t *, evist_txn_result_t)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `complete_read`, `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`, `evist_ctrl_txn_tick`, `evist_native_store_step`
- Direct callees: —

### `begin`

- Location: `src/controller_write.c:78`
- Linkage: `static`
- Type: `void (evist_ctrl_txn_t *, evist_txn_kind_t, uint32_t)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `evist_ctrl_txn_discover_accel`, `evist_ctrl_txn_read_accel_p0`, `evist_ctrl_txn_read_accel_p2`, `evist_ctrl_txn_read_assist`, `evist_ctrl_txn_read_start_angle`, `evist_ctrl_txn_read_wheel`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `evist_ctrl_txn_write_wheel`
- Direct callees: `is_wheel`, `is_write`

### `evist_wheel_config_encode`

- Location: `src/controller_write.c:98`
- Linkage: `global`
- Type: `void (const evist_wheel_config_t *, uint8_t *)`
- Responsibility: Encoding in the controller_write module.
- Direct callers: `evist_ctrl_txn_write_wheel`
- Direct callees: —

### `evist_wheel_config_decode`

- Location: `src/controller_write.c:105`
- Linkage: `global`
- Type: `void (const uint8_t *, evist_wheel_config_t *)`
- Responsibility: Decoding in the controller_write module.
- Direct callers: `evist_ride_apply_start_txn`, `wheel_load`
- Direct callees: —

### `evist_ctrl_txn_init`

- Location: `src/controller_write.c:112`
- Linkage: `global`
- Type: `void (evist_ctrl_txn_t *)`
- Responsibility: Initialization in the controller_write module.
- Direct callers: `main`
- Direct callees: —

### `evist_ctrl_txn_busy`

- Location: `src/controller_write.c:126`
- Linkage: `global`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Public function of the controller_write module.
- Direct callers: `can_start`, `dpc245_ble_app_start_controller`, `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`, `evist_ctrl_txn_tick`, `evist_ride_apply_start_txn`
- Direct callees: —

### `can_start`

- Location: `src/controller_write.c:130`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *)`
- Responsibility: Start operation in the controller_write module.
- Direct callers: `evist_ctrl_txn_discover_accel`, `evist_ctrl_txn_read_accel_p0`, `evist_ctrl_txn_read_accel_p2`, `evist_ctrl_txn_read_assist`, `evist_ctrl_txn_read_start_angle`, `evist_ctrl_txn_read_wheel`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `evist_ctrl_txn_write_wheel`
- Direct callees: `evist_ctrl_txn_busy`

### `evist_ctrl_txn_read_wheel`

- Location: `src/controller_write.c:134`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Read operation in the controller_write module.
- Direct callers: `controller_config_service`, `evist_ride_apply_start_txn`
- Direct callees: `begin`, `can_start`

### `evist_ctrl_txn_write_wheel`

- Location: `src/controller_write.c:140`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, const evist_wheel_config_t *, uint32_t)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `controller_config_service`, `evist_ride_apply_start_txn`
- Direct callees: `begin`, `can_start`, `evist_wheel_config_encode`

### `evist_ctrl_txn_read_assist`

- Location: `src/controller_write.c:147`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Read operation in the controller_write module.
- Direct callers: `controller_config_service`, `dpc245_ble_app_start_controller`
- Direct callees: `begin`, `can_start`

### `evist_ctrl_txn_discover_accel`

- Location: `src/controller_write.c:153`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Public function of the controller_write module.
- Direct callers: `controller_config_service`, `dpc245_ble_app_start_controller`
- Direct callees: `begin`, `can_start`

### `evist_ctrl_txn_read_accel_p0`

- Location: `src/controller_write.c:159`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Read operation in the controller_write module.
- Direct callers: `controller_config_service`, `dpc245_ble_app_start_controller`
- Direct callees: `begin`, `can_start`

### `evist_ctrl_txn_read_accel_p2`

- Location: `src/controller_write.c:165`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Read operation in the controller_write module.
- Direct callers: `controller_config_service`, `dpc245_ble_app_start_controller`
- Direct callees: `begin`, `can_start`

### `evist_ctrl_txn_read_start_angle`

- Location: `src/controller_write.c:171`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Read operation in the controller_write module.
- Direct callers: `dpc245_ble_app_start_controller`
- Direct callees: `begin`, `can_start`

### `evist_ctrl_txn_write_assist`

- Location: `src/controller_write.c:177`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, const uint8_t *, evist_firmware_profile_t, uint32_t)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `controller_config_service`
- Direct callees: `begin`, `can_start`, `copy_bytes`, `evist_assist_write_allowed`

### `evist_ctrl_txn_write_app_p1`

- Location: `src/controller_write.c:185`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, const uint8_t *, evist_firmware_profile_t, uint32_t)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `dpc245_ble_app_start_controller`
- Direct callees: `begin`, `can_start`, `copy_bytes`, `evist_assist_write_allowed`

### `evist_ctrl_txn_write_app_p0`

- Location: `src/controller_write.c:198`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, const uint8_t *, uint8_t, uint32_t)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `controller_config_service`, `dpc245_ble_app_start_controller`
- Direct callees: `begin`, `can_start`, `copy_bytes`

### `evist_ctrl_txn_write_app_p2`

- Location: `src/controller_write.c:213`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, const uint8_t *, uint32_t)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `controller_config_service`
- Direct callees: `begin`, `can_start`, `copy_bytes`, `evist_assist_block_valid`

### `controller_id`

- Location: `src/controller_write.c:226`
- Linkage: `static`
- Type: `uint32_t (uint8_t, uint8_t, uint8_t)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `assist_write_frame`, `send_write`
- Direct callees: `bafang_can_id_encode`

### `frame_clear`

- Location: `src/controller_write.c:231`
- Linkage: `static`
- Type: `void (bafang_can_frame_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `dpc245_hmi_service_tx_next`, `dpc245_v3_build_status_frame_1`, `dpc245_v3_build_status_frame_2`, `dpc245_v3_build_status_frame_3`, `dpc245_v3_build_status_frame_4`, `evist_ctrl_txn_next_tx`
- Direct callees: —

### `assist_write_frame`

- Location: `src/controller_write.c:238`
- Linkage: `static`
- Type: `void (const evist_ctrl_txn_t *, uint8_t, bafang_can_frame_t *)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `send_write`
- Direct callees: `controller_id`, `copy_bytes`, `read_sub`

### `send_write`

- Location: `src/controller_write.c:253`
- Linkage: `static`
- Type: `bool (evist_ctrl_txn_t *, uint32_t, bafang_can_frame_t *)`
- Responsibility: Write operation in the controller_write module.
- Direct callers: `evist_ctrl_txn_next_tx`
- Direct callees: `assist_write_frame`, `controller_id`, `copy_bytes`, `is_wheel`

### `send_read`

- Location: `src/controller_write.c:275`
- Linkage: `static`
- Type: `bool (evist_ctrl_txn_t *, uint32_t, bafang_can_frame_t *)`
- Responsibility: Read operation in the controller_write module.
- Direct callers: `evist_ctrl_txn_next_tx`
- Direct callees: `bafang_build_read_query`, `is_accel_block`, `is_start_angle`, `is_wheel`, `read_sub`

### `evist_ctrl_txn_next_tx`

- Location: `src/controller_write.c:301`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, uint32_t, bafang_can_frame_t *)`
- Responsibility: Next-step selection in the controller_write module.
- Direct callers: `controller_config_service`
- Direct callees: `frame_clear`, `send_read`, `send_write`

### `complete_read`

- Location: `src/controller_write.c:309`
- Linkage: `static`
- Type: `void (evist_ctrl_txn_t *, const uint8_t *)`
- Responsibility: Read operation in the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`
- Direct callees: `copy_bytes`, `evist_assist_checksum`, `finish`, `is_write`, `payload_len`, `same_bytes`

### `from_controller_to_us`

- Location: `src/controller_write.c:334`
- Linkage: `static`
- Type: `bool (const bafang_can_id_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`
- Direct callees: —

### `record_error`

- Location: `src/controller_write.c:339`
- Linkage: `static`
- Type: `void (evist_ctrl_txn_t *, const bafang_can_frame_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`
- Direct callees: —

### `validator_reject`

- Location: `src/controller_write.c:345`
- Linkage: `static`
- Type: `bool (const bafang_can_frame_t *)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`
- Direct callees: —

### `window_closed`

- Location: `src/controller_write.c:351`
- Linkage: `static`
- Type: `bool (const evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Internal helper of the controller_write module.
- Direct callers: `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`
- Direct callees: `expired`

### `evist_ctrl_txn_on_frame`

- Location: `src/controller_write.c:356`
- Linkage: `global`
- Type: `void (evist_ctrl_txn_t *, const bafang_can_frame_t *, uint32_t)`
- Responsibility: Public function of the controller_write module.
- Direct callers: `main`
- Direct callees: `bafang_can_id_decode`, `complete_read`, `evist_ctrl_txn_busy`, `finish`, `from_controller_to_us`, `is_accel_block`, `is_block_write`, `is_start_angle`, `is_wheel`, `read_sub`, `record_error`, `validator_reject`, `window_closed`

### `evist_ctrl_txn_on_service`

- Location: `src/controller_write.c:440`
- Linkage: `global`
- Type: `void (evist_ctrl_txn_t *, const bafang_service_message_t *, uint32_t)`
- Responsibility: Public function of the controller_write module.
- Direct callers: `main`
- Direct callees: `complete_read`, `evist_ctrl_txn_busy`, `finish`, `is_app_write`, `is_wheel`, `read_sub`, `window_closed`

### `evist_ctrl_txn_tick`

- Location: `src/controller_write.c:456`
- Linkage: `global`
- Type: `void (evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Periodic step in the controller_write module.
- Direct callers: `controller_config_service`
- Direct callees: `evist_ctrl_txn_busy`, `expired`, `finish`, `is_accel_block`, `is_block_write`, `is_wheel`

### `evist_ctrl_txn_take_result`

- Location: `src/controller_write.c:505`
- Linkage: `global`
- Type: `bool (evist_ctrl_txn_t *, evist_txn_kind_t *, evist_txn_result_t *, uint8_t *)`
- Responsibility: Consume-once accessor in the controller_write module.
- Direct callers: `controller_config_service`
- Direct callees: `payload_len`

### `evist_txn_result_text`

- Location: `src/controller_write.c:517`
- Linkage: `global`
- Type: `const char *(evist_txn_result_t)`
- Responsibility: Public function of the controller_write module.
- Direct callers: `diag_result_text`, `evist_advanced_on_result`
- Direct callees: —

## `src/dpc245_app.c`

Top-level user-interface state machine and user-event ownership.

Functions: **42**

### `wrap_up`

- Location: `src/dpc245_app.c:22`
- Linkage: `static`
- Type: `uint8_t (uint8_t, size_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `accel_key`, `assist_key`, `dpc245_app_key`, `evist_advanced_key`, `preference_key`, `wheel_key`
- Direct callees: —

### `wrap_down`

- Location: `src/dpc245_app.c:26`
- Linkage: `static`
- Type: `uint8_t (uint8_t, size_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `accel_key`, `assist_key`, `dpc245_app_key`, `evist_advanced_key`, `preference_key`, `wheel_key`
- Direct callees: —

### `enter`

- Location: `src/dpc245_app.c:30`
- Linkage: `static`
- Type: `void (dpc245_app_t *, dpc245_app_screen_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `back_to_menu`, `dpc245_app_event`, `dpc245_app_key`, `dpc245_app_tick`, `evist_advanced_key`, `evist_advanced_open_page`, `open_accel`, `open_advanced`, `open_assist`, `open_wheel`
- Direct callees: —

### `return_to`

- Location: `src/dpc245_app.c:36`
- Linkage: `static`
- Type: `void (dpc245_app_t *, dpc245_app_screen_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dirty`

- Location: `src/dpc245_app.c:41`
- Linkage: `static`
- Type: `void (dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_key`
- Direct callees: `dpc245_settings_finalize`

### `leave_themes`

- Location: `src/dpc245_app.c:45`
- Linkage: `static`
- Type: `void (dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_key`, `dpc245_app_tick`
- Direct callees: `dpc245_set_theme`, `dpc245_settings_finalize`, `dpc245_theme`

### `open_advanced`

- Location: `src/dpc245_app.c:56`
- Linkage: `static`
- Type: `void (dpc245_app_t *, evist_adv_screen_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_key`
- Direct callees: `enter`, `evist_advanced_open`, `evist_advanced_open_page`, `evist_advanced_set_ride`

### `return_from_advanced`

- Location: `src/dpc245_app.c:68`
- Linkage: `static`
- Type: `void (dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `next_enabled_panel`

- Location: `src/dpc245_app.c:74`
- Linkage: `static`
- Type: `uint8_t (const dpc245_app_t *, uint8_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_apply_preferences`, `dpc245_app_key`
- Direct callees: —

### `value_edit_active`

- Location: `src/dpc245_app.c:84`
- Linkage: `static`
- Type: `bool (const dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_event`
- Direct callees: —

### `value_edit_repeatable`

- Location: `src/dpc245_app.c:93`
- Linkage: `static`
- Type: `bool (const dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_event`
- Direct callees: —

### `dpc245_app_init`

- Location: `src/dpc245_app.c:122`
- Linkage: `global`
- Type: `void (dpc245_app_t *, const dpc245_settings_t *)`
- Responsibility: Initialization in the dpc245_app module.
- Direct callers: `main`
- Direct callees: `dpc245_runtime_init`, `dpc245_theme`, `evist_advanced_open`, `evist_preferences_defaults`, `evist_ride_defaults`

### `dpc245_app_apply_preferences`

- Location: `src/dpc245_app.c:188`
- Linkage: `global`
- Type: `void (dpc245_app_t *, const evist_preferences_t *)`
- Responsibility: Apply operation in the dpc245_app module.
- Direct callers: `main`
- Direct callees: `evist_advanced_open`, `evist_advanced_set_ride`, `evist_preferences_validate`, `next_enabled_panel`

### `dpc245_app_menu_count`

- Location: `src/dpc245_app.c:198`
- Linkage: `global`
- Type: `size_t (dpc245_app_screen_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `dpc245_app_key`, `draw_menu`, `ui_dashboard_draw`
- Direct callees: —

### `dpc245_app_detail_page_count`

- Location: `src/dpc245_app.c:210`
- Linkage: `global`
- Type: `size_t (dpc245_app_screen_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `dpc245_app_key`, `draw_battery`, `draw_controller`, `draw_sensor`
- Direct callees: —

### `is_paged_info`

- Location: `src/dpc245_app.c:219`
- Linkage: `static`
- Type: `bool (dpc245_app_screen_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dpc245_app_menu_label`

- Location: `src/dpc245_app.c:225`
- Linkage: `global`
- Type: `const char *(dpc245_app_screen_t, size_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `draw_menu`
- Direct callees: —

### `edit_u8_wrap`

- Location: `src/dpc245_app.c:241`
- Linkage: `static`
- Type: `void (uint8_t *, uint8_t, uint8_t, dpc245_app_key_t, bool)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dpc245_app_key`

- Location: `src/dpc245_app.c:247`
- Linkage: `global`
- Type: `void (dpc245_app_t *, dpc245_app_key_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `dpc245_app_event`
- Direct callees: `dirty`, `dpc245_al_sensitivity`, `dpc245_app_detail_page_count`, `dpc245_app_menu_count`, `dpc245_auto_off_min`, `dpc245_brightness_level`, `dpc245_runtime_trip_reset`, `dpc245_service_tips`, `dpc245_set_al_sensitivity`, `dpc245_set_auto_off_min`, `dpc245_set_brightness_level`, `dpc245_set_service_tips`, `dpc245_set_theme`, `dpc245_set_unit`, `dpc245_settings_finalize`, `dpc245_unit`, `edit_u8_wrap`, `enter`, `evist_advanced_key`, `evist_advanced_take_exit`, `evist_advanced_take_preferences`, `evist_advanced_take_ride`, `is_paged_info`, `leave_themes`, `next_enabled_panel`, `open_advanced`, `return_from_advanced`, `return_to`, `wrap_down`, `wrap_up`

### `dpc245_app_dashboard_field_label`

- Location: `src/dpc245_app.c:463`
- Linkage: `global`
- Type: `const char *(dpc245_dashboard_field_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: —
- Direct callees: —

### `ride_assist_zero`

- Location: `src/dpc245_app.c:468`
- Linkage: `static`
- Type: `void (dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `ride_combo_cancel`, `ride_short`
- Direct callees: —

### `ride_walk_busy`

- Location: `src/dpc245_app.c:473`
- Linkage: `static`
- Type: `bool (const dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_event`, `ride_combo_cancel`, `ride_short`
- Direct callees: —

### `ride_effective_offroad`

- Location: `src/dpc245_app.c:477`
- Linkage: `static`
- Type: `bool (const dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_ride_applied`, `ride_short`
- Direct callees: —

### `ride_combo_cancel`

- Location: `src/dpc245_app.c:482`
- Linkage: `static`
- Type: `void (dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_event`, `dpc245_app_tick`, `ride_short`
- Direct callees: `ride_assist_zero`, `ride_walk_busy`

### `ride_short`

- Location: `src/dpc245_app.c:490`
- Linkage: `static`
- Type: `bool (dpc245_app_t *, dpc245_key_event_t, uint32_t)`
- Responsibility: Internal helper of the dpc245_app module.
- Direct callers: `dpc245_app_event`
- Direct callees: `ride_assist_zero`, `ride_combo_cancel`, `ride_effective_offroad`, `ride_walk_busy`

### `dpc245_app_event`

- Location: `src/dpc245_app.c:542`
- Linkage: `global`
- Type: `void (dpc245_app_t *, dpc245_key_event_t, uint32_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `dpc245_app_down_state`, `main`
- Direct callees: `dpc245_app_key`, `enter`, `ride_combo_cancel`, `ride_short`, `ride_walk_busy`, `value_edit_active`, `value_edit_repeatable`

### `dpc245_app_down_state`

- Location: `src/dpc245_app.c:664`
- Linkage: `global`
- Type: `void (dpc245_app_t *, bool, uint32_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `main`
- Direct callees: `dpc245_app_event`

### `dpc245_app_temperature_sample`

- Location: `src/dpc245_app.c:670`
- Linkage: `global`
- Type: `void (dpc245_app_t *, const bafang_telemetry_t *)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `main`
- Direct callees: `bafang_temperature_raw_valid`

### `dpc245_app_dashboard_render_complete`

- Location: `src/dpc245_app.c:682`
- Linkage: `global`
- Type: `void (dpc245_app_t *, const dpc245_app_t *, uint32_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: —
- Direct callees: —

### `temperature_warning_tick`

- Location: `src/dpc245_app.c:691`
- Linkage: `static`
- Type: `void (dpc245_app_t *, uint32_t)`
- Responsibility: Periodic step in the dpc245_app module.
- Direct callers: `dpc245_app_tick`
- Direct callees: —

### `dpc245_app_tick`

- Location: `src/dpc245_app.c:742`
- Linkage: `global`
- Type: `void (dpc245_app_t *, uint32_t)`
- Responsibility: Periodic step in the dpc245_app module.
- Direct callers: `main`
- Direct callees: `enter`, `evist_advanced_abandon`, `leave_themes`, `ride_combo_cancel`, `temperature_warning_tick`

### `dpc245_app_power_graph_tick`

- Location: `src/dpc245_app.c:790`
- Linkage: `global`
- Type: `void (dpc245_app_t *, int32_t, uint32_t)`
- Responsibility: Periodic step in the dpc245_app module.
- Direct callers: —
- Direct callees: —

### `dpc245_app_take_light_toggle`

- Location: `src/dpc245_app.c:805`
- Linkage: `global`
- Type: `bool (dpc245_app_t *)`
- Responsibility: Consume-once accessor in the dpc245_app module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_app_set_ride`

- Location: `src/dpc245_app.c:812`
- Linkage: `global`
- Type: `void (dpc245_app_t *, const evist_ride_cfg_t *)`
- Responsibility: Setter in the dpc245_app module.
- Direct callers: `main`
- Direct callees: `evist_advanced_set_ride`, `evist_ride_boot_mode`, `evist_ride_cfg_valid`

### `dpc245_app_take_ride_cfg_dirty`

- Location: `src/dpc245_app.c:823`
- Linkage: `global`
- Type: `bool (dpc245_app_t *, evist_ride_cfg_t *)`
- Responsibility: Consume-once accessor in the dpc245_app module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_app_take_ride_apply`

- Location: `src/dpc245_app.c:828`
- Linkage: `global`
- Type: `bool (dpc245_app_t *, uint16_t *)`
- Responsibility: Consume-once accessor in the dpc245_app module.
- Direct callers: `main`
- Direct callees: `evist_ride_target_x100`

### `dpc245_app_ride_applied`

- Location: `src/dpc245_app.c:836`
- Linkage: `global`
- Type: `void (dpc245_app_t *, const evist_ride_apply_t *)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `controller_config_service`
- Direct callees: `ride_effective_offroad`

### `dpc245_app_ride_wheel_verified`

- Location: `src/dpc245_app.c:860`
- Linkage: `global`
- Type: `void (dpc245_app_t *, const uint8_t *)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `controller_config_service`
- Direct callees: —

### `dpc245_app_take_preferences_dirty`

- Location: `src/dpc245_app.c:872`
- Linkage: `global`
- Type: `bool (dpc245_app_t *, evist_preferences_t *)`
- Responsibility: Consume-once accessor in the dpc245_app module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_app_take_settings_dirty`

- Location: `src/dpc245_app.c:879`
- Linkage: `global`
- Type: `bool (dpc245_app_t *)`
- Responsibility: Consume-once accessor in the dpc245_app module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_app_theme_store_result`

- Location: `src/dpc245_app.c:886`
- Linkage: `global`
- Type: `void (dpc245_app_t *, bool, uint8_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_app_theme_row_value`

- Location: `src/dpc245_app.c:895`
- Linkage: `global`
- Type: `const char *(const dpc245_app_t *, uint8_t)`
- Responsibility: Public function of the dpc245_app module.
- Direct callers: `draw_menu`
- Direct callees: —

## `src/dpc245_backlight_model.c`

Module implementation.

Functions: **3**

### `clamp_position`

- Location: `src/dpc245_backlight_model.c:4`
- Linkage: `static`
- Type: `uint8_t (uint8_t)`
- Responsibility: Internal helper of the dpc245_backlight_model module.
- Direct callers: `dpc245_backlight_forward_steps`
- Direct callees: —

### `dpc245_backlight_level_to_position`

- Location: `src/dpc245_backlight_model.c:10`
- Linkage: `global`
- Type: `uint8_t (uint8_t)`
- Responsibility: Public function of the dpc245_backlight_model module.
- Direct callers: `dpc245_backlight_set_level`
- Direct callees: —

### `dpc245_backlight_forward_steps`

- Location: `src/dpc245_backlight_model.c:16`
- Linkage: `global`
- Type: `uint8_t (uint8_t, uint8_t)`
- Responsibility: Public function of the dpc245_backlight_model module.
- Direct callers: `set_position_internal`
- Direct callees: `clamp_position`

## `src/dpc245_ble_app.c`

Module implementation.

Functions: **28**

### `zero_bytes`

- Location: `src/dpc245_ble_app.c:9`
- Linkage: `static`
- Type: `void (uint8_t *, size_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_build_periodic`, `dpc245_ble_app_handle`, `dpc245_ble_app_init`
- Direct callees: —

### `copy_bytes`

- Location: `src/dpc245_ble_app.c:13`
- Linkage: `static`
- Type: `void (uint8_t *, const uint8_t *, size_t)`
- Responsibility: Copy helper in the dpc245_ble_app module.
- Direct callers: `assist_write_frame`, `bafang_battery_service_apply`, `bafang_service_rx_feed`, `cache_block`, `complete_read`, `copy_record_field`, `dpc245_ble_app_apply_advanced_cache`, `dpc245_ble_frame_build`, `dpc245_ble_parser_feed`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_next`, `dpc245_hmi_service_tx_start`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `load_record`, `make_text_reply`, `queue_motor_write`, `send_write`, `write_block`
- Direct callees: —

### `copy_text_field`

- Location: `src/dpc245_ble_app.c:17`
- Linkage: `static`
- Type: `void (uint8_t *, size_t, const char *)`
- Responsibility: Copy helper in the dpc245_ble_app module.
- Direct callers: `refresh_a3`, `refresh_a5`
- Direct callees: —

### `copy_record_field`

- Location: `src/dpc245_ble_app.c:26`
- Linkage: `static`
- Type: `void (uint8_t *, size_t, const uint8_t *, uint8_t)`
- Responsibility: Copy helper in the dpc245_ble_app module.
- Direct callers: `refresh_a5`
- Direct callees: `copy_bytes`

### `put_u16le`

- Location: `src/dpc245_ble_app.c:34`
- Linkage: `static`
- Type: `void (uint8_t *, uint16_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_build_periodic`, `put_i16le`, `refresh_a3`
- Direct callees: —

### `put_i16le`

- Location: `src/dpc245_ble_app.c:39`
- Linkage: `static`
- Type: `void (uint8_t *, int16_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `refresh_a3`
- Direct callees: `put_u16le`

### `put_u32le`

- Location: `src/dpc245_ble_app.c:43`
- Linkage: `static`
- Type: `void (uint8_t *, uint32_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_build_periodic`
- Direct callees: —

### `temp_style`

- Location: `src/dpc245_ble_app.c:50`
- Linkage: `static`
- Type: `uint8_t (int16_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `refresh_a3`
- Direct callees: —

### `build_reply`

- Location: `src/dpc245_ble_app.c:57`
- Linkage: `static`
- Type: `size_t (dpc245_ble_app_result_t *, const dpc245_ble_frame_t *, uint8_t, const uint8_t *, uint8_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `handle_10`, `handle_a5`, `read_block`, `write_block`
- Direct callees: `dpc245_ble_frame_build`

### `assist_wire`

- Location: `src/dpc245_ble_app.c:72`
- Linkage: `static`
- Type: `uint8_t (const dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `refresh_a5`
- Direct callees: `bafang_assist_code`

### `refresh_a3`

- Location: `src/dpc245_ble_app.c:78`
- Linkage: `static`
- Type: `void (dpc245_ble_app_t *, const dpc245_app_t *, const evist_can_router_t *)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_handle`
- Direct callees: `bafang_temperature_raw_valid`, `copy_text_field`, `put_i16le`, `put_u16le`, `temp_style`

### `refresh_a5`

- Location: `src/dpc245_ble_app.c:121`
- Linkage: `static`
- Type: `void (dpc245_ble_app_t *, const dpc245_app_t *, const dpc245_hmi_identity_records_t *, _Bool)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `handle_a5`
- Direct callees: `assist_wire`, `copy_record_field`, `copy_text_field`, `dpc245_auto_off_min`, `dpc245_get_u16_le`, `dpc245_get_u32_le`, `dpc245_unit`

### `refresh_a9`

- Location: `src/dpc245_ble_app.c:161`
- Linkage: `static`
- Type: `void (dpc245_ble_app_t *, const dpc245_app_t *)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_handle`
- Direct callees: —

### `queue_motor_write`

- Location: `src/dpc245_ble_app.c:187`
- Linkage: `static`
- Type: `_Bool (dpc245_ble_app_t *, uint8_t, evist_firmware_profile_t)`
- Responsibility: Write operation in the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_handle`
- Direct callees: `copy_bytes`, `evist_assist_checksum`, `evist_assist_write_allowed`

### `dpc245_ble_app_start_controller`

- Location: `src/dpc245_ble_app.c:214`
- Linkage: `global`
- Type: `_Bool (dpc245_ble_app_t *, evist_ctrl_txn_t *, evist_firmware_profile_t, uint32_t)`
- Responsibility: Start operation in the dpc245_ble_app module.
- Direct callers: `controller_config_service`
- Direct callees: `evist_ctrl_txn_busy`, `evist_ctrl_txn_discover_accel`, `evist_ctrl_txn_read_accel_p0`, `evist_ctrl_txn_read_accel_p2`, `evist_ctrl_txn_read_assist`, `evist_ctrl_txn_read_start_angle`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`

### `cache_block`

- Location: `src/dpc245_ble_app.c:254`
- Linkage: `static`
- Type: `void (dpc245_ble_app_t *, evist_txn_kind_t, const uint8_t *)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_capture_adv_result`, `dpc245_ble_app_on_controller_result`
- Direct callees: `copy_bytes`

### `dpc245_ble_app_on_controller_result`

- Location: `src/dpc245_ble_app.c:279`
- Linkage: `global`
- Type: `void (dpc245_ble_app_t *, evist_txn_kind_t, evist_txn_result_t, const uint8_t *)`
- Responsibility: Public function of the dpc245_ble_app module.
- Direct callers: `controller_config_service`
- Direct callees: `cache_block`

### `dpc245_ble_app_capture_adv_result`

- Location: `src/dpc245_ble_app.c:309`
- Linkage: `global`
- Type: `void (dpc245_ble_app_t *, evist_txn_kind_t, evist_txn_result_t, const uint8_t *)`
- Responsibility: Public function of the dpc245_ble_app module.
- Direct callers: `controller_config_service`
- Direct callees: `cache_block`

### `dpc245_ble_app_apply_advanced_cache`

- Location: `src/dpc245_ble_app.c:323`
- Linkage: `global`
- Type: `void (const dpc245_ble_app_t *, evist_advanced_t *)`
- Responsibility: Apply operation in the dpc245_ble_app module.
- Direct callers: `main`
- Direct callees: `copy_bytes`

### `requested_chunk_len`

- Location: `src/dpc245_ble_app.c:348`
- Linkage: `static`
- Type: `uint8_t (const dpc245_ble_frame_t *, size_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `read_block`
- Direct callees: —

### `read_block`

- Location: `src/dpc245_ble_app.c:359`
- Linkage: `static`
- Type: `_Bool (dpc245_ble_app_result_t *, const dpc245_ble_frame_t *, const uint8_t *, size_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_handle`, `handle_a5`
- Direct callees: `build_reply`, `requested_chunk_len`

### `write_block`

- Location: `src/dpc245_ble_app.c:367`
- Linkage: `static`
- Type: `_Bool (dpc245_ble_app_result_t *, const dpc245_ble_frame_t *, uint8_t *, size_t)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_handle`
- Direct callees: `build_reply`, `copy_bytes`

### `handle_10`

- Location: `src/dpc245_ble_app.c:379`
- Linkage: `static`
- Type: `_Bool (dpc245_ble_app_t *, const dpc245_ble_frame_t *, dpc245_app_t *, dpc245_ble_app_result_t *)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_handle`
- Direct callees: `build_reply`, `dpc245_settings_finalize`

### `handle_a5`

- Location: `src/dpc245_ble_app.c:427`
- Linkage: `static`
- Type: `_Bool (dpc245_ble_app_t *, const dpc245_ble_frame_t *, dpc245_app_t *, const dpc245_hmi_identity_records_t *, _Bool, dpc245_update_t *, dpc245_ble_app_result_t *)`
- Responsibility: Internal helper of the dpc245_ble_app module.
- Direct callers: `dpc245_ble_app_handle`
- Direct callees: `build_reply`, `dpc245_update_request`, `read_block`, `refresh_a5`

### `dpc245_ble_app_init`

- Location: `src/dpc245_ble_app.c:466`
- Linkage: `global`
- Type: `void (dpc245_ble_app_t *)`
- Responsibility: Initialization in the dpc245_ble_app module.
- Direct callers: `main`
- Direct callees: `zero_bytes`

### `dpc245_ble_app_handle`

- Location: `src/dpc245_ble_app.c:471`
- Linkage: `global`
- Type: `_Bool (dpc245_ble_app_t *, const dpc245_ble_frame_t *, dpc245_app_t *, evist_can_router_t *, const dpc245_hmi_identity_records_t *, _Bool, dpc245_update_t *, dpc245_ble_app_result_t *)`
- Responsibility: Public function of the dpc245_ble_app module.
- Direct callers: `main`
- Direct callees: `handle_10`, `handle_a5`, `queue_motor_write`, `read_block`, `refresh_a3`, `refresh_a9`, `write_block`, `zero_bytes`

### `dpc245_ble_app_build_periodic`

- Location: `src/dpc245_ble_app.c:601`
- Linkage: `global`
- Type: `size_t (uint8_t *, size_t, const dpc245_app_t *, const evist_can_router_t *, _Bool)`
- Responsibility: Frame or record construction in the dpc245_ble_app module.
- Direct callers: `main`
- Direct callees: `dpc245_ble_frame_build`, `dpc245_get_u16_le`, `dpc245_get_u32_le`, `dpc245_runtime_calories_kcal`, `dpc245_unit`, `put_u16le`, `put_u32le`, `zero_bytes`

### `dpc245_ble_app_take_hmi_event_bits`

- Location: `src/dpc245_ble_app.c:636`
- Linkage: `global`
- Type: `uint8_t (dpc245_ble_app_t *)`
- Responsibility: Consume-once accessor in the dpc245_ble_app module.
- Direct callers: `main`
- Direct callees: —

## `src/dpc245_ble_proto.c`

BLE UART packet framing, checksum and parser state machine.

Functions: **6**

### `dpc245_ble_checksum`

- Location: `src/dpc245_ble_proto.c:4`
- Linkage: `global`
- Type: `uint16_t (const uint8_t *, size_t)`
- Responsibility: Checksum calculation or check in the dpc245_ble_proto module.
- Direct callers: `dpc245_ble_frame_build`, `dpc245_ble_parser_feed`
- Direct callees: —

### `copy_bytes`

- Location: `src/dpc245_ble_proto.c:11`
- Linkage: `static`
- Type: `void (uint8_t *, const uint8_t *, size_t)`
- Responsibility: Copy helper in the dpc245_ble_proto module.
- Direct callers: `assist_write_frame`, `bafang_battery_service_apply`, `bafang_service_rx_feed`, `cache_block`, `complete_read`, `copy_record_field`, `dpc245_ble_app_apply_advanced_cache`, `dpc245_ble_frame_build`, `dpc245_ble_parser_feed`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_next`, `dpc245_hmi_service_tx_start`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `load_record`, `make_text_reply`, `queue_motor_write`, `send_write`, `write_block`
- Direct callees: —

### `dpc245_ble_parser_init`

- Location: `src/dpc245_ble_proto.c:15`
- Linkage: `global`
- Type: `void (dpc245_ble_parser_t *)`
- Responsibility: Initialization in the dpc245_ble_proto module.
- Direct callers: `dpc245_preflight_begin`, `main`
- Direct callees: —

### `parser_reset`

- Location: `src/dpc245_ble_proto.c:21`
- Linkage: `static`
- Type: `void (dpc245_ble_parser_t *)`
- Responsibility: Internal helper of the dpc245_ble_proto module.
- Direct callers: `dpc245_ble_parser_feed`
- Direct callees: —

### `dpc245_ble_parser_feed`

- Location: `src/dpc245_ble_proto.c:26`
- Linkage: `global`
- Type: `bool (dpc245_ble_parser_t *, uint8_t, dpc245_ble_frame_t *)`
- Responsibility: Input handling in the dpc245_ble_proto module.
- Direct callers: `dpc245_preflight_poll`, `main`
- Direct callees: `copy_bytes`, `dpc245_ble_checksum`, `parser_reset`

### `dpc245_ble_frame_build`

- Location: `src/dpc245_ble_proto.c:97`
- Linkage: `global`
- Type: `size_t (uint8_t *, size_t, uint8_t, uint8_t, uint8_t, uint8_t, const uint8_t *, uint8_t)`
- Responsibility: Frame or record construction in the dpc245_ble_proto module.
- Direct callers: `build_reply`, `dpc245_ble_app_build_periodic`
- Direct callees: `copy_bytes`, `dpc245_ble_checksum`

## `src/dpc245_clock_tree.c`

Module implementation.

Functions: **4**

### `dpc245_clock_classify`

- Location: `src/dpc245_clock_tree.c:28`
- Linkage: `global`
- Type: `dpc245_clock_tree_t (uint32_t, uint32_t, uint32_t)`
- Responsibility: Public function of the dpc245_clock_tree module.
- Direct callers: `classify_now`, `dpc245_clock_tree_now`
- Direct callees: —

### `dpc245_clock_tree_core_hz`

- Location: `src/dpc245_clock_tree.c:45`
- Linkage: `global`
- Type: `uint32_t (dpc245_clock_tree_t)`
- Responsibility: Public function of the dpc245_clock_tree module.
- Direct callers: `Reset_Handler`
- Direct callees: —

### `classify_now`

- Location: `src/dpc245_clock_tree.c:51`
- Linkage: `static`
- Type: `dpc245_clock_tree_t (const dpc245_rcu_regs_t *)`
- Responsibility: Internal helper of the dpc245_clock_tree module.
- Direct callers: `dpc245_clock_force_hsi`
- Direct callees: `dpc245_clock_classify`

### `dpc245_clock_force_hsi`

- Location: `src/dpc245_clock_tree.c:55`
- Linkage: `global`
- Type: `dpc245_clock_tree_t (const dpc245_rcu_regs_t *, uint32_t)`
- Responsibility: Public function of the dpc245_clock_tree module.
- Direct callers: `clock_force_hsi8`, `dpc245_clock_fallback_hsi`
- Direct callees: `classify_now`

## `src/dpc245_hmi_control.c`

Module implementation.

Functions: **5**

### `u24le`

- Location: `src/dpc245_hmi_control.c:7`
- Linkage: `static`
- Type: `uint32_t (const uint8_t *)`
- Responsibility: Internal helper of the dpc245_hmi_control module.
- Direct callers: `dpc245_hmi_control_process`
- Direct callees: —

### `make_ack`

- Location: `src/dpc245_hmi_control.c:11`
- Linkage: `static`
- Type: `void (const bafang_can_id_t *, const bafang_can_frame_t *, bafang_can_frame_t *, _Bool)`
- Responsibility: Internal helper of the dpc245_hmi_control module.
- Direct callers: `dpc245_hmi_control_process`
- Direct callees: `bafang_can_id_encode`, `memset`

### `put_u24le`

- Location: `src/dpc245_hmi_control.c:25`
- Linkage: `static`
- Type: `void (uint8_t *, uint32_t)`
- Responsibility: Internal helper of the dpc245_hmi_control module.
- Direct callers: `dpc245_hmi_control_build_read_reply`
- Direct callees: —

### `dpc245_hmi_control_build_read_reply`

- Location: `src/dpc245_hmi_control.c:31`
- Linkage: `global`
- Type: `_Bool (const bafang_can_frame_t *, const dpc245_runtime_t *, bafang_can_frame_t *)`
- Responsibility: Read operation in the dpc245_hmi_control module.
- Direct callers: `main`
- Direct callees: `bafang_can_id_decode`, `bafang_can_id_encode`, `dpc245_get_u16_le`, `dpc245_get_u32_le`, `memset`, `put_u24le`

### `dpc245_hmi_control_process`

- Location: `src/dpc245_hmi_control.c:74`
- Linkage: `global`
- Type: `_Bool (const bafang_can_frame_t *, dpc245_runtime_t *, const dpc245_rtc_io_t *, dpc245_hmi_control_result_t *)`
- Responsibility: Public function of the dpc245_hmi_control module.
- Direct callers: `main`
- Direct callees: `bafang_can_id_decode`, `dpc245_rtc_set_time`, `dpc245_set_u16_le`, `dpc245_set_u32_le`, `dpc245_settings_finalize`, `make_ack`, `memset`, `u24le`

## `src/dpc245_hmi_service.c`

HMI service identity replies and long-transfer transmit state machine.

Functions: **14**

### `copy_bytes`

- Location: `src/dpc245_hmi_service.c:6`
- Linkage: `static`
- Type: `void (uint8_t *, const uint8_t *, uint8_t)`
- Responsibility: Copy helper in the dpc245_hmi_service module.
- Direct callers: `assist_write_frame`, `bafang_battery_service_apply`, `bafang_service_rx_feed`, `cache_block`, `complete_read`, `copy_record_field`, `dpc245_ble_app_apply_advanced_cache`, `dpc245_ble_frame_build`, `dpc245_ble_parser_feed`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_next`, `dpc245_hmi_service_tx_start`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p1`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `load_record`, `make_text_reply`, `queue_motor_write`, `send_write`, `write_block`
- Direct callees: —

### `text_len`

- Location: `src/dpc245_hmi_service.c:10`
- Linkage: `static`
- Type: `uint8_t (const char *)`
- Responsibility: Internal helper of the dpc245_hmi_service module.
- Direct callers: `make_text_reply`
- Direct callees: —

### `dpc245_hmi_identity_records_init`

- Location: `src/dpc245_hmi_service.c:16`
- Linkage: `global`
- Type: `void (dpc245_hmi_identity_records_t *)`
- Responsibility: Initialization in the dpc245_hmi_service module.
- Direct callers: `dpc245_hmi_identity_records_load`, `main`
- Direct callees: `memset`

### `load_record`

- Location: `src/dpc245_hmi_service.c:20`
- Linkage: `static`
- Type: `bool (const dpc245_persist_io_t *, uint32_t, uint8_t *, uint8_t *)`
- Responsibility: Internal helper of the dpc245_hmi_service module.
- Direct callers: `dpc245_hmi_identity_records_load`
- Direct callees: `copy_bytes`

### `dpc245_hmi_identity_records_load`

- Location: `src/dpc245_hmi_service.c:36`
- Linkage: `global`
- Type: `bool (const dpc245_persist_io_t *, dpc245_hmi_identity_records_t *)`
- Responsibility: Load operation in the dpc245_hmi_service module.
- Direct callers: `main`
- Direct callees: `dpc245_hmi_identity_records_init`, `load_record`

### `make_text_reply`

- Location: `src/dpc245_hmi_service.c:45`
- Linkage: `static`
- Type: `bool (dpc245_hmi_service_reply_t *, const char *)`
- Responsibility: Internal helper of the dpc245_hmi_service module.
- Direct callers: `dpc245_hmi_service_build_read_reply`
- Direct callees: `copy_bytes`, `text_len`

### `hex_digit`

- Location: `src/dpc245_hmi_service.c:52`
- Linkage: `static`
- Type: `uint8_t (uint8_t)`
- Responsibility: Internal helper of the dpc245_hmi_service module.
- Direct callers: `dpc245_hmi_service_build_read_reply`, `ui_hex32`, `ui_hex8`
- Direct callees: —

### `dpc245_hmi_service_build_read_reply`

- Location: `src/dpc245_hmi_service.c:58`
- Linkage: `global`
- Type: `bool (const bafang_can_frame_t *, const dpc245_hmi_identity_records_t *, const dpc245_settings_t *, dpc245_hmi_service_reply_t *)`
- Responsibility: Read operation in the dpc245_hmi_service module.
- Direct callers: `main`
- Direct callees: `bafang_can_id_decode`, `copy_bytes`, `dpc245_error_at`, `dpc245_error_count`, `hex_digit`, `make_text_reply`, `memset`

### `dpc245_hmi_service_tx_init`

- Location: `src/dpc245_hmi_service.c:111`
- Linkage: `global`
- Type: `void (dpc245_hmi_service_tx_t *)`
- Responsibility: Initialization in the dpc245_hmi_service module.
- Direct callers: `main`
- Direct callees: `memset`

### `dpc245_hmi_service_tx_start`

- Location: `src/dpc245_hmi_service.c:115`
- Linkage: `global`
- Type: `bool (dpc245_hmi_service_tx_t *, const dpc245_hmi_service_reply_t *)`
- Responsibility: Start operation in the dpc245_hmi_service module.
- Direct callers: `main`
- Direct callees: `copy_bytes`, `memset`

### `frame_clear`

- Location: `src/dpc245_hmi_service.c:130`
- Linkage: `static`
- Type: `void (bafang_can_frame_t *)`
- Responsibility: Internal helper of the dpc245_hmi_service module.
- Direct callers: `dpc245_hmi_service_tx_next`, `dpc245_v3_build_status_frame_1`, `dpc245_v3_build_status_frame_2`, `dpc245_v3_build_status_frame_3`, `dpc245_v3_build_status_frame_4`, `evist_ctrl_txn_next_tx`
- Direct callees: —

### `make_id`

- Location: `src/dpc245_hmi_service.c:135`
- Linkage: `static`
- Type: `void (bafang_can_frame_t *, uint8_t, uint8_t, uint8_t, uint8_t)`
- Responsibility: Internal helper of the dpc245_hmi_service module.
- Direct callers: `dpc245_hmi_service_tx_next`
- Direct callees: `bafang_can_id_encode`

### `dpc245_hmi_service_tx_next`

- Location: `src/dpc245_hmi_service.c:142`
- Linkage: `global`
- Type: `bool (dpc245_hmi_service_tx_t *, bafang_can_frame_t *)`
- Responsibility: Next-step selection in the dpc245_hmi_service module.
- Direct callers: `main`
- Direct callees: `copy_bytes`, `frame_clear`, `make_id`

### `dpc245_hmi_service_tx_on_ack`

- Location: `src/dpc245_hmi_service.c:195`
- Linkage: `global`
- Type: `bool (dpc245_hmi_service_tx_t *, const bafang_can_frame_t *)`
- Responsibility: Public function of the dpc245_hmi_service module.
- Direct callers: `main`
- Direct callees: `bafang_can_id_decode`

## `src/dpc245_key_sampler.c`

Clock-driven key sampling and timestamped event queue.

Functions: **4**

### `dpc245_key_time_for_pass`

- Location: `src/dpc245_key_sampler.c:7`
- Linkage: `global`
- Type: `uint32_t (uint32_t, uint32_t)`
- Responsibility: Public function of the dpc245_key_sampler module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_key_sampler_init`

- Location: `src/dpc245_key_sampler.c:11`
- Linkage: `global`
- Type: `void (dpc245_key_sampler_t *, uint8_t, uint32_t)`
- Responsibility: Initialization in the dpc245_key_sampler module.
- Direct callers: —
- Direct callees: `dpc245_keys_init`

### `dpc245_key_sampler_tick`

- Location: `src/dpc245_key_sampler.c:21`
- Linkage: `global`
- Type: `void (dpc245_key_sampler_t *, uint8_t, uint32_t)`
- Responsibility: Periodic step in the dpc245_key_sampler module.
- Direct callers: `dpc245_buttons_tick_1ms`
- Direct callees: `dpc245_keys_update`

### `dpc245_key_sampler_pop`

- Location: `src/dpc245_key_sampler.c:54`
- Linkage: `global`
- Type: `bool (dpc245_key_sampler_t *, dpc245_key_sample_event_t *)`
- Responsibility: Public function of the dpc245_key_sampler module.
- Direct callers: `dpc245_buttons_key_event`
- Direct callees: —

## `src/dpc245_keys.c`

Debounced key gesture interpretation and Walk/settings event generation.

Functions: **6**

### `dpc245_raw_key_code`

- Location: `src/dpc245_keys.c:4`
- Linkage: `global`
- Type: `uint8_t (uint8_t)`
- Responsibility: Public function of the dpc245_keys module.
- Direct callers: `dpc245_keys_init`, `dpc245_keys_update`
- Direct callees: —

### `dpc245_translate_appkey`

- Location: `src/dpc245_keys.c:16`
- Linkage: `global`
- Type: `uint8_t (uint8_t, uint8_t, _Bool, _Bool)`
- Responsibility: Public function of the dpc245_keys module.
- Direct callers: —
- Direct callees: —

### `elapsed`

- Location: `src/dpc245_keys.c:40`
- Linkage: `static`
- Type: `uint32_t (uint32_t, uint32_t)`
- Responsibility: Internal helper of the dpc245_keys module.
- Direct callers: `dpc245_keys_update`
- Direct callees: —

### `released_event`

- Location: `src/dpc245_keys.c:44`
- Linkage: `static`
- Type: `uint32_t (uint8_t, uint32_t, _Bool)`
- Responsibility: Internal helper of the dpc245_keys module.
- Direct callers: `dpc245_keys_update`
- Direct callees: —

### `dpc245_keys_init`

- Location: `src/dpc245_keys.c:53`
- Linkage: `global`
- Type: `void (dpc245_keys_t *, uint8_t, uint32_t)`
- Responsibility: Initialization in the dpc245_keys module.
- Direct callers: `dpc245_key_sampler_init`
- Direct callees: `dpc245_raw_key_code`

### `dpc245_keys_update`

- Location: `src/dpc245_keys.c:69`
- Linkage: `global`
- Type: `uint32_t (dpc245_keys_t *, uint8_t, uint32_t)`
- Responsibility: State update in the dpc245_keys module.
- Direct callers: `dpc245_key_sampler_tick`
- Direct callees: `dpc245_raw_key_code`, `elapsed`, `released_event`

## `src/dpc245_persist.c`

Persistent settings journal selection, loading and atomic append.

Functions: **5**

### `read_u16_le`

- Location: `src/dpc245_persist.c:4`
- Linkage: `static`
- Type: `_Bool (const dpc245_persist_io_t *, uint32_t, uint16_t *)`
- Responsibility: Internal helper of the dpc245_persist module.
- Direct callers: `dpc245_persist_find_latest`, `dpc245_persist_load_checked`
- Direct callees: —

### `dpc245_persist_find_latest`

- Location: `src/dpc245_persist.c:11`
- Linkage: `global`
- Type: `uint16_t (const dpc245_persist_io_t *, dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_persist module.
- Direct callers: `dpc245_persist_load`, `dpc245_persist_save`
- Direct callees: `dpc245_settings_validate`, `read_u16_le`

### `dpc245_persist_load`

- Location: `src/dpc245_persist.c:30`
- Linkage: `global`
- Type: `uint8_t (const dpc245_persist_io_t *, dpc245_settings_t *)`
- Responsibility: Load operation in the dpc245_persist module.
- Direct callers: —
- Direct callees: `dpc245_persist_find_latest`, `dpc245_settings_defaults`

### `dpc245_persist_load_checked`

- Location: `src/dpc245_persist.c:43`
- Linkage: `global`
- Type: `_Bool (const dpc245_persist_io_t *, dpc245_settings_t *, _Bool *)`
- Responsibility: Load operation in the dpc245_persist module.
- Direct callers: `evist_native_store_init`
- Direct callees: `dpc245_settings_defaults`, `dpc245_settings_validate`, `read_u16_le`

### `dpc245_persist_save`

- Location: `src/dpc245_persist.c:66`
- Linkage: `global`
- Type: `_Bool (const dpc245_persist_io_t *, dpc245_settings_t *, uint8_t *)`
- Responsibility: Save operation in the dpc245_persist module.
- Direct callers: —
- Direct callees: `dpc245_persist_find_latest`, `dpc245_settings_finalize`

## `src/dpc245_recovery_trigger.c`

Module implementation.

Functions: **5**

### `dpc245_is_can_update_request`

- Location: `src/dpc245_recovery_trigger.c:7`
- Linkage: `global`
- Type: `_Bool (const bafang_can_frame_t *)`
- Responsibility: State update in the dpc245_recovery_trigger module.
- Direct callers: `dpc245_preflight_poll`, `main`
- Direct callees: `bafang_can_id_decode`

### `dpc245_is_ble_update_request`

- Location: `src/dpc245_recovery_trigger.c:19`
- Linkage: `global`
- Type: `_Bool (const dpc245_ble_frame_t *)`
- Responsibility: State update in the dpc245_recovery_trigger module.
- Direct callers: `dpc245_preflight_poll`
- Direct callees: —

### `dpc245_recovery_chord_init`

- Location: `src/dpc245_recovery_trigger.c:27`
- Linkage: `global`
- Type: `void (dpc245_recovery_chord_t *)`
- Responsibility: Initialization in the dpc245_recovery_trigger module.
- Direct callers: `dpc245_preflight_begin`
- Direct callees: —

### `dpc245_recovery_chord_update`

- Location: `src/dpc245_recovery_trigger.c:33`
- Linkage: `global`
- Type: `_Bool (dpc245_recovery_chord_t *, uint8_t, uint32_t)`
- Responsibility: State update in the dpc245_recovery_trigger module.
- Direct callers: —
- Direct callees: —

### `dpc245_marker_clear_retry`

- Location: `src/dpc245_recovery_trigger.c:48`
- Linkage: `global`
- Type: `uint8_t (_Bool (*)(void), uint8_t)`
- Responsibility: Public function of the dpc245_recovery_trigger module.
- Direct callers: —
- Direct callees: —

## `src/dpc245_rtc.c`

Module implementation.

Functions: **7**

### `dpc245_rtc_bcd_encode`

- Location: `src/dpc245_rtc.c:10`
- Linkage: `global`
- Type: `uint8_t (uint8_t)`
- Responsibility: Encoding in the dpc245_rtc module.
- Direct callers: `dpc245_rtc_write`
- Direct callees: —

### `dpc245_rtc_bcd_decode`

- Location: `src/dpc245_rtc.c:14`
- Linkage: `global`
- Type: `_Bool (uint8_t, uint8_t *)`
- Responsibility: Decoding in the dpc245_rtc module.
- Direct callers: `dpc245_rtc_read`
- Direct callees: —

### `dpc245_rtc_datetime_validate`

- Location: `src/dpc245_rtc.c:23`
- Linkage: `global`
- Type: `_Bool (const dpc245_rtc_datetime_t *)`
- Responsibility: Validation in the dpc245_rtc module.
- Direct callers: `dpc245_rtc_read`, `dpc245_rtc_write`
- Direct callees: —

### `field_ptr`

- Location: `src/dpc245_rtc.c:31`
- Linkage: `static`
- Type: `uint8_t *(dpc245_rtc_datetime_t *, uint8_t)`
- Responsibility: Internal helper of the dpc245_rtc module.
- Direct callers: `dpc245_rtc_read`
- Direct callees: —

### `dpc245_rtc_read`

- Location: `src/dpc245_rtc.c:43`
- Linkage: `global`
- Type: `_Bool (const dpc245_rtc_io_t *, dpc245_rtc_datetime_t *)`
- Responsibility: Read operation in the dpc245_rtc module.
- Direct callers: `dpc245_rtc_set_time`
- Direct callees: `dpc245_rtc_bcd_decode`, `dpc245_rtc_datetime_validate`, `field_ptr`

### `dpc245_rtc_write`

- Location: `src/dpc245_rtc.c:58`
- Linkage: `global`
- Type: `_Bool (const dpc245_rtc_io_t *, const dpc245_rtc_datetime_t *)`
- Responsibility: Write operation in the dpc245_rtc module.
- Direct callers: `dpc245_rtc_set_time`
- Direct callees: `dpc245_rtc_bcd_encode`, `dpc245_rtc_datetime_validate`

### `dpc245_rtc_set_time`

- Location: `src/dpc245_rtc.c:85`
- Linkage: `global`
- Type: `_Bool (const dpc245_rtc_io_t *, uint8_t, uint8_t, uint8_t)`
- Responsibility: Setter in the dpc245_rtc module.
- Direct callers: `dpc245_hmi_control_process`
- Direct callees: `dpc245_rtc_read`, `dpc245_rtc_write`

## `src/dpc245_runtime.c`

Ride statistics, service counters and telemetry-derived runtime state.

Functions: **10**

### `add_u32`

- Location: `src/dpc245_runtime.c:5`
- Linkage: `static`
- Type: `void (dpc245_settings_t *, uint8_t, uint32_t)`
- Responsibility: Internal helper of the dpc245_runtime module.
- Direct callers: `dpc245_runtime_on_can`, `dpc245_runtime_tick_500ms`
- Direct callees: `dpc245_get_u32_le`, `dpc245_set_u32_le`

### `dpc245_runtime_init`

- Location: `src/dpc245_runtime.c:12`
- Linkage: `global`
- Type: `void (dpc245_runtime_t *, const dpc245_settings_t *)`
- Responsibility: Initialization in the dpc245_runtime module.
- Direct callers: `dpc245_app_init`
- Direct callees: `dpc245_settings_defaults`, `update_service_due`

### `update_service_due`

- Location: `src/dpc245_runtime.c:28`
- Linkage: `static`
- Type: `void (dpc245_runtime_t *)`
- Responsibility: Internal helper of the dpc245_runtime module.
- Direct callers: `dpc245_runtime_init`, `dpc245_runtime_on_can`
- Direct callees: `dpc245_get_u32_le`, `dpc245_set_u32_le`

### `dpc245_runtime_on_can`

- Location: `src/dpc245_runtime.c:37`
- Linkage: `global`
- Type: `bool (dpc245_runtime_t *, const bafang_can_frame_t *)`
- Responsibility: CAN frame handler in the dpc245_runtime module.
- Direct callers: `main`
- Direct callees: `add_u32`, `bafang_can_id_decode`, `dpc245_error_push`, `dpc245_get_u16_le`, `dpc245_get_u32_le`, `dpc245_set_u16_le`, `update_service_due`

### `dpc245_runtime_service_due`

- Location: `src/dpc245_runtime.c:133`
- Linkage: `global`
- Type: `bool (const dpc245_runtime_t *)`
- Responsibility: Public function of the dpc245_runtime module.
- Direct callers: —
- Direct callees: —

### `dpc245_runtime_service_tip_active`

- Location: `src/dpc245_runtime.c:137`
- Linkage: `global`
- Type: `bool (const dpc245_runtime_t *)`
- Responsibility: Public function of the dpc245_runtime module.
- Direct callers: `draw_dashboard`
- Direct callees: `dpc245_service_tips`

### `dpc245_runtime_trip_reset`

- Location: `src/dpc245_runtime.c:141`
- Linkage: `global`
- Type: `void (dpc245_runtime_t *)`
- Responsibility: Public function of the dpc245_runtime module.
- Direct callers: `dpc245_app_key`
- Direct callees: `dpc245_set_u16_le`, `dpc245_set_u32_le`

### `dpc245_runtime_tick_500ms`

- Location: `src/dpc245_runtime.c:153`
- Linkage: `global`
- Type: `void (dpc245_runtime_t *, bool)`
- Responsibility: Periodic step in the dpc245_runtime module.
- Direct callers: `main`
- Direct callees: `add_u32`

### `dpc245_runtime_hmi_stats`

- Location: `src/dpc245_runtime.c:161`
- Linkage: `global`
- Type: `void (const dpc245_runtime_t *, dpc245_v3_hmi_stats_raw_t *)`
- Responsibility: Public function of the dpc245_runtime module.
- Direct callers: `dpc245_build_periodic_frame`
- Direct callees: `dpc245_get_u16_le`, `dpc245_get_u32_le`

### `dpc245_runtime_calories_kcal`

- Location: `src/dpc245_runtime.c:176`
- Linkage: `global`
- Type: `uint32_t (const dpc245_runtime_t *)`
- Responsibility: Public function of the dpc245_runtime module.
- Direct callers: `dpc245_ble_app_build_periodic`, `ui_dashboard_lower_pair_text`
- Direct callees: —

## `src/dpc245_settings.c`

Canonical settings record defaults, validation, accessors and error history.

Functions: **25**

### `dpc245_settings_checksum`

- Location: `src/dpc245_settings.c:5`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Checksum calculation or check in the dpc245_settings module.
- Direct callers: `dpc245_settings_finalize`, `dpc245_settings_validate`
- Direct callees: —

### `dpc245_settings_defaults`

- Location: `src/dpc245_settings.c:14`
- Linkage: `global`
- Type: `void (dpc245_settings_t *)`
- Responsibility: Default values in the dpc245_settings module.
- Direct callers: `dpc245_persist_load`, `dpc245_persist_load_checked`, `dpc245_runtime_init`, `evist_native_store_init`, `reset_options`
- Direct callees: `dpc245_set_u32_le`, `dpc245_settings_finalize`

### `dpc245_settings_validate`

- Location: `src/dpc245_settings.c:30`
- Linkage: `global`
- Type: `_Bool (const dpc245_settings_t *)`
- Responsibility: Validation in the dpc245_settings module.
- Direct callers: `dpc245_persist_find_latest`, `dpc245_persist_load_checked`, `values_valid`
- Direct callees: `dpc245_settings_checksum`

### `dpc245_settings_finalize`

- Location: `src/dpc245_settings.c:44`
- Linkage: `global`
- Type: `void (dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dirty`, `dpc245_app_key`, `dpc245_error_push`, `dpc245_hmi_control_process`, `dpc245_persist_save`, `dpc245_settings_defaults`, `evist_native_store_begin`, `handle_10`, `leave_themes`, `reset_options`
- Direct callees: `dpc245_settings_checksum`

### `dpc245_unit`

- Location: `src/dpc245_settings.c:49`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dash_speed_unit`, `display_metric`, `distance_unit`, `dpc245_app_key`, `dpc245_ble_app_build_periodic`, `refresh_a5`, `ride_speed_text`, `speed_unit`, `ui_dashboard_draw`
- Direct callees: —

### `dpc245_set_unit`

- Location: `src/dpc245_settings.c:50`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dpc245_service_tips`

- Location: `src/dpc245_settings.c:51`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dpc245_app_key`, `dpc245_runtime_service_tip_active`, `ui_dashboard_draw`
- Direct callees: —

### `dpc245_set_service_tips`

- Location: `src/dpc245_settings.c:52`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dpc245_auto_off_min`

- Location: `src/dpc245_settings.c:53`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dpc245_app_key`, `refresh_a5`, `ui_dashboard_draw`
- Direct callees: —

### `dpc245_set_auto_off_min`

- Location: `src/dpc245_settings.c:54`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dpc245_brightness_level`

- Location: `src/dpc245_settings.c:55`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dpc245_app_key`, `main`, `ui_dashboard_draw`
- Direct callees: —

### `dpc245_set_brightness_level`

- Location: `src/dpc245_settings.c:56`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dpc245_al_sensitivity`

- Location: `src/dpc245_settings.c:57`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dpc245_app_key`, `main`, `ui_dashboard_draw`
- Direct callees: —

### `dpc245_set_al_sensitivity`

- Location: `src/dpc245_settings.c:58`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `dpc245_app_key`
- Direct callees: —

### `dpc245_theme`

- Location: `src/dpc245_settings.c:60`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `apply_theme`, `dpc245_app_init`, `leave_themes`, `main`, `ui_dashboard_draw`
- Direct callees: —

### `dpc245_set_theme`

- Location: `src/dpc245_settings.c:64`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `dpc245_app_key`, `leave_themes`
- Direct callees: —

### `dpc245_language`

- Location: `src/dpc245_settings.c:67`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: —
- Direct callees: —

### `dpc245_set_language`

- Location: `src/dpc245_settings.c:68`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: —
- Direct callees: —

### `dpc245_get_u32_le`

- Location: `src/dpc245_settings.c:70`
- Linkage: `global`
- Type: `uint32_t (const dpc245_settings_t *, uint8_t)`
- Responsibility: Getter in the dpc245_settings module.
- Direct callers: `add_u32`, `dpc245_ble_app_build_periodic`, `dpc245_hmi_control_build_read_reply`, `dpc245_runtime_hmi_stats`, `dpc245_runtime_on_can`, `refresh_a5`, `ui_dashboard_lower_pair_text`, `update_service_due`
- Direct callees: —

### `dpc245_set_u32_le`

- Location: `src/dpc245_settings.c:77`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t, uint32_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `add_u32`, `dpc245_hmi_control_process`, `dpc245_runtime_trip_reset`, `dpc245_settings_defaults`, `update_service_due`
- Direct callees: —

### `dpc245_get_u16_le`

- Location: `src/dpc245_settings.c:84`
- Linkage: `global`
- Type: `uint16_t (const dpc245_settings_t *, uint8_t)`
- Responsibility: Getter in the dpc245_settings module.
- Direct callers: `dpc245_ble_app_build_periodic`, `dpc245_hmi_control_build_read_reply`, `dpc245_runtime_hmi_stats`, `dpc245_runtime_on_can`, `refresh_a5`, `ui_dashboard_lower_pair_text`
- Direct callees: —

### `dpc245_set_u16_le`

- Location: `src/dpc245_settings.c:88`
- Linkage: `global`
- Type: `void (dpc245_settings_t *, uint8_t, uint16_t)`
- Responsibility: Setter in the dpc245_settings module.
- Direct callers: `dpc245_hmi_control_process`, `dpc245_runtime_on_can`, `dpc245_runtime_trip_reset`
- Direct callees: —

### `dpc245_error_count`

- Location: `src/dpc245_settings.c:95`
- Linkage: `global`
- Type: `size_t (const dpc245_settings_t *)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dpc245_error_at`, `dpc245_error_push`, `dpc245_hmi_service_build_read_reply`, `draw_errors`
- Direct callees: —

### `dpc245_error_at`

- Location: `src/dpc245_settings.c:101`
- Linkage: `global`
- Type: `uint8_t (const dpc245_settings_t *, size_t)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dpc245_hmi_service_build_read_reply`, `draw_errors`
- Direct callees: `dpc245_error_count`

### `dpc245_error_push`

- Location: `src/dpc245_settings.c:107`
- Linkage: `global`
- Type: `_Bool (dpc245_settings_t *, uint8_t)`
- Responsibility: Public function of the dpc245_settings module.
- Direct callers: `dpc245_runtime_on_can`
- Direct callees: `dpc245_error_count`, `dpc245_settings_finalize`

## `src/dpc245_tx.c`

Periodic HMI frames and service query scheduling/building.

Functions: **4**

### `dpc245_build_periodic_frame`

- Location: `src/dpc245_tx.c:6`
- Linkage: `global`
- Type: `bool (uint32_t, const dpc245_app_t *, bool, uint8_t, uint8_t, bafang_can_frame_t *)`
- Responsibility: Frame or record construction in the dpc245_tx module.
- Direct callers: `main`
- Direct callees: `bafang_build_hmi_heartbeat_v3`, `dpc245_runtime_hmi_stats`, `dpc245_v3_build_status_frame_1`, `dpc245_v3_build_status_frame_2`, `dpc245_v3_build_status_frame_3`, `dpc245_v3_build_status_frame_4`

### `query_set`

- Location: `src/dpc245_tx.c:79`
- Linkage: `static`
- Type: `const dpc245_query_spec_t *(dpc245_app_screen_t, size_t *)`
- Responsibility: Internal helper of the dpc245_tx module.
- Direct callers: `dpc245_build_service_query`, `dpc245_service_query_count`
- Direct callees: —

### `dpc245_service_query_count`

- Location: `src/dpc245_tx.c:97`
- Linkage: `global`
- Type: `size_t (dpc245_app_screen_t)`
- Responsibility: Public function of the dpc245_tx module.
- Direct callers: `main`
- Direct callees: `query_set`

### `dpc245_build_service_query`

- Location: `src/dpc245_tx.c:103`
- Linkage: `global`
- Type: `bool (dpc245_app_screen_t, size_t, bafang_can_frame_t *)`
- Responsibility: Frame or record construction in the dpc245_tx module.
- Direct callers: `main`
- Direct callees: `bafang_build_read_query`, `query_set`

## `src/dpc245_update.c`

Module implementation.

Functions: **5**

### `dpc245_update_init`

- Location: `src/dpc245_update.c:4`
- Linkage: `global`
- Type: `void (dpc245_update_t *)`
- Responsibility: Initialization in the dpc245_update module.
- Direct callers: `handoff_after_ble_request`, `main`
- Direct callees: —

### `dpc245_update_request`

- Location: `src/dpc245_update.c:10`
- Linkage: `global`
- Type: `_Bool (dpc245_update_t *)`
- Responsibility: State update in the dpc245_update module.
- Direct callers: `handle_a5`, `handoff_after_ble_request`
- Direct callees: —

### `dpc245_update_next_can`

- Location: `src/dpc245_update.c:16`
- Linkage: `global`
- Type: `_Bool (dpc245_update_t *, bafang_can_frame_t *)`
- Responsibility: State update in the dpc245_update module.
- Direct callers: `handoff_after_ble_request`, `main`
- Direct callees: —

### `dpc245_update_can_sent`

- Location: `src/dpc245_update.c:25`
- Linkage: `global`
- Type: `void (dpc245_update_t *, uint32_t)`
- Responsibility: State update in the dpc245_update module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_update_tick`

- Location: `src/dpc245_update.c:31`
- Linkage: `global`
- Type: `_Bool (dpc245_update_t *, uint32_t, _Bool)`
- Responsibility: State update in the dpc245_update module.
- Direct callers: —
- Direct callees: —

## `src/dpc245_v3_profile.c`

Periodic HMI status-frame schedule and serialization.

Functions: **9**

### `dpc245_v3_tx_schedule_init`

- Location: `src/dpc245_v3_profile.c:28`
- Linkage: `global`
- Type: `void (dpc245_v3_tx_schedule_t *, uint32_t)`
- Responsibility: Initialization in the dpc245_v3_profile module.
- Direct callers: —
- Direct callees: —

### `dpc245_v3_tx_schedule_poll`

- Location: `src/dpc245_v3_profile.c:35`
- Linkage: `global`
- Type: `uint32_t (dpc245_v3_tx_schedule_t *, uint32_t)`
- Responsibility: Public function of the dpc245_v3_profile module.
- Direct callers: `main`
- Direct callees: —

### `dpc245_v3_periodic_find`

- Location: `src/dpc245_v3_profile.c:51`
- Linkage: `global`
- Type: `const dpc245_v3_periodic_can_t *(uint32_t)`
- Responsibility: Public function of the dpc245_v3_profile module.
- Direct callers: —
- Direct callees: —

### `frame_clear`

- Location: `src/dpc245_v3_profile.c:58`
- Linkage: `static`
- Type: `void (bafang_can_frame_t *, uint32_t, uint8_t)`
- Responsibility: Internal helper of the dpc245_v3_profile module.
- Direct callers: `dpc245_hmi_service_tx_next`, `dpc245_v3_build_status_frame_1`, `dpc245_v3_build_status_frame_2`, `dpc245_v3_build_status_frame_3`, `dpc245_v3_build_status_frame_4`, `evist_ctrl_txn_next_tx`
- Direct callees: —

### `put_u24_le`

- Location: `src/dpc245_v3_profile.c:64`
- Linkage: `static`
- Type: `void (uint8_t *, uint32_t)`
- Responsibility: Internal helper of the dpc245_v3_profile module.
- Direct callers: `dpc245_v3_build_status_frame_1`, `dpc245_v3_build_status_frame_2`
- Direct callees: —

### `dpc245_v3_build_status_frame_4`

- Location: `src/dpc245_v3_profile.c:70`
- Linkage: `global`
- Type: `_Bool (bafang_can_frame_t *, const dpc245_v3_hmi_stats_raw_t *)`
- Responsibility: Frame or record construction in the dpc245_v3_profile module.
- Direct callers: `dpc245_build_periodic_frame`
- Direct callees: `frame_clear`

### `dpc245_v3_build_status_frame_1`

- Location: `src/dpc245_v3_profile.c:81`
- Linkage: `global`
- Type: `_Bool (bafang_can_frame_t *, const dpc245_v3_hmi_stats_raw_t *)`
- Responsibility: Frame or record construction in the dpc245_v3_profile module.
- Direct callers: `dpc245_build_periodic_frame`
- Direct callees: `frame_clear`, `put_u24_le`

### `dpc245_v3_build_status_frame_2`

- Location: `src/dpc245_v3_profile.c:94`
- Linkage: `global`
- Type: `_Bool (bafang_can_frame_t *, const dpc245_v3_hmi_stats_raw_t *)`
- Responsibility: Frame or record construction in the dpc245_v3_profile module.
- Direct callers: `dpc245_build_periodic_frame`
- Direct callees: `frame_clear`, `put_u24_le`

### `dpc245_v3_build_status_frame_3`

- Location: `src/dpc245_v3_profile.c:104`
- Linkage: `global`
- Type: `_Bool (bafang_can_frame_t *, const dpc245_v3_hmi_stats_raw_t *)`
- Responsibility: Frame or record construction in the dpc245_v3_profile module.
- Direct callers: `dpc245_build_periodic_frame`
- Direct callees: `frame_clear`

## `src/evist_preferences.c`

EVistDrive preferences validation and persistent journal format.

Functions: **41**

### `evist_preferences_defaults`

- Location: `src/evist_preferences.c:19`
- Linkage: `global`
- Type: `void (evist_preferences_t *)`
- Responsibility: Default values in the evist_preferences module.
- Direct callers: `dpc245_app_init`, `evist_advanced_open`, `evist_native_store_init`, `evist_pref_store_init`, `evist_preferences_load`, `image_decode`, `reset_options`, `verify_full`
- Direct callees: —

### `evist_preferences_validate`

- Location: `src/evist_preferences.c:39`
- Linkage: `global`
- Type: `bool (const evist_preferences_t *)`
- Responsibility: Validation in the evist_preferences module.
- Direct callers: `commit_preferences`, `dpc245_app_apply_preferences`, `evist_pref_store_begin`, `evist_pref_store_save`, `evist_preferences_save`, `image_decode`, `values_valid`
- Direct callees: —

### `read_header`

- Location: `src/evist_preferences.c:60`
- Linkage: `static`
- Type: `pref_read_t (const dpc245_persist_io_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`, `evist_preferences_save`, `image_scan`
- Direct callees: —

### `region_blank`

- Location: `src/evist_preferences.c:67`
- Linkage: `static`
- Type: `bool (const dpc245_persist_io_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_init`, `evist_preferences_save`, `verify_full`
- Direct callees: —

### `write_verified`

- Location: `src/evist_preferences.c:77`
- Linkage: `static`
- Type: `bool (const dpc245_persist_io_t *, uint32_t, const uint8_t *, size_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `append_records`, `evist_pref_store_save_step`
- Direct callees: —

### `valid_record`

- Location: `src/evist_preferences.c:85`
- Linkage: `static`
- Type: `bool (uint8_t, uint8_t, uint8_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `latest_pair`, `scan_log`, `store_scan_chunk`
- Direct callees: —

### `evist_pref_setting_slot_of_code`

- Location: `src/evist_preferences.c:105`
- Linkage: `global`
- Type: `int (uint8_t)`
- Responsibility: Public function of the evist_preferences module.
- Direct callers: `append_records`, `evist_pref_store_save_step`, `scan_log`, `store_scan_chunk`
- Direct callees: —

### `hold_from_code`

- Location: `src/evist_preferences.c:114`
- Linkage: `static`
- Type: `uint8_t (uint8_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `image_decode`
- Direct callees: —

### `hold_code`

- Location: `src/evist_preferences.c:119`
- Linkage: `static`
- Type: `uint8_t (uint8_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `setting_code`
- Direct callees: —

### `setting_code`

- Location: `src/evist_preferences.c:122`
- Linkage: `static`
- Type: `uint8_t (const evist_preferences_t *, unsigned int)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `plan_codes`
- Direct callees: `hold_code`

### `log_used_end`

- Location: `src/evist_preferences.c:144`
- Linkage: `static`
- Type: `bool (const dpc245_persist_io_t *, uint32_t, uint32_t, uint32_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `append_records`, `evist_pref_store_save_step`, `evist_preferences_save`, `scan_log`, `store_check`
- Direct callees: —

### `scan_log`

- Location: `src/evist_preferences.c:160`
- Linkage: `static`
- Type: `bool (const dpc245_persist_io_t *, uint32_t, uint32_t, evist_pref_image_t *, uint32_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `image_scan`
- Direct callees: `evist_pref_setting_slot_of_code`, `log_used_end`, `valid_record`

### `evist_pref_panel_from_record_field`

- Location: `src/evist_preferences.c:204`
- Linkage: `global`
- Type: `uint8_t (uint8_t)`
- Responsibility: Public function of the evist_preferences module.
- Direct callers: `image_decode`
- Direct callees: —

### `evist_pref_record_field_from_panel`

- Location: `src/evist_preferences.c:208`
- Linkage: `global`
- Type: `uint8_t (uint8_t)`
- Responsibility: Public function of the evist_preferences module.
- Direct callers: `append_records`, `evist_pref_store_save_step`
- Direct callees: —

### `pair_addr`

- Location: `src/evist_preferences.c:212`
- Linkage: `static`
- Type: `uint32_t (uint32_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `append_records`, `evist_pref_store_save_step`, `pair_read`
- Direct callees: —

### `pair_read`

- Location: `src/evist_preferences.c:214`
- Linkage: `static`
- Type: `bool (const dpc245_persist_io_t *, uint32_t, uint8_t *)`
- Responsibility: Read operation in the evist_preferences module.
- Direct callers: `latest_pair`, `pair_first_blank`
- Direct callees: `pair_addr`

### `pair_first_blank`

- Location: `src/evist_preferences.c:220`
- Linkage: `static`
- Type: `bool (const dpc245_persist_io_t *, uint32_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `append_records`, `evist_pref_store_save_step`, `evist_preferences_save`, `latest_pair`, `store_check`
- Direct callees: `pair_read`

### `pair_usable`

- Location: `src/evist_preferences.c:233`
- Linkage: `static`
- Type: `bool (uint8_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `latest_pair`
- Direct callees: —

### `latest_pair`

- Location: `src/evist_preferences.c:238`
- Linkage: `static`
- Type: `pref_read_t (const dpc245_persist_io_t *, uint32_t *, uint8_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`, `image_scan`
- Direct callees: `pair_first_blank`, `pair_read`, `pair_usable`, `valid_record`

### `image_clear`

- Location: `src/evist_preferences.c:255`
- Linkage: `static`
- Type: `void (evist_pref_image_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_init`, `evist_pref_store_save_step`, `evist_preferences_save`, `image_scan`
- Direct callees: —

### `image_scan`

- Location: `src/evist_preferences.c:267`
- Linkage: `static`
- Type: `pref_read_t (const dpc245_persist_io_t *, evist_pref_image_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_init`, `evist_preferences_load`, `verify_full`
- Direct callees: `image_clear`, `latest_pair`, `read_header`, `scan_log`

### `have`

- Location: `src/evist_preferences.c:281`
- Linkage: `static`
- Type: `bool (const evist_pref_image_t *, unsigned int)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `image_decode`, `stored_panel_bits`
- Direct callees: —

### `stored_panel_bits`

- Location: `src/evist_preferences.c:285`
- Linkage: `static`
- Type: `uint16_t (const evist_pref_image_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`, `image_decode`
- Direct callees: `have`

### `image_decode`

- Location: `src/evist_preferences.c:295`
- Linkage: `static`
- Type: `bool (const evist_pref_image_t *, evist_preferences_t *)`
- Responsibility: Decoding in the evist_preferences module.
- Direct callers: `evist_pref_store_init`, `evist_pref_store_save_step`, `evist_preferences_load`, `verify_full`
- Direct callees: `evist_pref_panel_from_record_field`, `evist_preferences_defaults`, `evist_preferences_validate`, `have`, `hold_from_code`, `stored_panel_bits`

### `evist_preferences_load`

- Location: `src/evist_preferences.c:323`
- Linkage: `global`
- Type: `bool (const dpc245_persist_io_t *, evist_preferences_t *)`
- Responsibility: Load operation in the evist_preferences module.
- Direct callers: `evist_native_store_init`, `evist_preferences_save`
- Direct callees: `evist_preferences_defaults`, `image_decode`, `image_scan`

### `same_preferences`

- Location: `src/evist_preferences.c:332`
- Linkage: `static`
- Type: `bool (const evist_preferences_t *, const evist_preferences_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`, `verify_full`
- Direct callees: —

### `temp_state`

- Location: `src/evist_preferences.c:345`
- Linkage: `static`
- Type: `uint8_t (const evist_preferences_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`, `evist_preferences_save`, `plan_codes`
- Direct callees: —

### `plan_codes`

- Location: `src/evist_preferences.c:350`
- Linkage: `static`
- Type: `size_t (const evist_preferences_t *, uint16_t, uint8_t, const evist_preferences_t *, uint8_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_preferences_save`
- Direct callees: `setting_code`, `temp_state`

### `pair_differs`

- Location: `src/evist_preferences.c:363`
- Linkage: `static`
- Type: `bool (const evist_preferences_t *, const evist_preferences_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`, `evist_preferences_save`
- Direct callees: —

### `append_records`

- Location: `src/evist_preferences.c:371`
- Linkage: `static`
- Type: `evist_pref_save_result_t (const dpc245_persist_io_t *, evist_pref_image_t *, const uint8_t *, size_t, bool, const evist_preferences_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_preferences_save`
- Direct callees: `evist_pref_record_field_from_panel`, `evist_pref_setting_slot_of_code`, `log_used_end`, `pair_addr`, `pair_first_blank`, `write_verified`

### `verify_full`

- Location: `src/evist_preferences.c:410`
- Linkage: `static`
- Type: `evist_pref_save_result_t (const dpc245_persist_io_t *, const evist_preferences_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_preferences_save`
- Direct callees: `evist_preferences_defaults`, `image_decode`, `image_scan`, `region_blank`, `same_preferences`

### `evist_preferences_save`

- Location: `src/evist_preferences.c:424`
- Linkage: `global`
- Type: `evist_pref_save_result_t (const dpc245_persist_io_t *, const evist_preferences_t *, const evist_preferences_t *)`
- Responsibility: Save operation in the evist_preferences module.
- Direct callers: —
- Direct callees: `append_records`, `evist_preferences_load`, `evist_preferences_validate`, `image_clear`, `log_used_end`, `pair_differs`, `pair_first_blank`, `plan_codes`, `read_header`, `region_blank`, `temp_state`, `verify_full`

### `store_check`

- Location: `src/evist_preferences.c:456`
- Linkage: `static`
- Type: `evist_pref_save_result_t (const dpc245_persist_io_t *, const evist_pref_image_t *)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`
- Direct callees: `log_used_end`, `pair_first_blank`

### `store_finish`

- Location: `src/evist_preferences.c:478`
- Linkage: `static`
- Type: `evist_pref_save_result_t (evist_pref_store_t *, evist_pref_save_result_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`
- Direct callees: —

### `store_scan_chunk`

- Location: `src/evist_preferences.c:486`
- Linkage: `static`
- Type: `bool (evist_pref_store_t *, const dpc245_persist_io_t *, uint32_t)`
- Responsibility: Internal helper of the evist_preferences module.
- Direct callers: `evist_pref_store_save_step`
- Direct callees: `evist_pref_setting_slot_of_code`, `valid_record`

### `evist_pref_store_init`

- Location: `src/evist_preferences.c:506`
- Linkage: `global`
- Type: `void (evist_pref_store_t *, const dpc245_persist_io_t *, evist_preferences_t *)`
- Responsibility: Initialization in the evist_preferences module.
- Direct callers: —
- Direct callees: `evist_preferences_defaults`, `image_clear`, `image_decode`, `image_scan`, `region_blank`

### `evist_pref_store_busy`

- Location: `src/evist_preferences.c:523`
- Linkage: `global`
- Type: `bool (const evist_pref_store_t *)`
- Responsibility: Public function of the evist_preferences module.
- Direct callers: —
- Direct callees: —

### `evist_pref_store_begin`

- Location: `src/evist_preferences.c:525`
- Linkage: `global`
- Type: `bool (evist_pref_store_t *, const evist_preferences_t *)`
- Responsibility: Public function of the evist_preferences module.
- Direct callers: `evist_pref_store_save`
- Direct callees: `evist_preferences_validate`

### `evist_pref_store_save_step`

- Location: `src/evist_preferences.c:537`
- Linkage: `global`
- Type: `evist_pref_save_result_t (evist_pref_store_t *, const dpc245_persist_io_t *)`
- Responsibility: Save operation in the evist_preferences module.
- Direct callers: `evist_pref_store_save`
- Direct callees: `evist_pref_record_field_from_panel`, `evist_pref_setting_slot_of_code`, `image_clear`, `image_decode`, `latest_pair`, `log_used_end`, `pair_addr`, `pair_differs`, `pair_first_blank`, `read_header`, `same_preferences`, `store_check`, `store_finish`, `store_scan_chunk`, `stored_panel_bits`, `temp_state`, `write_verified`

### `evist_pref_store_save`

- Location: `src/evist_preferences.c:667`
- Linkage: `global`
- Type: `evist_pref_save_result_t (evist_pref_store_t *, const dpc245_persist_io_t *, const evist_preferences_t *)`
- Responsibility: Save operation in the evist_preferences module.
- Direct callers: —
- Direct callees: `evist_pref_store_begin`, `evist_pref_store_save_step`, `evist_preferences_validate`

### `evist_pref_save_text`

- Location: `src/evist_preferences.c:677`
- Linkage: `global`
- Type: `const char *(evist_pref_save_result_t)`
- Responsibility: Save operation in the evist_preferences module.
- Direct callers: —
- Direct callees: —

## `src/evistdrive_adapter.c`

Motor-family adapter functions between generic runtime and Bafang telemetry.

Functions: **2**

### `m820_on_frame`

- Location: `src/evistdrive_adapter.c:4`
- Linkage: `static`
- Type: `_Bool (const bafang_can_frame_t *, bafang_telemetry_t *)`
- Responsibility: Internal helper of the evistdrive_adapter module.
- Direct callers: —
- Direct callees: `bafang_decode_telemetry`

### `evist_adapter_bafang_m820`

- Location: `src/evistdrive_adapter.c:10`
- Linkage: `global`
- Type: `const evist_motor_adapter_t *(void)`
- Responsibility: Public function of the evistdrive_adapter module.
- Direct callers: —
- Direct callees: —

## `src/light_controller.c`

Ambient-light filtering, automatic lamp state and display-lighting policy.

Functions: **8**

### `median3`

- Location: `src/light_controller.c:4`
- Linkage: `static`
- Type: `uint16_t (uint16_t, uint16_t, uint16_t)`
- Responsibility: Internal helper of the light_controller module.
- Direct callers: `evist_light_update_als`
- Direct callees: —

### `auto_position`

- Location: `src/light_controller.c:10`
- Linkage: `static`
- Type: `uint8_t (uint16_t, uint8_t)`
- Responsibility: Internal helper of the light_controller module.
- Direct callers: `evist_light_update_als`
- Direct callees: —

### `evist_light_init`

- Location: `src/light_controller.c:19`
- Linkage: `global`
- Type: `void (evist_light_controller_t *, uint8_t)`
- Responsibility: Initialization in the light_controller module.
- Direct callers: —
- Direct callees: —

### `evist_light_configure`

- Location: `src/light_controller.c:26`
- Linkage: `global`
- Type: `void (evist_light_controller_t *, _Bool, uint8_t, uint8_t, uint16_t, uint8_t)`
- Responsibility: Public function of the light_controller module.
- Direct callers: `main`
- Direct callees: —

### `evist_light_set_mode`

- Location: `src/light_controller.c:36`
- Linkage: `global`
- Type: `void (evist_light_controller_t *, evist_light_mode_t)`
- Responsibility: Setter in the light_controller module.
- Direct callers: —
- Direct callees: —

### `evist_light_update_als`

- Location: `src/light_controller.c:41`
- Linkage: `global`
- Type: `_Bool (evist_light_controller_t *, uint16_t)`
- Responsibility: State update in the light_controller module.
- Direct callers: `main`
- Direct callees: `auto_position`, `median3`

### `evist_light_user_toggle`

- Location: `src/light_controller.c:55`
- Linkage: `global`
- Type: `_Bool (evist_light_controller_t *)`
- Responsibility: Public function of the light_controller module.
- Direct callers: `main`
- Direct callees: —

### `evist_light_backlight_position`

- Location: `src/light_controller.c:56`
- Linkage: `global`
- Type: `uint8_t (const evist_light_controller_t *)`
- Responsibility: Public function of the light_controller module.
- Direct callers: `main`
- Direct callees: —

## `src/loop_stats.c`

Module implementation.

Functions: **3**

### `evist_micros_compose`

- Location: `src/loop_stats.c:4`
- Linkage: `global`
- Type: `uint32_t (uint32_t, uint32_t, uint32_t, _Bool)`
- Responsibility: Public function of the loop_stats module.
- Direct callers: `micros`
- Direct callees: —

### `evist_loop_stats_init`

- Location: `src/loop_stats.c:9`
- Linkage: `global`
- Type: `void (evist_loop_stats_t *, uint32_t)`
- Responsibility: Initialization in the loop_stats module.
- Direct callers: —
- Direct callees: —

### `evist_loop_stats_add`

- Location: `src/loop_stats.c:17`
- Linkage: `global`
- Type: `void (evist_loop_stats_t *, uint32_t, uint32_t)`
- Responsibility: Public function of the loop_stats module.
- Direct callers: —
- Direct callees: —

## `src/main.c`

Platform bootstrap and cooperative application scheduler.

Functions: **15**

### `tick_init`

- Location: `src/main.c:57`
- Linkage: `static`
- Type: `void (uint32_t)`
- Responsibility: Initialization in the main module.
- Direct callers: `clock_failure_safe_mode`, `main`
- Direct callees: —

### `millis`

- Location: `src/main.c:58`
- Linkage: `static`
- Type: `uint32_t (void)`
- Responsibility: Internal helper of the main module.
- Direct callers: `clock_failure_safe_mode`, `delay_ms`, `main`
- Direct callees: —

### `settings_erase_start`

- Location: `src/main.c:59`
- Linkage: `static`
- Type: `bool (void *, uint32_t)`
- Responsibility: Start operation in the main module.
- Direct callers: —
- Direct callees: `dpc245_spi_mem_erase_start`

### `settings_ready`

- Location: `src/main.c:62`
- Linkage: `static`
- Type: `int (void *)`
- Responsibility: Read operation in the main module.
- Direct callers: —
- Direct callees: `dpc245_spi_mem_ready`

### `micros`

- Location: `src/main.c:67`
- Linkage: `static`
- Type: `uint32_t (void)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`, `stage_end`
- Direct callees: `evist_micros_compose`

### `stage_end`

- Location: `src/main.c:76`
- Linkage: `static`
- Type: `uint32_t (dpc245_app_t *, unsigned int, uint32_t)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`
- Direct callees: `micros`

### `screen_draw`

- Location: `src/main.c:90`
- Linkage: `static`
- Type: `void (void *)`
- Responsibility: Drawing routine in the main module.
- Direct callers: —
- Direct callees: `ui_dashboard_draw`

### `screen_push`

- Location: `src/main.c:95`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t, const uint16_t *, uint16_t)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`
- Direct callees: `dpc245_lcd_data16`, `dpc245_lcd_set_window`

### `delay_ms`

- Location: `src/main.c:102`
- Linkage: `static`
- Type: `void (uint32_t)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`
- Direct callees: `millis`

### `send_normal`

- Location: `src/main.c:112`
- Linkage: `static`
- Type: `bool (evist_can_router_t *, const bafang_can_frame_t *)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`, `send_service_ack`
- Direct callees: `dpc245_can_transmit`, `evist_can_router_tx_allowed`

### `send_service_ack`

- Location: `src/main.c:116`
- Linkage: `static`
- Type: `void (evist_can_router_t *)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`
- Direct callees: `dpc245_can_tx_idle`, `evist_can_router_peek_service_ack`, `evist_can_router_pop_service_ack`, `send_normal`

### `controller_config_service`

- Location: `src/main.c:128`
- Linkage: `static`
- Type: `void (evist_ctrl_txn_t *, dpc245_app_t *, const evist_can_router_t *, evist_ride_apply_t *, bool *, dpc245_ble_app_t *, bool, uint32_t)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`
- Direct callees: `dpc245_app_ride_applied`, `dpc245_app_ride_wheel_verified`, `dpc245_ble_app_capture_adv_result`, `dpc245_ble_app_on_controller_result`, `dpc245_ble_app_start_controller`, `dpc245_can_transmit`, `dpc245_can_tx_idle`, `evist_advanced_on_result`, `evist_advanced_request_failed`, `evist_advanced_take_request`, `evist_advanced_wheel_target`, `evist_ctrl_txn_discover_accel`, `evist_ctrl_txn_next_tx`, `evist_ctrl_txn_read_accel_p0`, `evist_ctrl_txn_read_accel_p2`, `evist_ctrl_txn_read_assist`, `evist_ctrl_txn_read_wheel`, `evist_ctrl_txn_take_result`, `evist_ctrl_txn_tick`, `evist_ctrl_txn_write_app_p0`, `evist_ctrl_txn_write_app_p2`, `evist_ctrl_txn_write_assist`, `evist_ctrl_txn_write_wheel`, `evist_ride_apply_on_result`, `evist_ride_apply_start_txn`

### `start_clock`

- Location: `src/main.c:261`
- Linkage: `static`
- Type: `dpc245_clock_tree_t (void)`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`
- Direct callees: `dpc245_clock_fallback_hsi`, `dpc245_clock_init`, `dpc245_clock_tree_now`

### `clock_failure_safe_mode`

- Location: `src/main.c:273`
- Linkage: `static`
- Type: `void (void) __attribute__((noreturn))`
- Responsibility: Internal helper of the main module.
- Direct callers: `main`
- Direct callees: `dpc245_buttons_init`, `dpc245_preflight_begin`, `millis`, `tick_init`

### `main`

- Location: `src/main.c:283`
- Linkage: `global`
- Type: `int (void)`
- Responsibility: Runs platform initialization and the cooperative application loop.
- Direct callers: `Reset_Handler`
- Direct callees: `bafang_can_id_decode`, `bafang_power_w`, `clock_failure_safe_mode`, `controller_config_service`, `delay_ms`, `dpc245_adc_init`, `dpc245_adc_sample32`, `dpc245_adc_window_filter_init`, `dpc245_adc_window_filter_tick`, `dpc245_al_sensitivity`, `dpc245_app_apply_preferences`, `dpc245_app_down_state`, `dpc245_app_event`, `dpc245_app_init`, `dpc245_app_set_ride`, `dpc245_app_take_light_toggle`, `dpc245_app_take_preferences_dirty`, `dpc245_app_take_ride_apply`, `dpc245_app_take_ride_cfg_dirty`, `dpc245_app_take_settings_dirty`, `dpc245_app_temperature_sample`, `dpc245_app_theme_store_result`, `dpc245_app_tick`, `dpc245_backlight_init`, `dpc245_backlight_set_level`, `dpc245_ble_app_apply_advanced_cache`, `dpc245_ble_app_build_periodic`, `dpc245_ble_app_handle`, `dpc245_ble_app_init`, `dpc245_ble_app_take_hmi_event_bits`, `dpc245_ble_parser_feed`, `dpc245_ble_parser_init`, `dpc245_ble_uart_poll`, `dpc245_ble_uart_read_byte`, `dpc245_ble_uart_send_device_name_command`, `dpc245_ble_uart_write`, `dpc245_brightness_level`, `dpc245_build_periodic_frame`, `dpc245_build_service_query`, `dpc245_button_signal_filter`, `dpc245_buttons_init`, `dpc245_buttons_key_event`, `dpc245_buttons_state`, `dpc245_buttons_take_released`, `dpc245_can_receive_poll`, `dpc245_can_transmit`, `dpc245_can_tx_idle`, `dpc245_enter_resident_bootloader`, `dpc245_hmi_control_build_read_reply`, `dpc245_hmi_control_process`, `dpc245_hmi_identity_records_init`, `dpc245_hmi_identity_records_load`, `dpc245_hmi_service_build_read_reply`, `dpc245_hmi_service_tx_init`, `dpc245_hmi_service_tx_next`, `dpc245_hmi_service_tx_on_ack`, `dpc245_hmi_service_tx_start`, `dpc245_hwinfo_read`, `dpc245_is_can_update_request`, `dpc245_key_time_for_pass`, `dpc245_preflight_begin`, `dpc245_rtc_board_io`, `dpc245_runtime_on_can`, `dpc245_runtime_tick_500ms`, `dpc245_service_query_count`, `dpc245_spi_mem_end_step`, `dpc245_spi_mem_init`, `dpc245_spi_mem_persist_io`, `dpc245_spi_mem_set_limits`, `dpc245_system_reset`, `dpc245_theme`, `dpc245_update_can_sent`, `dpc245_update_init`, `dpc245_update_next_can`, `dpc245_v3_tx_schedule_poll`, `evist_advanced_set_status`, `evist_advanced_take_light_runtime`, `evist_advanced_take_ride`, `evist_can_router_init`, `evist_can_router_rx`, `evist_ctrl_txn_init`, `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`, `evist_light_backlight_position`, `evist_light_configure`, `evist_light_update_als`, `evist_light_user_toggle`, `evist_native_result_text`, `evist_native_store_begin`, `evist_native_store_init`, `evist_native_store_step`, `evist_ride_apply_begin`, `evist_ride_apply_init`, `micros`, `millis`, `screen_push`, `send_normal`, `send_service_ack`, `stage_end`, `start_clock`, `tick_init`, `ui_boot_splash_draw`, `ui_strip_invalidate`, `ui_strip_render_active`, `ui_strip_render_cancel`, `ui_strip_render_step`

## `src/native_store.c`

Module implementation.

Functions: **19**

### `page_address`

- Location: `src/native_store.c:22`
- Linkage: `static`
- Type: `uint32_t (uint16_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `blank_page`, `evist_native_store_init`, `evist_native_store_step`, `scan_native_pages`
- Direct callees: —

### `put16`

- Location: `src/native_store.c:26`
- Linkage: `static`
- Type: `void (uint8_t *, uint16_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `encode`
- Direct callees: —

### `get16`

- Location: `src/native_store.c:27`
- Linkage: `static`
- Type: `uint16_t (const uint8_t *)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `decode`
- Direct callees: —

### `put32`

- Location: `src/native_store.c:28`
- Linkage: `static`
- Type: `void (uint8_t *, uint32_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `encode`
- Direct callees: —

### `get32`

- Location: `src/native_store.c:31`
- Linkage: `static`
- Type: `uint32_t (const uint8_t *)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `decode`
- Direct callees: —

### `crc32`

- Location: `src/native_store.c:35`
- Linkage: `static`
- Type: `uint32_t (const uint8_t *, size_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `decode`, `encode`
- Direct callees: —

### `values_valid`

- Location: `src/native_store.c:45`
- Linkage: `static`
- Type: `bool (const evist_native_values_t *)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `decode`, `evist_native_store_begin`
- Direct callees: `dpc245_settings_validate`, `evist_preferences_validate`, `evist_ride_cfg_valid`

### `encode`

- Location: `src/native_store.c:50`
- Linkage: `static`
- Type: `void (uint8_t *, const evist_native_values_t *, uint32_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `evist_native_store_step`
- Direct callees: `crc32`, `put16`, `put32`

### `current_firmware_version`

- Location: `src/native_store.c:76`
- Linkage: `static`
- Type: `bool (const uint8_t *)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `scan_native_pages`
- Direct callees: —

### `reset_options`

- Location: `src/native_store.c:82`
- Linkage: `static`
- Type: `void (evist_native_values_t *)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `evist_native_store_init`
- Direct callees: `dpc245_settings_defaults`, `dpc245_settings_finalize`, `evist_preferences_defaults`, `evist_ride_defaults`

### `decode`

- Location: `src/native_store.c:98`
- Linkage: `static`
- Type: `bool (const uint8_t *, evist_native_values_t *, uint32_t *)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `evist_native_store_step`, `scan_native_pages`
- Direct callees: `crc32`, `get16`, `get32`, `values_valid`

### `blank_page`

- Location: `src/native_store.c:125`
- Linkage: `static`
- Type: `int (const dpc245_persist_io_t *, uint16_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `evist_native_store_init`
- Direct callees: `page_address`

### `newer`

- Location: `src/native_store.c:134`
- Linkage: `static`
- Type: `bool (uint32_t, uint32_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `scan_native_pages`
- Direct callees: —

### `scan_native_pages`

- Location: `src/native_store.c:136`
- Linkage: `static`
- Type: `bool (evist_native_store_t *, const dpc245_persist_io_t *)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `evist_native_store_init`
- Direct callees: `current_firmware_version`, `decode`, `newer`, `page_address`

### `evist_native_store_init`

- Location: `src/native_store.c:155`
- Linkage: `global`
- Type: `bool (evist_native_store_t *, const evist_native_io_t *, evist_native_values_t *)`
- Responsibility: Initialization in the native_store module.
- Direct callers: `main`
- Direct callees: `blank_page`, `dpc245_persist_load_checked`, `dpc245_settings_defaults`, `evist_preferences_defaults`, `evist_preferences_load`, `evist_ride_defaults`, `evist_ride_store_load_checked`, `page_address`, `reset_options`, `scan_native_pages`

### `evist_native_store_begin`

- Location: `src/native_store.c:206`
- Linkage: `global`
- Type: `bool (evist_native_store_t *, const evist_native_values_t *)`
- Responsibility: Public function of the native_store module.
- Direct callers: `main`
- Direct callees: `dpc245_settings_finalize`, `values_valid`

### `finish`

- Location: `src/native_store.c:215`
- Linkage: `static`
- Type: `evist_native_result_t (evist_native_store_t *, evist_native_result_t)`
- Responsibility: Internal helper of the native_store module.
- Direct callers: `complete_read`, `evist_ctrl_txn_on_frame`, `evist_ctrl_txn_on_service`, `evist_ctrl_txn_tick`, `evist_native_store_step`
- Direct callees: —

### `evist_native_store_step`

- Location: `src/native_store.c:219`
- Linkage: `global`
- Type: `evist_native_result_t (evist_native_store_t *, const evist_native_io_t *, uint32_t)`
- Responsibility: Public function of the native_store module.
- Direct callers: `main`
- Direct callees: `decode`, `encode`, `finish`, `page_address`

### `evist_native_result_text`

- Location: `src/native_store.c:286`
- Linkage: `global`
- Type: `const char *(evist_native_result_t)`
- Responsibility: Public function of the native_store module.
- Direct callers: `main`
- Direct callees: —

## `src/ride_mode.c`

Module implementation.

Functions: **25**

### `evist_ride_target_x100`

- Location: `src/ride_mode.c:6`
- Linkage: `global`
- Type: `uint16_t (const evist_ride_cfg_t *)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: `dpc245_app_take_ride_apply`
- Direct callees: —

### `evist_ride_defaults`

- Location: `src/ride_mode.c:12`
- Linkage: `global`
- Type: `evist_ride_cfg_t (void)`
- Responsibility: Default values in the ride_mode module.
- Direct callers: `dpc245_app_init`, `evist_advanced_open`, `evist_native_store_init`, `evist_ride_log_scan`, `evist_ride_record_decode`, `evist_ride_store_load`, `merge_unpaired_v1`, `reset_options`
- Direct callees: —

### `evist_ride_boot_mode`

- Location: `src/ride_mode.c:19`
- Linkage: `global`
- Type: `evist_ride_mode_t (const evist_ride_cfg_t *)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: `dpc245_app_set_ride`
- Direct callees: —

### `evist_ride_speed_valid`

- Location: `src/ride_mode.c:25`
- Linkage: `global`
- Type: `bool (uint16_t)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: `evist_ride_apply_begin`, `evist_ride_cfg_valid`, `evist_ride_record_decode`
- Direct callees: —

### `evist_ride_legal_speed_valid`

- Location: `src/ride_mode.c:30`
- Linkage: `global`
- Type: `bool (uint16_t)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: `evist_ride_cfg_valid`
- Direct callees: —

### `evist_ride_cfg_valid`

- Location: `src/ride_mode.c:35`
- Linkage: `global`
- Type: `bool (const evist_ride_cfg_t *)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: `dpc245_app_set_ride`, `evist_advanced_set_ride`, `evist_ride_record_decode`, `evist_ride_record_encode`, `evist_ride_record_v2_decode`, `evist_ride_record_v2_encode`, `evist_ride_store_append`, `preference_key`, `values_valid`
- Direct callees: `evist_ride_legal_speed_valid`, `evist_ride_speed_valid`

### `record_crc`

- Location: `src/ride_mode.c:45`
- Linkage: `static`
- Type: `uint8_t (const uint8_t *)`
- Responsibility: Internal helper of the ride_mode module.
- Direct callers: `evist_ride_record_decode`, `evist_ride_record_encode`, `evist_ride_record_v2_decode`, `evist_ride_record_v2_encode`
- Direct callees: —

### `evist_ride_record_encode`

- Location: `src/ride_mode.c:49`
- Linkage: `global`
- Type: `bool (const evist_ride_cfg_t *, uint8_t *)`
- Responsibility: Encoding in the ride_mode module.
- Direct callers: —
- Direct callees: `evist_ride_cfg_valid`, `record_crc`

### `evist_ride_record_decode`

- Location: `src/ride_mode.c:60`
- Linkage: `global`
- Type: `bool (const uint8_t *, evist_ride_cfg_t *)`
- Responsibility: Decoding in the ride_mode module.
- Direct callers: `record_any_decode`
- Direct callees: `evist_ride_cfg_valid`, `evist_ride_defaults`, `evist_ride_speed_valid`, `record_crc`

### `evist_ride_record_v2_encode`

- Location: `src/ride_mode.c:76`
- Linkage: `global`
- Type: `bool (const evist_ride_cfg_t *, uint8_t *)`
- Responsibility: Encoding in the ride_mode module.
- Direct callers: `evist_ride_store_append`
- Direct callees: `evist_ride_cfg_valid`, `record_crc`

### `evist_ride_record_v2_decode`

- Location: `src/ride_mode.c:88`
- Linkage: `global`
- Type: `bool (const uint8_t *, evist_ride_cfg_t *)`
- Responsibility: Decoding in the ride_mode module.
- Direct callers: `evist_ride_log_scan`, `evist_ride_store_load`, `record_any_decode`
- Direct callees: `evist_ride_cfg_valid`, `record_crc`

### `record_any_decode`

- Location: `src/ride_mode.c:103`
- Linkage: `static`
- Type: `bool (const uint8_t *, evist_ride_cfg_t *)`
- Responsibility: Decoding in the ride_mode module.
- Direct callers: `evist_ride_log_scan`, `evist_ride_store_load`
- Direct callees: `evist_ride_record_decode`, `evist_ride_record_v2_decode`

### `record_v1_speed`

- Location: `src/ride_mode.c:107`
- Linkage: `static`
- Type: `uint16_t (const uint8_t *)`
- Responsibility: Internal helper of the ride_mode module.
- Direct callers: `evist_ride_log_scan`, `evist_ride_store_load`, `merge_unpaired_v1`
- Direct callees: —

### `merge_unpaired_v1`

- Location: `src/ride_mode.c:111`
- Linkage: `static`
- Type: `evist_ride_cfg_t (const uint8_t *, const evist_ride_cfg_t *, const evist_ride_cfg_t *)`
- Responsibility: Internal helper of the ride_mode module.
- Direct callers: `evist_ride_log_scan`, `evist_ride_store_load`
- Direct callees: `evist_ride_defaults`, `record_v1_speed`

### `evist_ride_record_blank`

- Location: `src/ride_mode.c:122`
- Linkage: `global`
- Type: `bool (const uint8_t *)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: `evist_ride_log_scan`, `evist_ride_store_append`, `evist_ride_store_load`
- Direct callees: —

### `evist_ride_log_scan`

- Location: `src/ride_mode.c:128`
- Linkage: `global`
- Type: `bool (const uint8_t *, size_t, evist_ride_cfg_t *)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: —
- Direct callees: `evist_ride_defaults`, `evist_ride_record_blank`, `evist_ride_record_v2_decode`, `merge_unpaired_v1`, `record_any_decode`, `record_v1_speed`

### `evist_ride_store_load`

- Location: `src/ride_mode.c:159`
- Linkage: `global`
- Type: `bool (const dpc245_persist_io_t *, evist_ride_cfg_t *, uint32_t *)`
- Responsibility: Load operation in the ride_mode module.
- Direct callers: `evist_ride_boot_load`, `evist_ride_store_load_checked`
- Direct callees: `evist_ride_defaults`, `evist_ride_record_blank`, `evist_ride_record_v2_decode`, `merge_unpaired_v1`, `record_any_decode`, `record_v1_speed`

### `evist_ride_boot_load`

- Location: `src/ride_mode.c:208`
- Linkage: `global`
- Type: `bool (const dpc245_persist_io_t *, evist_ride_cfg_t *, uint32_t *)`
- Responsibility: Load operation in the ride_mode module.
- Direct callers: —
- Direct callees: `evist_ride_store_load`

### `evist_ride_store_load_checked`

- Location: `src/ride_mode.c:213`
- Linkage: `global`
- Type: `bool (const dpc245_persist_io_t *, evist_ride_cfg_t *)`
- Responsibility: Load operation in the ride_mode module.
- Direct callers: `evist_native_store_init`
- Direct callees: `evist_ride_store_load`

### `evist_ride_store_append`

- Location: `src/ride_mode.c:226`
- Linkage: `global`
- Type: `evist_ride_store_result_t (const dpc245_persist_io_t *, const evist_ride_cfg_t *, const evist_ride_cfg_t *, uint32_t *)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: —
- Direct callees: `evist_ride_cfg_valid`, `evist_ride_record_blank`, `evist_ride_record_v2_encode`

### `evist_ride_store_status`

- Location: `src/ride_mode.c:259`
- Linkage: `global`
- Type: `const char *(evist_ride_store_result_t)`
- Responsibility: Public function of the ride_mode module.
- Direct callers: —
- Direct callees: —

### `evist_ride_apply_init`

- Location: `src/ride_mode.c:273`
- Linkage: `global`
- Type: `void (evist_ride_apply_t *)`
- Responsibility: Initialization in the ride_mode module.
- Direct callers: `main`
- Direct callees: —

### `evist_ride_apply_begin`

- Location: `src/ride_mode.c:280`
- Linkage: `global`
- Type: `void (evist_ride_apply_t *, uint16_t)`
- Responsibility: Apply operation in the ride_mode module.
- Direct callers: `main`
- Direct callees: `evist_ride_speed_valid`

### `evist_ride_apply_start_txn`

- Location: `src/ride_mode.c:286`
- Linkage: `global`
- Type: `bool (evist_ride_apply_t *, evist_ctrl_txn_t *, uint32_t)`
- Responsibility: Start operation in the ride_mode module.
- Direct callers: `controller_config_service`
- Direct callees: `evist_ctrl_txn_busy`, `evist_ctrl_txn_read_wheel`, `evist_ctrl_txn_write_wheel`, `evist_wheel_config_decode`

### `evist_ride_apply_on_result`

- Location: `src/ride_mode.c:306`
- Linkage: `global`
- Type: `void (evist_ride_apply_t *, evist_txn_result_t, const uint8_t *)`
- Responsibility: Apply operation in the ride_mode module.
- Direct callers: `controller_config_service`
- Direct callees: —

## `src/ui_dashboard.c`

Module implementation.

Functions: **107**

### `m_font`

- Location: `src/ui_dashboard.c:42`
- Linkage: `static`
- Type: `const ui_font1_t *(uint8_t, int *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `m_text`
- Direct callees: —

### `m_text`

- Location: `src/ui_dashboard.c:47`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, const char *, uint16_t, uint16_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `adv_row`, `diag_stage`, `draw_adv_assist`, `draw_adv_diag`, `draw_adv_wheel`, `draw_advanced`, `draw_battery`, `draw_controller`, `draw_dashboard`, `draw_edit`, `draw_errors`, `draw_hmi_info`, `draw_menu`, `draw_speed_limit`, `draw_temperature_warning`, `draw_trip_reset`, `draw_wheel`, `footer`, `lower_cell`, `m_hex32`, `m_num`, `theme_banner`, `title`, `ui_boot_splash_draw`, `ui_dashboard_draw`, `value_i32`, `value_text`, `value_u32`, `value_x100`, `value_x100_i`
- Direct callees: `m_font`, `ui_font1_text`

### `ui_dashboard_temp_limit_text`

- Location: `src/ui_dashboard.c:51`
- Linkage: `global`
- Type: `void (char *, uint8_t)`
- Responsibility: Public function of the ui_dashboard module.
- Direct callers: `draw_advanced`
- Direct callees: —

### `m_num`

- Location: `src/ui_dashboard.c:59`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, const char *, uint16_t, uint16_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `m_fixed_u32`, `m_i32`, `m_u32`
- Direct callees: `m_text`, `ui_font1_text`

### `m_u32`

- Location: `src/ui_dashboard.c:63`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, uint32_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_stage`, `draw_adv_diag`, `draw_battery`, `draw_errors`, `value_u32`
- Direct callees: `m_num`

### `m_i32`

- Location: `src/ui_dashboard.c:67`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, int32_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `value_i32`
- Direct callees: `m_num`

### `m_fixed_u32`

- Location: `src/ui_dashboard.c:72`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, uint32_t, uint8_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_wheel`, `value_x100`, `value_x100_i`
- Direct callees: `m_num`

### `m_hex32`

- Location: `src/ui_dashboard.c:81`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, uint32_t, uint8_t, uint16_t, uint16_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`, `draw_controller`
- Direct callees: `m_text`

### `clear`

- Location: `src/ui_dashboard.c:87`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `ui_boot_splash_draw`, `ui_dashboard_draw`
- Direct callees: `dpc245_lcd_fill`

### `title`

- Location: `src/ui_dashboard.c:88`
- Linkage: `static`
- Type: `void (const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_advanced`, `draw_battery`, `draw_controller`, `draw_edit`, `draw_edit_num`, `draw_errors`, `draw_hmi_info`, `draw_menu`, `draw_sensor`, `draw_speed_limit`, `draw_trip_reset`, `draw_wheel`, `ui_dashboard_draw`
- Direct callees: `m_text`

### `apply_theme`

- Location: `src/ui_dashboard.c:89`
- Linkage: `static`
- Type: `void (const dpc245_app_t *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `dpc245_theme`

### `splash_emit`

- Location: `src/ui_dashboard.c:95`
- Linkage: `static`
- Type: `void (void *, uint16_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `ui_boot_splash_draw`
- Direct callees: `dpc245_lcd_data16`

### `ui_boot_splash_draw`

- Location: `src/ui_dashboard.c:102`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `main`
- Direct callees: `clear`, `dpc245_lcd_set_window`, `m_text`, `splash_emit`

### `value_u32`

- Location: `src/ui_dashboard.c:113`
- Linkage: `static`
- Type: `void (uint16_t, const char *, uint32_t, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_u32`, `draw_adv_diag`, `draw_battery`, `draw_controller`, `draw_errors`, `draw_hmi_info`, `draw_sensor`, `draw_wheel`
- Direct callees: `m_text`, `m_u32`

### `value_i32`

- Location: `src/ui_dashboard.c:116`
- Linkage: `static`
- Type: `void (uint16_t, const char *, int32_t, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_i32`, `draw_battery`, `draw_controller`
- Direct callees: `m_i32`, `m_text`

### `value_x100`

- Location: `src/ui_dashboard.c:120`
- Linkage: `static`
- Type: `void (uint16_t, const char *, uint32_t, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`, `draw_battery`, `draw_controller`
- Direct callees: `m_fixed_u32`, `m_text`

### `value_x100_i`

- Location: `src/ui_dashboard.c:123`
- Linkage: `static`
- Type: `void (uint16_t, const char *, int32_t, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_battery`, `draw_controller`
- Direct callees: `m_fixed_u32`, `m_text`, `ui_font1_width`

### `footer`

- Location: `src/ui_dashboard.c:129`
- Linkage: `static`
- Type: `void (const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `adv_status_or_footer`, `draw_adv_diag`, `draw_advanced`, `draw_battery`, `draw_controller`, `draw_edit`, `draw_edit_num`, `draw_errors`, `draw_hmi_info`, `draw_menu`, `draw_sensor`, `draw_speed_limit`, `draw_wheel`
- Direct callees: `m_text`

### `menu_status`

- Location: `src/ui_dashboard.c:130`
- Linkage: `static`
- Type: `const char *(const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `adv_status_or_footer`, `draw_menu`
- Direct callees: —

### `metric_to_imperial`

- Location: `src/ui_dashboard.c:140`
- Linkage: `static`
- Type: `uint32_t (uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `display_metric`
- Direct callees: —

### `display_metric`

- Location: `src/ui_dashboard.c:145`
- Linkage: `static`
- Type: `uint32_t (const dpc245_app_t *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`, `draw_dashboard`, `draw_speed_limit`, `ride_speed_text`, `theme_speed`, `theme_speed_arc`, `ui_dashboard_lower_pair_text`
- Direct callees: `dpc245_unit`, `metric_to_imperial`

### `distance_unit`

- Location: `src/ui_dashboard.c:148`
- Linkage: `static`
- Type: `const char *(const dpc245_app_t *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `ui_dashboard_lower_pair_text`
- Direct callees: `dpc245_unit`

### `speed_unit`

- Location: `src/ui_dashboard.c:151`
- Linkage: `static`
- Type: `const char *(const dpc245_app_t *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`, `draw_speed_limit`
- Direct callees: `dpc245_unit`

### `dash_speed_unit`

- Location: `src/ui_dashboard.c:155`
- Linkage: `static`
- Type: `const char *(const dpc245_app_t *, bool)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_dashboard`, `theme_speed`, `theme_speed_arc`, `ui_dashboard_lower_pair_text`
- Direct callees: `dpc245_unit`

### `ride_speed_text`

- Location: `src/ui_dashboard.c:161`
- Linkage: `static`
- Type: `void (char *, const dpc245_app_t *, uint16_t, bool)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_advanced`, `draw_dashboard`
- Direct callees: `display_metric`, `dpc245_unit`, `fmt_u32_5`

### `flip_buffer`

- Location: `src/ui_dashboard.c:175`
- Linkage: `static`
- Type: `void (char *, unsigned int)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `fmt_u32_5`
- Direct callees: —

### `fmt_u32_5`

- Location: `src/ui_dashboard.c:176`
- Linkage: `static`
- Type: `void (char *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `adv_centi`, `adv_u16`, `draw_advanced`, `draw_battery_widget`, `fmt_i32_5`, `fmt_x10_5`, `ride_speed_text`, `theme_soc_text`, `ui_dashboard_lower_pair_text`
- Direct callees: `flip_buffer`

### `fmt_i32_5`

- Location: `src/ui_dashboard.c:179`
- Linkage: `static`
- Type: `void (char *, int32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_temperature_warning`, `temperature_value`
- Direct callees: `fmt_u32_5`

### `fmt_x10_5`

- Location: `src/ui_dashboard.c:182`
- Linkage: `static`
- Type: `void (char *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `fmt_x100_5`
- Direct callees: `fmt_u32_5`

### `fmt_x100_5`

- Location: `src/ui_dashboard.c:186`
- Linkage: `static`
- Type: `void (char *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `ui_dashboard_lower_pair_text`
- Direct callees: `fmt_x10_5`

### `temperature_warning_visible`

- Location: `src/ui_dashboard.c:188`
- Linkage: `static`
- Type: `bool (const dpc245_app_t *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_dashboard`, `draw_wedge`
- Direct callees: —

### `draw_temperature_warning`

- Location: `src/ui_dashboard.c:193`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, uint16_t, uint16_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_dashboard`, `draw_wedge`
- Direct callees: `fmt_i32_5`, `m_text`

### `lower_cell`

- Location: `src/ui_dashboard.c:212`
- Linkage: `static`
- Type: `void (uint16_t, const char *, const char *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_lower_panel`
- Direct callees: `dpc245_lcd_fill_rect`, `m_text`, `ui_font1_text`, `ui_font1_width`

### `temperature_value`

- Location: `src/ui_dashboard.c:228`
- Linkage: `static`
- Type: `void (const bafang_telemetry_t *, bool, char *, const char **)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `ui_dashboard_lower_pair_text`
- Direct callees: `bafang_temperature_raw_valid`, `fmt_i32_5`

### `selected_motor`

- Location: `src/ui_dashboard.c:239`
- Linkage: `static`
- Type: `bool (const dpc245_app_t *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `ui_dashboard_lower_pair_text`
- Direct callees: —

### `ui_dashboard_lower_pair_text`

- Location: `src/ui_dashboard.c:244`
- Linkage: `global`
- Type: `void (const dpc245_app_t *, const bafang_telemetry_t *, uint8_t, uint32_t, char *, char *, const char **, const char **, const char **, const char **)`
- Responsibility: Public function of the ui_dashboard module.
- Direct callers: `draw_lower_panel`, `theme_pair`
- Direct callees: `bafang_power_w`, `dash_speed_unit`, `display_metric`, `distance_unit`, `dpc245_get_u16_le`, `dpc245_get_u32_le`, `dpc245_runtime_calories_kcal`, `fmt_u32_5`, `fmt_x100_5`, `selected_motor`, `temperature_value`

### `soc_style`

- Location: `src/ui_dashboard.c:272`
- Linkage: `static`
- Type: `soc_style_t (uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_battery_bar`, `draw_battery_widget`, `draw_wedge`
- Direct callees: —

### `soc_blink_on`

- Location: `src/ui_dashboard.c:277`
- Linkage: `static`
- Type: `bool (soc_style_t, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_battery_bar`, `draw_battery_widget`
- Direct callees: —

### `round_inset`

- Location: `src/ui_dashboard.c:278`
- Linkage: `static`
- Type: `uint8_t (uint8_t, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `round_rect`
- Direct callees: —

### `round_rect`

- Location: `src/ui_dashboard.c:288`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t, uint8_t, uint16_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_mode_badge`
- Direct callees: `dpc245_lcd_fill_rect`, `round_inset`

### `badge_glyph`

- Location: `src/ui_dashboard.c:296`
- Linkage: `static`
- Type: `const ui_font1_glyph_t *(const ui_font1_t *, char)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `badge_ink_left`, `badge_ink_top`, `draw_mode_badge`, `draw_wedge`
- Direct callees: —

### `badge_ink_left`

- Location: `src/ui_dashboard.c:301`
- Linkage: `static`
- Type: `uint16_t (const ui_font1_t *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_mode_badge`
- Direct callees: `badge_glyph`

### `badge_ink_top`

- Location: `src/ui_dashboard.c:314`
- Linkage: `static`
- Type: `int (const ui_font1_t *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_mode_badge`
- Direct callees: `badge_glyph`

### `draw_battery_bar`

- Location: `src/ui_dashboard.c:330`
- Linkage: `static`
- Type: `void (const bafang_telemetry_t *, bool, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_battery_widget`
- Direct callees: `dpc245_lcd_fill_rect`, `soc_blink_on`, `soc_style`

### `draw_battery_widget`

- Location: `src/ui_dashboard.c:350`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const bafang_telemetry_t *, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_dashboard`
- Direct callees: `dpc245_lcd_fill_rect`, `draw_battery_bar`, `fmt_u32_5`, `soc_blink_on`, `soc_style`, `ui_font1_text`, `ui_font1_width`

### `draw_power_graph`

- Location: `src/ui_dashboard.c:394`
- Linkage: `static`
- Type: `void (const dpc245_app_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_dashboard`
- Direct callees: `dpc245_lcd_fill_rect`

### `draw_status_glyph`

- Location: `src/ui_dashboard.c:418`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, const uint32_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_status_icons`, `theme_status`
- Direct callees: `dpc245_lcd_fill_rect`

### `draw_status_icons`

- Location: `src/ui_dashboard.c:429`
- Linkage: `static`
- Type: `void (const dpc245_ui_status_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_dashboard`
- Direct callees: `draw_status_glyph`

### `draw_lower_panel`

- Location: `src/ui_dashboard.c:437`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const bafang_telemetry_t *, bool, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_dashboard`
- Direct callees: `dpc245_lcd_fill_rect`, `lower_cell`, `ui_dashboard_lower_pair_text`

### `draw_mode_badge`

- Location: `src/ui_dashboard.c:446`
- Linkage: `static`
- Type: `void (uint8_t, bool, bool, uint16_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_dashboard`
- Direct callees: `badge_glyph`, `badge_ink_left`, `badge_ink_top`, `round_rect`, `ui_font1_text`

### `speed_glyph`

- Location: `src/ui_dashboard.c:489`
- Linkage: `static`
- Type: `const ui_font1_glyph_t *(const ui_font1_t *, char)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_speed_digit`, `theme_speed_cell`
- Direct callees: —

### `draw_speed_digit`

- Location: `src/ui_dashboard.c:493`
- Linkage: `static`
- Type: `void (const ui_font1_t *, char, uint16_t, uint16_t, uint16_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_speed`
- Direct callees: `speed_glyph`, `ui_font1_text_clipped`

### `draw_speed`

- Location: `src/ui_dashboard.c:502`
- Linkage: `static`
- Type: `void (uint32_t, uint16_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: —
- Direct callees: `draw_speed_digit`, `ui_font1_text_clipped`

### `draw_dashboard`

- Location: `src/ui_dashboard.c:521`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *, const dpc245_ui_status_t *, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `bafang_walk_bit_fresh`, `dash_speed_unit`, `display_metric`, `dpc245_lcd_fill_rect`, `dpc245_runtime_service_tip_active`, `draw_battery_widget`, `draw_lower_panel`, `draw_mode_badge`, `draw_power_graph`, `draw_status_icons`, `draw_temperature_warning`, `m_text`, `ride_speed_text`, `temperature_warning_visible`, `ui_font1_text`, `ui_font1_width`

### `theme_clear_regions`

- Location: `src/ui_dashboard.c:566`
- Linkage: `static`
- Type: `void (const theme_region_t *, size_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: `dpc245_lcd_fill_rect`

### `power_zone_pct`

- Location: `src/ui_dashboard.c:570`
- Linkage: `static`
- Type: `uint16_t (uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: —

### `power_pct`

- Location: `src/ui_dashboard.c:576`
- Linkage: `static`
- Type: `uint8_t (const dpc245_app_t *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: —

### `theme_mode_colour`

- Location: `src/ui_dashboard.c:581`
- Linkage: `static`
- Type: `uint16_t (const dpc245_app_t *, const bafang_telemetry_t *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: `bafang_walk_bit_fresh`

### `theme_mode_name`

- Location: `src/ui_dashboard.c:585`
- Linkage: `static`
- Type: `const char *(const dpc245_app_t *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`
- Direct callees: —

### `theme_mode_letter`

- Location: `src/ui_dashboard.c:590`
- Linkage: `static`
- Type: `char (const dpc245_app_t *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_wedge`
- Direct callees: —

### `theme_speed_cell`

- Location: `src/ui_dashboard.c:595`
- Linkage: `static`
- Type: `void (const ui_font1_t *, char, uint16_t, uint16_t, int, uint16_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `theme_speed`, `theme_speed_arc`
- Direct callees: `speed_glyph`, `ui_font1_text_clipped`

### `theme_speed`

- Location: `src/ui_dashboard.c:602`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const bafang_telemetry_t *, uint16_t, int, int)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_wedge`
- Direct callees: `dash_speed_unit`, `display_metric`, `theme_speed_cell`, `ui_font1_text_clipped`

### `theme_isqrt`

- Location: `src/ui_dashboard.c:615`
- Linkage: `static`
- Type: `uint16_t (uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: —

### `theme_pair`

- Location: `src/ui_dashboard.c:621`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const bafang_telemetry_t *, uint32_t, char *, char *, const char **, const char **, const char **, const char **)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: `ui_dashboard_lower_pair_text`

### `theme_right_text`

- Location: `src/ui_dashboard.c:625`
- Linkage: `static`
- Type: `void (const char *, const ui_font1_t *, uint16_t, int, uint16_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `arc_tile`, `draw_wedge`
- Direct callees: `ui_font1_text`, `ui_font1_width`

### `arc_tile`

- Location: `src/ui_dashboard.c:629`
- Linkage: `static`
- Type: `void (bool, const char *, const char *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`
- Direct callees: `dpc245_lcd_fill_rect`, `theme_right_text`

### `theme_speed_arc`

- Location: `src/ui_dashboard.c:640`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const bafang_telemetry_t *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`
- Direct callees: `dash_speed_unit`, `display_metric`, `theme_speed_cell`, `ui_font1_text_clipped`

### `theme_status`

- Location: `src/ui_dashboard.c:650`
- Linkage: `static`
- Type: `void (const dpc245_ui_status_t *, bool)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: `draw_status_glyph`

### `theme_soc_text`

- Location: `src/ui_dashboard.c:656`
- Linkage: `static`
- Type: `void (const bafang_telemetry_t *, char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: `fmt_u32_5`

### `theme_banner`

- Location: `src/ui_dashboard.c:661`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, uint16_t, uint16_t, uint16_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_arc`, `draw_wedge`
- Direct callees: `dpc245_lcd_fill_rect`, `m_text`

### `draw_arc`

- Location: `src/ui_dashboard.c:667`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *, const dpc245_ui_status_t *, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `arc_tile`, `dpc245_lcd_fill_rect`, `draw_temperature_warning`, `power_pct`, `power_zone_pct`, `soc_style`, `temperature_warning_visible`, `theme_banner`, `theme_clear_regions`, `theme_isqrt`, `theme_mode_colour`, `theme_mode_name`, `theme_pair`, `theme_soc_text`, `theme_speed_arc`, `theme_status`, `ui_font1_text`

### `draw_wedge`

- Location: `src/ui_dashboard.c:718`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *, const dpc245_ui_status_t *, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `badge_glyph`, `dpc245_lcd_fill_rect`, `draw_temperature_warning`, `power_pct`, `power_zone_pct`, `soc_style`, `temperature_warning_visible`, `theme_banner`, `theme_clear_regions`, `theme_isqrt`, `theme_mode_colour`, `theme_mode_letter`, `theme_pair`, `theme_right_text`, `theme_soc_text`, `theme_speed`, `theme_status`, `ui_font1_text`

### `draw_menu`

- Location: `src/ui_dashboard.c:778`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const char *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `dpc245_app_menu_count`, `dpc245_app_menu_label`, `dpc245_app_theme_row_value`, `dpc245_lcd_fill_rect`, `footer`, `m_text`, `menu_status`, `title`

### `draw_edit`

- Location: `src/ui_dashboard.c:799`
- Linkage: `static`
- Type: `void (const char *, const char *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `footer`, `m_text`, `title`

### `draw_edit_num`

- Location: `src/ui_dashboard.c:802`
- Linkage: `static`
- Type: `void (const char *, uint32_t, const char *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `footer`, `title`, `ui_font1_text`

### `draw_hmi_info`

- Location: `src/ui_dashboard.c:809`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const dpc245_hwinfo_t *, uint8_t, bool, bool)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `footer`, `m_text`, `title`, `value_u32`

### `draw_wheel`

- Location: `src/ui_dashboard.c:825`
- Linkage: `static`
- Type: `void (const evist_can_router_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `footer`, `m_fixed_u32`, `m_text`, `title`, `value_u32`

### `draw_speed_limit`

- Location: `src/ui_dashboard.c:831`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `display_metric`, `footer`, `m_text`, `speed_unit`, `title`

### `value_text`

- Location: `src/ui_dashboard.c:834`
- Linkage: `static`
- Type: `void (uint16_t, const char *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_i32`, `diag_live_source`, `diag_u32`, `draw_adv_diag`, `draw_battery`, `draw_controller`, `draw_sensor`
- Direct callees: `m_text`, `ui_font1_width`

### `raw4_le`

- Location: `src/ui_dashboard.c:839`
- Linkage: `static`
- Type: `uint32_t (const uint8_t *, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_battery`
- Direct callees: —

### `draw_page_no`

- Location: `src/ui_dashboard.c:843`
- Linkage: `static`
- Type: `void (uint8_t, size_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_adv_diag`, `draw_battery`
- Direct callees: `ui_font1_width`

### `draw_battery`

- Location: `src/ui_dashboard.c:849`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `dpc245_app_detail_page_count`, `draw_page_no`, `footer`, `m_text`, `m_u32`, `raw4_le`, `title`, `value_i32`, `value_text`, `value_u32`, `value_x100`, `value_x100_i`

### `draw_controller`

- Location: `src/ui_dashboard.c:892`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `bafang_temperature_raw_valid`, `dpc245_app_detail_page_count`, `footer`, `m_hex32`, `m_text`, `title`, `value_i32`, `value_text`, `value_u32`, `value_x100`, `value_x100_i`

### `draw_sensor`

- Location: `src/ui_dashboard.c:914`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *, const dpc245_adc_snapshot_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `dpc245_app_detail_page_count`, `footer`, `title`, `value_text`, `value_u32`

### `draw_errors`

- Location: `src/ui_dashboard.c:933`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `dpc245_error_at`, `dpc245_error_count`, `footer`, `m_text`, `m_u32`, `title`, `value_u32`

### `draw_trip_reset`

- Location: `src/ui_dashboard.c:951`
- Linkage: `static`
- Type: `void (void)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `m_text`, `title`

### `adv_row`

- Location: `src/ui_dashboard.c:956`
- Linkage: `static`
- Type: `void (uint16_t, const char *, const char *, bool, bool)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_assist`, `draw_adv_wheel`, `draw_advanced`
- Direct callees: `dpc245_lcd_fill_rect`, `m_text`

### `adv_u16`

- Location: `src/ui_dashboard.c:961`
- Linkage: `static`
- Type: `void (char *, uint16_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_wheel`, `draw_advanced`
- Direct callees: `fmt_u32_5`

### `adv_centi`

- Location: `src/ui_dashboard.c:963`
- Linkage: `static`
- Type: `void (char *, uint16_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_wheel`
- Direct callees: `fmt_u32_5`

### `adv_status_or_footer`

- Location: `src/ui_dashboard.c:967`
- Linkage: `static`
- Type: `void (const evist_advanced_t *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_assist`, `draw_adv_wheel`, `draw_advanced`
- Direct callees: `footer`, `menu_status`

### `draw_adv_wheel`

- Location: `src/ui_dashboard.c:970`
- Linkage: `static`
- Type: `void (const evist_advanced_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_advanced`
- Direct callees: `adv_centi`, `adv_row`, `adv_status_or_footer`, `adv_u16`, `evist_advanced_wheel_diameter_label`, `m_text`

### `draw_adv_assist`

- Location: `src/ui_dashboard.c:984`
- Linkage: `static`
- Type: `void (const evist_advanced_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_advanced`
- Direct callees: `adv_row`, `adv_status_or_footer`, `evist_advanced_assist_cell_value`, `evist_advanced_assist_cells`, `m_text`

### `diag_u32`

- Location: `src/ui_dashboard.c:1004`
- Linkage: `static`
- Type: `void (uint16_t, const char *, bool, uint32_t, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`
- Direct callees: `value_text`, `value_u32`

### `diag_i32`

- Location: `src/ui_dashboard.c:1007`
- Linkage: `static`
- Type: `void (uint16_t, const char *, bool, int32_t, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`
- Direct callees: `value_i32`, `value_text`

### `diag_append`

- Location: `src/ui_dashboard.c:1010`
- Linkage: `static`
- Type: `void (char *, unsigned int *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_elapsed_ms`, `diag_live_source`, `diag_signed`, `draw_adv_diag`
- Direct callees: —

### `diag_number`

- Location: `src/ui_dashboard.c:1014`
- Linkage: `static`
- Type: `void (char *, unsigned int *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_elapsed_ms`, `diag_live_source`, `diag_signed`, `draw_adv_diag`
- Direct callees: —

### `diag_elapsed_ms`

- Location: `src/ui_dashboard.c:1020`
- Linkage: `static`
- Type: `void (char *, unsigned int *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_live_source`
- Direct callees: `diag_append`, `diag_number`

### `diag_signed`

- Location: `src/ui_dashboard.c:1024`
- Linkage: `static`
- Type: `void (char *, unsigned int *, int16_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `diag_live_source`, `draw_adv_diag`
- Direct callees: `diag_append`, `diag_number`

### `diag_hex_byte`

- Location: `src/ui_dashboard.c:1028`
- Linkage: `static`
- Type: `void (char *, unsigned int *, uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`
- Direct callees: —

### `diag_sources`

- Location: `src/ui_dashboard.c:1032`
- Linkage: `static`
- Type: `const char *(uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`
- Direct callees: —

### `diag_stage`

- Location: `src/ui_dashboard.c:1040`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, const char *, uint32_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`
- Direct callees: `m_text`, `m_u32`

### `ble_reject_text`

- Location: `src/ui_dashboard.c:1045`
- Linkage: `static`
- Type: `const char *(uint8_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`
- Direct callees: —

### `diag_result_text`

- Location: `src/ui_dashboard.c:1056`
- Linkage: `static`
- Type: `const char *(evist_txn_result_t)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: `draw_adv_diag`
- Direct callees: `evist_txn_result_text`

### `diag_live_source`

- Location: `src/ui_dashboard.c:1065`
- Linkage: `static`
- Type: `void (uint16_t, const dpc245_temp_live_source_t *, bool, int16_t, const char *, const char *, const char *)`
- Responsibility: Internal helper of the ui_dashboard module.
- Direct callers: —
- Direct callees: `diag_append`, `diag_elapsed_ms`, `diag_number`, `diag_signed`, `value_text`

### `draw_adv_diag`

- Location: `src/ui_dashboard.c:1087`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *, uint8_t, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `draw_advanced`
- Direct callees: `bafang_power_w`, `bafang_temperature_raw_valid`, `ble_reject_text`, `diag_append`, `diag_hex_byte`, `diag_i32`, `diag_number`, `diag_result_text`, `diag_signed`, `diag_sources`, `diag_stage`, `diag_u32`, `display_metric`, `draw_page_no`, `footer`, `m_hex32`, `m_text`, `m_u32`, `speed_unit`, `value_text`, `value_u32`, `value_x100`

### `draw_advanced`

- Location: `src/ui_dashboard.c:1224`
- Linkage: `static`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *, uint32_t)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `ui_dashboard_draw`
- Direct callees: `adv_row`, `adv_status_or_footer`, `adv_u16`, `draw_adv_assist`, `draw_adv_diag`, `draw_adv_wheel`, `evist_advanced_menu_label`, `evist_advanced_row_count`, `evist_advanced_screen_title`, `evist_assist_level_slot`, `fmt_u32_5`, `footer`, `m_text`, `ride_speed_text`, `title`, `ui_dashboard_temp_limit_text`, `ui_font1_text`

### `ui_dashboard_draw`

- Location: `src/ui_dashboard.c:1313`
- Linkage: `global`
- Type: `void (const dpc245_app_t *, const evist_can_router_t *, const dpc245_adc_snapshot_t *, const dpc245_hwinfo_t *, uint8_t, bool, bool, bool, uint32_t, const dpc245_ui_status_t *)`
- Responsibility: Drawing routine in the ui_dashboard module.
- Direct callers: `screen_draw`
- Direct callees: `apply_theme`, `clear`, `dpc245_al_sensitivity`, `dpc245_app_menu_count`, `dpc245_auto_off_min`, `dpc245_brightness_level`, `dpc245_service_tips`, `dpc245_theme`, `dpc245_unit`, `draw_advanced`, `draw_arc`, `draw_battery`, `draw_controller`, `draw_dashboard`, `draw_edit`, `draw_edit_num`, `draw_errors`, `draw_hmi_info`, `draw_menu`, `draw_sensor`, `draw_speed_limit`, `draw_trip_reset`, `draw_wedge`, `draw_wheel`, `m_text`, `title`

## `src/ui_strip.c`

Module implementation.

Functions: **15**

### `ui_strip_capturing`

- Location: `src/ui_strip.c:16`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `dpc245_lcd_blit_rgb565`, `dpc245_lcd_data16`, `dpc245_lcd_fill`, `dpc245_lcd_fill_rect`, `dpc245_lcd_set_window`
- Direct callees: —

### `ui_strip_last_frame_ops`

- Location: `src/ui_strip.c:19`
- Linkage: `global`
- Type: `uint32_t (void)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: —
- Direct callees: —

### `ui_strip_rows`

- Location: `src/ui_strip.c:21`
- Linkage: `global`
- Type: `_Bool (uint16_t *, uint16_t *)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `text_clipped`
- Direct callees: —

### `ui_strip_invalidate`

- Location: `src/ui_strip.c:28`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `main`
- Direct callees: `ui_strip_render_active`

### `put`

- Location: `src/ui_strip.c:34`
- Linkage: `static`
- Type: `void (uint16_t, uint16_t, uint16_t)`
- Responsibility: Internal helper of the ui_strip module.
- Direct callers: `ui_strip_blit`, `ui_strip_data16`
- Direct callees: —

### `ui_strip_set_window`

- Location: `src/ui_strip.c:40`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t)`
- Responsibility: Setter in the ui_strip module.
- Direct callers: `dpc245_lcd_set_window`
- Direct callees: —

### `ui_strip_data16`

- Location: `src/ui_strip.c:45`
- Linkage: `global`
- Type: `void (uint16_t)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `dpc245_lcd_data16`
- Direct callees: `put`

### `ui_strip_fill_rect`

- Location: `src/ui_strip.c:55`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t, uint16_t)`
- Responsibility: Fill routine in the ui_strip module.
- Direct callers: `dpc245_lcd_fill`, `dpc245_lcd_fill_rect`
- Direct callees: —

### `ui_strip_blit`

- Location: `src/ui_strip.c:67`
- Linkage: `global`
- Type: `void (uint16_t, uint16_t, uint16_t, uint16_t, const uint16_t *)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `dpc245_lcd_blit_rgb565`
- Direct callees: `put`

### `tile_crc32`

- Location: `src/ui_strip.c:78`
- Linkage: `static`
- Type: `uint32_t (uint16_t)`
- Responsibility: Internal helper of the ui_strip module.
- Direct callers: `ui_strip_render_step`
- Direct callees: —

### `ui_strip_render_active`

- Location: `src/ui_strip.c:88`
- Linkage: `global`
- Type: `_Bool (void)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `main`, `ui_strip_invalidate`, `ui_strip_render`, `ui_strip_render_begin`, `ui_strip_render_step`
- Direct callees: —

### `ui_strip_render_cancel`

- Location: `src/ui_strip.c:90`
- Linkage: `global`
- Type: `void (void)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `main`
- Direct callees: —

### `ui_strip_render_begin`

- Location: `src/ui_strip.c:95`
- Linkage: `global`
- Type: `void (void (*)(void *), void *, ui_strip_push_fn)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `ui_strip_render`
- Direct callees: `ui_strip_render_active`

### `ui_strip_render_step`

- Location: `src/ui_strip.c:104`
- Linkage: `global`
- Type: `uint16_t (uint16_t)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: `main`, `ui_strip_render`
- Direct callees: `tile_crc32`, `ui_strip_render_active`

### `ui_strip_render`

- Location: `src/ui_strip.c:134`
- Linkage: `global`
- Type: `uint16_t (void (*)(void *), void *, ui_strip_push_fn)`
- Responsibility: Public function of the ui_strip module.
- Direct callers: —
- Direct callees: `ui_strip_render_active`, `ui_strip_render_begin`, `ui_strip_render_step`
