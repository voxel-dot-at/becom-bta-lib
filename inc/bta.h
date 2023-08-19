///  @file bta.h
///
///  @brief The main header for BltTofApi. Includes all interface functions, the config struct
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

#ifndef BTA_H_INCLUDED
#define BTA_H_INCLUDED

#include <bta_config.h>

#define BTA_VER_MAJ 3
#define BTA_VER_MIN 3
#define BTA_VER_NON_FUNC 11


#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif


#ifdef PLAT_WINDOWS
#   define BTA_CALLCONV __stdcall
#   ifdef COMPILING_DLL
#       define DLLEXPORT __declspec(dllexport)
#   else
#       define DLLEXPORT __declspec(dllimport)
#   endif
#else
    //must be empty
#   define DLLEXPORT
    //must be empty
#   define BTA_CALLCONV
#endif


///     @brief  The handle to hold the instance created by BTAopen
typedef void* BTA_Handle;


#include <stdint.h>

#include "bta_status.h"
#include "bta_frame.h"
#include "bta_flash_update.h"
#include "bta_ext.h"


///     @brief  Callback function to report on informative events.
///             The implementation of this function in the application must copy the relevant data and return immediately.
///             The parameter 'verbosity' in BTA_Config can be used to turn on/off certain events.
///     @param  status The status describing the reason for the infoEvent
///     @param  msg A string containing the information for the user
typedef void (BTA_CALLCONV *FN_BTA_InfoEvent)(BTA_Status status, int8_t *msg);



///     @brief  Callback function to report on informative events.
///             The implementation of this function in the application must copy the relevant data and return immediately.
///             The parameter 'verbosity' in BTA_Config can be used to turn on/off certain events.
///     @param  handle The handle as identification for the device
///     @param  status The status describing the reason for the infoEvent
///     @param  msg A string containing the information for the user
typedef void (BTA_CALLCONV *FN_BTA_InfoEventEx)(BTA_Handle handle, BTA_Status status, int8_t *msg);



///     @brief  Callback function to report on informative events.
///             The implementation of this function in the application must copy the relevant data and return immediately.
///             The parameter 'verbosity' in BTA_Config can be used to turn on/off certain events.
///     @param  handle The handle as identification for the device
///     @param  status The status describing the reason for the infoEvent
///     @param  msg A string containing the information for the user
///     @param  userArg A pointer set by the user via BTA_Config->userArg
typedef void (BTA_CALLCONV *FN_BTA_InfoEventEx2)(BTA_Handle handle, BTA_Status status, int8_t *msg, void *userArg);


#include "bta_discovery.h"



///     @brief  Callback function to report on data frames from the sensor.
///             The implementation of this function in the application must copy the relevant data and return immediately.
///             The BTA_Frame may NOT be altered!
///             Do not call BTAfreeFrame on frame, because it is free'd in the lib.
///     @param  frame A pointer to the structure containing the data frame
typedef void (BTA_CALLCONV *FN_BTA_FrameArrived)(BTA_Frame *frame);



///     @brief  Callback function to report on data frames from the sensor.
///             The implementation of this function in the application must copy the relevant data and return immediately.
///             The BTA_Frame may NOT be altered!
///             Do not call BTAfreeFrame on frame, because it is free'd in the lib.
///     @param  handle The handle as identification for the device
///     @param  frame A pointer to the structure containing the data frame
typedef void (BTA_CALLCONV *FN_BTA_FrameArrivedEx)(BTA_Handle handle, BTA_Frame *frame);



///     @brief  Structure for the user for use in frameArrivedEx2
///             The user can inform the BltTofApi about how he handled the callback
typedef struct BTA_FrameArrivedReturnOptions {
    uint8_t userFreesFrame;             ///< If the user sets this to true, the BltTofApi won't free the frame
} BTA_FrameArrivedReturnOptions;



