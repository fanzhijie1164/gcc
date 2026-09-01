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

/* IPA-arrayremapping */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "tree-pass.h"
#include "ssa.h"
#include "cgraph.h"
#include "gimple-pretty-print.h"
#include "alias.h"
#include "tree-eh.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "tree-dfa.h"
#include "alloc-pool.h"
#include "symbol-summary.h"
#include "dbgcnt.h"
#include "tree-inline.h"
#include "ipa-utils.h"
#include "builtins.h"
#include "cfganal.h"
#include "cfgloop.h"
#include "tree-streamer.h"
#include "internal-fn.h"
#include "tree-scalar-evolution.h"
#include "tree-ssa-loop-niter.h"
#include "tree-ssa-loop.h"
#include "fold-const.h"
#include "ipa-arrayremapping.h"
#include "tree-cfg.h"
#include "langhooks.h"
#include "gimple-fold.h"
#include "gimple-match.h"

/* access info of all functions */
auto_vec<arfunction *> all_functions;
/* access info of all global variables */
auto_vec<ardecl *> global_decls;
/* loop info */
auto_vec<arloop *> all_loops;

/* Build a binary operation and gimplify it.  Emit code before GSI.
   Return the gimple_val holding the result.  */
  
static tree
gimplify_build2 (gimple_stmt_iterator *gsi, enum tree_code code,
                 tree type, tree a, tree b)
{
  location_t loc = gimple_location (gsi_stmt (*gsi));
  gimple_seq stmts = NULL;
  tree ret = gimple_build (&stmts, loc, code, type, a, b);
  gsi_insert_seq_before (gsi, stmts, GSI_SAME_STMT);
  return ret;
}

/* Build a unary operation and gimplify it.  Emit code before GSI.
   Return the gimple_val holding the result.  */

static tree
gimplify_build1 (gimple_stmt_iterator *gsi, enum tree_code code, tree type,
                 tree a)
{
  location_t loc = gimple_location (gsi_stmt (*gsi));
  gimple_seq stmts = NULL;
  tree ret = gimple_build (&stmts, loc, code, type, a);
  gsi_insert_seq_before (gsi, stmts, GSI_SAME_STMT);
  return ret;
}

static ardecl *
find_global_decl (tree decl)
{
  for (unsigned i = 0; i < global_decls.length (); i++)
    if (global_decls[i]->decl == decl)
      return global_decls[i];
  return NULL;
}

static varpool_node *
find_varpool_node (tree decl)
{
  varpool_node *global_var;
  FOR_EACH_VARIABLE (global_var)
  {
    if (global_var->decl == decl)
      return global_var;
  }
  return NULL;
}

/* Find DECL in the function. */

ardecl *
arfunction::find_decl (tree decl)
{
  for (unsigned i = 0; i < load_decls.length (); i++)
    if (load_decls[i]->decl == decl)
      return load_decls[i];
  return NULL;
}

void
araccess::dump (FILE *dump_file)
{
  fprintf (dump_file, "function name: %s", node->name());
  fprintf (dump_file, "\n");
  print_gimple_stmt (dump_file, stmt, 0);
  fprintf (dump_file, "base_decl: ");
  print_generic_expr (dump_file, base_decl);
  fprintf (dump_file, "\nbase_off: ");
  fprintf (dump_file, "%ld", off);
  fprintf (dump_file, "\niter_step: ");
  fprintf (dump_file, "%ld", iter_step);
  fprintf (dump_file, "\nelem_size: ");
  fprintf (dump_file, "%ld", elem_size);
  fprintf (dump_file, "\niter_base: ");
  print_generic_expr (dump_file, iter_base);
  fprintf (dump_file, "\n");
}

void
ardecl::dump (FILE *dump_file)
{
  fprintf (dump_file, "\n");
  fprintf (dump_file, "load decls:");
  print_generic_expr (dump_file, decl);
  fprintf (dump_file, "\n");
  for (arloop *loop : loops)
    {
      fprintf (dump_file, "====== Loop accesses ======\n");
      for (araccess * arac : loop->accesses)
	{
	  arac->dump (dump_file);
	}
      fprintf (dump_file, "============");
    }
  for (arptrplus *app : ptrpluses)
    {
      fprintf (dump_file, "\nptrplus stmt:");
      print_gimple_stmt (dump_file, app->stmt, 0);
    }
  fprintf (dump_file, "\nsteps:");
  for (HOST_WIDE_INT step : steps)
    {
      fprintf (dump_file, "%ld ", step);
    }
  fprintf (dump_file, "\ngroup_size: ");
  fprintf (dump_file, "%ld", group_size);
  fprintf (dump_file, "\nelem_sizes:");
  for (HOST_WIDE_INT es : elem_sizes)
    {
      fprintf (dump_file, "%ld ", es);
    }
  fprintf (dump_file, "\nelem_size: ");
  fprintf (dump_file, "%ld", elem_size);
  fprintf (dump_file, "\ntype: ");
  fprintf (dump_file, "%d", scope_type);
  fprintf (dump_file, "\nargument_index: ");
  fprintf (dump_file, "%d", argument_index);
  fprintf (dump_file, "\nmalloc_size: ");
  fprintf (dump_file, "%ld", malloc_size);
  fprintf (dump_file, "\ninit_offset: ");
  fprintf (dump_file, "%ld", init_offset);
  fprintf (dump_file, "\ncan_opt: ");
  fprintf (dump_file, "%d", can_opt);
  fprintf (dump_file, "\n");
}

arloop *
ardecl::find_loop (loop_p loop)
{
  for (unsigned i = 0; i < loops.length (); i++)
    if (loops[i]->loop == loop)
      return loops[i];
  return NULL;
}

ardecl *
ardecl::find_assign_decl (tree decl)
{
  for (unsigned i = 0; i < assign_decls.length (); i++)
    if (assign_decls[i]->decl == decl)
      return assign_decls[i];
  return NULL;
}

/* Return true when DECL is an argument to FN and assigns the index value
   of the argument to INDEX. */
static bool
is_argument_decl (tree decl, tree fn, int *index)
{
  int i = -1;
  for (tree parm = DECL_ARGUMENTS (fn);
       parm != NULL_TREE;
       parm = TREE_CHAIN (parm))
    {
      i++;
      if (parm == decl)
	{
	  if (index != NULL)
	    *index = i;
	  return true;
	}
    }
  return false;
}

