#pragma once



#define MTHmax(a,b)  (((a) > (b)) ? (a) : (b))
#define MTHmin(a,b)  (((a) < (b)) ? (a) : (b))
#define MTHabs(a)  (((a) > 0) ? (a) : (-(a)))
#define MTHroundNeg(a) ((int)(a - 0.5))
#define MTHroundPos(a) ((int)(a + 0.5))
#define MTHround(a)  (((a) > 0) ? MTHroundPos(a) : MTHroundNeg(a))