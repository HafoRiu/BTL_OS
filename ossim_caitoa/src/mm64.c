/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "syscall.h"
#include "libmem.h"
#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,       // present
             addr_t fpn,    // FPN
             int drt,       // dirty
             int swp,       // swap
             int swptyp,    // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0)
  {
    if (swp == 0)
    { // Non swap ~ page online
      if (fpn == 0)
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table
 */
static int is_canonical64(addr_t addr)
{
  addr_t top = addr >> (PAGING64_ADDR_PGD_HIBIT + 1);
  return top == 0 || top == ((1ULL << (MM64_BITS_PER_LONG - PAGING64_ADDR_PGD_HIBIT - 1)) - 1);
}

static addr_t *alloc_pagetable_level(void)
{
  return calloc(PAGING64_MAX_PGN, sizeof(addr_t));
}

static addr_t *get_pte_ptr(struct mm_struct *mm, addr_t pgn, int create)
{
  addr_t pgd = 0, p4d = 0, pud = 0, pmd = 0, pt = 0;
  addr_t *level = NULL;
  addr_t *next = NULL;

  if (!mm)
    return NULL;

  if (get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt) != 0)
    return NULL;

  level = mm->pgd;
  if (!level)
    return NULL;

  next = (addr_t *)level[pgd];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[pgd] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  next = (addr_t *)level[p4d];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[p4d] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  next = (addr_t *)level[pud];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[pud] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  next = (addr_t *)level[pmd];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[pmd] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  return &level[pt];
}

/* Generic walker that starts from an explicit top-level page directory pointer.
 * Useful for walking kernel page tables stored in `krnl->krnl_pgd`.
 */
static addr_t *get_pte_ptr_root(addr_t *root, addr_t pgn, int create)
{
  addr_t pgd = 0, p4d = 0, pud = 0, pmd = 0, pt = 0;
  addr_t *level = NULL;
  addr_t *next = NULL;

  if (!root)
    return NULL;

  if (get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt) != 0)
    return NULL;

  level = root;
  next = (addr_t *)level[pgd];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[pgd] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  next = (addr_t *)level[p4d];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[p4d] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  next = (addr_t *)level[pud];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[pud] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  next = (addr_t *)level[pmd];
  if (!next && create) {
    next = alloc_pagetable_level();
    if (!next)
      return NULL;
    level[pmd] = (addr_t)next;
  }
  if (!next)
    return NULL;

  level = next;
  return &level[pt];
}

int get_pd_from_address(addr_t addr, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt)
{
  if (!is_canonical64(addr))
    return -1;

  /* Extract page directories */
  *pgd = (addr & PAGING64_ADDR_PGD_MASK) >> PAGING64_ADDR_PGD_LOBIT;
  *p4d = (addr & PAGING64_ADDR_P4D_MASK) >> PAGING64_ADDR_P4D_LOBIT;
  *pud = (addr & PAGING64_ADDR_PUD_MASK) >> PAGING64_ADDR_PUD_LOBIT;
  *pmd = (addr & PAGING64_ADDR_PMD_MASK) >> PAGING64_ADDR_PMD_LOBIT;
  *pt = (addr & PAGING64_ADDR_PT_MASK) >> PAGING64_ADDR_PT_LOBIT;

  /* TODO: implement the page directories mapping */

  return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table
 */
int get_pd_from_pagenum(addr_t pgn, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt)
{
  if (pgn >= PAGING64_MAX_PGN)
    return -1;

  return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                             pgd, p4d, pud, pmd, pt);
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
  // struct krnl_t *krnl = caller->krnl;
  struct krnl_t *krnl = caller->krnl;
  addr_t *pte = NULL;

  if (!krnl || !krnl->mm)
    return -1;

  /* Get value from the system */
  /* TODO: perform multi-level page mapping through caller->krnl->mm page tables */
  pte = get_pte_ptr(krnl->mm, pgn, 1);
  if (pte == NULL)
    return -1;

  CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
  // struct krnl_t *krnl = caller->krnl;
  struct krnl_t *krnl = caller->krnl;
  addr_t *pte = NULL;

  if (!krnl || !krnl->mm)
    return -1;

  /* Get value from the system */
  /* TODO: perform multi-level page mapping through caller->krnl->mm page tables */
  pte = get_pte_ptr(krnl->mm, pgn, 1);
  if (pte == NULL)
    return -1;

  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  // struct krnl_t *krnl = caller->krnl;
  struct krnl_t *krnl = caller->krnl;
  uint32_t pte = 0;
  addr_t pgd = 0;
  addr_t p4d = 0;
  addr_t pud = 0;
  addr_t pmd = 0;
  addr_t pt = 0;

  if (!krnl || !krnl->mm)
    return 0;

  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //... krnl->mm->pgd
  //... krnl->mm->pt
  // pte = &krnl->mm->pt;
  addr_t *pte_ptr = get_pte_ptr(krnl->mm, pgn, 0);
  if (!pte_ptr)
    return 0;

  return (uint32_t)*pte_ptr;
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
  struct krnl_t *krnl = caller->krnl;
  if (!krnl || !krnl->mm)
    return -1;

  /* TODO: set PTE entry using multi-level page table mapping */

  addr_t *pte_ptr = get_pte_ptr(krnl->mm, pgn, 1);
  if (!pte_ptr)
    return -1;

  *pte_ptr = (addr_t)pte_val;
  return 0;
}

