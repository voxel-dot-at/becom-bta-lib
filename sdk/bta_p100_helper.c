#include "bta_p100_helper.h"
#include "bta_oshelper.h"
#include <pthread_helper.h>
//#include <assert.h>

#ifndef BTA_WO_USB



//----------------------------------------
struct device_container_struct device_container[MAX_NR_OF_DEVICES];
//----------------------------------------

volatile unsigned int api_init = 1;

//---static forward declarations---------
static int p100_open(int vendorId, int productId, int device, int *hndl);
static int p100_clear(int hndl, unsigned char end, int msec);
static int p100_close(int hnd);
#if 0 /* unused? */
static int pxx_control(int hndl, int requesttype, int request, int value, int index, char *bytes, int size, int msec);
#endif
static int p100_read(int hndl, unsigned char end, char *buf, unsigned size, int msec);
static int p100_write(int hndl, unsigned char end, char *buf, unsigned size, int msec);

//opens the next available device
//param hndl: pointer to the handle that will be associated with the device
//returns P100_OKAY is successful
static int open_device_any(int *hndl);
//opens the device with serial number "serial"
//param hndl: pointer to the handle that will be associated with the device
//param serial: last two digits on the P100's label (= decimal value)
//returns P100_OKAY is successful
static int open_device_serial(int *hndl, int serialCode, int serialNumber);

static int set_magic_word(int hndl, unsigned int magic_value);
static int flash_command(int hndl, uint8_t address, uint32_t command);
static int flashOperation(int hndl, uint8_t reg_addr, uint32_t reg_val);

//static int readFileAlign4(const char *filename, unsigned char **data, unsigned int *data_buffer_size, unsigned char switchByteOrder /*1 = yes*/);
//----------------------------------------

static float dx_values_default[P100_WIDTH * P100_HEIGHT] = { CAL_DX };
static float dy_values_default[P100_WIDTH * P100_HEIGHT] = { CAL_DY };
static float dz_values_default[P100_WIDTH * P100_HEIGHT] = { CAL_DZ };

unsigned int src_data_container_header_pos_ntohs(uint8_t *raw_data, unsigned int container_nr, unsigned int doubleword_offset) {
    unsigned int temp = *(((unsigned int *)(raw_data + (P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*container_nr + P100_IMG_HEADER_SIZE*container_nr))) + doubleword_offset);
    unsigned int ret_val = 0;
    ret_val |= (temp & 0xFF000000) >> 24;
    ret_val |= (temp & 0x00FF0000) >> 8;
    ret_val |= (temp & 0x0000FF00) << 8;
    ret_val |= (temp & 0x000000FF) << 24;
    return ret_val;
}
//--------------------------------------------------

int openDevice(int *hndl, int serialCode, int serialNumber, unsigned int flags)
{
    void *memset_res;
    int result = P100_OKAY;

    //should only be called when first device is opened
    if (api_init == 1) {
        api_init = 0;
        memset_res = memset((void *)device_container, 0, sizeof(device_container));
        if (memset_res == NULL) {
            return P100_MEMORY_ERROR;
        }
    }

    if ((flags & OPEN_SERIAL_ANY) || (flags == 0)) {
        result = open_device_any(hndl);
    }
    else {
        if (flags & OPEN_SERIAL_SPECIFIED) {
            result = open_device_serial(hndl, serialCode, serialNumber);
        }
    }

    return result;
}

int open_device_serial(int *hndl, int serialCode, int serialNumber) {

    int handles[MAX_NR_OF_DEVICES];
    int cnt = 0;
    int found = 0;
    unsigned int reg_addr = P100_REG_SERIAL;
    unsigned int my_reg;
    int my_reg_res;
    int t;
    if (!hndl) {
        return P100_INVALID_HANDLE;
    }
    *hndl = -2;
    for (t = 0; t < MAX_NR_OF_DEVICES; t++) {
        handles[t] = -1;
    }

    while (1) {

        int res = p100_open(BLT_P100_VID, BLT_P100_PID, 0, &handles[cnt]);

        if (res != P100_OKAY) {
            #if DETAILED_DEBUG
            println("breaking, CNT is %i", cnt);
            #endif
            break;
        }

        #ifndef PLAT_WINDOWS
        //### DOES NOT WORK ON WINDOWS!!! ###
        //reset device
        res = usb_reset(device_container[handles[cnt]].m_hnd);
        if (res < 0) {
            return P100_USB_ERROR;
        }
        //println("resetting USB device");
        //###################################
        #endif

        res = usb_resetep(device_container[handles[cnt]].m_hnd, SPARTAN6_USB_ENDPOINT_READ);
        res = usb_resetep(device_container[handles[cnt]].m_hnd, SPARTAN6_USB_ENDPOINT_WRITE);
        res = usb_resetep(device_container[handles[cnt]].m_hnd, SPARTAN6_USB_ENDPOINT_FRAME);

        res = p100_clear(handles[cnt], SPARTAN6_USB_ENDPOINT_READ, SPARTAN6_USB_READ_TIMEOUT);
        res = p100_clear(handles[cnt], SPARTAN6_USB_ENDPOINT_FRAME, SPARTAN6_USB_READ_TIMEOUT);

        //get serial
        my_reg_res= getRegister(handles[cnt], reg_addr, &my_reg);
        if (my_reg_res == P100_OKAY) {
            #if DETAILED_DEBUG
            println("### serial found: %x ", my_reg);
            #endif
        }
        else {
            //println("error reading register");
            return P100_READ_REG_ERROR;
        }

        if ((!serialCode || (uint32_t)serialCode == (my_reg >> 20)) && (!serialNumber || serialNumber == (int)(my_reg & 0xfffff))) {
            #if DETAILED_DEBUG
            println("matching p100 found! - serial %i - handle_internal_nr %i - handle_global_nr %i", serialNumber, cnt, handles[cnt]);
            #endif
            *hndl = handles[cnt];
            found = 1;
            break;
        }
        cnt++;
    }

    #if DETAILED_DEBUG
    println("CNT is %i ", cnt);
    #endif

    for (t = 0; t < MAX_NR_OF_DEVICES; t++) {
        if (handles[t] != -1 && handles[t] != *hndl) {
            closeDevice(handles[t]);
        }
    }

    if (found) {
        return P100_OKAY;
    }
    else {
        return P100_DEVICE_NOT_FOUND;
    }
}



////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// REDUNDANT!!!!!
int open_device_any(int *hndl) {
    int res;

    res = p100_open(BLT_P100_VID, BLT_P100_PID, 0, hndl);
    if (res != P100_OKAY) {
        return P100_DEVICE_NOT_FOUND;
    }

    #ifndef PLAT_WINDOWS
    //### DOES NOT WORK ON WINDOWS!!! ###
    res = usb_reset(device_container[*hndl].m_hnd);
    if (res < 0) {
        return P100_USB_ERROR;
    }
    //println("resetting USB device");
    //###################################
    #endif

    res = usb_resetep(device_container[*hndl].m_hnd, SPARTAN6_USB_ENDPOINT_READ);
    res = usb_resetep(device_container[*hndl].m_hnd, SPARTAN6_USB_ENDPOINT_WRITE);
    res = usb_resetep(device_container[*hndl].m_hnd, SPARTAN6_USB_ENDPOINT_FRAME);

    res = p100_clear(*hndl, SPARTAN6_USB_ENDPOINT_READ, 100);// SPARTAN6_USB_READ_TIMEOUT);
    res = p100_clear(*hndl, SPARTAN6_USB_ENDPOINT_FRAME, 100);// SPARTAN6_USB_READ_TIMEOUT);

    return P100_OKAY;
}

//--------------------------------------------------

int closeDevice(int hndl) {

    int res;

    res = p100_close(hndl);
    if (res != P100_OKAY) {
        return P100_COULD_NOT_CLOSE;
    }

    device_container[hndl].m_hnd = NULL;
    device_container[hndl].m_dev = NULL;
    device_container[hndl].created = FALSE;

    return P100_OKAY;
}

//--------------------------------------------------

