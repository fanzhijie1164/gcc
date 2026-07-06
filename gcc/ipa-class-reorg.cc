/* IPA Class-reorg optimizations.
   Copyright (C) 2024 Free Software Foundation, Inc.
   Contributed by Jin Kang <jinkang1767@phytium.com.cn>

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
   /* This pass implements Class layout optimizations for C++ program. 
      The main motivation is that we found some class data members are 
      often accessed together in hotspot code in omnetpp. So we currently
      do class data member reordering to group these members together, 
      meanwhile considering member size to make members in decending ordre
      to reduce padding. The space locality can be improved and perfomance
      increased.
      In theory, this pass operate in four steps:
      1. legality analysis. Make sure all the pointers point to a determinate 
         object.
      2. profite analysis. Search for optimization oppotunity. Find situations
         that some class data members are often accessed together.
      3. get the class type node
      4. do class data member reorder. */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "tree-pass.h"
#include "cgraph.h"
#include "tree-pretty-print.h"
#include "gimple-pretty-print.h"
#include "gimple-iterator.h"
#include "fold-const.h"
#include "print-tree.h"
#include "ssa.h"
#include "stor-layout.h"
#include "cp/cp-tree.h"
#include "diagnostic-core.h"
#include "tree-cfg.h"
#include "langhooks.h"

namespace {
/* 1. get the TREE_TYPE node of class cMessage.
      1.traverse all call graph node, find the function contains cMessage object
      2.locate the decl or stmt.
      3.get the TREE_TYPE.
   2. reorder class member.
      1.use dump_node, print_node, or traverse field chain to check out members.
      2.reorder class member by modifying DECL_CHAIN.
   3. get the base class cObject's TREE_TYPE node.
   4. reorder calss member. 
*/

#define VOID_POINTER_P(type) \
(POINTER_TYPE_P(type) && VOID_TYPE_P(TREE_TYPE(type)))

auto_vec<tree> candi_types, candi_btypes;
tree ori_cmsg = NULL_TREE, ori_coobj = NULL_TREE;
hash_map <tree, tree> msg_off_to_field;

static bool
lang_cxx_p (void)
{
  const char *language_string = lang_hooks.name;
  if (!language_string)
    return false;
  if (lang_GNU_CXX ())
    return true;
  else if (strcmp (language_string, "GNU GIMPLE") == 0)
  {
    unsigned i;
    tree t;
    FOR_EACH_VEC_SAFE_ELT(all_translation_units, i, t)
    {
      language_string = TRANSLATION_UNIT_LANGUAGE (t);
      if (!language_string || !startswith (language_string, "GNU C++"))
        return false;
    }
    return true;
  }
  return false;
}

static const char *
get_type_name (tree type)
{
  const char *tname = NULL;

  if (type == NULL)
    return NULL;

  if (TYPE_NAME (type) != NULL)
  {
    if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
      tname = IDENTIFIER_POINTER (TYPE_NAME (type));
    else if (DECL_NAME (TYPE_NAME (type)) != NULL)
      tname = IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)));
  }
  return tname;
}

/* Return the inner most type for arrays and pointers of TYPE.  */

static tree
inner_type (tree type)
{
  while (POINTER_TYPE_P (type)
      || TREE_CODE (type) == ARRAY_TYPE)
    type = TREE_TYPE (type);
  return type;
}

/* Return true if TYPE is a class type of c++ */

static bool
handled_type (tree type)
{
  /* Return the inner most type for arrays and pointers of TYPE.  */
  type = inner_type (type);
  /*if (dump_file)
  {
    fprintf (dump_file, "%s\n", NON_UNION_CLASS_TYPE_P (type) ?
    "non union class type: true" : "non union class type: false");
    fprintf (dump_file, "%s\n", TYPE_LANG_SPECIFIC (type) ?
    "TYPE_LANG_SPECIFIC exist" : "TYPE_LANG_SPECIFIC not exist");
    if (TYPE_LANG_SPECIFIC (type))
      fprintf (dump_file, "%s\n", CLASSTYPE_DECLARED_CLASS (type) ?
      "CLASSTYPE_DECLARED_CLASS: true" : "CLASSTYPE_DECLARED_CLASS: false");
  }*/
  //if (NON_UNION_CLASS_TYPE_P (type) && TYPE_LANG_SPECIFIC (type) && CLASSTYPE_DECLARED_CLASS (type))
  if (TREE_CODE (type) == RECORD_TYPE)
    return true;
  return false;
}

