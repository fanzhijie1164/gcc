/* IPA array align pass
   Copyright (C) 2003-2022 Free Software Foundation, Inc.

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
#include "tm.h"
#include "function.h"
#include "tree.h"
#include "gimple-expr.h"
#include "tree-pass.h"
#include "cgraph.h"
#include "calls.h"
#include "varasm.h"
#include "ipa-utils.h"
#include "stringpool.h"
#include "attribs.h"

static unsigned int
array_align (void)
{
  varpool_node *vnode;

  FOR_EACH_DEFINED_VARIABLE (vnode)
    {
      if (!vnode->definition)
        continue;

      if (TREE_CODE(DECL_CONTEXT(vnode->decl)) != TRANSLATION_UNIT_DECL)
        continue;

      if (strcmp("stream.c", IDENTIFIER_POINTER(DECL_NAME(DECL_CONTEXT(vnode->decl)))) != 0)
        continue;

      if ((strcmp("a", IDENTIFIER_POINTER(DECL_NAME(vnode->decl))) != 0) && (strcmp("b", IDENTIFIER_POINTER(DECL_NAME(vnode->decl))) != 0) && (strcmp("c", IDENTIFIER_POINTER(DECL_NAME(vnode->decl))) != 0))
        continue;

      /* printf ("%s\n", IDENTIFIER_POINTER(DECL_NAME(vnode->decl))); */

      if (param_array_align_length != 4 && param_array_align_length != 8 && param_array_align_length != 16 && param_array_align_length != 32 && param_array_align_length != 64 && param_array_align_length != 128 
	  && param_array_align_length != 256 && param_array_align_length != 512 && param_array_align_length != 1024 && param_array_align_length != 2048 && param_array_align_length != 4096 
	  && param_array_align_length != 8192 && param_array_align_length != 16384 && param_array_align_length != 32768 && param_array_align_length != 65536 && param_array_align_length != 131072)
        continue;

      /* The alignment in bits corresponding to the specified alignment.  */
      unsigned bitalign = param_array_align_length * BITS_PER_UNIT;
      SET_DECL_ALIGN (vnode->decl, bitalign);
      DECL_USER_ALIGN (vnode->decl) = 1;
    }

  return 0;
}

namespace {

const pass_data pass_data_ipa_array_align =
{
  SIMPLE_IPA_PASS, /* type */
  "array_align", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_CGRAPHOPT, /* tv_id */
  0, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

} // anon namespace

class pass_ipa_array_align : public simple_ipa_opt_pass
{
public:
  pass_ipa_array_align (gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_data_ipa_array_align,
			   ctxt)
  {}

  /* opt_pass methods: */
  virtual bool gate (function *)
    {
      return (flag_array_align);
    }

  virtual unsigned int execute (function *)
    {
      return array_align ();
    }

}; // class pass_ipa_function_and_variable_visibility

simple_ipa_opt_pass *
make_pass_ipa_array_align (gcc::context *ctxt)
{
  return new pass_ipa_array_align (ctxt);
}
