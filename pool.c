/* pool.c - Allocation Pool Library 
 * Copyright (C) 2007 Christopher Wellons <ccw129@psu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */ 

#include <stdlib.h>
#include "pool.h"

int default_pool_size = 512;
int miss_limit = 8;

/* Used internally to allocate more pool space. */
static subpool_t *create_subpool_node (size_t size);

pool_t *create_pool (size_t init_size)
{
  /* if init_size == 0, user didn't want to choose one */
  if (init_size == 0)
    {
      init_size = default_pool_size;
    }

  pool_t *new_pool = (pool_t *) malloc (sizeof (pool_t));
  if (new_pool == NULL)
    return NULL;

  /* allocate first subpool */
  new_pool->pools = create_subpool_node (init_size);
  new_pool->first = new_pool->pools;

  if (new_pool->first == NULL)
    return NULL;

  return new_pool;
}

/* Returns a pointer to the allocated size bytes from the given
 * pool. */
void *pool_alloc (pool_t * source_pool, size_t size)
{
  subpool_t *cur, *last;
  void *chunk = NULL;

  cur = source_pool->first;
  if (cur->misses > miss_limit)
    {
      /* this pool doesn't seem to be any good anymore */
      source_pool->first = source_pool->first->next;
    }

  do
    {
      if (size <= (size_t) (cur->free_end - cur->free_start))
	{
	  /* cut off a chunk and return it */
	  chunk = cur->free_start;
	  cur->free_start += size;
	  cur->misses = 0;
	}
      else
	{
	  /* current pool is too small */
	  cur->misses++;
	}

      last = cur;
      cur = cur->next;
    }
  while (cur != NULL && chunk == NULL);

  /* No existing pools had enough room. Make a new one. */
  if (chunk == NULL)
    {
      /* double the size of the last one */
      size_t new_size = last->size * 2;
      if (new_size <= size)
	{
	  /* quadruple requested size if its much bigger */
	  new_size = size * 4;
	}

      /* create new subpool */
      last->next = create_subpool_node (new_size);
      cur = last->next;

      if (cur == NULL)		/* failed to allocate subpool */
	return NULL;

      /* chop off requested amount */
      chunk = cur->free_start;
      cur->free_start += size;
    }

  return chunk;
}

/* Frees all data allocated by the pool. This will free all data
 * obtained by pool_alloc for this pool. */
void free_pool (pool_t * source_pool)
{
  subpool_t *last;
  subpool_t *cur = source_pool->pools;

  while (cur != NULL)
    {
      free (cur->mem_block);

      last = cur;
      cur = cur->next;

      free (last);
    }

  free(source_pool);
}

subpool_t *create_subpool_node (size_t size)
{
  subpool_t *new_subpool = (subpool_t *) malloc (sizeof (subpool_t));
  if (new_subpool == NULL)
    return NULL;

  /* allocate subpool memory */
  new_subpool->mem_block = malloc (size);
  if (new_subpool->mem_block == NULL)
    {
      free (new_subpool);
      return NULL;
    }

  /* initialize data */
  new_subpool->free_start = new_subpool->mem_block;
  new_subpool->free_end = new_subpool->mem_block + size;
  new_subpool->size = size;
  new_subpool->misses = 0;
  new_subpool->next = NULL;

  return new_subpool;
}
