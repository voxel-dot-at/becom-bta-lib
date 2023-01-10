#ifndef BTA_P100_HELPER_H_INCLUDED
#define BTA_P100_HELPER_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#ifndef BTA_WO_USB
#   ifdef PLAT_WINDOWS
#       include "win_usb.h"
#   else
#       include "usb.h"
#   endif
#else
typedef void usb_dev_handle;
#endif


#define DETAILED_DEBUG                      0
#define DEVEL_DEBUG                         0
#define USB_COMM_ERR_DEBUG                  0



// Regmap------------------------------------------------------------------------------------------
#define P100_REG_SERIAL                 0x01
#define P100_REG_RELEASE                0x02
#define P100_REG_RAW_DATA_SIZE          0x03
#define P100_REG_SEQ_LENGTH             0x0B
#define P100_REG_CAL_MODE               0x0F
#define P100_REG_UPDATE_CAMERA          0x13
#define P100_REG_UPDATE_FLASH           0x14
#define P100_REG_FLASH_MAGIC            0x15
#define P100_REG_TRIGGER_MODE           0x19

#define P100_REG_ADVANCED_FUNCTION      0x36
#define P100_REG_MODULATION_FREQUENCY_0 0x76        // use only when PLL ready
#define P100_REG_MODULATION_FREQUENCY_1 0x77        // use only when PLL ready

#define P100_REG_SEQ0_PLL_SELECT        0x80
#define P100_REG_SEQ0_INTTIME           0x81
#define P100_REG_SEQ0_MOD_FREQ          0x82
#define P100_REG_SEQ0_FRAME_TIME        0x83

#define P100_REG_SEQ0_DIST_OFFSET       0x86

#define P100_REG_5MHz_OFFSET                120        //r/w       neu       direkt beschreibbar
#define P100_REG_7MHz5_OFFSET               121        //r/w       neu       direkt beschreibbar
#define P100_REG_10MHz_OFFSET               122        //r/w       neu       direkt beschreibbar
#define P100_REG_15MHz_OFFSET               123        //r/w       neu       direkt beschreibbar
#define P100_REG_20MHz_OFFSET               124        //r/w       neu       direkt beschreibbar
#define P100_REG_25MHz_OFFSET               125        //r/w       neu       direkt beschreibbar
#define P100_REG_30MHz_OFFSET               126        //r/w       neu       direkt beschreibbar
#define P100_REG_FRAMES_PER_SECOND          127        //r/w       neu       direkt beschreibbar

// CALC MODE BITS
#define P100_REG_CALC_MODE_DOUT0                    5
#define P100_REG_CALC_MODE_DOUT1                    6
#define P100_REG_CALC_MODE_DOUT2                    7
#define P100_REG_CALC_MODE_DOUT3                    8

#define P100_REG_CALC_MODE_CALC_PHASE           9
#define P100_REG_CALC_MODE_CALC_AMP             10
#define P100_REG_CALC_MODE_CALC_INTENSITY       11
#define P100_REG_CALC_MODE_CALC_PLAUS_ACTIVE    12

#define P100_REG_CALC_MODE_DOUT0_DATA0      21
#define P100_REG_CALC_MODE_DOUT0_DATA1      22
#define P100_REG_CALC_MODE_DOUT1_DATA0      23
#define P100_REG_CALC_MODE_DOUT1_DATA1      24
#define P100_REG_CALC_MODE_DOUT2_DATA0      25
#define P100_REG_CALC_MODE_DOUT2_DATA1      26
#define P100_REG_CALC_MODE_DOUT3_DATA0      27
#define P100_REG_CALC_MODE_DOUT3_DATA1      28
// Regmap------------------------------------------------------------------------------------------


#include "cal_dxyz.h"

#include <fastBF/shiftableBF.h>

#define TRUE  1
#define FALSE 0

//----------------------------------------
#define PMD_SPARTAN6_FLAG_WRITE_DATA              0x10
#define PMD_SPARTAN6_FLAG_REQUIRE_ACKNOWLEDGE     0x20
#define PMD_SPARTAN6_FLAG_RECEIVE_FRAME           0x40

#define PMD_SPARTAN6_MAGIC_WORD                   0x504d4431
#define PMD_SPARTAN6_FPGA_TYPE                    "6slx25ftg256"
//----------------------------------------

