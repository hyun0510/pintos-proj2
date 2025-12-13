#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <list.h>

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */
#define BUDDY_MAX_ORDER 11
/* A memory pool. */
struct pool {
    struct lock lock;        /* Mutual exclusion. */
    struct bitmap *used_map; /* Bitmap of free pages. */
    uint8_t *base;           /* Base of pool. */
    
    size_t last_scan_idx;
    struct list buddy_list[BUDDY_MAX_ORDER + 1];
};

/* Two pools: one for kernel data, one for user pages. */
static struct pool kernel_pool, user_pool;

static void init_pool(struct pool *, void *base, size_t page_cnt,
                      const char *name);
static bool page_from_pool(const struct pool *, void *page);

/* Current allocation mode. */
static enum palloc_mode palloc_mode = PAL_FIRST_FIT;

/* Sets the allocation mode. */
void
palloc_set_mode (enum palloc_mode mode)
{
  palloc_mode = mode;
}

static size_t get_power(size_t page_cnt){
    size_t power = 0;
    size_t size = 1;
    while(size < page_cnt){
        size *= 2;
        power++;
    }
    return power;
}

static size_t get_buddy_idx(size_t page_idx, size_t power){
    /*size_t buddy_size = 1;
    for(int i = 0 ; i< power; i++){
        buddy_size *= 2;
    }
    size_t block_start = (page_idx/buddy_size) * buddy_size;
    if(page_idx < block_start + buddy_size){
        return block_start + buddy_size;
    }else{
        return block_start;
    }*/
    return page_idx ^ (1 << power);


}


