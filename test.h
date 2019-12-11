#ifdef __TEST_H__
#error "The test header has been included more than once."
#endif
#define __TEST_H__

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int (*test_t)(int);

static const int TBUFFER_SIZE = 512;

static const int TPASSED = 0;
static const int TFAILED = -1;
static const int TERROR = -2;

typedef struct _test_case {
  test_t func;
  int status;
  const char *name;
  int pipe[2];
  char output[TBUFFER_SIZE];
} test_case_t;

typedef struct _test_case_list {
  test_case_t *item;
  struct _test_case_list *next;
} test_case_list_t;

typedef struct _test_suite {
  test_case_list_t *head;
  test_case_list_t *tail;
} test_suite;

static struct _test_suite suite = {.head = NULL, .tail = NULL};

static inline void register_test_case(test_case_list_t *list) {
  if (suite.tail) {
    suite.tail->next = list;
    suite.tail = list;
  } else {
    suite.head = list;
    suite.tail = list;
  }
}

#define START_TEST(tfunc)                                                      \
  static inline int tfunc(int);                                                \
  static test_case_t tfunc##_test_case = {.func = tfunc, .name = #tfunc};      \
  static test_case_list_t tfunc##_test_case_node = {                           \
      .item = &tfunc##_test_case, .next = NULL};                               \
  static inline void __attribute__((constructor))                              \
      register_test_##tfunc(void) {                                            \
    register_test_case(&tfunc##_test_case_node);                               \
  }                                                                            \
  static inline int tfunc(int fd) {                                            \

#define END_TEST                                                               \
  return TPASSED;                                                              \
  }

int main(int argc, char *argv[]) {
  for (test_case_list_t *cur = suite.head; cur; cur = cur->next) {
    test_case_t *test = cur->item;

    fflush(stdout);
    pipe(test->pipe);

    int pid = fork();
    if (0 == pid) {
      // We are in the child process
      close(test->pipe[0]);
      exit(test->func(test->pipe[1]));
    } else if (-1 == pid) {
      // The fork failed
      perror("Could not start child process");
    } else {
      // We are in the parent process

      close(test->pipe[1]);
      read(test->pipe[0], test->output, TBUFFER_SIZE);
      close(test->pipe[0]);

      int status;
      waitpid(pid, &status, 0);
      if ((test->status = status)) {
        printf("\x1b[31mF\x1b[0m");
      } else {
        printf("\x1b[32m.\x1b[0m");
      }
    }
  }

  putchar('\n');

  for (test_case_list_t *cur = suite.head; cur; cur = cur->next) {
    if (TPASSED != cur->item->status)
      printf("\x1b[31m[FAIL]: %s\n%s\x1b[0m\n",
	     cur->item->name,
             cur->item->output);
  }
  return 0;
}