//------Spartan 6 HEADER------------------

#define HEADER_SIZE  8
#define OFFSET_CMD   4

#define HEADER_OFFSET_FLAGS                 0x04 //Flags (see PMD_SPARTAN6_FLAG_WRITE_DATA etc. for bit positions)
#define HEADER_OFFSET_NR_WORDS              0x06 //NumberOfReceivedWords
#define HEADER_OFFSET_ADDR_OFFSET           0x07 //AddressOffset

//------Spartan 6 ACKNOWLEDGE--------------
#define ACK_ERROR_CODE_OFFSET               0x01 //byte-array index

#define ACK_NO_ERROR                        0x00
#define ACK_ERR_DATA_SIZE_FAILURE           (1<<0)
#define ACK_ERR_FPS_TOO_HIGH                (1<<1)
#define ACK_ERR_FRAME_TIME_TOO_HIGH         (1<<2)
#define ACK_ERR_REG_SET_RECONFIGURED        (1<<3)
#define ACK_ERR_REG_WRITE_PROTECTED         (1<<4)
#define ACK_ERR_FREQUENCY_NOT_SUPPORTED     (1<<5)
#define ACK_ERR_INDEX_OUT_OF_RANGE          (1<<6)
//-----------------------------------------

#define P100_OKAY                               0
#define P100_DEVICE_NOT_FOUND                   -1
#define P100_INVALID                            -2
#define P100_USB_ERROR                          -3
#define P100_INVALID_VALUE                      -4
#define P100_NO_MORE_DEVICES                    -5
#define P100_READ_REG_ERROR                     -6
#define P100_WRITE_REG_ERROR                    -7
#define P100_COULD_NOT_CLOSE                    -8
#define P100_GET_DATA_ERROR                     -9
#define P100_CALC_DATA_ERROR                    -10
#define P100_FILE_ERROR                         -11
#define P100_ACK_ERROR                          -12
#define P100_ACK_ERROR_DATA_SIZE_FAILURE        -13
#define P100_ACK_ERROR_FPS_TOO_HIGH             -14
#define P100_ACK_ERROR_REG_SET_RECONFIGURED     -15
#define P100_ACK_ERROR_REG_WRITE_PROTECTED      -16
#define P100_ACK_ERROR_FREQUENCY_NOT_SUPPORTED  -17
#define P100_ACK_ERROR_INDEX_OUT_OF_RANGE       -18
#define P100_ACK_ERROR_FRAME_TIME_TOO_HIGH      -19

#define P100_INVALID_HANDLE                     -20

#define P100_MEMORY_ERROR                       -99 //memset, memcpy, malloc
#define LIBSUB_TIMEOUT                          -110
//-----------------------------------------

//P100 USB Interface
#define BLT_P100_VID 0x2398
#define BLT_P100_PID 0x1001

#define SPARTAN6_USB_ENDPOINT_FRAME     0x82
#define SPARTAN6_USB_ENDPOINT_WRITE     0x06
#define SPARTAN6_USB_ENDPOINT_READ      0x88
//#define SPARTAN6_USB_CLEAR_TIMEOUT      1000
#define SPARTAN6_USB_FRAME_TIMEOUT      2000
#define SPARTAN6_USB_WRITE_TIMEOUT      2000
#define SPARTAN6_USB_READ_TIMEOUT       2000
#define SPARTAN6_USB_FIRMWARE_TIMEOUT   60000
#define SPARTAN6_USB_END_MARKER         0x00 //??? what for ??? -> UNUSED
#define SPARTAN6_USB_CALIB_MARKER       0x01 //??? what for ???    -> UNUSED

//##########################################
//P100 Source Data Container Header
typedef struct P100_SrcDataHeader {
    uint32_t status;
    uint32_t serial;
    uint32_t release;
    uint32_t framebytes;
    uint32_t rows;
    uint32_t cols;
    uint32_t setROI;
    uint32_t roiColBegin;
    uint32_t roiColEnd;
    uint32_t roiRowBegin;
    uint32_t roiRowEnd;
    uint32_t seqLength;
    uint32_t seqCombine;
    uint32_t outputMode;
    uint32_t acquisitionMode;
    uint32_t calculationMode;
    uint32_t temperatureMain;
    uint32_t temperatureLed;
    uint32_t termperatureCustomer;
} P100_SrcDataHeader;

