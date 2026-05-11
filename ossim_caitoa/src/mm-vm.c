/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#ifdef MM64
#include "mm64.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  //struct vm_area_struct *cur_vma = get_vma_by_num(caller->kernl->mm, vmaid);

  //newrg = malloc(sizeof(struct vm_rg_struct));

  /* TODO: update the newrg boundary
  // newrg->rg_start = ...
  // newrg->rg_end = ...
  */
  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL)
    return NULL;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL)
    return NULL;

  addr_t reserve_sz = alignedsz ? alignedsz : size;
  addr_t actual_sz = size ? size : reserve_sz;

  if (reserve_sz == 0 || actual_sz == 0)
    return NULL;

  newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL)
    return NULL;

  addr_t start = cur_vma->sbrk;
  addr_t end = start + actual_sz;

  cur_vma->sbrk = cur_vma->sbrk + reserve_sz;

  newrg->rg_start = start;
  newrg->rg_end = end;
  newrg->rg_next = NULL;
  /* END TODO */

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  //struct vm_area_struct *vma = caller->krnl->mm->mmap;

  /* TODO validate the planned memory area is not overlapped */
  if (vmastart >= vmaend)
  {
    return -1;
  }

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->krnl->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  /* TODO validate the planned memory area is not overlapped */

  struct vm_area_struct *cur_area = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_area == NULL)
  {
    return -1;
  }

  if (vmastart < cur_area->vm_start)
  {
    return -1;
  }

  while (vma != NULL)
  {
    if (vma != cur_area && OVERLAP(vmastart, vmaend, vma->vm_start, vma->vm_end))
    {
      return -1;
    }
    vma = vma->vm_next;
  }
  /* End TODO*/

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  //struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));

  /* TODO with new address scheme, the size need tobe aligned 
   *      the raw inc_sz maybe not fit pagesize
   */ 
  //addr_t inc_amt;

//  int incnumpage =  inc_amt / PAGING_PAGESZ;

  /* TODO Validate overlap of obtained region */
  //if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
  //  return -1; /*Overlap and failed allocation */

  /* TODO: Obtain the new vm area based on vmaid */
  //cur_vma->vm_end... 
  // inc_limit_ret...
  /* The obtained vm area (only)
   * now will be alloc real ram region */

//  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
//                   old_end, incnumpage , newrg) < 0)
//    return -1; /* Map the memory to MEMRAM */

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL)
    return -1;

  if (inc_sz == 0)
    return 0;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL)
    return -1;

#ifdef MM64
  addr_t page_sz = PAGING64_PAGESZ;
  addr_t inc_amt = PAGING64_PAGE_ALIGNSZ(inc_sz);
#else
  addr_t page_sz = PAGING_PAGESZ;
  addr_t inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
#endif

  if (inc_amt == 0)
    inc_amt = page_sz;

  addr_t old_sbrk = cur_vma->sbrk;
  addr_t old_end = cur_vma->vm_end;

  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_amt, inc_amt);
  if (area == NULL)
    return -1;

  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
  {
    cur_vma->sbrk = old_sbrk;
    free(area);
    return -1;
  }

  int incnumpage = (int)DIV_ROUND_UP(inc_amt, page_sz);

  if (incnumpage > 0 &&
      vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage, area) < 0)
  {
    cur_vma->sbrk = old_sbrk;
    cur_vma->vm_end = old_end;
    free(area);
    return -1;
  }

  cur_vma->vm_end = area->rg_end;

  area->rg_next = cur_vma->vm_freerg_list;
  cur_vma->vm_freerg_list = area;

  return 0;
}

// #endif
