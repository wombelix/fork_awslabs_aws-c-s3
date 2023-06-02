/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#include <aws/testing/aws_test_harness.h>

#ifdef AWS_ENABLE_S3_ENDPOINT_RESOLVER
#    include <aws/s3/s3_endpoint_resolver/s3_endpoint_resolver.h>
#    include <aws/sdkutils/endpoints_rule_engine.h>

AWS_TEST_CASE(test_s3_endpoint_resolver_create_destroy, s_test_s3_endpoint_resolver_create_destroy)
static int s_test_s3_endpoint_resolver_create_destroy(struct aws_allocator *allocator, void *ctx) {
    (void)ctx;
    aws_s3_library_init(allocator);

    struct aws_s3_endpoint_resolver *resolver = aws_s3_endpoint_resolver_new(allocator);
    aws_s3_endpoint_resolver_release(resolver);

    aws_s3_library_clean_up();
    return 0;
}

AWS_TEST_CASE(test_s3_endpoint_resolver_resolve_endpoint, s_test_s3_endpoint_resolver_resolve_endpoint)
static int s_test_s3_endpoint_resolver_resolve_endpoint(struct aws_allocator *allocator, void *ctx) {
    (void)ctx;
    aws_s3_library_init(allocator);

    struct aws_s3_endpoint_resolver *resolver = aws_s3_endpoint_resolver_new(allocator);
    struct aws_endpoints_request_context *context = aws_endpoints_request_context_new(allocator);
    ASSERT_SUCCESS(aws_endpoints_request_context_add_string(
        allocator, context, aws_byte_cursor_from_c_str("Region"), aws_byte_cursor_from_c_str("us-west-2")));
    struct aws_endpoints_resolved_endpoint *resolved_endpoint =
        aws_s3_endpoint_resolver_resolve_endpoint(resolver, context);

    ASSERT_INT_EQUALS(AWS_ENDPOINTS_RESOLVED_ENDPOINT, aws_endpoints_resolved_endpoint_get_type(resolved_endpoint));

    struct aws_byte_cursor url_cur;
    ASSERT_SUCCESS(aws_endpoints_resolved_endpoint_get_url(resolved_endpoint, &url_cur));

    ASSERT_CURSOR_VALUE_CSTRING_EQUALS(url_cur, "https://s3.us-west-2.amazonaws.com");

    aws_endpoints_resolved_endpoint_release(resolved_endpoint);
    aws_endpoints_request_context_release(context);
    aws_s3_endpoint_resolver_release(resolver);

    aws_s3_library_clean_up();
    return 0;
}

#endif
