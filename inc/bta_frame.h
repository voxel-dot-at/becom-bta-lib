///  @file bta_frame.h
///
///  @brief This header file contains enums and structs dor the representation of a BTA_Frame
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

#ifndef BTA_FRAME_H_INCLUDED
#define BTA_FRAME_H_INCLUDED

#define BTA_FRAME_H_VER_MAJ 3
#define BTA_FRAME_H_VER_MIN 3
#define BTA_FRAME_H_VER_NON_FUNC 11

#include "bta_status.h"
#include <stdint.h>



///     @brief  Enumerator with valid frame modes to be passed with BTAsetFrameMode.
///             Not all frame modes are supported by all the cameras, please check the user manual.
typedef enum BTA_FrameMode {
    BTA_FrameModeCurrentConfig = 0,      ///< The sensors settings are not changed and data is passed through (according to device's current calculation/image mode settings)
    BTA_FrameModeDistAmp,                ///< Distance, Amplitude
    BTA_FrameModeZAmp,                   ///< Z coordinates, Amplitude
    BTA_FrameModeDistAmpFlags,           ///< Distance, Amplitude, Flags (see camera user manual)
    BTA_FrameModeXYZ,                    ///< X, Y, Z coordinates
    BTA_FrameModeXYZAmp,                 ///< X, Y, Z coordinates, Amplitude
    BTA_FrameModeDistAmpColor,           ///< Distance, Amplitude, Color
    BTA_FrameModeXYZAmpFlags,            ///< X, Y, Z coordinates, Amplitude, Flags (see camera user manual)
    BTA_FrameModeRawPhases,              ///< Raw phase data untouched as delivered by the sensor
    BTA_FrameModeIntensities,            ///< Intensity (Ambient light)
    BTA_FrameModeDistColor,              ///< Distance, RGB data
    BTA_FrameModeDistAmpBalance,         ///< Distance, Amplitude, Balance (see camera user manual)
    BTA_FrameModeXYZColor,               ///< X, Y, Z coordinates, RGB data overlay (see camera user manual)
    BTA_FrameModeDist,                   ///< Distance
    BTA_FrameModeDistConfExt,            ///< Distance, Confidence (Different image processing behaviour, consult camera software manual)
    BTA_FrameModeAmp,                    ///< Amplitude
    BTA_FrameModeRawdistAmp,             ///< Distance raw, Amplitude
    BTA_FrameModeRawPhasesExt,           ///< Up to 8 channels of BTA_ChannelIdRawPhase
    BTA_FrameModeRawQI,                  ///< BTA_ChannelIdRawI and BTA_ChannelIdRawQ
    BTA_FrameModeXYZConfColor,           ///< Cartesian, Confidence, RGB
    BTA_FrameModeXYZAmpColorOverlay,     ///< Cartesian, Amplitude, RGB, overlay
    BTA_FrameModeDistAmpConf,            ///< Distance, Amplitude, Confidence (see camera user manual)
    BTA_FrameModeChannelSelection = 255, ///< Frame mode is defined by channel selection registers (see camera user manual)
} BTA_FrameMode;


///     @brief Enumerator with channel selection values. These are used to select the content and shape of the channels to be streamed
typedef enum BTA_ChannelSelection {
    BTA_ChannelSelectionInactive,
    BTA_ChannelSelectionDistance,
    BTA_ChannelSelectionAmplitude,
    BTA_ChannelSelectionX,
    BTA_ChannelSelectionY,
    BTA_ChannelSelectionZ,
    BTA_ChannelSelectionConfidence,
    BTA_ChannelSelectionHeightMap,
    BTA_ChannelSelectionStdev,
    BTA_ChannelSelectionColor0,
    BTA_ChannelSelectionOverlay0,
    BTA_ChannelSelectionColor1,
    BTA_ChannelSelectionOverlay1,
    BTA_ChannelSelectionAmplitude8,
} BTA_ChannelSelection;


