/* Aggregate _Bool variables
   Copyright (C) 2008-2026 Free Software Foundation, Inc.

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "tree-pass.h"
#include "ssa.h"
#include "gimple-iterator.h"
#include "tree-iterator.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "tree-pretty-print.h"
#include "gimple-pretty-print.h"
#include "cfghooks.h"
#include "tree-cfg.h"
#include "cfgloop.h"
#include "tree-eh.h"

/**
 * This pass implements aggregation of boolean.
 * - Scalar boolean variables are replaced with bit operations on a 64-bit
 *   aggregation variable.
 * - Each boolean is assigned a unique bit position in the aggregation
 *   variable.
 * - Boolean assignments are transformed to bit sets/clears on the aggregation.
 *   variable using conditionals.
 * - Boolean uses are transformed bit extracts from the aggregation variable.
 *
 * This optimization reduces the number of boolean variables and can improve
 * performance on architectures where bit operations are efficient.
 *
 * Example transformation:
 *   Before:
 *     bool a = condition1; bool b = condition2;
 *     if (a && b) ...
 *
 *   After:
 *     uint64_t bool_flags = 0;
 *     if (condition1) bool_flags |= (1ULL << 0);
 *     else bool_flags &= ~(1ULL << 0);
 *     if (condition2) bool_flags |= (1ULL << 1);
 *     else bool_flags &= ~(1ULL << 1);
 *     uint64_t t1 = bool_flags & (1ULL << 0);
 *     bool t2 = (t1 != 0);
 *     uint64_t t3 = bool_flags & (1ULL << 1);
 *     bool t4 = (t3 != 0);
 *     if (t2 && t4) ...
 */

typedef unsigned long long AGGREGATION_TYPE;

struct bool_var
{
  tree var;
  int bit_loc;
  vec<gimple *> all_uses;
  bool can_optimize;
};

static bool
is_bool_var (tree var)
{
  return (VAR_P (var) || TREE_CODE (var) == PARM_DECL)
	 && TREE_TYPE (var)
	 && TREE_CODE (TREE_TYPE (var)) == BOOLEAN_TYPE
	 && !TREE_STATIC (var)
	 && !TREE_THIS_VOLATILE (var)
	 && !DECL_REGISTER (var);
}

/* Generate : bool_flags &= ~(1 << offset) */
static void
create_bit_and_store (gimple_stmt_iterator &gsi, tree bool_flags,
		      tree type, int offset)
{
  AGGREGATION_TYPE new_num = ~((AGGREGATION_TYPE) 1 << offset);
  tree new_num_t = build_int_cst (type, new_num);
  tree and_expr = build2 (BIT_AND_EXPR, type, bool_flags, new_num_t);
  gimple *new_stmt = gimple_build_assign (bool_flags, and_expr);
  gsi_insert_after (&gsi, new_stmt, GSI_NEW_STMT);
}

/* Generate : bool_flags |= (1 << offset) */
static void
create_bit_or_store (gimple_stmt_iterator &gsi, tree bool_flags,
		     tree type, int offset)
{
  AGGREGATION_TYPE new_num = ((AGGREGATION_TYPE) 1 << offset);
  tree new_num_t = build_int_cst (type, new_num);
  tree or_expr = build2 (BIT_IOR_EXPR, type, bool_flags, new_num_t);
  gimple *new_stmt = gimple_build_assign (bool_flags, or_expr);
  gsi_insert_after (&gsi, new_stmt, GSI_NEW_STMT);
}

/* bool = xxx =>
 *   new_cond = xxx
 *   if(new_cond) bool_flags |= (1 << offset)
 *   else bool_flags &= ~(1 << offset)
 */
static void
create_variable_store (gimple_stmt_iterator &gsi, tree bool_flags,
		       tree type, int offset, gimple *stmt, tree lhs)
{
  basic_block bb = gsi_bb (gsi);
  struct loop *orig_loop = bb->loop_father;
  edge e = split_block (bb, stmt);
  basic_block rest_bb = e->dest;
  basic_block then_bb = create_empty_bb (bb);
  basic_block else_bb = create_empty_bb (then_bb);
  rest_bb->prev_bb = else_bb;
  else_bb->next_bb = rest_bb;
  add_bb_to_loop (then_bb, orig_loop);
  add_bb_to_loop (else_bb, orig_loop);

  gcc_assert (TREE_CODE (TREE_TYPE (lhs)) == BOOLEAN_TYPE);
  tree cond = create_tmp_var (TREE_TYPE (lhs), "new_cond");
  gimple_set_op (stmt, 0, cond);
  update_stmt (stmt);
  tree zero = build_int_cst (TREE_TYPE (lhs), 0);
  gcond *cond_stmt = gimple_build_cond (NE_EXPR, cond,
					zero,
					NULL_TREE, NULL_TREE);
  gsi_insert_after (&gsi, cond_stmt, GSI_NEW_STMT);
  edge fall = find_fallthru_edge (bb->succs);
  if (fall)
    remove_edge (fall);
  make_edge (bb, then_bb, EDGE_TRUE_VALUE);
  make_edge (bb, else_bb, EDGE_FALSE_VALUE);

  // then bb
  gimple_stmt_iterator gsi_then = gsi_start_bb (then_bb);
  create_bit_or_store (gsi_then, bool_flags, type, offset);
  make_edge (then_bb, rest_bb, EDGE_FALLTHRU);

  // else bb
  gimple_stmt_iterator gsi_else = gsi_start_bb (else_bb);
  create_bit_and_store (gsi_else, bool_flags, type, offset);
  make_edge (else_bb, rest_bb, EDGE_FALLTHRU);

  free_dominance_info (CDI_DOMINATORS);
  calculate_dominance_info (CDI_DOMINATORS);
}

