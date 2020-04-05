#ifndef CONFIGURATIN_H_INCLUDED
#define CONFIGURATIN_H_INCLUDED




#ifndef ETH__________________________________________________________________________________
#define BTA_ETH_DEVICE_TYPES_LEN                    22
#define BTA_ETH_DEVICE_TYPES                        BTA_DeviceTypeArgos3dP33x,\
                                                    BTA_DeviceTypeMlx75123ValidationPlatform,\
                                                    BTA_DeviceTypeEvk7512x,\
                                                    BTA_DeviceTypeEvk75027,\
                                                    BTA_DeviceTypeEvk7512xTofCcBa,\
                                                    BTA_DeviceTypeP320S,\
                                                    BTA_DeviceTypeGrabberBoard,\
                                                    BTA_DeviceTypeSentis3dP509,\
                                                    BTA_DeviceTypeSentis3dM520,\
                                                    BTA_DeviceTypeSentis3dM530,\
                                                    BTA_DeviceTypeTimUpIrs1125,\
                                                    BTA_DeviceTypeMlx75023TofEval,\
                                                    BTA_DeviceTypeTimUp19kS3Eth,\
                                                    BTA_DeviceTypeArgos3dP310,\
                                                    BTA_DeviceTypeSentis3dM100,\
                                                    BTA_DeviceTypeArgos3dP32x,\
                                                    BTA_DeviceTypeArgos3dP321,\
                                                    BTA_DeviceTypeSentis3dP509Irs1020,\
                                                    BTA_DeviceTypeArgos3dP510SKT,\
                                                    BTA_DeviceTypeTimUp19kS3EthP,\
                                                    BTA_DeviceTypeMultiTofPlatformMlx,\
                                                    BTA_DeviceTypeMhsCamera
#endif


#ifndef USB__________________________________________________________________________________
#define BTA_USB_DEVICE_TYPES_LEN                    1
#define BTA_USB_DEVICE_TYPES                        BTA_DeviceTypeTimUpIrs1125
#endif


#ifndef UART_________________________________________________________________________________
#define BTA_UART_DEVICE_TYPES_LEN                   2
#define BTA_UART_DEVICE_TYPES                       BTA_DeviceTypeLimTesterV2,\
                                                    BTA_DeviceTypeEPC610TofModule
#endif


#ifndef Modulation_frequency_sets_________________________________________________________________________

// default frequencies (from Sentis3D - M100)
static const uint32_t modFreqs_1[7] = { 5000000, 7500000, 10000000, 15000000, 20000000, 25000000, 30000000 };

// TimEth
static const uint32_t modFreqs_2[9] = { 5000000, 5630000, 6430000, 7500000, 9000000, 11250000, 15000000, 22500000, 45000000 };

// Melexis 75023 ToF Eval
static const uint32_t modFreqs_3[10] = { 1000000, 5000000, 7500000, 10000000, 15000000, 20000000, 25000000, 30000000, 35000000, 40000000 };

// Melexis 75025 ToF Eval (ToF chip)
static const uint32_t modFreqs_4[8] = { 12000000, 16000000, 20000000, 24000000, 28000000, 32000000, 36000000, 40000000 };

// all IRS1020 based cameras
static const uint32_t modFreqs_5[12] = { 5004000, 7500000, 10007000, 15000000, 20013000, 25016000, 30000000, 40026000, 50032000, 60000000, 70000000, 80051000 }; // , 90000000, 100063000 };

// EPC
static const uint32_t modFreqs_6[16] = { 20000000, 10000000, 6670000, 5000000, 4000000, 3330000, 2860000, 2500000, 2220000, 2000000, 1820000, 1670000, 1540000, 1430000, 1330000, 1250000 };

// TimUpIrs1125
static const uint32_t modFreqs_8[14] = { 2290000, 5004000, 7500000, 10007000, 15000000, 20013000, 25016000, 30000000, 40026000, 50032000, 60000000, 70000000, 78062600, 80051000 };

// No predefined frequencies
//static const uint32_t *modFreqs_99 = 0;
#endif

#endif