///     @brief Enumerator with channel IDs. They allow the identification of the various channels in a BTA_Frame
typedef enum BTA_ChannelId {
    BTA_ChannelIdUnknown =        0x0,
    BTA_ChannelIdDistance =       0x1,
    BTA_ChannelIdAmplitude =      0x2,
    BTA_ChannelIdX =              0x4,
    BTA_ChannelIdY =              0x8,
    BTA_ChannelIdZ =             0x10,
    BTA_ChannelIdHeightMap =     0x11,
    BTA_ChannelIdConfidence =    0x20,
    BTA_ChannelIdFlags =         0x40,
    BTA_ChannelIdPhase0 =        0x80,   // Used for raw sensor output (phase shift 0°)
    BTA_ChannelIdPhase90 =      0x100,   // Used for raw sensor output (phase shift 90°)
    BTA_ChannelIdPhase180 =     0x200,   // Used for raw sensor output (phase shift 180°)
    BTA_ChannelIdPhase270 =     0x400,   // Used for raw sensor output (phase shift 270°)
    BTA_ChannelIdRawPhase =      0x81,   // Used for raw sensor output (variable phase shift)
    BTA_ChannelIdRawQ =          0x82,
    BTA_ChannelIdRawI =          0x83,
    BTA_ChannelIdTest =         0x800,
    BTA_ChannelIdColor =       0x1000,
    BTA_ChannelIdRawDist =     0x4000,   // Unfiltered unitless distance values (full unumbiguous range) scaled to full range of corresponding BTA_DataFormat (former BTA_ChannelIdPhase)
    BTA_ChannelIdBalance =    0x10000,
    BTA_ChannelIdStdDev =     0x20000,

    BTA_ChannelIdCustom01 = 0x1000000,
    BTA_ChannelIdCustom02 = 0x2000000,
    BTA_ChannelIdCustom03 = 0x3000000,
    BTA_ChannelIdCustom04 = 0x4000000,
    BTA_ChannelIdCustom05 = 0x5000000,
    BTA_ChannelIdCustom06 = 0x6000000,
    BTA_ChannelIdCustom07 = 0x7000000,
    BTA_ChannelIdCustom08 = 0x8000000,
    BTA_ChannelIdCustom09 = 0x9000000,
    BTA_ChannelIdCustom10 = 0xa000000,
} BTA_ChannelId;

#define BTA_ChannelIdGrayScale BTA_ChannelIdColor       // Deprecated, same as BTA_ChannelIdColor
#define BTA_ChannelIdPhase BTA_ChannelIdRawDist         // deprecated because confusing naming


///     @brief Enumerator with data formats which allows the parsing of the data in BTA_Channel.
///            The lowbyte stands for width (number of bytes) where known.
///            The highbyte stands for unsigned/signed/floating-point/color (continuous numbering).
typedef enum BTA_DataFormat {
    BTA_DataFormatUnknown         = 0x0,
    BTA_DataFormatUInt8           = 0x11,
    BTA_DataFormatUInt16          = 0x12,
    BTA_DataFormatSInt16Mlx1C11S  = 0x62,
    BTA_DataFormatSInt16Mlx12S    = 0x72,
    BTA_DataFormatUInt16Mlx1C11U  = 0x82,
    BTA_DataFormatUInt16Mlx12U    = 0x92,
    //BTA_DataFormatUInt24          = 0x13,
    BTA_DataFormatUInt32          = 0x14,
    //BTA_DataFormatSInt8           = 0x21,
    BTA_DataFormatSInt16          = 0x22,
    //BTA_DataFormatSInt24          = 0x23,
    BTA_DataFormatSInt32          = 0x24,
    //BTA_DataFormatFloat8          = 0x31,
    //BTA_DataFormatFloat16         = 0x32,
    //BTA_DataFormatFloat24         = 0x33,
    BTA_DataFormatFloat32         = 0x34,
    BTA_DataFormatFloat64         = 0x38,
    BTA_DataFormatRgb565          = 0x42,
    BTA_DataFormatRgb24           = 0x43,
	BTA_DataFormatJpeg            = 0x50,
    BTA_DataFormatYuv422          = 0x52, //YUV
    BTA_DataFormatYuv444          = 0x53, //YUV
    BTA_DataFormatYuv444UYV       = 0x63, //UYV
} BTA_DataFormat;

// legacy, do not use
#define BTA_DataFormatUInt16Mlx1C11S BTA_DataFormatSInt16Mlx1C11S
#define BTA_DataFormatUInt16Mlx12S BTA_DataFormatSInt16Mlx12S

///     @brief Enumerator with units which allows the interpretation of the data in a channel
typedef enum BTA_Unit {
    BTA_UnitUnitLess = 0,
    BTA_UnitMeter = 1,
    //BTA_UnitCentimeter = 2,
    BTA_UnitMillimeter = 3,
    //BTA_UnitPercent = 4,
} BTA_Unit;


