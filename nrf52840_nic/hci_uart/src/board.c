/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/regulator/nrf5x.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <hal/nrf_power.h>
#include <soc/nrfx_coredep.h>
#include <zephyr/logging/log.h>

#include <cmsis_core.h>

#include <zephyr/init.h>
#include <hal/nrf_power.h>

// // #define LOG_MODULE_NAME board
// // LOG_MODULE_REGISTER(LOG_MODULE_NAME);

// static int board_nrf52840dongle_nrf52840_init(void)
// {
// 	uint32_t testCnt = 0;
// 	// LOG_DBG("Start");

// 	testCnt++;
// 	// if((testCnt > 0) && (nrf_power_mainregstatus_get(NRF_POWER) ==
// 	//       NRF_POWER_MAINREGSTATUS_HIGH))
// 	// {
// 	// 	return 0;
// 	// }
// 	/* if the nrf52840dongle_nrf52840 board is powered from USB
// 	 * (high voltage mode), GPIO output voltage is set to 1.8 volts by
// 	 * default and that is not enough to turn the green and blue LEDs on.
// 	 * Increase GPIO voltage to 3.0 volts.
// 	 */
// 	// if ((nrf_power_mainregstatus_get(NRF_POWER) ==
// 	//      NRF_POWER_MAINREGSTATUS_HIGH) &&
// 	//     ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
// 	//      (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))) {
// 	// testCnt++;

// 	// 	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
// 	// 	while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
// 	// 		;
// 	// 	}

// 	// 	NRF_UICR->REGOUT0 =
// 	// 	    (NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
// 	// 	    (UICR_REGOUT0_VOUT_1V8 << UICR_REGOUT0_VOUT_Pos);

// 	// 	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
// 	// 	while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
// 	// 		;
// 	// 	}

// 	// 	/* a reset is required for changes to take effect */
// 	// 	NVIC_SystemReset();
// 	// }

// 	return 0;
// }

// SYS_INIT(board_nrf52840dongle_nrf52840_init, APPLICATION,
// 	 CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
