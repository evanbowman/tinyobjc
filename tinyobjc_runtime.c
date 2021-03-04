#include "tinyobjc_runtime.h"
#include "module_abi.h"
#include <string.h>
#include <stdlib.h>


////////////////////////////////////////////////////////////////////////////////
//
// Opaque GCC ABI accessors
//
////////////////////////////////////////////////////////////////////////////////


typedef void TinyObjcClass;
typedef void* MethodList;
typedef void* Method;


static inline TinyObjcClass* abi_get_super(TinyObjcClass* class)
{
    return ((struct objc_class_gsv1*)class)->super_class;
}


static inline void abi_set_super(TinyObjcClass* class,
                                 TinyObjcClass* super)
{
    ((struct objc_class_gsv1*)class)->super_class = (Class)super;
}


static inline const char* abi_get_name(TinyObjcClass* class)
{
    return ((struct objc_class_gsv1*)class)->name;
}


static inline void abi_set_name(TinyObjcClass* class, const char* name)
{
    ((struct objc_class_gsv1*)class)->name = name;
}


static inline BOOL abi_is_resolved(TinyObjcClass* class)
{
    return ((struct objc_class_gsv1*)class)->info;
}


static inline void abi_set_resolved(TinyObjcClass* class, BOOL resolved)
{
    ((struct objc_class_gsv1*)class)->info = resolved;
}


static inline TinyObjcClass* abi_class(TinyObjcClass* class)
{
    return ((struct objc_class_gsv1*)class)->isa;
}


static inline size_t abi_instance_size(TinyObjcClass* class)
{
    return ((struct objc_class_gsv1*)class)->instance_size;
}


static inline MethodList abi_methods(TinyObjcClass* class)
{
    return ((struct objc_class_gsv1*)class)->methods;
}


static inline Method* abi_get_dtable(TinyObjcClass* class)
{
    return ((struct objc_class_gsv1*)class)->dtable;
}


static inline void abi_set_dtable(TinyObjcClass* class, Method* dtable)
{
    ((struct objc_class_gsv1*)class)->dtable = dtable;
}


static inline int abi_method_count(MethodList methods)
{
    return ((struct objc_method_list_gcc*)methods)->count;
}


static inline Method abi_method_at(MethodList methods, int index)
{
    return &((struct objc_method_list_gcc*)methods)->methods[index];
}


static inline const char* abi_method_name(Method method)
{
    return ((const char*)&((struct objc_method_gcc*)method)->selector->index);
}


static inline const char* abi_method_typeinfo(Method method)
{
    return ((struct objc_method_gcc*)method)->types;
}



////////////////////////////////////////////////////////////////////////////////
//
// TinyObjc Library
//
////////////////////////////////////////////////////////////////////////////////


static struct TinyObjcClassTable {
    TinyObjcClass* class_;
} tinyobjc_classes[CLASS_TABLE_SIZE];


static id nil_method(id self, SEL _cmd)
{
    return self;
}


static struct objc_selector void_selector = {
    { .name = ""},
    .types = ""
};


static struct objc_method_gcc void_method_gcc = {
    .selector = &void_selector,
    .types = "",
    .imp = (IMP)(void*)nil_method,
};


void tinyobjc_make_cache(TinyObjcClass* class)
{
    if (abi_get_dtable(class)) {
        free(abi_get_dtable(class));
    }

    Method* dtable = malloc(METHOD_CACHE_SIZE * sizeof(Method));

    for (int i = 0; i < METHOD_CACHE_SIZE; ++i) {
        dtable[i] = &void_method_gcc;
    }

    abi_set_dtable(class, dtable);
}


static TinyObjcClass* tinyobjc_get_class(const char* name)
{
    for (int i = 0; i < CLASS_TABLE_SIZE; ++i) {
        if (tinyobjc_classes[i].class_) {

            if (strcmp(name, abi_get_name(tinyobjc_classes[i].class_)) == 0) {

                TinyObjcClass* result = tinyobjc_classes[i].class_;

                if (i > 0) {
                    // Move to front.
                    tinyobjc_classes[i].class_ = tinyobjc_classes[i - 1].class_;
                    tinyobjc_classes[i - 1].class_ = result;
                }

                return result;
            }
        }
    }

    return NULL;
}


#define TINYOBJC_FNV_PRIME_32 16777619
#define TINYOBJC_FNV_OFFSET_32 2166136261U


static uint32_t tinyobjc_fnv32(const char *s)
{
    uint32_t hash = TINYOBJC_FNV_OFFSET_32;

    while (*s != '\0') {
        hash = hash ^ (*s);
        hash = hash * TINYOBJC_FNV_PRIME_32;
        ++s;
    }

    return hash;
}


