;; ftc86x pipeline description
;; Copyright (C) 2018 Free Software Foundation, Inc.
;;
;; This file is part of GCC.
;;
;; GCC is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.
;;
;; GCC is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING3.  If not see
;; <http://www.gnu.org/licenses/>.

(define_automaton "ftc86x")

;; The ftc86x core has the following functional units

;; 1.  Two pipelines for simple integer operations: SIU1, SIU2

(define_cpu_unit "ftc86x_siu1_issue" "ftc86x")
(define_reservation "ftc86x_siu1" "ftc86x_siu1_issue")

(define_cpu_unit "ftc86x_siu2_issue" "ftc86x")
(define_reservation "ftc86x_siu2" "ftc86x_siu2_issue")

;; 2.  One pipeline for complex integer operations: CIU

(define_cpu_unit "ftc86x_ciu_issue" "ftc86x")
(define_reservation "ftc86x_ciu" "ftc86x_ciu_issue")

;; 3.  Two asymmetric pipelines for Asimd and FP operations: FVU1, FVU2

(define_automaton "ftc86x_fvu")

(define_cpu_unit "ftc86x_fvu1_issue" "ftc86x_fvu")
(define_reservation "ftc86x_fvu1" "ftc86x_fvu1_issue")

(define_cpu_unit "ftc86x_fvu2_issue" "ftc86x_fvu")
(define_reservation "ftc86x_fvu2" "ftc86x_fvu2_issue")

;; 4.  One pipeline for branch operations: BRU

(define_cpu_unit "ftc86x_bru_issue" "ftc86x")
(define_reservation "ftc86x_bru" "ftc86x_bru_issue")

;; 5.  Two pipelines for load and store operations: LSU1, LSU2

(define_cpu_unit "ftc86x_lsu1_issue" "ftc86x")
(define_reservation "ftc86x_lsu1" "ftc86x_lsu1_issue")

(define_cpu_unit "ftc86x_lsu2_issue" "ftc86x")
(define_reservation "ftc86x_lsu2" "ftc86x_lsu2_issue")

;; Block all issue queues.

(define_reservation "ftc86x_block" "ftc86x_siu1_issue + ftc86x_siu2_issue + ftc86x_ciu_issue
				  + ftc86x_fvu1_issue + ftc86x_fvu2_issue
				  + ftc86x_bru_issue + ftc86x_lsu1_issue + ftc86x_lsu2_issue")

;; Basic integer operations

(define_insn_reservation "ftc86x_basic_alu" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "alu_imm,alu_sreg,\
                        alus_imm,alus_sreg,\
                        logic_imm,logic_reg,\
                        logics_imm,logics_reg,\
                        adc_imm,adc_reg,\
                        no_insn"))
  "ftc86x_siu1|ftc86x_siu2")

;; Integer operations with extra extension and shift

(define_insn_reservation "ftc86x_alu_extend_shift" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "alu_shift_imm_lsl_1to4,alu_shift_imm_other,alu_shift_reg,alus_shift_imm,alus_shift_reg,\
                        alu_ext,alus_ext,\
                        logics_shift_imm,logics_shift_reg"))
  "ftc86x_ciu")

;; Logical shift without setting flags

(define_insn_reservation "ftc86x_logic_shift_noflag" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "logic_shift_imm,logic_shift_reg"))
  "ftc86x_siu1|ftc86x_siu2")

;; Conditional selection

(define_insn_reservation "ftc86x_cond_sel" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "csel"))
  "ftc86x_siu1|ftc86x_siu2")

;; Basic move operations

(define_insn_reservation "ftc86x_basic_mov" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "mov_imm,mov_reg"))
  "ftc86x_siu1|ftc86x_siu2")

;; CRC instructions

(define_insn_reservation "ftc86x_crc" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crc"))
  "ftc86x_ciu")

;; Bitfield insertion instructions

(define_insn_reservation "ftc86x_bfm" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "bfm"))
  "ftc86x_ciu")

;; Integer multiply instructions

(define_insn_reservation "ftc86x_mult" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "mul"))
  "ftc86x_ciu")

