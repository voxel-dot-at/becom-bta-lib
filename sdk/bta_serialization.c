#include "bta_serialization.h"
#include <string.h>
#include <bitconverter.h>


#include "lzma/LzmaLib.h"



BTA_Status BTAcompressSerializedFrameLzmaV22(uint8_t *frameSerialized, uint32_t frameSerializedLen, uint8_t *frameSerializedCompressed, uint32_t *frameSerializedCompressedLen) {
    if (!frameSerialized || !frameSerializedCompressed) {
        return BTA_StatusInvalidParameter;
    }
    uint8_t *dst = frameSerializedCompressed + sizeof(uint16_t) + sizeof(uint32_t) + LZMA_PROPS_SIZE;
    size_t frameSerializedCompressedLenTemp = *frameSerializedCompressedLen;
    uint8_t *props = frameSerializedCompressed + sizeof(uint16_t) + sizeof(uint32_t);
    size_t propsSize = LZMA_PROPS_SIZE;
    int result = LzmaCompress(dst, &frameSerializedCompressedLenTemp, frameSerialized, (size_t)frameSerializedLen, props, &propsSize, 5, 0, -1, -1, -1, -1, -1);
    if (result != SZ_OK) {
        //BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Grabbing: Error, LzmaCompress result is %d", result);
        return BTA_StatusRuntimeError;
    }
    if (propsSize != LZMA_PROPS_SIZE) {
        //BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Grabbing: Error, LzmaCompress used propsSize %d instead of %d", propsSize, LZMA_PROPS_SIZE);
        return BTA_StatusRuntimeError;
    }
    *frameSerializedCompressedLen = (uint32_t)frameSerializedCompressedLenTemp;
    // Props were written on the right spot and are treated as part of compressed data
    *frameSerializedCompressedLen += LZMA_PROPS_SIZE;

    // After 4 byte length of the frame we want a preamble specifying the compression
    uint32_t i = 0;
    BTAbitConverterFromUInt16(BTA_FRAME_SERIALIZED_PREAMBLE_LZMAV22, frameSerializedCompressed, &i);
    // And after that the uncompressed size (needed for decompression allocation)
    BTAbitConverterFromUInt32(frameSerializedLen, frameSerializedCompressed, &i);
    // Also account for compression preamble and compressed size
    *frameSerializedCompressedLen += sizeof(uint16_t) + sizeof(uint32_t);
    return BTA_StatusOk;
}


BTA_Status BTAdeserializeFrameV1(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    *framePtr = 0;
    BTA_Frame *frame = (BTA_Frame *)calloc(1, sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }
    uint32_t index = 3;
    if (*frameSerializedLen - index < sizeof(BTA_Frame)) {
        // not long enough to contain a BTA_Frame
        return BTA_StatusOutOfMemory;
    }
    frame->firmwareVersionNonFunc = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMinor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMajor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->mainTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->ledTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->genericTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->frameCounter = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->timeStamp = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->sequenceCounter = frameSerialized[index++];
    // channels
    frame->channelsLen = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->channels = (BTA_Channel **)calloc(frame->channelsLen, sizeof(BTA_Channel *));
    if (!frame->channels) {
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (*frameSerializedLen - index < sizeof(BTA_Channel)) {
            // not long enough to contain BTA_Channel
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
        if (!frame->channels[chInd]) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->id = (BTA_ChannelId)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->xRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->yRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->dataFormat = (BTA_DataFormat)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->unit = (BTA_Unit)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->integrationTime = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->modulationFrequency = BTAbitConverterToUInt32(frameSerialized, &index);
        if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->xRes * frame->channels[chInd]->yRes * (frame->channels[chInd]->dataFormat & 0xf))) {
            // not long enough to contain channel data
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->dataLen = frame->channels[chInd]->xRes * frame->channels[chInd]->yRes * (uint32_t)(frame->channels[chInd]->dataFormat & 0xf);
        frame->channels[chInd]->data = (uint8_t *)malloc(frame->channels[chInd]->dataLen);
        if (!frame->channels[chInd]->data) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        memcpy(frame->channels[chInd]->data, &frameSerialized[index], frame->channels[chInd]->dataLen);
        index += frame->channels[chInd]->dataLen;
        // TODO add metadata!!
    }
    *framePtr = frame;
    *frameSerializedLen = index;
    return BTA_StatusOk;
}


