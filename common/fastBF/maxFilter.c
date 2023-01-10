#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <mth_math.h>

#define DEBUG_OUTPUT 0


/**
* @brief finds maximum
*
* @param inImg input pixel-array
* @param yRes height of image in pixel
* @param xRes width of image in pixel
* @param windowSize window size of filter
*
* @return float returns the maximum             */
float maxFilter(float inImg[], const int yRes, const int xRes, int windowSize) {
    float result = -1;    
    int sym = (windowSize - 1) / 2;
    int m = yRes; //number of rows
    int n = xRes;  //number of cols

    int pad1 = (int)(windowSize * (ceil((float)m / (float)windowSize)) - m);
    int pad2 = (int)(windowSize * (ceil((float)n / (float)windowSize)) - n);

    m += pad1;
    n += pad2;

    int rows_new = m;
    int cols_new = n;

#if DEBUG_OUTPUT
    printf("-----------------padded array-------------------\n");
#endif

    //--------------------------padding the image-------------------------
    float *img_padded = (float *)malloc(rows_new * cols_new * sizeof(float));
    if (!img_padded) {
        return (float)-1;
    }
    memset(img_padded, 0, rows_new * cols_new * sizeof(float));

    int orig_cnt = 0;
    int index = 0;
    for (int row_cnt = 0; row_cnt < rows_new; row_cnt++) {
        for (int col_cnt = 0; col_cnt < cols_new; col_cnt++) {
            //whole row needs to be padded
            if ((row_cnt - 1) >= (rows_new - 1) - pad1) {
                img_padded[index] = img_padded[index - cols_new];
            }
            else {
                if ((col_cnt - 1) >= (cols_new - 1) - pad2) {
                    img_padded[index] = img_padded[index - 1];
                }
                else {
                    img_padded[index] = inImg[orig_cnt];
                    orig_cnt++;
                }
            }

#if DEBUG_OUTPUT
            printf("%f ", img_padded[index]);
#endif
            index++;
        }
#if DEBUG_OUTPUT
        printf("\n");
#endif
    }
    //-----------------------------------------------------------------

    //preserve original image for later
    float *img_padded_original = (float *)malloc(rows_new * cols_new * sizeof(float));
    if (!img_padded_original) {
        free(img_padded);
        return (float)-1;
    }
    memcpy(img_padded_original, img_padded, rows_new*cols_new*sizeof(float));

    //-------------------scan along row--------------------------------    
    //loop through each row
    float *Lunderscore = (float *)malloc(cols_new * sizeof(float));
    float *Runderscore = (float *)malloc(cols_new * sizeof(float));
    if (!Lunderscore || !Runderscore) {
        free(Runderscore);
        free(Lunderscore);
        free(img_padded_original);
        free(img_padded);
        return (float)-1;
    }
    for (int ii = 0; ii < m; ii++) {
        Lunderscore[0] = img_padded[ii*cols_new]; //Wert: Zeile ii, Spalte 1
        Runderscore[n - 1] = img_padded[ii*cols_new + cols_new - 1]; //Zeile ii, letzten Spalte

        for (int k = 2; k <= cols_new; k++) {
            if ((k - 1) % windowSize == 0) {
                Lunderscore[k - 1] = img_padded[ii*cols_new + (k - 1)];
                Runderscore[cols_new - k] = img_padded[ii*cols_new + (cols_new - k)];
            }
            else {
                Lunderscore[k - 1] = MTHmax(Lunderscore[k - 2], img_padded[ii*cols_new + (k - 1)]);
                Runderscore[cols_new - k] = MTHmax(Runderscore[cols_new - k + 1], img_padded[ii*cols_new + (cols_new - k)]);
            }
        }

#if DEBUG_OUTPUT        
        printf("L: \n");
        int z;
        for (z = 0; z < cols_new; z++) {
            printf("%f,", Lunderscore[z]);
        }
        printf("\nR: \n");
        for (z = 0; z < cols_new; z++) {
            printf("%f,", Runderscore[z]);
        }
        printf("\n----------------------------------\n");
#endif

        for (int kk = 1; kk <= n; kk++) {
            int p = kk - sym;
            int q = kk + sym;
            float r, l;
            if (p < 1) {
                r = -1.0f;
            }
            else {
                r = Runderscore[p - 1];
            }
            if (q > n) {
                l = -1.0f;
            }
            else {
                l = Lunderscore[q - 1];
            }
            img_padded[ii*cols_new + kk - 1] = MTHmax(r, l);
        }
    }

#if DEBUG_OUTPUT
    printf("----------------after row scan----------\n");
    for (int fu = 0; fu < rows_new*cols_new; fu++) {
        static int cnt = 1;
        printf("%f ", img_padded[fu]);
        if (cnt == cols_new) {
            printf("\n");
            cnt = 1;
        }
        else cnt++;
    }
    printf("------------------------------------------\n");
#endif

    //-------------------scan along column----------------------------------------------
    //loop through each column
    //Lunderscore = (float *) malloc(rows_new * sizeof(float));
    //Runderscore = (float *) malloc(rows_new * sizeof(float));
    for (int jj = 0; jj < n; jj++) {
        Lunderscore[0] = img_padded[jj]; //Wert: Spalte jj, Zeile 1
        Runderscore[n - 1] = img_padded[cols_new*(rows_new - 1) + jj]; //Wert: Spalte jj, letzte Zeile
        for (int k = 2; k <= rows_new; k++) {
            if ((k - 1) % windowSize == 0) {
                Lunderscore[k - 1] = img_padded[cols_new*(k - 1) + jj];
                Runderscore[rows_new - k] = img_padded[(rows_new - k)*cols_new + jj];
            }
            else {
                Lunderscore[k - 1] = MTHmax(Lunderscore[k - 2], img_padded[cols_new*(k - 1) + jj]);
                Runderscore[rows_new - k] = MTHmax(Runderscore[rows_new - k + 1], img_padded[(rows_new - k)*cols_new + jj]);
            }
        }

#if DEBUG_OUTPUT        
        printf("L: \n");
        for (int z = 0; z < cols_new; z++) {
            printf("%f,", Lunderscore[z]);
        }
        printf("\nR: \n");
        for (int z = 0; z < cols_new; z++) {
            printf("%f,", Runderscore[z]);
        }
        printf("\n----------------------------------\n");
#endif

        for (int kk = 1; kk <= m; kk++) {
            int p = kk - sym;
            int q = kk + sym;
            float r, l;
            if (p < 1) {
                r = -1.0f;
            }
            else {
                r = Runderscore[p - 1];
            }
            if (q > m) {
                l = -1.0f;
            }
            else {
                l = Lunderscore[q - 1];
            }
            float temp = MTHmax(r, l) - img_padded_original[(kk - 1)*cols_new + jj];

#if DEBUG_OUTPUT
            printf("index: %i \n", (kk - 1)*cols_new + jj);
            printf("outer loop %i, inner loop %i, temp = %f \n", jj, kk, temp);
#endif

            if (temp > result) {
                result = temp;
            }
        }
    }
    free(Runderscore);
    free(Lunderscore);
    free(img_padded_original);
    free(img_padded);
    return result;
}
