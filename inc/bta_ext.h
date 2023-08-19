///  @file bta_ext.h
///
///  @brief The extended main header for BltTofApi. Includes advanced interface functions and the declaration of the config struct organisation BTA_ConfigStructOrg
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

#ifndef BTA_EXT_H_INCLUDED
#define BTA_EXT_H_INCLUDED

#define BTA_EXT_H_VER_MAJ 3
#define BTA_EXT_H_VER_MIN 3
#define BTA_EXT_H_VER_NON_FUNC 11

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif

#include "bta.h"





///     @brief  Enumerator with queueing modes
typedef enum BTA_QueueMode {
    BTA_QueueModeDoNotQueue = 0,                ///< No queueing
    BTA_QueueModeDropOldest = 1,                ///< Before an overflow, the oldest item in the queue is removed
    BTA_QueueModeDropCurrent = 2,               ///< When full, the queue remains unchanged
    BTA_QueueModeAvoidDrop = 3                  ///< When full, the equeue function returns an error to the producer (do not use in BTAopen()!)
} BTA_QueueMode;



///     @brief  Enumerator with runtime configuration parameters for the library.
///             The behaviour of the library can be changed by setting various LibParams.
///             Some information about the state of the library can be retrieved by reading various LibParams.
///             Never are they affecting the devices configuration (registers).
typedef enum BTA_LibParam {
    BTA_LibParamKeepAliveMsgInterval = 0,               ///< The interval in seconds. If no communication during this time, a keep alive is sent. (Supported only by Ethernet cameras).
    BTA_LibParamCrcControlEnabled = 1,                  ///< Set > 0 in order to activate CRC sums for the control connection. (Supported only by Ethernet cameras).

    BTA_LibParamBltstreamTotalFrameCount = 2,           ///< Readonly. Contains the total amount of frames loaded from bltstream file.
    BTA_LibParamBltstreamAutoPlaybackSpeed = 3,         ///< Set > 0 in order to activate playback at recording rate times this factor. Set to 0 to pause playback.
    BTA_LibParamBltstreamPos = 4,                       ///< Get and set the index in the bltstream file. The first frame has index 0. If set, BTA_LibParamStreamAutoPlaybackSpeed is set to 0.
    BTA_LibParamBltstreamPosIncrement = 5,              ///< Writeonly. Set the increment to which position to jump to in the bltstream file. If set, BTA_LibParamStreamAutoPlaybackSpeed is set to 0.


    BTA_LibParamPauseCaptureThread = 7,                     ///< Set > 0 in order to pause internal capture thread. It saves CPU and/or bandwith, depending on the configuration and interface.
    BTA_LibParamDisableDataScaling,                     ///< This is only relevant for implementations where depth is calculated in the lib rather than the camera.
    BTA_LibParamUndistortRgb,                           ///< > 0: Channels of the kind BTA_ChannelIdColor are undistorted if intrinsic data for that configuration is present

    BTA_LibParamInfoEventVerbosity,                     ///< Modify the BTA_Config parameter after BTAopen
    BTA_LibParamEnableJpgDecoding,                      ///< Enable jpg decoding (default on) or send raw jpeg data instead

    BTA_LibParamDataStreamReadFailedCount,              ///< Readonly: count of failed socket reads (read to clear!)
    BTA_LibParamDataStreamBytesReceivedCount,           ///< Readonly: count of bytes received (read to clear!)
    BTA_LibParamDataStreamPacketsReceivedCount,         ///< Readonly: count of received packets (read to clear!)
    BTA_LibParamDataStreamPacketsMissedCount,           ///< Readonly: count of packets missed (not received) (read to clear!)
    BTA_LibParamDataStreamPacketsToParse,               ///< Readonly: count of packets queued for parsing (max since last read, read to clear!)
    BTA_LibParamDataStreamParseFrameDuration,           ///< Readonly: time in microseconds needed to parse a frame (max since last read, read to clear!) [ms]
    BTA_LibParamDataStreamFrameCounterGapsCount,        ///< Readonly: this value increases by 1 every time two consecutive frameCounters are further apart than specified in BTA_LibParamDataStreamFrameCounterGap
    BTA_LibParamDataStreamFramesParsedCount,            ///< Readonly: count of frames parsed (read to clear!)
    BTA_LibParamDataStreamFramesParsedPerSec,           ///< Readonly: Frames parsed per second

    BTA_LibParamDataStreamRetrReqMode,                  ///< Retransmission requests for repeating the sending of data stream data
                                                        ///< 0: Retransmission off. Frames are delivered incompletely as soon as the timeout strikes or a newer frame is complete
                                                        ///< 1: Retransmission with low latency first. Frames are delivered as completely as possible but within the timeout and before a newer frame is complete
                                                        ///< 2: Retransmission with completeness first. Not yet implemented
    BTA_LibParamDataStreamPacketWaitTimeout,            ///< The time to wait for any packet of a certain frame to arrive before taking further action regarding retransmission [ms]
    BTA_LibParamDataStreamRetrReqIntervalMin,           ///< How long the Api should wait before repeating a retransmission request (gaps excluded) [ms]
    BTA_LibParamDataStreamRetrReqMaxAttempts,           ///< If no packet was received within BTA_LibParamDataStreamPacketWaitTimeout, attempt a retransmission request for all missing packets this many times before giving up
    BTA_LibParamDataStreamRetrReqsCount,                ///< Readonly: the number of packets requested for retransmissions (read to clear!)
    BTA_LibParamDataStreamRetransPacketsCount,          ///< Readonly: The number of packets received that are retransmissions (read to clear!)
    BTA_LibParamDataStreamNdasReceived,                 ///< Readonly: The number of NDAs (no data available) received (read to clear!)
    BTA_LibParamDataStreamRedundantPacketCount,         ///< Readonly: The number of packets received multiple times (read to clear!)

    BTA_LibParamDataSockOptRcvtimeo,                    ///< Lets you read and set the timeout of the socket [ms]
    BTA_LibParamDataSockOptRcvbuf,                      ///< Lets you modify the size of the receiving buffer of the socket [bytes]


    BTA_LibParamDataStreamFrameCounterGap = 50,         ///< This value is used to count gaps in BTA_LibParamDataStreamFrameCounterGapsCount


    BTA_LibParamCalcXYZ = 100,                          ///< If enabled, a distance channel is available and the lenscalib can be read from the device, cartesian coordinate channels X, Y and Z are calculated and added to the frame
    BTA_LibParamOffsetForCalcXYZ = 101,                 ///< This offset is applied to the distance channel before calculating the cartesian coordinates
    BTA_LibParamBilateralFilterWindow = 102,            ///< The bilateral filter with this window size is applied to any distance channel
    BTA_LibParamGenerateColorFromTof = 103,             ///< >0: Based on data from ToF sensor a channel with BTA_ChanneldIdColor is added (and possibly undistorted)
    BTA_LibParamBltstreamCompressionMode = 104,         ///< Set a value of BTA_CompressionMode in order to activate compression when grabbing

    BTA_LIBParamDataStreamAllowIncompleteFrames = 200,  ///< Set this parameter to 1 if you wish to receive incomplete frames (pixels missing due to transmission errors are invalidated according to camera manual)

    BTA_LibParamDebugFlags01 = 5000,                    ///< For debug purposes
    BTA_LibParamDebugValue01,                           ///< For debug purposes
    BTA_LibParamDebugValue02,                           ///< For debug purposes
    BTA_LibParamDebugValue03,                           ///< For debug purposes
    BTA_LibParamDebugValue04,                           ///< For debug purposes
    BTA_LibParamDebugValue05,                           ///< For debug purposes
    BTA_LibParamDebugValue06,                           ///< For debug purposes
    BTA_LibParamDebugValue07,                           ///< For debug purposes
    BTA_LibParamDebugValue08,                           ///< For debug purposes
    BTA_LibParamDebugValue09,                           ///< For debug purposes
    BTA_LibParamDebugValue10,                           ///< For debug purposes


    // for backward compatibility
    BTA_LibParamStreamTotalFrameCount = BTA_LibParamBltstreamTotalFrameCount,           ///< Obsolete, use BTA_LibParamBltstreamTotalFrameCount!
    BTA_LibParamBltStreamTotalFrameCount = BTA_LibParamBltstreamTotalFrameCount,        ///< Obsolete, use BTA_LibParamBltstreamTotalFrameCount!
    BTA_LibParamStreamAutoPlaybackSpeed = BTA_LibParamBltstreamAutoPlaybackSpeed,       ///< Obsolete, use BTA_LibParamBltstreamAutoPlaybackSpeed!
    BTA_LibParamBltStreamAutoPlaybackSpeed = BTA_LibParamBltstreamAutoPlaybackSpeed,    ///< Obsolete, use BTA_LibParamBltstreamAutoPlaybackSpeed!
    BTA_LibParamStreamPos = BTA_LibParamBltstreamPos,                                   ///< Obsolete, use BTA_LibParamBltstreamPos!
    BTA_LibParamBltStreamPos = BTA_LibParamBltstreamPos,                                ///< Obsolete, use BTA_LibParamBltstreamPos!
    BTA_LibParamStreamPosIncrement = BTA_LibParamBltstreamPosIncrement,                 ///< Obsolete, use BTA_LibParamBltstreamPosIncrement!
    BTA_LibParamBltStreamPosIncrement = BTA_LibParamBltstreamPosIncrement,              ///< Obsolete, use BTA_LibParamBltstreamPosIncrement!
    BTA_LibParamBytesReceivedStream = BTA_LibParamDataStreamBytesReceivedCount,         ///< Obsolete, use BTA_LibParamDataStreamBytesReceivedCount!
    BTA_LibParamFramesParsedCount = BTA_LibParamDataStreamFramesParsedCount,            ///< Obsolete, use BTA_LibParamDataStreamFramesParsedCount!

} BTA_LibParam;



