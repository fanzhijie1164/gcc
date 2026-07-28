/* Hash Prefetch Pass
   Copyright (C) 2025 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "tree-pass.h"
#include "gimple-ssa.h"
#include "tree-pretty-print.h"
#include "fold-const.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-cfg.h"
#include "tree-ssa.h"
#include "cfghooks.h"
#include "diagnostic-core.h"
#include "dbgcnt.h"
#include "ssa.h"
#include "gimple-pretty-print.h"
#include "print-tree.h"
#include "tree-ssa-loop-ivopts.h"

/* This pass inserts prefetch instructions for hash table access patterns
   in hash-based applications.
   It identifies hash calculation sequences in the IR and generates
   speculative prefetch instructions for adjacent data regions:

   1) Pattern Detection:
      - Scans basic blocks for hash table access patterns
      - Analyzes memory access sequences that indicate stride-based access
      - Identifies hash table lookups with predictable memory layouts

   2) Prefetch Generation:
      - For detected hash patterns, the pass generates prefetch instructions
	for subsequent 4-byte regions (cur4-7, corresponding to original
	cur0-3 + 4 byte offset)
      - Prefetch targets are calculated using the same hash algorithms as
	the original access pattern, but with adjusted offsets

   3) Optimization Strategy:
      - Focuses on hash table access patterns where memory behavior is
	predictable through the hash function
      - Prefetchs likely future hash table accesses based on stride detection
      - Uses __builtin_prefetch to hint to the processor to load cache lines

   Target IR pattern to match:
   cur0 = *base_ptr;
   cur1 = MEM[(const uint8_t *)base_ptr + 1B];
   cur2 = MEM[(const uint8_t *)base_ptr + 2B];
   cur3 = MEM[(const uint8_t *)base_ptr + 3B];
   cur0_cast = (int) cur0;
   cur1_cast = (unsigned int) cur1;
   cur2_cast = (unsigned int) cur2;
   cur3_cast = (int) cur3;

   hash_table_cur0 = hash_table[0][cur0_cast];
   hash_table_cur3 = hash_table[0][cur3_cast];

   cur2_shift8 = cur2_cast << 8;
   hash_table_cur3_shift5 = hash_table_cur3 << 5;

   temp = hash_table_cur0 ^ cur1_cast;
   temp2 = cur2_shift8 ^ temp;
   temp3 = temp2 ^ hash_table_cur3_shift5;

   hash_2_value = temp & 1023;
   hash_3_value = temp2 & 65535;
   hash_value = temp3 & mf (D)->hash_mask;

   hash_addr = mf (D)->hash;

   hash_2_value_ext = (long unsigned int) hash_2_value;
   hash_2_value_offset = hash_2_value_ext * 4;
   hash_2_value_addr = hash_addr + hash_2_value_offset;
   hash_2 = *hash_2_value_addr;

   hash_3_value_add = hash_3_value + 1024;
   hash_3_value_ext = (long unsigned int) hash_3_value_add;
   hash_3_value_offset = hash_3_value_ext * 4;
   hash_3_value_addr = hash_addr + hash_3_value_offset;
   hash_3 = *hash_3_value_addr;

   hash_value_add = hash_value + 66560;
   hash_value_ext = (long unsigned int) hash_value_add;
   hash_value_offset = hash_value_ext * 4;
   hash_value_addr = hash_addr + hash_value_offset;
   hash = *hash_value_addr;

   When this pattern is detected, the pass generates speculative prefetches for:
   - hash_4 (based on hash_1 logic): prefetches hash memory region calculated
     from cur4-7 with hash_mask operation and 66560 offset
   - hash_5 (based on hash_2 logic): prefetches hash memory region calculated
     from cur4-7 with 1023 mask and no additional offset
   - hash_6 (based on hash_3 logic): prefetches hash memory region calculated
     from cur4-7 with 65535 mask and 1024 offset

   This optimization targets applications with predictable hash table access
   patterns where data locality can be exploited through speculative prefetch.
   The pass is effective for hash-heavy workloads where cache misses
   in hash table lookups dominate performance.
*/

/* Match pattern context - stores essential information for new IR generation */
struct hash_pattern_context
{
  bool found_cur0_to_cur3; /* Found all cur0, cur1, cur2, cur3 patterns */
  bool found_hash_1_value; /* Found hash_1 value pattern */
  bool found_hash_2_value; /* Found hash_2 value pattern */
  bool found_hash_3_value; /* Found hash_3 value pattern */
  bool match_conflict;     /* Found conflict pattern */
  tree base_ptr;           /* base pointer */
  tree hash_table;         /* hash_table - needed for hash table lookups */
  tree hash_value_addr;    /* Final hash_value_addr for transformation */
  tree mf_D_hash_mask;     /* mf(D)->hash_mask - needed for hash calculation */
  tree mf_D_hash;          /* mf(D)->hash - needed for final hash value */
  tree curs[4];            /* cur0, cur1, cur2, cur3 SSA names */

  void clear ()
  {
    found_cur0_to_cur3 = false;
    found_hash_1_value = false;
    found_hash_2_value = false;
    found_hash_3_value = false;
    match_conflict = false;
    base_ptr = hash_table = mf_D_hash_mask = mf_D_hash = nullptr;
    for (int i = 0; i < 4; i++)
      curs[i] = nullptr;
    hash_value_addr = nullptr;
  }

  bool found_all_pattern ()
  {
    return (found_cur0_to_cur3 && found_hash_1_value && found_hash_2_value
	    && found_hash_2_value && !match_conflict);
  }
};

