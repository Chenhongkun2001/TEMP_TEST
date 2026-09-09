# Technical Changelog (Since 2025-10-10)

## PredictGateway-2.0.5
**Date:**  2026-05-14 13:00:07
**Branch:** gw_new_for_merge
**Commit:**  4704536cc0effdca37729afd2fd4dc20e1e44422 [4704536c]
**Messages:**
1. Improve reliability.
    1. [update]try OTA no matter what the result of collecting data & prepare for 2.0.5
    1. [bugfix]fix wrong adv pkt in server role issue
    1. [update]support pro sensor(not include modbus)
    1. [update]try to aviod race condition while uploading sensordata
    1. [bugfix]fix device keep in paired status after disconnected-by-local and update finding connect-index method
    1. [update]sync install scripts for production
    1. [bugfix]fix sh script issue
    1. [update]create cron dir first to adapt for product version

## PredictGateway Version 2.0.4
**Date:**  2026-04-02 9:27:07
**Branch:** gw_new_for_merge
**Commit:**  ca7e3c6321613f3719e407b5327368a178dc34a1 [ca7e3c6]
**Messages:**
1. Improve reliability.
    1. [bugfix]fix wrong json file & update restart script of crontab
    1. [bugfix]fix reboot not work issue
    1. [update]support magicFile.json.user for mqtt_op
    1. [update]update default log level at prj&file level
    1. [bugfix]fix logging issue & move 5 fields from sensorConf into algoConfig
    1. [update]update log level controlling & partial code cleanup(hci_mgmt)
    1. [update]Update the changelog and update .bin files.
    1. [manu] Modify the access control of files.

## PredictGatewayPro Version 2.0.4 (-8 dBm)
**Date:**  2026-04-02 13:30:31
**Branch:** gw_new_for_merge
**Commit:**  587a0f2cd42256a41ecaa800c0fca6392f222143 [587a0f2]
**Messages:**
1. Improve reliability.
    1. [update]add support for user-define magicFile.json.user ( magicFile.json will take effect when magicFile.json.user does not exist )

## PredictGateway Version 2.0.3
**Date:**  2026-02-11 12:04:05
**Branch:** gw_new_for_merge
**Commit:** dca8d322f3eb79f76ecb017c7f8c3fd5845a4b38 [dca8d32]
**Messages:**
1. Improve memory.
    1. [perf] Optimize the shared memory used by sql_op.
1. Improve reliability.
    1. [bugFix] Optimize FUOTA by killing skf_gw_1 before upgrading.
    1. [bugFix] Fix periodic device update of Insight-T.
    1. [manu] Update the files and libs required by manufacturer.

## PredictGatewayPro Version 1.99.4 (-8 dBm)
**Date:** 2026-02-11 14:21:59
**Branch:** gw_new_for_merge
**Commit:** b8894e350d0620dd3eba53a99eee5cd4dc134018 [b8894e3]
**Messages:**
1. Improve memory.
    1. [perf] Optimize the shared memory used by sql_op and skf_gw_mod.
1. Improve reliability.
    1. [bugFix] check the shared memory first before creating in sql_op
    1. [bugFix] fix the problem that seiral port configuration is mapped to the wrong value

## PredictGateway Version 2.0.2
**Date:**  2026-01-22 14:26:59
**Branch:** gw_new_for_merge
**Commit:** 8453f7f002d3132c6f1047f563799092aa7e73db [8453f7f]
**Messages:**
1. Improve reliability.
    1. [bugFix/manu] Fix the typo of the logrotate running (N.B.: in previous version, the log would exhaust the memory!).
    1. [bugFix] Modify the waitpid to avoid defuncted process.
    1. [bugFix] Device list can be updated in runtime.

## PredictGateway Version 2.0.1
**Date:** 2026-01-08 13:51:18
**Branch:** gw_new_for_merge
**Commit:** b3a997a23c8fcb69d56a2485e098dfebc950a75b [b3a997a]
**Messages:**
1. Improve reliability.
    1. [bugFix] Mofify the order of sending command and FUOTA (originally sending command before FUOTA may cause no FUOTA would run forever).
    1. [bugFix] Modify the logrotate running period (originally the logrotate would be called per 12 hours and be called by the "restart_mqtt.sh" additionally, which may cause logrotate failure and memory exhaustion).

## PredictGateway Version 2.0.0
**Date:** 2025-12-29 9:45:32
**Branch:** gw_new_for_merge
**Commit:** 57ab26e60a849fc8a7d24352462a7bf9c6776b1a   [57ab26e]
**Messages:**
1. Improve reliability.
    1. [bugFix] Fix the bug that unkonwn measurement type cannot be parsed, which can be used to parse there is no "selected" data in the reply of data selection of Froto.
    1. [bugFix] Fix the bug that no-adapter-can-be-found when the application is just started by adding force to reboot the gateway. N.B.: This is the only way to recover the AW-CM358 according to forlinx.
    1. [bugFix] Fix the bug that ACC_STATUS (i.e., the 3-axial acceleration data) cannot be uploaded to the backend.
    1. [bugFix] Fix the bug that the gateway cannot remove the device from the devices of bluetoothctl which cause some device cannot be scanned and connected forever.