/* Return the element type if NODE is pointer/array. */
static tree
get_elem_type (tree node)
{
  if (TREE_CODE (node) == INTEGER_TYPE ||
      TREE_CODE (node) == REAL_TYPE)
    return node;
  else if (TREE_CODE (node) == POINTER_TYPE)
    return get_elem_type (TREE_TYPE (node));
  else if (TREE_CODE (node) == ARRAY_TYPE)
    return get_elem_type (TREE_TYPE (node));
  else
    return NULL_TREE;
}

/* Iterate over all functions and collect load access information
   within functions on a function basis. */
static void
record_all_accesses (void)
{
  struct cgraph_node *node;
  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
  {
    if (!node->decl)
      continue;
    if (!node->inlined_to && !node->clone_of)
      node->get_body ();
    struct function *func = DECL_STRUCT_FUNCTION (node->decl);
    if (!func)
      continue;
    /* init info */
    push_cfun (func);
    calculate_dominance_info (CDI_DOMINATORS);
    calculate_dominance_info (CDI_POST_DOMINATORS);
    loop_optimizer_init (LOOPS_NORMAL | LOOPS_HAVE_RECORDED_EXITS);
    scev_initialize ();

    arfunction *fn = new arfunction (node);
    all_functions.safe_push (fn);
    basic_block bb;

    FOR_ALL_BB_FN (bb, cfun)
    {
      gimple_stmt_iterator gsi;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
	{
	  gimple *stmt = gsi_stmt (gsi);
	  tree op;
	  tree check_iter;
	  affine_iv iv;
	  HOST_WIDE_INT off;
	  if (!gimple_assign_load_p (stmt))
	    continue;
	  op = gimple_assign_rhs1 (stmt);
	  if (TREE_CODE (op) != MEM_REF)
	    continue;

	  off = mem_ref_offset (op).force_shwi ().to_constant ();
	  check_iter = TREE_OPERAND (op, 0);

	  if (!simple_iv (loop_containing_stmt (stmt),
			  loop_containing_stmt (stmt),
			  check_iter, &iv, false))
	    continue;

	  gcc_assert (iv.base && iv.step);
	  araccess *load_access = new araccess (stmt, node);
	  load_access->loop = loop_containing_stmt (stmt);

	  tree iter_base = iv.base;
	  HOST_WIDE_INT iter_offset = 0;
	  tree elem_type = get_elem_type (TREE_TYPE (iter_base));
	  if (!elem_type)
	    continue;
	  HOST_WIDE_INT elem_size = int_cst_value (TYPE_SIZE_UNIT (elem_type));
	  load_access->elem_size = elem_size;
	  load_access->iter_base = iter_base;
	  if (TREE_CODE (iter_base) == POINTER_PLUS_EXPR)
	    {
	      tree iter_offset_tree;
	      iter_base = TREE_OPERAND (iter_base, 0);
	      iter_offset_tree = TREE_OPERAND (iv.base, 1);
	      if (TREE_CODE (iter_offset_tree) == INTEGER_CST)
		{
		  iter_offset = int_cst_value (iter_offset_tree);
		}
	    }
	  /* type conversion may be NOP_EXPR */
	  if (TREE_CODE (iter_base) == NOP_EXPR)
	    {
	      iter_base = TREE_OPERAND (iter_base, 0);
	    }
	  /* SSA_NAME : _1/var_134
	     ADDR_EXPR : [&value] */
	  if (TREE_CODE (iter_base) != SSA_NAME &&
	      TREE_CODE (iter_base) != ADDR_EXPR)
	    continue;
	  tree def_var;
	  if (TREE_CODE (iter_base) == ADDR_EXPR)
	    def_var = TREE_OPERAND (iter_base, 0);
	  else
	    def_var = SSA_NAME_VAR (iter_base);

	  if (!def_var || !(is_global_var (def_var)
			    || is_argument_decl (def_var, node->decl, NULL)))
	    {
	      /* TODO : no decl/malloc/(malloc + offset)
		Currently 519 is not inline, this will happen
		if alloc function is inline */
	      gimple *def_stmt = SSA_NAME_DEF_STMT (iter_base);
	      if (!gimple_assign_load_p (def_stmt))
		continue;
	      def_var = gimple_assign_rhs1 (def_stmt);
	      if (!DECL_P (def_var))
		continue;
	      if (!(is_global_var (def_var) ||
		  is_argument_decl (def_var, node->decl, NULL)))
		continue;
	    }

	  if (!DECL_P (def_var))
	    continue;
	  /* Set load_access */
	  load_access->base_decl = def_var;
	  load_access->iter_step = int_cst_value (iv.step) / elem_size;
	  load_access->off = (iter_offset + off) / elem_size;
	  fn->load_accesses.safe_push (load_access);
	}
    }

    /* The load access in the function is sorted by symbol. */
    for (araccess *load_access : fn->load_accesses) {
      if (dump_file) {
	fprintf (dump_file, "\n");
	fprintf (dump_file, "Record All Accesses, all info : ");
	load_access->dump (dump_file);
      }
      ardecl *access_decl = fn->find_decl (load_access->base_decl);
      if (!access_decl) {
	access_decl = new ardecl (load_access->base_decl);
	fn->load_decls.safe_push (access_decl);
      }
      arloop *ac_loop = access_decl->find_loop (load_access->loop);
      if (!ac_loop)
	{
	  ac_loop = new arloop (load_access->loop);
	  access_decl->loops.safe_push (ac_loop);
	}
      ac_loop->accesses.safe_push (load_access);
      access_decl->steps.safe_push (load_access->iter_step);
      access_decl->elem_sizes.safe_push (load_access->elem_size);
    }

    /* set symbol scope */
    for (ardecl *symbol : fn->load_decls)
      {
	int index;
	if (is_global_var (symbol->decl))
	  symbol->scope_type = GLOBAL_DECL;
	else if (is_argument_decl (symbol->decl, fn->node->decl, &index))
	  {
	    symbol->scope_type = ARGUMENT_DECL;
	    symbol->argument_index = index;
	  }
	else
	  gcc_unreachable ();
	if (dump_file)
	  {
	    symbol->dump (dump_file);
	  }
      }

    /* finalize info */
    scev_finalize ();
    loop_optimizer_finalize ();
    free_dominance_info (CDI_DOMINATORS);
    free_dominance_info (CDI_POST_DOMINATORS);
    pop_cfun ();
  }
}

