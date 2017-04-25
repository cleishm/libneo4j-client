#include <check.h>

TCase* buffering_iostream_tcase(void);
TCase* chunking_iostream_tcase(void);
TCase* config_tcase(void);
TCase* connection_tcase(void);
TCase* deserialization_tcase(void);
TCase* dotdir_tcase(void);
TCase* error_handling_tcase(void);
TCase* logging_tcase(void);
TCase* memory_tcase(void);
TCase* render_plan_tcase(void);
TCase* render_results_tcase(void);
TCase* result_stream_tcase(void);
TCase* ring_buffer_tcase(void);
TCase* serialization_tcase(void);
TCase* tofu_tcase(void);
TCase* u8_tcase(void);
TCase* uri_tcase(void);
TCase* util_tcase(void);
TCase* values_tcase(void);
TCase* openssl_tcase(void);

Suite *libneo4j_client_suite(void)
{
    Suite *s = suite_create("libneo4j-client");
    suite_add_tcase(s, buffering_iostream_tcase());
    suite_add_tcase(s, chunking_iostream_tcase());
    suite_add_tcase(s, config_tcase());
    suite_add_tcase(s, connection_tcase());
    suite_add_tcase(s, deserialization_tcase());
    suite_add_tcase(s, dotdir_tcase());
    suite_add_tcase(s, error_handling_tcase());
    suite_add_tcase(s, logging_tcase());
    suite_add_tcase(s, memory_tcase());
    suite_add_tcase(s, render_plan_tcase());
    suite_add_tcase(s, render_results_tcase());
    suite_add_tcase(s, result_stream_tcase());
    suite_add_tcase(s, ring_buffer_tcase());
    suite_add_tcase(s, serialization_tcase());
    suite_add_tcase(s, tofu_tcase());
    suite_add_tcase(s, u8_tcase());
    suite_add_tcase(s, uri_tcase());
    suite_add_tcase(s, util_tcase());
    suite_add_tcase(s, values_tcase());
    suite_add_tcase(s, openssl_tcase());
    return s;
}