## PredictGateway Version 1.99.2
**Date:** 2025-12-15 9:31:33
**Branch:** gw_new_for_merge
**Commit:** 43032856f23bbd7a0d388da474d4f75c6afa434c  [4303285]
**Messages:**
1. Support 3-axial acceleration data uploading
    1. [newFeat] Support the work mode parameters of sensor: codec of JSON, Froto, and database. In this way, the 3-axial acceleration data uploading can be enabled/disabled by this parameter.
    1. [newFeat] Support 3-axial data selection by updating Froto.
1. Improve reliability.   
    1. [newFeat] Support command dissemination of Froto to close the BLE session explicitly and adjust the order of fuota and sendCmd.
    1. [bugFix] Fix bug: fail-to-collect-wave-of-manual-trigger.
    1. [style] Clean the macro ENABLE_MODBUS_FEATURE.
    1. [bugFix] Fix the bug that no-adapter-can-be-found after restarting bluez by adding force to reboot the gateway. N.B.: This is the only way to recover the AW-CM358 according to forlinx.
    1. [bugFix] Remove devices once sensor has just been disconnected to avoid sensor advertisement cannot be found.
    1. [bugFix] Avoid sending command to the BLE process to disconnect a sensor once the sensor has just been disconnected (passively). This way avoid mismatch of "index" in the general process and in the BLE process.
    1. [bugFix] Support retry once unexpected packets are received or timeout happens (which generally means some packets are missed), which improve the reliability significantly.
1. Others:
    1. Add WaveThumb data compression (but not be integrated)

## PredictGateway Version 1.99.1
**Date:** 2025-11-05 14:05:33
**Branch:** gw_new_for_merge
**Commit:** 3fe2bfe0ea0a47d740e4fb22a11f05218ae275ee [3fe2bfe]
**Messages:**
1. Support FUOTA from 1.0.x.
    1. [deploy] Support FUOTA from 1.0.x to current version.
1. Improve reliability.   
    1. [bugFix] Fix LED display.
    1. [bugFix] Fix a typo in logging message.
    1. [style] Code clean of app_tim_service.c and .h.

## PredictGateway Version 1.99.0
**Date:** 2025-11-03 15:01:40
**Branch:** gw_new_for_merge
**Commit:** 6f2167317e93aecc2caee0bb407f7acc5569f8e6 [6f21673]
**Messages:**
1. A new feature of support multiple BLE connection is brought by a 9-month code refactor.
    1. [refactor] Refactor BLE connection framework.
    1. [refactor] Refactor codec of Froto.
    1. [refactor] Refactor the test framework.
1. Improve reliability.
    1. [bugFix] Upgrade libell to 0.79.

## PredictGateway Version 1.0.11
**Date:** 2025-10-21 17:25:52
**Branch:** Master
**Commit:** 77b07bb4c289ae00f27a0dfef9f8b62126d4137f [77b07bb]
**Messages:**
1. Improve the reliability.
    1. [bugFix] Force to restart the application when scanning nothing in BT_EVENT_TO_SCANNING mode to address the bug "the gateway cannot find the sensor for long time".

## PredictGatewayPro Version 1.99.3 (-8 dBm)
**Date:** 2026-02-04 8:48:56
**Branch:** gw_new_485
**Commit:** 5d78d1a923d35080c5141ddb010fbe0283bd1572 [5d78d1a]
**Messages:**
1. Support multi-connection MODBUS
    1. [bugFix] refurbish the register remapping feature of MODBUS.
    1. [feat] Support MODBUS when multiple sensors being connected simultaneously and support data ready register to show which data is valid (i.e., new collected).
    1. [feat] Support endieness configuration of MODBUS.
1. Improve reliability.
    1. [bugFix] Optimize FUOTA by killing skf_gw_1 before upgrading.
    1. [bugFix] Fix periodic device update of Insight-T.
    1. [manu] Update the files and libs required by manufacturer.

## PredictGatewayPro Version 1.99.2
A variant of 1.99.1 (with -8 dBm nrf52840-NIC Tx power).

