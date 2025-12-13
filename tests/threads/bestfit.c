#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void test_bestfit (void) 
{
  palloc_set_mode (PAL_BEST_FIT);
  
  void *a = palloc_get_multiple(0, 20); 
  void *b = palloc_get_multiple(0, 5);  
  void *c = palloc_get_multiple(0, 5); 
  void *d = palloc_get_multiple(0, 5);  

  palloc_free_multiple(a, 20);
  palloc_free_multiple(c, 5); 
  
  void *new = palloc_get_multiple(0, 4);
  size_t new_idx = palloc_get_page_index(new);
  size_t c_idx = palloc_get_page_index(c); 
  
  if (new_idx == c_idx) {
        printf("PASS\n");
  } else {
        printf("FAIL");
  }

  palloc_free_multiple(b, 5);
  palloc_free_multiple(d, 5);
  palloc_free_multiple(new, 4);
}