/* Match the specific hash curs pattern */
static bool
match_hash_curs_pattern (gimple *stmt, hash_pattern_context &ctx)
{
  if (!is_gimple_assign (stmt) || gimple_assign_rhs_code (stmt) != MEM_REF)
    return false;

  /* Match cur0 to cur3 patterns. */
  /* First match: cur3 = MEM[(const uint8_t *)base_ptr + 3B]; */
  tree lhs = gimple_assign_lhs (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  if (TREE_CODE (lhs) != SSA_NAME)
    return false;
  tree base_ptr = TREE_OPERAND (rhs1, 0);
  if (TREE_CODE (base_ptr) != SSA_NAME)
    return false;

  /* TODO: Normalize the pointer, handle type conversion. */
  /* Check if the offset is 3B */
  tree off3 = TREE_OPERAND (rhs1, 1);
  if (!off3 || !tree_fits_shwi_p (off3) || tree_to_shwi (off3) != 3)
    return false;

  tree curs[4] = {nullptr, nullptr, nullptr, lhs};
  /* Match:
      cur0 = *base_ptr;
      cur1 = MEM[(const uint8_t *)base_ptr + 1B];
      cur2 = MEM[(const uint8_t *)base_ptr + 2B]; */
  imm_use_iterator use_iter;
  gimple *use_stmt;
  FOR_EACH_IMM_USE_STMT (use_stmt, use_iter, base_ptr)
  {
    if (use_stmt == stmt || gimple_bb (use_stmt) != gimple_bb (stmt))
      continue;
    if (!is_gimple_assign (use_stmt) ||
	gimple_assign_rhs_code (use_stmt) != MEM_REF)
      continue;

    tree use_lhs = gimple_assign_lhs (use_stmt);
    tree use_rhs1 = gimple_assign_rhs1 (use_stmt);
    if (TREE_CODE (use_lhs) != SSA_NAME)
      continue;
    tree use_ptr = TREE_OPERAND (use_rhs1, 0);
    if (!operand_equal_p (use_ptr, base_ptr, 0))
      continue;
    tree use_off = TREE_OPERAND (use_rhs1, 1);
    if (use_off && tree_fits_shwi_p (use_off))
    {
      int offset = tree_to_shwi (use_off);
      if (offset < 0 || offset > 2 || curs[offset] != nullptr)
	return false;
      curs[offset] = use_lhs;
    }
    else
      return false;
  }

  if (!curs[0] || !curs[1] || !curs[2] || !curs[3])
    return false;

  ctx.found_cur0_to_cur3 = true;
  ctx.base_ptr = base_ptr;
  ctx.curs[0] = curs[0];
  ctx.curs[1] = curs[1];
  ctx.curs[2] = curs[2];
  ctx.curs[3] = curs[3];

  /* Print the cur0-cur3 IR for debugging */
  if (dump_file && (dump_flags & TDF_DETAILS))
  {
    fprintf (dump_file, "  Found cur0-cur3 pattern:\n");
    fprintf (dump_file, "    cur0: ");
    print_generic_expr (dump_file, curs[0], TDF_SLIM);
    fprintf (dump_file, "\n");
    fprintf (dump_file, "    cur1: ");
    print_generic_expr (dump_file, curs[1], TDF_SLIM);
    fprintf (dump_file, "\n");
    fprintf (dump_file, "    cur2: ");
    print_generic_expr (dump_file, curs[2], TDF_SLIM);
    fprintf (dump_file, "\n");
    fprintf (dump_file, "    cur3: ");
    print_generic_expr (dump_file, curs[3], TDF_SLIM);
    fprintf (dump_file, "\n");
  }
  return true;
}

/* Match the hash_1_value pattern which calculated:
   Target IR pattern to match:
   cur0 = *base_ptr;
   cur1 = MEM[(const uint8_t *)base_ptr + 1B];
   cur2 = MEM[(const uint8_t *)base_ptr + 2B];
   cur3 = MEM[(const uint8_t *)base_ptr + 3B];
   cur0_cast = (int) cur0;
   cur1_cast = (unsigned int) cur1;
   cur2_cast = (unsigned int) cur2;
   cur3_cast = (int) cur3;
   hash_table_cur0 = hash_table[0][cur0_cast];
   hash_table_cur3 = hash_table[0][cur3_cast];
   cur2_shift8 = cur2_cast << 8;
   hash_table_cur3_shift5 = hash_table_cur3 << 5;
   temp = hash_table_cur0 ^ cur1_cast;
   temp2 = cur2_shift8 ^ temp;
   temp3 = temp2 ^ hash_table_cur3_shift5;
   hash_value = temp3 & mf (D)->hash_mask;
  */
static bool match_hash1_value_pattern (gimple *stmt, hash_pattern_context &ctx)
{
  if (!is_gimple_assign (stmt))
    return false;

  /* Trace back temps and extract hash_mask:
    temp = hash_table_cur0 ^ cur1_cast;
    temp2 = cur2_shift8 ^ temp;
    temp3 = temp2 ^ hash_table_cur3_shift5;
    hash_value = temp3 & mf (D)->hash_mask; */
  tree temp3 = gimple_assign_rhs1 (stmt);
  if (TREE_CODE (temp3) != SSA_NAME ||
      gimple_assign_rhs_code (stmt) != BIT_AND_EXPR)
    return false;
  tree hash_mask = gimple_assign_rhs2 (stmt);
  if (ctx.mf_D_hash_mask)
    return false;
  ctx.mf_D_hash_mask = hash_mask;
  gimple *temp3_def = SSA_NAME_DEF_STMT (temp3);
  if (!is_gimple_assign (temp3_def) ||
      gimple_assign_rhs_code (temp3_def) != BIT_XOR_EXPR)
    return false;
  tree temp2 = gimple_assign_rhs1 (temp3_def);
  if (TREE_CODE (temp2) != SSA_NAME)
    return false;
  gimple *temp2_def = SSA_NAME_DEF_STMT (temp2);
  if (!is_gimple_assign (temp2_def) ||
      gimple_assign_rhs_code (temp2_def) != BIT_XOR_EXPR)
    return false;
  tree temp = gimple_assign_rhs2 (temp2_def);
  if (TREE_CODE (temp) != SSA_NAME)
    return false;
  gimple *temp_def = SSA_NAME_DEF_STMT (temp);
  if (!is_gimple_assign (temp_def) ||
      gimple_assign_rhs_code (temp_def) != BIT_XOR_EXPR)
    return false;

  /* Trace back cur1_cast to cur1 */
  tree cur1_cast = gimple_assign_rhs2 (temp_def);
  if (TREE_CODE (cur1_cast) != SSA_NAME)
    return false;
  gimple *cur1_cast_def = SSA_NAME_DEF_STMT (cur1_cast);
  if (!is_gimple_assign (cur1_cast_def))
    return false;
  if (gimple_assign_rhs_code (cur1_cast_def) != NOP_EXPR)
    return false;
  tree cur1 = gimple_assign_rhs1 (cur1_cast_def);
  if (!operand_equal_p (cur1, ctx.curs[1], 0))
    return false;

  /* Trace back cur2_shift8 to cur2 */
  tree cur2_shift8 = gimple_assign_rhs1 (temp2_def);
  if (TREE_CODE (cur2_shift8) != SSA_NAME)
    return false;
  gimple *cur2_shift8_def = SSA_NAME_DEF_STMT (cur2_shift8);
  if (!is_gimple_assign (cur2_shift8_def) ||
      gimple_assign_rhs_code (cur2_shift8_def) != LSHIFT_EXPR)
    return false;
  tree cur2_cast = gimple_assign_rhs1 (cur2_shift8_def);
  if (TREE_CODE (cur2_cast) != SSA_NAME ||
      gimple_assign_rhs_code (SSA_NAME_DEF_STMT (cur2_cast)) != NOP_EXPR)
    return false;
  tree shift_amount = gimple_assign_rhs2 (cur2_shift8_def);
  if (!tree_fits_shwi_p (shift_amount) || tree_to_shwi (shift_amount) != 8)
    return false;
  tree cur2 = gimple_assign_rhs1 (SSA_NAME_DEF_STMT (cur2_cast));
  if (!operand_equal_p (cur2, ctx.curs[2], 0))
    return false;

  /* Trace back hash_table_cur0 to cur0. */
  tree hash_table_cur0 = gimple_assign_rhs1 (temp_def);
  if (TREE_CODE (hash_table_cur0) != SSA_NAME)
    return false;
  gimple *hash_table_cur0_def = SSA_NAME_DEF_STMT (hash_table_cur0);
  enum tree_code cur0_arr_code = gimple_assign_rhs_code (hash_table_cur0_def);
  if (cur0_arr_code != ARRAY_REF)
    return false;
  tree cur0_arr_rhs = gimple_assign_rhs1 (hash_table_cur0_def);
  tree cur0_cast = TREE_OPERAND (cur0_arr_rhs, 1);
  if (TREE_CODE (cur0_cast) != SSA_NAME)
    return false;
  gimple *cur0_cast_def = SSA_NAME_DEF_STMT (cur0_cast);
  enum tree_code cur0_cast_code = gimple_assign_rhs_code (cur0_cast_def);
  if (cur0_cast_code != NOP_EXPR && cur0_cast_code != CONVERT_EXPR)
    return false;
  tree cur0 = gimple_assign_rhs1 (cur0_cast_def);
  if (!operand_equal_p (cur0, ctx.curs[0], 0))
    return false;

  /* Trace back hash_table_cur3_shift5 to cur3. */
  tree hash_table_cur3_shift5 = gimple_assign_rhs2 (temp3_def);
  if (TREE_CODE (hash_table_cur3_shift5) != SSA_NAME)
    return false;
  gimple *shift5_def = SSA_NAME_DEF_STMT (hash_table_cur3_shift5);
  if (!is_gimple_assign (shift5_def) ||
      gimple_assign_rhs_code (shift5_def) != LSHIFT_EXPR)
    return false;
  tree shift5_amount = gimple_assign_rhs2 (shift5_def);
  if (!tree_fits_shwi_p (shift5_amount) || tree_to_shwi (shift5_amount) != 5)
    return false;
  tree hash_table_cur3 = gimple_assign_rhs1 (shift5_def);
  if (TREE_CODE (hash_table_cur3) != SSA_NAME)
    return false;
  gimple *hash_table_cur3_def = SSA_NAME_DEF_STMT (hash_table_cur3);
  enum tree_code cur3_arr_code = gimple_assign_rhs_code (hash_table_cur3_def);
  if (cur3_arr_code != ARRAY_REF)
    return false;
  tree cur3_arr_rhs = gimple_assign_rhs1 (hash_table_cur3_def);
  if (TREE_CODE (cur3_arr_rhs) != ARRAY_REF)
    return false;
  tree cur3_cast = TREE_OPERAND (cur3_arr_rhs, 1);
  if (!cur3_cast || TREE_CODE (cur3_cast) != SSA_NAME)
    return false;
  gimple *cur3_cast_def = SSA_NAME_DEF_STMT (cur3_cast);
  enum tree_code cur3_cast_code = gimple_assign_rhs_code (cur3_cast_def);
  if (cur3_cast_code != NOP_EXPR && cur3_cast_code != CONVERT_EXPR)
    return false;
  tree cur3 = gimple_assign_rhs1 (cur3_cast_def);
  if (!operand_equal_p (cur3, ctx.curs[3], 0))
    return false;

  /* Extract hash_table from hash_table_cur0. */
  tree hash_table = TREE_OPERAND (cur0_arr_rhs, 0);
  if (ctx.hash_table && ctx.hash_table != hash_table)
    return false;

  /* All checks passed! Pattern successfully matched */
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "  Successfully matched hash1_value pattern\n");

  ctx.hash_table = hash_table;
  ctx.found_hash_1_value = true;
  return true;
}

