#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define DEBUG_OUTPUT 0

/**
* @brief imfilter_sep
*
* @param inImg input image
* @param filter filter kernel array
* @param output output image
* @param img_height in pixel
* @param img_width in pixel
* @param window window size (kernel has size window^2)
*/

int imfilter_sep(float inImg[], float filter[], float output[], int img_height, int img_width, int window){

    int omnidir_pad = (int)floor((float)window/(float)2);
    int rows_new = img_height + 2*omnidir_pad;
    int cols_new = img_width + 2*omnidir_pad;
    int width_new = cols_new;
    float *img_padded;
    float *filter_sep;
    int row_cnt,col_cnt;
    int orig_cnt = 0;
    int index = 0;
    int i;
    int j;
    int l;
    int g;
    int cnt;
    int filter_left_pos = 0;
    int filter_top_pos = 0;
    int col_cnt2 = 0;
    float *temp;
    float temp_result;

    #if DEBUG_OUTPUT
        println("-----------------padded array-------------------");
    #endif

    //--------------------------padding the image on all sides with omnidir_pad elements-------------------------

    img_padded = (float *)malloc(rows_new * cols_new *sizeof(float));
    if(img_padded == NULL){
        return -1;
    }

    for(row_cnt=0; row_cnt<rows_new; row_cnt++){
        
        for(col_cnt=0; col_cnt<cols_new; col_cnt++){
            
            //padd a whole row at beginning or end
            if(row_cnt <= (omnidir_pad-1) || row_cnt >= (omnidir_pad+img_height)){
                
                img_padded[index] = 0; //pad with 0
            }
            else{
                if(col_cnt <= (omnidir_pad-1) || col_cnt >= (omnidir_pad+img_width)){

                    img_padded[index] = 0; //pad with 0
                }
                else{
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
        println("");
        #endif
    }

    //--------------------------------correlating with filter---------------------------------

    //place center of filter on original image (= place left top corner of filter on padded image)
    //and move right (img_width-1) times
    //repeat for each img_height rows

    filter_sep = (float *)malloc(window * sizeof(float));
    if(filter_sep == NULL){
        free(img_padded);
        return -1;
    }
    memset(filter_sep,0,window * sizeof(float));

    //spearate filter
    for(i=0; i<window*window; i+=window){

        for(j=0; j<window; j++){
            filter_sep[i/window] += filter[i+j];
        }
    }

    temp = (float *)malloc(width_new * sizeof(float));
    if(temp == NULL){
        free(img_padded);
        free(filter_sep);
        return -1;
    }
    
    for(cnt=0; cnt < img_height*img_width; cnt++){

        //temp results of separable (cols)
        if(filter_left_pos==0){
            
            for(l=0; l<width_new; l++){

                temp[l] = 0; //clear

                for(g=0; g<window; g++){

                    index = (filter_top_pos*width_new) + (l+g*width_new);

                    temp[l] += img_padded[index] * filter_sep[g];
                }
            }
        }

        temp_result=0;

        for(i=0; i <= window-1; i++){
            temp_result += temp[filter_left_pos+i] * filter_sep[i];
        }

        output[cnt] = temp_result;

        filter_left_pos++;

        col_cnt2++;
        if(col_cnt2 == img_width){
            col_cnt2 = 0;
            filter_left_pos = 0;
            filter_top_pos ++;
        }        
    }
    free(img_padded);
    free(temp);
    free(filter_sep);

    return 0;
}
