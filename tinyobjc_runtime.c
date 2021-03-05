#include "tinyobjc_runtime.h"
#include "module_abi.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


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


static inline MethodList abi_methods_next(MethodList methods)
{
    return ((struct objc_method_list_gcc*)methods)->next;
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
    // In an objc_method_gcc structure, the selector field is in fact not a
    // pointer to a selector, but a pointer to string literal. I wish this was
    // documented, because I was super confused.
    return (const char*)((struct objc_method_gcc*)method)->selector;
}


static inline void abi_method_set_name(Method method, const char* name)
{
    struct objc_method_gcc* m = method;
    // See comment in abi_method_name().
    m->selector = (SEL)name;
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


typedef struct {
    const char* name_;
    const char* types_;
} CachedSelector;


static CachedSelector selector_table[SELECTOR_TABLE_SIZE] = {
};


// We register a selector by replacing its string with a pointer to the location
// of the string in a selector string array. This allows us to determine whether
// a selector is registered by comparing its string pointer to the range of the
// selector table.
static inline BOOL tinyobjc_is_sel_name_registered(const char* name)
{
    return ((CachedSelector*) name >= selector_table) &&
           ((CachedSelector*) name < selector_table + SELECTOR_TABLE_SIZE);
}


const char* selector_name(SEL s)
{
    if (tinyobjc_is_sel_name_registered(s->name)) {
        // SEL is registered, so it's been replaced by a pointer into the
        // selector string table.
        return *((char**)s->name);
    }
    return s->name;
}


static const char* tinyobjc_sel_register_name(const char* name,
                                              const char* types)
{
    for (int i = 0; i < SELECTOR_TABLE_SIZE; ++i) {
        if (selector_table[i].name_ == NULL) {
            selector_table[i].name_ = name;
            selector_table[i].types_ = types;
            return (const char*)&selector_table[i];
        } else if (strcmp(selector_table[i].name_, name) == 0 &&
                   strcmp(selector_table[i].types_, types) == 0) {
            return (const char*)&selector_table[i];
        }
    }

    while (1) ;
}


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


Class tinyobjc_get_superclass(Class c)
{
    Class super = abi_get_super(c);

    if (super) {
        return super;
    }

    return (Class)nil;
}


id objc_get_class(const char* name)
{
    return (id)tinyobjc_get_class(name);
}


size_t tinyobjc_class_instance_size(id class)
{
    return abi_instance_size(class);
}


/* static int type_compare(const char* lhs, const char* rhs) */
/* { */
/*     // TODO: find a way to avoid the string compare for the typename. */
/*     // Some characters in the typename may be ignored when matching, right? */
/*     return lhs == rhs || strcmp(lhs, rhs) == 0; */
/* } */


static struct objc_method_gcc* objc_load_method_slow(TinyObjcClass* class,
                                                     SEL selector)
{
    while (1) {
        MethodList methods = abi_methods(class);

        while (methods) {
            for (int i = 0; i < abi_method_count(methods); ++i) {
                Method method = abi_method_at(methods, i);

                const char* method_sel_name = abi_method_name(method);
                const char* typeinfo = abi_method_typeinfo(method);

                if (!tinyobjc_is_sel_name_registered(method_sel_name)) {
                    abi_method_set_name(method,
                                        tinyobjc_sel_register_name(method_sel_name,
                                                                   typeinfo));
                    method_sel_name = abi_method_name(method);
                }

                if (selector->name == method_sel_name) {
                    return method;
                }
            }

            methods = abi_methods_next(methods);
        }

        if (abi_get_super(class)) {
            class = (TinyObjcClass*)abi_get_super(class);
        } else {
            return NULL;
        }
    }
}


static inline unsigned int hash(unsigned int value)
{
    unsigned basis = 0x811C9DC5;
    unsigned prime = 0x01000193;
    unsigned result = basis;
    result *= prime;
    result ^= value;
    return result;
}


static inline IMP tinyobjc_msg_lookup(TinyObjcClass* class, SEL selector)
{
    tinyobjc_resolve_class(class);

    if (!tinyobjc_is_sel_name_registered(selector->name)) {
        // This line is probably pretty scary. Yes we are casting away const,
        // but the GCC ABI practically expects you to do this, and other
        // implementations for gcc and clang do the same thing.
        ((struct objc_selector*)selector)->name =
            tinyobjc_sel_register_name(selector->name,
                                       selector->types);
    }

    Method* dtable = abi_get_dtable(class);

    if (dtable) {
        uint32_t offset =
            hash((unsigned)(intptr_t)selector->name) % METHOD_CACHE_SIZE;

        struct objc_method_gcc* cached = dtable[offset];

        const char* cached_name = (const char*)(cached)->selector;

        if (selector->name == cached_name) {
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


static void tinyobjc_load_category(struct objc_category_gcc* category)
{
    while (1) {
        // TODO: support categories...
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

    while (symbols->class_count > 1) {
        // TODO: support multiple classes per module.
    }
    for (int i = 0; i < symbols->class_count; ++i) {
        tinyobjc_load_class(defs);
        ++defs;
    }

    while (symbols->category_count) ;
    for (int i = 0; i < symbols->category_count; ++i) {
        tinyobjc_load_category(defs);
        ++defs;
    }

    int i = 0;
    while (1) {
        if (symbols->definitions[i] == NULL) {
            break;
        }

        ++i;
    }
}