// @param device skip this number of p100 (connect to the index <device>)
int p100_open(int vendorId, int productId, int device, int *hndl) {

    struct usb_bus *busses;
    struct usb_bus *bus;
    usb_dev_handle *m_hnd = NULL;
    struct usb_device *m_dev = NULL;
    int found = FALSE;
    int deviceNumber;
    struct usb_device *dev;
    int vendor_id;
    int product_id;
    int res;
    int k;
    #if DETAILED_DEBUG
    int i;
    #endif

    usb_init();
    usb_find_busses();
    usb_find_devices();

    busses = usb_get_busses();

    deviceNumber = 0;
    for (bus = busses; bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            vendor_id = dev->descriptor.idVendor;
            product_id = dev->descriptor.idProduct;


            if (vendor_id == vendorId && product_id == productId) {
#               if DETAILED_DEBUG
                println("VID %4x PID %4x", vendor_id, product_id);
#               endif
                if (deviceNumber++ < device) {
#                   if DETAILED_DEBUG
                    println("skip");
#                   endif
                    continue;
                }

                m_hnd = usb_open(dev);
                if (m_hnd == 0) {
#                   if DETAILED_DEBUG
                    println("usb_open error: %p", m_hnd);
#                   endif
                    continue;
                }
                m_dev = dev;

                if (m_dev->descriptor.bNumConfigurations == 0) {
#                   if DETAILED_DEBUG
                    println("m_dev->descriptor.bNumConfigurations == 0");
#                   endif
                    continue;
                }

                res = usb_set_configuration(m_hnd, m_dev->config[0].bConfigurationValue);
                if (res < 0) {
#                   if DETAILED_DEBUG
                    println("usb_set_configuration failed: %d", res);
#                   endif
                    continue;
                }

                #if DETAILED_DEBUG
                println("--- available endpoints ---");
                for (i = 0; i < m_dev->config[0].interface[0].altsetting[0].bNumEndpoints; ++i)
                {
                    println("endpoint address %i ", (m_dev->config[0].interface[0].altsetting[0].endpoint[i].bEndpointAddress));
                }
                println("---------------------------");
                #endif

                res = usb_claim_interface(m_hnd, m_dev->config[0].interface[0].altsetting[0].bInterfaceNumber);
                if (res < 0) {
                    #if DETAILED_DEBUG
                    println("usb_claim_interface failed: %d", res);
                    #endif
                    continue;
                }

                res = usb_set_altinterface(m_hnd, 0);
                if (res < 0) {
                    #if DETAILED_DEBUG
                    println("usb_set_altinterface failed: %d", res);
                    #endif
                    continue;
                }

                found = TRUE;
                break;
            }
        }
        if (found == TRUE) {
            break;
        }
    }

    if (found) {
        // find room for handle
        for (k = 0; k < MAX_NR_OF_DEVICES; k++) {
            if ((device_container[k].created) == FALSE) {
                device_container[k].m_hnd = m_hnd;
                device_container[k].m_dev = m_dev;
                device_container[k].created = TRUE;
                device_container[k].dx_values = dx_values_default;
                device_container[k].dy_values = dy_values_default;
                device_container[k].dz_values = dz_values_default;
                *hndl = k;

                res = BTAinitMutex((void **)&(device_container[*hndl].usbMutex));

                return P100_OKAY;
            }
        }
        // no more room
        if (k == MAX_NR_OF_DEVICES) {
            hndl = NULL;
            return P100_NO_MORE_DEVICES;
        }
    }
    hndl = NULL;
    return P100_DEVICE_NOT_FOUND;
}

//--------------------------------------------------

int p100_close(int hndl) {
    usb_dev_handle *m_hnd;
    struct usb_device *m_dev;
    int created;
    unsigned int i;
    int res;
    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    m_dev = device_container[hndl].m_dev;
    m_hnd = device_container[hndl].m_hnd;
    created = device_container[hndl].created;
    if (created == FALSE) {
        return P100_INVALID_HANDLE;
    }

    res = BTAcloseMutex((device_container[hndl].usbMutex));

    for (i = 0; i < m_dev->config[0].interface[0].altsetting[0].bNumEndpoints; ++i)
    {
        usb_clear_halt(m_hnd, m_dev->config[0].interface[0].altsetting[0].endpoint[i].bEndpointAddress);
    }

    res = usb_close(m_hnd);
    if (res < 0) {
        return P100_USB_ERROR;
    }
    return P100_OKAY;
}


//--------------------------------------------------
int p100_clear(int hndl, unsigned char end, int msec) {

    //---------
    usb_dev_handle *m_hnd = device_container[hndl].m_hnd;
    /* struct usb_device *m_dev = device_container[hndl].m_dev; */
    int created = device_container[hndl].created;
    //--------
    int res;
    char data[512];

    if ((created == FALSE) || (m_hnd == NULL)) {
        return P100_INVALID;
    }
    res = 0;

    while (res >= 0) {
        res = usb_bulk_read(m_hnd, end, data, 512, msec);
        if (res >= 0) {
            //assert(0); //debug
        }
    }

    return P100_OKAY;
}

//--------------------------------------------------
#if 0 /* unused? */
int pxx_control(int hndl, int requesttype, int request, int value, int index, char *bytes, int size, int msec) {

    //---------
    usb_dev_handle *m_hnd = device_container[hndl].m_hnd;
    /* struct usb_device *m_dev = device_container[hndl].m_dev; */
    /* int created = device_container[hndl].created; */
    //--------
    int res;

    res = usb_control_msg(m_hnd, requesttype, request, value, index, bytes, size, msec);

    if (res < 0) {
        return P100_USB_ERROR;
    }
    return P100_OKAY;
}
#endif
//--------------------------------------------------

int p100_read(int hndl, unsigned char end, char *buf, unsigned size, int msec) {

    usb_dev_handle *m_hnd;
    int created;
    int res;

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
#       if USB_COMM_ERR_DEBUG
            println("%s: Invalid handle [%d] [%u]", __func__, hndl, __LINE__);
#       endif
        return P100_INVALID_HANDLE;
    }
    m_hnd = device_container[hndl].m_hnd;
    created = device_container[hndl].created;
    if (created == FALSE) {
#       if USB_COMM_ERR_DEBUG
            println("%s: Invalid handle [%d] [%u]", __func__, created, __LINE__);
#       endif
        return P100_INVALID_HANDLE;
    }

    end |= 0x80; // ensure read bit

    res = usb_bulk_read(m_hnd, end, buf, size, msec);
    if (res < 0) {
#       if USB_COMM_ERR_DEBUG
            println("%s: Error from usb_bulk_read() [%d] [%u]", __func__, res, __LINE__);
#       endif
    }
    if ((unsigned)res != size) {
        //assert(0); //debug
    }

    /*
      if (res < 0) {
      println("%s ", usb_strerror());
      //return P100_USB_ERROR;
      }
    */

    return res;
}

//--------------------------------------------------

int p100_write(int hndl, unsigned char end, char *buf, unsigned size, int msec) {

    usb_dev_handle *m_hnd;
    int created;
    int res;

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
#       if USB_COMM_ERR_DEBUG
            println("%s: Invalid handle [%d] [%u]", __func__, hndl, __LINE__);
#       endif
        return P100_INVALID_HANDLE;
    }
    m_hnd = device_container[hndl].m_hnd;
    created = device_container[hndl].created;
    if (created == FALSE) {
#       if USB_COMM_ERR_DEBUG
            println("%s: Invalid handle [%d] [%u]", __func__, created, __LINE__);
#       endif
        return P100_INVALID_HANDLE;
    }


    end &= 0x7f;

    res = usb_bulk_write(m_hnd, end, buf, size, msec);
    if (res < 0) {
#       if USB_COMM_ERR_DEBUG
            println("%s: Error from usb_bulk_write() [%d] [%u]", __func__, res, __LINE__);
#       endif
    }

    /*
      if (res < 0) {
      println("%s ", usb_strerror());
      return P100_USB_ERROR;
      }
    */
    return res;
}
//--------------------------------------------------

int readFrame(int hndl, uint8_t *buf, size_t size) {
    usb_dev_handle *m_hnd;
    void * memset_res;
    unsigned char spartan6_header[HEADER_SIZE];
    int res;
    char rcv_buffer[4];
    int rcv_num_bytes;
    int created;

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        #if USB_COMM_ERR_DEBUG
            println("%s: Invalid handle [%d] [%u]", __func__, hndl, __LINE__);
        #endif
        return P100_INVALID_HANDLE;
    }
    m_hnd = device_container[hndl].m_hnd;
    created = device_container[hndl].created;
    if (created == FALSE || m_hnd == NULL) {
        #if USB_COMM_ERR_DEBUG
            println("%s: Invalid handle [%d] [%u]", __func__, created, __LINE__);
        #endif
        return P100_INVALID_HANDLE;
    }

    if (size == 0) {
        return P100_OKAY;
    }

    memset_res = memset(spartan6_header, 0, sizeof(spartan6_header));
    if (memset_res == NULL) {
        #if USB_COMM_ERR_DEBUG
            println("%s: Memset error [%p] [%u]", __func__, memset_res, __LINE__);
        #endif
        return P100_MEMORY_ERROR;
    }
    spartan6_header[0] = 8;
    spartan6_header[HEADER_OFFSET_FLAGS] = PMD_SPARTAN6_FLAG_RECEIVE_FRAME;

    #if DETAILED_DEBUG
        println("--- Header ---");
        for (unsigned int debug_i = 0; debug_i < sizeof(spartan6_header); debug_i++) {
        println("%i",spartan6_header[debug_i]);
        }
        println("--------------");
    #endif

    BTAlockMutex((device_container[hndl].usbMutex));
    res = p100_write(hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *) spartan6_header, sizeof(spartan6_header), SPARTAN6_USB_WRITE_TIMEOUT);
    #if DETAILED_DEBUG
        println("p100_write result: %i ", res);
    #endif
    if (res < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        #if USB_COMM_ERR_DEBUG
            println("%s: Error from p100_write() [%d] [%u]", __func__, res, __LINE__);
        #endif
        return P100_USB_ERROR;
    }

    //-----read acknowledge-----
    rcv_num_bytes = p100_read(hndl, SPARTAN6_USB_ENDPOINT_READ, rcv_buffer, 4, SPARTAN6_USB_READ_TIMEOUT);
    if (rcv_num_bytes < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        #if USB_COMM_ERR_DEBUG
            println("%s: Error from p100_read() [%d] [%u]", __func__, rcv_num_bytes, __LINE__);
        #endif
        return P100_USB_ERROR;
    }
    #if DETAILED_DEBUG
        println("p100_read result: %i ", rcv_num_bytes);
        for (int debug_j=0; debug_j < rcv_num_bytes; debug_j++) {
            println("buffer %d: %d ", debug_j, (unsigned char) rcv_buffer[debug_j]);
        }
    #endif

    if (rcv_buffer[ACK_ERROR_CODE_OFFSET] != ACK_NO_ERROR) {
        #if DETAILED_DEBUG
            println("Acknowledge error");
        #endif
        BTAunlockMutex((device_container[hndl].usbMutex));
        #if USB_COMM_ERR_DEBUG
            println("%s: Ack error [%d] [%u]", __func__, rcv_buffer[ACK_ERROR_CODE_OFFSET], __LINE__);
        #endif
        return P100_ACK_ERROR;
    }
    //-------------------------

    // read frame data
    size_t pos = 0;
    do {
        res = p100_read(hndl, SPARTAN6_USB_ENDPOINT_FRAME, (char *)(buf+pos), (unsigned int)(size - pos), SPARTAN6_USB_FRAME_TIMEOUT);
        if (res > 0) {
            pos += res;
        }
    } while (pos < size && res >= 0);
    BTAunlockMutex((device_container[hndl].usbMutex));

    if (size != pos) {
        #if DETAILED_DEBUG
            println("Could not capture whole frame !");
        #endif
        return P100_GET_DATA_ERROR;
    }
    return P100_OKAY;
}

