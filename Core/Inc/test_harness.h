#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdint.h>

void test_suite_run(void);
void test_suite_run_one(unsigned index);
void test_harness_pass(void);
void test_harness_fail(const char *reason);
void test_harness_assert_eq_impl(intmax_t a, const char *a_str, intmax_t b, const char *b_str);
void test_harness_assert_neq_impl(intmax_t a, const char *a_str, intmax_t b, const char *b_str);

#define autotest_assert(cond) test_harness_assert_neq_impl((cond), #cond, 0, "0")
#define autotest_assert_eq(a, b) test_harness_assert_eq_impl((a), #a, (b), #b)
#define autotest_assert_neq(a, b) test_harness_assert_neq_impl((a), #a, (b), #b)
#define autotest_pass() test_harness_pass()
#define autotest_fail(reason) test_harness_fail(reason)

#endif /* TEST_HARNESS_H */
