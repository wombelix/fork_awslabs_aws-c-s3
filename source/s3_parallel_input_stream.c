/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#define _GNU_SOURCE
#include "aws/s3/private/s3_parallel_input_stream.h"

#include <aws/common/file.h>
#include <aws/common/clock.h>

#include <aws/io/future.h>
#include <aws/io/stream.h>

#include <inttypes.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#include <unistd.h>

void aws_parallel_input_stream_init_base(
    struct aws_parallel_input_stream *stream,
    struct aws_allocator *alloc,
    const struct aws_parallel_input_stream_vtable *vtable,
    void *impl) {

    AWS_ZERO_STRUCT(*stream);
    stream->alloc = alloc;
    stream->vtable = vtable;
    stream->impl = impl;
    aws_ref_count_init(&stream->ref_count, stream, (aws_simple_completion_callback *)vtable->destroy);
}

struct aws_parallel_input_stream *aws_parallel_input_stream_acquire(struct aws_parallel_input_stream *stream) {
    if (stream != NULL) {
        aws_ref_count_acquire(&stream->ref_count);
    }
    return stream;
}

struct aws_parallel_input_stream *aws_parallel_input_stream_release(struct aws_parallel_input_stream *stream) {
    if (stream != NULL) {
        aws_ref_count_release(&stream->ref_count);
    }
    return NULL;
}

struct aws_future_bool *aws_parallel_input_stream_read(
    struct aws_parallel_input_stream *stream,
    uint64_t offset,
    struct aws_byte_buf *dest) {
    /* Ensure the buffer has space available */
    if (dest->len == dest->capacity) {
        struct aws_future_bool *future = aws_future_bool_new(stream->alloc);
        aws_future_bool_set_error(future, AWS_ERROR_SHORT_BUFFER);
        return future;
    }
    struct aws_future_bool *future = stream->vtable->read(stream, offset, dest);
    return future;
}

struct aws_parallel_input_stream_from_file_impl {
    struct aws_parallel_input_stream base;

    struct aws_string *file_path;
    int fd;
};

static void s_para_from_file_destroy(struct aws_parallel_input_stream *stream) {
    struct aws_parallel_input_stream_from_file_impl *impl = stream->impl;

    aws_string_destroy(impl->file_path);

    aws_mem_release(stream->alloc, impl);
}

struct aws_future_bool *s_para_from_file_read(
    struct aws_parallel_input_stream *stream,
    uint64_t offset,
    struct aws_byte_buf *dest) {

    struct aws_future_bool *future = aws_future_bool_new(stream->alloc);
    struct aws_parallel_input_stream_from_file_impl *impl = stream->impl;
    bool success = false;
    struct aws_input_stream *file_stream = NULL;
    struct aws_stream_status status = {
        .is_end_of_stream = false,
        .is_valid = true,
    };
    uint64_t start;
    if (aws_high_res_clock_get_ticks(&start)) {
    }
    //file_stream = aws_input_stream_new_from_file(stream->alloc, aws_string_c_str(impl->file_path));
    if (!file_stream) {
        goto done;
    }

//    if (aws_input_stream_seek(file_stream, offset, AWS_SSB_BEGIN)) {
//        goto done;
//    }
    /* Keep reading until fill the buffer.
     * Note that we must read() after seek() to determine if we're EOF, the seek alone won't trigger it. */
    ssize_t bytes_read = pread(impl->fd, dest->buffer+dest->len, dest->capacity, (off_t) offset);
    if(bytes_read<0) {
        goto done;
    }
//    while ((dest->len < dest->capacity) && !status.is_end_of_stream) {
//        /* Read from stream */
//
//        if (aws_input_stream_read(file_stream, dest) != AWS_OP_SUCCESS) {
//            goto done;
//        }
//
//        /* Check if stream is done */
//        if (aws_input_stream_get_status(file_stream, &status) != AWS_OP_SUCCESS) {
//            goto done;
//        }
//    }
    success = true;
done:
    ;
    uint64_t end;
    if (aws_high_res_clock_get_ticks(&end)) {
    }
    uint64_t nanos = 0;
    if (aws_sub_u64_checked(end, start, &nanos)) {
    }
    uint64_t milis = aws_timestamp_convert(nanos, AWS_TIMESTAMP_NANOS, AWS_TIMESTAMP_MILLIS, NULL);
    AWS_LOGF_ERROR(AWS_LS_S3_META_REQUEST, "waahm7: %" PRIu64 "\n", milis);
    if (success) {
        aws_future_bool_set_result(future, status.is_end_of_stream);
    } else {
        aws_future_bool_set_error(future, aws_last_error());
    }

    aws_input_stream_release(file_stream);

    return future;
}

static struct aws_parallel_input_stream_vtable s_parallel_input_stream_from_file_vtable = {
    .destroy = s_para_from_file_destroy,
    .read = s_para_from_file_read,
};

struct aws_parallel_input_stream *aws_parallel_input_stream_new_from_file(
    struct aws_allocator *allocator,
    struct aws_byte_cursor file_name) {

    struct aws_parallel_input_stream_from_file_impl *impl =
        aws_mem_calloc(allocator, 1, sizeof(struct aws_parallel_input_stream_from_file_impl));
    aws_parallel_input_stream_init_base(&impl->base, allocator, &s_parallel_input_stream_from_file_vtable, impl);
    impl->file_path = aws_string_new_from_cursor(allocator, &file_name);
    impl->fd = open(file_name.ptr, O_RDONLY | O_DIRECT);
    if (!aws_path_exists(impl->file_path)) {
        /* If file path not exists, raise error from errno. */
        aws_translate_and_raise_io_error(errno);
        goto error;
    }
    return &impl->base;
error:
    s_para_from_file_destroy(&impl->base);
    return NULL;
}