//--------------------------------------------------

int getRegister(int hndl, unsigned int reg_addr, unsigned int *ret_val) {
    usb_dev_handle *m_hnd;
    int created;
    void * memset_res;
    unsigned char spartan6_header[HEADER_SIZE];
    int res;
    unsigned int rcv_buffer_size;
    char *rcv_buffer;
    int rcv_num_bytes;
    int h;
    unsigned int ret_val_temp;
    int ack_error = 0;
    //-----------------

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
#if USB_COMM_ERR_DEBUG
        println("%s: Invalid handle [%d] [%u]", __func__, hndl, __LINE__);
#endif
        return P100_INVALID_HANDLE;
    }
    m_hnd = device_container[hndl].m_hnd;
    created = device_container[hndl].created;
    if ((created == FALSE) || m_hnd == NULL) {
#if USB_COMM_ERR_DEBUG
        println("%s: Invalid handle [%d] [%u]", __func__, created, __LINE__);
#endif
        return P100_INVALID_HANDLE;
    }

    if (reg_addr > 0xFF) {
#if USB_COMM_ERR_DEBUG
        println("%s: Invalid handle [%d] [%u]", __func__, reg_addr, __LINE__);
#endif
        return P100_INVALID_VALUE;
    }

    memset_res = memset(spartan6_header, 0, sizeof(spartan6_header));
    if (memset_res == NULL) {
        //println("memset error!");
#if USB_COMM_ERR_DEBUG
        println("%s: Invalid handle [%p] [%u]", __func__, memset_res, __LINE__);
#endif
        return P100_MEMORY_ERROR;
    }
    spartan6_header[0] = 8; //actual header size
    spartan6_header[HEADER_OFFSET_FLAGS] = PMD_SPARTAN6_FLAG_REQUIRE_ACKNOWLEDGE;
    spartan6_header[HEADER_OFFSET_NR_WORDS] = 1;
    spartan6_header[HEADER_OFFSET_ADDR_OFFSET] = (unsigned char)reg_addr;

#if DETAILED_DEBUG
    println("--- Header ---");
    for (unsigned int debug_i = 0; debug_i < sizeof(spartan6_header); debug_i++) {
        println("%i",spartan6_header[debug_i]);
    }
    println("--------------");
#endif

    BTAlockMutex((device_container[hndl].usbMutex));
    res = p100_write(hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *) spartan6_header, sizeof(spartan6_header), SPARTAN6_USB_WRITE_TIMEOUT);
#if DETAILED_DEBUG
    println("p100_write result: %i ", res);
#endif
    if (res < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
#if USB_COMM_ERR_DEBUG
        println("%s: Error from p100_write() [%d] [%u]", __func__, res, __LINE__);
#endif
        return P100_USB_ERROR;
    }

    rcv_buffer_size = spartan6_header[HEADER_OFFSET_NR_WORDS] * 4;

#if DETAILED_DEBUG
    println("buffer size: %i ", rcv_buffer_size);
#endif

    rcv_buffer = (char *)malloc(rcv_buffer_size);
    if (rcv_buffer == NULL) {
        //println("malloc error! ");
        BTAunlockMutex((device_container[hndl].usbMutex));
#if USB_COMM_ERR_DEBUG
        println("%s: Malloc error [%p] [%u]", __func__, rcv_buffer, __LINE__);
#endif
        return P100_MEMORY_ERROR;
    }

    memset_res = memset(rcv_buffer, 0, rcv_buffer_size);
    if (memset_res == NULL) {
        //println("memset error!");
        BTAunlockMutex((device_container[hndl].usbMutex));
#if USB_COMM_ERR_DEBUG
        println("%s: Memset error [%p] [%u]", __func__, memset_res, __LINE__);
#endif
        return P100_MEMORY_ERROR;
    }

    for (h=0; h<2; h++) {
        rcv_num_bytes = p100_read(hndl, SPARTAN6_USB_ENDPOINT_READ, rcv_buffer, spartan6_header[HEADER_OFFSET_NR_WORDS] * 4, SPARTAN6_USB_READ_TIMEOUT);

        if (rcv_num_bytes < 0) {
            BTAunlockMutex((device_container[hndl].usbMutex));
#if USB_COMM_ERR_DEBUG
            println("%s: Error from p100_read() [%d] [%u]", __func__, rcv_num_bytes, __LINE__);
#endif
            return P100_USB_ERROR;
        }

#if DETAILED_DEBUG
        println("p100_read result: %i ", rcv_num_bytes);
        for (int debug_i=0; debug_i < rcv_num_bytes; debug_i++) {
            println("buffer %d: %d ",debug_i, (unsigned char) rcv_buffer[debug_i]);
        }
#endif

        //--------------------
        //first p100_read yields acknowledge, second = data
        if (h == 0) {
            if (rcv_buffer[ACK_ERROR_CODE_OFFSET] != ACK_NO_ERROR) {
#if DETAILED_DEBUG
                println("Acknowledge error");
#endif

                switch(rcv_buffer[ACK_ERROR_CODE_OFFSET]){
                case ACK_ERR_REG_WRITE_PROTECTED:
                    ack_error = P100_ACK_ERROR_REG_WRITE_PROTECTED;
                    break;

                case ACK_ERR_FRAME_TIME_TOO_HIGH:
                    ack_error = P100_ACK_ERROR_FRAME_TIME_TOO_HIGH;
                    break;

                case ACK_ERR_FPS_TOO_HIGH:
                    ack_error = P100_ACK_ERROR_FPS_TOO_HIGH;
                    break;

                case ACK_ERR_FREQUENCY_NOT_SUPPORTED:
                    ack_error = P100_ACK_ERROR_FREQUENCY_NOT_SUPPORTED;
                    break;

                default:
                    break;
                }
            }
        }
        //--------------------
    }
    BTAunlockMutex((device_container[hndl].usbMutex));

    if(ack_error != 0){
#if USB_COMM_ERR_DEBUG
        println("%s: Ack error [%d] [%u]", __func__, ack_error, __LINE__);
#endif
        return ack_error;
    }

    ret_val_temp =   (unsigned int)(rcv_buffer[rcv_num_bytes-1] & 0xFF);
    ret_val_temp |= ((unsigned int)(rcv_buffer[rcv_num_bytes-2] & 0xFF)) << 8;
    ret_val_temp |= (((unsigned int)(rcv_buffer[rcv_num_bytes-3] & 0xFF)) << 16);
    ret_val_temp |= ((unsigned int)(rcv_buffer[rcv_num_bytes-4] & 0xFF)) << 24;
    free(rcv_buffer);
    rcv_buffer = 0;

    *ret_val = ret_val_temp;

    return P100_OKAY;
}

//--------------------------------------------------

