#ifndef NUMBERS_H
#define NUMBERS_H
//*****************************************************************************
//
// numbers.h
//
// This provides a wrapper around integers, doubles, and longs so we can pass
// them around as pointers.
//
//*****************************************************************************

typedef struct int_struct      INTEGER;
typedef struct double_struct    DOUBLE;
typedef struct long_struct        LONG;
typedef struct boolean_struct  BOOLEAN;

INTEGER *newInteger(int val);
void  deleteInteger(INTEGER *integer);
int   integerGetVal(INTEGER *integer);
void  integerSetVal(INTEGER *integer, int val);
int      integerCmp(INTEGER *int1, INTEGER *int2);

DOUBLE   *newDouble(double val);
void   deleteDouble(DOUBLE *dbl);
double doubleGetVal(DOUBLE *dbl);
void   doubleSetVal(DOUBLE *dbl, double val);
int       doubleCmp(DOUBLE *dbl1, DOUBLE *dbl2);

LONG       *newLong(long val);
void     deleteLong(LONG *lng);
long     longGetVal(LONG *lng);
void     longSetVal(LONG *lng, long val);
int         longCmp(LONG *lng1, LONG *lng2);

BOOLEAN *newBoolean(bool val);
void  deleteBoolean(BOOLEAN *bl);
bool  booleanGetVal(BOOLEAN *bl);
void  booleanSetVal(BOOLEAN *bl, bool val);
int      booleanCmp(BOOLEAN *bl1, BOOLEAN *bl2);

#endif // NUMBERS_H
