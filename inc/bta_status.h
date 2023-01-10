///  @file bta_status.h
///
///  @brief This header file contains the status ID enum used as return value for most functions
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

#ifndef BTA_STATUS_H_INCLUDED
#define BTA_STATUS_H_INCLUDED

#define BTA_STATUS_H_VER_MAJ 3
#define BTA_STATUS_H_VER_MIN 3
#define BTA_STATUS_H_VER_NON_FUNC 6



#define VERBOSE_CRITICAL       1    ///< Verbosity to set in order to receive only critical events
#define VERBOSE_ERROR          2    ///< Verbosity to set in order to receive critical and error events
#define VERBOSE_WARNING        4    ///< Verbosity to set in order to receive critical, error and warning events
#define VERBOSE_INFO           6    ///< Verbosity to set in order to receive critical, error, warning and informative events
#define VERBOSE_WRITE_OP       8    ///< Verbosity to set in order to receive critical, error, warning, informative and write operation events
#define VERBOSE_READ_OP        9    ///< Verbosity to set in order to receive critical, error, warning, informative, write- and read operation events
#define VERBOSE_DEBUG         42    ///< Verbosity to set in order to receive critical, error, warning, informative, write- and read operation events



///   @brief Error code for error handling
typedef enum BTA_Status {
    BTA_StatusOk                = 0,        ///< Everything went ok

    // Errors
    BTA_StatusInvalidParameter  = -32768,   ///< At least one parameter passed is invalid or out of valid range
                                            ///< The register address provided is outside the valid range
                                            ///< The combination of parameters is contradictory or incomplete
                                            ///< The provided frame does not contain the channel(s) expected
    BTA_StatusIllegalOperation  = -32767,   ///< The data requested by the user cannot be read / written because it does not exist or is not accessible in the current configuration / state
                                            ///< The modulation frequency to be set or currently configured is not supported
                                            ///< BTAclose was already called
    BTA_StatusTimeOut           = -32766,   ///< Within the waiting period a necessary condition was not met, so the operation had to be aborted
                                            ///< After trying repeatedly the operation did not succeed
    BTA_StatusDeviceUnreachable = -32765,   ///< The connection to the device could not be established
                                            ///< An error occurred during communication
                                            ///< The device with the specified attributes could not be found
    BTA_StatusNotConnected      = -32764,   ///< The operation cannot be executed because the connection is down
    BTA_StatusInvalidVersion    = -32763,   ///<
    BTA_StatusRuntimeError      = -32762,   ///< A system resource (mutex, semReadPool, thread, file) could not be created / initialized / read / written
                                            ///< The ToF device did not react as expected
    BTA_StatusOutOfMemory       = -32761,   ///< A malloc, realloc or calloc failed to reserve the needed memory
                                            ///< The buffer provided by the caller is not large enough
                                            ///< The end of the file was reached
    BTA_StatusNotSupported      = -32760,   ///< The function is not supported by this device/firmware
                                            ///<
    BTA_StatusCrcError          = -32759,   ///< The cyclic redundancy check revealed that the data in question must be corrupt
    BTA_StatusUnknown           = -32758,
    BTA_StatusInvalidData       = -32757,   ///< The data to be processed is inconsistent / insufficient / invalid

    // These stati are used only in infoEvent callbacks, it is merely a placeholder rather than a state
    BTA_StatusInformation       = 1,        ///< The infoEvent message contains the actual information
    BTA_StatusWarning           = 2,        ///< The infoEvent message describes the cause of the warning
    BTA_StatusAlive             = 3,        ///< This could be used for alive messages...?!

    BTA_StatusConfigParamError  = 5,        ///< If BTAopen fails due to a bad parameter in BTA_Config, this status code is used in the infoEvent callback
} BTA_Status;


/// @brief  Deprecated BTA_EventId now only represents a BTA_Status
typedef BTA_Status BTA_EventId;

#endif