int setRegister(int hndl, const unsigned int addr, const unsigned int val) {

    usb_dev_handle *m_hnd;
    int created;
    unsigned char spartan6_header[HEADER_SIZE];
    void * memset_res;
    char header_new[HEADER_SIZE + 4];
    void *memcpy_res;
    int res;
    unsigned int rcv_buffer_size;
    char *rcv_buffer;
    int rcv_num_bytes;
    int h;
    //unsigned int ret_val_temp;
    int ack_error = 0;

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
      m_hnd = device_container[hndl].m_hnd;
    created = device_container[hndl].created;
    if (created == FALSE || m_hnd == NULL) {
        return P100_INVALID_HANDLE;
    }

    memset_res = memset(spartan6_header, 0, sizeof(spartan6_header));
    if (memset_res == NULL) {
        //println("memset error!");
        return P100_MEMORY_ERROR;
    }
    spartan6_header[0] = HEADER_SIZE + 4;
    spartan6_header[HEADER_OFFSET_FLAGS] = (PMD_SPARTAN6_FLAG_REQUIRE_ACKNOWLEDGE | PMD_SPARTAN6_FLAG_WRITE_DATA);
    spartan6_header[HEADER_OFFSET_NR_WORDS] = 1;
    spartan6_header[HEADER_OFFSET_ADDR_OFFSET] = (unsigned char)addr;

    memcpy_res = memcpy(header_new, spartan6_header, HEADER_SIZE);
    if (memcpy_res == NULL) {
        //println("memcpy error!");
        return P100_MEMORY_ERROR;
    }

    header_new[sizeof(header_new)-4] = (unsigned char)(val >> 24);
    header_new[sizeof(header_new)-3] = (unsigned char)(val >> 16);
    header_new[sizeof(header_new)-2] = (unsigned char)(val >> 8);
    header_new[sizeof(header_new)-1] = (unsigned char)(val);

    #if DETAILED_DEBUG
    println("--- Header ---");
    for (unsigned int debug_i = 0; debug_i < sizeof(header_new); debug_i++) {
        println("%i", header_new[debug_i]);
    }
    println("--------------");
    #endif

    BTAlockMutex((device_container[hndl].usbMutex));

    res = p100_write(hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *) header_new, sizeof(header_new), SPARTAN6_USB_WRITE_TIMEOUT);
    #if DETAILED_DEBUG
    println("p100_write result: %i ", res);
    #endif
    if (res < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_USB_ERROR;
    }

    //-------------------------------------------------------
    rcv_buffer_size = spartan6_header[HEADER_OFFSET_NR_WORDS] * 4;

    #if DETAILED_DEBUG
    println("buffer size: %i ", rcv_buffer_size);
    #endif

    rcv_buffer = (char *)malloc(rcv_buffer_size);
    if (rcv_buffer == NULL) {
        //println("malloc error! ");
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_MEMORY_ERROR;
    }

    memset_res = memset(rcv_buffer, 0, rcv_buffer_size);
    if (memset_res == NULL) {
        //println("memset error!");
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_MEMORY_ERROR;
    }

    for (h = 0; h < 2; h++) {
        rcv_num_bytes = p100_read(hndl, SPARTAN6_USB_ENDPOINT_READ, rcv_buffer, spartan6_header[HEADER_OFFSET_NR_WORDS] * 4, SPARTAN6_USB_READ_TIMEOUT);

        if (rcv_num_bytes < 0) {
            BTAunlockMutex((device_container[hndl].usbMutex));
            return P100_USB_ERROR;
        }

        #if DETAILED_DEBUG
        println("p100_read result: %i ", rcv_num_bytes);

        for (int debug_j=0; debug_j < rcv_num_bytes; debug_j++) {
            println("buffer %d: %d ",debug_j, (unsigned char) rcv_buffer[debug_j]);
        }
        #endif

        //first p100_read yields acknowledge, second = data
        if (h == 0) {
            if (rcv_buffer[ACK_ERROR_CODE_OFFSET] != ACK_NO_ERROR) {
                #if DETAILED_DEBUG
                println("Acknowledge error");
                #endif

                switch(rcv_buffer[ACK_ERROR_CODE_OFFSET]){
                    case ACK_ERR_REG_WRITE_PROTECTED:
                        ack_error = P100_ACK_ERROR_REG_WRITE_PROTECTED;
                    break;

                    case ACK_ERR_FRAME_TIME_TOO_HIGH:
                        ack_error = P100_ACK_ERROR_FRAME_TIME_TOO_HIGH;
                    break;

                    case ACK_ERR_FPS_TOO_HIGH:
                        ack_error = P100_ACK_ERROR_FPS_TOO_HIGH;
                    break;

                    case ACK_ERR_FREQUENCY_NOT_SUPPORTED:
                        ack_error = P100_ACK_ERROR_FREQUENCY_NOT_SUPPORTED;
                    break;

                    default:
                    break;
                }
            }
        }
    }

    BTAunlockMutex((device_container[hndl].usbMutex));

    if(ack_error != 0){
        return ack_error;
    }

    //ret_val_temp = (unsigned int)(rcv_buffer[rcv_num_bytes-1] & 0xFF);
    //ret_val_temp |= ((unsigned int)(rcv_buffer[rcv_num_bytes-2] & 0xFF)) << 8;
    //ret_val_temp |= (((unsigned int)(rcv_buffer[rcv_num_bytes-3] & 0xFF)) << 16);
    //ret_val_temp |= ((unsigned int)(rcv_buffer[rcv_num_bytes-4] & 0xFF)) << 24;
    free(rcv_buffer);
    rcv_buffer = 0;
    //-------------------------------------------------------

    //return ret_val_temp;

    return P100_OKAY;
}


//--------------------------------------------------

