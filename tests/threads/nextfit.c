#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void test_nextfit (void) 
{
  palloc_set_mode (PAL_NEXT_FIT);


  void *p1 = palloc_get_multiple(0, 5);

  void *p2 = palloc_get_multiple(0, 3);
  size_t idx2 = palloc_get_page_index(p2);
  palloc_free_multiple(p1, 5);

  void *p3 = palloc_get_multiple(0, 2);
  size_t idx3 = palloc_get_page_index(p3);
  if (idx3 > idx2) {
      printf("PASS\n");
  } else {
      printf("FAIL\n");
  }
  
  palloc_free_multiple(p2, 3);
  palloc_free_multiple(p3, 2);
}