/* t1 = bool_flags & (1 << offset)
 * t2 = t1 != 0
 */
static tree
create_new_use (gimple_stmt_iterator &gsi, tree bool_flags,
		tree type, int offset, tree ori_bool)
{
  AGGREGATION_TYPE new_num = ((AGGREGATION_TYPE) 1 << offset);
  tree new_num_t = build_int_cst (type, new_num);
  tree and_expr = build2 (BIT_AND_EXPR, type, bool_flags, new_num_t);
  tree tmp1 = create_tmp_var (type, "tmp");
  gimple *stmt1 = gimple_build_assign (tmp1, and_expr);
  gsi_insert_before (&gsi, stmt1, GSI_NEW_STMT);

  tree tmp2 = create_tmp_var (TREE_TYPE (ori_bool), "tmp");
  tree zero = build_int_cst (type, 0);
  tree ne_expr = build2 (NE_EXPR, type, tmp1, zero);
  gimple *stmt2 = gimple_build_assign (tmp2, ne_expr);
  gsi_insert_after (&gsi, stmt2, GSI_NEW_STMT);

  return tmp2;
}

static tree
create_new_bool_flags (function *fun, tree type)
{
  tree bool_flags = create_tmp_var (type, "bool_flags");
  tree zero = build_int_cst (type, 0);
  gimple *init_stmt = gimple_build_assign (bool_flags, zero);
  basic_block entry_bb = ENTRY_BLOCK_PTR_FOR_FN (fun)->next_bb;
  gimple_stmt_iterator gsi = gsi_after_labels (entry_bb);
  gsi_insert_before (&gsi, init_stmt, GSI_NEW_STMT);
  return bool_flags;
}

