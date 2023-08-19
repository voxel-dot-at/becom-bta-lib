///  @file bta_flash_update.h
///
///  @brief This header file contains enums and structs for flash update
///
///  Copyright BECOM Systems GmbH 2019
///
///  @author Alex Falkensteiner
///
///  @cond svn
///
///  Information of last commit
///  $Rev::               $:  Revision of last commit
///  $Author::            $:  Author of last commit
///  $Date::              $:  Date of last commit
///
///  @endcond
///

#ifndef BTA_FLASH_UPDATE_H_INCLUDED
#define BTA_FLASH_UPDATE_H_INCLUDED

#define BTA_FLASH_UPDATE_H_VER_MAJ 3
#define BTA_FLASH_UPDATE_H_VER_MIN 3
#define BTA_FLASH_UPDATE_H_VER_NON_FUNC 11

#include <stdint.h>



///     @brief  Callback function to report status and progress during transfer and programming
///     @param  status Please refer to bta_status.h
///     @param  percentage Contains the progress in [%].
///                          0: File transfer started (can only be reported once per file transfer).
///                        100: File transfer finished (can only be reported once per file transfer).
typedef int (BTA_CALLCONV *FN_BTA_ProgressReport)(BTA_Status status, uint8_t percentage);



///  @brief BTA_FlashTarget describes the kind of data transmitted as well as
///         what the intended target (in the device/memory) of the data is
typedef enum BTA_FlashTarget {
    BTA_FlashTargetBootloader,
    BTA_FlashTargetApplication,
    BTA_FlashTargetGeneric,
    BTA_FlashTargetPixelList,               // obsolete
    BTA_FlashTargetLensCalibration,
    BTA_FlashTargetOtp,
    BTA_FlashTargetFactoryConfig,
	BTA_FlashTargetWigglingCalibration,
    BTA_FlashTargetIntrinsicTof,            // obsolete
    BTA_FlashTargetIntrinsicColor,          // obsolete
    BTA_FlashTargetExtrinsic,               // obsolete
    BTA_FlashTargetAmpCompensation,         // obsolete
    BTA_FlashTargetFpn,
    BTA_FlashTargetFppn,
    BTA_FlashTargetGeometricModelParameters,
    BTA_FlashTargetOverlayCalibration,
    BTA_FlashTargetPredefinedConfig,
    BTA_FlashTargetDeadPixelList,
    BTA_FlashTargetXml,
    BTA_FlashTargetLogFiles,
} BTA_FlashTarget;



///  @brief BTA_FlashId may be needed to further specify the BTA_FlashTarget
typedef enum BTA_FlashId {
    BTA_FlashIdSpi,
    BTA_FlashIdParallel,
    BTA_FlashIdEmmc,
    BTA_FlashIdSd,
    BTA_FlashIdTim,
    BTA_FlashIdLim
} BTA_FlashId;



///  @brief This configuration structure contains all the data and parameters needed for a BTAflashUpdate
typedef struct BTA_FlashUpdateConfig {
    BTA_FlashTarget target;         ///< Type of update, indicating the target where to copy the data to
    BTA_FlashId flashId;            ///< Parameter to distinguish between different flash modules on the device
    uint32_t address;               ///< Address within the specified memory. Special case: if target is BTA_FlashTargetGeometricModelParameters and if reading from flash this is used as lensIndex
    uint8_t *data;                  ///< Data to be transmitted and saved
    uint32_t dataLen;               ///< Size of data in bytes
} BTA_FlashUpdateConfig;

#endif