#define IMG_HEADER_STATUS                   0
#define IMG_HEADER_SERIAL                   1
#define IMG_HEADER_RELEASE                  2
#define IMG_HEADER_FRAMEBYTES               3
#define IMG_HEADER_NOF_ROWS                 4
#define IMG_HEADER_NOF_COLS                 5
#define IMG_HEADER_SETROI                   6
#define IMG_HEADER_ROI_COL_BEG              7
#define IMG_HEADER_ROI_COL_END              8
#define IMG_HEADER_ROI_ROW_BEG              9
#define IMG_HEADER_ROI_ROW_END              10
#define IMG_HEADER_SEQ_LENGTH               11
#define IMG_HEADER_SEQ_COMBINE              12
#define IMG_HEADER_OUTPUTMODE               13
#define IMG_HEADER_ACQISITION_MODE          14
#define IMG_HEADER_CALC_MODE                15
#define IMG_HEADER_TEMPERATURE              16
#define IMG_HEADER_TEMERATURE_ILL           17
#define IMG_HEADER_TEMPERATURE_CUST         18
#define IMG_HEADER_INTEGRATIONTIME          97
#define IMG_HEADER_MODULATIONFREQUENCY      98
#define IMG_HEADER_FRAME_COUNTER            106
#define IMG_HEADER_TIME_STAMP               107

//for IMG_HEADER_OUTPUTMODE_OFFSET
#define IMG_HEADER_DIST_VALUES              0x01
#define IMG_HEADER_AMP_VALUES               0x02
#define IMG_HEADER_INTENS_VALUES            0x03
#define IMG_HEADER_FLAG_VALUES              0x04
#define IMG_HEADER_PHASE_VALUES             0x05

#define SIZEOF_1_CONTAINER                 (P100_WIDTH*P100_HEIGHT*2 + P100_IMG_HEADER_SIZE)
#define SIZEOF_2_CONTAINERS                (SIZEOF_1_CONTAINER * 2)
#define SIZEOF_3_CONTAINERS                (SIZEOF_1_CONTAINER * 3)
#define SIZEOF_4_CONTAINERS                (SIZEOF_1_CONTAINER * 4)
//##########################################
///////////////////


//P100 MISC
#define P100_MAX_INTEGRATION_TIME         5200
#define P100_MAX_MODULATION_FREQUENCY     30000000
#define P100_WIDTH                        160
#define P100_HEIGHT                       120
#define P100_IMG_HEADER_SIZE              512
#define P100_BYTES_PER_PIXEL              2

#define MAX_NR_OF_DEVICES                 10

#define SPEED_OF_LIGHT                    299792458UL

#define BILAT_SIGMA_S                     20
#define BILAT_SIGMA_R                     30
#define BILAT_TOL                         0.01

#define SELECT_FPPN                       1
#define SELECT_FPN                        2

//Flash Commands
#define CMD_FPPN                          0x02
#define CMD_FPN                           0x04
#define CMD_WIGGLING                      0x10
#define CMD_ISMDATA                       0x08
#define CMD_FIRMWARE                      0xA0
#define CMD_SREC_DATA                     0xA1

//-----------------------------------------
//---------------- SDK Flags --------------
#define FLAGS_NONE                       0
#define OPEN_SERIAL_ANY                 (1 << 0)
#define OPEN_SERIAL_SPECIFIED           (1 << 1)

#define DIST_FLOAT_METER                (1 << 0)
#define DIST_FLOAT_PHASE                (1 << 1)
//-----------------------------------------


//---------------- structures -------------
struct device_container_struct{
    usb_dev_handle *m_hnd;
    struct usb_device *m_dev;
    int created;
    pthread_mutex_t *usbMutex;
    float *dx_values;
    float *dy_values;
    float *dz_values;
    unsigned char use_bilateral_filter;
    unsigned char window_bilateral_filter;
};


