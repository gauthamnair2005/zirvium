/*
 * Zirvium Kernel Module Support
 * Module loading and management
 */

#ifndef _KERNEL_MODULE_H
#define _KERNEL_MODULE_H

#include <kernel/types.h>

/* Module initialization and cleanup */
typedef int (*module_init_t)(void);
typedef void (*module_exit_t)(void);

/* Module metadata */
#define MODULE_LICENSE(x) \
    static const char __module_license[] __attribute__((section(".modinfo"), used)) = "license=" x

#define MODULE_AUTHOR(x) \
    static const char __module_author[] __attribute__((section(".modinfo"), used)) = "author=" x

#define MODULE_DESCRIPTION(x) \
    static const char __module_description[] __attribute__((section(".modinfo"), used)) = "description=" x

#define MODULE_VERSION(x) \
    static const char __module_version[] __attribute__((section(".modinfo"), used)) = "version=" x

#define MODULE_ALIAS(x) \
    static const char __module_alias[] __attribute__((section(".modinfo"))) = "alias=" x

/* Module init/exit registration */
#ifdef MODULE
/* Loadable module version */
#define module_init(initfn) \
    static inline module_init_t __inittest(void) { return initfn; } \
    int init_module(void) __attribute__((alias(#initfn)));

#define module_exit(exitfn) \
    static inline module_exit_t __exittest(void) { return exitfn; } \
    void cleanup_module(void) __attribute__((alias(#exitfn)));
#else
/* Built-in module support (compiled into kernel) */
#define module_init(initfn) \
    static inline int __init_##initfn(void) { return initfn(); } \
    static int (* __initcall_##initfn)(void) __attribute__((__used__)) \
    __attribute__((__section__(".initcall"))) = __init_##initfn

#define module_exit(exitfn) \
    static inline void __exit_##exitfn(void) { exitfn(); } \
    static void (* __exitcall_##exitfn)(void) __attribute__((__used__)) \
    __attribute__((__section__(".exitcall"))) = __exit_##exitfn
#endif

/* Module parameters */
#define module_param(name, type, perm) \
    static type name

#define module_param_named(name, var, type, perm) \
    static type var

#define MODULE_PARM_DESC(name, desc) \
    static const char __param_desc_##name[] = desc

/* Module symbols export */
#define EXPORT_SYMBOL(sym) \
    extern typeof(sym) sym; \
    static const char __ksymtab_##sym[] \
    __attribute__((section("__ksymtab_strings"))) = #sym

#define EXPORT_SYMBOL_GPL(sym) EXPORT_SYMBOL(sym)

#endif /* _KERNEL_MODULE_H */
