/* Callee saved register optimization
   Copyright (C) 2017-2023 Free Software Foundation, Inc.

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
#include "target.h"
#include "tree-pass.h"

/* This pass runs before register allocation and implements an callee
   saved register optimization.

   In entry block we copy all callee-saved registers from a physical
   register to a virtual one. In all exit blocks we copy do the reverse.
   This has the advantage that the register allocator can essentialy do
   shrink-wrapping on per register basis.  */

namespace {

const pass_data pass_data_csr_opt =
{
  RTL_PASS, /* type */
  "csr_opt", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_CSR_OPT, /* tv_id */
  0, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

class pass_csr_opt : public rtl_opt_pass
{
public:
  pass_csr_opt (gcc::context *ctxt)
    : rtl_opt_pass (pass_data_csr_opt, ctxt)
  {}

  /* opt_pass methods: */
  bool gate (function *) final override
  {
    return optimize > 1;
  }

  unsigned int execute (function *f) final override
  {
    targetm.copy_csr_to_virt ();
    return 0;
  }
}; // class pass_csr_opt

} // anon namespace

rtl_opt_pass *
make_pass_csr_opt (gcc::context *ctxt)
{
  return new pass_csr_opt (ctxt);
}
