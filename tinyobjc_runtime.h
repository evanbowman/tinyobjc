#pragma once


enum {
    // Default size of the class table. If you exceed this size, the library
    // will use malloc.
    CLASS_TABLE_SIZE = 100,

    // Size of a method cache. Due to memory constraints, not all classes have a
    // method cache by default; you may create caches by sending the message
    // "cache" to a Class. You may want to play around with the message cache
    // size, to find a good balance between performance and memory usage. Also:
    // use a power-of-two size!
    METHOD_CACHE_SIZE = 32,
};