(define_insn_reservation "ftc86x_mult_l" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "smull,umull"))
  "ftc86x_ciu")

(define_insn_reservation "ftc86x_mlal" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "mla,smlal,umlal"))
  "ftc86x_ciu")

;; Integer divide instructions

(define_insn_reservation "ftc86x_div" 12
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "udiv,sdiv"))
  "ftc86x_ciu")

;; Block all issue pipes for a cycle
(define_insn_reservation "ftc86x_block_1" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "block"))
  "ftc86x_block")

;; Branch instructions

(define_insn_reservation "ftc86x_branch" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "branch"))
  "ftc86x_bru")

;; Count leading

(define_insn_reservation "ftc86x_clz" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "clz"))
  "ftc86x_siu1|ftc86x_siu2")

;; Reverse bits or bytes

(define_insn_reservation "ftc86x_rbit_rev" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "rbit,rev"))
  "ftc86x_siu1|ftc86x_siu2")

;; Load instructions

(define_insn_reservation "ftc86x_load1" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "load_4"))
  "ftc86x_lsu1|ftc86x_lsu2")

(define_insn_reservation "ftc86x_load2" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "load_8,load_16,neon_ldp,neon_ldp_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

;; Store instructions

(define_insn_reservation "ftc86x_store1" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "store_4,store_8,store_16,neon_stp,neon_stp_q"))
  "(ftc86x_lsu1+ftc86x_siu2)|(ftc86x_lsu2+ftc86x_siu2)")

;; Advanced SIMD Integer Arithmetic Instructions

(define_insn_reservation  "ftc86x_neon_abd" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_abd,neon_abd_long,neon_abd_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation  "ftc86x_neon_aba" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_arith_acc,neon_arith_acc_q"))
  "ftc86x_fvu2")

(define_insn_reservation  "ftc86x_neon_arith_basic" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_abs,neon_abs_q,\
                        neon_neg,neon_neg_q,\
                        neon_add,neon_add_long,neon_add_q,neon_add_widen,\
                        neon_sub,neon_sub_long,neon_sub_q,neon_sub_widen,\
                        neon_logic,neon_logic_q,\
                        neon_compare,neon_compare_q,neon_compare_zero,neon_compare_zero_q,\
                        neon_minmax,neon_minmax_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation  "ftc86x_neon_reduc_add" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_reduc_add"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation  "ftc86x_neon_reduc_add_lq" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_reduc_add_long,neon_reduc_add_q"))
  "ftc86x_fvu2")

(define_insn_reservation  "ftc86x_neon_reduc_minmax" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_reduc_minmax,neon_reduc_minmax_q"))
  "ftc86x_fvu2")

