/* vi:set ts=4 sw=4 expandtab:
 *
 * Copyright 2016, Chris Leishman (http://github.com/cleishm)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "../config.h"
#include "../src/lib/neo4j-client.h"
#include <check.h>
#include <errno.h>
#include <unistd.h>


#define assert_parsed(expected,s,n,complete) do { \
    char actual[(n)+1]; \
    memcpy(actual, (s), (n)); \
    actual[n] = '\0'; \
    ck_assert_str_eq(actual, expected); \
    ck_assert_msg((complete) == true, \
            "Expected a complete parse, but was incomplete"); \
} while (0)


START_TEST (parse_empty_input)
{
    bool complete;
    size_t consumed = neo4j_cli_parse("", NULL, NULL, &complete);
    ck_assert_int_eq(consumed, 0);
    ck_assert(complete == false);

    consumed = neo4j_cli_parse("     ", NULL, NULL, &complete);
    ck_assert_int_eq(consumed, 5);
    ck_assert(complete == false);

    const char *s;
    size_t l;
    consumed = neo4j_cli_parse("     ", &s, &l, &complete);
    ck_assert_int_eq(consumed, 5);
    ck_assert_int_eq(l, 0);
    ck_assert(complete == false);
}
END_TEST


START_TEST (parse_single_statement)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(";", &s, &l, &complete);
    ck_assert_int_eq(consumed, 1);
    assert_parsed("", s, l, complete);

    consumed = neo4j_cli_parse("       ;", &s, &l, &complete);
    ck_assert_int_eq(consumed, 8);
    assert_parsed("", s, l, complete);

    consumed = neo4j_cli_parse("match (n) return n;", &s, &l, &complete);
    ck_assert_int_eq(consumed, 19);
    assert_parsed("match (n) return n", s, l, complete);

    consumed = neo4j_cli_parse("  return 1   ;     ", &s, &l, &complete);
    ck_assert_int_eq(consumed, 14);
    assert_parsed("return 1", s, l, complete);
}
END_TEST


START_TEST (parse_single_multiline_statement)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(
            "  match (n)\n"
            "where n.foo\n"
            "return n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 33);
    assert_parsed(
            "match (n)\n"
            "where n.foo\n"
            "return n", s, l, complete);
}
END_TEST


START_TEST (parse_statement_with_quoted_strings)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(
            "match (n)\n"
            "where n.foo = \"testing;double\"\n"
            "return n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 50);
    assert_parsed(
            "match (n)\n"
            "where n.foo = \"testing;double\"\n"
            "return n", s, l, complete);

    consumed = neo4j_cli_parse(
            "match (n)\n"
            "where n.foo = \'testing;single\'\n"
            "return 'hello;world';\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 62);
    assert_parsed(
            "match (n)\n"
            "where n.foo = \'testing;single\'\n"
            "return 'hello;world'", s, l, complete);

    consumed = neo4j_cli_parse("  not valid \\; cypher; ", &s, &l, &complete);
    ck_assert_int_eq(consumed, 22);
    assert_parsed("not valid \\; cypher", s, l, complete);
}
END_TEST


START_TEST (parse_statement_with_line_comments)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(
            "// first line comment\n"
            "return n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 31);
    assert_parsed("return n", s, l, complete);

    consumed = neo4j_cli_parse(
            "// first line comment;\n"
            " return n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 33);
    assert_parsed("return n", s, l, complete);

    consumed = neo4j_cli_parse(
            "match (n)\n"
            "// middle comment;\n"
            "return n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 38);
    assert_parsed(
            "match (n)\n"
            "// middle comment;\n"
            "return n", s, l, complete);
}
END_TEST


START_TEST (parse_statement_with_block_comments)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(
            "/* first line comment\n"
            "continued on second line*/\n"
            "return n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 58);
    assert_parsed("return n", s, l, complete);

    consumed = neo4j_cli_parse(
            "/* first line comment\n"
            "continued*/ return n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 43);
    assert_parsed("return n", s, l, complete);

    consumed = neo4j_cli_parse(
            "return /* middle comment\n"
            "continued*/ n;\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 39);
    assert_parsed(
            "return /* middle comment\n"
            "continued*/ n", s, l, complete);
}
END_TEST


START_TEST (parse_multiple_statements)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse("return 1; return 2; return 3;",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 9);
    assert_parsed("return 1", s, l, complete);

    consumed = neo4j_cli_parse(" return \n1; return 2;\nreturn 3;",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 11);
    assert_parsed("return \n1", s, l, complete);
}
END_TEST