int calcDistances(int hndl, uint8_t *raw_data, int raw_data_size, float *dist_data, int dist_data_size, unsigned int flags, unsigned char *container_nr) {
    unsigned int total_nr_of_containers = 0;
    unsigned int found_container = 0;
    int container = 0;
    unsigned int i;
    unsigned int start_index;
    float *dist_data_cpy;
    int pixelInRow;
    int col_cnt;
    unsigned int bilat_window;
    #if MIRROR_IMG_HORI
    int k;
    int j;
    float *dist_data_save;
    #endif

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    if (!device_container[hndl].created) {
        return P100_INVALID_HANDLE;
    }

    if (dist_data_size < P100_WIDTH * P100_HEIGHT * (int)sizeof(float)) {
        #if DETAILED_DEBUG
        println("array for float dist data too small ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    //---------------------------------------
    //determine number of containers and check headers
    if (raw_data_size == SIZEOF_1_CONTAINER) {
        total_nr_of_containers = 1;
    }
    else if (raw_data_size == SIZEOF_2_CONTAINERS) {
        total_nr_of_containers = 2;
    }
    else if (raw_data_size == SIZEOF_3_CONTAINERS) {
        total_nr_of_containers = 3;
    }
    else if (raw_data_size == SIZEOF_4_CONTAINERS) {
        total_nr_of_containers = 4;
    }
    else {
        #if DETAILED_DEBUG
            println("container number not implemented ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    unsigned int g;
    for (g = 0; g < total_nr_of_containers; g++) {
        unsigned int temp = src_data_container_header_pos_ntohs(raw_data, g, IMG_HEADER_OUTPUTMODE);
        if (temp == IMG_HEADER_DIST_VALUES) {
            container = g;
            found_container = 1;
            //println("container %u has distance data ", container);
            if (container_nr) {
                *container_nr = (unsigned char)container;
            }
            break;
        }
    }

    if (!found_container) {
        #if DETAILED_DEBUG
        println("distance data container not found");
        #endif
        return P100_CALC_DATA_ERROR;
    }
    //---------------------------------------
    float modFreq = (float)src_data_container_header_pos_ntohs(raw_data, container, 98);
    // 299792458 / (2 * modFreq * 65535)
    float scale = (0.5f * (float)SPEED_OF_LIGHT / modFreq) / (float)UINT16_MAX;
    uint8_t bilateralEnabled = device_container[hndl].use_bilateral_filter != 0;
    if (!bilateralEnabled) {
        // the scaling from meters to millimeters is done either directly or after bilateral filter because the filter only works with meters
        scale *= 1000.0f;
    }

    pixelInRow = P100_WIDTH;
    col_cnt = 0;
    start_index = P100_IMG_HEADER_SIZE*(container + 1) + P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*container;
    for (i = 0, g = start_index; g < start_index + P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL; g += P100_BYTES_PER_PIXEL) {
        pixelInRow--;
        if (pixelInRow == -1) {
            pixelInRow = P100_WIDTH-1;
            col_cnt++;
        }

        if (flags & DIST_FLOAT_METER) {
            uint32_t index = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixelInRow * P100_BYTES_PER_PIXEL);
            dist_data[i] = scale * ((float)((((uint16_t)raw_data[index + 1]) << 8) | ((uint16_t)raw_data[index])));
        }
        else if (flags & DIST_FLOAT_PHASE) {
            uint32_t index = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixelInRow * P100_BYTES_PER_PIXEL);
            dist_data[i] = (float)((((uint16_t)raw_data[index + 1]) << 8) | ((uint16_t)raw_data[index]));
        }
        else {
            //not applicable/supported
        }
        i++;
    }

    #if MIRROR_IMG_HORI
    dist_data_save = malloc(dist_data_size);
    memcpy((void*)dist_data_save, (void*)dist_data, dist_data_size);
    memset(dist_data, 0, dist_data_size);

    j = (dist_data_size/sizeof(float))-1;
    for(k = 0; k<(dist_data_size/sizeof(float)); k++){
        dist_data[k] = dist_data_save[j--];
    }

    free(dist_data_save);
    dist_data_save = 0;
    #endif

    //-------- Bilateral Filter -----------
    if (bilateralEnabled) {
        #if DETAILED_DEBUG
        println("calcDistances: bilateral filter applied");
        #endif

        dist_data_cpy = (float *)malloc(dist_data_size);
        if (dist_data_cpy == NULL) {
            return P100_MEMORY_ERROR;
        }
        if (memcpy((void *)dist_data_cpy, (void *)dist_data, dist_data_size) == NULL) {
            free(dist_data_cpy);
            dist_data_cpy = 0;
            return P100_MEMORY_ERROR;
        }
        bilat_window = device_container[hndl].window_bilateral_filter;

        shiftableBF(dist_data_cpy, dist_data, P100_HEIGHT, P100_WIDTH, BILAT_SIGMA_S, BILAT_SIGMA_R, bilat_window, (float)BILAT_TOL, 1000.0f);
        free(dist_data_cpy);
        dist_data_cpy = 0;
    }

    return P100_OKAY;
}

int calcAmplitudes(uint8_t *raw_data, int32_t raw_data_size, float *amp_data, int32_t amp_data_size, uint32_t flags, unsigned char *container_nr) {
    int container = 0;
    unsigned int found_container = 0;
    unsigned int total_nr_of_containers = 0;
    unsigned int temp;
    unsigned int i;
    unsigned int g;
    unsigned int start_index;
    int pixel_in_row;
    int col_cnt;
    #if MIRROR_IMG_HORI
    int k;
    int j;
    float *amp_data_save;
    #endif

    if (amp_data_size < P100_WIDTH*P100_HEIGHT*(int)sizeof(float)) {
        #if DETAILED_DEBUG
        println("array for float amplitude data too small ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    //---------------------------------------
    //determine number of containers and check headers
    if (raw_data_size == SIZEOF_1_CONTAINER) {
        total_nr_of_containers = 1;
    }
    else if (raw_data_size == SIZEOF_2_CONTAINERS) {
        total_nr_of_containers = 2;
    }
    else if (raw_data_size == SIZEOF_3_CONTAINERS) {
        total_nr_of_containers = 3;
    }
    else if (raw_data_size == SIZEOF_4_CONTAINERS) {
        total_nr_of_containers = 4;
    }
    else {
        #if DETAILED_DEBUG
            println("container number not implemented ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    for (g = 0; g < total_nr_of_containers; g++) {
        temp = src_data_container_header_pos_ntohs(raw_data, g, IMG_HEADER_OUTPUTMODE);
        if (temp == IMG_HEADER_AMP_VALUES) {
            container = g;
            found_container = 1;
            //println("container %u has amplitude data ", container);
            if (container_nr != NULL) {
                *container_nr = (unsigned char)container;
            }
            break;
        }
    }

    if (!found_container) {
        #if DETAILED_DEBUG
        println("amplitude data container not found");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    //copy to amp_data
    pixel_in_row = P100_WIDTH;
    col_cnt = 0;
    i = 0;
    start_index = P100_IMG_HEADER_SIZE*(container + 1) + (P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*container);
    for (g = start_index; g < start_index + P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL; g += P100_BYTES_PER_PIXEL) {
        pixel_in_row--;
        if (pixel_in_row == -1) {
            pixel_in_row = P100_WIDTH-1;
            col_cnt++;
        }

        uint32_t index = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixel_in_row * P100_BYTES_PER_PIXEL);
        amp_data[i++] = (float)(((uint16_t)(raw_data[index + 1]) << 8) | ((uint16_t)raw_data[index]));
    }

    #if MIRROR_IMG_HORI
    amp_data_save = malloc(amp_data_size);
    memcpy((void*)amp_data_save, (void*)amp_data, amp_data_size);
    memset(amp_data, 0, amp_data_size);
    j = (amp_data_size/sizeof(float))-1;
    for(k = 0; k<(amp_data_size/sizeof(float)); k++){
        amp_data[k] = amp_data_save[j--];
    }
    free(amp_data_save);
    amp_data_save = 0;
    #endif
    return P100_OKAY;
}


int calcFlags(uint8_t *raw_data, int raw_data_size, unsigned int *flag_data, int flag_data_size, unsigned int flags, unsigned char *container_nr) {
    unsigned int container = 0;
    unsigned int found_container = 0;
    unsigned int total_nr_of_containers = 0;
    /* int offset = IMG_HEADER_OUTPUTMODE;//Output Mode */
    unsigned int temp;
    unsigned int i = 0; //should only run from 0 to (P100_WIDTH*P100_HEIGHT-1)
    unsigned int g;
    unsigned int start_index;
    int pixel_in_row;
    int col_cnt;
    unsigned int pixel_index_mirrored1, pixel_index_mirrored2;
    #if MIRROR_IMG_HORI
    int k;
    int j;
    unsigned int *flag_data_save;
    #endif

    if (flag_data_size < (P100_WIDTH*P100_HEIGHT*(int)sizeof(unsigned int)) ) {
        #if DETAILED_DEBUG
        println("array for unsigned int flag data too small ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    //---------------------------------------
    //determine number of containers and check headers
    if (raw_data_size == SIZEOF_1_CONTAINER) {
        total_nr_of_containers = 1;
    }
    else if (raw_data_size == SIZEOF_2_CONTAINERS) {
        total_nr_of_containers = 2;
    }
    else if (raw_data_size == SIZEOF_3_CONTAINERS) {
        total_nr_of_containers = 3;
    }
    else if (raw_data_size == SIZEOF_4_CONTAINERS) {
        total_nr_of_containers = 4;
    }
    else {
        #if DETAILED_DEBUG
            println("container number not implemented ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    for (g = 0; g < total_nr_of_containers; g++) {
        temp = src_data_container_header_pos_ntohs(raw_data, g, IMG_HEADER_OUTPUTMODE);

        if (temp == IMG_HEADER_FLAG_VALUES) {
            container = g;
            found_container = 1;
            //println("container %u has flag data ", container);
            if(container_nr != NULL){
                *container_nr = (unsigned char)container;
        }
            break;
        }
    }

    if (!found_container) {
        #if DETAILED_DEBUG
        println("flag data container not found");
        #endif
        return P100_CALC_DATA_ERROR;
    }
    //---------------------------------------

    //copy to flag_data
    pixel_in_row = P100_WIDTH;
    col_cnt = 0;
    start_index = P100_IMG_HEADER_SIZE*(container+1) + (P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*container);
    for(g=start_index; g<start_index+(P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL); g+=P100_BYTES_PER_PIXEL) {

        pixel_in_row--;
        if(pixel_in_row == -1){
            pixel_in_row = P100_WIDTH-1;
            col_cnt++;
        }

        pixel_index_mirrored1 = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixel_in_row * P100_BYTES_PER_PIXEL) + 1;
        pixel_index_mirrored2 = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixel_in_row * P100_BYTES_PER_PIXEL);
        flag_data[i] = (((unsigned int)(raw_data[pixel_index_mirrored1]) << 8) | ((unsigned int)raw_data[pixel_index_mirrored2]));
        i++;
    }

    #if MIRROR_IMG_HORI
    flag_data_save = malloc(flag_data_size);
    memcpy((void*)flag_data_save, (void*)flag_data, flag_data_size);
    memset(flag_data, 0, flag_data_size);

    j = (flag_data_size/sizeof(unsigned int))-1;
    for(k = 0; k<(flag_data_size/sizeof(unsigned int)); k++){
        flag_data[k] = flag_data_save[j--];
    }
    free(flag_data_save);
    flag_data_save = 0;
    #endif

    return P100_OKAY;
}


int calc_phases(uint8_t *raw_data, int raw_data_size, uint16_t *phase_data, int data_size, int phase_nr) {
    int t;
    int start_index;
    int pixel_in_row;
    int col_cnt;
    unsigned int i = 0;
    if (data_size < (P100_WIDTH*P100_HEIGHT*(int)sizeof(uint16_t)) ) {
        #if DETAILED_DEBUG
        println("array for phase data too small ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    if (((phase_nr == 0) && (raw_data_size < SIZEOF_1_CONTAINER)) ||  //ridicuros
        ((phase_nr == 1) && (raw_data_size < SIZEOF_2_CONTAINERS)) ||
        ((phase_nr == 2) && (raw_data_size < SIZEOF_3_CONTAINERS)) ||
        ((phase_nr == 3) && (raw_data_size < SIZEOF_4_CONTAINERS)) ) {
        return P100_CALC_DATA_ERROR;
    }

    if (src_data_container_header_pos_ntohs(raw_data, phase_nr, IMG_HEADER_OUTPUTMODE) != IMG_HEADER_PHASE_VALUES) {
        return P100_CALC_DATA_ERROR;
    }

    //copy to phase_data
    pixel_in_row = P100_WIDTH;
    col_cnt = 0;
    start_index = P100_IMG_HEADER_SIZE*(phase_nr + 1) + P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*phase_nr;
    i = 0;
    for (t = start_index; (t < start_index + (P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL)) && (i < P100_WIDTH*P100_HEIGHT); t += P100_BYTES_PER_PIXEL) {
        pixel_in_row--;
        if (pixel_in_row == -1) {
            pixel_in_row = P100_WIDTH - 1;
            col_cnt++;
        }
        uint32_t index = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixel_in_row * P100_BYTES_PER_PIXEL);
        phase_data[i] = (((uint16_t)(raw_data[index + 1]) << 8) | ((unsigned int)raw_data[index]));
        i++;
    }

    //not supported
    #if MIRROR_IMG_HORI
    return P100_CALC_DATA_ERROR;
    #endif

    return P100_OKAY;

}

int calc_intensities(uint8_t *raw_data, int raw_data_size, uint16_t *intensities, int intensities_size){

    unsigned int temp;
    unsigned int g;
    unsigned int container;
    int i = 0;
    unsigned int start_index;
    unsigned int pixel_index_mirrored1, pixel_index_mirrored2;
    int pixel_in_row;
    int col_cnt;

    if (intensities_size < (P100_WIDTH*P100_HEIGHT*(int)sizeof(uint16_t))) {
        #if DETAILED_DEBUG
        println("array for intensity data too small ");
        #endif
        return P100_CALC_DATA_ERROR;
    }

    if (raw_data_size < SIZEOF_1_CONTAINER) {
        return P100_CALC_DATA_ERROR;
    }

    temp = src_data_container_header_pos_ntohs(raw_data, 0, IMG_HEADER_OUTPUTMODE);

    if(temp != IMG_HEADER_INTENS_VALUES){
        return P100_CALC_DATA_ERROR;
    }

    pixel_in_row = P100_WIDTH;
    col_cnt = 0;
    container = 0;
    start_index = P100_IMG_HEADER_SIZE*(container+1) + (P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*container);
    for (g=start_index; g<start_index+(P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL); g+=P100_BYTES_PER_PIXEL) {
        pixel_in_row--;
        if (pixel_in_row == -1) {
            pixel_in_row = P100_WIDTH - 1;
            col_cnt++;
        }

        pixel_index_mirrored1 = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixel_in_row * P100_BYTES_PER_PIXEL) + 1;
        pixel_index_mirrored2 = start_index + (col_cnt * P100_WIDTH * P100_BYTES_PER_PIXEL) + (pixel_in_row * P100_BYTES_PER_PIXEL);
        intensities[i] = (((uint16_t)(raw_data[pixel_index_mirrored1]) << 8) | ((uint16_t)raw_data[pixel_index_mirrored2]));
        i++;
    }

    //not supported
    #if MIRROR_IMG_HORI
    return P100_CALC_DATA_ERROR;
    #endif

    return P100_OKAY;
}

int set3DCalibArrays(int hndl, float *dx, float *dy, float *dz) {
    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    if (!device_container[hndl].created) {
        return P100_INVALID_HANDLE;
    }
    device_container[hndl].dx_values = dx;
    device_container[hndl].dy_values = dy;
    device_container[hndl].dz_values = dz;
    /*println("----------- Calib data test output --------------");
    int cnt;
    for(cnt = 0; cnt <= P100_WIDTH*P100_HEIGHT; cnt++){
        println("--- %u ---", cnt);
        println("calib values default x-y-z: %f|%f|%f", dx_values_default[cnt], dy_values_default[cnt], dz_values_default[cnt]);
        println("calib values custom x-y-z:  %f|%f|%f", device_container[hndl].dx_values[cnt], device_container[hndl].dy_values[cnt], device_container[hndl].dz_values[cnt]);
        println("--------------");
    }
    println("-------------------------------------------------");*/
    return P100_OKAY;
}


int setBilateralStatus(int hndl, uint8_t status) {
    int created;
    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    created = device_container[hndl].created;
    if (created == FALSE) {
        return P100_INVALID_HANDLE;
    }
    device_container[hndl].use_bilateral_filter = status;
    return P100_OKAY;
}


int setBilateralWindow(int hndl, uint8_t window) {
    int created;
    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    created = device_container[hndl].created;
    if (created == FALSE) {
        return P100_INVALID_HANDLE;
    }
    device_container[hndl].window_bilateral_filter = window;
    return P100_OKAY;
}


int calc3Dcoordinates(int hndl, uint8_t *raw_data, int raw_data_size, float *coord_data_x, float *coord_data_y, float *coord_data_z,  unsigned int flags, unsigned char *container_nr) {
    int result;
    unsigned int ipos;
    float distances[P100_WIDTH * P100_HEIGHT];
    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    if (!device_container[hndl].created) {
        return P100_INVALID_HANDLE;
    }
    result = calcDistances(hndl, raw_data, raw_data_size, distances, sizeof(distances), DIST_FLOAT_METER, container_nr);
    if (result != P100_OKAY) {
        return result;
    }
    for (ipos = 0; ipos < P100_WIDTH * P100_HEIGHT; ipos++) {
        coord_data_x[ipos] = device_container[hndl].dx_values[ipos] * distances[ipos];
        coord_data_y[ipos] = device_container[hndl].dy_values[ipos] * distances[ipos];
        coord_data_z[ipos] = device_container[hndl].dz_values[ipos] * distances[ipos];
    }
    return P100_OKAY;
}


int getIntegrationTime(int hndl, unsigned int *value, int sequence) {
    int my_reg_res = getRegister(hndl, P100_REG_SEQ0_INTTIME + 10*sequence, value);
    if (my_reg_res != P100_OKAY) {
        return P100_READ_REG_ERROR;
    }
    return P100_OKAY;
}

int setIntegrationTime(int hndl, unsigned int value, int sequence) {
    int my_reg_res = setRegister(hndl, P100_REG_SEQ0_INTTIME + 10*sequence, value);
    if (my_reg_res != P100_OKAY) {
        return P100_WRITE_REG_ERROR;
    }
    return P100_OKAY;
}

int getModulationFrequency(int hndl, unsigned int *value, int sequence) {
    int my_reg_res = getRegister(hndl, P100_REG_SEQ0_MOD_FREQ + 10*sequence, value);
    if (my_reg_res != P100_OKAY) {
        return P100_READ_REG_ERROR;
    }
    return P100_OKAY;
}

int setModulationFrequency(int hndl, unsigned int value, int sequence) {
    int my_reg_res = setRegister(hndl, P100_REG_SEQ0_MOD_FREQ + 10*sequence, value);
    if (my_reg_res != P100_OKAY) {
        return P100_WRITE_REG_ERROR;
    }
    return P100_OKAY;
}


int setFPS(int hndl, float value) {
    unsigned int value_int = (unsigned int)((float)1000000 / (4.0f * value));
    int my_reg_res = setRegister(hndl, P100_REG_SEQ0_FRAME_TIME, value_int);
    return my_reg_res;
}

int getFPS(int hndl, float *frameRate) {
    uint32_t integrationTime;
    uint32_t phaseTime;
    int status = getRegister(hndl, P100_REG_SEQ0_FRAME_TIME, &phaseTime);
    if (status != P100_OKAY) {
        return P100_READ_REG_ERROR;
    }

    if (phaseTime == 0) {
        status = getRegister(hndl, P100_REG_SEQ0_INTTIME, &integrationTime);
        if (status != P100_OKAY) {
            return P100_READ_REG_ERROR;
        }
        //the theoretical frame rate is calculated
        *frameRate = (float)1000000 / (4.0f * integrationTime + 1500);
    }
    else {
        *frameRate = (float)1000000 / (4.0f * (float)phaseTime);
    }

    return P100_OKAY;
}
//-----------------------------------------------------------------------------

int firmwareUpdate(int hndl, unsigned char *firmware_data, unsigned int firmware_data_size) {

    usb_dev_handle *m_hnd;
    int created;
    unsigned char spartan6_header[12];
    unsigned int size;
    void *memset_res;
    char rcv_buffer[4];
    int rcv_num_bytes;
    unsigned char *firmware_data_swendian;
    unsigned int i;
    unsigned int cnt;
    int res;

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    m_hnd = device_container[hndl].m_hnd;
    created = device_container[hndl].created;
    if (created == FALSE || m_hnd == NULL) {
        return P100_INVALID_HANDLE;
    }

    //############################### LOAD FIRMWARE ################################
    memset_res = memset(spartan6_header, 0, sizeof(spartan6_header));
    if (memset_res == NULL) {
        //println("memset error!");
        return P100_MEMORY_ERROR;
    }

    size = firmware_data_size + sizeof(spartan6_header);
    #if DETAILED_DEBUG
    println("size: %u ", size);
    #endif

    //size
    spartan6_header[0] = (unsigned char)(size);
    spartan6_header[1] = (unsigned char)(size >> 8);
    spartan6_header[2] = (unsigned char)(size >> 16);
    spartan6_header[3] = (unsigned char)(size >> 24);

    spartan6_header[HEADER_OFFSET_FLAGS] = PMD_SPARTAN6_FLAG_WRITE_DATA;
    spartan6_header[HEADER_OFFSET_NR_WORDS] = 0;
    spartan6_header[HEADER_OFFSET_ADDR_OFFSET] = P100_REG_UPDATE_CAMERA;

    spartan6_header[HEADER_SIZE + 3] = CMD_FIRMWARE; //see DigiCamConnectionWrapper::writeFirmware()

    #if DETAILED_DEBUG
    println("--- Header ---");
    for (unsigned int debug_i = 0; debug_i < sizeof(spartan6_header); debug_i++) {
        println("%i", spartan6_header[debug_i]);
    }
    println("--------------");
    #endif

    //switch endianness
    firmware_data_swendian = (unsigned char *)malloc(firmware_data_size);
    if (firmware_data_swendian == NULL) {
        return P100_MEMORY_ERROR;
    }
    i = 0;
    for (cnt = 0; cnt < firmware_data_size; cnt += 4) {
        firmware_data_swendian[i] = firmware_data[cnt + 3];
        firmware_data_swendian[i + 1] = firmware_data[cnt + 2];
        firmware_data_swendian[i + 2] = firmware_data[cnt + 1];
        firmware_data_swendian[i + 3] = firmware_data[cnt];
        i += 4;
    }

    BTAlockMutex((device_container[hndl].usbMutex));

    //------------------------------------------------------------------
    res = p100_write(hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *) spartan6_header, sizeof(spartan6_header), SPARTAN6_USB_WRITE_TIMEOUT);
    #if DETAILED_DEBUG
    println("p100_write result: %i ", res);
    #endif
    if (res < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_USB_ERROR;
    }

    res = p100_write(hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *) firmware_data_swendian, firmware_data_size, SPARTAN6_USB_FIRMWARE_TIMEOUT);
    #if DETAILED_DEBUG
    println("p100_write result: %i ", res);
    #endif
    if (res < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_USB_ERROR;
    }

    rcv_num_bytes = p100_read(hndl, SPARTAN6_USB_ENDPOINT_READ, rcv_buffer, 4, SPARTAN6_USB_FIRMWARE_TIMEOUT);

    #if DETAILED_DEBUG
    println("p100_read result: %i ", rcv_num_bytes);
    for (int debug_j=0; debug_j < rcv_num_bytes; debug_j++) {
        println("buffer %d: %d ",debug_j, (unsigned char) rcv_buffer[debug_j]);
    }
    #endif

    if (rcv_num_bytes < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_USB_ERROR;
    }

    if (rcv_buffer[ACK_ERROR_CODE_OFFSET] != ACK_NO_ERROR) {
        #if DETAILED_DEBUG
        println("Acknowledge error");
        #endif
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_ACK_ERROR;
    }
    //------------------------------------------------------------------

    free(firmware_data_swendian);
    firmware_data_swendian = 0;

    //######################## WRITE FIRMWARE ########################

    //set magic word to PMD_SPARTAN6_MAGIC_WORD
    res = set_magic_word(hndl, PMD_SPARTAN6_MAGIC_WORD);
    if (res != P100_OKAY){

        BTAunlockMutex((device_container[hndl].usbMutex));
        return res;
    }
    //Flash Update Command "0xA0"
    //see DigiCamConnectionWrapper::writeFirmwareToFlash()
    res = flash_command(hndl, P100_REG_UPDATE_FLASH, CMD_FIRMWARE);
    if (res != P100_OKAY){
        BTAunlockMutex((device_container[hndl].usbMutex));
        return res;
    }

    //set magic word to 0
    res = set_magic_word(hndl, 0);
    if (res != P100_OKAY){
        BTAunlockMutex((device_container[hndl].usbMutex));
        return res;
    }

    BTAunlockMutex((device_container[hndl].usbMutex));
    return P100_OKAY;
}

//-------------------------------------------------------
//if only do_load = 1 then file is not saved after loading
//if only do_save = 1 then current settings are saved
//if both are one file is loaded and saved to flash
int p100WriteToFlash(int hndl, uint8_t *dataBuffer, uint32_t dataBufferLen, uint8_t flashCmd) {

    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    if (!device_container[hndl].created || !device_container[hndl].m_hnd) {
        return P100_INVALID_HANDLE;
    }


    BTAlockMutex((device_container[hndl].usbMutex));


    // copy data because it must be altered
    uint8_t *dataToFlash;
    uint32_t dataToFlashLen;
    if (flashCmd == CMD_WIGGLING) {
        dataToFlashLen = dataBufferLen * 4;
        dataToFlash = (uint8_t *)malloc(dataToFlashLen);
        if (!dataToFlash) {
            BTAunlockMutex((device_container[hndl].usbMutex));
            return P100_MEMORY_ERROR;
        }
        uint32_t i;
        for (i = 0; i < dataBufferLen; i++) {
            dataToFlash[i * 4 + 0] = dataBuffer[i];
            dataToFlash[i * 4 + 1] = 0;
            dataToFlash[i * 4 + 2] = (uint8_t)i;
            dataToFlash[i * 4 + 3] = (uint8_t)(i >> 8);
        }
    }
    else if (flashCmd == CMD_FPN || flashCmd == CMD_FPPN) {
        dataToFlashLen = dataBufferLen;
        dataToFlash = (uint8_t *)malloc(dataToFlashLen);
        if (!dataToFlash) {
            BTAunlockMutex((device_container[hndl].usbMutex));
            return P100_MEMORY_ERROR;
        }
        uint32_t i;
        // switch byte order
        for (i = 0; i < dataBufferLen; i += 4) {
            dataToFlash[i + 0] = dataBuffer[i + 3];
            dataToFlash[i + 1] = dataBuffer[i + 2];
            dataToFlash[i + 2] = dataBuffer[i + 1];
            dataToFlash[i + 3] = dataBuffer[i + 0];
        }
    }
    else {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return BTA_StatusInvalidParameter;
    }

    //################### writexxxNData ###################################
    unsigned char spartan6Header[12];
    memset(spartan6Header, 0, sizeof(spartan6Header));
    spartan6Header[0] = (unsigned char)(dataToFlashLen + sizeof(spartan6Header));
    spartan6Header[1] = (unsigned char)((dataToFlashLen + sizeof(spartan6Header)) >> 8);
    spartan6Header[2] = (unsigned char)((dataToFlashLen + sizeof(spartan6Header)) >> 16);
    spartan6Header[3] = (unsigned char)((dataToFlashLen + sizeof(spartan6Header)) >> 24);
    spartan6Header[HEADER_OFFSET_FLAGS] = PMD_SPARTAN6_FLAG_WRITE_DATA;
    spartan6Header[HEADER_OFFSET_NR_WORDS] = 0;
    spartan6Header[HEADER_OFFSET_ADDR_OFFSET] = P100_REG_UPDATE_CAMERA;
    spartan6Header[HEADER_SIZE + 3] = flashCmd; //see DigiCamConnectionWrapper

    //------------------------------------------------------------------
    int result = p100_write(hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *)spartan6Header, sizeof(spartan6Header), SPARTAN6_USB_WRITE_TIMEOUT);
    #if DETAILED_DEBUG
        println("p100_write result: %i ", result);
    #endif
    if (result < 0) {
        free(dataToFlash);
        dataToFlash = 0;
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_USB_ERROR;
    }

    result = p100_write(hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *)dataToFlash, dataToFlashLen, SPARTAN6_USB_WRITE_TIMEOUT);
    free(dataToFlash);
    dataToFlash = 0;
    #if DETAILED_DEBUG
        println("p100_write result: %i ", result);
    #endif
    if (result < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_USB_ERROR;
    }

    char rcvBuffer[4];
    int bytesReceivedCount = p100_read(hndl, SPARTAN6_USB_ENDPOINT_READ, rcvBuffer, 4, SPARTAN6_USB_READ_TIMEOUT);
    #if DETAILED_DEBUG
        println("p100_read result: %i ", bytesReceivedCount);
        for (int debug_j = 0; debug_j < bytesReceivedCount; debug_j++) {
            println("buffer %d: %d ", debug_j, (unsigned char)rcvBuffer[debug_j]);
        }
    #endif

    if (bytesReceivedCount < 0) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_USB_ERROR;
    }

    if (rcvBuffer[ACK_ERROR_CODE_OFFSET] != ACK_NO_ERROR) {
        #if DETAILED_DEBUG
            println("Acknowledge error");
        #endif
        BTAunlockMutex((device_container[hndl].usbMutex));
        return P100_ACK_ERROR;
    }


    //################### writexxxDatatoFlash ############################
    //set magic word to PMD_SPARTAN6_MAGIC_WORD
    result = set_magic_word(hndl, PMD_SPARTAN6_MAGIC_WORD);
    if (result != P100_OKAY) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return result;
    }
    result = flash_command(hndl, P100_REG_UPDATE_FLASH, flashCmd);
    if (result != P100_OKAY) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return result;
    }
    //set magic word to 0
    result = set_magic_word(hndl, 0);
    if (result != P100_OKAY) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return result;
    }

    BTAunlockMutex((device_container[hndl].usbMutex));
    return P100_OKAY;
}


