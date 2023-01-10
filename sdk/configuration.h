#ifndef CONFIGURATIN_H_INCLUDED
#define CONFIGURATIN_H_INCLUDED

#define Argos3dP310                 0x9ba6      /*Ethernet                          */
#define Argos3dP321                 0xb321      /*Ethernet                          */
#define Argos3dP32x                 0xb320      /*Ethernet                          */
#define Argos3dP33x                 0x03fc      /*Ethernet                          */
#define Argos3dP320S                0x3de4      /*Ethernet                          */
#define Argos3dP510Skt              0xb510      /*Ethernet                          */
#define Sentis3dM100                0xa9c1      /*Ethernet                          */
#define Sentis3dP509                0x4859      /*Ethernet      (Hyera IF-MTK)      */
#define Sentis3dP509Irs1020         0xb509      /*Ethernet                          */
#define Sentis3dM520                0x5032      /*Ethernet      (Sentis-ToF - P510) */
#define Sentis3dM530                0x5110      /*Ethernet                          */
#define TimUp19kS3Eth               0x795c      /*Ethernet                          */
#define TimUp19kS3EthP              0xb620      /*Ethernet                          */
#define TimUpIrs1125                0x5a79      /*Ethernet, USB                     */
#define TimUpIrs1125Ffc             0x524d      /*Ethernet, USB                     */
#define Argos3dP25x                 0xb250      /*Ethernet                          */
#define Argos3dP65x                 0xb650      /*Ethernet                          */
#define Mlx75123ValidationPlatform  0x1e3c      /*Ethernet                          */
#define Mlx75023TofEval             0x7502      /*Ethernet                          */
#define Evk7512x                    0x31ee      /*Ethernet                          */
#define Evk75027                    0x31ff      /*Ethernet                          */
#define Evk7512xTofCcBa             0x32ee      /*Ethernet                          */
#define AudiGrabberBoard            0x4762      /*Ethernet                          */
#define MultiTofPlatformMlx         0xbe41      /*Ethernet                          */
#define MhsCamera                   0x22d3      /*Ethernet                          */
#define PuFCamera                   0x5046      /*Ethernet                          */
#define Argos3dP100                 0xa3c4      /*USBP100                           */
#define TimUp19kS3Spartan6          0x13ab      /*USBP100                           */
#define LimTesterV2                 0x4c54      /*UART                              */
#define Epc610TofModule             0x7a3d      /*UART                              */


#ifndef ETH__________________________________________________________________________________
#define BTA_ETH_DEVICE_TYPES_LEN                    26
#define BTA_ETH_DEVICE_TYPES                        Argos3dP310,\
                                                    Argos3dP321,\
                                                    Argos3dP32x,\
                                                    Argos3dP33x,\
                                                    Argos3dP320S,\
                                                    Argos3dP510Skt,\
                                                    Sentis3dM100,\
                                                    Sentis3dP509,\
                                                    Sentis3dP509Irs1020,\
                                                    Sentis3dM520,\
                                                    Sentis3dM530,\
                                                    TimUp19kS3Eth,\
                                                    TimUp19kS3EthP,\
                                                    TimUpIrs1125,\
                                                    TimUpIrs1125Ffc,\
                                                    Mlx75123ValidationPlatform,\
                                                    Mlx75023TofEval,\
                                                    Evk7512x,\
                                                    Evk75027,\
                                                    Evk7512xTofCcBa,\
                                                    AudiGrabberBoard,\
                                                    MultiTofPlatformMlx,\
                                                    MhsCamera,\
                                                    PuFCamera,\
                                                    Argos3dP25x,\
                                                    Argos3dP65x
#endif


#ifndef USB__________________________________________________________________________________
#define BTA_USB_DEVICE_TYPES_LEN                    2
#define BTA_USB_DEVICE_TYPES                        TimUpIrs1125,\
                                                    TimUpIrs1125Ffc
#endif


#ifndef UART_________________________________________________________________________________
#define BTA_UART_DEVICE_TYPES_LEN                   2
#define BTA_UART_DEVICE_TYPES                       LimTesterV2,\
                                                    Epc610TofModule
#endif