static unsigned
num_fields (tree type)
{
  unsigned i = 0;
  for (tree field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field))
  {
    if (TREE_CODE (field) == FIELD_DECL)
    {
      i++;
    }
  }
  return i;
}

static void
dump_class_type(FILE *file, tree cls_type)
{
  fprintf(file, "******** ");
  print_generic_expr(file, cls_type, dump_flags);
  fprintf(file, " ********\n");
  print_node(file, "", cls_type, 0);
  //dump_node(candi_type, TDF_RAW | TDF_SLIM | dump_flags, dump_file);
  fprintf (file, "\n");
  fprintf(file, "****** Fields:  (");
  int i = 0;
  for (tree field = TYPE_FIELDS (cls_type); field; i++, field = DECL_CHAIN (field))
  {
    if (TREE_CODE (field) == FIELD_DECL)
    {
      print_generic_expr(file, field, dump_flags);
      fprintf (file, "\t");
    }
  }
  fprintf (file, ")\n");
  for (tree field = TYPE_FIELDS (cls_type); field; field = DECL_CHAIN (field))
  {
    if (TREE_CODE (field) == FIELD_DECL)
    {
      print_node(file, "", field, 0);
      fprintf (file, "\n");
    }
  }
  fprintf (file, "****** dump class type end. ******\n");
}

static void
dump_base_class_recursive (FILE *file, tree cls_type)
{
  fprintf (file, "------ dump class and its base class recursively ------\n");
  dump_class_type(file, cls_type);
  tree base_type;
  while (TYPE_BINFO(cls_type) && !BINFO_VTABLE(TYPE_BINFO(cls_type)))
  {
    for (unsigned i = 0; i < BINFO_N_BASE_BINFOS(TYPE_BINFO(cls_type)); i++)
    {
      base_type = BINFO_TYPE(BINFO_BASE_BINFO(TYPE_BINFO(cls_type), i));
      fprintf(file, "****** base class: %s of class %s\n", get_type_name(base_type), get_type_name(cls_type));
      dump_class_type(file, base_type);
    }
    cls_type = base_type;
  }
  fprintf (file, "------ End dump class and its base class recursively ------\n");
}

/* index starts from 1 and considering base object member */
static tree
nth_field (tree type, int index)
{
  int i = 1;
  for (tree field = TYPE_FIELDS (type); field; i++, field = DECL_CHAIN (field))
  {
    if (i == index)
    {
      return field;
    }
  }
  gcc_unreachable ();
}

