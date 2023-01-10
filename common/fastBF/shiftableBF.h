#ifndef SHIFTABLEBF_H
#define SHIFTABLEBF_H

int shiftableBF(float inImg[], float outImg[], const int yRes, const int xRes, int sigmaS, int sigmaR, int windowSize, float tol, float outFactor);
int shiftableBFU16(float inImg[], uint16_t outImg[], const int yRes, const int xRes, int sigmaS, int sigmaR, int windowSize, float tol, float outFactor);

#endif
