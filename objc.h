#ifndef OBJC_H
#define OBJC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>


#define __GNU_LIBOBJC__ 20110608


#undef BOOL
typedef unsigned char BOOL;

#define YES   (BOOL)1
#define NO    (BOOL)0


typedef const struct objc_selector* SEL;

typedef struct objc_class* Class;

typedef struct objc_object
{
    Class class_pointer;
}* id;


typedef id (*IMP)(id, SEL, ...);


#define nil (id)0

#define Nil (Class) 0


#ifndef __OBJC__
  typedef struct objc_object Protocol;
#else /* __OBJC__ */
  @class Protocol;
#endif

#ifdef __cplusplus
}
#endif

#endif // OBJC_H