## PredictGatewayPro Version 1.99.1
**Date:** 2026-01-22 13:23:57
**Branch:** gw_new_for_merge
**Commit:** 72f2c874d35abc9220461da448ffc3dad50b5fb7 [72f2c87]
**Messages:**
1. Improve reliability.
    1. [bugFix/manu] Fix the typo of the logrotate running (N.B.: in previous version, the log would exhaust the memory!).
    1. [bugFix] Fix the bug that the gateway may fail to get correct gateway address when switching from cellular mode.
    1. [bugFix] Modify the waitpid to avoid defuncted process.
    1. [bugFix] Device list can be updated in runtime.
    1. [bugFix] The default tx-power of nrf52840-NIC is set to -20 dBm and the bug failed-to-set-a-negative-txpower-level is fixed.

## PredictGatewayPro Version 1.99.0
**Date:** 2026-01-12 10:41:57
**Branch:** gw_new_for_merge
**Commit:** 5457eda44536bf106acc4225faa4431fa4553f18 [5457eda]
**Messages:**
1. A new feature of support multiple BLE connection is brought by a 9-month code refactor.
    1. [refactor] Refactor BLE connection framework.
    1. [refactor] Refactor codec of Froto.
    1. [refactor] Refactor the test framework.
1. Support 3-axial acceleration data uploading
    1. [newFeat] Support the work mode parameters of sensor: codec of JSON, Froto, and database. In this way, the 3-axial acceleration data uploading can be enabled/disabled by this parameter.
    1. [newFeat] Support 3-axial data selection by updating Froto.
1. Improve reliability.
    1. [bugFix] Upgrade libell to 0.79.
    1. [bugFix] Fix baudrate error of dtm_tool.
    1. [bugFix] Fix the bug that no-adapter-can-be-found when the application is just started by adding force to reboot the gateway. N.B.: This is the only way to recover the AW-CM358 according to forlinx.
    1. [bugFix] Fix the bug that the gateway cannot remove the device from the devices of bluetoothctl which cause some device cannot be scanned and connected forever.
    1. [newFeat] Support command dissemination of Froto to close the BLE session explicitly and adjust the order of fuota and sendCmd.
    1. [bugFix] Fix the nrf52840-NIC's firmare FUOTA.
    1. [manu] Fix the bug of nrf-52840-NIC in manufacturing stage.
    1. [bugFix] Modify the logrotate running period (originally the logrotate would be called per 12 hours and be called by the "restart_mqtt.sh" additionally, which may cause logrotate failure and memory exhaustion).
1. Others:
    1. Add WaveThumb data compression (but not be integrated)

## PredictGatewayPro Version 1.0.2
**Date:** 2025-12-24 10:27:40
**Branch:** Master
**Commit:** dbc96dc426804b0f00930f6217eec2299d329675  [dbc96dc]
**Messages:**
1. Release nrf-52840-NIC DTM firmware v1.0.1.
    1. [bugFix] Enable nrf52840-nic dtm to support fem and to adjust the tx power.
    1. [bugFix] Fix the bug that the dtm_tool cannot set the baudrate.
    1. [docs] Revise the examples 1 and 2 in the instruction document of DTM.
1. Support adjustable tx-power of nrf-52840-NIC.
    1. [newFeat] Support adjustable tx-power of nrf-52840-NIC from cloud backend and mobile APP.
    1. [newFeat] Support txpower configuration of -40 dbm.
1. Support FUOTA of firmware of nrf-52840-NIC.
    1. [newFeat] Support FUOTA of firmware of nrf-52840-NIC and this function can be upgraded FUOTA.
    1. [manu] Support FUOTA of firmware of nrf-52840-NIC in manufacturing stage.
1. Improve the reliability.
    1. [bugFix] Force to restart the application when scanning nothing in BT_EVENT_TO_SCANNING mode to address the bug "the gateway cannot find the sensor for long time".
    1. [bugFix] Fix the failed-to-fuota-with-nrf52840-NIC bug.

## PredictGatewayPro Version 1.0.1
**Date:** 2025-10-10 15:39:42
**Branch:** Master
**Commit:** 4c67d47670f87351634a02c29d5d9cb04f7ed513 [4c67d47]
**Messages:**
1. Support the new feature: modbus register remapping.
    1. [docs] Add some explanations of the feature “modbus register remapping”.
    1. [newFeat] Support modbus register remapping by using a magic file (magicFile.json).
1. Improve the reliability.
    1. [bugFix] Fix the failed-to-fuota-with-nrf52840-NIC bug.
    1. [bugFix] Fix the bug "the gateway fails to release the file descriptor resource in the process of USB device (i.e., the LTE module) checking".
    1. [bugFix] Modify timestamping in logging to avoid multiple threads deadlock to address the bug "the gateway cannot find the sensor for long time".
    1. [bugFix] Fix bug mqtt client cannot receive downlink messages from cloud when the mqtt process has been running for long time.
    1. [bugFix] Fix the bug that the gateway failed to trigger an auto ack when a configuration from cloud is received (to upload modbus).
    1. [bugFix] Fix bug in autorun.sh to avoid the failure of running script in some gateways.
    1. [bugFix] Fix typos in the sql process.

