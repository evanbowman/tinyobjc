#import "Object.h"


@interface Pool : Object
- (size_t) elementSize;
- (void*) malloc;
- (void) free: (void*) mem;
@end