static void
add_to_global_decls (tree check_decl, ardecl *fndecl)
{
  ardecl *gdecl = find_global_decl (check_decl);
  if (!gdecl)
    {
      gdecl = new ardecl (check_decl);
      gdecl->scope_type = GLOBAL_DECL;
      global_decls.safe_push (gdecl);
    }
  for (arloop *l : fndecl->loops)
    gdecl->loops.safe_push (l);
  for (HOST_WIDE_INT s : fndecl->steps)
    gdecl->steps.safe_push (s);
  for (HOST_WIDE_INT es : fndecl->elem_sizes)
    gdecl->elem_sizes.safe_push (es);
  gdecl->vnode = find_varpool_node (check_decl);
  if (!gdecl->vnode)
    gdecl->can_opt = false;
}

/* Check and record the number and size of malloc. */
static bool
get_malloc_info (tree alloc_node, ardecl *decl_info)
{
  gcc_assert (TREE_CODE (alloc_node) == SSA_NAME);
  gimple *def_stmt = SSA_NAME_DEF_STMT (alloc_node);
  if (gimple_code (def_stmt) != GIMPLE_CALL)
    return false;
  tree malloc_decl = gimple_call_fndecl (def_stmt);
  if (!DECL_IS_MALLOC (malloc_decl))
    return false;
  /* only malloc once */
  if (decl_info->malloc_size != -1)
    return false;
  tree malloc_size = gimple_call_arg (def_stmt, 0);
  decl_info->malloc_ssa = alloc_node;
  decl_info->malloc_size = int_cst_value (malloc_size) /
      int_cst_value (TYPE_SIZE_UNIT (TREE_TYPE (decl_info->decl)));
  gcc_assert (decl_info->group_size != -1);
  if (decl_info->group_size == 0)
    return true;
  if (decl_info->malloc_size % decl_info->group_size != 0)
    return false;
  decl_info->num_of_groups = decl_info->malloc_size / decl_info->group_size;
  return true;
}

/* false - unexpected stmt */
static bool
analysis_call_stmt (gimple *call_stmt, ardecl *decl_info)
{
  tree callee_decl = gimple_call_fndecl (call_stmt);
  if (!callee_decl || !DECL_STRUCT_FUNCTION (callee_decl) ||
      stdarg_p (TREE_TYPE (callee_decl)))
    return false;
  struct cgraph_node *node = cgraph_node::get (callee_decl);
  if (!node->has_gimple_body_p ())
    return false;
  tree callee_arg = DECL_ARGUMENTS (callee_decl);
  if (!callee_arg)
    return false;
  int used_arg_index = -1;
  /* get arg node and index */
  for (unsigned i = 0; i < gimple_call_num_args (call_stmt); i++)
    {
      tree arg = gimple_call_arg (call_stmt, i);
      if (TREE_CODE (arg) != ADDR_EXPR)
	continue;
      arg = TREE_OPERAND (arg, 0);
      if (arg == decl_info->decl)
	{
	  if (used_arg_index != -1)
	    return false;
	  used_arg_index = i;
	}
      if (used_arg_index == -1)
	callee_arg = TREE_CHAIN (callee_arg);
    }
  if (used_arg_index == -1)
    return false;
  tree arg_ssa = ssa_default_def (DECL_STRUCT_FUNCTION (callee_decl),
				  callee_arg);
  if (!arg_ssa)
    return false;

  imm_use_iterator imm_iter;
  gimple *use_stmt = NULL;
  push_cfun (DECL_STRUCT_FUNCTION (callee_decl));
  calculate_dominance_info (CDI_DOMINATORS);
  calculate_dominance_info (CDI_POST_DOMINATORS);

  /* Traverse the use of ARG to examine the following scenarios:
	LBM_allocateGrid (double * * ptr)
	  {
	    _1 = malloc (214400000);
	    ...
	    *ptr_6(D) = _1;
	    ...
	    _3 = _1 + 3200000;
	    *ptr_6(D) = _3;
	    ...
	  }
   */
  FOR_EACH_IMM_USE_STMT (use_stmt, imm_iter, arg_ssa)
    {
      if (!gimple_assign_single_p (use_stmt))
	goto fail;
      tree lhs = gimple_get_lhs (use_stmt);
      tree rhs = gimple_assign_rhs1 (use_stmt);
      if (TREE_CODE (lhs) != MEM_REF ||
	  TREE_OPERAND (lhs, 0) != arg_ssa ||
	  TREE_CODE (rhs) != SSA_NAME)
	goto fail;
      gimple *def_stmt = SSA_NAME_DEF_STMT (rhs);
      if (gimple_code (def_stmt) == GIMPLE_CALL)
	{
	  if (decl_info->malloc_ssa != NULL_TREE)
	    {
	      if (decl_info->malloc_ssa != rhs)
		goto fail;
	      gcc_assert (decl_info->visited_stmt && decl_info->malloc_func);
	      if (decl_info->malloc_func != callee_decl)
		goto fail;
	      if (!stmt_dominates_stmt_p (use_stmt, decl_info->visited_stmt))
		goto fail;
	    }
	  else if (!get_malloc_info (rhs, decl_info))
	    goto fail;
	  decl_info->malloc_func = callee_decl;
	  decl_info->visited_stmt = use_stmt;
	}
      else if (gimple_code (def_stmt) == GIMPLE_ASSIGN)
	{
	  /* TODO : assign NULL */
	  if (gimple_assign_rhs_code (def_stmt) != POINTER_PLUS_EXPR)
	    goto fail;
	  tree ptr_base = gimple_assign_rhs1 (def_stmt);
	  tree ptr_offset = gimple_assign_rhs2 (def_stmt);
	  if (TREE_CODE (ptr_base) != SSA_NAME ||
	      TREE_CODE (ptr_offset) != INTEGER_CST)
	    goto fail;
	  if (decl_info->malloc_ssa != NULL_TREE)
	    {
	      /* is recorded or not recorded malloc */
	      if (decl_info->init_offset != -1 ||
		  ptr_base != decl_info->malloc_ssa)
		goto fail;
	      gcc_assert (decl_info->visited_stmt && decl_info->malloc_func);
	      if (decl_info->malloc_func != callee_decl)
		goto fail;
	      if (!stmt_dominates_stmt_p (decl_info->visited_stmt, use_stmt))
		goto fail;
	    }
	  else
	    {
	      if (!get_malloc_info (ptr_base, decl_info))
		goto fail;
	      decl_info->malloc_func = callee_decl;
	    }
	  decl_info->init_offset = int_cst_value (ptr_offset) /
	      int_cst_value (TYPE_SIZE_UNIT (TREE_TYPE (decl_info->decl)));
	  decl_info->visited_stmt = use_stmt;
	}
    }
  free_dominance_info (CDI_DOMINATORS);
  free_dominance_info (CDI_POST_DOMINATORS);
  pop_cfun ();
  return true;

fail:
  free_dominance_info (CDI_DOMINATORS);
  free_dominance_info (CDI_POST_DOMINATORS);
  pop_cfun ();
  end_imm_use_stmt_traverse (&imm_iter);
  return false;
}

