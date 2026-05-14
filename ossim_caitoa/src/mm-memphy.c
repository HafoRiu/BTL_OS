/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


// check if a specific frame exists in a list 
static int frame_in_list(struct framephy_struct *head, addr_t fpn) {
    while (head != NULL) {
        if (head->fpn == fpn) return 1;
        head = head->fp_next;
    }
    return 0;
}

// allocate N contiguous physical frames for KMALLOC 
int MEMPHY_get_contiguous_freefp(struct memphy_struct *mp, int num_frames, addr_t *first_fpn)
{
   pthread_mutex_lock(&mp->lock);

   if (mp == NULL || num_frames <= 0) {
      pthread_mutex_unlock(&mp->lock);
      return -1;
   }
   
   int total_frames = mp->maxsz / PAGING_PAGESZ;
   int found_start = -1;

   for (int start_fpn = 0; start_fpn <= total_frames - num_frames; start_fpn++) {
      int is_contiguous = 1;
      for (int offset = 0; offset < num_frames; offset++) {
         if (!frame_in_list(mp->free_fp_list, start_fpn + offset)) {
               is_contiguous = 0;
               break;
         }
      }
      if (is_contiguous) {
         found_start = start_fpn;
         break;
      }
   }

   if (found_start == -1) {
      pthread_mutex_unlock(&mp->lock);
      return -1;
   } 

   for (int offset = 0; offset < num_frames; offset++) {
      addr_t target_fpn = found_start + offset;
      struct framephy_struct *curr = mp->free_fp_list;
      struct framephy_struct *prev = NULL;
      
      while (curr != NULL) {
         if (curr->fpn == target_fpn) {
               if (prev == NULL) mp->free_fp_list = curr->fp_next;
               else prev->fp_next = curr->fp_next;
               
               curr->fp_next = mp->used_fp_list;
               mp->used_fp_list = curr;
               break;
         }
         prev = curr;
         curr = curr->fp_next;
      }
   }

   *first_fpn = found_start;
   pthread_mutex_unlock(&mp->lock);
   return 0;
}

/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
int MEMPHY_mv_csr(struct memphy_struct *mp, addr_t offset)
{
   int numstep = 0;

   mp->cursor = 0;
   while (numstep < offset && numstep < mp->maxsz)
   {
      /* Traverse sequentially */
      mp->cursor = (mp->cursor + 1) % mp->maxsz;
      numstep++;
   }

   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, addr_t addr, BYTE *value)
{
   if (mp == NULL)
      return -1;

   if (mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   *value = (BYTE)mp->storage[addr];

   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct *mp, addr_t addr, BYTE *value)
{
   pthread_mutex_lock(&mp->lock);

   if (mp == NULL) {
      pthread_mutex_unlock(&mp->lock);
      return -1;
   }

   int ret = 0;
   if (mp->rdmflg) *value = mp->storage[addr];
   else {
      ret = MEMPHY_seq_read(mp, addr, value);
   }

   return ret;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct *mp, addr_t addr, BYTE value)
{

   if (mp == NULL)
      return -1;

   if (mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   mp->storage[addr] = value;

   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct *mp, addr_t addr, BYTE data)
{
   pthread_mutex_lock(&mp->lock);
   if (mp == NULL) {
      pthread_mutex_unlock(&mp->lock);
      return -1;
   }

   int ret = 0;
   if (mp->rdmflg) mp->storage[addr] = data;
   else {
      ret = MEMPHY_seq_write(mp, addr, data);
   }
   return ret;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
   /* This setting come with fixed constant PAGESZ */
   int numfp = mp->maxsz / pagesz;
   struct framephy_struct *newfst, *fst;
   int iter = 0;

   if (numfp <= 0)
      return -1;

   /* Init head of free framephy list */
   fst = malloc(sizeof(struct framephy_struct));
   fst->fpn = iter;
   mp->free_fp_list = fst;

   /* We have list with first element, fill in the rest num-1 element member*/
   for (iter = 1; iter < numfp; iter++)
   {
      newfst = malloc(sizeof(struct framephy_struct));
      newfst->fpn = iter;
      newfst->fp_next = NULL;
      fst->fp_next = newfst;
      fst = newfst;
   }

   return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, addr_t *retfpn)
{
   pthread_mutex_lock(&mp->lock);
   struct framephy_struct *fp = mp->free_fp_list;

   if (fp == NULL) {
      pthread_mutex_unlock(&mp->lock);
      return -1;
   }
   // *retfpn = fp->fpn;
   // mp->free_fp_list = fp->fp_next;

   // /* MEMPHY is iteratively used up until its exhausted
   //  * No garbage collector acting then it not been released
   //  */
   // free(fp);

   *retfpn = fp->fpn;

   /* Detach from free list */
   mp->free_fp_list = fp->fp_next;

   /* Attach directly to used list */
   fp->fp_next = mp->used_fp_list;
   mp->used_fp_list = fp;

   pthread_mutex_unlock(&mp->lock);
   return 0;
}

int MEMPHY_dump(struct memphy_struct *mp)
{
   /* Dump memphy content mp->storage for tracing the memory content */
   
   printf("\n============= MEMPHY DUMP ============\n");
   
   /* Safety check: ensure the memory device actually exists */
   if (mp == NULL || mp->storage == NULL) {
       printf("Error: Memory physical device is uninitialized.\n");
       return -1;
   }

int i;
   int empty_flag = 1; //Used to track if the memory is completely blank
   for (i = 0; i < mp->maxsz; i++) {
       if (mp->storage[i] != 0) {
           printf("Address [0x%08x] : Data [0x%02x]\n", i, (unsigned char)mp->storage[i]);
           empty_flag = 0;
       }
   }

   if (empty_flag) {
       printf("Memory is completely empty (all zeros).\n");
   }
   
   printf("======================================\n\n");

   return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, addr_t fpn)
{
   // lock
   pthread_mutex_lock(&mp->lock);

   struct framephy_struct *curr = mp->used_fp_list;
   struct framephy_struct *prev = NULL;
   
   // Find and detach the frame from the used list 
   while (curr != NULL)
   {
      if (curr->fpn == fpn)
      {
         if (prev == NULL)
            mp->used_fp_list = curr->fp_next; // It was the head
         else
            prev->fp_next = curr->fp_next; // It was in the middle/end
         break;
      }
      prev = curr;
      curr = curr->fp_next;
   }

   if (curr == NULL) {
      pthread_mutex_unlock(&mp->lock);
      return -1;
   }

   addr_t frame_addr = fpn * PAGING_PAGESZ;
   if (frame_addr < mp->maxsz) {
      memset(mp->storage + frame_addr, 0, PAGING_PAGESZ);
   }

   curr->fp_next = mp->free_fp_list;
   mp->free_fp_list = curr;

   // unlock 
   pthread_mutex_unlock(&mp->lock);

   return 0;
}

/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, addr_t max_size, int randomflg)
{
   if (mp == NULL || max_size == 0)
      return -1;
   memset(mp, 0, sizeof(struct memphy_struct));

   //Initialize the mutex lock
   pthread_mutex_init(&mp->lock, NULL);

   mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
   mp->maxsz = max_size;
   memset(mp->storage, 0, max_size * sizeof(BYTE));

   MEMPHY_format(mp, PAGING_PAGESZ);

   mp->rdmflg = (randomflg != 0) ? 1 : 0;

   if (!mp->rdmflg) /* Not Ramdom acess device, then it serial device*/
      mp->cursor = 0;

   return 0;
}

// #endif