///     @brief  Callback function to report on data frames from the sensor.
///             The implementation of this function in the application must copy the relevant data and return immediately.
///             The BTA_Frame may NOT be altered!
///             Do not call BTAfreeFrame on frame, because it is free'd in the lib.
///     @param  handle The handle as identification for the device
///     @param  frame A pointer to the structure containing the data frame
///     @param  userArg A pointer set by the user in BTAopen via BTA_Config->userArg
///     @param  frameArrivedReturnOptions An empty pointer to a struct where the user can give information to the BltTofApi
typedef void (BTA_CALLCONV *FN_BTA_FrameArrivedEx2)(BTA_Handle handle, BTA_Frame *frame, void *userArg, struct BTA_FrameArrivedReturnOptions *frameArrivedReturnOptions);



#include "bta_discovery.h"




///     @brief  The BTA_Config shall be 8-byte aligned
#define BTA_CONFIG_STRUCT_STRIDE 8
#ifdef PLAT_WINDOWS
#define BTA_PRAGMA_ALIGN __declspec(align(BTA_CONFIG_STRUCT_STRIDE))
#else
#define BTA_PRAGMA_ALIGN __attribute__((aligned(BTA_CONFIG_STRUCT_STRIDE)))
#endif

///     @brief  Configuration structure to be passed with BTAopen
typedef struct BTA_Config {
    BTA_PRAGMA_ALIGN uint8_t *udpDataIpAddr;                ///< The IP address for the UDP data interface (The address the device is configured to stream to)
    BTA_PRAGMA_ALIGN uint8_t udpDataIpAddrLen;              ///< The length of udpDataIpAddr buffer in [byte]
    BTA_PRAGMA_ALIGN uint16_t udpDataPort;                  ///< The port for the UDP data interface (The port the device is configured to stream to)
    BTA_PRAGMA_ALIGN uint8_t *udpControlOutIpAddr;          ///< The IP address for the UDP control interface (outbound connection) (The IP address of the camera device)
    BTA_PRAGMA_ALIGN uint8_t udpControlOutIpAddrLen;        ///< The length of udpControlOutIpAddr buffer in [byte]
    BTA_PRAGMA_ALIGN uint16_t udpControlPort;               ///< The port for the UDP control interface (outbound connection) (The port where the device awaits commands at)
    BTA_PRAGMA_ALIGN uint8_t *udpControlInIpAddr;           ///< The callback IP address for the UDP control interface (inbound connection) (The address the device should answer to, usually the local IP address) [This parameter is optional since TIM-UP-19K-S3 - ETH Firmware v1.6]
    BTA_PRAGMA_ALIGN uint8_t udpControlInIpAddrLen;         ///< The length of udpControlInIpAddr buffer in [byte]
    BTA_PRAGMA_ALIGN uint16_t udpControlCallbackPort;       ///< The callback port for the UDP control interface (inbound connection) (The port the device should answer to) [This parameter is optional since TIM-UP-19K-S3 - ETH Firmware v1.6]
    BTA_PRAGMA_ALIGN uint8_t *tcpDeviceIpAddr;              ///< The IP address for the TCP data and control interface (The device's IP address)
    BTA_PRAGMA_ALIGN uint8_t tcpDeviceIpAddrLen;            ///< The length of tcpDeviceIpAddr buffer in [byte]
    BTA_PRAGMA_ALIGN uint16_t tcpDataPort;                  ///< The port for the TCP data interface (The port the device sends data to) (not supported yet)
    BTA_PRAGMA_ALIGN uint16_t tcpControlPort;               ///< The port for the TCP control interface (The port the device awaits commands at)

    BTA_PRAGMA_ALIGN uint8_t *uartPortName;                 ///< The port name of the UART to use (ASCII coded)
    BTA_PRAGMA_ALIGN uint32_t uartBaudRate;                 ///< The UART baud rate
    BTA_PRAGMA_ALIGN uint8_t uartDataBits;                  ///< The number of UART data bits used
    BTA_PRAGMA_ALIGN uint8_t uartStopBits;                  ///< 0: None, 1: One, 2: Two, 3: 1.5 stop bits
    BTA_PRAGMA_ALIGN uint8_t uartParity;                    ///< 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space Parity
    BTA_PRAGMA_ALIGN uint8_t uartTransmitterAddress;        ///< The source address for UART communications
    BTA_PRAGMA_ALIGN uint8_t uartReceiverAddress;           ///< The target address for UART communications

    BTA_PRAGMA_ALIGN BTA_DeviceType deviceType;             ///< The device type, when not left 0 implies the type of connection to use (BTA_DeviceTypeAny, BTA_DeviceTypeEthernet, BTA_DeviceTypeUsb, BTA_DeviceTypeBltstream)
    BTA_PRAGMA_ALIGN uint8_t *pon;                          ///< Product Order Number of device to be opened (0 == not specified) (ASCII coded)
    BTA_PRAGMA_ALIGN uint32_t serialNumber;                 ///< Serial number of device to be opened (0 == not specified)

    BTA_PRAGMA_ALIGN uint8_t *calibFileName;                ///< No longer supported, please use BTAwigglingUpdate()
    BTA_PRAGMA_ALIGN uint8_t *zFactorsFileName;             ///< No longer supported
    BTA_PRAGMA_ALIGN uint8_t *wigglingFileName;             ///< No longer supported, please use BTAwigglingUpdate()

    BTA_PRAGMA_ALIGN BTA_FrameMode frameMode;               ///< Frame mode to be set in SDK/device

    BTA_PRAGMA_ALIGN FN_BTA_InfoEvent infoEvent;            ///< Callback function pointer to the function to be called upon an informative event (optional but handy for debugging/tracking) (deprecated, use infoEventEx/2)
    BTA_PRAGMA_ALIGN FN_BTA_InfoEventEx infoEventEx;        ///< Callback function pointer to the function to be called upon an informative event (optional but handy for debugging/tracking)
    BTA_PRAGMA_ALIGN FN_BTA_InfoEventEx2 infoEventEx2;      ///< Callback function pointer to the function to be called upon an informative event (optional but handy for debugging/tracking)
    BTA_PRAGMA_ALIGN uint8_t verbosity;                     ///< A value to tell the library when and when not to generate InfoEvents (0: Only critical events, 10: Maximum amount of events)
    BTA_PRAGMA_ALIGN FN_BTA_FrameArrived frameArrived;      ///< Callback function pointer to the function to be called when a frame is ready (optional) (deprecated, use frameArrivedEx/2)
    BTA_PRAGMA_ALIGN FN_BTA_FrameArrivedEx frameArrivedEx;  ///< Callback function pointer to the function to be called when a frame is ready (optional)
    BTA_PRAGMA_ALIGN FN_BTA_FrameArrivedEx2 frameArrivedEx2;///< Callback function pointer to the function to be called when a frame is ready (optional)
    BTA_PRAGMA_ALIGN void *userArg;                         ///< Set this pointer and it will be set as the third parameter in frameArrivedEx2 and infoEventEx2 callbacks

    BTA_PRAGMA_ALIGN uint16_t frameQueueLength;             ///< The library queues this amount of frames internally
    BTA_PRAGMA_ALIGN BTA_QueueMode frameQueueMode;          ///< The frame queue configuration parameter

    BTA_PRAGMA_ALIGN uint16_t averageWindowLength;          ///< No longer supported

    BTA_PRAGMA_ALIGN uint8_t *bltstreamFilename;            ///< Only for BTA_DeviceTypeBltstream: Specify the file (containing the stream) to read from (ASCII coded)
    BTA_PRAGMA_ALIGN uint8_t *infoEventFilename;            ///< All infoEvents are appended to this file. Be careful with this feature as it opens and closes and the file with each infoEvent

    BTA_PRAGMA_ALIGN uint8_t udpDataAutoConfig;             ///< 1: BTAopen automatically configures the device to stream to the correct destination
    BTA_PRAGMA_ALIGN uint8_t shmDataEnabled;                ///< 1: frames are retrieved via the shared memory interface
} BTA_Config;
#define CONFIG_STRUCT_ORG_LEN 42