static void tinyobjc_resolve_class(TinyObjcClass* class)
{
    if (abi_is_resolved(class)) {
        return;
    }

    // The unresolved class could be a metaclass, in which case, we want to just
    // go ahead and resolve the whole class chain, so we don't need complicated
    // logic to figure out whether we're a metaclass or not, to link to the
    // correct metaclass in the parallel superclass meta chain.
    class = tinyobjc_get_class(abi_get_name(class));

    if (abi_get_super(class) == NULL) {
        return;
    }

    TinyObjcClass* super =
        tinyobjc_get_class((const char*)abi_get_super(class));

    if (!super) {
        // Hmm... what? The linker should have raised an error...
        while (1) ;
    }

    abi_set_super(class, super);
    abi_set_super(abi_class(class), abi_class(super));

    abi_set_resolved(class, YES);
    abi_set_resolved(abi_class(class), YES);
}


id objc_get_class(const char* name)
{
    return (id)tinyobjc_get_class(name);
}


size_t tinyobjc_class_instance_size(id class)
{
    return abi_instance_size(class);
}


static int type_compare(const char* lhs, const char* rhs)
{
    return lhs == rhs || strcmp(lhs, rhs) == 0;
}


static struct objc_method_gcc* objc_load_method_slow(TinyObjcClass* class,
                                                     SEL selector)
{
    while (1) {
        if (abi_methods(class)) {
            MethodList methods = abi_methods(class);

            for (int i = 0; i < abi_method_count(methods); ++i) {
                Method method = abi_method_at(methods, i);

                const char* method_sel_name = abi_method_name(method);
                const char* typeinfo = abi_method_typeinfo(method);

                if (type_compare(typeinfo, selector->types) &&
                    (selector->name == method_sel_name ||
                     strcmp(selector->name, method_sel_name) == 0)) {
                    return method;
                }
            }
        }

        if (abi_get_super(class)) {
            class = (TinyObjcClass*)abi_get_super(class);
        } else {
            // FIXME...
            return NULL;
        }
    }
}


IMP tinyobjc_msg_lookup(TinyObjcClass* class, SEL selector)
{
    tinyobjc_resolve_class(class);

    Method* dtable = abi_get_dtable(class);

    if (dtable) {
        uint32_t offset = tinyobjc_fnv32(selector->name) % METHOD_CACHE_SIZE;

        struct objc_method_gcc* cached = dtable[offset];

        const char* cached_name = (const char*)&(cached)->selector->index;

        if (strcmp(selector->name, cached_name) == 0) {
            return cached->imp;
        }

        struct objc_method_gcc* found = objc_load_method_slow(class, selector);
        dtable[offset] = found;

        return found->imp;
    }

    return objc_load_method_slow(class, selector)->imp;
}


IMP objc_msg_lookup_super(struct objc_super* super, SEL selector)
{
    id receiver = super->receiver;

    if (__builtin_expect(receiver == nil, NO)) {
        return (IMP)(void*)nil_method;
    }

    // I wish there was a better way than looking up the class by string name :/
    TinyObjcClass* class = tinyobjc_get_class((const char*)super->class);
    if (class) {
        return tinyobjc_msg_lookup(class, selector);
    }

    return (IMP)(void*)nil_method;
}


IMP objc_msg_lookup(id receiver, SEL selector)
{
    if (__builtin_expect(receiver == nil, NO)) {
        return (IMP)(void*)nil_method;
    }

    Class class = receiver->class_pointer;

    return tinyobjc_msg_lookup((TinyObjcClass*)class, selector);
}


static TinyObjcClass* tinyobjc_make_class(struct objc_class_gsv1* class)
{
    abi_set_dtable(abi_class(class), NULL);
    abi_set_resolved(abi_class(class), NO);

    abi_set_dtable(abi_class(abi_class(class)), NULL);
    abi_set_resolved(abi_class(abi_class(class)), NO);

    return (TinyObjcClass*)class->isa;
}


static void tinyobjc_load_class(struct objc_class_gsv1* class)
{
    for (int i = 0; i < CLASS_TABLE_SIZE; ++i) {
        if (tinyobjc_classes[i].class_ == NULL) {
            tinyobjc_classes[i].class_ = tinyobjc_make_class(class);
            return;
        }
    }

    while (1) {
        // FIXME: dynamically resize the class table.
    }
}

void __objc_exec_class(struct objc_module_abi_8* module)
{
    struct objc_symbol_table_abi_8* symbols = module->symbol_table;

    if (module->version != 8) {
        while (1) ; // TODO: raise error for incompatible version... but main
                    // hasn't even been called yet, so how to signal the error
                    // to the user?
    }

    while (symbols->selector_count) ;

    void* defs = symbols->definitions;

    for (int i = 0; i < symbols->class_count; ++i) {
        tinyobjc_load_class(defs);
        ++defs;
    }

    for (int i = 0; i < symbols->category_count; ++i) {
        ++defs;
    }
}
