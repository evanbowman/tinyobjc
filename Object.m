#include </opt/devkitpro/devkitARM/lib/gcc/arm-none-eabi/10.2.0/include/objc/objc.h>
#import "Object.h"
#include <stdio.h>


id __objc_allocate_instance(id class);
id __objc_free_instance(id instance);


@implementation Object

+ (id)alloc
{
    return __objc_allocate_instance(self);
}


+ (id)new
{
    return [[self alloc] init];
}


- (id)free
{
    return __objc_free_instance(self);
}


- (id)init
{
    return self;
}

@end