(define_insn_reservation  "ftc86x_neon_arith_complex" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_add_halve,neon_add_halve_q,neon_add_halve_narrow_q,\
                        neon_sub_halve,neon_sub_halve_q,neon_sub_halve_narrow_q,\
                        neon_qabs,neon_qabs_q,\
                        neon_qneg,neon_qneg_q,\
                        neon_qadd,neon_qadd_q,\
                        neon_qsub,neon_qsub_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

;; Advanced SIMD Integer Multiply Instructions, D-form

(define_insn_reservation "ftc86x_neon_mul" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_mul_b,neon_mul_h,neon_mul_s,\
                        neon_sat_mul_b,neon_sat_mul_h,neon_sat_mul_s,\
                        neon_mul_b_long,neon_mul_h_long,neon_mul_s_long,\
                        neon_sat_mul_b_long,neon_sat_mul_h_long,neon_sat_mul_s_long,\
                        neon_mla_b,neon_mla_h,neon_mla_s,\
                        neon_mla_b_long,neon_mla_h_long,neon_mla_s_long,\
                        neon_sat_mla_b_long,neon_sat_mla_h_long,neon_sat_mla_s_long,\
                        neon_mul_h_scalar,neon_mul_s_scalar,\
                        neon_sat_mul_h_scalar,neon_sat_mul_s_scalar,\
                        neon_mul_h_scalar_long,neon_mul_s_scalar_long,\
                        neon_sat_mul_h_scalar_long,neon_sat_mul_s_scalar_long,\
                        neon_mla_h_scalar,neon_mla_s_scalar,\
                        neon_mla_h_scalar_long,neon_mla_s_scalar_long,\
                        neon_sat_mla_h_scalar_long,neon_sat_mla_s_scalar_long"))
  "ftc86x_fvu1")

;; Advanced SIMD Integer Multiply Instructions, Q-form

(define_insn_reservation "ftc86x_neon_mul_q" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_mul_b_q,neon_mul_h_q,neon_mul_s_q,\
                        neon_sat_mul_b_q,neon_sat_mul_h_q,neon_sat_mul_s_q,\
                        neon_mla_b_q,neon_mla_h_q,neon_mla_s_q,\
                        neon_mul_h_scalar_q,neon_mul_s_scalar_q,\
                        neon_sat_mul_h_scalar_q,neon_sat_mul_s_scalar_q,\
                        neon_mla_h_scalar_q,neon_mla_s_scalar_q"))
  "ftc86x_fvu1")

;; Advanced SIMD Integer Shift Instructions

(define_insn_reservation
  "ftc86x_neon_shift_acc" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_shift_acc,neon_shift_acc_q"))
  "ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_shift_basic" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_shift_imm,neon_shift_imm_long,neon_shift_reg"))
  "ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_fp_xtn" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_shift_imm_narrow_q"))
  "(ftc86x_fvu1|ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_shift_complex" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_shift_imm_q,\
                        neon_sat_shift_imm,neon_sat_shift_imm_q,neon_sat_shift_imm_narrow_q,\
                        neon_shift_reg_q,neon_sat_shift_reg,neon_sat_shift_reg_q"))
  "ftc86x_fvu2")

;; Advanced SIMD Floating Point Instructions

(define_insn_reservation
  "ftc86x_neon_fp_basic" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_abs_s,neon_fp_abs_d,neon_fp_abs_s_q,neon_fp_abs_d_q,\
                        neon_fp_abd_s,neon_fp_abd_d,neon_fp_abd_s_q,neon_fp_abd_d_q,\
                        neon_fp_addsub_s,neon_fp_addsub_d,neon_fp_addsub_s_q,neon_fp_addsub_d_q,\
                        neon_fp_compare_s,neon_fp_compare_d,neon_fp_compare_s_q,neon_fp_compare_d_q,\
                        neon_fp_neg_s,neon_fp_neg_d,neon_fp_neg_s_q,neon_fp_neg_d_q"))
  "(ftc86x_fvu1|ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_fp_minmax" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_minmax_s,neon_fp_minmax_d,neon_fp_minmax_s_q,neon_fp_minmax_d_q"))
  "(ftc86x_fvu1|ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_fp_reduc_minmax" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_reduc_minmax_s,neon_fp_reduc_minmax_d"))
  "(ftc86x_fvu1|ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_fp_reduc_minmax_q" 6
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_reduc_minmax_s_q,neon_fp_reduc_minmax_d_q"))
  "(ftc86x_fvu1|ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_fp_cvt_widen1" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_cvt_widen_h"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_cvt_widen2" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_cvt_widen_s"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_cvt_narrow1" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_cvt_narrow_s_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_cvt_narrow2" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_cvt_narrow_d_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_cvt_other" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_to_int_s,neon_fp_to_int_d,neon_fp_to_int_s_q,neon_fp_to_int_d_q,\
                        neon_int_to_fp_s,neon_int_to_fp_d,neon_int_to_fp_s_q,neon_int_to_fp_d_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_div1" 7
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_div_s"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_div2" 10
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_div_d"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_div3" 13
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_div_s_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_div4" 10
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_div_d_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_mul" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_mul_s,neon_fp_mul_d,\
                        neon_fp_mul_s_q,neon_fp_mul_d_q,\
                        neon_fp_mul_s_scalar_q,neon_fp_mul_d_scalar_q,\
                        neon_fp_mul_s_scalar"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_fp_mla" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_mla_s,neon_fp_mla_d,\
                        neon_fp_mla_s_q,neon_fp_mla_d_q,\
                        neon_fp_mla_s_scalar_q,neon_fp_mla_d_scalar_q,\
                        neon_fp_mla_s_scalar"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_fp_round1" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_round_s,neon_fp_round_d"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_round2" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_round_d_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_round3" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_round_s_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_sqrt1" 7
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_sqrt_s"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_sqrt2" 10
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_sqrt_d"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_sqrt3" 13
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_sqrt_s_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_sqrt4" 10
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_sqrt_d_q"))
  "ftc86x_fvu1")

;; Advanced SIMD Miscellaneous Instructions

(define_insn_reservation
  "ftc86x_neon_rbit" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_rbit,neon_rbit_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_bsl" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_bsl,neon_bsl_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_count" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_cls,neon_cls_q,neon_cnt,neon_cnt_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_dup" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_dup,neon_dup_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_ext" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_ext,neon_ext_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_ins" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_ins,neon_ins_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_move" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_move,neon_move_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_fp_estimate1" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_recpe_s,neon_fp_recpe_d,\
                        neon_fp_rsqrte_s,neon_fp_rsqrte_d"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_estimate2" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_recpe_s_q,neon_fp_rsqrte_s_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_estimate3" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_recpe_d_q,neon_fp_rsqrte_d_q"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_recpx" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_recpx_s,neon_fp_recpx_d"))
  "ftc86x_fvu1")

(define_insn_reservation
  "ftc86x_neon_fp_estimate4" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_fp_recps_s,neon_fp_recps_d,neon_fp_recps_s_q,neon_fp_recps_d_q,\
                        neon_fp_rsqrts_s,neon_fp_rsqrts_d,neon_fp_rsqrts_s_q,neon_fp_rsqrts_d_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_rev" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_rev,neon_rev_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_tbl1" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_tbl1,neon_tbl1_q,neon_tbl2,neon_tbl2_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_tbl2" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_tbl3,neon_tbl3_q,neon_tbl4,neon_tbl4_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation
  "ftc86x_neon_zip" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_zip,neon_zip_q"))
  "ftc86x_fvu1|ftc86x_fvu2")

;; Advanced SIMD Load Instructions

(define_insn_reservation
  "ftc86x_neon_ld1_reg12" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load1_1reg,neon_load1_1reg_q,\
                        neon_load1_2reg,neon_load1_2reg_q"))
  "ftc86x_lsu1|ftc86x_lsu2")

(define_insn_reservation
  "ftc86x_neon_ld1_reg34" 6
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load1_3reg,neon_load1_3reg_q,\
                        neon_load1_4reg,neon_load1_4reg_q"))
  "ftc86x_lsu1|ftc86x_lsu2")

(define_insn_reservation
  "ftc86x_neon_ld1_lane" 7
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load1_one_lane,neon_load1_one_lane_q,\
                        neon_load1_all_lanes,neon_load1_all_lanes_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_ld2" 7
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load2_one_lane,neon_load2_one_lane_q,\
                        neon_load2_all_lanes,neon_load2_all_lanes_q,\
                        neon_load2_2reg,neon_load2_2reg_q,\
                        neon_load2_4reg,neon_load2_4reg_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_ld3_d" 8
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load3_3reg"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_ld3_q" 9
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load3_3reg_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_ld3_lane" 8
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load3_one_lane,neon_load3_one_lane_q,\
                        neon_load3_all_lanes,neon_load3_all_lanes_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_ld4_d" 8
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load4_4reg"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_ld4_q" 10
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load4_4reg_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_ld4_lane" 8
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_load4_one_lane,neon_load4_one_lane_q,\
                        neon_load4_all_lanes,neon_load4_all_lanes_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

;; Advanced SIMD Store Instructions

(define_insn_reservation
  "ftc86x_neon_st1_reg1" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_1reg,neon_store1_1reg_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_st1_reg2_d" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_2reg"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_st1_reg2_q" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_2reg_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_st1_reg3_d" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_3reg"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_st1_reg3_q" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_3reg_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_st1_reg4_d" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_4reg"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_st1_reg4_q" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_4reg_q"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

(define_insn_reservation
  "ftc86x_neon_st1_lane" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store1_one_lane,neon_store1_one_lane_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st2_d" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store2_2reg,neon_store2_4reg"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st2_q" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store2_2reg_q,neon_store2_4reg_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st2_lane" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store2_one_lane,neon_store2_one_lane_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st3_d" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store3_3reg"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st3_q" 6
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store3_3reg_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st3_lane" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store3_one_lane"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st3_lane_q" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store3_one_lane_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st4_d" 7
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store4_4reg"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st4_q" 9
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store4_4reg_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st4_lane" 6
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store4_one_lane"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

(define_insn_reservation
  "ftc86x_neon_st4_lane_q" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "neon_store4_one_lane_q"))
  "(ftc86x_fvu1+ftc86x_lsu1)|(ftc86x_fvu1+ftc86x_lsu2)|(ftc86x_fvu2+ftc86x_lsu1)|(ftc86x_fvu2+ftc86x_lsu2)")

;; Floating-Point instructions

(define_insn_reservation "ftc86x_fp_const" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fconsts,fconstd,fmov"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation "ftc86x_fp_arith" 2
  (and (eq_attr "tune" "ftc86x")
    (eq_attr "type" "fadds,faddd,ffariths,ffarithd"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation "ftc86x_fp_cvt" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "f_cvt"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_cvt_if" 6
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "f_cvti2f"))
  "ftc86x_ciu+ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_cvt_fi" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "f_cvtf2i"))
  "ftc86x_fvu1+ftc86x_fvu2")