typedef enum BTA_MetadataId {
    BTA_MetadataIdChessboardCorners     = 0xab8471f9,
    BTA_MetadataIdMlxMeta1              = 0xa720b906,
    BTA_MetadataIdMlxMeta2              = 0xa720b907,
    BTA_MetadataIdMlxTest               = 0xa720b908,
    BTA_MetadataIdMlxAdcData            = 0xa720b909,
} BTA_MetadataId;


///     @brief BTA_Channel holds a two-dimensional array of data  (A part of BTA_Frame)
typedef struct BTA_Metadata {
    BTA_MetadataId id;              ///< Type of metadata. Needs to be specified outside. The BTA is not aware of its meaning
    void *data;                     ///< The data
    uint32_t dataLen;               ///< The length of data in bytes
} BTA_Metadata;


typedef struct BTA_ChannelFilter {
    uint8_t filterByChannelId;          ///< >0: 'id' is a valid filter for ChannelId
    BTA_ChannelId id;
    uint8_t filterByResolution;         ///< >0: 'xRes' and 'yRes' are valid filters. 1: resolution must match exactly
    uint16_t xRes;
    uint16_t yRes;
    uint8_t filterByDataFormat;         ///< >0: 'dataFormat' is a valid filter
    BTA_DataFormat dataFormat;
    uint8_t filterByLensIndex;          ///< >0: 'lensIndex' is a valid filter
    uint8_t lensIndex;
    uint32_t filterByFlagsMask;         ///< >0: 'flags' is a valid filter, mask is used to determine relevant bits
    uint32_t flags;
    uint8_t filterBySequenceCounter;    ///< >0: 'sequenceCounter' is a valid filter. 1: sequenceCounter must match exactly
    uint8_t sequenceCounter;
} BTA_ChannelFilter;


///     @brief BTA_Channel holds a two-dimensional array of data  (A part of BTA_Frame)
typedef struct BTA_Channel {
    BTA_ChannelId id;                   ///< Type of data in this channel
    uint16_t xRes;                      ///< Number of columns
    uint16_t yRes;                      ///< Number of rows
    BTA_DataFormat dataFormat;          ///< The bytestream in data needs to be casted to this format
    BTA_Unit unit;                      ///< Informative, for easier interpretation of the data
    uint32_t integrationTime;           ///< Integration time at which the frame was captured in [us]
    uint32_t modulationFrequency;       ///< Modulation frequency at which the frame was captured in [Hz]
    uint8_t *data;                      ///< Pixels starting with upper left pixel. For nofBytesPerPixel bigger than 1 the first byte is the lowbyte
    uint32_t dataLen;                   ///< Length of the channel data in bytes (= xRes*yRes*bytesPerPixel)
    BTA_Metadata **metadata;            ///< List of pointers to additional generic data
    uint32_t metadataLen;               ///< The number of BTA_Metadata pointers stored in metadata
	uint8_t lensIndex;                  ///< The index defining the sensor and lens the data originates from
	uint32_t flags;                     ///< More information on the channel content
    uint8_t sequenceCounter;            ///< If multiple sequences were captured, they can be distinguished by the sequence counter
    float gain;                         ///< The magnitude of amplification the sensor produces
} BTA_Channel;


///     @brief BTA_Frame holds all the data gathered from one frame (one or more channels)
typedef struct BTA_Frame {
    uint8_t firmwareVersionMajor;       ///< Firmware version major
    uint8_t firmwareVersionMinor;       ///< Firmware version minor
    uint8_t firmwareVersionNonFunc;     ///< Firmware version non functional
    float mainTemp;                     ///< Main-board/processor temperature sensor in degree Celcius
    float ledTemp;                      ///< Led-board temperature sensor in degree Celcius
    float genericTemp;                  ///< Additional Generic temperature sensor in degree Celcius
    uint32_t frameCounter;              ///< Consecutive numbering of frames
    uint32_t timeStamp;                 ///< Time-stamp at which the frame was captured (in microseconds) (max 1h 11m 34s 967ms 295 us)
    BTA_Channel **channels;             ///< Data containing channelsLen Channel structs
    uint8_t channelsLen;                ///< The number of BTA_Channel pointers stored in channels
    uint8_t sequenceCounter;            ///< DEPRECATED
    BTA_Metadata **metadata;            ///< List of pointers to additional generic data
    uint32_t metadataLen;               ///< The number of BTA_Metadata pointers stored in metadata
    //uint32_t shmOffset;                 ///< in case of shared memory, this is the 'id' that is returned to the camera's shared memory management
    /*TODO uint16_t deviceType;
    BTA_DeviceType interfaceType;
    uint16_t protocolVersion;*/
} BTA_Frame;



#endif