/*
int ISMData_Update(int hndl, const char *filename){
    return file_load_and_writetoflash(hndl, filename, CMD_ISMDATA);
}

int SRECData_Update(int hndl, const char *filename){
    return file_load_and_writetoflash(hndl, filename, CMD_SREC_DATA);
}
*/

//do not forget to free(data) in calling function (but don't allocate it!)
//
//PMD's function setDataValueUInt() (in Spartan6Command.cpp) switches the byte order
//-> it uses pmd::flipByteOrder4()
/*
inline void flipByteOrder4 (unsigned *val)
{
    unsigned temp = *val;
    unsigned char *ptrOut = (unsigned char *) val;
    unsigned char *ptrIn = (unsigned char *) &temp;
    ptrOut[0] = ptrIn[3];
    ptrOut[1] = ptrIn[2];
    ptrOut[2] = ptrIn[1];
    ptrOut[3] = ptrIn[0];
}
*/
//static int readFileAlign4(const char *filename, unsigned char **data, unsigned int *data_buffer_size) {
//
//    unsigned int size = 0;
//    unsigned int size_resized = 0;
//    FILE *f = NULL;
//    *data = NULL;
//
//    f = fopen(filename, "rb");
//    if (!f) {
//        return P100_INVALID_VALUE;
//    }
//
//    fseek(f, 0, SEEK_END);
//    size = ftell (f);
//    fseek(f, 0, SEEK_SET);
//
//    //resize data to 4 Byte alignment
//    switch (size % 4) {
//    case 0:
//        size_resized = size;
//        break;
//    case 1:
//        size_resized = size + 3;
//        break;
//    case 2:
//        size_resized = size + 2;
//        break;
//    case 3:
//        size_resized = size + 1;
//        break;
//    }
//
//    *data = (unsigned char *)calloc(size_resized, 1);
//    if (!(*data)) {
//        fclose (f);
//        return P100_MEMORY_ERROR;
//    }
//
//    if (fread (*data, 1, size, f) != size) {
//        fclose (f);
//        return P100_FILE_ERROR;
//    }
//    *data_buffer_size = size_resized;
//    fclose (f);
//    return P100_OKAY;
//}


