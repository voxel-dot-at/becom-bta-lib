///  @file bta_discovery.h
///
///  @brief This header file contains enums and structs regarding discovery functions and device identification
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

#ifndef BTA_DISCOVERY_H_INCLUDED
#define BTA_DISCOVERY_H_INCLUDED

#define BTA_DISCOVERY_H_VER_MAJ 3
#define BTA_DISCOVERY_H_VER_MIN 3
#define BTA_DISCOVERY_H_VER_NON_FUNC 11


#include <stdint.h>

///  @brief All interfaces currently known to the SDK
typedef uint32_t BTA_DeviceType;
#define BTA_DeviceTypeAny           ((uint32_t)0x0000)

#define BTA_DeviceTypeEthernet      ((uint32_t)0x0001)
#define BTA_DeviceTypeUsb           ((uint32_t)0x0002)
#define BTA_DeviceTypeUart          ((uint32_t)0x0003)
#define BTA_DeviceTypeBltstream     ((uint32_t)0x000f)



/// This structure holds information about the device
typedef struct BTA_DeviceInfo {
    BTA_DeviceType deviceType;          ///< Two-byte-id for a device or module (independent of hardware and software versions)
    uint8_t *productOrderNumber;        ///< String containing the PON (not including the serial number) (unique in combination with serial number)
    uint32_t serialNumber;              ///< Serial number (unique in combination with PON)
    uint32_t firmwareVersionMajor;      ///< Firmware version major
    uint32_t firmwareVersionMinor;      ///< Firmware version minor
    uint32_t firmwareVersionNonFunc;    ///< Firmware version non functional
    uint32_t mode0;                     ///< The content of the device's primary mode register
    uint32_t status;                    ///< The content of the device's status register
    uint32_t uptime;                    ///< The content of the device's uptime register

    uint8_t *deviceMacAddr;             ///< The MAC address of the device
    uint32_t deviceMacAddrLen;          ///< The length in bytes of deviceMacAddr
    uint8_t *deviceIpAddr;              ///< The IP address of the device
    uint32_t deviceIpAddrLen;           ///< The length in bytes of deviceIpAddr
    uint8_t *subnetMask;                ///< The subnet IP address of the device
    uint32_t subnetMaskLen;             ///< The length in bytes of subnetMask
    uint8_t *gatewayIpAddr;             ///< The gateway IP address of the device
    uint32_t gatewayIpAddrLen;          ///< The length in bytes of gatewayIpAddr
    uint16_t tcpControlPort;            ///< The TCP port of the control interface
    uint16_t tcpDataPort;               ///< The TCP port of the data interface
    uint8_t *udpDataIpAddr;             ///< The IP destination address of the UDP data stream
    uint32_t udpDataIpAddrLen;          ///< The length in bytes of udpDataIpAddr
    uint16_t udpDataPort;               ///< The UDP port of the data interface
    uint16_t udpControlPort;            ///< The UDP port of the control interface

    uint8_t* bltstreamFilename;         ///< The location of the bltstream
} BTA_DeviceInfo;



///     @brief  Callback function to report on a discovered device
///             Do not call BTAfreeDeviceInfo on deviceInfo, because it is free'd in the lib
///     @param  handle The handle created by BTAstartDiscovery for reference
///     @param  deviceInfo A struct containing information about the discovered device
typedef void (BTA_CALLCONV *FN_BTA_DeviceFound)(BTA_Handle handle, BTA_DeviceInfo *deviceInfo);



///     @brief  Callback function to report on a discovered device
///             Do not call BTAfreeDeviceInfo on deviceInfo, because it is free'd in the lib
///     @param  handle The handle created by BTAstartDiscovery for reference
///     @param  deviceInfo A struct containing information about the discovered device
///     @param  userArg A pointer set by the user in BTAstartDiscoveryEx via BTA_DiscoveryConfig->userArg
typedef void (BTA_CALLCONV *FN_BTA_DeviceFoundEx)(BTA_Handle handle, BTA_DeviceInfo *deviceInfo, void *userArg);