///     @brief  Configuration structure to be passed with BTAstartGrabbing.
typedef struct BTA_GrabbingConfig {
    uint8_t *filename;                                      ///< The filename of the *.bltstream file. Where the grabbing process stores the stream. (ASCII coded).
} BTA_GrabbingConfig;


typedef enum BTA_CompressionMode {
    BTA_CompressionModeNone,
    BTA_CompressionModeLzmaV22,
} BTA_CompressionMode;


///     @brief  Data structure for holding intrinsic parameters.
typedef struct BTA_IntrinsicData {
    uint16_t xRes;                                      ///< Resolution of the (image) sensor.
    uint16_t yRes;                                      ///< Resolution of the (image) sensor.
    float fx;                                           ///< Focal lengths of the lens.
    float fy;                                           ///< Focal lengths of the lens.
    float cx;                                           ///< Center point in pixels.
    float cy;                                           ///< Center point in pixels.
    float k1;                                           ///< Distortion coefficient 1.
    float k2;                                           ///< Distortion coefficient 2.
    float k3;                                           ///< Distortion coefficient 3.
    float k4;                                           ///< Distortion coefficient 4.
    float k5;                                           ///< Distortion coefficient 5.
    float k6;                                           ///< Distortion coefficient 6.
    float p1;                                           ///< Distortion coefficient polar 1.
    float p2;                                           ///< Distortion coefficient polar 2.
    uint16_t lensIndex;                                 ///< Lens index (used to identify corresponding sensor).
    uint16_t lensId;                                    ///< Lens ID as used in hardware config register(s).
} BTA_IntrinsicData;