#ifdef __cplusplus
extern "C"
{
#endif

//---------------- functions --------------

//opens device by serial number or arbitrarily, according to flags OPEN_SERIAL_ANY, OPEN_SERIAL_SPECIFIED
//serialCode and serialNumber are PON and serial number as documented in the support wiki
//returns P100_OKAY if successful
extern int openDevice(int *hndl, int serialCode, int serialNumber, unsigned int flags);

//closes the device associated with the handle "hndl"
//returns P100_OKAY is successful
extern int closeDevice(int hndl);

//param reg_addr: adress of register to be read
//param ret_val: pointer to variable to hold register content
//returns P100_OKAY if successful
extern int getRegister(int hndl, unsigned int reg_addr, unsigned int *ret_val);

//param addr: adress of register to be written
//param val: value to be written to addr
//returns P100_OKAY if successful
//(TODO? could also return acknowledged contents of register)
extern int setRegister(int hndl, const unsigned int addr, const unsigned int val);

//make sure to call "getRegister(..., P100_RAW_DATA_SIZE ,...)" before calling getFrame()
//to get the latest value for the parameter "data_size"
//param fr_buffer: array/buffer of size data_size to receive raw data
//param data_size: size of raw data in bytes [= (img_width*img_height*2 + header_size) * number_of_containers)]
//returns P100_OKAY if successful
extern int readFrame(int hndl, uint8_t *fr_buf, size_t data_size);

//calculate/extract distances from raw data
//container_nr is written by the function calcDistances and contains the zero-based number of the distances container inside the raw data structure
extern int calcDistances(int hndl, uint8_t *raw_data, int raw_data_size, float *dist_data, int dist_data_size, unsigned int flags, unsigned char *container_nr);

//calculate/extract amplitudes from raw data
extern int calcAmplitudes(uint8_t *raw_data, int32_t raw_data_size, float *amp_data, int32_t amp_data_size, uint32_t flags, unsigned char *container_nr);

//calculate/extract flags from raw data
extern int calcFlags(uint8_t *raw_data, int raw_data_size, unsigned int *flag_data, int flag_data_size, unsigned int flags, unsigned char *container_nr);

//calculate/extract 3D-Coordinates from raw data
extern int calc3Dcoordinates(int hndl, uint8_t *raw_data, int raw_data_size, float *coord_data_x, float *coord_data_y, float *coord_data_z,  unsigned int flags, unsigned char *container_nr);

//sets the calibration arrays for xzy calculation to other than default values
extern int set3DCalibArrays(int hndl, float *dx, float *dy, float *dz);

//int calc_phases(uint8_t *raw_data, int raw_data_size, uint16_t *phase_data1, uint16_t *phase_data2, uint16_t *phase_data3, uint16_t *phase_data4, int data_size);
int calc_phases(uint8_t *raw_data, int raw_data_size, uint16_t *phase_data, int data_size, int phase_nr);
int calc_intensities(uint8_t *raw_data, int raw_data_size, uint16_t *intensities, int intensities_size);

//status can be 0=off or >0=on
extern int setBilateralStatus(int hndl, uint8_t status);
extern int setBilateralWindow(int hndl, uint8_t window);

extern int getIntegrationTime(int hndl, unsigned int *value, int sequence);
extern int setIntegrationTime(int hndl, unsigned int value, int sequence);

extern int getModulationFrequency(int hndl, unsigned int *value, int sequence);
extern int setModulationFrequency(int hndl, unsigned int value, int sequence);

//value is in Hz
extern int setFPS(int hndl, float value);
extern int getFPS(int hndl, float *value);

//uses a .bit file with header
//extern int firmwareUpdate_file(int hndl, const char* filename);

//firmware_data has to be loaded from a file by the application
extern int firmwareUpdate(int hndl, unsigned char *firmware_data, unsigned int firmware_data_size);

extern int saveConfig(int hndl);
extern int saveSerial(int hndl, unsigned int serial_NR);

//extern int FppnUpdate(int hndl, const char *filename);
//extern int FpnUpdate(int hndl, const char *filename);
//extern int WigglingUpdate(int hndl, const char *filename);
//extern int Wiggling_Write_Only(int hndl);
//extern int Wiggling_Load_Only(int hndl, const char *filename);
int p100WriteToFlash(int hndl, uint8_t *dataBuffer, uint32_t dataBufferLen, uint8_t flashCmd);

//helper function
extern unsigned int src_data_container_header_pos_ntohs(uint8_t *raw_data, unsigned int container_nr, unsigned int doubleword_offset);

//-----------------------------------------

#ifdef __cplusplus
}
#endif

#endif