/* Match the hash_2_value pattern which calculated:
   Target IR pattern to match:
   cur0 = *base_ptr;
   cur1 = MEM[(const uint8_t *)base_ptr + 1B];
   cur0_cast = (int) cur0;
   cur1_cast = (unsigned int) cur1;
   hash_table_cur0 = hash_table[0][cur0_cast];
   temp = hash_table_cur0 ^ cur1_cast;
   hash_2_value = temp & 1023;
  */
static bool match_hash2_value_pattern (gimple *stmt, hash_pattern_context &ctx)
{
  if (!is_gimple_assign (stmt))
    return false;

  /* Trace back temps and extract mask:
    temp = hash_table_cur0 ^ cur1_cast;
    hash_2_value = temp & 1023; */
  tree temp = gimple_assign_rhs1 (stmt);
  if (TREE_CODE (temp) != SSA_NAME)
    return false;
  tree mask2 = gimple_assign_rhs2 (stmt);
  if (!tree_fits_shwi_p (mask2) || tree_to_shwi (mask2) != 1023)
    return false;
  gimple *temp_def = SSA_NAME_DEF_STMT (temp);
  if (!is_gimple_assign (temp_def) ||
      gimple_assign_rhs_code (temp_def) != BIT_XOR_EXPR)
    return false;

  /* Trace back cur1_cast to cur1 */
  tree cur1_cast = gimple_assign_rhs2 (temp_def);
  if (TREE_CODE (cur1_cast) != SSA_NAME)
    return false;
  gimple *cur1_cast_def = SSA_NAME_DEF_STMT (cur1_cast);
  if (!is_gimple_assign (cur1_cast_def) ||
      gimple_assign_rhs_code (cur1_cast_def) != NOP_EXPR)
    return false;
  tree cur1 = gimple_assign_rhs1 (cur1_cast_def);
  if (!operand_equal_p (cur1, ctx.curs[1], 0))
    return false;

  /* Trace back hash_table_cur0 to cur0. */
  tree hash_table_cur0 = gimple_assign_rhs1 (temp_def);
  if (TREE_CODE (hash_table_cur0) != SSA_NAME)
    return false;
  gimple *hash_table_cur0_def = SSA_NAME_DEF_STMT (hash_table_cur0);
  if (gimple_assign_rhs_code (hash_table_cur0_def) != ARRAY_REF)
    return false;
  tree cur0_arr_rhs = gimple_assign_rhs1 (hash_table_cur0_def);
  tree cur0_cast = TREE_OPERAND (cur0_arr_rhs, 1);
  if (TREE_CODE (cur0_cast) != SSA_NAME)
    return false;

  gimple *cur0_cast_def = SSA_NAME_DEF_STMT (cur0_cast);
  if (gimple_assign_rhs_code (cur0_cast_def) != NOP_EXPR)
    return false;
  tree cur0 = gimple_assign_rhs1 (cur0_cast_def);
  if (!operand_equal_p (cur0, ctx.curs[0], 0))
    return false;

  /* Extract and check hash_table[0] */
  tree hash_table = TREE_OPERAND (cur0_arr_rhs, 0);
  if (TREE_CODE (hash_table) != ARRAY_REF)
    return false;

  if (ctx.hash_table && ctx.hash_table != hash_table)
    return false;

  /* All checks passed! Pattern successfully matched */
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "  Successfully matched hash2_value pattern\n");

  ctx.hash_table = hash_table;
  ctx.found_hash_2_value = true;
  return true;
}