static bool
analysis_store_stmt (gimple *store_stmt, ardecl *decl_info)
{
  if (!gimple_assign_single_p (store_stmt))
    return false;
  tree rhs = gimple_assign_rhs1 (store_stmt);
  if (TREE_CODE (rhs) == INTEGER_CST)
    {
      if (int_cst_value (rhs) != 0)
	return false;
    }
  else if (TREE_CODE (rhs) == SSA_NAME)
    {
      gimple *def_stmt = SSA_NAME_DEF_STMT (rhs);
      if (!gimple_assign_load_p (def_stmt))
	return false;
      tree def_var = gimple_assign_rhs1 (def_stmt);
      if (!DECL_P (def_var) || !is_global_var (def_var))
	return false;
      if (decl_info->find_assign_decl (def_var))
	return true;
      ardecl *def_decl = find_global_decl (def_var);
      if (!def_decl ||
	  !def_decl->can_opt)
	return false;
      decl_info->assign_decls.safe_push (def_decl);
      def_decl->assign_decls.safe_push (decl_info);
    }
  else
    return false;

  return true;
}

/* load/store/free */
static bool
is_legal_use_p (tree node)
{
  imm_use_iterator imm_iter;
  gimple *use_stmt = NULL;
  FOR_EACH_IMM_USE_STMT (use_stmt, imm_iter, node)
    {
      if (gimple_assign_single_p (use_stmt))
	{
	  tree use_lhs = gimple_get_lhs (use_stmt);
	  tree use_rhs = gimple_assign_rhs1 (use_stmt);
	  if (TREE_CODE (use_lhs) == MEM_REF)
	    {
	      use_lhs = TREE_OPERAND (use_lhs, 0);
	      if (use_lhs != node)
	        {
		  end_imm_use_stmt_traverse (&imm_iter);
		  return false;
	        }
	    }
	  else if (TREE_CODE (use_rhs) == MEM_REF)
	    {
	      use_rhs = TREE_OPERAND (use_rhs, 0);
	      if (use_rhs != node)
	        {
		  end_imm_use_stmt_traverse (&imm_iter);
		  return false;
	        }
	    }
	  else
	    {
	      end_imm_use_stmt_traverse (&imm_iter);
	      return false;
	    }
	}
      else if (gimple_code (use_stmt) == GIMPLE_CALL)
	{
	  tree callee_decl = gimple_call_fndecl (use_stmt);
	  const char *name = IDENTIFIER_POINTER (DECL_NAME (callee_decl));
	  if (strcmp (name, "free"))
	    {
	      end_imm_use_stmt_traverse (&imm_iter);
	      return false;
	    }
	}
      else
        {
	  end_imm_use_stmt_traverse (&imm_iter);
	  return false;
        }
    }
  return true;
}

static void
record_index_info (gimple *stmt, tree index, HOST_WIDE_INT index_offset,
		   arptrplus *newpp)
{
  gcc_assert (TREE_CODE (index) == SSA_NAME);
  gimple *def_stmt = SSA_NAME_DEF_STMT (index);
  if (gimple_code (def_stmt) == GIMPLE_ASSIGN &&
      gimple_assign_rhs_code (def_stmt) == MULT_EXPR &&
      TREE_CODE (gimple_assign_rhs2 (def_stmt)) == INTEGER_CST)
    {
      /* case 2 */
      newpp->index = index;
      newpp->index_step = int_cst_value (gimple_assign_rhs2 (def_stmt));
      newpp->index_offset = index_offset;
      newpp->need_runtime = false;
      return;
    }

  affine_iv iv;
  if (!simple_iv (loop_containing_stmt (stmt),
		  loop_containing_stmt (stmt),
		  index, &iv, false))
    return;
  if (TREE_CODE (iv.step) != INTEGER_CST)
    return;
  newpp->index_step = int_cst_value (iv.step);
  if (TREE_CODE (iv.base) == INTEGER_CST)
    {
      if (int_cst_value (iv.base) % newpp->index_step != 0)
	return;
      newpp->index = index;
      newpp->index_offset = index_offset;
      newpp->need_runtime = false;
      return;
    }
  else if (TREE_CODE (iv.base) == MULT_EXPR)
    {
      if (TREE_CODE (TREE_OPERAND (iv.base, 1)) != INTEGER_CST ||
	  int_cst_value (TREE_OPERAND (iv.base, 1)) != newpp->index_step)
	return;
      newpp->index = index;
      newpp->index_offset = index_offset;
      newpp->need_runtime = false;
      return;
    }
}

/* _2 = GV1;
   ...

case 1:
   _84 = i_83 + 200000;
   _85 = (long unsigned int) _84;
   _86 = _85 * 8; (* elem_size)
   _87 = _2 + _86;
   _88 = *_87;
   ...

case 2:
   _1944 = _1942 + x_1943;
   _1946 = z_1945 * 10000;
   _1947 = _1944 + _1946;
   _1948 = _1947 * 20;
   _1949 = (long unsigned int) _1948;
   _1950 = _1949 * 8;
   _1951 = _2 + _1950;
   _1952 = *_1951;
   ...
 */
