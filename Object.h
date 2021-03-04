
@interface Object
{
    Class isa;
    int retain_count_;
}

+ (id)alloc;
- (id)free;

+ (id)new;
- (id)init;

- (void)release;
- (instancetype)retain;

@end