/* Match the hash_3_value pattern which calculated:
   Target IR pattern to match:
   cur0 = *base_ptr;
   cur1 = MEM[(const uint8_t *)base_ptr + 1B];
   cur2 = MEM[(const uint8_t *)base_ptr + 2B];
   cur0_cast = (int) cur0;
   cur1_cast = (unsigned int) cur1;
   cur2_cast = (unsigned int) cur2;
   hash_table_cur0 = hash_table[0][cur0_cast];
   cur2_shift8 = cur2_cast << 8;
   temp = hash_table_cur0 ^ cur1_cast;
   temp2 = cur2_shift8 ^ temp;
   hash_3_value = temp2 & 65535;
  */
static bool match_hash3_value_pattern (gimple *stmt, hash_pattern_context &ctx)
{
  if (!is_gimple_assign (stmt))
    return false;

  /* Trace back temps and extract mask:
    temp = hash_table_cur0 ^ cur1_cast;
    temp2 = cur2_shift8 ^ temp;
    hash_3_value = temp2 & 65535; */
  tree temp2 = gimple_assign_rhs1 (stmt);
  if (TREE_CODE (temp2) != SSA_NAME ||
      gimple_assign_rhs_code (stmt) != BIT_AND_EXPR)
    return false;
  tree mask3 = gimple_assign_rhs2 (stmt);
  if (!tree_fits_shwi_p (mask3) || tree_to_shwi (mask3) != 65535)
    return false;

  gimple *temp2_def = SSA_NAME_DEF_STMT (temp2);
  if (!is_gimple_assign (temp2_def) ||
      gimple_assign_rhs_code (temp2_def) != BIT_XOR_EXPR)
    return false;

  /* Trace back cur2_shift8 to cur2 */
  tree cur2_shift8 = gimple_assign_rhs1 (temp2_def);
  if (TREE_CODE (cur2_shift8) != SSA_NAME)
    return false;

  gimple *cur2_shift8_def = SSA_NAME_DEF_STMT (cur2_shift8);
  if (!is_gimple_assign (cur2_shift8_def) ||
      gimple_assign_rhs_code (cur2_shift8_def) != LSHIFT_EXPR)
    return false;
  tree cur2_cast = gimple_assign_rhs1 (cur2_shift8_def);
  if (TREE_CODE (cur2_cast) != SSA_NAME)
    return false;
  tree shift_amount = gimple_assign_rhs2 (cur2_shift8_def);
  if (!tree_fits_shwi_p (shift_amount) || tree_to_shwi (shift_amount) != 8)
    return false;
  gimple *cur2_cast_def = SSA_NAME_DEF_STMT (cur2_cast);
  if (gimple_assign_rhs_code (cur2_cast_def) != NOP_EXPR)
    return false;
  tree cur2 = gimple_assign_rhs1 (cur2_cast_def);
  if (!operand_equal_p (cur2, ctx.curs[2], 0))
    return false;

  /* Trace back temp to cur1_cast and hash_table_cur0 */
  tree temp = gimple_assign_rhs2 (temp2_def);
  if (TREE_CODE (temp) != SSA_NAME)
    return false;

  gimple *temp_def = SSA_NAME_DEF_STMT (temp);
  if (!is_gimple_assign (temp_def) ||
      gimple_assign_rhs_code (temp_def) != BIT_XOR_EXPR)
    return false;

  /* Trace back cur1_cast to cur1 */
  tree cur1_cast = gimple_assign_rhs2 (temp_def);
  if (TREE_CODE (cur1_cast) != SSA_NAME)
    return false;
  gimple *cur1_cast_def = SSA_NAME_DEF_STMT (cur1_cast);
  if (!is_gimple_assign (cur1_cast_def) ||
      gimple_assign_rhs_code (cur1_cast_def) != NOP_EXPR)
    return false;
  tree cur1 = gimple_assign_rhs1 (cur1_cast_def);
  if (!operand_equal_p (cur1, ctx.curs[1], 0))
    return false;

  /* Trace back hash_table_cur0 to cur0. */
  tree hash_table_cur0 = gimple_assign_rhs1 (temp_def);
  if (TREE_CODE (hash_table_cur0) != SSA_NAME)
    return false;
  gimple *hash_table_cur0_def = SSA_NAME_DEF_STMT (hash_table_cur0);
  if (gimple_assign_rhs_code (hash_table_cur0_def) != ARRAY_REF)
    return false;
  tree cur0_arr_rhs = gimple_assign_rhs1 (hash_table_cur0_def);
  tree cur0_cast = TREE_OPERAND (cur0_arr_rhs, 1);
  if (TREE_CODE (cur0_cast) != SSA_NAME)
    return false;

  gimple *cur0_cast_def = SSA_NAME_DEF_STMT (cur0_cast);
  if (gimple_assign_rhs_code (cur0_cast_def) != NOP_EXPR)
    return false;
  tree cur0 = gimple_assign_rhs1 (cur0_cast_def);
  if (!operand_equal_p (cur0, ctx.curs[0], 0))
    return false;

  /* Extract and check hash_table */
  tree hash_table = TREE_OPERAND (cur0_arr_rhs, 0);
  if (TREE_CODE (hash_table) != ARRAY_REF)
    return false;

  if (ctx.hash_table && ctx.hash_table != hash_table)
    return false;

  /* All checks passed! Pattern successfully matched */
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "  Successfully matched hash3_value pattern\n");

  ctx.hash_table = hash_table;
  ctx.found_hash_3_value = true;
  return true;
}

