// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct kmem kmems[NCPU];

struct run* trypopr(int id){
  struct run *r;
  r = kmems[id].freelist;
  if(r)
    kmems[id].freelist = r->next;
  return r;
}

void trypushr(int id, struct run* r){
  if(r){
    r->next = kmems[id].freelist;
    kmems[id].freelist = r;
  }
  else
  {
    panic("cannot push null run");
  }
}

void
kinit()
{
  /* 初始化NCPU个🔒 */
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmems[i].lock, "kmem");
  }  
  //initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  
  push_off();
  int currentid = cpuid();
  
  acquire(&kmems[currentid].lock);
  trypushr(currentid, r);
  release(&kmems[currentid].lock); 
  
  pop_off();
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int issteal = 0;/** 标识是否为偷盗 */
  push_off();
  int currentid = cpuid();

  acquire(&kmems[currentid].lock);
  
  r = trypopr(currentid);
  /**
   * 将id的一块free page卸下，然后给currentid
   * 这个过程中经历了：
   * 
   * 1.卸下id的free page
   * 2.为current id的freelist添加该page
   * 3.将current id的freelist中的该page卸载掉
   * 4.返回该page
   * 
   * 整个过程完成了current id偷盗id的free page的行为
   */
  if(!r){
    //printf("oops out of memory\n");
    for (int id = 0; id < NCPU; id++)
    {
      /* steal first run */
      if(id != currentid){
        /** 锁住id的freelist，此时不让其他cpu访问  */
        if(kmems[id].freelist){
          acquire(&kmems[id].lock);
          /** 卸下id的free page */
          r = trypopr(id);
          /** 为currentid的freelist添加一个run */
          trypushr(currentid, r);

          issteal = 1;
          release(&kmems[id].lock);
          break;
        }
      } 
      //printf("\n");
    }
  }
  /** 如果是偷盗的，则把currentid的freelist释放出来  */
  if(issteal)
    r = trypopr(currentid);
  
  release(&kmems[currentid].lock);
  pop_off();
  
  if(r){
    //printf("currentid: %d, r: %p\n", currentid, r);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  /** 返回该page  */
  //printf("issteal: %d \n", issteal);
  return (void*)r;
}