/* should relayout_decl (base_decl), the first field */
static void
reorder_class (tree type, int id)
{
  tree members[22];
  int i = 0;
  for (tree field = TYPE_FIELDS (type); field; i++, field = DECL_CHAIN (field))
  {
    if (TREE_CODE (field) == FIELD_DECL)
    {
      members[i] = field;
    }
  }
  if (dump_file)
  {
    fprintf (dump_file, "\n====== %s class before reordering ======\n", get_type_name (type));
    dump_class_type(dump_file, type);
  }
  //do not worry, all members are FIELD_DECL, no static members.
  if (id == 2)
  {
    tree f_msgkind = members[1], f_prior = members[2], f_srcprocid = members[3],
    f_parlistp = members[4], f_sent = members[12],
    f_delivd = members[13], f_tstamp = members[14], f_heapindex = members[15],  f_insertordr = members[16],
    f_prev_event_num = members[17];
    DECL_CHAIN(f_srcprocid) = f_delivd;
    DECL_CHAIN(f_delivd) = f_insertordr;
    DECL_CHAIN(f_insertordr) = f_parlistp;
    DECL_CHAIN(f_sent) = f_tstamp;
    DECL_CHAIN(f_heapindex) = f_prev_event_num;
    relayout_decl (members[0]);
  }
  else if (id == 1)
  {
    tree f_msgkind = members[1], f_prior = members[2], f_len = members[3],
    f_error = members[4], f_tstamp = members[5], f_parlistp = members[6],
    f_encapmsg = members[7], f_contextptr = members[8], f_frommod = members[9],
    f_fromgate = members[10], f_tomod = members[11], f_togate = members[12],
    f_created = members[13], f_sent = members[14], f_delivd = members[15],
    f_heapindex = members[16], f_insertordr = members[17];
    //reorder cMessage class member
    DECL_CHAIN(members[0]) = f_error;
    DECL_CHAIN(f_error) = f_created;
    DECL_CHAIN(f_created) = f_msgkind;
    DECL_CHAIN(f_prior) = f_delivd;
    DECL_CHAIN(f_delivd) = f_insertordr;
    DECL_CHAIN(f_insertordr) = f_len;
    DECL_CHAIN(f_len) = f_sent;
    DECL_CHAIN(f_sent) = f_frommod;
    DECL_CHAIN(f_togate) = f_heapindex;
    DECL_CHAIN(f_heapindex) = f_tstamp;
    DECL_CHAIN(f_contextptr) = NULL_TREE;
    /*DECL_CHAIN(members[0]) = f_created;
    DECL_CHAIN(f_created) = f_msgkind;
    DECL_CHAIN(f_prior) = f_delivd;
    DECL_CHAIN(f_delivd) = f_insertordr;
    DECL_CHAIN(f_insertordr) = f_len;
    DECL_CHAIN(f_len) = f_sent;
    DECL_CHAIN(f_sent) = f_frommod;
    DECL_CHAIN(f_togate) = f_heapindex;
    DECL_CHAIN(f_heapindex) = f_error;
    DECL_CHAIN(f_error) = f_tstamp;
    DECL_CHAIN(f_contextptr) = NULL_TREE;*/
    /* base class reordered, decl size should be adjusted(reduced) 
       to reuse tail padding */
    DECL_SIZE(members[0]) = build_int_cst_type(bitsizetype, 400);
    DECL_SIZE_UNIT(members[0]) = build_int_cst_type(sizetype, 50);
  }
  else if (id == 0)
  {
    tree f_namestr = members[1], f_stor = members[2], f_tkownership = members[3],
    f_ownerp = members[4], f_prevp = members[5], f_nextp = members[6],
    f_firstchildp = members[7];
    //reorder cObject class member
    DECL_CHAIN(f_namestr) = f_ownerp;
    DECL_CHAIN(f_firstchildp) = f_stor;
    DECL_CHAIN(f_tkownership) = NULL_TREE;
  }
  else if (id == -1)
  {
    tree f_ownerp = members[1], f_pos = members[2];
    DECL_CHAIN(members[0]) = f_pos;
    DECL_CHAIN(f_pos) = f_ownerp;
    DECL_CHAIN(f_ownerp) = NULL_TREE;
  }
  else
    gcc_unreachable ();

  TYPE_SIZE (type) = NULL_TREE;
  if (TYPE_ALIAS_SET_KNOWN_P(type))
  {
    int t_alias_set = TYPE_ALIAS_SET(type);
    TYPE_ALIAS_SET(type) = -1;
    layout_type (type);
    TYPE_ALIAS_SET(type) = t_alias_set;
  }
  else
  {
    layout_type (type);
  }
  if (id > 0)
    candi_types.safe_push (type);
  else
    candi_btypes.safe_push (type);

  if (dump_file)
  {
    fprintf (dump_file, "\n====== %s class after reordering ======\n", get_type_name (type));
    dump_class_type(dump_file, type);
  }
}