//----------------------------------------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif


///     @brief  For querying API version
///     @param  verMaj If not null it points to the major firmware version of the device on return
///     @param  verMin If not null it points to the minor firmware version of the device on return
///     @param  verNonFun If not null it points to the non functional firmware version of the device on return
///     @param  buildDateTime A char array allocated by the caller containing the date/time string (ASCII) of build on return (can be left null)
///     @param  buildDateTimeLen Size of the preallocated buffer behind buildDateTime in [byte]
///     @param  supportedDeviceTypes Array allocated by the caller containing the codes of all devices supported by a specifiy BTA implementation on return (can be left null)
///     @param  supportedDeviceTypesLen Pointer to size of supportedDeviceTypes (the number of supported devices); on return it contains number of supported device types
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetVersion(uint32_t *verMaj, uint32_t *verMin, uint32_t *verNonFun, uint8_t *buildDateTime, uint32_t buildDateTimeLen, uint16_t *supportedDeviceTypes, uint32_t *supportedDeviceTypesLen);



///     @brief  Fills the discovery config structure with standard values
///     @param  config Pointer to the structure to be initialized to standard values
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAinitDiscoveryConfig(BTA_DiscoveryConfig *config);



///     @brief  Starts the discovery of devices.
///             If possible, broadcast messages are transmitted otherwise all possible connections are tested
///     @param  discoveryConfig Parameters on how to perform the discovery.
///                             Fill in the struct members that are relevant to discover your device
///                             Do not free (or leave the scope of) the struct or its members before calling BTAstopDiscovery!
///     @param  deviceFound     The callback to be invoked when a device has been discovered
///     @param  infoEvent       The callback to be invoked when an error occurs
///     @param  handle          On return it contains the discovery handle which has to be used to stop the background process.
///     @return                 Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle);



