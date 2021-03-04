// FIXME: path
#include </opt/devkitpro/devkitARM/lib/gcc/arm-none-eabi/10.2.0/include/objc/objc.h>


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

+ (Class) superclass;

@end