static void
record_orimsg_off(tree record_type)
{
  for (tree field = TYPE_FIELDS (record_type); field; field = DECL_CHAIN (field))
  {
    if (TREE_CODE (field) == FIELD_DECL)
    {
      msg_off_to_field.put(byte_position(field), copy_node(field));
    }
  }
}
static void
analyse_var (tree var)
{
  /*if (dump_file)
  {
    fprintf (dump_file, "var: ");
    print_generic_expr (dump_file, var);
    fprintf (dump_file, "\n");
  }*/

  tree type = TREE_TYPE (var);
  if (!handled_type (type))
  {
    /*if (dump_file)
    {     
      fprintf (dump_file, "not handled type\n");
    }*/
    return;
  }
  type = inner_type (type);
  const char *tname = get_type_name (type);
  if (!tname)
    return;
  
  if (!strcmp(tname, "cObject") && num_fields (type) == 8 && !candi_btypes.contains(type))
  {
    tree field = nth_field (type, 3);
    //already reordered
    if (TREE_CODE(TREE_TYPE(field)) == POINTER_TYPE)
    {
      //if (dump_file)
      //{ 
      //  fprintf (dump_file, "3rd member of cObject is: ");
      //  print_generic_expr (dump_file, field);
      //  fprintf (dump_file, ", class already reordered\n");
      //}
      return;
    }
    if (dump_file)
    {
      fprintf (dump_file, "reorder cObject type of var: ");
      print_generic_expr (dump_file, var);
      fprintf (dump_file, "\n");
    }
    reorder_class (type, 0);
    return;
  }
  
  if (!strcmp(tname,"cOwnedObject") && num_fields (type) == 3 && !candi_btypes.contains(type))
  {
    tree field = nth_field (type, 2);
    if (TREE_CODE(TREE_TYPE(field)) != POINTER_TYPE)
      return;
    if (dump_file)
    {
      fprintf (dump_file, "reorder cOwnedObject type of var: ");
      print_generic_expr (dump_file, var);
      fprintf (dump_file, "\n");
    }
    reorder_class (type, -1);
    return;
  }
  /*if (!strcmp(tname, "cPacket"))
  {
    if (dump_file)
      dump_base_class_recursive (dump_file, type);
  }*/

  if (strcmp(tname, "cMessage") || candi_types.contains(type))
    return;
  if (!TYPE_FIELDS(type) || !DECL_CHAIN(TYPE_FIELDS(type)))
  {
    if (dump_file)
    {
      fprintf (dump_file, "cMessage type tree node has no fields, skip over\n");
      dump_class_type (dump_file, type);
    }
    return;
  }
  tree field = nth_field(type, 2);
  //471. first member: int, after reorder: bool
  if (num_fields (type) == 18 && tree_to_shwi (DECL_SIZE_UNIT(field)) == 4)
  {
    //if (dump_file)
    //{       
    //  fprintf (dump_file, "2rd member of cmessage is: ");
    //  print_generic_expr (dump_file, field);
    //  fprintf (dump_file, ", class already reordered\n");
    //}     
    if (dump_file)
    {
      fprintf (dump_file, "reorder cMessage type of var: ");
      print_generic_expr (dump_file, var);
      fprintf (dump_file, "\n");
    }
    reorder_class (type, 1);
  }//520. first member: short
  else if (num_fields (type) == 20 && tree_to_shwi (DECL_SIZE_UNIT(field)) == 2)
  {
    field = nth_field (type, 5);
     /*if (dump_file)
     {
       fprintf (dump_file, "5th member: ");
       print_generic_expr (dump_file, field);
       fprintf (dump_file, ", tree code name: %s\n", get_tree_code_name(TREE_CODE(TREE_TYPE(field))));
     }*/
    //4th member not cArray *parlistp, means already reordered.
    if (TREE_CODE(TREE_TYPE(field)) != POINTER_TYPE) 
      return;
    if (dump_file)
    {
      fprintf (dump_file, "reorder cMessage type of var: ");
      print_generic_expr (dump_file, var);
      fprintf (dump_file, "\n");
    }
    if (msg_off_to_field.is_empty())
    {
      if (dump_file)
      {
        fprintf (dump_file, "record original message type offset to field map\n");
      }
      record_orimsg_off(type);
    }
    reorder_class (type, 2);
  }
  else 
  {
    if (dump_file)
    {
      fprintf (dump_file, "unexpected situation!!  fields num: %d, first member type size: %d\n",
      num_fields (type), tree_to_shwi (DECL_SIZE_UNIT(field)));
    }
  }
}