/* Match hash's common calculate:
  hash_x_value_add = hash_x_value + 0 (1024,66560);
  hash_x_value_ext = (long unsigned int) hash_x_value;
  hash_x_value_offset = hash_x_value_ext * 4;
  hash_x_value_addr = hash_addr + hash_x_value_offset;
  hash_x = *hash_x_value_addr;
*/
static bool match_hash_calculation_pattern (gimple *stmt,
					    hash_pattern_context &ctx)
{
  if (!is_gimple_assign (stmt) || gimple_assign_rhs_code (stmt) != MEM_REF)
    return false;
  /* Start matching from hash_x = *hash_x_value_addr */
  tree lhs = gimple_assign_lhs (stmt);
  if (TREE_CODE (lhs) != SSA_NAME)
    return false;
  tree hash_x_value_addr_ref = gimple_assign_rhs1 (stmt);
  tree hash_x_value_addr = TREE_OPERAND (hash_x_value_addr_ref, 0);
  tree hash_x_value_addr_off = TREE_OPERAND (hash_x_value_addr_ref, 1);
  if (!tree_fits_shwi_p (hash_x_value_addr_off) ||
      tree_to_shwi (hash_x_value_addr_off) != 0)
    return false;

  /* Start matching from hash_x_value_addr = hash_addr + hash_x_value_offset */
  gimple *hash_x_value_addr_def = SSA_NAME_DEF_STMT (hash_x_value_addr);
  if (!is_gimple_assign (hash_x_value_addr_def) ||
      gimple_assign_rhs_code (hash_x_value_addr_def) != POINTER_PLUS_EXPR)
    return false;

  tree hash_addr = gimple_assign_rhs1 (hash_x_value_addr_def);
  tree hash_x_value_offset = gimple_assign_rhs2 (hash_x_value_addr_def);

  if (TREE_CODE (hash_addr) != SSA_NAME ||
      TREE_CODE (hash_x_value_offset) != SSA_NAME)
    return false;

  /* Start matching from hash_x_value_offset = hash_x_value_ext * 4; */
  gimple *hash_x_value_offset_def = SSA_NAME_DEF_STMT (hash_x_value_offset);
  if (!is_gimple_assign (hash_x_value_offset_def) ||
      gimple_assign_rhs_code (hash_x_value_offset_def) != MULT_EXPR)
    return false;

  tree hash_x_value_ext = gimple_assign_rhs1 (hash_x_value_offset_def);
  if (TREE_CODE (hash_x_value_ext) != SSA_NAME)
    return false;

  tree offset_rhs2 = gimple_assign_rhs2 (hash_x_value_offset_def);
  if (!tree_fits_shwi_p (offset_rhs2) || tree_to_shwi (offset_rhs2) != 4)
    return false;

  /* Start matching from hash_x_value_ext = (long unsigned int) hash_x_value */
  gimple *hash_x_value_ext_def = SSA_NAME_DEF_STMT (hash_x_value_ext);
  if (!is_gimple_assign (hash_x_value_ext_def))
    return false;

  enum tree_code ext_code = gimple_assign_rhs_code (hash_x_value_ext_def);
  if (ext_code != NOP_EXPR && ext_code != CONVERT_EXPR)
    return false;

  tree hash_x_value = gimple_assign_rhs1 (hash_x_value_ext_def);
  if (TREE_CODE (hash_x_value) != SSA_NAME)
    return false;

  /* Start matching:
    (hash_2_value is original)
    hash_3_value_add = hash_3_value + 1024
    hash_1_value_add = hash_1_value + 66560 */
  int hash_num = 0;
  gimple *hash_x_value_def = SSA_NAME_DEF_STMT (hash_x_value);
  if (!is_gimple_assign (hash_x_value_def))
    return false;

  enum tree_code value_code = gimple_assign_rhs_code (hash_x_value_def);
  if (value_code == PLUS_EXPR)
  {
    tree value_rhs1 = gimple_assign_rhs1 (hash_x_value_def);
    tree value_rhs2 = gimple_assign_rhs2 (hash_x_value_def);
    if (TREE_CODE (value_rhs1) != SSA_NAME || !tree_fits_shwi_p (value_rhs2))
      return false;
    int value_offset = tree_to_shwi (value_rhs2);
    if (value_offset == 1024)
      hash_num = 3;
    else if (value_offset == 66560)
      hash_num = 1;
    else
      return false;
    hash_x_value = value_rhs1;
    hash_x_value_def = SSA_NAME_DEF_STMT (hash_x_value);
  }
  else if (value_code == BIT_AND_EXPR)
    hash_num = 2;
  else
    return false;

  /* Match hash_x_value */
  bool match_value = false;
  if (hash_num == 1)
    match_value = match_hash1_value_pattern (hash_x_value_def, ctx);
  else if (hash_num == 2)
    match_value = match_hash2_value_pattern (hash_x_value_def, ctx);
  else if (hash_num == 3)
    match_value = match_hash3_value_pattern (hash_x_value_def, ctx);

  if (!match_value)
  {
    ctx.match_conflict = true;
    return false;
  }

  if (ctx.hash_value_addr && ctx.hash_value_addr != hash_addr)
  {
    ctx.match_conflict = true;
    return false;
  }
  ctx.hash_value_addr = hash_addr;
  return true;
}

