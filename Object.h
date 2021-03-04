// FIXME: path
#include "objc.h"


@interface Object
{
    Class isa;
    int retainCount_;
}

+ (id) alloc;

+ (id) new;
- (id) init;

- (void) release;
- (instancetype) retain;

+ (id) retain;

- (int) retainCount;

+ (size_t) instanceSize;

+ (Class) class;
- (Class) class;

+ (Class) superclass;

+ (void) cache;

@end