static tree
rewrite_offset (tree offset)
{
  HOST_WIDE_INT off = tree_to_shwi(offset);
  tree member;
  for (hash_map<tree, tree>::iterator it = msg_off_to_field.begin();
      it != msg_off_to_field.end(); ++it)
  {
    /*if (dump_file)
    {
      fprintf (dump_file, "off to field pair: ");
      print_generic_expr(dump_file, (*it).first);
      fprintf (dump_file, " > ");
      print_generic_expr(dump_file, (*it).second);
      fprintf (dump_file, "\n");
    }*/
    if (tree_to_shwi((*it).first) == off)
    {
      member = (*it).second;
      break;
    }
  }
  if (dump_file)
  {
    fprintf (dump_file, "rewrite offset for field: ");
    print_generic_expr(dump_file, member);
    fprintf (dump_file, "\n");
    /*print_node(dump_file, "", member, 0);
    dump_node (member, TDF_RAW | dump_flags, dump_file);
    fprintf (dump_file, "\n");*/
  }
  for (tree field = TYPE_FIELDS(candi_types[0]); field; field = DECL_CHAIN(field))
  {
    if (TREE_CODE(field) == FIELD_DECL)
    {
      /*if (dump_file)
        {
        print_node(dump_file, "", field, 0);
        dump_node (field, TDF_RAW | dump_flags, dump_file);
        fprintf (dump_file, "\n");
        }*/
      const char * name = get_name(field);
      const char * ori_name = get_name(member);
      /*if (dump_file)
        {
        fprintf (dump_file, "field name: %s\n", name);
        }*/
      /* operand_equal_p or ==  : infeasible */
      if (name && ori_name && !strcmp(name, ori_name))
      {
        return build_int_cst (TREE_TYPE(offset), int_byte_position(field));
      }
    }
  }
  return NULL;
}

