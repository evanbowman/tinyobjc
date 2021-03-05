#import "Object.h"
#import "Pool.h"
#include <stdlib.h>
#include <string.h>


size_t tinyobjc_class_instance_size(id class);
Class tinyobjc_get_superclass(Class c);
void tinyobjc_make_cache(void* class);


static const size_t header_size = sizeof(void*);


@implementation Object

+ (id) alloc
{
    const size_t size = tinyobjc_class_instance_size(self);

    if (header_size < sizeof(int)) {
        // I don't know of a system where this statement would evaluate to true,
        // but we should make sure that there's enough space in the header to
        // store the retain count. We're using sizeof(void*) as the object
        // header size, as this should play nice with the alignment requirements
        // of the instance data members.
        //
        // TODO: raise error?
        return nil;
    }

    char* mem = calloc(1, size + header_size);

    if (mem == NULL) {
        return nil;
    }

    mem += header_size;
    id obj = (id)mem;

    obj->class_pointer = self;

    return [obj retain];
}


+ (id) poolAlloc: (Pool*) pool
{
    const size_t size = tinyobjc_class_instance_size(self);

    if (size + header_size < [pool elementSize]) {
        return nil;
    }

    char* mem = [pool malloc];

    if (mem == NULL) {
        return nil;
    }

    memset(mem, 0, size + header_size);

    mem += header_size;
    id obj = (id)mem;

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
    char* mem = ((char*)self) - header_size;

    int* retain_count_p = (int*)mem;

    if (--(*retain_count_p) == 0) {
        free(mem);
    }
}


- (void) poolRelease: (Pool*) pool
{
    const size_t size = tinyobjc_class_instance_size(self);

    if (size + header_size < [pool elementSize]) {
        // TODO: throw error, once we've implemented exception handling.
        return;
    }

    char* mem = ((char*)self) - header_size;

    int* retain_count_p = (int*)mem;

    if (--(*retain_count_p) == 0) {
        [pool free: mem];
    }
}


- (instancetype) retain
{
    char* header = ((char*)self) - header_size;

    int* retain_count_p = (int*)header;

    ++(*retain_count_p);

    return self;
}


+ (id) retain
{
    return self;
}


- (int) retainCount
{
    char* header = ((char*)self) - header_size;

    int* retain_count_p = (int*)header;

    return *retain_count_p;
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
    return tinyobjc_get_superclass(self);
}


- (Class) superclass
{
    return tinyobjc_get_superclass([self class]);
}


+ (void) cache
{
    tinyobjc_make_cache(self);
}


@end