///     @brief  Starts the discovery of devices.
///             If possible, broadcast messages are transmitted otherwise all possible connections are tested
///     @param  discoveryConfig Parameters on how to perform the discovery.
///                             The connection interface used defines which parameters have to be set in BTA_DiscoveryConfig.
///     @param  deviceFound     The callback to be invoked when a device has been discovered
///     @param  infoEvent       The callback to be invoked when an error occurs
///     @param  handle          On return it contains the discovery handle which has to be used to stop the background process.
///     @return                 Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAstartDiscoveryEx(BTA_DiscoveryConfig *discoveryConfig, BTA_Handle *handle);



///     @brief  Stops the discovery of devices
///     @param  handle  Pass the handle from startDiscovery in order to identify the ongoing discovery process
///     @return         Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAstopDiscovery(BTA_Handle *handle);



///     @brief  Stops the discovery of devices
///     @param  handle  Pass the handle from startDiscovery in order to identify the ongoing discovery process
///     @param  deviceList Optional. Contains discovered devices after BTAstopDiscovery. If null is passed it is ignored
///     @param  deviceListLen Optional with deviceList. Specified the number of pointers deviceList is able to hold. If more devices are discovered,
///                           they are not included in the list. Contains the number of actually discovered devices on return.
///     @return         Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAstopDiscoveryEx(BTA_Handle *handle, BTA_DeviceInfo **deviceList, uint16_t *deviceListLen);



///     @brief  Fills the config structure with standard values
///     @param  config Pointer to the structure to be initialized to standard values
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAinitConfig(BTA_Config *config);



///     @brief  Establishes a connection to the device and returns a handle
///     @param  config Pointer to the previously initialized config structure
///     @param  handle Pointer containing the handle to the device on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAopen(BTA_Config *config, BTA_Handle *handle);



///     @brief Disconnects from the sensor and closes the handle
///     @param handle Pointer to the handle of the device to be closed; points to null on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAclose(BTA_Handle *handle);



///     @brief  For querying information about the device.
///             If successful, BTAfreeDeviceInfo must be called afterwards.
///     @param  handle Handle of the device to be used
///     @param  deviceInfo Pointer to pointer to structure with information about the device on return
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetDeviceInfo(BTA_Handle handle, BTA_DeviceInfo **deviceInfo);



///     @brief For freeing the device information structure
///     @param deviceInfo Pointer to structure to be freed
DLLEXPORT BTA_Status BTA_CALLCONV BTAfreeDeviceInfo(BTA_DeviceInfo *deviceInfo);