START_TEST (parse_single_command)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(":schema\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 8);
    assert_parsed(":schema", s, l, complete);

    consumed = neo4j_cli_parse(" :schema        \n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 17);
    assert_parsed(":schema", s, l, complete);

    consumed = neo4j_cli_parse(":   schema      \n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 17);
    assert_parsed(":   schema", s, l, complete);

    consumed = neo4j_cli_parse(":connect 'neo4j://localhost'\n",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 29);
    assert_parsed(":connect 'neo4j://localhost'", s, l, complete);
}
END_TEST


START_TEST (parse_command_with_line_comments)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(
            "// first line comment\n"
            ":schema\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 30);
    assert_parsed(":schema", s, l, complete);

    consumed = neo4j_cli_parse(":schema  // the schema\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 23);
    assert_parsed(":schema", s, l, complete);
}
END_TEST


START_TEST (parse_command_with_block_comments)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(
            "/* first line comment\n"
            "continued */\n"
            ":schema\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 43);
    assert_parsed(":schema", s, l, complete);

    consumed = neo4j_cli_parse(":schema  /* run schema\n */\n",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 27);
    assert_parsed(":schema", s, l, complete);

    consumed = neo4j_cli_parse(":schema /* the schema /* foo\n",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 29);
    assert_parsed(":schema /* the schema /* foo", s, l, complete);
}
END_TEST


START_TEST (parse_multiple_commands)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(":schema\n:stats\n",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 8);
    assert_parsed(":schema", s, l, complete);

    consumed = neo4j_cli_parse(":schema \nmatch (n) return n;\n",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 9);
    assert_parsed(":schema", s, l, complete);
}
END_TEST


START_TEST (parse_quoted_command)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse(":foo \"bar\" \nreturn 1;\n",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 12);
    assert_parsed(":foo \"bar\"", s, l, complete);

    consumed = neo4j_cli_parse(":foo \"bar\nreturn 1;\n",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 10);
    assert_parsed(":foo \"bar", s, l, complete);
}
END_TEST


START_TEST (parse_incomplete_query)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_parse("  return\n", &s, &l, &complete);
    ck_assert_int_eq(consumed, 9);
    ck_assert_msg(l == 7 && strncmp("return\n", s, 7) == 0,
            "Did not return start of query");
    ck_assert(complete == false);
}
END_TEST


START_TEST (parse_from_stream)
{
    int fds[2];
    ck_assert(pipe(fds) == 0);
    FILE *in = fdopen(fds[0], "r");
    ck_assert(in != NULL);
    FILE *out = fdopen(fds[1], "w");
    ck_assert(out != NULL);

    fprintf(out, "match (n) return n;\n");
    fprintf(out, "  return 'hello world';\n");
    fprintf(out, ":schema \n");
    fprintf(out, "return 'hello';return 'goodbye'");
    fclose(out);

    char *buf = NULL;
    size_t bufcap = 0;
    char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_fparse(in, &buf, &bufcap, &s, &l, &complete);
    ck_assert_int_eq(consumed, 19);
    assert_parsed("match (n) return n", s, l, complete);
    ck_assert_ptr_ne(buf, NULL);
    ck_assert(bufcap > 0);

    consumed = neo4j_cli_fparse(in, &buf, &bufcap, &s, &l, &complete);
    ck_assert_int_eq(consumed, 24);
    assert_parsed("return 'hello world'", s, l, complete);

    consumed = neo4j_cli_fparse(in, &buf, &bufcap, &s, &l, &complete);
    ck_assert_int_eq(consumed, 10);
    assert_parsed(":schema", s, l, complete);

    consumed = neo4j_cli_fparse(in, &buf, &bufcap, &s, &l, &complete);
    ck_assert_int_eq(consumed, 15);
    assert_parsed("return 'hello'", s, l, complete);

    consumed = neo4j_cli_fparse(in, &buf, &bufcap, &s, &l, &complete);
    ck_assert_int_eq(consumed, 16);

    ck_assert_msg(l == 16 && strncmp("return 'goodbye'", s, 16) == 0,
            "Did not return start of query");
    ck_assert(complete == false);

    fclose(in);
    free(buf);
}
END_TEST