//-------------------------------------------------------


int saveSerial(int hndl, unsigned int serial_NR) {
    return flashOperation(hndl, P100_REG_SERIAL, serial_NR);
}
//-------------------------------------------------------

int saveConfig(int hndl) {
    return flashOperation(hndl, P100_REG_UPDATE_FLASH, 1);
}


static int flashOperation(int hndl, uint8_t reg_addr, uint32_t reg_val) {
    int res;
    if (hndl < 0 || hndl >= MAX_NR_OF_DEVICES) {
        return P100_INVALID_HANDLE;
    }
    if (!device_container[hndl].created || !device_container[hndl].m_hnd) {
        return P100_INVALID_HANDLE;
    }

    //set magic word to PMD_SPARTAN6_MAGIC_WORD
    BTAlockMutex((device_container[hndl].usbMutex));
    res = set_magic_word(hndl, PMD_SPARTAN6_MAGIC_WORD);
    if (res != P100_OKAY) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return res;
    }
    res = flash_command(hndl, reg_addr, reg_val);
    if (res != P100_OKAY) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return res;
    }
    //set magic word to 0
    res = set_magic_word(hndl, 0);
    if (res != P100_OKAY) {
        BTAunlockMutex((device_container[hndl].usbMutex));
        return res;
    }

    BTAunlockMutex((device_container[hndl].usbMutex));
    return P100_OKAY;
}