///     @brief Data structure for holding extrinsic parameters.
typedef struct BTA_ExtrinsicData {
    uint16_t lensIndex;                                 ///< Lens index (used to identify corresponding sensor).
    uint16_t lensId;                                    ///< Lens ID as used in hardware config register(s).
    float rot[9];                                       ///< Rotation matrix (3x3).
    float trl[3];                                       ///< Translation vector.
    float rotTrlInv[12];                                ///< Inverse rotation translation matrix (3x4).
} BTA_ExtrinsicData;


typedef struct BTA_LensVectors {
    uint16_t lensIndex;                                 ///< Lens index (used to identify corresponding sensor).
    uint16_t lensId;                                    ///< Lens ID as used in hardware config register(s).
    uint16_t xRes;                                      ///< Resolution of the (image) sensor.
    uint16_t yRes;                                      ///< Resolution of the (image) sensor.
    float* vectorsX;                                    ///< Array of xRes*yRes float values: x-component of vectors
    float* vectorsY;                                    ///< Array of xRes*yRes float values: y-component of vectors
    float* vectorsZ;                                    ///< Array of xRes*yRes float values: z-component of vectors
} BTA_LensVectors;


///     @brief  This struct is used for the representation of the BTA_Config struct.
///             Programming languages that don't use header files are able to query the elements of BTA_Config generically.
typedef struct BTA_ConfigStructOrg {
    const char *variableName;                           ///< The name of the field in the BTA_Config
    uint8_t pointer;                                    ///< 1 --> field is a pointer, 0 --> field is not a pointer
} BTA_ConfigStructOrg;



///     @brief  This struct contains information about the BTA_Config struct
DLLEXPORT extern BTA_ConfigStructOrg btaConfigStructOrg[];
///     @brief  The size of the struct containing information about the BTA_Config struct
DLLEXPORT extern const uint32_t btaConfigStructOrgLen;



//----------------------------------------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif

///     @brief  With this function a literal description of the config struct can be retrieved.
///             In programming languages which do not support header files it can be used to allocate and fill the BTA_Config struct.
///     @param  fieldCount The number of elements (config struct fields) in the result on return
///     @param  bytesPerField The number of bytes per element (config struct field) in the result on return
DLLEXPORT BTA_ConfigStructOrg *BTA_CALLCONV BTAgetConfigStructOrg(uint32_t *fieldCount, uint8_t *bytesPerField);



///     @brief  Queries whether the handle is open and background threads are running correctly
///     @param  handle Handle of the device to be used
///     @return 1 if the service is running, 0 otherwise
DLLEXPORT uint8_t BTA_CALLCONV BTAisRunning(BTA_Handle handle);



///     @brief  Helper function to calculate the length of a frame in serialized form; needed for BTAserializeFrame
///     @param  frame The pointer to the frame to be serialized
///     @param  frameSerializedLen Pointer to the length of the (to be) serialized frame on return.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetSerializedLength(BTA_Frame *frame, uint32_t *frameSerializedLen);


///     @brief  Helper function to convert a BTA_Frame structure into a serialized stream; useful for recording frames to files
///     @param  frame The pointer to the frame to be serialized
///     @param  frameSerialized Buffer to contain the frame data on return. Must be allocated by the caller
///     @param  frameSerializedLen Pointer to length of the buffer frameSerialized allocated by the caller.
///                                Pointer to the actual length of the serialized frame on return.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAserializeFrame(BTA_Frame *frame, uint8_t *frameSerialized, uint32_t *frameSerializedLen);


///     @brief  Helper function to compress a serialized BTA_Frame
///     @param  frameSerialized The pointer to the serialized frame
///     @param  frameSerializedLen The length of the buffer frameSerialized.
///     @param  compressionMode This parameter dictates the compression algorithm
///     @param  frameSerializedCompressed Pointer to the result buffer. The user needs to do the allocation
///     @param  frameSerializedCompressedLen Pointer to length of the buffer frameSerializedCompressed allocated by the caller.
///                                          Pointer to the actual length of the frameSerializedCompressed frame on return.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAcompressSerializedFrame(uint8_t *frameSerialized, uint32_t frameSerializedLen, BTA_CompressionMode compressionMode, uint8_t *frameSerializedCompressed, uint32_t *frameSerializedCompressedLen);


///     @brief  Helper function to convert a serialized stream into a BTA_Frame structure; useful for replaying recorded frames from files
///     @param  frame Double pointer to the deserialized frame on return.
///                   Don't forget to call BTAfreeFrame.
///     @param  frameSerialized Buffer that contains the frame data
///     @param  frameSerializedLen Pointer to length of the buffer frameSerialized; number of bytes parsed on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAdeserializeFrame(BTA_Frame **frame, uint8_t *frameSerialized, uint32_t *frameSerializedLen);



///     @brief  Convenience function for extracting metadata from a provided channel.
///             It simply returns the pointer and the length of the data. The same data can be accessed directly going through the BTA_Channel structure.
///             If there is no metadata with the right id present in the channel, an error is returned.
///     @param  channel The channel from which to extract the metadata
///     @param  metadataId The metadata id that should be extracted
///     @param  metadata Pointer to the metadata on return (null on error)
///     @param  metadataLen Pointer to the length of data in metadata in [byte] on return
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetMetadata(BTA_Channel *channel, uint32_t metadataId, void **metadata, uint32_t *metadataLen);



///     @brief  Function for setting a parameter for the library. Library parameters do not directly affect the camera's configuration.
///     @param  handle      Handle of the device to be used
///     @param  libParam    Identifier for the parameter (consult the support wiki for a description of its function)
///     @param  value       The value to be set for the library parameter
///     @return             Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetLibParam(BTA_Handle handle, BTA_LibParam libParam, float value);


///     @brief  Function for getting a parameter from the library.
///     @param  handle      Handle of the device to be used
///     @param  libParam    Identifier for the parameter (consult the support wiki for a description of its function)
///     @param  value       On return it points to the value of the library parameter
///     @return             Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetLibParam(BTA_Handle handle, BTA_LibParam libParam, float *value);




///     @brief  Initiates a reset of the device
///     @param  handle Handle of the device to be used
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAsendReset(BTA_Handle handle);




///     @brief  Fills the flash update/read config structure with standard values
///     @param  config Pointer to the structure to be initialized to standard values
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAinitFlashUpdateConfig(BTA_FlashUpdateConfig *config);