BTA_Status BTAdeserializeFrameV2(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    *framePtr = 0;
    BTA_Frame *frame = (BTA_Frame *)calloc(1, sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }
    uint32_t index = 3;
    if (*frameSerializedLen - index < sizeof(BTA_Frame)) {
        // not long enough to contain a BTA_Frame
        return BTA_StatusOutOfMemory;
    }
    frame->firmwareVersionNonFunc = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMinor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMajor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->mainTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->ledTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->genericTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->frameCounter = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->timeStamp = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->sequenceCounter = frameSerialized[index++];
    // channels
    frame->channelsLen = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->channels = (BTA_Channel **)calloc(1, frame->channelsLen * sizeof(BTA_Channel *));
    if (!frame->channels) {
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (*frameSerializedLen - index < sizeof(BTA_Channel)) {
            // not long enough to contain BTA_Channel
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
        if (!frame->channels[chInd]) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->dataLen)) {
            // not long enough to contain channel data
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->data = (uint8_t *)malloc(frame->channels[chInd]->dataLen);
        if (!frame->channels[chInd]->data) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        memcpy(frame->channels[chInd]->data, &frameSerialized[index], frame->channels[chInd]->dataLen);
        index += frame->channels[chInd]->dataLen;
    }
    *framePtr = frame;
    *frameSerializedLen = index;
    return BTA_StatusOk;
}