(define_insn_reservation "ftc86x_fp_cmp" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fcmps,fcmpd,fccmps,fccmpd"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_div_s" 10
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fdivs"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_div_d" 15
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fdivd"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_minmax" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "f_minmaxs,f_minmaxd"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation "ftc86x_fp_mul" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fmuls,fmuld"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation "ftc86x_fp_mac" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fmacs,fmacd"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation "ftc86x_fp_rint" 3
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "f_rints,f_rintd"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_csel" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fcsel"))
  "ftc86x_fvu1|ftc86x_fvu2")

(define_insn_reservation "ftc86x_fp_sqrt_s" 10
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fsqrts"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_sqrt_d" 17
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "fsqrtd"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_fp_load" 5
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "f_loads,f_loadd"))
  "(ftc86x_lsu1+ftc86x_siu1)|(ftc86x_lsu2+ftc86x_siu1)|(ftc86x_lsu1+ftc86x_siu2)|(ftc86x_lsu2+ftc86x_siu2)")

(define_insn_reservation "ftc86x_fp_store" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "f_stores,f_stored"))
  "(ftc86x_lsu1+ftc86x_fvu1)|(ftc86x_lsu2+ftc86x_fvu1)|(ftc86x_lsu1+ftc86x_fvu2)|(ftc86x_lsu2+ftc86x_fvu2)")