///     @brief  Allows sending large data to the device.
///             Mainly this is used for sending calibration data and firmware updates.
///             This function handles the complete transfer of the file and blocks during transmission.
///             If possible, this function performs its task in a way that allows the OS to
///             regain control periodically, i.e. it uses blocking functions in a fine granularity.
///             The callback function is called (if not null) in the following cases:
///             - An error occurs -> report error status ignoring percentage.
///             - An transmission starts -> report BTA_StatusOk with percentage 0%.
///             - An transmission ends -> report BTA_StatusOk with percentage 100%.
///             (--> so the callback will always be used at least once and at least twice in case of success).
///             During transmission, progress is reported repeatedly when possible (report BTA_StatusOk with percentage).
///     @param  handle Handle of the device to be used
///     @param  flashUpdateConfig Contains the data and all the necessary information for handling it
///     @param  progressReport Callback function for reporting the status and progress during transfer and programming
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAflashUpdate(BTA_Handle handle, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);


///     @brief  Convenience function for doing a firmware update. Uses BTAflashUpdate() internally
///     @param  handle Handle of the device to be used
///     @param  filename Name of the binary file
///     @param  progressReport Callback function for reporting the status and progress during transfer and programming. Can be null
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAwigglingUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport);


///     @brief  Convenience function for doing a firmware update. Uses BTAflashUpdate() internally
///     @param  handle Handle of the device to be used
///     @param  filename Name of the binary file
///     @param  progressReport Callback function for reporting the status and progress during transfer and programming. Can be null
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAfpnUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport);


///     @brief  Convenience function for doing a firmware update. Uses BTAflashUpdate() internally
///     @param  handle Handle of the device to be used
///     @param  filename Name of the binary file
///     @param  progressReport Callback function for reporting the status and progress during transfer and programming. Can be null
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAfppnUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport);


///     @brief  Allows reading large data from the device.
///             This function handles the complete transfer of the file and blocks during transmission.
///             If possible, this function performs its task in a way that allows the OS to
///             regain control periodically, i.e. it uses blocking functions in a fine granularity.
///             The callback function is called (if not null) in the following cases:
///             - An error occurs -> report error status ignoring percentage.
///             - An transmission starts -> report BTA_StatusOk with percentage 0%.
///             - An transmission ends -> report BTA_StatusOk with percentage 100%.
///             (--> so the callback will always be used at least once and at least twice in case of success).
///             During transmission, progress is reported repeatedly when possible (report BTA_StatusOk with percentage).
///     @param  handle Handle of the device to be used
///     @param  flashUpdateConfig Must contain the flash target and flash id to read from and optionally the flash address.
///                               Can have have the pointer data with preallocated space denoted in dataLen.
///                               If dataLen is 0 the function will allocate the needed memory but the user is still responsible for freeing it.
///                               On return it contains the data read and its actual length.
///     @param  progressReport Callback function for reporting the status and progress during transfer
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAflashRead(BTA_Handle handle, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);



///     @brief  Allows access to intrinsic and extrinsic parameters stored on the camera.
///             The sensors and corresponding lenses on the camera have a predetermined order. This function lists the parameters in that same order.
///             If there is no data available for a certain lens, the struct is filled with zeros.
///     @param  handle Handle of the device to be used.
///     @param  intData A preallocated array of BTA_IntrinsicData. On return it contains data gathered from the camera, when available.
///     @param  intDataLen A pointer to the length of the array intData. On return it contains the actual length.
///     @param  extData A preallocated array of BTA_ExtrinsicData. On return it contains data gathered from the camera, when available.
///     @param  extDataLen A pointer to the length of the preallocated array extData. On return it contains the actual length.
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetLensParameters(BTA_Handle handle, BTA_IntrinsicData *intrinsicData, uint32_t *intrinsicDataLen, BTA_ExtrinsicData *extrinsicData, uint32_t *extrinsicDataLen);


///     @brief  Allows access to intrinsic parameters in vector format stored on the camera.
///             These vectors, also called base vectors are optical axes for each pixel and can be used to compute cartesian coordinates from distances
///     @param  handle Handle of the device to be used.
///     @param  lensVectorsList A preallocated array of BTA_LensVectors pointers. On return it contains data gathered from the camera, when available.
///     @param  lensVectorsListLen A pointer to the length of the preallocated array lensVectorsList. On return it contains the actual length.
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetLensVectors(BTA_Handle handle, BTA_LensVectors **lensVectorsList, uint16_t *lensVectorsListLen);


