#pragma once

#include "objc.h"
#include <stdint.h>


struct objc_class_gsv1
{
	/**
	 * Pointer to the metaclass for this class.  The metaclass defines the
	 * methods use when a message is sent to the class, rather than an
	 * instance.
	 */
	Class                      isa;
	/**
	 * Pointer to the superclass.  The compiler will set this to the name of
	 * the superclass, the runtime will initialize it to point to the real
	 * class.
	 */
	Class                      super_class;
	/**
	 * The name of this class.  Set to the same value for both the class and
	 * its associated metaclass.
	 */
	const char                *name;
	/**
	 * The version of this class.  This is not used by the language, but may be
	 * set explicitly at class load time.
	 */
	long                       version;
	/**
	 * A bitfield containing various flags.  See the objc_class_flags
	 * enumerated type for possible values.
	 */
	unsigned long              info;
	/**
	 * The size of this class.  For classes using the non-fragile ABI, the
	 * compiler will set this to a negative value The absolute value will be
	 * the size of the instance variables defined on just this class.  When
	 * using the fragile ABI, the instance size is the size of instances of
	 * this class, including any instance variables defined on superclasses.
	 *
	 * In both cases, this will be set to the size of an instance of the class
	 * after the class is registered with the runtime.
	 */
	long                       instance_size;
	/**
	 * Metadata describing the instance variables in this class.
	 */
	struct objc_ivar_list_gcc *ivars;
	/**
	 * Metadata for for defining the mappings from selectors to IMPs.  Linked
	 * list of method list structures, one per class and one per category.
	 */
	struct objc_method_list_gcc   *methods;
	/**
	 * The dispatch table for this class.  Intialized and maintained by the
	 * runtime.
	 */
	void                      *dtable;
	/**
	 * A pointer to the first subclass for this class.  Filled in by the
	 * runtime.
	 */
	Class                      subclass_list;
	/**
	 * A pointer to the next sibling class to this.  You may find all
	 * subclasses of a given class by following the subclass_list pointer and
	 * then subsequently following the sibling_class pointers in the
	 * subclasses.
	 */
	Class                      sibling_class;

	/**
	 * Metadata describing the protocols adopted by this class.  Not used by
	 * the runtime.
	 */
	struct objc_protocol_list *protocols;
	/**
	 * Linked list of extra data attached to this class.
	 */
	struct reference_list     *extra_data;
	/**
	* New ABI.  The following fields are only available with classes compiled to
	* support the new ABI.  You may test whether any given class supports this
	* ABI by using the CLS_ISNEW_ABI() macro.
	*/

	/**
	* The version of the ABI used for this class.  Zero indicates the ABI first
	* implemented by clang 1.0.  One indicates the presence of bitmaps
	* indicating the offsets of strong, weak, and unretained ivars.  Two
	* indicates that the new ivar structure is used.
	*/
	long                       abi_version;

	/**
	* Array of pointers to variables where the runtime will store the ivar
	* offset.  These may be used for faster access to non-fragile ivars if all
	* of the code is compiled for the new ABI.  Each of these pointers should
	* have the mangled name __objc_ivar_offset_value_{class name}.{ivar name}
	*
	* When using the compatible non-fragile ABI, this faster form should only be
	* used for classes declared in the same compilation unit.
	*
	* The compiler should also emit symbols of the form
	* __objc_ivar_offset_{class name}.{ivar name} which are pointers to the
	* offset values.  These should be emitted as weak symbols in every module
	* where they are used.  The legacy-compatible ABI uses these with a double
	* layer of indirection.
	*/
	int                      **ivar_offsets;
	/**
	* List of declared properties on this class (NULL if none).  This contains
	* the accessor methods for each property.
	*/
	struct objc_property_list_gsv1   *properties;

	/**
	 * GC / ARC ABI: Fields below this point only exist if abi_version is >= 1.
	 */

	/**
	 * The location of all strong pointer ivars declared by this class.
	 *
	 * If the low bit of this field is 0, then this is a pointer to an
	 * objc_bitfield structure.  If the low bit is 1, then the remaining 63
	 * bits are set, from low to high, for each ivar in the object that is a
	 * strong pointer.
	 */
	uintptr_t                  strong_pointers;
	/**
	 * The location of all zeroing weak pointer ivars declared by this class.
	 * The format of this field is the same as the format of the
	 * strong_pointers field.
	 */
	uintptr_t                  weak_pointers;
};


#include <assert.h>

/**
 * Metadata structure describing a method.
 */
// begin: objc_method
struct objc_method
{
	/**
	 * A pointer to the function implementing this method.
	 */
	IMP         imp;
	/**
	 * Selector used to send messages to this method.
	 */
	SEL         selector;
	/**
	 * The extended type encoding for this method.
	 */
	const char *types;
};
// end: objc_method

struct objc_method_gcc
{
	/**
	 * Selector used to send messages to this method.  The type encoding of
	 * this method should match the types field.
	 */
    // You think this selector pointer points to a selector, do you? Ha. Wrong.
    // It's actually a pointer to a C-string literal, and no one bothered to
    // document it anywhere.
	SEL         selector;
	/**
	 * The type encoding for this selector.  Used only for introspection, and
	 * only required because of the stupid selector handling in the old GNU
	 * runtime.  In future, this field may be reused for something else.
	 */
	const char *types;
	/**
	 * A pointer to the function implementing this method.
	 */
	IMP         imp;
};

/**
 * Method list.  Each class or category defines a new one of these and they are
 * all chained together in a linked list, with new ones inserted at the head.
 * When constructing the dispatch table, methods in the start of the list are
 * used in preference to ones at the end.
 */