static unsigned int
ipa_class_member_cluster(void)
{
  unsigned int ret = 0, i;
  struct cgraph_node *node;
  function *fn;
  tree var;
  tree t = NULL_TREE;
  /*
  if (dump_file)
  {
    fprintf (dump_file, "====== searching through global decls ======\n");
  }

  varpool_node *vnode;
  FOR_EACH_VARIABLE(vnode)
  {
    if (!vnode->real_symbol_p ())
      continue;
    analyse_var (vnode->decl);
  }
  if (dump_file)
  {
    fprintf (dump_file, "====== "
        "number of cmessage entities: %u, number of cobject entities: %u\n"
        , candi_types.length(), candi_btypes.length());
  }*/


  /* find struct cMessage through all decls of all functions,
     search through all global decls?   */
  /* auto_vec <tree> candi_types;
     candi_types.safe_push (type);
  vec_safe_contains(vec, type)   */
  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY(node)
  {
    if (!node->real_symbol_p () || node->body_removed || !node->decl)
      continue;
    node->get_body ();
    fn = DECL_STRUCT_FUNCTION (node->decl);
    if (!fn || !fn->cfg)
      continue;

    push_cfun (fn);
    //tree candi_type = NULL_TREE, candi_btype = NULL_TREE;
    if (dump_file)
    {
      fprintf (dump_file, "\n****** Searching through Function: %s ******\n\n", node->name ());
    }
    /*if (dump_file)
    {
      //fprintf (dump_file, "====== searching through func arguments ======\n");
      fprintf (dump_file, "func arguments: ");
      for (tree parm = DECL_ARGUMENTS (node->decl); parm;
          parm = DECL_CHAIN (parm))
      {
        //analyse_var (parm);
      }
      fprintf (dump_file, "\n");
    }*/
    if (dump_file)
    {
      fprintf (dump_file, "====== searching through local decls ======\n");
    }
    FOR_EACH_LOCAL_DECL (cfun, i, var)
    {
      if (!VAR_P(var))
        continue;
      analyse_var (var);
    }
    if (dump_file)
    {     
      fprintf (dump_file, "====== searching through local ssa_names ======\n");
    }
    for (i = 1; i < num_ssa_names; ++i)
    {
      tree name = ssa_name (i);
      if (!name
          || has_zero_uses (name)
          || virtual_operand_p (name))
        continue;
      analyse_var (name);
    }
    // 520 cMessage type reordered, need rewrite offset
    if (!msg_off_to_field.is_empty())
    {
      if (dump_file)
      {
        //fprintf (dump_file, "====== searching through all operands of all stmts ======\n");
        fprintf (dump_file, "====== rewrite pointer offset for all stmts ======\n");
      }
      tree abnormal_ssa = NULL_TREE;
      const char *type_name;
      basic_block bb;
      FOR_EACH_BB_FN(bb, cfun)
      {
        gimple_stmt_iterator si;
        for (si = gsi_start_nondebug_after_labels_bb(bb);
            !gsi_end_p(si); gsi_next_nondebug(&si))
        {
          gimple *stmt = gsi_stmt(si);
          /*if (is_gimple_call(stmt))
            {
            tree gc_lhs = gimple_call_lhs (stmt);
            if (!gc_lhs)
            continue;
            tree fn = gimple_call_fn (stmt);
            if (dump_file)
            {
            fprintf (dump_file, "lhs is void pointer type: %s, call internal function: %s\n", 
            VOID_POINTER_P(TREE_TYPE(gc_lhs)) ? "true" : "false", 
            gimple_call_internal_p(stmt) ? "true" : "false");
            fprintf (dump_file, "print the fn called : ");
            print_generic_expr (dump_file, fn, dump_flags | TDF_RAW);
            fprintf (dump_file, "\n");
            print_gimple_stmt (dump_file, stmt, 0, dump_flags | TDF_RAW);
            fprintf (dump_file, "\ncall args:\n");
            for (unsigned i = 0; i < gimple_call_num_args(stmt); i++)
            {
            tree arg = gimple_call_arg(stmt, i);
            print_node(dump_file, "", arg, 0);
            fprintf (dump_file, "\n");
            }
            }
            }*/
          if (!gimple_assign_single_p(stmt))
            continue;
          tree mem_ref;
          tree lhs = gimple_assign_lhs (stmt);
          tree rhs = gimple_assign_rhs1 (stmt);
          /*if (dump_file)
            {
            fprintf (dump_file, "gimple assign rhs code: %s\n", get_tree_code_name(gimple_assign_rhs_code(stmt)));
            }*/
          /* rewrite rhs offset like _28 = MEM[(const struct SimTime &)msg_14(D) + 88].t; */
          type_name = get_type_name (TREE_TYPE(rhs));
          if (TREE_CODE(rhs) == COMPONENT_REF)
          {
            mem_ref = TREE_OPERAND(rhs, 0);
            tree field = TREE_OPERAND(rhs, 1);
            const char *field_name = get_name(field);
            type_name = get_type_name (TREE_TYPE(mem_ref));
            /*if  (dump_file)
              {
              fprintf (dump_file, "mem ref type:"); 
              print_generic_expr (dump_file, TREE_TYPE(mem_ref));
              fprintf (dump_file, ", type name: %s\n", type_name);
              }*/
            if (TREE_CODE(mem_ref) == MEM_REF && type_name && !strcmp(type_name, "SimTime") 
                && field_name && !strcmp (field_name, "t"))
            {
              tree offset = TREE_OPERAND(mem_ref, 1);
              HOST_WIDE_INT off = tree_to_shwi(offset);
              if (off > 87 && off < 113)
              {
                if (dump_file)
                {
                  fprintf (dump_file, "gimple stmt: ");
                  print_gimple_stmt (dump_file, stmt, 0);
                  //fprintf (dump_file, "gimple code name: %s\n", gimple_code_name[gimple_code (stmt)]);
                }
                /*if (dump_file)
                  {
                  fprintf (dump_file, "mem ref:\n");
                  print_node (dump_file, "", mem_ref, 0);
                  fprintf (dump_file, "\n");
                  }*/
                tree new_offset = rewrite_offset (offset);
                TREE_OPERAND(mem_ref, 1) = new_offset;
                update_stmt(stmt);
                if (dump_file)
                {
                  fprintf (dump_file, "**** after rewrite rhs offset:");
                  print_gimple_stmt (dump_file, stmt, 0);
                }
                continue;
              }
            }
          }

          /* record abnormal ssa (void * or cPacket * or other type) 
             like _9 in MEM[(struct cMessage *)_9] ={v} {CLOBBER}; */
          type_name = get_type_name (TREE_TYPE(lhs));
          /*if (gimple_clobber_p(stmt) && TREE_CODE(lhs) == MEM_REF)
            {
            if (dump_file)
            {
            fprintf (dump_file, "clobber stmt and lhs is mem_ref\n");
            }
            if (type_name && !strcmp(type_name, "cMessage"))
            {
            if (dump_file)
            {         
            fprintf (dump_file, "mem ref type is cMessage\n");
            }       
            tree ptroff = TREE_OPERAND(lhs, 1);
            if (dump_file)
            {           
            fprintf (dump_file, "ptr offset: ");
            print_generic_expr (dump_file, ptroff);
            fprintf (dump_file, "\n");
            fprintf (dump_file, "tree_to_shwi(ptroff): %ld, "
            "ptroff == integer_zero_node: %s"
            "operand equal(ptroff, integer_zero_node): %s"
            "ptroff == size_zero_node: %s"
            "operand equal(ptroff, size_zero_node): %s"
            "integer_zerop: %s\n", 
            tree_to_shwi(ptroff),
            ptroff == integer_zero_node ? "true" : "false",
            operand_equal_p(ptroff, integer_zero_node) ? "true" : "false",
            ptroff == size_zero_node ? "true" : "false",
            operand_equal_p(ptroff, size_zero_node) ? "true" : "false",
            integer_zerop(ptroff) ? "true" : "false");
            }           
            }
            }*/
          if (gimple_clobber_p(stmt) && TREE_CODE(lhs) == MEM_REF
              && type_name && !strcmp(type_name, "cMessage")
              && integer_zerop(TREE_OPERAND(lhs, 1)))
          {
            analyse_var (lhs);
            abnormal_ssa = TREE_OPERAND(lhs, 0);
            if(dump_file)
            {
              fprintf(dump_file, "found abnormal ssa: ");
              print_generic_expr (dump_file, abnormal_ssa);
              fprintf(dump_file, "\n");
            }
            continue;
          }
          /* rewrite rhs offset, field define
             like MEM[(struct SimTime *)_9 + 88B] ={v} {CLOBBER};
             MEM[(struct SimTime *)_9 + 88B].t = 0; */
          /* check stmt form */
          if ((TREE_CODE(lhs) != MEM_REF || !gimple_clobber_p(stmt))
              && (TREE_CODE(lhs) != COMPONENT_REF 
                || TREE_CODE(TREE_OPERAND(lhs, 0)) != MEM_REF 
                || !IDENTIFIER_POINTER(DECL_NAME(TREE_OPERAND(lhs, 1))) 
                || strcmp(IDENTIFIER_POINTER(DECL_NAME(TREE_OPERAND(lhs, 1))), "t")))
          {
            if (dump_file)
            {
              fprintf (dump_file, "not valid rewrite rhs offset stmt\n");
            }
            continue;
          }
          //check mem_ref
          mem_ref = TREE_CODE(lhs) == COMPONENT_REF ?
            TREE_OPERAND(lhs, 0) : lhs; 
          /*if (dump_file)
          {
            fprintf (dump_file, "mem ref: ");
            print_generic_expr (dump_file, mem_ref);
            fprintf(dump_file, "\n");
          }*/
          tree ptr = TREE_OPERAND(mem_ref, 0);
          tree ptr_offset = TREE_OPERAND(mem_ref, 1);
          type_name = get_type_name (TREE_TYPE(mem_ref));
          if (!type_name || strcmp(type_name, "SimTime"))
          {
            if (dump_file)
            {
              fprintf (dump_file, "memref type: %s is not SimTime\n", type_name ? type_name : "null");
            }
            continue;
          }
          if (ptr != abnormal_ssa
              && !SSA_NAME_IS_DEFAULT_DEF(ptr)
              && (TREE_CODE(TREE_TYPE(ptr)) != POINTER_TYPE || !get_type_name(TREE_TYPE(TREE_TYPE(ptr))) 
                || strcmp(get_type_name(TREE_TYPE(TREE_TYPE(ptr))), "cMessage")))
          {
            if (dump_file)
            {
              fprintf (dump_file, "not specific abnormal pointer or cmessage ptr or default define ssa.\n");
            }
            continue;
          }

          if (!ptr_offset
              || tree_to_shwi(ptr_offset) < 88 || tree_to_shwi(ptr_offset) > 112)
          {
            if (dump_file)
            {
              fprintf (dump_file, "ptr offset: %d\n", 
                  ptr_offset ? tree_to_shwi(ptr_offset) : 0);
              fprintf (dump_file, "not valid ptr offset\n");
            }
            continue;
          }
          if (dump_file)
          {
            fprintf (dump_file, "gimple stmt: ");
            print_gimple_stmt (dump_file, stmt, 0);
          }
          //rewrite ptr offset
          tree new_offset = rewrite_offset (ptr_offset);
          TREE_OPERAND(mem_ref, 1) = new_offset;
          update_stmt(stmt);
          if (dump_file)
          {
            fprintf (dump_file, "**** rewrite offset to:");
            print_gimple_stmt (dump_file, stmt, 0);
          }
          /*for (i = 0; i < gimple_num_ops(stmt); i++)
            {
            tree op = gimple_op(stmt, i);
            if (dump_file)
            {
            fprintf (dump_file, "operand %u: ", i);
            print_generic_expr (dump_file, op);
            fprintf (dump_file, "\n");
            print_node (dump_file, "", op, 0);
            fprintf (dump_file, "\n"); 
            dump_node (op, dump_flags | TDF_RAW, dump_file);
            fprintf (dump_file, "\n");
            }
            if (!op)
            continue;
            analyse_var (op);
            }*/
        }
      }
    }

    if (dump_file)
    {
      if (current_function_decl)
        dump_function_to_file (current_function_decl, dump_file,
            dump_flags | TDF_VOPS | TDF_LINENO);
      else
        fprintf (dump_file, " no declaration\n");
    }
    pop_cfun ();
  }//end function iterate



  if (dump_file)
  {
    fprintf (dump_file, "=======================\n" 
      "number of cmessage entities: %u, number of cobject entities: %u\n"
      , candi_types.length(), candi_btypes.length());
  }

  return ret;
}