static void
do_bool_aggregate_1 (function *fun, vec<bool_var> bool_vars)
{
  tree aggr_type = build_nonstandard_integer_type (64, 1);
  gcc_assert (TYPE_PRECISION (aggr_type) == 64);
  gcc_assert (TYPE_MODE (aggr_type) == DImode);
  tree bool_flags = create_new_bool_flags (fun, aggr_type);
  int bit_loc_base = 0;

  for (unsigned idx = 0; idx < bool_vars.length(); idx++)
  {
    bool_var &var = bool_vars[idx];
    if (!var.can_optimize)
      continue;
    if (var.bit_loc > 0 && var.bit_loc % 64 == 0)
    {
      bool_flags = create_new_bool_flags (fun, aggr_type);
      bit_loc_base += 64;
    }
    gcc_assert (bool_flags != NULL_TREE);
    gcc_assert (var.bit_loc >= bit_loc_base);
    int offset = var.bit_loc - bit_loc_base;

    for (gimple *stmt : var.all_uses)
    {
      gimple_stmt_iterator gsi;
      switch (gimple_code (stmt))
	{
	case GIMPLE_ASSIGN:
	  {
	    for (unsigned i = 0; i < gimple_num_ops (stmt); i++)
	    {
	      gsi = gsi_for_stmt (stmt);
	      tree op = gimple_op (stmt, i);
	      if (op != var.var)
		continue;
	      if (i == 0)
	      {
		if (gimple_assign_rhs_class (stmt) == GIMPLE_SINGLE_RHS)
		{
		  tree rhs = gimple_assign_rhs1 (stmt);
		  gcc_assert (rhs);
		  if (integer_zerop (rhs))
		  {
		    /* bool = 0 => bool_flags &= ~(1 << offset) */
		    create_bit_and_store (gsi, bool_flags, aggr_type, offset);
		    gsi = gsi_for_stmt (stmt);
		    gsi_remove (&gsi, true);
		    break;
		  }
		  else if (integer_onep (rhs))
		  {
		    /* bool = 1 => bool_flags |= (1 << offset) */
		    create_bit_or_store (gsi, bool_flags, aggr_type, offset);
		    gsi = gsi_for_stmt (stmt);
		    gsi_remove (&gsi, true);
		    break;
		  }
		}
		/* bool = xxx =>
		     new_cond = xxx
		     if(new_cond) bool_flags |= (1 << offset)
		     else bool_flags &= ~(1 << offset) */
		create_variable_store (gsi, bool_flags, aggr_type,
				       offset, stmt, op);
		continue;
	      }
	      /* xxx = bool [+ else] =>
		   t1 = bool_flags & (1 << offset)
		   t2 = t1 != 0
		   xxx = t2  [+ else] */
	      tree new_use = create_new_use (gsi, bool_flags,
					     aggr_type, offset, op);
	      gimple_set_op (stmt, i, new_use);
	      update_stmt (stmt);
	    }
	    break;
	  }
	case GIMPLE_CALL:
	  {
	    tree lhs = gimple_call_lhs (stmt);
	    for (unsigned i = 0; i < gimple_num_ops (stmt); i++)
	    {
	      gsi = gsi_for_stmt (stmt);
	      tree op = gimple_op (stmt, i);
	      if (op != var.var)
		continue;
	      if (i == 0 && lhs)
	      {
		/* bool = func (xxx) =>
		     tmp = func (xxx)
		     if(tmp) bool_flags |= (1 << offset)
		     else bool_flags &= ~(1 << offset) */
		gcc_assert (lhs == op);
		create_variable_store (gsi, bool_flags, aggr_type,
				       offset, stmt, op);
		continue;
	      }
	      /* [xxx = ]func (..., bool, ...) */
	      tree new_use = create_new_use (gsi, bool_flags,
					     aggr_type, offset, op);
	      gimple_set_op (stmt, i, new_use);
	      update_stmt (stmt);
	    }
	    break;
	  }
	case GIMPLE_COND:
	case GIMPLE_RETURN:
	  {
	    for (unsigned i = 0; i < gimple_num_ops (stmt); i++)
	    {
	      gsi = gsi_for_stmt (stmt);
	      tree op = gimple_op (stmt, i);
	      if (op != var.var)
		continue;
	      tree new_use = create_new_use (gsi, bool_flags,
					     aggr_type, offset, op);
	      gimple_set_op (stmt, i, new_use);
	      update_stmt (stmt);
	    }
	    break;
	  }
	default:
	  if (dump_file)
	  {
	    fprintf (dump_file, "unexpected stmt :");
	    print_gimple_stmt (dump_file, stmt, 0, TDF_SLIM);
	  }
	  gcc_assert (false);
	  break;
	}
    }
  }
}

/* Check if TARGET is used in child nodes of T. */
static bool
has_complex_use (tree t, tree target)
{
  if (t == target)
    return true;
  if (!t || VAR_P (t) || TREE_CODE (t) == PARM_DECL)
    return false;
  else if (EXPR_P (t) || REFERENCE_CLASS_P (t))
  {
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); i++)
      if (has_complex_use (TREE_OPERAND (t, i), target))
	return true;
  }
  return false;
}

