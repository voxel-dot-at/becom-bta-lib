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
#define BTA_DISCOVERY_H_VER_MIN 0
#define BTA_DISCOVERY_H_VER_NON_FUNC 5


#include <stdint.h>
#include "bta.h"

///  @brief All interfaces currently known to the SDK
typedef uint32_t BTA_DeviceType;
#define BTA_DeviceTypeAny           ((uint32_t)0x0000)

#define BTA_DeviceTypeEthernet      ((uint32_t)0x0001)
#define BTA_DeviceTypeUsb           ((uint32_t)0x0002)
#define BTA_DeviceTypeUart          ((uint32_t)0x0003)
#define BTA_DeviceTypeBltstream     ((uint32_t)0x000f)



///  @brief This structure is used to configure the process of device discovery
typedef struct BTA_DiscoveryConfig {
    BTA_DeviceType deviceType;          ///< A special device type to look for, 0: Any

    uint8_t *broadcastIpAddr;           ///< The UDP broadcast IP address, null: 255.255.255.255
    uint8_t broadcastIpAddrLen;         ///< The length in bytes of broadcastIpAddr
    uint16_t broadcastPort;             ///< The UDP port to send the broadcast to, 0: 11003
    uint8_t *callbackIpAddr;            ///< The UDP callback IP address, null: chosen automatically
    uint8_t callbackIpAddrLen;          ///< The length in bytes of callbackIpAddr
    uint16_t callbackPort;              ///< The UDP port to listen for responses, 0: chosen automatically

    uint8_t *uartPortName;              ///< The port name of the UART to use (ASCII coded)
    int32_t uartBaudRate;               ///< The UART baud rate
    uint8_t uartDataBits;               ///< The number of UART data bits used
    uint8_t uartStopBits;               ///< 0: None, 1: One, 2: Two, 3: 1.5 stop bits
    uint8_t uartParity;                 ///< 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space Parity
    uint8_t uartTransmitterAddress;     ///< The source address for UART communications
} BTA_DiscoveryConfig;



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
} BTA_DeviceInfo;



///     @brief  Callback function to report on a discovered device
///             Do not call BTAfreeDeviceInfo on deviceInfo, because it is free'd in the lib
///     @param  handle The handle created by BTAstartDiscovery for reference
///     @param  deviceInfo A struct containing information about the discovered device
typedef void (BTA_CALLCONV *FN_BTA_DeviceFound)(BTA_Handle handle, BTA_DeviceInfo *deviceInfo);







// legacy (for backward compatibility ONLY USED INTERNALLY)
#define BTA_DeviceTypeGenericEth                    ((uint32_t)0x0001)
#define BTA_DeviceTypeGenericUsb                    ((uint32_t)0x0002)
#define BTA_DeviceTypeGenericUart                   ((uint32_t)0x0003)
#define BTA_DeviceTypeGenericBltstream              ((uint32_t)0x000f)
#define BTA_DeviceTypeArgos3dP310                   ((uint32_t)0x9ba6)
#define BTA_DeviceTypeArgos3dP32x                   ((uint32_t)0xb320)
#define BTA_DeviceTypeArgos3dP33x                   ((uint32_t)0x03fc)
#define BTA_DeviceTypeSentis3dM100                  ((uint32_t)0xa9c1)
#define BTA_DeviceTypeSentis3dP509                  ((uint32_t)0x4859)
#define BTA_DeviceTypeSentis3dM520                  ((uint32_t)0x5032)
#define BTA_DeviceTypeSentis3dM530                  ((uint32_t)0x5110)
#define BTA_DeviceTypeTimUp19kS3Eth                 ((uint32_t)0x795c)
#define BTA_DeviceTypeMultiTofPlatformMlx           ((uint32_t)0xbe41)
#define BTA_DeviceTypeTimUp19kS3Spartan6            ((uint32_t)0x13ab)
#define BTA_DeviceTypeEPC610TofModule               ((uint32_t)0x7a3d)
#define BTA_DeviceTypeSentis3dP510                  ((uint32_t)0x5032)
#define BTA_DeviceTypeSentisTofP510                 ((uint32_t)0x5032)
#define BTA_DeviceTypeSentisTofM100                 ((uint32_t)0xa9c1)
#define BTA_DeviceTypeSentisTofP509                 ((uint32_t)0x4859)
#define BTA_DeviceTypeTimUp19kS3PEth                ((uint32_t)0xb620)
#define BTA_DeviceTypeArgos3dP320                   ((uint32_t)0xb320)
#define BTA_DeviceTypeArgos3dP321                   ((uint32_t)0xb321)
#define BTA_DeviceTypeMlx75123ValidationPlatform    ((uint32_t)0x1e3c)
#define BTA_DeviceTypeEvk7512x                      ((uint32_t)0x31ee)
#define BTA_DeviceTypeEvk75027                      ((uint32_t)0x31ff)
#define BTA_DeviceTypeEvk7512xTofCcBa               ((uint32_t)0x32ee)
#define BTA_DeviceTypeP320S                         ((uint32_t)0x3de4)
#define BTA_DeviceTypeGrabberBoard                  ((uint32_t)0x4762)
#define BTA_DeviceTypeLimTesterV2                   ((uint32_t)0x4c54)
#define BTA_DeviceTypeTimUpIrs1125                  ((uint32_t)0x5a79)
#define BTA_DeviceTypeMlx75023TofEval               ((uint32_t)0x7502)
#define BTA_DeviceTypeSentis3dP509Irs1020           ((uint32_t)0xb509)
#define BTA_DeviceTypeArgos3dP510SKT                ((uint32_t)0xb510)
#define BTA_DeviceTypeTimUp19kS3EthP                ((uint32_t)0xb620)
#define BTA_DeviceTypeMhsCamera                     ((uint32_t)0x22d3)



#endif