START_TEST (parse_empty_args)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_arg_parse("", &s, &l, &complete);
    ck_assert_int_eq(consumed, 0);
    ck_assert_int_eq(l, 0);
    ck_assert(complete == false);

    consumed = neo4j_cli_arg_parse("     ", &s, &l, &complete);
    ck_assert_int_eq(consumed, 5);
    ck_assert_int_eq(l, 0);
    ck_assert(complete == false);
}
END_TEST


START_TEST (parse_arg)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_arg_parse("abc def", &s, &l, &complete);
    ck_assert_int_eq(consumed, 4);
    assert_parsed("abc", s, l, complete);

    consumed = neo4j_cli_arg_parse("abc\ndef", &s, &l, &complete);
    ck_assert_int_eq(consumed, 4);
    assert_parsed("abc", s, l, complete);

    consumed = neo4j_cli_arg_parse("abc\\ def ghi", &s, &l, &complete);
    ck_assert_int_eq(consumed, 9);
    assert_parsed("abc\\ def", s, l, complete);
}
END_TEST


START_TEST (parse_quoted_arg)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_arg_parse("'abc def' ghi", &s, &l, &complete);
    ck_assert_int_eq(consumed, 10);
    assert_parsed("abc def", s, l, complete);

    consumed = neo4j_cli_arg_parse("'abc\ndef' ghi", &s, &l, &complete);
    ck_assert_int_eq(consumed, 10);
    assert_parsed("abc\ndef", s, l, complete);

    consumed = neo4j_cli_arg_parse("'abc\"def' ghi", &s, &l, &complete);
    ck_assert_int_eq(consumed, 10);
    assert_parsed("abc\"def", s, l, complete);

    consumed = neo4j_cli_arg_parse("\"abc def\" ghi", &s, &l, &complete);
    ck_assert_int_eq(consumed, 10);
    assert_parsed("abc def", s, l, complete);

    consumed = neo4j_cli_arg_parse("\"abc'def\" ghi", &s, &l, &complete);
    ck_assert_int_eq(consumed, 10);
    assert_parsed("abc'def", s, l, complete);

}
END_TEST


START_TEST (parse_arg_joined_to_quoted_arg)
{
    const char *s;
    size_t l;
    bool complete;
    size_t consumed = neo4j_cli_arg_parse("abc\"def ghi\" jkl",
            &s, &l, &complete);
    ck_assert_int_eq(consumed, 3);
    assert_parsed("abc", s, l, complete);
}
END_TEST


TCase* cli_parser_tcase(void)
{
    TCase *tc = tcase_create("cli_parser");
    tcase_add_test(tc, parse_empty_input);
    tcase_add_test(tc, parse_single_statement);
    tcase_add_test(tc, parse_single_multiline_statement);
    tcase_add_test(tc, parse_statement_with_quoted_strings);
    tcase_add_test(tc, parse_statement_with_line_comments);
    tcase_add_test(tc, parse_statement_with_block_comments);
    tcase_add_test(tc, parse_multiple_statements);
    tcase_add_test(tc, parse_single_command);
    tcase_add_test(tc, parse_command_with_line_comments);
    tcase_add_test(tc, parse_command_with_block_comments);
    tcase_add_test(tc, parse_multiple_commands);
    tcase_add_test(tc, parse_quoted_command);
    tcase_add_test(tc, parse_incomplete_query);
    tcase_add_test(tc, parse_from_stream);
    tcase_add_test(tc, parse_empty_args);
    tcase_add_test(tc, parse_arg);
    tcase_add_test(tc, parse_quoted_arg);
    tcase_add_test(tc, parse_arg_joined_to_quoted_arg);
    return tc;
}
