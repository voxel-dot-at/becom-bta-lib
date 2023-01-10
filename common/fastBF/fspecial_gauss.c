#include <stdio.h>
#include <math.h>

#define UP_COUNTING 1
#define DOWN_COUNTING 0

#define DEBUG_PRINT 0

/**
* @brief implements Matlab's fspecial('gaussian',...)
*
* @param window has to be an odd positive integer value
* @param sigmaS sigma value
* @param filter_array is an array of two dimensions: array[window][window]
*        it holds the output of fspecial                    */
void fspecial_gauss(int windowSize, float sigmaS, float *filter_array) {
    int max_value = (windowSize - 1) / 2;
    int y_value = max_value;
    int y_counting_direction = DOWN_COUNTING;
    float sum = 0;
    for (int zeile = 0; zeile <= windowSize - 1; zeile++) {
        int x_value = max_value;
        int x_counting_direction = DOWN_COUNTING;
        for (int spalte = 0; spalte <= windowSize - 1; spalte++) {
            filter_array[(zeile * windowSize) + spalte] = (float)exp((float)-(y_value * y_value + x_value * x_value) / (2 * sigmaS * sigmaS));
            sum += filter_array[(zeile * windowSize) + spalte];
            if (x_value > 0 && x_counting_direction == DOWN_COUNTING) {
                x_value--;
            }
            else {
                x_counting_direction = UP_COUNTING;
                x_value++;
            }
        }
        if (y_value > 0 && y_counting_direction == DOWN_COUNTING) {
            y_value--;
        }
        else {
            y_counting_direction = UP_COUNTING;
            y_value++;
        }
    }

    #if DEBUG_PRINT
    printf("\n");
    printf("Sum: %f \n\n", sum);
    #endif

    //if (sum != 0) {
    if (sum < -0.0000001f || sum > 0.0000001f) {
        for (int i = 0; i < windowSize; i++) {
            for (int j = 0; j < windowSize; j++) {
                filter_array[(i * windowSize) + j] /= sum;
                #if DEBUG_PRINT
                printf("%f ", filter_array[(i*window)+j]);
                #endif
            }
            #if DEBUG_PRINT
            printf("\n");
            #endif
        }
    }
    
}

