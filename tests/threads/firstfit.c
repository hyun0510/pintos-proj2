#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void
test_firstfit (void) 
{
  palloc_set_mode (PAL_FIRST_FIT);
  
  void *p = palloc_get_multiple(0,5);
  if(p){
      printf("PASS\n");
      palloc_free_multiple(p,5);
  }else{
      printf("FAIL\n");
  }
}