/* Generate new hash calculation statements for cur4-7:
   cur4 = MEM[(const uint8_t *)base_ptr + 4B];
   cur5 = MEM[(const uint8_t *)base_ptr + 5B];
   cur6 = MEM[(const uint8_t *)base_ptr + 6B];
   cur7 = MEM[(const uint8_t *)base_ptr + 7B];
   cur4_cast = (int) cur4;
   cur5_cast = (unsigned int) cur5;
   cur6_cast = (unsigned int) cur6;
   cur7_cast = (int) cur7;

   hash_table_cur4 = hash_table[0][cur4_cast];
   hash_table_cur7 = hash_table[0][cur7_cast];

   cur6_shift8 = cur6_cast << 8;
   hash_table_cur7_shift5 = hash_table_cur7 << 5;

   temp4 = hash_table_cur4 ^ cur5_cast;
   temp5 = cur6_shift8 ^ temp4;
   temp6 = temp5 ^ hash_table_cur7_shift5;

   hash_5_value = temp4 & 1023;
   hash_6_value = temp5 & 65535;
   hash_4_value = temp6 & hash_mask;

   hash_addr = mf (D)->hash;

   hash_5_value_ext = (long unsigned int) hash_5_value;
   hash_5_value_offset = hash_5_value_ext * 4;
   hash_5_value_addr = hash_addr + hash_5_value_offset;
   __builtin_prefetch (hash_5_value_addr);

   hash_6_value_add = hash_6_value + 1024;
   hash_6_value_ext = (long unsigned int) hash_6_value_add;
   hash_6_value_offset = hash_6_value_ext * 4;
   hash_6_value_addr = hash_addr + hash_6_value_offset;
   __builtin_prefetch (hash_6_value_addr);

   hash_4_value_add = hash_4_value + 66560;
   hash_4_value_ext = (long unsigned int) hash_4_value_add;
   hash_4_value_offset = hash_4_value_ext * 4;
   hash_4_value_addr = hash_addr + hash_4_value_offset;
   __builtin_prefetch (hash_4_value_addr); */
