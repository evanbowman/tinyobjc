#include "tinyobjc_runtime.h"
#include "module_abi.h"
#include <string.h>
#include <stdlib.h>


typedef struct TinyObjcClass {
    struct TinyObjcClass* class_;
    struct TinyObjcClass* super_class_;
    struct objc_method_list_gcc* method_list_;
    struct TinyObjcCategory* categories_;
    struct objc_method_gcc** dtable_;
    const char* name_;
    unsigned instance_size_;
    BOOL resolved_;
} TinyObjcClass;


static struct TinyObjcClassTable {
    TinyObjcClass* class_;
} tinyobjc_classes[CLASS_TABLE_SIZE];


static long long nil_method(id self, SEL _cmd)
{
    return 0;
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
    class->dtable_ =
        malloc(METHOD_CACHE_SIZE * sizeof(struct objc_method_gcc *));

    for (int i = 0; i < METHOD_CACHE_SIZE; ++i) {
        class->dtable_[i] = &void_method_gcc;
    }
}


static TinyObjcClass* tinyobjc_get_class(const char* name)
{
    for (int i = 0; i < CLASS_TABLE_SIZE; ++i) {
        if (tinyobjc_classes[i].class_) {

            if (strcmp(name, tinyobjc_classes[i].class_->name_) == 0) {

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
    if (class->resolved_) {
        return;
    }

    // The unresolved class could be a metaclass, in which case, we want to just
    // go ahead and resolve the whole class chain, so we don't need complicated
    // logic to figure out whether we're a metaclass or not, to link to the
    // correct metaclass in the parallel superclass meta chain.
    class = tinyobjc_get_class(class->name_);

    if (class->super_class_ == NULL) {
        return;
    }

    TinyObjcClass* super = tinyobjc_get_class((const char*)class->super_class_);

    if (!super) {
        // Hmm... what? The linker should have raised an error...
        while (1) ;
    }

    class->super_class_ = super;
    class->class_->super_class_ = super->class_;

    class->resolved_ = YES;
    class->class_->resolved_ = YES;
}


id objc_get_class(const char* name)
{
    return (id)tinyobjc_get_class(name);
}


size_t tinyobjc_class_instance_size(id class)
{
    return ((TinyObjcClass*)class)->instance_size_;
}


static int type_compare(const char* lhs, const char* rhs)
{
    return lhs == rhs || strcmp(lhs, rhs) == 0;
}


static struct objc_method_gcc* objc_load_method_slow(TinyObjcClass* class,
                                                     SEL selector)
{
    while (1) {
        if (class->method_list_) {
            struct objc_method_list_gcc* methods = class->method_list_;

            while (methods) {
                for (int i = 0; i < methods->count; ++i) {
                    struct objc_method_gcc* method = &methods->methods[i];

                    const char* method_sel_name =
                        (const char*)&method->selector->index;

                    if (type_compare(method->types, selector->types) &&
                        (selector->name == method_sel_name ||
                         strcmp(selector->name, method_sel_name) == 0)) {
                        return method;
                    }
                }
                methods = methods->next;
            }

        }

        if (class->super_class_) {
            class = class->super_class_;
        } else {
            // FIXME...
            return NULL;
        }
    }
}


IMP tinyobjc_msg_lookup(TinyObjcClass* class, SEL selector)
{
    tinyobjc_resolve_class(class);

    if (class->dtable_) {
        uint32_t hash = tinyobjc_fnv32(selector->name);

        uint32_t offset = hash % METHOD_CACHE_SIZE;

        struct objc_method_gcc* cached = class->dtable_[offset];

        const char* cached_name = (const char*)&(cached)->selector->index;

        if (strcmp(selector->name, cached_name) == 0) {
            return cached->imp;
        }

        struct objc_method_gcc* found = objc_load_method_slow(class, selector);
        class->dtable_[offset] = found;

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
    TinyObjcClass* new = malloc(sizeof(TinyObjcClass));

    new->class_ = malloc(sizeof(TinyObjcClass));
    new->class_->name_ = ((struct objc_class_gsv1*)class->isa)->name;

    // The GCC ABI stores superclasses as strings. We will do the same, and
    // resolve the pointers to the actual classes after loading everything.
    new->class_->super_class_ = (TinyObjcClass*)
        ((struct objc_class_gsv1*)class->isa)->super_class;

    new->class_->method_list_ =
        ((struct objc_class_gsv1*)((struct objc_class_gsv1*)class->isa)->isa)->methods;
    new->class_->class_ = NULL;
    new->class_->resolved_ = NO;
    new->class_->dtable_ = NULL;

    new->class_->instance_size_ = (sizeof (TinyObjcClass));


    new->name_ = new->class_->name_;
    new->super_class_ = new->class_->super_class_;
    new->method_list_ = ((struct objc_class_gsv1*)class->isa)->methods;
    new->resolved_ = NO;
    new->dtable_ = NULL;

    new->instance_size_ = ((struct objc_class_gsv1*)class->isa)->instance_size;

    return new;
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
