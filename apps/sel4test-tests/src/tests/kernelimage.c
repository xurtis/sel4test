/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include <vspace/vspace.h>

#include "../test.h"
#include "../helpers.h"
#include <utils/util.h>

#ifdef CONFIG_KERNEL_IMAGES

#define TEST_KIID 4

typedef struct vka_object_list vka_object_list_t;
struct vka_object_list {
    vka_object_t object;
    vka_object_list_t *next;
};

static inline void vka_object_list_push(vka_object_list_t **list, vka_object_t object)
{
    assert(list != NULL);

    vka_object_list_t *node = malloc(sizeof(*node));
    assert(node != NULL);

    *node = (vka_object_list_t) {
        .object = object,
        .next = *list,
    };

    *list = node;
}

static inline vka_object_t vka_object_list_pop(vka_object_list_t **list)
{
    assert(list != NULL);
    assert(*list != NULL);

    vka_object_list_t *head = *list;
    vka_object_t object = head->object;

    *list = head->next;
    free(head);

    return object;
}

typedef struct {
    vka_object_t image;
    vka_object_list_t *memories;
} kernel_image_alloc_t;

static inline int create_kernel_image(env_t env, kernel_image_alloc_t *alloc)
{
    int error;

    error = vka_alloc_kernel_image(&env->vka, &alloc->image);
    test_eq(error, 0);

    error = seL4_KIIDTable_Assign(env->kiid_table, alloc->image.cptr, TEST_KIID);
    test_eq(error == 0);

    alloc->memories = NULL;

    uint32_t mapped = 0;
    uint32_t level = 0;
    while (level < seL4_KernelImageNumLevels) {
        vka_object_t memory;
        error = vka_alloc_kernel_memory(&env->vka, seL4_GetKernelImageLevelSizeBits(level), &memory);
        test_eq(error, 0);

        error = api_kernel_memory_map(memory.cptr, alloc->image.cptr);
        test_eq(error, 0);

        vka_object_list_push(&alloc->memories, memory);

        mapped += 1;
        if (mapped >= env->kernel_image_level_count[level]) {
            mapped = 0;
            level += 1;
        }
    }

    return sel4test_get_result();
}

static inline int destroy_kernel_image(env_t env, kernel_image_alloc_t *alloc)
{
    int error;

    while (alloc->memories != NULL) {
        vka_object_t memory = vka_object_list_pop(&alloc->memories);
        error = api_kernel_memory_unmap(memory.cptr);
        test_eq(error, 0);
        vka_free_object(&env->vka, &memory);
    }

    vka_free_object(&env->vka, &alloc->image);

    return sel4test_get_result();
}

static int test_map_kernel_image(env_t env)
{
    int error;
    kernel_image_alloc_t image_alloc = {};

    error = create_kernel_image(env, &image_alloc);
    test_eq(error, 0);

    error = destroy_kernel_image(env, &image_alloc);
    test_eq(error, 0);

    return sel4test_get_result();
}
DEFINE_TEST(KERNELIMAGE0001, "Map a kernel image", test_map_kernel_image, config_set(CONFIG_KERNEL_IMAGES))

static int test_clone_kernel_image(env_t env)
{
    int error;
    kernel_image_alloc_t image_alloc = {};

    error = create_kernel_image(env, &image_alloc);
    test_eq(error, 0);

    error = api_kernel_image_clone(image_alloc.image.cptr, env->kernel_image);
    test_eq(error, 0);

    error = destroy_kernel_image(env, &image_alloc);
    test_eq(error, 0);

    return sel4test_get_result();
}
DEFINE_TEST(KERNELIMAGE0002, "Clone a kernel image", test_clone_kernel_image, config_set(CONFIG_KERNEL_IMAGES))

static int test_kernel_image_bind_vspace(env_t env)
{
    int error;
    kernel_image_alloc_t image_alloc = {};
    vka_object_t vspace = {};

    error = create_kernel_image(env, &image_alloc);
    test_eq(error, 0);

    error = vka_alloc_vspace_root(&env->vka, &vspace);
    test_eq(error, 0);

    error = seL4_ARCH_ASIDPool_Assign(env->asid_pool, vspace.cptr);
    test_eq(error, 0);

    error = api_kernel_image_clone(image_alloc.image.cptr, env->kernel_image);
    test_eq(error, 0);

    error = api_kernel_image_bind(image_alloc.image.cptr, vspace.cptr);
    test_eq(error, 0);

    vka_free_object(&env->vka, &vspace);

    error = destroy_kernel_image(env, &image_alloc);
    test_eq(error, 0);

    return sel4test_get_result();
}
DEFINE_TEST(KERNELIMAGE0003, "Bind a VSpace to a kernel image", test_kernel_image_bind_vspace,
            config_set(CONFIG_KERNEL_IMAGES))

#endif /* CONFIG_KERNEL_IMAGES */