static void generate_hash_prefetch_ir (gimple_stmt_iterator &gsi,
				       const hash_pattern_context &ctx)
{
  /* Get element type for type conversions */
  tree ptr_type = TREE_TYPE (ctx.base_ptr);
  tree elem_type = TREE_TYPE (ptr_type);

  /* Generate memory loads: cur4 = MEM[base_ptr + 4B];
			    cur5 = MEM[base_ptr + 5B];
			    cur6 = MEM[base_ptr + 6B];
			    cur7 = MEM[base_ptr + 7B]; */
  tree cst4_ptr = build_int_cst (build_pointer_type (elem_type), 4);
  tree cur4_ref = build2 (MEM_REF, elem_type, ctx.base_ptr, cst4_ptr);
  tree cur4 = make_ssa_name (elem_type);
  gassign *cur4_load = gimple_build_assign (cur4, MEM_REF, cur4_ref);
  gsi_insert_before (&gsi, cur4_load, GSI_SAME_STMT);

  tree cst5_ptr = build_int_cst (build_pointer_type (elem_type), 5);
  tree cur5_ref = build2 (MEM_REF, elem_type, ctx.base_ptr, cst5_ptr);
  tree cur5 = make_ssa_name (elem_type);
  gassign *cur5_load = gimple_build_assign (cur5, MEM_REF, cur5_ref);
  gsi_insert_before (&gsi, cur5_load, GSI_SAME_STMT);

  tree cst6_ptr = build_int_cst (build_pointer_type (elem_type), 6);
  tree cur6_ref = build2 (MEM_REF, elem_type, ctx.base_ptr, cst6_ptr);
  tree cur6 = make_ssa_name (elem_type);
  gassign *cur6_load = gimple_build_assign (cur6, MEM_REF, cur6_ref);
  gsi_insert_before (&gsi, cur6_load, GSI_SAME_STMT);

  tree cst7_ptr = build_int_cst (build_pointer_type (elem_type), 7);
  tree cur7_ref = build2 (MEM_REF, elem_type, ctx.base_ptr, cst7_ptr);
  tree cur7 = make_ssa_name (elem_type);
  gassign *cur7_load = gimple_build_assign (cur7, MEM_REF, cur7_ref);
  gsi_insert_before (&gsi, cur7_load, GSI_SAME_STMT);

  /* Generate: cur4_cast = (int) cur4;
	       cur5_cast = (unsigned int) cur5;
	       cur6_cast = (unsigned int) cur6;
	       cur7_cast = (int) cur7; */
  tree cur4_cast = make_ssa_name (integer_type_node);
  gassign *cur4_cast_assign = gimple_build_assign (cur4_cast, NOP_EXPR, cur4);
  gsi_insert_before (&gsi, cur4_cast_assign, GSI_SAME_STMT);

  tree cur5_cast = make_ssa_name (unsigned_type_node);
  gassign *cur5_cast_assign = gimple_build_assign (cur5_cast, NOP_EXPR, cur5);
  gsi_insert_before (&gsi, cur5_cast_assign, GSI_SAME_STMT);

  tree cur6_cast = make_ssa_name (unsigned_type_node);
  gassign *cur6_cast_assign = gimple_build_assign (cur6_cast, NOP_EXPR, cur6);
  gsi_insert_before (&gsi, cur6_cast_assign, GSI_SAME_STMT);

  tree cur7_cast = make_ssa_name (integer_type_node);
  gassign *cur7_cast_assign = gimple_build_assign (cur7_cast, NOP_EXPR, cur7);
  gsi_insert_before (&gsi, cur7_cast_assign, GSI_SAME_STMT);

  /* Generate: hash_table_cur4 = hash_table[0][cur4_cast];
	       hash_table_cur7 = hash_table[0][cur7_cast]; */
  tree hash_table_cur4 = make_ssa_name (unsigned_type_node);
  tree hash_table_cur4_ref = build4 (ARRAY_REF, unsigned_type_node,
				     ctx.hash_table, cur4_cast,
				     NULL_TREE, NULL_TREE);
  gassign *hash_table_cur4_load = gimple_build_assign (hash_table_cur4,
						       ARRAY_REF,
						       hash_table_cur4_ref);
  gsi_insert_before (&gsi, hash_table_cur4_load, GSI_SAME_STMT);

  tree hash_table_cur7 = make_ssa_name (unsigned_type_node);
  tree hash_table_cur7_ref = build4 (ARRAY_REF, unsigned_type_node,
				     ctx.hash_table, cur7_cast,
				     NULL_TREE, NULL_TREE);
  gassign *hash_table_cur7_load = gimple_build_assign (hash_table_cur7,
						       ARRAY_REF,
						       hash_table_cur7_ref);
  gsi_insert_before (&gsi, hash_table_cur7_load, GSI_SAME_STMT);

  /* Generate: cur6_shift8 = cur6_cast << 8;
	       hash_table_cur7_shift5 = hash_table_cur7 << 5; */
  tree cur6_shift8 = make_ssa_name (unsigned_type_node);
  tree const_shift8 = build_int_cst (unsigned_type_node, 8);
  gassign *cur6_shift8_assign = gimple_build_assign (cur6_shift8, LSHIFT_EXPR,
						     cur6_cast, const_shift8);
  gsi_insert_before (&gsi, cur6_shift8_assign, GSI_SAME_STMT);

  tree hash_table_cur7_shift5 = make_ssa_name (unsigned_type_node);
  tree const_shift5 = build_int_cst (unsigned_type_node, 5);
  gassign *hash_table_cur7_shift5_assign =
      gimple_build_assign (hash_table_cur7_shift5, LSHIFT_EXPR,
			   hash_table_cur7, const_shift5);
  gsi_insert_before (&gsi, hash_table_cur7_shift5_assign, GSI_SAME_STMT);

  /* Generate: temp4 = hash_table_cur4 ^ cur5_cast;
	       temp5 = cur6_shift8 ^ temp4;
	       temp6 = temp5 ^ hash_table_cur7_shift5; */
  tree temp4 = make_ssa_name (unsigned_type_node);
  gassign *temp4_assign = gimple_build_assign (temp4, BIT_XOR_EXPR,
					      hash_table_cur4, cur5_cast);
  gsi_insert_before (&gsi, temp4_assign, GSI_SAME_STMT);

  tree temp5 = make_ssa_name (unsigned_type_node);
  gassign *temp5_assign = gimple_build_assign (temp5, BIT_XOR_EXPR,
					       cur6_shift8, temp4);
  gsi_insert_before (&gsi, temp5_assign, GSI_SAME_STMT);

  tree temp6 = make_ssa_name (unsigned_type_node);
  gassign *temp6_assign = gimple_build_assign (temp6, BIT_XOR_EXPR,
					       temp5, hash_table_cur7_shift5);
  gsi_insert_before (&gsi, temp6_assign, GSI_SAME_STMT);

  /* Generate: hash_5_value = temp4 & 1023;
	       hash_6_value = temp5 & 65535;
	       hash_4_value = temp6 & hash_mask; */
  tree hash_5_value = make_ssa_name (unsigned_type_node);
  tree const_mask32 = build_int_cst (unsigned_type_node, 1023);
  gassign *hash_5_value_assign = gimple_build_assign (hash_5_value,
						      BIT_AND_EXPR,
						      temp4, const_mask32);
  gsi_insert_before (&gsi, hash_5_value_assign, GSI_SAME_STMT);

  tree hash_6_value = make_ssa_name (unsigned_type_node);
  tree const_mask64k = build_int_cst (unsigned_type_node, 65535);
  gassign *hash_6_value_assign = gimple_build_assign (hash_6_value,
						      BIT_AND_EXPR,
						      temp5, const_mask64k);
  gsi_insert_before (&gsi, hash_6_value_assign, GSI_SAME_STMT);

  tree hash_4_value = make_ssa_name (unsigned_type_node);
  gassign *hash_4_value_assign = gimple_build_assign (hash_4_value,
						      BIT_AND_EXPR, temp6,
						      ctx.mf_D_hash_mask);
  gsi_insert_before (&gsi, hash_4_value_assign, GSI_SAME_STMT);

  /* Generate: hash_5_value_ext = (long unsigned int) hash_5_value;
	       hash_5_value_offset = hash_5_value_ext * 4;
	       hash_5_value_addr = hash_addr + hash_5_value_offset;
	       __builtin_prefetch (hash_5_value_addr); */
  tree hash_5_value_ext = make_ssa_name (uint64_type_node);
  gassign *hash_5_value_ext_assign = gimple_build_assign (hash_5_value_ext,
							  NOP_EXPR,
							  hash_5_value);
  gsi_insert_before (&gsi, hash_5_value_ext_assign, GSI_SAME_STMT);

  tree hash_5_value_offset = make_ssa_name (uint64_type_node);
  tree const_mul4 = build_int_cst (uint64_type_node, 4);
  gassign *hash_5_value_offset_assign =
    gimple_build_assign (hash_5_value_offset, MULT_EXPR, hash_5_value_ext,
			 const_mul4);
  gsi_insert_before (&gsi, hash_5_value_offset_assign, GSI_SAME_STMT);

  tree hash_5_value_addr = make_ssa_name (ptr_type);
  gassign *hash_5_value_addr_assign =
    gimple_build_assign (hash_5_value_addr, POINTER_PLUS_EXPR,
			 ctx.hash_value_addr, hash_5_value_offset);
  gsi_insert_before (&gsi, hash_5_value_addr_assign, GSI_SAME_STMT);

  tree builtin_prefetch_decl = builtin_decl_explicit (BUILT_IN_PREFETCH);
  gcall *prefetch5_call = gimple_build_call (builtin_prefetch_decl, 1,
					     hash_5_value_addr);
  gsi_insert_before (&gsi, prefetch5_call, GSI_SAME_STMT);

  /* Generate: hash_6_value_add = hash_6_value + 1024;
	       hash_6_value_ext = (long unsigned int) hash_6_value_add;
	       hash_6_value_offset = hash_6_value_ext * 4;
	       hash_6_value_addr = hash_addr + hash_6_value_offset;
	       __builtin_prefetch (hash_6_value_addr); */
  tree hash_6_value_add = make_ssa_name (unsigned_type_node);
  tree const_add1024 = build_int_cst (unsigned_type_node, 1024);
  gassign *hash_6_value_add_assign = gimple_build_assign (hash_6_value_add,
							  PLUS_EXPR,
							  hash_6_value,
							  const_add1024);
  gsi_insert_before (&gsi, hash_6_value_add_assign, GSI_SAME_STMT);

  tree hash_6_value_ext = make_ssa_name (uint64_type_node);
  gassign *hash_6_value_ext_assign = gimple_build_assign (hash_6_value_ext,
							  NOP_EXPR,
							  hash_6_value_add);
  gsi_insert_before (&gsi, hash_6_value_ext_assign, GSI_SAME_STMT);

  tree hash_6_value_offset = make_ssa_name (uint64_type_node);
  gassign *hash_6_value_offset_assign =
    gimple_build_assign (hash_6_value_offset, MULT_EXPR, hash_6_value_ext,
			 const_mul4);
  gsi_insert_before (&gsi, hash_6_value_offset_assign, GSI_SAME_STMT);

  tree hash_6_value_addr = make_ssa_name (ptr_type);
  gassign *hash_6_value_addr_assign =
    gimple_build_assign (hash_6_value_addr, POINTER_PLUS_EXPR,
			 ctx.hash_value_addr, hash_6_value_offset);
  gsi_insert_before (&gsi, hash_6_value_addr_assign, GSI_SAME_STMT);

  gcall *prefetch6_call = gimple_build_call (builtin_prefetch_decl, 1,
					     hash_6_value_addr);
  gsi_insert_before (&gsi, prefetch6_call, GSI_SAME_STMT);

  /* Generate: hash_4_value_add = hash_4_value + 66560;
	       hash_4_value_ext = (long unsigned int) hash_4_value_add;
	       hash_4_value_offset = hash_4_value_ext * 4;
	       hash_4_value_addr = hash_addr + hash_4_value_offset;
	       __builtin_prefetch (hash_4_value_addr); */
  tree hash_4_value_add = make_ssa_name (unsigned_type_node);
  tree const_add66560 = build_int_cst (unsigned_type_node, 66560);
  gassign *hash_4_value_add_assign = gimple_build_assign (hash_4_value_add,
							  PLUS_EXPR,
							  hash_4_value,
							  const_add66560);
  gsi_insert_before (&gsi, hash_4_value_add_assign, GSI_SAME_STMT);

  tree hash_4_value_ext = make_ssa_name (uint64_type_node);
  gassign *hash_4_value_ext_assign = gimple_build_assign (hash_4_value_ext,
							  NOP_EXPR,
							  hash_4_value_add);
  gsi_insert_before (&gsi, hash_4_value_ext_assign, GSI_SAME_STMT);

  tree hash_4_value_offset = make_ssa_name (uint64_type_node);
  gassign *hash_4_value_offset_assign =
    gimple_build_assign (hash_4_value_offset, MULT_EXPR,
			 hash_4_value_ext, const_mul4);
  gsi_insert_before (&gsi, hash_4_value_offset_assign, GSI_SAME_STMT);

  tree hash_4_value_addr = make_ssa_name (ptr_type);
  gassign *hash_4_value_addr_assign = gimple_build_assign (hash_4_value_addr,
							   POINTER_PLUS_EXPR,
							   ctx.hash_value_addr,
							   hash_4_value_offset);
  gsi_insert_before (&gsi, hash_4_value_addr_assign, GSI_SAME_STMT);

  gcall *prefetch4_call = gimple_build_call (builtin_prefetch_decl, 1,
					     hash_4_value_addr);
  gsi_insert_before (&gsi, prefetch4_call, GSI_SAME_STMT);

  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file,
	     "Generated new hash calculation IR statements for cur4-7\n");
}