static unsigned int
do_bool_aggregate (function *fun)
{
  vec<bool_var> bool_vars;
  size_t i;
  tree var;
  unsigned int save_bit;

  /* Collect bool variables */
  bool_vars.create (0);
  save_bit = 0;
  FOR_EACH_LOCAL_DECL (fun, i, var)
    {
      if (dump_file)
      {
	if (DECL_NAME (var))
	  fprintf (dump_file, "check '%s'",
		   IDENTIFIER_POINTER (DECL_NAME (var)));
	else
	  fprintf (dump_file, "check <unknown>");
      }

      if (is_bool_var (var))
	{
	  bool_var new_var;
	  new_var.var = var;
	  new_var.all_uses.create(0);
	  new_var.can_optimize = true;
	  save_bit++;
	  bool_vars.safe_push (new_var);
	  if (dump_file)
	    fprintf (dump_file, " is bool variable\n");
	}
      else
	{
	  if (dump_file)
	    fprintf (dump_file, " not bool variable\n");
	}
    }

  if (save_bit < param_bool_aggregate_min_num)
    {
      if (dump_file)
	fprintf (dump_file, "No enough bool variables\n");
      return 0;
    }

  /* Collect use stmt */
  basic_block bb;
  gimple_stmt_iterator gsi;
  FOR_EACH_BB_FN (bb, fun)
  {
    for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
    {
      gimple *stmt = gsi_stmt (gsi);
      for (unsigned i = 0; i < gimple_num_ops (stmt); i++)
      {
	tree op = gimple_op (stmt, i);
	if (op && (VAR_P (op) || TREE_CODE (op) == PARM_DECL))
	{
	  if (dump_file)
	  {
	    if (DECL_NAME (op))
	      fprintf (dump_file, "found '%s' in :",
		       IDENTIFIER_POINTER (DECL_NAME (op)));
	    else
	      fprintf (dump_file, "found <unknown> in :");
	    print_gimple_stmt (dump_file, stmt, 0, TDF_SLIM);
	  }
	  for (unsigned idx = 0; idx < bool_vars.length(); idx++)
	  {
	    if (bool_vars[idx].var == op)
	    {
	      if (!bool_vars[idx].all_uses.contains (stmt))
		bool_vars[idx].all_uses.safe_push (stmt);
	      /* clobber */
	      if (gimple_clobber_p (stmt))
		bool_vars[idx].can_optimize = false;
	      /* exception handling */
	      if (lookup_stmt_eh_lp (stmt) != 0)
		bool_vars[idx].can_optimize = false;
	    }
	  }
	}
	else
	{
	  for (unsigned idx = 0; idx < bool_vars.length(); idx++)
	  {
	    if (has_complex_use (op, bool_vars[idx].var))
	    {
	      if (dump_file)
	      {
		if (DECL_NAME (bool_vars[idx].var))
		  fprintf (dump_file, "'%s' has complex use in :",
			   IDENTIFIER_POINTER (DECL_NAME (bool_vars[idx].var)));
		else
		  fprintf (dump_file, "<unknown> has complex use in :");
		print_gimple_stmt (dump_file, stmt, 0, TDF_SLIM);
	      }
	      if (!bool_vars[idx].all_uses.contains (stmt))
		bool_vars[idx].all_uses.safe_push (stmt);
	      /* complex user (&b/...) */
	      bool_vars[idx].can_optimize = false;
	    }
	  }
	}
      }
    }
  }

  /* Clear bool variables */
  save_bit = 0;
  for (unsigned idx = 0; idx < bool_vars.length(); idx++)
  {
    if (bool_vars[idx].all_uses.length() < 2)
      bool_vars[idx].can_optimize = false;
    if (bool_vars[idx].all_uses.length() == 2
	&& (gsi_bb (gsi_for_stmt (bool_vars[idx].all_uses[0])) ==
	    gsi_bb (gsi_for_stmt (bool_vars[idx].all_uses[1]))))
      bool_vars[idx].can_optimize = false;
    if (bool_vars[idx].can_optimize)
    {
      bool_vars[idx].bit_loc = save_bit;
      save_bit++;
    }
    else
      bool_vars[idx].bit_loc = -1;
  }

  if (save_bit < param_bool_aggregate_min_num)
    {
      if (dump_file)
	fprintf (dump_file, "No enough optimizable bool variables\n");
      return 0;
    }

  if (dump_file)
  {
    for (unsigned idx = 0; idx < bool_vars.length(); idx++)
      {
	if (DECL_NAME (bool_vars[idx].var))
	  fprintf (dump_file, "\ncollected '%s' bit : %d can_opt: %d:%s\n",
		   IDENTIFIER_POINTER (DECL_NAME (bool_vars[idx].var)),
		   bool_vars[idx].bit_loc, bool_vars[idx].can_optimize,
		   function_name (fun));
	else
	  fprintf (dump_file, "\ncollected <unknown> bit : %d can_opt: %d:%s\n",
		   bool_vars[idx].bit_loc, bool_vars[idx].can_optimize,
		   function_name (fun));
	for (gimple *stmt : bool_vars[idx].all_uses)
	  print_gimple_stmt (dump_file, stmt, 0, TDF_SLIM);
      }
  }

  do_bool_aggregate_1 (fun, bool_vars);

  for (unsigned idx = 0; idx < bool_vars.length(); idx++)
    {
      bool_vars[idx].all_uses.release();
    }
  bool_vars.release ();
  return 0;
}

namespace {

const pass_data pass_data_bool_aggregate =
{
  GIMPLE_PASS, /* type */
  "bool_aggregate", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_TREE_BOOL_AGGREGATE, /* tv_id */
  PROP_gimple_any, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_cleanup_cfg, /* todo_flags_finish */
};

class pass_bool_aggregate : public gimple_opt_pass
{
public:
  pass_bool_aggregate (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_bool_aggregate, ctxt)
  {}

  bool gate (function *) final override
  {
    return flag_tree_bool_aggregate != 0;
  }
  unsigned int execute (function *fun) final override
  {
    return do_bool_aggregate (fun);
  }

 private:
}; // class pass_bool_aggregate

} // anon namespace

gimple_opt_pass *
make_pass_bool_aggregate (gcc::context *ctxt)
{
  return new pass_bool_aggregate (ctxt);
}