// begin: objc_method_list
struct objc_method_list
{
	/**
	 * The next group of methods in the list.
	 */
	struct objc_method_list  *next;
	/**
	 * The number of methods in this list.
	 */
	int                       count;
	/**
	 * Sze of `struct objc_method`.  This allows runtimes downgrading newer
	 * versions of this structure.
	 */
	size_t                    size;
	/**
	 * An array of methods.  Note that the actual size of this is count.
	 */
	struct objc_method        methods[];
};
// end: objc_method_list

/**
 * Returns a pointer to the method inside the `objc_method` structure.  This
 * structure is designed to allow the compiler to add other fields without
 * breaking the ABI, so although the `methods` field appears to be an array
 * of `objc_method` structures, it may be an array of some future version of
 * `objc_method` structs, which have fields appended that this version of the
 * runtime does not know about.
 */
static inline struct objc_method *method_at_index(struct objc_method_list *l, int i)
{
	assert(l->size >= sizeof(struct objc_method));
	return (struct objc_method*)(((char*)l->methods) + (i * l->size));
}


struct objc_category_gcc
{
	/**
	 * The name of this category.
	 */
	const char                *name;
	/**
	 * The name of the class to which this category should be applied.
	 */
	const char                *class_name;
	/**
	 * The list of instance methods to add to the class.
	 */
	struct objc_method_list_gcc   *instance_methods;
	/**
	 * The list of class methods to add to the class.
	 */
	struct objc_method_list_gcc   *class_methods;
	/**
	 * The list of protocols adopted by this category.
	 */
	struct objc_protocol_list *protocols;
};


/**
 * Legacy version of the method list.
 */
struct objc_method_list_gcc
{
	/**
	 * The next group of methods in the list.
	 */
	struct objc_method_list_gcc *next;
	/**
	 * The number of methods in this list.
	 */
	int                       count;
	/**
	 * An array of methods.  Note that the actual size of this is count.
	 */
	struct objc_method_gcc methods[];
};


/**
 * Defines the module structures.
 *
 * When defining a new ABI, the
 */

/**
 * The symbol table for a module.  This structure references all of the
 * Objective-C symbols defined for a module, allowing the runtime to find and
 * register them.
 */
struct objc_symbol_table_abi_8
{
	/**
	 * The number of selectors referenced in this module.
	 */
	unsigned long  selector_count;
	/**
	 * An array of selectors used in this compilation unit.  SEL is a pointer
	 * type and this points to the first element in an array of selectors.
	 */
	SEL            selectors;
	/**
	 * The number of classes defined in this module.
	 */
	unsigned short class_count;
	/**
	 * The number of categories defined in this module.
	 */
	unsigned short category_count;
	/**
	 * A null-terminated array of pointers to symbols defined in this module.
	 * This contains class_count pointers to class structures, category_count
	 * pointers to category structures, and then zero or more pointers to
	 * static object instances.
	 *
	 * Current compilers only use this for constant strings.  The runtime
	 * permits other types.
	 */
	void           *definitions[];
};

/**
 * The module structure is passed to the __objc_exec_class function by a
 * constructor function when the module is loaded.
 *
 * When defining a new ABI version, the first two fields in this structure must
 * be retained.
 */
struct objc_module_abi_8
{
	/**
	 * The version of the ABI used by this module.  This is checked against the
	 * list of ABIs that the runtime supports, and the list of incompatible
	 * ABIs.
	 */
	unsigned long                   version;
	/**
	 * The size of the module.  This is used for sanity checking, to ensure
	 * that the compiler and runtime's idea of the module size match.
	 */
	unsigned long                   size;
	/**
	 * The full path name of the source for this module.  Not currently used
	 * for anything, could be used for debugging in theory, but duplicates
	 * information available from DWARF data, so probably won't.
	 */
	const char                     *name;
	/**
	 * A pointer to the symbol table for this compilation unit.
	 */
	struct objc_symbol_table_abi_8 *symbol_table;
};

struct objc_module_abi_10
{
	/**
	 * Inherited fields from version 8 of the ABI.
	 */
	struct objc_module_abi_8 old;
	/**
	 * GC mode.  GC_Optional code can be mixed with anything, but GC_None code
	 * can't be mixed with GC_Required code.
	 */
	int gc_mode;
};

/**
 * List of static instances of a named class provided in this module.
 */
struct objc_static_instance_list
{
	/**
	 * The name of the class.  The isa pointer of all of the instances will be
	 * set to the class with this name.
	 */
	char *class_name;
	/**
	 * NULL-terminated array of statically-allocated instances.
	 */
	id    instances[];
};


/**
 * Structure used to store selectors in the list.
 */
struct objc_selector
{
	union
	{
		/**
		 * The name of this selector.  Used for unregistered selectors.
		 */
		const char *name;
		/**
		 * The index of this selector in the selector table.  When a selector
		 * is registered with the runtime, its name is replaced by an index
		 * uniquely identifying this selector.  The index is used for dispatch.
		 */
		uintptr_t index;
	};
	/**
	 * The Objective-C type encoding of the message identified by this selector.
	 */
	const char * types;
};


#ifndef __unsafe_unretained
#	ifndef __has_feature
#		define __unsafe_unretained
#	elif !__has_feature(objc_arc)
#		define __unsafe_unretained
#	endif
#endif


struct objc_super
{
	/** The receiver of the message. */
	__unsafe_unretained id receiver;
	/** The class containing the method to call. */
#	if !defined(__cplusplus)  &&  !__OBJC2__
	Class class;
#	else
	Class super_class;
#	endif
};