/* This function analyzes a single basic block to find patterns that
   should get prefetch instructions inserted.
   Returns true if a pattern was found.
*/
static bool recognize_and_transform_pattern (basic_block bb)
{
  gimple_stmt_iterator gsi;
  gimple *stmt;
  bool changed = false;

  /* Initialize pattern context */
  static hash_pattern_context ctx;
  ctx.clear ();

  /* Walk through all statements in this basic block */
  for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
  {
    if (ctx.match_conflict)
      break;
    stmt = gsi_stmt (gsi);
    if (is_gimple_debug (stmt))
      continue;
    /* Try to match the hash calculation pattern */
    if (!ctx.found_cur0_to_cur3)
      match_hash_curs_pattern (stmt, ctx);
    else
      match_hash_calculation_pattern (stmt, ctx);

    /* If we found all pattern components, process them */
    if (ctx.found_all_pattern ())
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
      {
	fprintf (dump_file, "\n=== HASH PATTERN MATCHED SUCCESSFULLY ===\n");
	fprintf (dump_file, "Base pointer: ");
	print_generic_expr (dump_file, ctx.base_ptr, TDF_SLIM);
	fprintf (dump_file, "\n");
      }
      /* Generate new IR statements */
      gsi_next (&gsi);
      generate_hash_prefetch_ir (gsi, ctx);
      changed = true;
      break;
    }
  }

  return changed;
}

/* Main entry point for this pass - processes each basic block */
static unsigned int tree_ssa_hash_prefetch (function *fun)
{
  bool changed = false;
  basic_block bb;

  if (dump_file && (dump_flags & TDF_DETAILS))
  {
    fprintf (dump_file, "=== Hash Prefetch Pass ===\n");
    fprintf (dump_file, "Function: %s\n", function_name (fun));
    fprintf (dump_file, "Basic blocks: %d\n\n", n_basic_blocks_for_fn (fun));
  }

  /* Process all basic blocks - you can choose different traversal orders:
     - reverse post-order: for basic_blocks_in_reverse_post_order
     - regular order: FOR_EACH_BB_FN */
  FOR_EACH_BB_FN (bb, fun)
  {
    if (recognize_and_transform_pattern (bb))
    {
      changed = true;
      break;
    }
  }

  if (dump_file && (dump_flags & TDF_DETAILS))
  {
    fprintf (dump_file, "\n=== Hash Prefetch Pass Completed ===\n");
    fprintf (dump_file, "Changed: %s\n\n", changed ? "yes" : "no");
  }

  return changed ? TODO_update_ssa : 0;
}

namespace {

/* Pass data structure */
const pass_data pass_data_hash_prefetch = {
  GIMPLE_PASS,              /* type */
  "hprefetch",              /* name */
  OPTGROUP_NONE,            /* optinfo_flags */
  TV_TREE_PREFETCH,         /* tv_id */
  (PROP_cfg | PROP_ssa),    /* properties_required */
  0,                        /* properties_provided */
  0,                        /* properties_destroyed */
  0,                        /* todo_flags_start */
  0,                        /* todo_flags_finish */
};

/* Hash prefetch pass class */
class pass_hash_prefetch : public gimple_opt_pass
{
public:
  pass_hash_prefetch (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_hash_prefetch, ctxt)
  {}

  /* Gate function - returns true if pass should run */
  bool gate (function *) final override
  {
    return flag_prefetch_hash_tables;
  }

  /* Execute function - main pass implementation */
  unsigned int execute (function *fun) final override
  {
    return tree_ssa_hash_prefetch (fun);
  }
}; // class pass_hash_prefetch

} // anon namespace

/* Pass registration function - called by GCC pass manager */
gimple_opt_pass *
make_pass_hash_prefetch (gcc::context *ctxt)
{
  return new pass_hash_prefetch (ctxt);
}