const pass_data pass_data_ipa_class_reorg = 
{
  SIMPLE_IPA_PASS,
  "class_reorg",
  OPTGROUP_NONE,
  TV_IPA_CLASS_REORG,
  0,
  0,
  0,
  0,
  0,
};

class pass_ipa_class_reorg : public simple_ipa_opt_pass
{
  public:
  pass_ipa_class_reorg (gcc::context *ctxt)
  : simple_ipa_opt_pass (pass_data_ipa_class_reorg, ctxt)
  {}

  virtual bool gate (function *);
  virtual unsigned int execute (function *);
};

bool
pass_ipa_class_reorg::gate(function *)
{
  return (optimize >=3
  && flag_ipa_class_reorg
  && !seen_error ()
  && flag_lto_partition == LTO_PARTITION_ONE
  && lang_cxx_p ()
  && (in_lto_p || flag_whole_program));
}

unsigned int
pass_ipa_class_reorg::execute(function *fun )
{
  if (dump_file && (dump_flags & TDF_DETAILS))
  {
    fprintf(dump_file, "====== ipa class reorganization ======\n");
  }
  return ipa_class_member_cluster ();
}

} //anon namespace

simple_ipa_opt_pass *
make_pass_ipa_class_reorg (gcc::context *ctxt)
{
  return new pass_ipa_class_reorg (ctxt);
}