///     @brief  Frees the allocated memory of and for a BTA_LensVector struct and its contents
///     @param  lensVectors The pointer to the struct to be free'd (as received from BTAgetLensVectors)
DLLEXPORT void BTA_CALLCONV BTAfreeLensVectors(BTA_LensVectors *lensVectors);



///     @brief  Fills the grab config structure with standard values
///     @param  config Pointer to the structure to be initialized to standard values
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAinitGrabbingConfig(BTA_GrabbingConfig *config);


///     @brief  Issues the library to grab (or stop grabbing) all the frames and store them to the file provided in the config.
///     @param  handle Handle of the device to be used.
///     @param  config Pointer to the config structure specifying parameters for the grabbing process.
///                    Pass null in order to stop grabbing.
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAstartGrabbing(BTA_Handle handle, BTA_GrabbingConfig *config);




///     @brief  A convenience function to convert a BTA_Status into a string
///     @param status The BTA_Status to be converted into a string
///     @param statusString A buffer allocated by the caller to contain the result on return
///     @param statusStringLen The length of the preallocated buffer in statusString
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAstatusToString(BTA_Status status, char* statusString, uint16_t statusStringLen);


///     @brief  A convenience function to convert a BTA_EventId (== BTA_Status) into a string
///     @param status The BTA_EventId (BTA_Status) to be converted into a string
///     @param statusString A buffer allocated by the caller to contain the result on return
///     @param statusStringLen The length of the preallocated buffer in statusString
///     @return Please refer to bta_status.h
DLLEXPORT BTA_Status BTA_CALLCONV BTAeventIdToString(BTA_EventId status, char *statusString, uint16_t statusStringLen);


///     @brief  DEPRECATED  Please use LibParam BTA_LibParamKeepAliveMsgInterval instead
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetKeepAliveMsgInterval(BTA_Handle handle, float interval);
///     @brief  DEPRECATED  Please use LibParam BTA_LibParamCrcControlEnabled instead
DLLEXPORT BTA_Status BTA_CALLCONV BTAsetControlCrcEnabled(BTA_Handle handle, uint8_t enabled);





// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// Blt ToF API Frame Queue - A courtesy of Bluetechnix Systems GmbH




typedef void* BFQ_FrameQueueHandle;

///     @brief  Initializes a handy frame queue for queueing frames.
///             It can be used to store frames or in order to pass them to another thread.
///             Yes, it is thread-save
///     @param frameQueueLength Pass the desired maximum length of the queue.
///     @param frameQueueMode This option defines the behaviour of the queue in case frameQueueLength is reached.
///     @param handle On return the pointer points to a handle (of type void *) to the queue.
///     @return BTA_StatusOk on success
///             BTA_StatusOutOfMemory if mode set to BTA_QueueModeAvoidDrop and queue is full
DLLEXPORT BTA_Status BTA_CALLCONV BFQinit(uint32_t frameQueueLength, BTA_QueueMode frameQueueMode, BFQ_FrameQueueHandle *handle);


///     @brief  Closes the instance of the queue instantiated with BFQinit
///     @param handle The pointer to the handle (of type BFQ_FrameQueueHandle) to be closed and free'd
DLLEXPORT BTA_Status BTA_CALLCONV BFQclose(BFQ_FrameQueueHandle *handle);


///     @brief  Function to query the current amount of frames in the queue
///     @param handle Handle of the queue
///     @param count Pointer that points to the result on return
DLLEXPORT BTA_Status BTA_CALLCONV BFQgetCount(BFQ_FrameQueueHandle handle, uint32_t *count);


///     @brief Enqueues a frame. The queue does not clone the frame, so please do not call BTAfreeFrame after enqueueing it.
///     @param handle Handle of the queue
///     @param frame The frame to be enqueued
///     @return BTA_StatusOk on success
///             BTA_StatusOutOfMemory if mode set to BTA_QueueModeAvoidDrop and queue is full
DLLEXPORT BTA_Status BTA_CALLCONV BFQenqueue(BFQ_FrameQueueHandle handle, BTA_Frame *frame);


///     @brief Dequeues a frame. You get the exact same frame that was enqueued, not a clone. So now is the time to call BTAfreeFrame (when frame is no longer needed)
///     @param handle Handle of the queue
///     @param frame Pointer to the frame dequeued on return
///     @param timeout The maximum time in [ms] to wait for a frame to become available if the queue is currently empty. (0: infinite!)
///     @return BTA_StatusOk on success
///             BTA_StatusTimeOut if there is no frame in the queue
DLLEXPORT BTA_Status BTA_CALLCONV BFQdequeue(BFQ_FrameQueueHandle handle, BTA_Frame **frame, uint32_t timeout);