;; Crypto operations

(define_insn_reservation "ftc86x_crypto_aes" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_aese,crypto_aesmc"))
  "ftc86x_fvu1")
 
(define_insn_reservation "ftc86x_crypto_pmull" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_pmull"))
  "ftc86x_fvu1")
 
(define_insn_reservation "ftc86x_crypto_sha1_slow" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sha1_slow"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_crypto_sha1_fast" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sha1_fast,crypto_sha1_xor"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_crypto_sha256_slow" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sha256_slow"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_crypto_sha256_fast" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sha256_fast"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_crypto_sha512" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sha512"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_crypto_sha3" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sha3"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_crypto_sm3" 2
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sm3"))
  "ftc86x_fvu1")

(define_insn_reservation "ftc86x_crypto_sm4" 4
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "crypto_sm4"))
  "ftc86x_fvu1")

;; Call instructions

(define_insn_reservation "ftc86x_call" 1
  (and (eq_attr "tune" "ftc86x")
       (eq_attr "type" "call"))
  "ftc86x_siu1_issue+ftc86x_siu2_issue+ftc86x_ciu_issue+ftc86x_fvu1_issue+ftc86x_fvu2_issue\
    +ftc86x_bru_issue+ftc86x_lsu1_issue+ftc86x_lsu2_issue")

(define_bypass 1 "ftc86x_*"
                 "ftc86x_call,ftc86x_branch")