///     @brief  For querying the device type.
///     @param  handle Handle of the device to be used
///     @param  deviceType Preallocated pointer to hold the deviceType on return
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetDeviceType(BTA_Handle handle, BTA_DeviceType *deviceType);



///     @brief  Queries whether the library has a valid connection to the sensor
///     @param  handle Handle of the device to be used
///     @return 1 if connected to the sensor, 0 otherwise
DLLEXPORT uint8_t BTA_CALLCONV BTAisConnected(BTA_Handle handle);



///     @brief  Allows to set a BTA_FrameMode which defines the data delivered by the sensor.
///             The device and/or the SDK is configured depending on the frame mode, so that the desired channels are included in each BTA_Frame.
///     @param  handle Handle of the device to be used
///     @param  frameMode The desired frame-mode
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetFrameMode(BTA_Handle handle, BTA_FrameMode frameMode);



///     @brief  Allows to get a BTA_FrameMode which defines the data delivered by the sensor.
///     @param  handle Handle of the device to be used
///     @param  frameMode The current frame-mode or BTA_FrameModeCurrentConfig if the camera configuration is not conclusive
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetFrameMode(BTA_Handle handle, BTA_FrameMode *frameMode);


///     @brief  Allows to set a number of selected channels for streaming
///     @param  handle Handle of the device to be used
///     @param  channelSelection Pointer to list of BTA_ChannelSelections.
///     @param  channelSelectionCount count of BTA_ChannelSelections in channelSelection
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetChannelSelection(BTA_Handle handle, BTA_ChannelSelection *channelSelection, int channelSelectionCount);


///     @brief  Allows to get a list of the selected channels for streaming
///     @param  handle Handle of the device to be used
///     @param  channelSelection Pointer to list of BTA_ChannelSelections allocated by the caller. Contains selected channels on return.
///     @param  channelSelectionCount Pointer to size of channelSelection (the number of supported BTA_ChannelSelections); on return it contains number of selected channels
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetChannelSelection(BTA_Handle handle, BTA_ChannelSelection *channelSelection, int *channelSelectionCount);



///     @brief  Helper function to clone/duplicate/deep-copy a BTA_Frame structure.
///             If successful, BTAfreeFrame must be called on frameDst afterwards.
///     @param  frameSrc The pointer to the frame to be copied
///     @param  frameDst The pointer to the new duplicated frame
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAcloneFrame(BTA_Frame *frameSrc, BTA_Frame **frameDst);



///     @brief  Actively requests a frame.
///             For this function to work, frameQueueLength and frameQueueMode must be set to queue frames!
///             For most applications it is adviced to use the frameArrived/Ex/Ex2 callback instead
///             BTAfreeFrame must not be called on the frame because it remains in the queue
///     @param  handle Handle of the device to be used
///     @param  frame Pointer to frame (null if failed) on return (needs to be freed with BTAfreeFrame)
///     @param  millisecondsTimeout Timeout to wait if no frame is yet available in [ms]. If timeout == 0 the function waits endlessly for a frame from the device.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTApeekFrame(BTA_Handle handle, BTA_Frame **frame, uint32_t millisecondsTimeout);



///     @brief  Actively requests a frame.
///             For this function to work, frameQueueLength and frameQueueMode must be set to queue frames!
///             For most applications it is adviced to use the frameArrived/Ex/Ex2 callback instead
///             If successful, BTAfreeFrame must be called afterwards.
///     @param  handle Handle of the device to be used
///     @param  frame Pointer to frame (null if failed) on return (needs to be freed with BTAfreeFrame)
///     @param  millisecondsTimeout Timeout to wait if no frame is yet available in [ms]. If timeout == 0 the function waits endlessly for a frame from the device.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetFrame(BTA_Handle handle, BTA_Frame** frame, uint32_t millisecondsTimeout);



///     @brief  Helper function to free a BTA_Frame structure
///     @param  frame The pointer to the frame to be freed; points to null on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAfreeFrame(BTA_Frame **frame);