BTA_Status BTAdeserializeFrameV3(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    *framePtr = 0;
    BTA_Frame *frame = (BTA_Frame *)calloc(1, sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }
    uint32_t index = 3;
    if (*frameSerializedLen - index < sizeof(BTA_Frame)) {
        // not long enough to contain a BTA_Frame
        return BTA_StatusOutOfMemory;
    }
    frame->firmwareVersionNonFunc = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMinor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMajor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->mainTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->ledTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->genericTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->frameCounter = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->timeStamp = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->channelsLen = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->channels = (BTA_Channel **)calloc(1, frame->channelsLen * sizeof(BTA_Channel *));
    if (!frame->channels) {
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (*frameSerializedLen - index < 28) {
            // not long enough to contain BTA_Channel
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
        if (!frame->channels[chInd]) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->id = (BTA_ChannelId)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->xRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->yRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->dataFormat = (BTA_DataFormat)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->unit = (BTA_Unit)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->integrationTime = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->modulationFrequency = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->dataLen)) {
            // not long enough to contain channel data
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->data = (uint8_t *)malloc(frame->channels[chInd]->dataLen);
        if (!frame->channels[chInd]->data) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        BTAbitConverterToStream(frameSerialized, &index, frame->channels[chInd]->data, frame->channels[chInd]->dataLen);
        // metadata
        if (*frameSerializedLen - index < 4) {
            // not long enough to contain metadataLen
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->metadataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->metadata = (BTA_Metadata **)calloc(frame->channels[chInd]->metadataLen, sizeof(BTA_Metadata *));
        if (!frame->channels[chInd]->metadata) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        for (uint32_t mdInd = 0; mdInd < frame->channels[chInd]->metadataLen; mdInd++) {
            if (*frameSerializedLen - index < 8) {
                // not long enough to contain metadata
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->metadata[mdInd] = (BTA_Metadata *)calloc(1, sizeof(BTA_Metadata));
            if (!frame->channels[chInd]->metadata[mdInd]) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->metadata[mdInd]->id = (BTA_MetadataId)BTAbitConverterToUInt32(frameSerialized, &index);
            frame->channels[chInd]->metadata[mdInd]->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
            if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->metadata[mdInd]->dataLen)) {
                // not long enough to contain the data
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->metadata[mdInd]->data = malloc(frame->channels[chInd]->metadata[mdInd]->dataLen);
            if (!frame->channels[chInd]->metadata[mdInd]->data) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            BTAbitConverterToStream(frameSerialized, &index, (uint8_t *)frame->channels[chInd]->metadata[mdInd]->data, frame->channels[chInd]->metadata[mdInd]->dataLen);
        }
    }
    frame->sequenceCounter = BTAbitConverterToUInt08(frameSerialized, &index);
    *framePtr = frame;
    *frameSerializedLen = index;
    return BTA_StatusOk;
}


BTA_Status BTAdeserializeFrameV4(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    *framePtr = 0;
    BTA_Frame *frame = (BTA_Frame *)calloc(1, sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }
    uint32_t index = 3;
    if (*frameSerializedLen - index < sizeof(BTA_Frame)) {
        // not long enough to contain a BTA_Frame
        return BTA_StatusOutOfMemory;
    }
    frame->firmwareVersionNonFunc = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMinor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMajor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->mainTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->ledTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->genericTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->frameCounter = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->timeStamp = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->channelsLen = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->channels = (BTA_Channel **)calloc(1, frame->channelsLen * sizeof(BTA_Channel *));
    if (!frame->channels) {
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (*frameSerializedLen - index < 28) {
            // not long enough to contain BTA_Channel
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
        if (!frame->channels[chInd]) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->id = (BTA_ChannelId)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->xRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->yRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->dataFormat = (BTA_DataFormat)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->unit = (BTA_Unit)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->integrationTime = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->modulationFrequency = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->dataLen)) {
            // not long enough to contain channel data
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->data = (uint8_t *)malloc(frame->channels[chInd]->dataLen);
        if (!frame->channels[chInd]->data) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        BTAbitConverterToStream(frameSerialized, &index, frame->channels[chInd]->data, frame->channels[chInd]->dataLen);
        // metadata
        if (*frameSerializedLen - index < 4) {
            // not long enough to contain metadataLen
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->metadataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->metadata = (BTA_Metadata **)calloc(frame->channels[chInd]->metadataLen, sizeof(BTA_Metadata *));
        if (!frame->channels[chInd]->metadata) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        for (uint32_t mdInd = 0; mdInd < frame->channels[chInd]->metadataLen; mdInd++) {
            if (*frameSerializedLen - index < 8) {
                // not long enough to contain metadata
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->metadata[mdInd] = (BTA_Metadata *)calloc(1, sizeof(BTA_Metadata));
            if (!frame->channels[chInd]->metadata[mdInd]) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->metadata[mdInd]->id = (BTA_MetadataId)BTAbitConverterToUInt32(frameSerialized, &index);
            frame->channels[chInd]->metadata[mdInd]->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
            if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->metadata[mdInd]->dataLen)) {
                // not long enough to contain the data
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->metadata[mdInd]->data = malloc(frame->channels[chInd]->metadata[mdInd]->dataLen);
            if (!frame->channels[chInd]->metadata[mdInd]->data) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            BTAbitConverterToStream(frameSerialized, &index, (uint8_t *)frame->channels[chInd]->metadata[mdInd]->data, frame->channels[chInd]->metadata[mdInd]->dataLen);
        }
    }
    frame->sequenceCounter = BTAbitConverterToUInt08(frameSerialized, &index);
    // metadata
    if (*frameSerializedLen - index < 4) {
        // not long enough to contain metadataLen
        // ignore and assume 'no frame metadata'
        frame->metadataLen = 0;
        frame->metadata = 0;
        *framePtr = frame;
        *frameSerializedLen = index;
        return BTA_StatusOk;
    }
    frame->metadataLen = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->metadata = (BTA_Metadata **)calloc(frame->metadataLen, sizeof(BTA_Metadata *));
    if (!frame->metadata) {
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (uint32_t mdInd = 0; mdInd < frame->metadataLen; mdInd++) {
        if (*frameSerializedLen - index < 8) {
            // not long enough to contain metadata
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->metadata[mdInd] = (BTA_Metadata *)calloc(1, sizeof(BTA_Metadata));
        if (!frame->metadata[mdInd]) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->metadata[mdInd]->id = (BTA_MetadataId)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->metadata[mdInd]->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        if (*frameSerializedLen - index < (uint32_t)(frame->metadata[mdInd]->dataLen)) {
            // not long enough to contain the data
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->metadata[mdInd]->data = malloc(frame->metadata[mdInd]->dataLen);
        if (!frame->metadata[mdInd]->data) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        BTAbitConverterToStream(frameSerialized, &index, (uint8_t *)frame->metadata[mdInd]->data, frame->metadata[mdInd]->dataLen);
    }
    *framePtr = frame;
    *frameSerializedLen = index;
    return BTA_StatusOk;
}


BTA_Status BTAdeserializeFrameV5(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    *framePtr = 0;
    BTA_Frame *frame = (BTA_Frame *)calloc(1, sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }
    uint32_t index = 3;
    if (*frameSerializedLen - index < 28) {
        // not long enough to contain a BTA_Frame
        return BTA_StatusOutOfMemory;
    }
    frame->firmwareVersionNonFunc = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMinor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->firmwareVersionMajor = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->mainTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->ledTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->genericTemp = BTAbitConverterToFloat4(frameSerialized, &index);
    frame->frameCounter = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->sequenceCounter = 0;
    frame->timeStamp = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->channelsLen = BTAbitConverterToUInt08(frameSerialized, &index);
    frame->channels = (BTA_Channel **)calloc(1, frame->channelsLen * sizeof(BTA_Channel *));
    if (!frame->channels) {
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (*frameSerializedLen - index < 42) {
            // not long enough to contain BTA_Channel
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        BTA_Channel *channel = frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
        if (!channel) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        frame->channels[chInd]->id = (BTA_ChannelId)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->xRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->yRes = BTAbitConverterToUInt16(frameSerialized, &index);
        frame->channels[chInd]->dataFormat = (BTA_DataFormat)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->unit = (BTA_Unit)BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->integrationTime = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->modulationFrequency = BTAbitConverterToUInt32(frameSerialized, &index);
        frame->channels[chInd]->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        if (*frameSerializedLen - index < channel->dataLen) {
            // not long enough to contain channel data
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        BTAbitConverterToStream(frameSerialized, &index, channel->data, channel->dataLen);
        // channel metadata
        if (*frameSerializedLen - index < 4) {
            // not long enough to contain metadataLen
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        channel->metadataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        channel->metadata = (BTA_Metadata **)calloc(channel->metadataLen, sizeof(BTA_Metadata *));
        if (!channel->metadata) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        for (uint32_t mdInd = 0; mdInd < channel->metadataLen; mdInd++) {
            if (*frameSerializedLen - index < 8) {
                // not long enough to contain metadata
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            BTA_Metadata *metadata = channel->metadata[mdInd] = (BTA_Metadata *)calloc(1, sizeof(BTA_Metadata));
            if (!metadata) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            metadata->id = (BTA_MetadataId)BTAbitConverterToUInt32(frameSerialized, &index);
            metadata->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
            if (*frameSerializedLen - index < metadata->dataLen) {
                // not long enough to contain the data
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            metadata->data = malloc(metadata->dataLen);
            if (!metadata->data) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            BTAbitConverterToStream(frameSerialized, &index, (uint8_t *)metadata->data, metadata->dataLen);
        }
        channel->lensIndex = BTAbitConverterToUInt08(frameSerialized, &index);
        channel->flags = BTAbitConverterToUInt32(frameSerialized, &index);
        channel->sequenceCounter = BTAbitConverterToUInt08(frameSerialized, &index);
        channel->gain = BTAbitConverterToFloat4(frameSerialized, &index);
    }
    // frame metadata
    if (*frameSerializedLen - index < 4) {
        // not long enough to contain metadataLen
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    frame->metadataLen = BTAbitConverterToUInt32(frameSerialized, &index);
    frame->metadata = (BTA_Metadata **)calloc(frame->metadataLen, sizeof(BTA_Metadata *));
    if (!frame->metadata) {
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (uint32_t mdInd = 0; mdInd < frame->metadataLen; mdInd++) {
        if (*frameSerializedLen - index < 8) {
            // not long enough to contain metadata
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        BTA_Metadata *metadata = frame->metadata[mdInd] = (BTA_Metadata *)calloc(1, sizeof(BTA_Metadata));
        if (!metadata) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        metadata->id = (BTA_MetadataId)BTAbitConverterToUInt32(frameSerialized, &index);
        metadata->dataLen = BTAbitConverterToUInt32(frameSerialized, &index);
        if (*frameSerializedLen - index < (uint32_t)(metadata->dataLen)) {
            // not long enough to contain the data
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        metadata->data = malloc(metadata->dataLen);
        if (!metadata->data) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        BTAbitConverterToStream(frameSerialized, &index, (uint8_t *)metadata->data, metadata->dataLen);
    }
    *framePtr = frame;
    *frameSerializedLen = index;
    return BTA_StatusOk;
}