//-------------------------------------------------------------------
static int set_magic_word(int hndl, unsigned int magic_value){
    return flash_command(hndl, P100_REG_FLASH_MAGIC, magic_value);
}
//-------------------------------------------------------------------

//does not unlock mutexes!
//-> has to be handled by higher layer
static int flash_command(int hndl, uint8_t address, uint32_t command){

    unsigned char spartan6_header[12];
    void *memset_res;
    char rcv_buffer[4];
    int res;
    int rcv_num_bytes;

    //send: 0x0C,0,0,0,0x10,0x0,0x0,address,0,0,0,command
    //receive: 0x10,0,0,address

    memset_res = memset(spartan6_header, 0, sizeof(spartan6_header));
    if (memset_res == NULL) {
        return P100_MEMORY_ERROR;
    }
    spartan6_header[0] = 12;
    spartan6_header[HEADER_OFFSET_FLAGS] = PMD_SPARTAN6_FLAG_WRITE_DATA;
    spartan6_header[HEADER_OFFSET_NR_WORDS] = 0;
    spartan6_header[HEADER_OFFSET_ADDR_OFFSET] = address;
    spartan6_header[11] = (unsigned char)(command);
    spartan6_header[10] = (unsigned char)(command >> 8);
    spartan6_header[9] = (unsigned char)(command >> 16);
    spartan6_header[8] = (unsigned char)(command >> 24);

    #if DETAILED_DEBUG
    println("--- Header ---");
    for (unsigned int debug_i = 0; debug_i < sizeof(spartan6_header); debug_i++) {
        println("%i", spartan6_header[debug_i]);
    }
    println("--------------");
    #endif

    res = p100_write (hndl, SPARTAN6_USB_ENDPOINT_WRITE, (char *) spartan6_header, sizeof(spartan6_header), SPARTAN6_USB_WRITE_TIMEOUT);
    #if DETAILED_DEBUG
    println("p100_write result: %i ", res);
    #endif
    if (res < 0) {
        return P100_USB_ERROR;
    }

    rcv_num_bytes = p100_read (hndl, SPARTAN6_USB_ENDPOINT_READ, rcv_buffer, 4, SPARTAN6_USB_FIRMWARE_TIMEOUT);
    #if DETAILED_DEBUG
    println("p100_read result: %i ", rcv_num_bytes);
    for (int debug_j=0; debug_j < rcv_num_bytes; debug_j++) {
        println("buffer %d: %d ",debug_j, (unsigned char) rcv_buffer[debug_j]);
    }
    #endif

    if (rcv_num_bytes < 0) {
        return P100_USB_ERROR;
    }

    if (rcv_buffer[ACK_ERROR_CODE_OFFSET] != ACK_NO_ERROR) {
        #if DETAILED_DEBUG
        println("Acknowledge error");
        #endif
        return P100_ACK_ERROR;
    }

    return P100_OKAY;
}




#else
unsigned int src_data_container_header_pos_ntohs(uint8_t *raw_data, unsigned int container_nr, unsigned int doubleword_offset) {
    return 0;
}
//--------------------------------------------------

int openDevice(int *hndl, int serialCode, int serialNumber, unsigned int flags) {
    return P100_INVALID;
}

int open_device_serial(int *hndl, int serialCode, int serialNumber) {
    return P100_INVALID;
}

int open_device_any(int *hndl) {
    return P100_INVALID;
}

int closeDevice(int hndl) {
    return P100_INVALID;
}

int p100_open(int vendorId, int productId, int device, int *hndl) {
    return P100_INVALID;
}

int p100_close(int hndl) {
    return P100_INVALID;
}

int p100_clear(int hndl, unsigned char end, int msec) {
    return P100_INVALID;
}

int p100_read(int hndl, unsigned char end, char *buf, unsigned size, int msec) {
    return P100_INVALID;
}

int p100_write(int hndl, unsigned char end, char *buf, unsigned size, int msec) {
    return P100_INVALID;
}

int readFrame(int hndl, uint8_t *buf, size_t size) {
    return P100_INVALID;
}

int getRegister(int hndl, unsigned int reg_addr, unsigned int *ret_val) {
    return P100_INVALID;
}

//--------------------------------------------------

int setRegister(int hndl, const unsigned int addr, const unsigned int val) {
    return P100_INVALID;
}

int calcDistances(int hndl, uint8_t *raw_data, int raw_data_size, float *dist_data, int dist_data_size, unsigned int flags, unsigned char *container_nr) {
    return P100_INVALID;
}

int calcAmplitudes(uint8_t *raw_data, int32_t raw_data_size, float *amp_data, int32_t amp_data_size, uint32_t flags, unsigned char *container_nr) {
    return P100_INVALID;
}

int calcFlags(uint8_t *raw_data, int raw_data_size, unsigned int *flag_data, int flag_data_size, unsigned int flags, unsigned char *container_nr) {
    return P100_INVALID;
}

int calc_phases(uint8_t *raw_data, int raw_data_size, uint16_t *phase_data, int data_size, int phase_nr) {
    return P100_INVALID;
}

int calc_intensities(uint8_t *raw_data, int raw_data_size, uint16_t *intensities, int intensities_size) {
    return P100_INVALID;
}

int set3DCalibArrays(int hndl, float *dx, float *dy, float *dz) {
    return P100_INVALID;
}

int setBilateralStatus(int hndl, uint8_t status) {
    return P100_INVALID;
}


int setBilateralWindow(int hndl, uint8_t window) {
    return P100_INVALID;
}

int calc3Dcoordinates(int hndl, uint8_t *raw_data, int raw_data_size, float *coord_data_x, float *coord_data_y, float *coord_data_z, unsigned int flags, unsigned char *container_nr) {
    return P100_INVALID;
}

int getIntegrationTime(int hndl, unsigned int *value, int sequence) {
    return P100_INVALID;
}

int setIntegrationTime(int hndl, unsigned int value, int sequence) {
    return P100_INVALID;
}

int getModulationFrequency(int hndl, unsigned int *value, int sequence) {
    return P100_INVALID;
}

int setModulationFrequency(int hndl, unsigned int value, int sequence) {
    return P100_INVALID;
}

int setFPS(int hndl, float value) {
    return P100_INVALID;
}

int getFPS(int hndl, float *frameRate) {
    return P100_INVALID;
}

int firmwareUpdate(int hndl, unsigned char *firmware_data, unsigned int firmware_data_size) {
    return P100_INVALID;
}

int p100WriteToFlash(int hndl, uint8_t *dataBuffer, uint32_t dataBufferLen, uint8_t flashCmd) {
    return P100_INVALID;
}

int saveSerial(int hndl, unsigned int serial_NR) {
    return P100_INVALID;
}

int saveConfig(int hndl) {
    return P100_INVALID;
}
#endif