///     @brief  Get number of currently queued frames
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetFrameCount(BTA_Handle handle, uint32_t *frameCount);



///     @brief  Flush the internal frame queue.
///             All frames captured so far are discarded
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAflushFrameQueue(BTA_Handle handle);



///     @brief  Convenience function for extracting channel data from a provided frame.
///             It simply returns the pointer and copies some information. The same data can be accessed directly going through the BTA_Frame structure.
///             If there is no matching channel is present in the frame, an error is returned.
///     @param  frame The frame from which to extract the data
///     @param  channelId Identification for the channel to be extracted. If more than one channel with this id is present, only the first is regarded
///     @param  data Pointer to the distances on return (null on error)
///     @param  dataFormat Pointer to the BTA_DataFormat, thus how to parse 'distBuffer'
///     @param  unit Pointer to BTA_Unit, thus how to interpret 'distBuffer'
///     @param  xRes Pointer to the number of columns of 'distBuffer'
///     @param  yRes Pointer to the number of rows of 'distBuffer'
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetDataByChannelId(BTA_Frame *frame, BTA_ChannelId channelId, void **data, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes);



///     @brief  Convenience function for extracting distances from a provided frame.
///             It simply returns the pointer and copies some information. The same data can be accessed directly going through the BTA_Frame structure.
///             If there is no channel with distance data present in the frame, an error is returned.
///     @param  frame The frame from which to extract the data
///     @param  distBuffer Pointer to the distances on return (null on error)
///     @param  dataFormat Pointer to the BTA_DataFormat, thus how to parse 'distBuffer'
///     @param  unit Pointer to BTA_Unit, thus how to interpret 'distBuffer'
///     @param  xRes Pointer to the number of columns of 'distBuffer'
///     @param  yRes Pointer to the number of rows of 'distBuffer'
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetDistances(BTA_Frame *frame, void **distBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes);



///     @brief  Convenience function for extracting amplitudes from a provided frame.
///             It simply returns the pointer and copies some information. The same data can be accessed directly going through the BTA_Frame structure.
///             If there is no channel with amplitude data present in the frame, an error is returned.
///     @param  frame The frame from which to extract the data
///     @param  ampBuffer Pointer to the amplitudes on return (null on error)
///     @param  dataFormat Pointer to the BTA_DataFormat, thus how to parse 'ampBuffer'
///     @param  unit Pointer to BTA_Unit, thus how to interpret 'ampBuffer'
///     @param  xRes Pointer to the number of columns of 'ampBuffer'
///     @param  yRes Pointer to the number of rows of 'ampBuffer'
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetAmplitudes(BTA_Frame *frame, void **ampBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes);



///     @brief  Convenience function for extracting flags from a provided frame.
///             It simply returns the pointer and copies some information. The same data can be accessed directly going through the BTA_Frame structure.
///             If there is no channel with flag data present in the frame, an error is returned.
///     @param  frame The frame from which to extract the data
///     @param  flagBuffer Pointer to the flags on return (null on error)
///     @param  dataFormat Pointer to the BTA_DataFormat, thus how to parse 'flagBuffer'
///     @param  unit Pointer to BTA_Unit, thus how to interpret 'flagBuffer'
///     @param  xRes Pointer to the number of columns of 'flagBuffer'
///     @param  yRes Pointer to the number of rows of 'flagBuffer'
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetFlags(BTA_Frame *frame, void **flagBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes);



///     @brief  Convenience function for extracting the 3D-coordinates from a provided frame.
///             It simply returns the pointer and copies some information. The same data can be accessed directly going through the BTA_Frame structure.
///             If there are not 3 channels with coordinate data present in the sensor data, an error is returned.
///     @param  frame The frame from which to extract the data
///     @param  xBuffer A pointer to the cartesian x coordinates on return (null on error)
///     @param  yBuffer A pointer to the cartesian y coordinates on return (null on error)
///     @param  zBuffer A pointer to the cartesian z coordinates on return (null on error)
///     @param  dataFormat Pointer to the BTA_DataFormat, thus how to parse 'xBuffer', 'yBuffer' and 'zBuffer'
///     @param  unit Pointer to BTA_Unit, thus how to interpret 'xBuffer', 'yBuffer' and 'zBuffer'
///     @param  xRes Pointer to the number of columns of 'xBuffer', 'yBuffer' and 'zBuffer'
///     @param  yRes Pointer to the number of rows of 'xBuffer', 'yBuffer' and 'zBuffer'
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetXYZcoordinates(BTA_Frame *frame, void **xBuffer, void **yBuffer, void **zBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes);