///     @brief Empties the queue. Also, BTAfreeFrame is called on all the inserted frames.
///     @param handle Handle of the queue
///     @return BTA_StatusOk on success
DLLEXPORT BTA_Status BTA_CALLCONV BFQclear(BFQ_FrameQueueHandle handle);






// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// Processing of frames and channels, for your convenience


///     @brief Calculates the average of multiple frames
///     @param frames A list of pointers to BTA_Frame
///     @param framesLen Lenght of the list frames
///     @param minValidPixelPercentage If a pixel is invalid in more frames than this percentage, the resulting pixel will also be invalid
///                                    If there are less invalid values for a specific pixel than this threshold, the remaining valid values are
///                                    averaged and invalid values are ignored
///     @param result A newly allocated frame with all frames averaged into one (same channels, same format, same resolution, ...)
///     @return BTA_StatusOk on success
DLLEXPORT BTA_Status BTA_CALLCONV BTAaverageFrames(BTA_Frame** frames, int framesLen, float minValidPixelPercentage, BTA_Frame** result);


///     @brief Calculates the average of multiple channels
///     @param channels A list of pointers to BTA_Channel
///     @param channelsLen Lenght of the list channels
///     @param minValidPixelPercentage If a pixel is invalid in more channels than this percentage, the resulting pixel will also be invalid
///                                    If there are less invalid values for a specific pixel than this threshold, the remaining valid values are
///                                    averaged and invalid values are ignored
///     @param result A newly allocated channel with all channels averaged into one (same format, same resolution, ...)
///     @return BTA_StatusOk on success
DLLEXPORT BTA_Status BTA_CALLCONV BTAaverageChannels(BTA_Channel **channels, int channelsLen, float minValidPixelPercentage, BTA_Channel **result);


///     @brief Calculates cartesian coordinates from a distance channel and calibration data
///     @param frame The frame containing at least one Distance channel. On return 3 channels X, Y and Z are inserted.
///     @param lensVectorsList List of Intrinsic calibration data in vector format as returned by BTAgetLensVectors or BTAparseLensCalib
///     @param lensVectorsListLen Number of elements in lensVectorsList
///     @param extrinsicDataList Optional parameter containing List of extrinsic data. This is used to transform the point-cloud into the respective coordinate system.
///     @param extrinsicDataListLen Number of elements in extrinsicDataList
///     @return BTA_StatusOk on success
DLLEXPORT BTA_Status BTA_CALLCONV BTAcalcXYZ(BTA_Frame *frame, BTA_LensVectors **lensVectorsList, uint16_t lensVectorsListLen, BTA_ExtrinsicData **extrinsicDataList, uint16_t extrinsicDataListLen);


DLLEXPORT BTA_Status BTA_CALLCONV BTAcalcMonochromeFromAmplitude(BTA_Frame *frame);



// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// Undocumented utility functions - use at your own risk

DLLEXPORT void BTA_CALLCONV BTAzeroLogTimestamp();

DLLEXPORT uint8_t BTA_CALLCONV BTAisEthDevice(uint16_t deviceType);
DLLEXPORT uint8_t BTA_CALLCONV BTAisUsbDevice(uint16_t deviceType);
DLLEXPORT uint8_t BTA_CALLCONV BTAisP100Device(uint16_t deviceType);
DLLEXPORT uint8_t BTA_CALLCONV BTAisUartDevice(uint16_t deviceType);

DLLEXPORT BTA_Status BTA_CALLCONV BTAinsertChannelIntoFrame(BTA_Frame *frame, BTA_Channel *channel);
DLLEXPORT BTA_Status BTA_CALLCONV BTAinsertChannelIntoFrame2(BTA_Frame *frame, BTA_ChannelId id, uint16_t xRes, uint16_t yRes, BTA_DataFormat dataFormat, BTA_Unit unit, uint32_t integrationTime, uint32_t modulationFrequency, uint8_t *data, uint32_t dataLen,
                                                             BTA_Metadata **metadata, uint32_t metadataLen, uint8_t lensIndex, uint32_t flags, uint8_t sequenceCounter, float gain);
