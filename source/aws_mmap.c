
/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "aws/s3/private/aws_mmap.h"
#include <aws/common/allocator.h>

#ifdef _WIN32
#    include <windows.h>
#else
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif /* _WIN32 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
struct aws_mmap_context_win_impl {
    struct aws_allocator *allocator;

    HANDLE hFile;
    HANDLE hMapping;
    LPVOID lpMapAddress;
};

struct aws_mmap_context *s_mmap_context_destroy(struct aws_mmap_context *context) {
    if (!context) {
        return NULL;
    }
    struct aws_mmap_context_win_impl *impl = context->impl;
    if (impl->lpMapAddress) {
        UnmapViewOfFile(impl->lpMapAddress);
    }
    if (impl->hMapping) {
        CloseHandle(impl->hMapping);
    }
    if (impl->hFile) {
        CloseHandle(impl->hFile);
    }
    aws_mem_release(impl->allocator, context);
    return NULL;
}

struct aws_mmap_context *aws_mmap_context_release(struct aws_mmap_context *context) {
    return s_mmap_context_destroy(context);
}

struct aws_mmap_context *aws_mmap_context_new(struct aws_allocator *allocator, const char *file_name) {
    struct aws_mmap_context *context = NULL;
    struct aws_mmap_context_win_impl *impl = NULL;
    aws_mem_acquire_many(
        allocator, 2, &context, sizeof(struct aws_mmap_context), &impl, sizeof(struct aws_mmap_context_win_impl));

    impl->allocator = allocator;
    impl->hFile = CreateFile(
        file_name,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL /*SecurityAttributes*/,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL /*TemplateFile*/);
    if (impl->hFile == INVALID_HANDLE_VALUE) {
        goto error;
    }
    impl->hMapping = CreateFileMapping(impl->hFile, NULL, PAGE_READONLY, 0, 0, NULL /*Name*/);
    if (impl->hMapping == NULL) {
        goto error;
    }
    impl->lpMapAddress = MapViewOfFile(impl->hMapping, FILE_MAP_READ, 0, 0, 0);
    if (impl->lpMapAddress == NULL) {
        goto error;
    }

    context->content = (char *)impl->lpMapAddress;
    context->impl = impl;
    return context;
error:
    /* TODO: LOG and raise AWS ERRORs */
    return s_mmap_context_destroy(context);
}
#else

struct aws_mmap_context_posix_impl {
    struct aws_allocator *allocator;
    int fd;
};

struct aws_mmap_context *s_mmap_context_destroy(struct aws_mmap_context *context) {
    if (!context) {
        return NULL;
    }
    struct aws_mmap_context_posix_impl *impl = context->impl;
    if (impl->fd) {
        close(impl->fd);
    }
    aws_mem_release(impl->allocator, context);
    return NULL;
}

struct aws_mmap_context *aws_mmap_context_release(struct aws_mmap_context *context) {
    return s_mmap_context_destroy(context);
}

struct aws_mmap_context *aws_mmap_context_new(struct aws_allocator *allocator, const char *file_name) {
    struct aws_mmap_context *context = NULL;
    struct aws_mmap_context_posix_impl *impl = NULL;
    aws_mem_acquire_many(
        allocator, 2, &context, sizeof(struct aws_mmap_context), &impl, sizeof(struct aws_mmap_context_posix_impl));

    impl->fd = open(file_name, O_RDWR);
    impl->allocator = allocator;
    if (impl->fd == -1) {
        goto error;
    }

    struct stat file_stat;
    if (fstat(impl->fd, &file_stat) == -1) {
        goto error;
    }
    void *mapped_data = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, impl->fd, 0);
    if (mapped_data == MAP_FAILED) {
        goto error;
    }

    context->content = (char *)mapped_data;
    context->impl = impl;
    return context;
error:
    /* TODO: LOG and raise AWS ERRORs */
    return s_mmap_context_destroy(context);
}
#endif /* _WIN32 */
