// FIXME: path
#include "objc.h"


@class Pool;


@interface Object
{
    Class isa;
}

+ (id) alloc;
+ (id) poolAlloc: (Pool*) Pool;

+ (id) new;
- (id) init;

- (void) release;
- (void) poolRelease: (Pool*) Pool;
- (instancetype) retain;

+ (id) retain;

- (int) retainCount;

+ (size_t) instanceSize;

+ (Class) class;
- (Class) class;

+ (Class) superclass;
- (Class) superclass;

+ (void) cache;

@end