/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void palloc_init(size_t user_page_limit)
{
    /* Free memory starts at 1 MB and runs to the end of RAM. */
    uint8_t *free_start = ptov(1024 * 1024);
    uint8_t *free_end = ptov(init_ram_pages * PGSIZE);
    size_t free_pages = (free_end - free_start) / PGSIZE;
    size_t user_pages = free_pages / 2;
    size_t kernel_pages;
    if (user_pages > user_page_limit)
        user_pages = user_page_limit;
    kernel_pages = free_pages - user_pages;

    /* Give half of memory to kernel, half to user. */
    init_pool(&kernel_pool, free_start, kernel_pages, "kernel pool");
    init_pool(&user_pool, free_start + kernel_pages * PGSIZE,
              user_pages, "user pool");
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_multiple(enum palloc_flags flags, size_t page_cnt)
{
    struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
    void *pages;
    size_t page_idx = BITMAP_ERROR;

    if (page_cnt == 0)
        return NULL;

    lock_acquire(&pool->lock);
    
    //add
    if(palloc_mode == PAL_BUDDY){
        size_t power = get_power(page_cnt);
        size_t scan_power = power;
        
        while(scan_power <= BUDDY_MAX_ORDER && list_empty(&pool->buddy_list[scan_power])){
            scan_power++;
        }
        if(scan_power <= BUDDY_MAX_ORDER){
            struct list_elem *e = list_pop_front(&pool->buddy_list[scan_power]);
            uint8_t *page_addr = (uint8_t *)e;
            page_idx = (page_addr - pool->base) / PGSIZE;
            
            while(scan_power > power){
                scan_power--;
                size_t buddy_size = 1;
                for(int i = 0; i<scan_power; i++){
                    buddy_size *= 2;
                }
                size_t buddy_idx = page_idx + buddy_size;
                
                void *buddy_addr = pool-> base + (buddy_idx * PGSIZE);
                struct list_elem *buddy_elem = (struct list_elem *)buddy_addr;
                list_push_back(&pool->buddy_list[scan_power], buddy_elem);                          
            }
            size_t cnt =1;
            for(int i = 0; i<power; i++){
                    cnt *= 2;
            }
            bitmap_set_multiple(pool->used_map, page_idx, cnt ,true);
        }
    }
    else{
        if(palloc_mode == PAL_FIRST_FIT){
            page_idx = bitmap_scan_and_flip(pool->used_map, 0, page_cnt, false);
        }
        else if(palloc_mode == PAL_NEXT_FIT){
            page_idx = bitmap_scan_and_flip_next(pool->used_map, 0, page_cnt, false, &pool->last_scan_idx);        
        }
        else if(palloc_mode == PAL_BEST_FIT){
            page_idx = bitmap_scan_and_flip_best(pool->used_map, 0, page_cnt, false);
        }
        else{
            page_idx = BITMAP_ERROR;
        }
            
    }
    
    lock_release(&pool->lock);

    if (page_idx != BITMAP_ERROR)
        pages = pool->base + PGSIZE * page_idx;
    else
        pages = NULL;

    if (pages != NULL) {
        if (flags & PAL_ZERO)
            memset(pages, 0, PGSIZE * page_cnt);
    } 
    else {
        if (flags & PAL_ASSERT)
            PANIC("palloc_get: out of pages");
    }

    return pages;
}

/* Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_page(enum palloc_flags flags)
{
    return palloc_get_multiple(flags, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
void palloc_free_multiple(void *pages, size_t page_cnt)
{
    struct pool *pool;
    size_t page_idx;

    ASSERT(pg_ofs(pages) == 0);
    if (pages == NULL || page_cnt == 0)
        return;

    if (page_from_pool(&kernel_pool, pages))
        pool = &kernel_pool;
    else if (page_from_pool(&user_pool, pages))
        pool = &user_pool;
    else
        NOT_REACHED();

    page_idx = pg_no(pages) - pg_no(pool->base);
    
    #ifndef NDEBUG
    memset(pages, 0xcc, PGSIZE * page_cnt);
    #endif
    //add
    lock_acquire(&pool->lock);
    
    if(palloc_mode == PAL_BUDDY){
        size_t power  = get_power(page_cnt);
        size_t current_idx = page_idx;
        size_t cnt =1 ;
        for(int i = 0; i<power; i++){
                cnt *= 2;
        }
        bitmap_set_multiple(pool->used_map, page_idx, cnt ,false);
        
        while(power < BUDDY_MAX_ORDER){
            size_t buddy_idx = get_buddy_idx(current_idx, power);
            if(buddy_idx + cnt > bitmap_size(pool->used_map)) break;
            if (bitmap_contains(pool->used_map, buddy_idx, cnt, true)) break;
            
            struct list_elem *buddy_elem = (struct list_elem *)(pool->base + buddy_idx * PGSIZE);
            bool found = false;
            
            if (!list_empty(&pool->buddy_list[power])) {
                struct list_elem *e;
                for (e = list_begin(&pool->buddy_list[power]); e != list_end(&pool->buddy_list[power]); e = list_next(e)) {
                    if (e == buddy_elem) {
                        found = true;
                        break;
                    }
                }
            }

            if (!found) break;
            list_remove(buddy_elem);
            
            if(buddy_idx < current_idx){
                current_idx = buddy_idx;
            }
            power++;
            cnt*= 2;
        }
        struct list_elem *new_elem = (struct list_elem *)(pool->base + current_idx * PGSIZE);
        list_push_back(&pool->buddy_list[power], new_elem);
        
    }
    else{
        ASSERT(bitmap_all(pool->used_map, page_idx, page_cnt));
        bitmap_set_multiple(pool->used_map, page_idx, page_cnt, false);
    }

    lock_release(&pool->lock);


    
}

/* Frees the page at PAGE. */
void palloc_free_page(void *page)
{
    palloc_free_multiple(page, 1);
}

/* Returns the index of the page in the pool's bitmap. */
size_t
palloc_get_page_index (void *page)
{
  struct pool *pool;

  if (page_from_pool (&kernel_pool, page))
    pool = &kernel_pool;
  else if (page_from_pool (&user_pool, page))
    pool = &user_pool;
  else
    return BITMAP_ERROR;

  return pg_no (page) - pg_no (pool->base);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void
init_pool(struct pool *p, void *base, size_t page_cnt, const char *name)
{
    /* We'll put the pool's used_map at its base.
     Calculate the space needed for the bitmap
     and subtract it from the pool's size. */
    size_t bm_pages = DIV_ROUND_UP(bitmap_buf_size(page_cnt), PGSIZE);
    if (bm_pages > page_cnt)
        PANIC("Not enough memory in %s for bitmap.", name);
    page_cnt -= bm_pages;

    printf("%zu pages available in %s.\n", page_cnt, name);

    /* Initialize the pool. */
    lock_init(&p->lock);
    p->used_map = bitmap_create_in_buf(page_cnt, base, bm_pages * PGSIZE);
    p->base = base + bm_pages * PGSIZE;
    
    //add
    p->last_scan_idx = 0;
    
    for(int i = 0 ; i<=BUDDY_MAX_ORDER; i++){
        list_init(&p->buddy_list[i]);
    }
    
    size_t current_idx = 0;
    size_t remaining_pages = page_cnt;
    
    while(remaining_pages > 0){
        size_t power = get_power(remaining_pages);
        size_t power_res = 1;
        for(int i = 0; i<power; i++){
                power_res *= 2;
        }
        while(power_res > remaining_pages){
            if(power == 0)break;
            power--;
            power_res = 1 << power;
        }
        struct list_elem *elem = (struct list_elem  *)(p->base + current_idx * PGSIZE);
        list_push_back(&p->buddy_list[power], elem);
        
        power_res = 1;
        for(int i = 0; i<power; i++){
                power_res *= 2;
        }
        size_t block_size = power_res;
        current_idx += block_size;
        remaining_pages -=block_size;
    }
    
    
    
}

/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
static bool
page_from_pool(const struct pool *pool, void *page)
{
    size_t page_no = pg_no(page);
    size_t start_page = pg_no(pool->base);
    size_t end_page = start_page + bitmap_size(pool->used_map);

    return page_no >= start_page && page_no < end_page;
}