static bool
record_ptr_plus_info (gimple *stmt, function *fn, ardecl *decl_info)
{
  gcc_assert (gimple_assign_rhs_code (stmt) == POINTER_PLUS_EXPR);
  tree lhs = gimple_get_lhs (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  tree rhs2 = gimple_assign_rhs2 (stmt);
  arptrplus *newpp = new arptrplus (stmt);

  if (!POINTER_TYPE_P (TREE_TYPE (rhs1)))
    return false;
  if (TREE_CODE (rhs2) == INTEGER_CST)
    {
      newpp->lhs = lhs;
      newpp->ptr_base = rhs1;
      newpp->index = build_int_cst (TREE_TYPE (rhs2),
				    int_cst_value (rhs2) /
				    decl_info->elem_size);
      decl_info->ptrpluses.safe_push (newpp);
      return true;
    }
  if (TREE_CODE (rhs2) != SSA_NAME)
    return false;
  gimple *def_stmt = SSA_NAME_DEF_STMT (rhs2);
  if (gimple_code (def_stmt) != GIMPLE_ASSIGN ||
      gimple_assign_rhs_code (def_stmt) != MULT_EXPR ||
      TREE_CODE (gimple_assign_rhs2 (def_stmt)) != INTEGER_CST ||
      int_cst_value (gimple_assign_rhs2 (def_stmt)) != decl_info->elem_size)
    return false;
  newpp->lhs = lhs;
  newpp->ptr_base = rhs1;
  newpp->index = gimple_assign_rhs1 (def_stmt);
  newpp->fn = fn;
  decl_info->ptrpluses.safe_push (newpp);

  push_cfun (fn);
  loop_optimizer_init (LOOPS_NORMAL | LOOPS_HAVE_RECORDED_EXITS);
  scev_initialize ();
  tree index = gimple_assign_rhs1 (def_stmt);

  gcc_assert (TREE_CODE (index) == SSA_NAME);
  gimple *index_stmt = SSA_NAME_DEF_STMT (index);
  /* _85 = (long unsigned int) _84; */
  if (gimple_code (index_stmt) == GIMPLE_ASSIGN &&
      gimple_assign_rhs_code (index_stmt) == NOP_EXPR &&
      TREE_CODE (gimple_assign_rhs1 (index_stmt)) == SSA_NAME)
    {
      index = gimple_assign_rhs1 (index_stmt);
      index_stmt = SSA_NAME_DEF_STMT (index);
    }

  if (gimple_code (index_stmt) == GIMPLE_ASSIGN &&
      gimple_assign_rhs_code (index_stmt) == PLUS_EXPR &&
      TREE_CODE (gimple_assign_rhs2 (index_stmt)) == INTEGER_CST)
    record_index_info (def_stmt, gimple_assign_rhs1 (index_stmt),
		       int_cst_value (gimple_assign_rhs2 (index_stmt)),
		       newpp);
  else
    record_index_info (def_stmt, index, 0, newpp);

  scev_finalize ();
  loop_optimizer_finalize ();
  pop_cfun ();
  return true;
}

static bool
record_call_ptr_plus_info (gimple *stmt, tree actual_arg, ardecl *decl_info)
{
  gcc_assert (gimple_code (stmt) == GIMPLE_CALL);
  tree callee_decl = gimple_call_fndecl (stmt);
  /* function pointer / variadic function */
  if (!callee_decl || !DECL_STRUCT_FUNCTION (callee_decl) ||
      stdarg_p (TREE_TYPE (callee_decl)))
    return false;
  struct cgraph_node *node = cgraph_node::get (callee_decl);
  if (!node->has_gimple_body_p ())
    return false;

  tree callee_arg = DECL_ARGUMENTS (callee_decl);
  if (!callee_arg)
    return false;
  int used_arg_index = -1;
  /* get arg node and index */
  for (unsigned i = 0; i < gimple_call_num_args (stmt); i++)
    {
      tree arg = gimple_call_arg (stmt, i);
      if (arg == actual_arg)
	{
	  if (used_arg_index != -1)
	    return false;
	  used_arg_index = i;
	}
      if (used_arg_index == -1)
	callee_arg = TREE_CHAIN (callee_arg);
    }
  if (used_arg_index == -1)
    return false;

  /* check all user of callee */
  /* TODO : Generate two copies depending on whether the parameter can
     be optimized */
  for (struct cgraph_edge *e = node->callers; e; e = e->next_caller)
    {
      gimple *call_stmt = e->call_stmt;
      if (call_stmt == stmt)
	continue;
      tree arg = gimple_call_arg (call_stmt, used_arg_index);
      if (TREE_CODE (arg) != SSA_NAME)
	return false;
      gimple *def_stmt = SSA_NAME_DEF_STMT (arg);
      if (!gimple_assign_single_p (def_stmt))
	return false;
      tree def_decl = gimple_assign_rhs1 (def_stmt);
      if (decl_info->find_assign_decl (def_decl))
	continue;
      ardecl *check_decl = find_global_decl (def_decl);
      if (!check_decl || !check_decl->can_opt)
	return false;
      decl_info->assign_decls.safe_push (check_decl);
      check_decl->assign_decls.safe_push (decl_info);
    }

  function *callee_func = DECL_STRUCT_FUNCTION (callee_decl);
  tree arg_ssa = ssa_default_def (callee_func, callee_arg);
  if (!arg_ssa)
    return false;

  imm_use_iterator imm_iter;
  gimple *use_stmt = NULL;
  /* Only POINTER_PLUS_EXPR
     TODO : other MEM_REF */
  FOR_EACH_IMM_USE_STMT (use_stmt, imm_iter, arg_ssa)
    {
      if (gimple_assign_rhs_code (use_stmt) != POINTER_PLUS_EXPR)
        {
	  end_imm_use_stmt_traverse (&imm_iter);
	  return false;
        }
      if (!record_ptr_plus_info (use_stmt, callee_func, decl_info))
        {
	  end_imm_use_stmt_traverse (&imm_iter);
	  return false;
        }
    }
  return true;
}

/* get function of stmt (or other way) */
static function *
get_stmt_function (gimple *stmt)
{
  basic_block bb;
  struct cgraph_node *node;
  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
  {
    if (!node->decl || !DECL_STRUCT_FUNCTION (node->decl))
      continue;
    FOR_ALL_BB_FN (bb, DECL_STRUCT_FUNCTION (node->decl))
      {
	gimple_stmt_iterator gsi;
	for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
	  {
	    if (gsi_stmt (gsi) == stmt)
	      return DECL_STRUCT_FUNCTION (node->decl);
	  }
      }
  }
  gcc_unreachable ();
}

static bool
analysis_load_stmt (gimple *load_stmt, ardecl *decl_info)
{
  if (!gimple_assign_single_p (load_stmt))
    return false;
  tree lhs = gimple_get_lhs (load_stmt);
  if (TREE_CODE (lhs) != SSA_NAME)
    return false;

  function *fn = get_stmt_function (load_stmt);

  imm_use_iterator imm_iter;
  gimple *use_stmt = NULL;
  auto_vec<tree> check_nodes;
  FOR_EACH_IMM_USE_STMT (use_stmt, imm_iter, lhs)
    {
      /* assign to another GV */
      if (gimple_assign_single_p (use_stmt))
	{
	  tree use_lhs = gimple_get_lhs (use_stmt);
	  tree use_rhs = gimple_assign_rhs1 (use_stmt);
	  if (decl_info->find_assign_decl (use_lhs))
	    continue;
	  ardecl *lhs_decl = find_global_decl (use_lhs);
	  if (use_rhs != lhs ||
	      !lhs_decl ||
	      !lhs_decl->can_opt)
	    {
	      end_imm_use_stmt_traverse (&imm_iter);
	      return false;
	    }
	  decl_info->assign_decls.safe_push (lhs_decl);
	  lhs_decl->assign_decls.safe_push (decl_info);
	}
      /* maybe load/store */
      else if (gimple_code (use_stmt) == GIMPLE_ASSIGN)
	{
	  if (gimple_assign_rhs_code (use_stmt) != POINTER_PLUS_EXPR)
	    {
	      end_imm_use_stmt_traverse (&imm_iter);
	      return false;
	    }
	  check_nodes.safe_push (gimple_get_lhs (use_stmt));
	  if (!record_ptr_plus_info (use_stmt, fn, decl_info))
	    {
	      end_imm_use_stmt_traverse (&imm_iter);
	      return false;
	    }
	}
      /* maybe call arg */
      else if (gimple_code (use_stmt) == GIMPLE_CALL)
	{
	  if (!record_call_ptr_plus_info (use_stmt, lhs, decl_info))
	    {
	      end_imm_use_stmt_traverse (&imm_iter);
	      return false;
	    }
	}
      else
        {
	  end_imm_use_stmt_traverse (&imm_iter);
	  return false;
        }
    }
  for (tree check_node : check_nodes)
    {
      if (!is_legal_use_p (check_node))
	return false;
    }
  return true;
}

static void
check_assign_decls (void)
{
  for (ardecl *decl : global_decls)
    {
      for (ardecl *assign_decl : decl->assign_decls)
	{
	  if (!decl->can_opt)
	    {
	      assign_decl->can_opt = false;
	      continue;
	    }
	  if (assign_decl == decl)
	    continue;
	  if (assign_decl->malloc_size != decl->malloc_size ||
	      assign_decl->init_offset != decl->init_offset)
	    {
	      decl->can_opt = false;
	      assign_decl->can_opt = false;
	    }
	  if (assign_decl->group_size != decl->group_size)
	    {
	      if (assign_decl->group_size == 0)
		{
		  assign_decl->group_size = decl->group_size;
		  assign_decl->num_of_groups =
		      assign_decl->malloc_size / assign_decl->group_size;
		}
	      else if (decl->group_size == 0)
		{
		  decl->group_size = assign_decl->group_size;
		  decl->num_of_groups = decl->malloc_size / decl->group_size;
		}
	      else
		{
		  decl->can_opt = false;
		  assign_decl->can_opt = false;
		}
	    }
	}
    }
}

static void
record_global_decls (void)
{
  /* Add access information from functions to global decls. */
  for (arfunction *fn : all_functions)
    {
      for (ardecl *fndecl : fn->load_decls)
	{
	  if (fndecl->scope_type == GLOBAL_DECL)
	    {
	      add_to_global_decls (fndecl->decl, fndecl);
	    }
	  else
	    {
	      /* ARGUMENT_DECL */
	      cgraph_edge *e;
	      auto_vec<tree> visited_globals;
	      for (e = fn->node->callers; e; e = e->next_caller)
		{
		  /* Only arg from global variable */
		  /* TODO : arg = gv + offset(constant) */
		  gimple *call = e->call_stmt;
		  if (!call)
		    continue;
		  tree actual_arg =
		      gimple_call_arg (call, fndecl->argument_index);
		  if (TREE_CODE (actual_arg) != SSA_NAME)
		    continue;
		  gimple *def_stmt = SSA_NAME_DEF_STMT (actual_arg);
		  if (!gimple_assign_load_p (def_stmt))
		    continue;
		  tree def_var = gimple_assign_rhs1 (def_stmt);
		  if (!DECL_P (def_var) || !is_global_var (def_var))
		    continue;
		  if (visited_globals.contains (def_var))
		    continue;
		  visited_globals.safe_push (def_var);
		  add_to_global_decls (def_var, fndecl);
		}
	    }
	}
    }

  for (ardecl *decl_info : global_decls)
    {
      /* calculate group_size */
      decl_info->group_size = 0;
      for (unsigned i = 0; i < decl_info->steps.length (); i++)
	{
	  if (decl_info->group_size == 0)
	    decl_info->group_size = decl_info->steps[i];
	  else if (decl_info->steps[i] != 0 &&
		   decl_info->group_size != decl_info->steps[i])
	    {
	      decl_info->group_size = -1;
	      decl_info->can_opt = false;
	      if (dump_file)
		fprintf (dump_file, "\ncannot opt: can't get group_size ");
	      break;
	    }
	}
      if (decl_info->group_size < 0)
	{
	  decl_info->group_size = -1;
	  decl_info->can_opt = false;
	}

      /* calculate elem_size */
      decl_info->elem_size = decl_info->elem_sizes[0];
      for (unsigned i = 1; i < decl_info->elem_sizes.length (); i++)
	{
	  if (decl_info->elem_size != decl_info->elem_sizes[i])
	    {
	      decl_info->elem_size = -1;
	      decl_info->can_opt = false;
	      if (dump_file)
		fprintf (dump_file, "\ncannot opt: can't get elem_size ");
	      break;
	    }
	}
      if (!decl_info->can_opt)
	continue;

      /* get malloc info/all ref info */
      ipa_ref *ref;
      for (unsigned i = 0; decl_info->vnode->iterate_referring (i, ref); i++)
	{
	  gimple *use_stmt = ref->stmt;
	  if (!use_stmt)
	    {
	      decl_info->can_opt = false;
	      continue;
	    }
	  if (dump_file)
	    {
	      fprintf (dump_file, "\niterate_referring : ");
	      print_gimple_stmt (dump_file, ref->stmt, 0);
	    }
	  if (ref->use == IPA_REF_ADDR)
	    {
	      /* LBM_allocateGrid (&GV1); */
	      if (gimple_code (use_stmt) == GIMPLE_CALL)
		{
		  if (!analysis_call_stmt (use_stmt, decl_info))
		    {
		      decl_info->can_opt = false;
		      if (dump_file)
			fprintf (dump_file,
				 "\ncannot opt: can't analysis call stmt ");
		    }
		}
	      else
		{
		  decl_info->can_opt = false;
		  if (dump_file)
		    fprintf (dump_file,
			     "\ncannot opt: can't get legal REF_ADDR ");
		}
	    }
	  else if (ref->use == IPA_REF_STORE)
	    {
	      /* MEM[(double * *)&GV1] = 0B;
		 GV1 = 0B
		 GV1 = _1 (from another GV)
	       */
	      if (!analysis_store_stmt (use_stmt, decl_info))
		{
		  decl_info->can_opt = false;
		  if (dump_file)
		    fprintf (dump_file,
			     "\ncannot opt: can't analysis store stmt ");
		}
	    }
	  else if (ref->use == IPA_REF_LOAD)
	    {
	      /* _40 = GV1;
		 _2103 = MEM[(double * *)&GV1]; */
	      if (!analysis_load_stmt (use_stmt, decl_info))
		{
		  decl_info->can_opt = false;
		  if (dump_file)
		    fprintf (dump_file,
			     "\ncannot opt: can't analysis load stmt ");
		}
	    }
	  else
	    {
	      decl_info->can_opt = false;
	      if (dump_file)
		fprintf (dump_file, "\ncannot opt: can't get valid REF ");
	    }
	}
      if (decl_info->malloc_size == -1)
	{
	  decl_info->can_opt = false;
	  if (dump_file)
	    fprintf (dump_file, "\ncannot opt: no malloc info ");
	}
    }

  check_assign_decls ();

  if (dump_file)
    {
      fprintf (dump_file, "\n======== collected global decl ==========\n");
      for (ardecl *decl_info : global_decls)
	{
	  decl_info->dump (dump_file);
	}
      fprintf (dump_file, "\n======== collected global decl end ==========\n");
    }
}

/* If step is equal to group_size, a new iv is generated. */
static tree
get_new_iv (loop_p curr_loop, tree index, HOST_WIDE_INT step)
{
  gcc_assert (TREE_CODE (index) == SSA_NAME);

  for (arloop *ar_loop : all_loops)
    if (ar_loop->loop == curr_loop &&
	ar_loop->step == step)
      return ar_loop->new_iv;

  gimple *def_stmt = SSA_NAME_DEF_STMT (index);
  if (gimple_code (def_stmt) != GIMPLE_PHI ||
      gimple_phi_num_args (def_stmt) != 2)
    return NULL_TREE;
  tree const_in = NULL_TREE;
  tree iter_ssa = NULL_TREE;
  tree type = TREE_TYPE (index);
  if (TREE_CODE (PHI_ARG_DEF (def_stmt, 0)) == INTEGER_CST &&
      TREE_CODE (PHI_ARG_DEF (def_stmt, 1)) == SSA_NAME)
    {
      const_in = PHI_ARG_DEF (def_stmt, 0);
      iter_ssa = PHI_ARG_DEF (def_stmt, 1);
    }
  else if (TREE_CODE (PHI_ARG_DEF (def_stmt, 1)) == INTEGER_CST &&
	   TREE_CODE (PHI_ARG_DEF (def_stmt, 0)) == SSA_NAME)
    {
      const_in = PHI_ARG_DEF (def_stmt, 1);
      iter_ssa = PHI_ARG_DEF (def_stmt, 0);
    }
  else
    return NULL_TREE;

  gimple *iter_stmt = SSA_NAME_DEF_STMT (iter_ssa);
  if (gimple_code (iter_stmt) != GIMPLE_ASSIGN ||
      gimple_assign_rhs_code (iter_stmt) != PLUS_EXPR ||
      gimple_assign_rhs1 (iter_stmt) != index ||
      TREE_CODE (gimple_assign_rhs2 (iter_stmt)) != INTEGER_CST ||
      int_cst_value (gimple_assign_rhs2 (iter_stmt)) != step)
    return NULL_TREE;

  tree new_iv = make_temp_ssa_name (type, NULL, "new_iv");
  gphi *phi = create_phi_node (new_iv, gimple_bb (def_stmt));
  gimple_stmt_iterator gsi = gsi_for_stmt (iter_stmt);
  tree new_iter = gimplify_build2 (&gsi, PLUS_EXPR, type,
				   new_iv, build_int_cst (type, 1));
  tree new_const = build_int_cst (type, int_cst_value (const_in) / step);
  edge e;
  edge_iterator ei;
  FOR_EACH_EDGE (e, ei, gimple_bb (def_stmt)->preds)
    {
      if (e->src == gimple_bb (iter_stmt))
	add_phi_arg (phi, new_iter, e, UNKNOWN_LOCATION);
      else
	add_phi_arg (phi, new_const, e, UNKNOWN_LOCATION);
    }
  arloop *new_loop = new arloop (curr_loop, new_iv, step);
  all_loops.safe_push (new_loop);
  return new_iv;
}

/* (index + init_offset) % group_size * num_of_groups +
   (index + init_offset) / group_size - init_offset
 */
static void
rewrite_all_refs (void)
{
  auto_vec<gimple *> visited_stmts;
  for (ardecl *decl : global_decls)
    {
      if (!decl->can_opt)
	continue;
      for (arptrplus *pp : decl->ptrpluses)
	{
	  if (visited_stmts.contains (pp->stmt))
	    continue;
	  visited_stmts.safe_push (pp->stmt);
	  if (dump_file)
	    {
	      fprintf (dump_file, "\nold stmt: ");
	      print_gimple_stmt (dump_file, pp->stmt, 0);
	    }
	  tree index_type = TREE_TYPE (pp->index);
	  tree offset_type = TREE_TYPE (gimple_assign_rhs2 (pp->stmt));
	  if (TREE_CODE (pp->index) == INTEGER_CST)
	    {
	      /* Direct calculation of constants */
	      gcc_assert (index_type == offset_type);
	      HOST_WIDE_INT old_index = int_cst_value (pp->index);
	      HOST_WIDE_INT new_index =
		  (old_index + decl->init_offset) % decl->group_size;
	      new_index *= decl->num_of_groups;
	      new_index += (old_index + decl->init_offset) / decl->group_size;
	      new_index -= decl->init_offset;
	      HOST_WIDE_INT new_size = new_index * decl->elem_size;
	      gimple_assign_set_rhs2 (pp->stmt,
				      build_int_cst (index_type, new_size));
	      update_stmt (pp->stmt);
	    }
	  else if (TREE_CODE (pp->index) == SSA_NAME)
	    {
	      push_cfun (pp->fn);
	      gimple_stmt_iterator gsi = gsi_for_stmt (pp->stmt);
	      if (pp->need_runtime || pp->index_step != decl->group_size)
		{
		  gcc_assert (index_type == offset_type);
		  tree init_offset =
		      build_int_cst (index_type, decl->init_offset);
		  tree group_size =
		      build_int_cst (index_type, decl->group_size);
		  tree num_of_groups =
		      build_int_cst (index_type, decl->num_of_groups);
		  tree elem_size = build_int_cst (index_type, decl->elem_size);

		  tree base_index = gimplify_build2 (&gsi, PLUS_EXPR,
						     index_type,
						     pp->index, init_offset);
		  tree mod_res = gimplify_build2 (&gsi, TRUNC_MOD_EXPR,
						  index_type,
						  base_index, group_size);
		  tree mul_res = gimplify_build2 (&gsi, MULT_EXPR, index_type,
						  mod_res, num_of_groups);
		  tree div_res = gimplify_build2 (&gsi, TRUNC_DIV_EXPR,
						  index_type,
						  base_index, group_size);
		  tree add_res = gimplify_build2 (&gsi, PLUS_EXPR, index_type,
						  mul_res, div_res);
		  tree new_index = gimplify_build2 (&gsi, MINUS_EXPR,
						    index_type,
						    add_res, init_offset);
		  tree new_size = gimplify_build2 (&gsi, MULT_EXPR, index_type,
						   new_index, elem_size);
		  gimple_assign_set_rhs2 (pp->stmt, new_size);
		  update_stmt (pp->stmt);
		}
	      else
		{
		  HOST_WIDE_INT tmp = (pp->index_offset + decl->init_offset) %
				      decl->group_size;
		  if (tmp < 0)
		    tmp +=  decl->group_size;
		  tmp *= decl->num_of_groups;
		  tmp += (pp->index_offset + decl->init_offset) /
			 decl->group_size;
		  if (pp->index_offset + decl->init_offset < 0)
		    tmp -= 1;
		  tmp -= decl->init_offset;
		  tree group_size =
		      build_int_cst (index_type, decl->group_size);
		  tree new_index_offset = build_int_cst (index_type, tmp);
		  tree elem_size = build_int_cst (index_type, decl->elem_size);

		  tree new_iter = get_new_iv (loop_containing_stmt (pp->stmt),
					      pp->index, decl->group_size);
		  if (!new_iter)
		    new_iter = gimplify_build2 (&gsi, FLOOR_DIV_EXPR,
						index_type,
						pp->index, group_size);
		  tree new_index = gimplify_build2 (&gsi, PLUS_EXPR,
						    index_type, new_iter,
						    new_index_offset);
		  if (index_type != offset_type)
		    new_index = gimplify_build1 (&gsi, NOP_EXPR,
						 offset_type, new_index);
		  tree new_size = gimplify_build2 (&gsi, MULT_EXPR,
						   offset_type,
						   new_index, elem_size);
		  gimple_assign_set_rhs2 (pp->stmt, new_size);
		  update_stmt (pp->stmt);
		}
	      pop_cfun ();
	    }
	  else
	    gcc_unreachable ();
	  if (dump_file)
	    {
	      fprintf (dump_file, "\nnew stmt: ");
	      print_gimple_stmt (dump_file, pp->stmt, 0);
	    }
	}
    }
}

unsigned int
ipa_arrayremapping_analysis (void)
{
  record_all_accesses ();
  record_global_decls ();
  rewrite_all_refs ();
  return 0;
}

namespace {
const pass_data pass_data_ipa_arrayremapping =
{
  IPA_PASS, /* type */
  "arrayremapping", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_IPA_ARRAY_REMAPPING, /* tv_id */
  0, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  ( TODO_dump_symtab ), /* todo_flags_finish */
};

class pass_ipa_arrayremapping : public simple_ipa_opt_pass
{
public:
  pass_ipa_arrayremapping (gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_data_ipa_arrayremapping, ctxt)
  {}

  /* opt_pass methods: */
  virtual bool gate (function *)
    {
      return (optimize >= 3
	      && flag_ipa_arrayremapping
	      /* Don't bother doing anything if the program has errors.  */
	      && !seen_error ()
	      && flag_lto_partition == LTO_PARTITION_ONE
	      /* Only enable struct optimizations in C since other
		 languages' grammar forbid.  */
	      && lang_c_p ()
	      /* Only enable struct optimizations in lto or whole_program.  */
	      && (in_lto_p || flag_whole_program));
    }

  virtual unsigned int execute (function *)
    {
      return ipa_arrayremapping_analysis ();
    }

}; // class pass_ipa_arrayremapping

} // anon namespace

simple_ipa_opt_pass *
make_pass_ipa_arrayremapping (gcc::context *ctxt)
{
  return new pass_ipa_arrayremapping (ctxt);
}
