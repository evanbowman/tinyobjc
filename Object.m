#import "Object.h"
#import "AutoReleasePool.h"
#include <stdlib.h>


size_t tinyobjc_class_instance_size(id class);
void tinyobjc_make_cache(void* class);


@implementation Object

+ (id) alloc
{
    const size_t size = tinyobjc_class_instance_size(self);

    id obj = calloc(1, size);

    obj->class_pointer = self;

    return [obj retain];
}


+ (id) new
{
    return [[self alloc] init];
}


- (id) init
{
    return self;
}


- (void) release
{
    if (--retainCount_ == 0) {
        free(self);
    }
}


- (instancetype) retain
{
    ++retainCount_;
    return self;
}


+ (id) retain
{
    return self;
}


- (int) retainCount
{
    return retainCount_;
}


+ (size_t) instanceSize
{
    return tinyobjc_class_instance_size(self);
}


+ (Class) class
{
    return self;
}


- (Class) class
{
    return isa;
}


+ (Class) superclass
{
    return super;
}


+ (void) cache
{
    tinyobjc_make_cache(self);
}


@end