DLLEXPORT BTA_Status BTA_CALLCONV BTAremoveChannelFromFrame(BTA_Frame *frame, BTA_Channel *channel);
DLLEXPORT BTA_Status BTA_CALLCONV BTAcloneChannel(BTA_Channel *channelSrc, BTA_Channel **channelDst);
DLLEXPORT BTA_Status BTA_CALLCONV BTAcloneChannelEmpty(BTA_Channel *channelSrc, BTA_Channel **channelDst);
DLLEXPORT BTA_Status BTA_CALLCONV BTAinsertMetadataIntoChannel(BTA_Channel *channel, BTA_Metadata *metadata);
DLLEXPORT BTA_Status BTA_CALLCONV BTAinsertMetadataDataIntoChannel(BTA_Channel *channel, BTA_MetadataId id, void *data, uint32_t dataLen);
DLLEXPORT BTA_Status BTA_CALLCONV BTAcloneMetadata(BTA_Metadata *metadataSrc, BTA_Metadata **metadataDst);
DLLEXPORT BTA_Status BTA_CALLCONV BTAdivideChannelByNumber(BTA_Channel *dividend, uint32_t divisor, BTA_Channel **quotient);
DLLEXPORT BTA_Status BTA_CALLCONV BTAaddChannelInPlace(BTA_Channel *augendSum, BTA_Channel *addend);
DLLEXPORT BTA_Status BTA_CALLCONV BTAsubtChannelInPlace(BTA_Channel *minuendDiff, BTA_Channel *subtrahend);
DLLEXPORT BTA_Status BTA_CALLCONV BTAsubtChannel(BTA_Channel *minuend, BTA_Channel *subtrahend, BTA_Channel **diff);
DLLEXPORT BTA_Status BTA_CALLCONV BTAthresholdInPlace(BTA_Channel *channel, uint32_t threshold, uint8_t alsoNegative);
DLLEXPORT BTA_Status BTA_CALLCONV BTAchangeDataFormat(BTA_Channel *channel, BTA_DataFormat dataFormat);
DLLEXPORT BTA_Status BTA_CALLCONV BTAfreeChannel(BTA_Channel **channel);
DLLEXPORT BTA_Status BTA_CALLCONV BTAfreeMetadata(BTA_Metadata **metadata);

///     @brief Sets or cleares the videoMode flag in register Mode0
///     @param handle Handle of the queue
///     @param modulationFrequencies address of a ponter: on return this points to the list in static memory. do not free or modify!
///     @return BTA_StatusOk on success
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetValidModulationFrequencies(BTA_Handle handle, const uint32_t **modulationFrequencies, int32_t *modulationFrequenciesCount);
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetNextBestModulationFrequency(BTA_Handle handle, uint32_t modFreq, uint32_t *validModFreq, int32_t *index);
DLLEXPORT BTA_Status BTA_CALLCONV BTAgetOptimalAmplitude(uint16_t deviceType, float *amplitude);


///     @brief Sets or cleares the videoMode flag in register Mode0
///     @param handle Handle of the queue
///     @param flagVideoMode Set the parameter to 1 or 0 in order to turn videoMode on/off.
///                          On return this parameter contains the value the videoMode flag had before (for easy restoration).
///     @return BTA_StatusOk on success
DLLEXPORT BTA_Status BTAsetVideoMode(BTA_Handle handle, uint8_t *flagVideoMode);
DLLEXPORT BTA_Status BTAsetSoftwareTrigger(BTA_Handle handle);

DLLEXPORT BTA_FrameMode BTA_CALLCONV BTAimageDataFormatToFrameMode(int deviceType, int imageMode);
DLLEXPORT int BTA_CALLCONV BTAframeModeToImageMode(int deviceType, BTA_FrameMode frameMode);

DLLEXPORT BTA_Status BTA_CALLCONV BTAgetNetworkBroadcastAddrs(uint8_t ***localIpAddrs, uint8_t ***networkBroadcastAddrs, uint32_t *networkBroadcastAddrsLen);
DLLEXPORT void BTA_CALLCONV BTAfreeNetworkBroadcastAddrs(uint8_t ***localIpAddrs, uint8_t ***networkBroadcastAddrs, uint32_t networkBroadcastAddrsLen);

DLLEXPORT void BTA_CALLCONV BTAgeneratePlanarView(int16_t *chX, int16_t *chY, int16_t *chZ, uint16_t *chAmp, int resX, int resY, int planarViewResX, int planarViewResY, float planarViewScale, int16_t *planarViewZ, uint16_t *planarViewAmp);


DLLEXPORT BTA_Status BTA_CALLCONV BTAfreeFrameFromShm(BTA_Frame **frame);
#ifdef __cplusplus
}
#endif

#endif