/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller, // process call
                    addr_t addr,          // start address which is aligned to pagesz
                    int pgnum)            // num of mapping page
{
  // int pgit = 0;
  // uint64_t pattern = 0xdeadbeef;
  struct krnl_t *krnl = caller->krnl;
  int i;
  addr_t pgn = (addr >> PAGING64_ADDR_PT_SHIFT);

  if (!krnl || !krnl->mm)
    return -1;

  /* TODO: memset the page table with given pattern for the target range */
  /* Reset page table entries in the target range. This creates page
   * table leaves if needed and clears the mapping state. */
  for (i = 0; i < pgnum; i++) {
    addr_t cur_pgn = pgn + i;
    pte_set_entry(caller, cur_pgn, 0xdeadbeef);
  }

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                       addr_t addr,                    // start address which is aligned to pagesz
                       int pgnum,                      // num of mapping page
                       struct framephy_struct *frames, // list of the mapped frames
                       struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                      // no guarantee all given pages are mapped
                                                       // struct framephy_struct *fpit;
  // int pgit = 0;
  // addr_t pgn;
  /* TODO: update the rg_end and rg_start of ret_rg
  //ret_rg->rg_end =  ....
  //ret_rg->rg_start = ...
  //ret_rg->vmaid = ...
  */

  /* TODO map range of frame to address space
   *      [addr to addr + pgnum*PAGING_PAGESZ
   *      in page table caller->krnl->mm->pgd,
   *                    caller->krnl->mm->pud...
   *                    ...
   */

  /* Tracking for later page replacement activities (if needed)
   * Enqueue new usage page */
  // enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn64 + pgit);

  int i = 0;
  addr_t pgn = (addr >> PAGING64_ADDR_PT_SHIFT);
  struct framephy_struct *fp = frames;
  int mapped = 0;

  if (!caller || !caller->krnl || !caller->krnl->mm || ret_rg == NULL)
    return -1;

  for (i = 0; i < pgnum; i++) {
    if (fp == NULL)
      break; /* no more frames provided */

    addr_t cur_pgn = pgn + i;
    /* set PTE to point to frame number */
    pte_set_fpn(caller, cur_pgn, fp->fpn);
    mapped++;
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, cur_pgn);
    fp = fp->fp_next;
  }

  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + (addr_t)mapped * PAGING64_PAGESZ;
  ret_rg->rg_next = NULL;

  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  // addr_t fpn;
  // int pgit;
  // struct framephy_struct *newfp_str = NULL;

  /* TODO: allocate the page
  //caller-> ...
  //frm_lst-> ...
  */
  // TODO: allocate the page
  addr_t fpn;
  int pgit;
  struct framephy_struct *head = NULL, *tail = NULL;

  if (!caller || !caller->krnl || !caller->krnl->mram)
    return -1;

  for (pgit = 0; pgit < req_pgnum; pgit++) {
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0) {
      struct framephy_struct *newfp = malloc(sizeof(struct framephy_struct));
      if (!newfp) {
        /* malloc failed: cleanup and return error */
        struct framephy_struct *it = head;
        while (it) {
          struct framephy_struct *nx = it->fp_next;
          free(it);
          it = nx;
        }
        *frm_lst = NULL;
        return -1;
      } else {
        newfp->fpn = fpn;
        newfp->fp_next = NULL;
        newfp->owner = NULL;
        if (head == NULL) {
          head = tail = newfp;
        } else {
          tail->fp_next = newfp;
          tail = newfp;
        }
      }
    } else {
      /* TODO: ERROR CODE of obtaining somes but not enough frames */
      /* Not enough frames: cleanup and return out-of-memory code */
      struct framephy_struct *it = head;
      while (it) {
        struct framephy_struct *nx = it->fp_next;
        free(it);
        it = nx;
      }
      *frm_lst = NULL;
      return -3000;
    }
  }

  *frm_lst = head;
  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_range(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
  // int pgnum = incpgnum;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_kernel(struct pcb_t *caller,
                     int incpgnum,
                     struct vm_rg_struct *ret_rg)
{
  const addr_t KERNEL_BASE = 0xffff800000000000ULL;
  struct framephy_struct *frm_lst = NULL;
  struct framephy_struct *fpit = frm_lst;
  struct krnl_t *krnl = caller->krnl;

  /* TODO: tính địa chỉ kernel virtual bắt đầu KERNEL_BASE + fpit->fpn * PAGING64_PAGESZ -> KERNEL_BASE + size */
  /* TODO map từng frame vào page table kernel -> krnl->krnl_pgd */
  /* Simplified kernel mapping: allocate frames and set kernel page table
   * entries using krnl->krnl_pt. This is a minimal implementation used by
   * the simulator and not a full kernel allocator.
   */
  if (alloc_pages_range(caller, incpgnum, &frm_lst) != 0)
    return -1;
  /* Ensure caller and kernel are valid */
  if (!caller || !krnl) {
    /* free frames list on error */
    struct framephy_struct *it = frm_lst;
    while (it) {
      struct framephy_struct *nx = it->fp_next;
      free(it);
      it = nx;
    }
    return -1;
  }