#ifndef P100_________________________________________________________________________________
#define BTA_P100_DEVICE_TYPES_LEN                   8
#define BTA_P100_DEVICE_TYPES                       Argos3dP100,\
                                                    Argos3dP100,\
                                                    Argos3dP100,\
                                                    TimUp19kS3Spartan6,\
                                                    TimUp19kS3Spartan6,\
                                                    TimUp19kS3Spartan6,\
                                                    TimUp19kS3Spartan6,\
                                                    TimUp19kS3Spartan6

#define BTA_P100_PON_CODES                          1, 2, 3, 4, 5, 6, 7, 8
#define BTA_PON_ARGOS3D_P100_0                      "150-2001"
#define BTA_PON_ARGOS3D_P100_1                      "160-0001"
#define BTA_PON_TIM_UP_19K_S3_SPARTAN6_0            "150-2201"
#define BTA_PON_TIM_UP_19K_S3_SPARTAN6_1            "170-2201"
#define BTA_PON_TIM_UP_19K_S3_SPARTAN6_2            "160-0001"
#define BTA_PON_TIM_UP_19K_S3_SPARTAN6_3            "909-2222"
#define BTA_P100_PRODUCT_ORDER_NUMBERS              BTA_PON_ARGOS3D_P100_0,\
                                                    BTA_PON_ARGOS3D_P100_1,\
                                                    BTA_PON_ARGOS3D_P100_1,\
                                                    BTA_PON_TIM_UP_19K_S3_SPARTAN6_0,\
                                                    BTA_PON_TIM_UP_19K_S3_SPARTAN6_1,\
                                                    BTA_PON_TIM_UP_19K_S3_SPARTAN6_2,\
                                                    BTA_PON_TIM_UP_19K_S3_SPARTAN6_0,\
                                                    BTA_PON_TIM_UP_19K_S3_SPARTAN6_3
#define BTA_P100_PRODUCT_ORDER_NUMBER_SUFFIXES      "-1",\
                                                    "-1",\
                                                    "-2",\
                                                    "-1",\
                                                    "-2",\
                                                    "-3",\
                                                    "-2",\
                                                    "-1"
#endif


#ifndef Modulation_frequency_sets_________________________________________________________________________

// default frequencies (from Sentis3D - M100)
static const uint32_t modFreqs_01[7] = { 5000000, 7500000, 10000000, 15000000, 20000000, 25000000, 30000000 };

// TimEth
static const uint32_t modFreqs_02[9] = { 5000000, 5630000, 6430000, 7500000, 9000000, 11250000, 15000000, 22500000, 45000000 };

// Melexis 75023 ToF Eval
static const uint32_t modFreqs_03[10] = { 1000000, 5000000, 7500000, 10000000, 15000000, 20000000, 25000000, 30000000, 35000000, 40000000 };

// Melexis 75025 ToF Eval (ToF chip)
static const uint32_t modFreqs_04[8] = { 12000000, 16000000, 20000000, 24000000, 28000000, 32000000, 36000000, 40000000 };

// all IRS1020 based cameras
static const uint32_t modFreqs_05[12] = { 5000000, 7500000, 10010000, 15000000, 20010000, 25020000, 30000000, 40030000, 50030000, 60000000, 70000000, 80050000 }; // , 90000000, 100060000 };

// EPC
static const uint32_t modFreqs_06[16] = { 20000000, 10000000, 6670000, 5000000, 4000000, 3330000, 2860000, 2500000, 2220000, 2000000, 1820000, 1670000, 1540000, 1430000, 1330000, 1250000 };

// TimUpIrs1125
static const uint32_t modFreqs_08[14] = { 2290000, 5000000, 7500000, 10010000, 15000000, 20010000, 25020000, 30000000, 40030000, 50030000, 60000000, 70000000, 78060000, 80050000 };
// TimUpIrs1125Ffc
static const uint32_t modFreqs_10[7] = { 5000000, 7000000, 10000000, 13020000, 14000000, 15000000, 16010000 };

// P+F
static const uint32_t modFreqs_09[15] = { 5000000, 7000000, 10000000, 15000000, 20000000, 25000000, 30000000, 40000000, 50000000, 60000000, 70000000, 80000000, 90000000, 100000000, 110000000 };

// ----- !!! ROUND TO 10kHz !!! -----

// No predefined frequencies
//static const uint32_t *modFreqs_99 = 0;
#endif

#endif