///     @brief  Convenience function for extracting colors from a provided frame.
///             It simply returns the pointer and copies some information. The same data can be accessed directly going through the BTA_Frame structure.
///             If there is no channel with color data present in the frame, an error is returned.
///     @param  frame The frame from which to extract the data
///     @param  colorBuffer Pointer to the colors on return (null on error)
///     @param  dataFormat Pointer to the BTA_DataFormat, thus how to parse 'colorBuffer'
///     @param  unit Pointer to BTA_Unit, thus how to interpret 'colorBuffer'
///     @param  xRes Pointer to the number of columns of 'colorBuffer'
///     @param  yRes Pointer to the number of rows of 'colorBuffer'
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetColors(BTA_Frame *frame, void **colorBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes);



///     @brief  Convenience function for extracting a specific channel from a provided frame.
///             The channel pointers are extracted, but data is not copied; free the frame as usual and the channels returned 
///     @param  frame The frame from which to extract the channel
///     @param  filter Pointer to the struct containing the filter information to be used to select the result channel(s)
///     @param  channels Pointer to a preallocated array of pointers to BTA_Channels. Contains channelsLen valid channel pointers on return
///     @param  channelsLen Pointer to the length of the preallocated array channels. Number of found channels on return.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetChannels(BTA_Frame *frame, BTA_ChannelFilter *filter, BTA_Channel **channels, int *channlesLen);



///     @brief  Facilitates setting the integration time for the default capture sequence(s)
///     @param  handle Handle of the device to be used
///     @param  integrationTime The desired integration time in [us]
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetIntegrationTime(BTA_Handle handle, uint32_t integrationTime);



///     @brief  Facilitates the retrieval of the current integration time of the default capture sequence(s)
///     @param  handle Handle of the device to be used
///     @param  integrationTime Pointer containing the integration time in [us] on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetIntegrationTime(BTA_Handle handle, uint32_t *integrationTime);



///     @brief Facilitates setting the frame rate for the default capture sequence
///     @param handle Handle of the device to be used
///     @param frameRate The desired frame rate in [Hz]
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetFrameRate(BTA_Handle handle, float frameRate);



///     @brief  Facilitates the retrieval of the current theoretical frame rate of the default capture sequence(s)
///     @param  handle Handle of the device to be used
///     @param  frameRate Pointer containing the frame rate in [Hz] on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetFrameRate(BTA_Handle handle, float *frameRate);



///     @brief  Facilitates setting the modulation frequency for the default capture sequence(s)
///     @param  handle Handle of the device to be used
///     @param  modulationFrequency The desired modulation frequency in [Hz]
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetModulationFrequency(BTA_Handle handle, uint32_t modulationFrequency);



///     @brief  Facilitates the retrieval of the current theoretical frame rate of the default capture sequence(s)
///     @param  handle Handle of the device to be used
///     @param  modulationFrequency Pointer containing the modulation frequency in [Hz] on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetModulationFrequency(BTA_Handle handle, uint32_t *modulationFrequency);



///     @brief  Function for setting the distance offset being applied to all pixels equally.
///             It is, for all current devices, valid for the currently set modulation frequency.
///             It can only be set for predefined modulation frequencies (see device’s SUM).
///     @param  handle Handle of the device to be used
///     @param  globalOffset offset in [mm]
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetGlobalOffset(BTA_Handle handle, float globalOffset);



///     @brief  Function for getting the distance offset being applied to all pixels equally.
///             It is, for all current devices, valid for the currently set modulation frequency.
///             When changing the modulation frequency, the global offset changes.
///     @param  handle Handle of the device to be used
///     @param  globalOffset Pointer to hold offset in mm
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetGlobalOffset(BTA_Handle handle, float *globalOffset);



