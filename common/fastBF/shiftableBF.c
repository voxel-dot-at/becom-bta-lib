#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "fspecial_gauss.h"
#include "maxFilter.h"
#include "imfilter.h"
#include <mth_math.h>

#define DEBUG_OUTPUT 0

static int binomial_coefficient(int n, int k);


/**
* @brief shiftableBF
*
* @param inImg input image
* @param outImg output image (same size as input image)
* @param img_height number of rows in image
* @param img_width number of columns in image
* @param sigmaS width of spatial Gaussian
* @param sigmaR width of range Gaussian
* @param window domain of spatial Gaussian
* @param tol truncation error
* @param outFactor the factor the output is multiplied with
*
* @return returns negative value in case of error                   */
int shiftableBF(float inImg[], float outImg[], const int yRes, const int xRes, int sigmaS, int sigmaR, int windowSize, float tol, float outFactor) {
    if (windowSize < 3 || (windowSize % 2) == 0) {
        fprintf(stderr, "window size has to be an odd value >= 3\n");
        return -1;
    }
    float inputMax = maxFilter(inImg, yRes, xRes, windowSize);

    float *gauss_filter = (float *)malloc(windowSize*windowSize * sizeof(float));
    if (!gauss_filter) {
        return -1;
    }
    fspecial_gauss(windowSize, (float)sigmaS, gauss_filter);

#if DEBUG_OUTPUT
    printf("max filter result: %f \n", inputMax);
#endif

    float N = (float)ceil(0.405 * pow((inputMax / (float)sigmaR), 2));
    float gamma = (float)(1 / (sqrt(N) * (float)sigmaR));
    float twoN = (float)pow(2, N);

    //---------------------compute truncation----------------------
    float M = 0;
    if (tol >= 0.000000001) { //if (tol == 0)
        if (sigmaR <= 40) {
            if (sigmaR > 10) {
                float sumCoeffs = 0;
                int k_max = (int)round(N / 2);
                for (int k = 0; k <= k_max; k++) {
                    sumCoeffs = (sumCoeffs + (binomial_coefficient((int)N, (int)k) / twoN));
                    if (sumCoeffs > tol / 2) {
                        M = (float)k;
                        break;
                    }
                }
            }
            else {
                M = (float)ceil(0.5 * (N - sqrt(4 * N*log(2 / tol))));
            }
        }
    }

#if DEBUG_OUTPUT
    printf("N = %f \n", N);
    printf("gamma = %f \n", gamma);
    printf("twoN = %f \n", twoN);
    printf("M = %f \n", M);
#endif

    //-------------------------main filter--------------------------

    float *outImg1 = (float *)malloc(yRes*xRes * sizeof(float));
    float *outImg2 = (float *)malloc(yRes*xRes * sizeof(float));
    if (!outImg1 || !outImg2) {
        free(outImg2);
        free(outImg1);
        free(gauss_filter);
        return -1;
    }

    //zeroing out the arrays is of the utmost importance!!!
    memset(outImg1, 0, yRes*xRes * sizeof(float));
    memset(outImg2, 0, yRes*xRes * sizeof(float));

    //locals inside loop, moved out of loop
    float *temp1 = (float *)malloc(yRes*xRes * sizeof(float));
    float *temp2 = (float *)malloc(yRes*xRes * sizeof(float));
    float *temp1_mult = (float *)malloc(yRes*xRes * sizeof(float));
    float *temp2_mult = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi1 = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi2 = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi3 = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi4 = (float *)malloc(yRes*xRes * sizeof(float));
    if (!temp1 || !temp2 || !temp1_mult || !temp2_mult || !phi1 || !phi2 || !phi3 || !phi4) {
        free(phi4);
        free(phi3);
        free(phi2);
        free(phi1);
        free(temp2_mult);
        free(temp1_mult);
        free(temp2);
        free(temp1);
        free(outImg2);
        free(outImg1);
        free(gauss_filter);
        return -1;
    }

    for (int k = (int)M; k <= (int)N - M; k++) {
        float coeff = (float)(binomial_coefficient((int)N, (int)k)) / twoN;

#if DEBUG_OUTPUT
        printf("coeff: %f \n", coeff);
#endif

        for (int cnt1 = 0; cnt1 < yRes*xRes; cnt1++) {
            temp1[cnt1] = (float)cos((float)(2 * k - N) * gamma * inImg[cnt1]);
            temp1_mult[cnt1] = temp1[cnt1] * inImg[cnt1];
            temp2[cnt1] = (float)sin((float)(2 * k - N) * gamma * inImg[cnt1]);
            temp2_mult[cnt1] = temp2[cnt1] * inImg[cnt1];
        }

        if (imfilter_sep(temp1_mult, gauss_filter, phi1, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }
        if (imfilter_sep(temp2_mult, gauss_filter, phi2, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }
        if (imfilter_sep(temp1, gauss_filter, phi3, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }
        if (imfilter_sep(temp2, gauss_filter, phi4, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }

        for (int cnt2 = 0; cnt2 < yRes*xRes; cnt2++) {
            outImg1[cnt2] += coeff * ((temp1[cnt2] * (phi1[cnt2])) + (temp2[cnt2] * (phi2[cnt2])));
            outImg2[cnt2] += coeff * ((temp1[cnt2] * (phi3[cnt2])) + (temp2[cnt2] * (phi4[cnt2])));
#if DEBUG_OUTPUT
            printf("iteration %i -- ", cnt2);
            printf("phi1: %f -- ", phi1[cnt2]);
            printf("phi2: %f -- ", phi2[cnt2]);
            printf("phi3: %f -- ", phi3[cnt2]);
            printf("phi4: %f -- ", phi4[cnt2]);
            printf("outImg1: %f -- ", outImg1[cnt2]);
            printf("outImg2: %f \n", outImg2[cnt2]);
#endif
        }

    }

    //avoid division by zero
    for (int cnt_out = 0; cnt_out < yRes*xRes; cnt_out++) {
        if (outImg2[cnt_out] > -0.0001f && outImg2[cnt_out] < 0.0001f) {
            outImg[cnt_out] = outFactor * inImg[cnt_out];
        }
        else {
            outImg[cnt_out] = outFactor * outImg1[cnt_out] / outImg2[cnt_out];
        }
    }

    free(phi4);
    free(phi3);
    free(phi2);
    free(phi1);
    free(temp2_mult);
    free(temp1_mult);
    free(temp2);
    free(temp1);
    free(outImg2);
    free(outImg1);
    free(gauss_filter);

    return 1;
}


int shiftableBFU16(float inImg[], uint16_t outImg[], const int yRes, const int xRes, int sigmaS, int sigmaR, int windowSize, float tol, float outFactor) {
    if (windowSize < 3 || (windowSize % 2) == 0) {
        fprintf(stderr, "window size has to be an odd value >= 3\n");
        return -1;
    }
    float inputMax = maxFilter(inImg, yRes, xRes, windowSize);

    float *gauss_filter = (float *)malloc(windowSize*windowSize * sizeof(float));
    if (!gauss_filter) {
        return -1;
    }
    fspecial_gauss(windowSize, (float)sigmaS, gauss_filter);

#if DEBUG_OUTPUT
    printf("max filter result: %f \n", inputMax);
#endif

    float N = (float)ceil(0.405 * pow((inputMax / (float)sigmaR), 2));
    float gamma = (float)(1 / (sqrt(N) * (float)sigmaR));
    float twoN = (float)pow(2, N);

    //---------------------compute truncation----------------------
    float M = 0;
    if (tol >= 0.000000001) { //if (tol == 0)
        if (sigmaR <= 40) {
            if (sigmaR > 10) {
                float sumCoeffs = 0;
                int k_max = (int)round(N / 2);
                for (int k = 0; k <= k_max; k++) {
                    sumCoeffs = (sumCoeffs + (binomial_coefficient((int)N, (int)k) / twoN));
                    if (sumCoeffs > tol / 2) {
                        M = (float)k;
                        break;
                    }
                }
            }
            else {
                M = (float)ceil(0.5 * (N - sqrt(4 * N*log(2 / tol))));
            }
        }
    }

#if DEBUG_OUTPUT
    printf("N = %f \n", N);
    printf("gamma = %f \n", gamma);
    printf("twoN = %f \n", twoN);
    printf("M = %f \n", M);
#endif

    //-------------------------main filter--------------------------

    float *outImg1 = (float *)malloc(yRes*xRes * sizeof(float));
    float *outImg2 = (float *)malloc(yRes*xRes * sizeof(float));
    if (!outImg1 || !outImg2) {
        free(outImg2);
        free(outImg1);
        free(gauss_filter);
        return -1;
    }

    //zeroing out the arrays is of the utmost importance!!!
    memset(outImg1, 0, yRes*xRes * sizeof(float));
    memset(outImg2, 0, yRes*xRes * sizeof(float));

    //locals inside loop, moved out of loop
    float *temp1 = (float *)malloc(yRes*xRes * sizeof(float));
    float *temp2 = (float *)malloc(yRes*xRes * sizeof(float));
    float *temp1_mult = (float *)malloc(yRes*xRes * sizeof(float));
    float *temp2_mult = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi1 = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi2 = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi3 = (float *)malloc(yRes*xRes * sizeof(float));
    float *phi4 = (float *)malloc(yRes*xRes * sizeof(float));
    if (!temp1 || !temp2 || !temp1_mult || !temp2_mult || !phi1 || !phi2 || !phi3 || !phi4) {
        free(phi4);
        free(phi3);
        free(phi2);
        free(phi1);
        free(temp2_mult);
        free(temp1_mult);
        free(temp2);
        free(temp1);
        free(outImg2);
        free(outImg1);
        free(gauss_filter);
        return -1;
    }

    for (int k = (int)M; k <= (int)N - M; k++) {
        float coeff = (float)(binomial_coefficient((int)N, (int)k)) / twoN;

#if DEBUG_OUTPUT
        printf("coeff: %f \n", coeff);
#endif

        for (int cnt1 = 0; cnt1 < yRes*xRes; cnt1++) {
            temp1[cnt1] = (float)cos((float)(2 * k - N) * gamma * inImg[cnt1]);
            temp1_mult[cnt1] = temp1[cnt1] * inImg[cnt1];
            temp2[cnt1] = (float)sin((float)(2 * k - N) * gamma * inImg[cnt1]);
            temp2_mult[cnt1] = temp2[cnt1] * inImg[cnt1];
        }

        if (imfilter_sep(temp1_mult, gauss_filter, phi1, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }
        if (imfilter_sep(temp2_mult, gauss_filter, phi2, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }
        if (imfilter_sep(temp1, gauss_filter, phi3, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }
        if (imfilter_sep(temp2, gauss_filter, phi4, yRes, xRes, windowSize) != 0) {
            //TODO: error
        }

        for (int cnt2 = 0; cnt2 < yRes*xRes; cnt2++) {
            outImg1[cnt2] += coeff * ((temp1[cnt2] * (phi1[cnt2])) + (temp2[cnt2] * (phi2[cnt2])));
            outImg2[cnt2] += coeff * ((temp1[cnt2] * (phi3[cnt2])) + (temp2[cnt2] * (phi4[cnt2])));
#if DEBUG_OUTPUT
            printf("iteration %i -- ", cnt2);
            printf("phi1: %f -- ", phi1[cnt2]);
            printf("phi2: %f -- ", phi2[cnt2]);
            printf("phi3: %f -- ", phi3[cnt2]);
            printf("phi4: %f -- ", phi4[cnt2]);
            printf("outImg1: %f -- ", outImg1[cnt2]);
            printf("outImg2: %f \n", outImg2[cnt2]);
#endif
        }

    }

    //avoid division by zero
    for (int cnt_out = 0; cnt_out < yRes*xRes; cnt_out++) {
        if (outImg2[cnt_out] > -0.0001f && outImg2[cnt_out] < 0.0001f) {
            outImg[cnt_out] = (uint16_t)(outFactor * inImg[cnt_out]);
        }
        else {
            outImg[cnt_out] = (uint16_t)MTHround(outFactor * outImg1[cnt_out] / outImg2[cnt_out]);
        }
    }

    free(phi4);
    free(phi3);
    free(phi2);
    free(phi1);
    free(temp2_mult);
    free(temp1_mult);
    free(temp2);
    free(temp1);
    free(outImg2);
    free(outImg1);
    free(gauss_filter);
    return 1;
}


//n over k = n!/(k!*(n-k)!)
static int binomial_coefficient(int n, int k) {
    long long numerator = 1;
    long long denominator = 1;
    int i;
    int j;

    for (i = 1; i <= n; i++) {
        numerator *= i;
    }
    for (j = 1; j <= k; j++) {
        denominator *= j;
    }
    for (j = 1; j <= (n - k); j++) {
        denominator *= j;
    }
    return (int)(numerator / denominator);
}

