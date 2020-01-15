#ifdef __TEST_H__
#error "The test header has been included more than once."
#endif
#define __TEST_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define __xconcat(x, y) x##y
#define __concat(x, y) __xconcat(x, y)
#define __new_symb(x) __concat(x, __COUNTER__)

// Color Macros
#define CSPEC_COLOR_RESET "\033[0m"
#define CSPEC_COLOR_RED "\033[0;31m"
#define CSPEC_COLOR_GREEN "\033[0;32m"
#define CSPEC_COLOR_CYAN "\033[0;36m"

/*****************************************************************************/
/*                                 Test Case                                 */
/*****************************************************************************/
typedef struct _cspec_test_case {
  const char *desc;
  const char *filename;
  const int line;

  int (*run)(void);

  int status;

  struct _cspec_test_case *next;
} CSpecTestCase;

/*****************************************************************************/
/*                                Test Suite                                 */
/*****************************************************************************/

typedef struct _cspec_test_suite {
  const char *desc;

  CSpecTestCase *tests_head;
  CSpecTestCase *tests_tail;

  struct _cspec_test_suite *next;
} CSpecTestSuite;

static CSpecTestSuite cspec_global_tests = {
    .desc = 0, .tests_head = 0, .tests_tail = 0, .next = 0};

static CSpecTestSuite *cspec_test_suites_head = &cspec_global_tests;
static CSpecTestSuite *cspec_test_suites_tail = &cspec_global_tests;

/*****************************************************************************/
/*                             Test Suite Methods                            */
/*****************************************************************************/

static inline void cspec_register_test_suite(CSpecTestSuite *node) {
  cspec_test_suites_tail->next = node;
  cspec_test_suites_tail = node;
}

#define __SUITE__ __new_symb(suite)

#define __init_test_suite(__name, __desc)                                      \
  static CSpecTestSuite __name = {                                             \
      .desc = __desc, .tests_head = 0, .tests_tail = 0, .next = 0};            \
  static inline void __attribute__((constructor))                              \
      __concat(register_suite_, __name)(void) {                                \
    cspec_register_test_suite(&__name);                                        \
  }

#define describe(desc, body) __init_test_suite(__SUITE__, desc) body

/*****************************************************************************/
/*                             Test Case Methods                             */
/*****************************************************************************/

static inline void cspec_register_test_case(CSpecTestCase *node) {
  if (cspec_test_suites_tail->tests_tail) {
    cspec_test_suites_tail->tests_tail->next = node;
    cspec_test_suites_tail->tests_tail = node;
  } else {
    cspec_test_suites_tail->tests_tail = node;
    cspec_test_suites_tail->tests_head = node;
  }
}

#define __TEST__ __new_symb(suite)

#define __init_test_case(__name, __desc)                                       \
  static inline int __concat(run_test_, __name)(void);                         \
  static CSpecTestCase __name = {.desc = __desc,                               \
                                 .next = 0,                                    \
                                 .run = __concat(run_test_, __name),           \
                                 .filename = __FILE__,                         \
                                 .line = __LINE__};                            \
  static inline void __attribute__((constructor))                              \
      __concat(register_test_, __name)(void) {                                 \
    cspec_register_test_case(&__name);                                         \
  }                                                                            \
  static inline int __concat(run_test_, __name)(void)

#define it(desc, body)                                                         \
  __init_test_case(__TEST__, desc) {                                           \
    body;                                                                      \
    return 0;                                                                  \
  }

/*****************************************************************************/
/*                                    Main                                   */
/*****************************************************************************/

int main(int argc, char *argv[]) {
  int nerrors = 0;
  int nfailures = 0;
  int ntests = 0;

  for (CSpecTestSuite *cur = cspec_test_suites_head; cur; cur = cur->next) {
    for (CSpecTestCase *test = cur->tests_head; test; test = test->next) {
      int pid = fork();
      if (-1 == pid) {
        // fork failed
        puts("Fork failed");
      } else if (0 == pid) {
        // we are in the child process
        int result = test->run();
        exit(result);
      } else {

        // we are in the parent process

        // Wait for child to finish
        int status;
        waitpid(pid, &status, 0);
        // Figure out if it crashed, failed, or passed
        if (!WIFEXITED(status)) {
          nerrors++;
          ntests++;
          printf(CSPEC_COLOR_RED "E" CSPEC_COLOR_RESET);
          test->status = -1;
        } else if (WEXITSTATUS(status)) {
          nfailures++;
          ntests++;

          printf(CSPEC_COLOR_RED "F" CSPEC_COLOR_RESET);
          test->status = 1;
        } else {
          ntests++;
          printf(CSPEC_COLOR_GREEN "." CSPEC_COLOR_RESET);
          test->status = 0;
        }
        // We have to flush stdout because of all the forking
        fflush(stdout);
      }
    }
  }
  putchar('\n');

  // If there are any failures, print their output
  if (nfailures) {
    printf("\nFailures:\n\n");

    int counter = 0;
    for (CSpecTestSuite *cur = cspec_test_suites_head; cur; cur = cur->next) {
      for (CSpecTestCase *test = cur->tests_head; test; test = test->next) {
        if (1 == test->status) {
          printf("%4d) %s %s\n", ++counter, cur->desc ? cur->desc : "It",
                 test->desc);
        }
      }
    }
  }

  if (nerrors) {
    printf("\nErrors:\n\n");

    int counter = 0;
    for (CSpecTestSuite *cur = cspec_test_suites_head; cur; cur = cur->next) {
      for (CSpecTestCase *test = cur->tests_head; test; test = test->next) {
        if (-1 == test->status) {
          printf("%4d) %s %s\n", ++counter, cur->desc ? cur->desc : "It",
                 test->desc);
          printf(CSPEC_COLOR_CYAN "      # ./%s:%d\n\n" CSPEC_COLOR_RESET,
                 test->filename, test->line);
        }
      }
    }
  }

  // Finish up with a quick summary
  putchar('\n');
  if (nerrors || nfailures)
    printf(CSPEC_COLOR_RED);
  else
    printf(CSPEC_COLOR_GREEN);

  printf("%d examples, %d failures, %d errors" CSPEC_COLOR_RESET "\n", ntests,
         nfailures, nerrors);
}