///  @brief This structure is used to configure the process of device discovery
typedef struct BTA_DiscoveryConfig {
    BTA_DeviceType deviceType;          ///< Choose between BTA_DeviceTypeEthernet, BTA_DeviceTypeUsb, BTA_DeviceTypeUart, BTA_DeviceTypeAny or the device specific one

    uint8_t *broadcastIpAddr;           ///< The UDP broadcast IP address, null: 255.255.255.255
    uint8_t broadcastIpAddrLen;         ///< The length in bytes of broadcastIpAddr
    uint16_t broadcastPort;             ///< The UDP port to send the broadcast to, 0: 11003
    uint8_t *callbackIpAddr;            ///< The UDP callback IP address, null: chosen automatically by os
    uint8_t callbackIpAddrLen;          ///< The length in bytes of callbackIpAddr
    uint16_t callbackPort;              ///< The UDP port to listen for responses, 0: chosen automatically by os

    uint8_t *uartPortName;              ///< The port name of the UART to use (ASCII coded)
    int32_t uartBaudRate;               ///< The UART baud rate
    uint8_t uartDataBits;               ///< The number of UART data bits used
    uint8_t uartStopBits;               ///< 0: None, 1: One, 2: Two, 3: 1.5 stop bits
    uint8_t uartParity;                 ///< 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space Parity
    uint8_t uartTransmitterAddress;     ///< The source address for UART communications

    FN_BTA_DeviceFound deviceFound;     ///< Optional. The callback to be invoked when a device has been discovered
    FN_BTA_DeviceFoundEx deviceFoundEx; ///< Optional. The callback to be invoked when a device has been discovered
    FN_BTA_InfoEventEx2 infoEventEx2;   ///< Optional. The callback to be invoked when an error occurs
    void *userArg;                      ///< Optional. Set this pointer and it will be set as the third parameter in deviceFoundEx and infoEventEx2 callbacks

    uint32_t millisInterval;            ///< 0: Send discovery message / query USB devices once (Eth), >0: Send message and query USB at given interval
    uint8_t periodicReports;            ///< 0: Devices that are discovered again at 'millisInterval' are not reported via callback again, 1: duplicates are always reported
} BTA_DiscoveryConfig;







// legacy (for backward compatibility DO NOT USE)
#define BTA_DeviceTypeGenericEth                    (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeGenericUsb                    (BTA_DeviceTypeUsb)
#define BTA_DeviceTypeGenericUart                   (BTA_DeviceTypeUart)
#define BTA_DeviceTypeGenericBltstream              (BTA_DeviceTypeBltstream)
#define BTA_DeviceTypeArgos3dP310                   (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeArgos3dP32x                   (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeArgos3dP33x                   (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentis3dM100                  (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentis3dP509                  (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentis3dM520                  (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentis3dM530                  (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeTimUp19kS3Eth                 (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeMultiTofPlatformMlx           (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeArgos3dP100                   (BTA_DeviceTypeUsb)
#define BTA_DeviceTypeTimUp19kS3Spartan6            (BTA_DeviceTypeUsb)
#define BTA_DeviceTypeEPC610TofModule               (BTA_DeviceTypeUart)
#define BTA_DeviceTypeSentis3dP510                  (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentisTofP510                 (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentisTofM100                 (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentisTofP509                 (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeTimUp19kS3PEth                (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeArgos3dP320                   (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeArgos3dP321                   (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeMlx75123ValidationPlatform    (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeEvk7512x                      (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeEvk75027                      (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeEvk7512xTofCcBa               (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeP320S                         (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeGrabberBoard                  (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeLimTesterV2                   (BTA_DeviceTypeUart)
#define BTA_DeviceTypeTimUpIrs1125                  (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeMlx75023TofEval               (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeSentis3dP509Irs1020           (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeArgos3dP510SKT                (BTA_DeviceTypeEthernet)
#define BTA_DeviceTypeTimUp19kS3EthP                (BTA_DeviceTypeEthernet)

#endif