///     @brief  Reads registers from the device/SDK
///     @param  handle Handle of the device to be used
///     @param  address The address of the first register to read from
///     @param  data Pointer to buffer allocated by the caller. Contains register data on return.
///                  The data in the buffer on return consists of one or more register values, each 4 bytes wide.
///     @param  registerCount Pointer to the number of registers to be read.
///                           On return, if not null, it contains the number of registers actually read.
///                           If null is passed, one register is read.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAreadRegister(BTA_Handle handle, uint32_t address, uint32_t *data, uint32_t *registerCount);



///     @brief Writes registers to the device/SDK
///     @param handle Handle of the device to be used
///     @param address The address of the first register to write to
///     @param data Pointer to buffer containing register data to be written.
///                 The data in the buffer consists of one or more register values, each 4 bytes wide.
///     @param registerCount Pointer which contains the number of registers to be written.
///                          On return, if not null, it contains the number of registers actually written.
///                          If null is passed, one register is written.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAwriteRegister(BTA_Handle handle, uint32_t address, uint32_t *data, uint32_t *registerCount);



///     @brief  Convenience function for doing a firmware update. Uses BTAflashUpdate() internally
///     @param  handle Handle of the device to be used
///     @param  filename Name of the binary file
///     @param  progressReport Callback function for reporting the status and progress during transfer and programming. Can be null
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAfirmwareUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport);



///     @brief  Writes the current configuration (i.e. register values) to non-volatile memory
///     @param  handle Handle of the device to be used
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAwriteCurrentConfigToNvm(BTA_Handle handle);



///     @brief  Erases the register settings previously stored with BTAwriteCurrentConfigToNvm (May require rebbot for loading default values)
///     @param  handle Handle of the device to be used
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTArestoreDefaultConfig(BTA_Handle handle);


///     @brief  A convenience function to convert a device type into a string
///     @param deviceType The BTA_DeviceType to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char *BTA_CALLCONV BTAdeviceTypeToString(BTA_DeviceType deviceType);



///     @brief Obsolete, use BTAstatusToString2!!
///            A convenience function to convert a BTA_Status into a string
///     @param status The BTA_Status to be converted into a string
///     @param statusString A buffer allocated by the caller to contain the result on return
///     @param statusStringLen The length of the preallocated buffer in statusString
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAstatusToString(BTA_Status status, char* statusString, uint16_t statusStringLen);



///     @brief  A convenience function to convert a BTA_Status into a string
///     @param status The BTA_Status to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char* BTA_CALLCONV BTAstatusToString2(BTA_Status status);


///     @brief  A convenience function to convert a BTA_Unit into a string
///     @param unit The BTA_Unit to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char *BTA_CALLCONV BTAunitToString(BTA_Unit unit);


///     @brief  A convenience function to convert a BTA_ChannelId into a string
///     @param id The BTA_ChannelId to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char *BTA_CALLCONV BTAchannelIdToString(BTA_ChannelId id);


///     @brief  A convenience function to convert a BTA_LibParam into a string
///     @param libParam The BTA_LibParam to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char* BTA_CALLCONV BTAlibParamToString(BTA_LibParam libParam);


///     @brief  A convenience function to convert a BTA_FrameMode into a string
///     @param frameMode The BTA_FrameMode to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char* BTA_CALLCONV BTAframeModeToString(BTA_FrameMode frameMode);


///     @brief  A convenience function to convert a BTA_DataFormat into a string
///     @param dataFormat The BTA_DataFormat to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char *BTA_CALLCONV BTAdataFormatToString(BTA_DataFormat dataFormat);


///     @brief  A convenience function to convert a BTA_ChannelSelection into a string
///     @param channelSelection The BTA_ChannelSelection to be converted into a string
///     @return Returns a constant char* that is not allocated and doesn't need to be free'd
DLLEXPORT const char *BTA_CALLCONV BTAchannelSelectionToString(BTA_ChannelSelection channelSelection);

#ifdef __cplusplus
}
#endif

#endif
