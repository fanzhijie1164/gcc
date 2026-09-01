/* Interprocedural array remapping
   Copyright (C) 2008-2020 Free Software Foundation, Inc.

   Contributed by Lu Shiwei <lushiwei1861@phytium.com.cn>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_IPA_ARRAY_REMAPPING_H
#define GCC_IPA_ARRAY_REMAPPING_H

struct arfunction;
struct araccess;
struct ardecl;
struct arloop;
struct arptrplus;

enum decl_type
{
  GLOBAL_DECL,
  ARGUMENT_DECL
};

struct arfunction
{
  cgraph_node *node;
  auto_vec<araccess *> load_accesses;
  auto_vec<ardecl *> load_decls;

  // constructors
  arfunction (cgraph_node *n)
    : node (n)
  {}

  // Methods
  void dump (FILE *file);
  ardecl *find_decl (tree);
};

struct araccess
{
  gimple *stmt;
  cgraph_node *node;
  loop_p loop;

  tree iter_base;
  HOST_WIDE_INT iter_step;

  tree base_decl;
  HOST_WIDE_INT off;
  HOST_WIDE_INT elem_size;

  // constructors
  araccess (gimple *s, cgraph_node *n)
    : stmt (s),
      node (n)
  {}

  // Methods
  void dump (FILE *dump_file);
};

struct ardecl
{
  tree decl;
  tree malloc_ssa = NULL_TREE;
  tree malloc_func = NULL_TREE;
  varpool_node *vnode;

  tree iter_base;

  // auto_vec<araccess *> accesses;
  auto_vec<HOST_WIDE_INT> steps;
  auto_vec<HOST_WIDE_INT> elem_sizes;
  auto_vec<arloop *> loops;
  auto_vec<ardecl *> assign_decls;
  auto_vec<arptrplus *> ptrpluses;

  bool can_opt = true;

  enum decl_type scope_type;
  int argument_index = -1;
  HOST_WIDE_INT malloc_size = -1;
  HOST_WIDE_INT init_offset = -1;
  HOST_WIDE_INT group_size = -1;
  HOST_WIDE_INT num_of_groups = -1;
  HOST_WIDE_INT elem_size = -1;

  gimple *visited_stmt = NULL;

  // Constructors
  ardecl (tree decl)
    : decl (decl)
  {}

  // Methods
  void dump (FILE *dump_file);
  // araccess *find_accesses (araccess *);
  arloop *find_loop (loop_p);
  ardecl *find_assign_decl (tree);
};

struct arloop
{
  loop_p loop;
  auto_vec<araccess *> accesses;
  tree new_iv;
  HOST_WIDE_INT step;

  // Constructors
  arloop (loop_p loop, tree new_iv = NULL_TREE, HOST_WIDE_INT step = -1)
    : loop (loop),
      new_iv (new_iv),
      step (step)
  {}
};

struct arptrplus
{
  gimple *stmt;
  function *fn;
  tree lhs;
  tree ptr_base;
  tree index;

  HOST_WIDE_INT index_step = -1;
  HOST_WIDE_INT index_offset;
  bool need_runtime = true;

  // Constructors
  arptrplus (gimple *stmt)
    : stmt (stmt)
  {}
};

# endif