  /* Allocate kernel pgd once before mapping to avoid repeated checks
   * inside the mapping loop and to ensure we can clean up on failure.
   */
  if (!krnl->krnl_pgd) {
    int entries = 1 << (PAGING64_ADDR_PGD_HIBIT - PAGING64_ADDR_PGD_LOBIT + 1);
    krnl->krnl_pgd = calloc(entries, sizeof(addr_t));
    if (!krnl->krnl_pgd) {
      struct framephy_struct *it = frm_lst;
      while (it) {
        struct framephy_struct *nx = it->fp_next;
        free(it);
        it = nx;
      }
      return -1;
    }
  }

  fpit = frm_lst;
  /* For simplicity, map consecutively at KERNEL_BASE + i*PAGESZ */
  int i = 0;
  addr_t cur_addr = KERNEL_BASE;
  while (fpit && i < incpgnum) {
    /* compute pgn and set kernel pte */
    addr_t pgn = (cur_addr >> PAGING64_ADDR_PT_SHIFT);

    /* Use the safe walker that starts from the kernel root */
    addr_t *pte_ptr = get_pte_ptr_root(krnl->krnl_pgd, pgn, 1);
    if (pte_ptr) {
      SETBIT(*pte_ptr, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte_ptr, PAGING_PTE_SWAPPED_MASK);
      SETVAL(*pte_ptr, fpit->fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    i++;
    cur_addr += PAGING64_PAGESZ;
    fpit = fpit->fp_next;
  }

  ret_rg->rg_start = KERNEL_BASE;
  ret_rg->rg_end = KERNEL_BASE + (addr_t)incpgnum * PAGING64_PAGESZ;
  return KERNEL_BASE;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING64_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING64_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING64_PAGESZ + cellidx;

    BYTE data1, data2;

    MEMPHY_read(mpsrc, addrsrc, &data1);
    MEMPHY_read(mpdst, addrdst, &data2);
    /* SWAP */
    BYTE tmp = data1;
    data1 = data2;
    data2 = tmp;

    MEMPHY_write(mpsrc, addrsrc, data1);
    MEMPHY_write(mpdst, addrdst, data2);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

  /* TODO init page table directory */
  // mm->pgd = ...
  // mm->p4d = ...
  // mm->pud = ...
  // mm->pmd = ...
  // mm->pt = ...
  /* Initialize multi-level page table arrays. Each level uses 9 bits
   * (indices 0..511). Compute entries from defined bit ranges. */
  int entries = 1 << (PAGING64_ADDR_PGD_HIBIT - PAGING64_ADDR_PGD_LOBIT + 1);
  mm->pgd = calloc(entries, sizeof(addr_t));
  mm->p4d = NULL;
  mm->pud = NULL;
  mm->pmd = NULL;
  mm->pt = NULL;

  if (!mm->pgd)
    return -1;

  /* The page table uses on-demand allocation for lower-level directories. */

  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* Finish initializing minimal VMA/MM bookkeeping */
  /* TODO update VMA0 next */
  // vma0->next = ...

  /* Point vma owner backward */
  // vma0->vm_mm = mm;
  vma0->vm_next = NULL;
  vma0->vm_mm = mm;
  /* TODO: update mmap */
  // mm->mmap = ...
  // mm->symrgtbl = ...
  // mm->kcpooltbl
  /* TODO: initialize mm->symrgtbl and mm->kcpooltbl */
  mm->mmap = vma0;
  memset(mm->symrgtbl, 0, sizeof(mm->symrgtbl));
  mm->fifo_pgn = NULL;
  mm->kcpooltbl = NULL;

  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("\n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  // addr_t pgn_start;//, pgn_end;
  addr_t pgn = start >> PAGING64_ADDR_PT_SHIFT;
  addr_t max_pgn = end ? (end >> PAGING64_ADDR_PT_SHIFT) : pgn;
  struct krnl_t *krnl = caller->krnl;
  struct mm_struct *mm = krnl ? krnl->mm : NULL;

  if (!mm || !mm->pgd)
    return -1;

  printf("print_pgtbl: start=" FORMAT_ADDR " end=" FORMAT_ADDR "\n", start, end);
  /* TODO: traverse the page map and dump the page directory entries */
  for (addr_t cur = pgn; cur <= max_pgn; cur++) {
    addr_t *pte_ptr = get_pte_ptr(mm, cur, 0);
    if (!pte_ptr)
      continue;
    addr_t pte = *pte_ptr;
    if (pte == 0)
      continue;
    printf(" pgn=" FORMAT_ADDR " pte=" FORMAT_ADDR " present=%s swap=%s\n",
           cur,
           pte,
           (pte & PAGING_PTE_PRESENT_MASK) ? "Y" : "N",
           (pte & PAGING_PTE_SWAPPED_MASK) ? "Y" : "N");
  }
  return 0;

  
}

#endif // def MM64