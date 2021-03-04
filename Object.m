#import "Object.h"
#import "AutoReleasePool.h"


id __objc_allocate_instance(id class);
id __objc_free_instance(id instance);


@implementation Object

+ (id) alloc
{
    return [__objc_allocate_instance(self) retain];
}


+ (id) new
{
    return [[self alloc] init];
}


- (id) free
{
    return __objc_free_instance(self);
}


- (id) init
{
    return self;
}


- (void) release
{
    if (--retainCount_ == 0) {
        [self free];
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


@end
