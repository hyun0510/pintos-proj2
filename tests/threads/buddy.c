#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void test_buddy (void) 
{
  palloc_set_mode (PAL_BUDDY);

  void *p = palloc_get_multiple (0, 3);
  msg("3 pages allocation");
  size_t idx = palloc_get_page_index (p);
  if (idx % 4 == 0)
    msg ("PASS: 4pages alloction success");
  else
    msg ("FAIL");   
  palloc_free_multiple (p, 4); 
  msg("--------merge test--------");
  //merge check
  void *a = palloc_get_multiple (0, 16);
  void *b = palloc_get_multiple (0, 16);
  size_t idx_a = palloc_get_page_index (a);
  size_t idx_b = palloc_get_page_index (b);
  msg("two 16pages allcation -> A, B");
  msg ("Allocated A at %zu, B at %zu", idx_a, idx_b);
  
  palloc_free_multiple (a, 16);
  palloc_free_multiple (b, 16);
  msg("free A,B");
  
  void *c = palloc_get_multiple (0, 32);
  size_t idx_c = palloc_get_page_index (c);
  msg("32pages allocation");
  size_t min_idx = (idx_a < idx_b) ? idx_a : idx_b;
  
  if (idx_c == min_idx) {
    msg ("PASS: Merge success C starts at %zu -> same as previous A/B", idx_c);
  } else {
    msg ("FAIL");
  }
  palloc_free_multiple (c, 32);
}
