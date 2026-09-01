;; ftc66x pipeline description
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

(define_automaton "ftc66x")

(define_attr "ftc66x_neon_type"
  "neon_arith_basic, neon_arith_complex,
   neon_multiply, neon_multiply_q,
   neon_multiply_long, neon_mla, neon_mla_q, neon_mla_long,
   neon_sat_mla_long, neon_shift_acc, neon_shift_imm_basic,
   neon_shift_imm_q, neon_shift_imm_complex,
   neon_shift_reg_basic, neon_shift_reg_basic_q, neon_shift_reg_complex,
   neon_shift_reg_complex_q, neon_fp_nar, neon_fp_minmax, neon_fp_reduc_minmax, neon_fp_arith,
   neon_fp_arith_q, neon_fp_reductions_q, neon_fp_cvt_int,
   neon_fp_cvt_int_q, neon_fp_cvt16, neon_fp_mul,
   neon_fp_mul_q, neon_fp_mla, neon_fp_mla_q, neon_fp_recpe_rsqrte,
   neon_fp_recpe_rsqrte_q, neon_fp_recps_rsqrts, neon_fp_recps_rsqrts_q,
   neon_bitops, neon_bitops_q, neon_dups, neon_from_gp,
   neon_from_gp_q, neon_move, neon_tbl3_tbl4, neon_zip_q, neon_to_gp,
   neon_load_a, neon_load_b, neon_load_c, neon_load_d, neon_load_e,
   neon_load_f, neon_store_a, neon_store_b, neon_store_complex,
   unknown"
  (cond [
	  (eq_attr "type" "neon_abs,neon_abs_q,neon_add, neon_add_q, neon_add_long,\
			   neon_add_widen, neon_neg, neon_neg_q,\
			   neon_sub, neon_sub_q,\
			   neon_sub_long, neon_sub_widen, neon_logic,\
			   neon_logic_q, neon_tst, neon_tst_q,\
			   neon_compare, neon_compare_q,\
			   neon_compare_zero, neon_compare_zero_q,\
			   neon_minmax, neon_minmax_q")
	    (const_string "neon_arith_basic")
	  (eq_attr "type" "neon_add_halve_narrow_q,\
			   neon_add_halve, neon_add_halve_q,\
			   neon_sub_halve, neon_sub_halve_q, neon_qabs,\
			   neon_qabs_q, neon_qadd, neon_qadd_q, neon_qneg,\
			   neon_qneg_q, neon_qsub, neon_qsub_q,\
			   neon_sub_halve_narrow_q")
	    (const_string "neon_arith_complex")

	  (eq_attr "type" "neon_mul_b, neon_mul_h, neon_mul_s,\
			   neon_mul_h_scalar, neon_mul_s_scalar,\
			   neon_sat_mul_b, neon_sat_mul_h,\
			   neon_sat_mul_s, neon_sat_mul_h_scalar,\
			   neon_sat_mul_s_scalar,\
			   neon_mul_b_long, neon_mul_h_long,\
			   neon_mul_s_long,\
			   neon_mul_h_scalar_long, neon_mul_s_scalar_long,\
			   neon_sat_mul_b_long, neon_sat_mul_h_long,\
			   neon_sat_mul_s_long, neon_sat_mul_h_scalar_long,\
			   neon_sat_mul_s_scalar_long,\
			   neon_mla_b, neon_mla_h, neon_mla_s,\
			   neon_mla_h_scalar, neon_mla_s_scalar,\
			   neon_mla_b_long, neon_mla_h_long,\
			   neon_mla_s_long,\
			   neon_mla_h_scalar_long, neon_mla_s_scalar_long,\
			   neon_sat_mla_b_long, neon_sat_mla_h_long,\
			   neon_sat_mla_s_long, neon_sat_mla_h_scalar_long,\
			   neon_sat_mla_s_scalar_long")
	    (const_string "neon_multiply")
	  (eq_attr "type" "neon_mul_b_q, neon_mul_h_q, neon_mul_s_q,\
			   neon_mul_h_scalar_q, neon_mul_s_scalar_q,\
			   neon_sat_mul_b_q, neon_sat_mul_h_q,\
			   neon_sat_mul_s_q, neon_sat_mul_h_scalar_q,\
			   neon_sat_mul_s_scalar_q,\
			   neon_mla_b_q, neon_mla_h_q, neon_mla_s_q,\
			   neon_mla_h_scalar_q, neon_mla_s_scalar_q")
	    (const_string "neon_multiply_q")

	  (eq_attr "type" "neon_shift_acc, neon_shift_acc_q")
	    (const_string "neon_shift_acc")
	  (eq_attr "type" "neon_shift_imm, neon_shift_imm_long")
	    (const_string "neon_shift_imm_basic")
          (eq_attr "type" "neon_shift_imm_q, neon_shift_imm_narrow_q")
            (const_string "neon_shift_imm_q")
	  (eq_attr "type" "neon_sat_shift_imm, neon_sat_shift_imm_q,\
			   neon_sat_shift_imm_narrow_q")
	    (const_string "neon_shift_imm_complex")
	  (eq_attr "type" "neon_shift_reg")
	    (const_string "neon_shift_reg_basic")
	  (eq_attr "type" "neon_shift_reg_q")
	    (const_string "neon_shift_reg_basic_q")
	  (eq_attr "type" "neon_sat_shift_reg")
	    (const_string "neon_shift_reg_complex")
	  (eq_attr "type" "neon_sat_shift_reg_q")
	    (const_string "neon_shift_reg_complex_q")

	  (eq_attr "type" "neon_fp_neg_s, neon_fp_neg_s_q,\
			   neon_fp_abs_s, neon_fp_abs_s_q,\
			   neon_fp_round_s, neon_fp_round_s_q,\
			   neon_fp_neg_d, neon_fp_neg_d_q,\
			   neon_fp_abs_d, neon_fp_abs_d_q,\
			   neon_fp_round_d, neon_fp_round_d_q")
	    (const_string "neon_fp_nar")
          (eq_attr "type" "neon_fp_minmax_s,neon_fp_minmax_d,neon_fp_minmax_s_q,neon_fp_minmax_d_q")
            (const_string "neon_fp_minmax")
	  (eq_attr "type" "neon_fp_addsub_s, neon_fp_abd_s,\
			   neon_fp_reduc_add_s, neon_fp_reduc_add_s_q, neon_fp_compare_s,\
			   neon_fp_addsub_d, neon_fp_abd_d,\
			   neon_fp_reduc_add_d, neon_fp_reduc_add_d_q, neon_fp_compare_d")
	    (const_string "neon_fp_arith")
	  (eq_attr "type" "neon_fp_addsub_s_q, neon_fp_abd_s_q,\
			   neon_fp_reduc_add_s_q, neon_fp_compare_s_q,\
			   neon_fp_addsub_d_q, neon_fp_abd_d_q,\
			   neon_fp_reduc_add_d_q, neon_fp_compare_d_q")
	    (const_string "neon_fp_arith_q")
	  (eq_attr "type" "neon_fp_reduc_minmax_s, neon_fp_reduc_minmax_s_q,\
			   neon_fp_reduc_minmax_d, neon_fp_reduc_minmax_d_q")
	    (const_string "neon_fp_reduc_minmax")
	  (eq_attr "type" "neon_fp_to_int_s, neon_int_to_fp_s,\
			   neon_fp_to_int_d, neon_int_to_fp_d")
	    (const_string "neon_fp_cvt_int")
	  (eq_attr "type" "neon_fp_to_int_s_q, neon_int_to_fp_s_q,\
			   neon_fp_to_int_d_q, neon_int_to_fp_d_q")
	    (const_string "neon_fp_cvt_int_q")
	  (eq_attr "type" "neon_fp_cvt_narrow_s_q, neon_fp_cvt_widen_h")
	    (const_string "neon_fp_cvt16")
	  (eq_attr "type" "neon_fp_mul_s, neon_fp_mul_s_scalar,\
			   neon_fp_mul_d")
	    (const_string "neon_fp_mul")
	  (eq_attr "type" "neon_fp_mul_s_q, neon_fp_mul_s_scalar_q,\
			   neon_fp_mul_d_q, neon_fp_mul_d_scalar_q")
	    (const_string "neon_fp_mul_q")
	  (eq_attr "type" "neon_fp_mla_s, neon_fp_mla_s_scalar,\
			   neon_fp_mla_d")
	    (const_string "neon_fp_mla")
	  (eq_attr "type" "neon_fp_mla_s_q, neon_fp_mla_s_scalar_q,\
			   neon_fp_mla_d_q, neon_fp_mla_d_scalar_q")
	    (const_string "neon_fp_mla_q")
	  (eq_attr "type" "neon_fp_recpe_s, neon_fp_rsqrte_s,\
			   neon_fp_recpx_s,\
			   neon_fp_recpe_d, neon_fp_rsqrte_d,\
			   neon_fp_recpx_d")
	    (const_string "neon_fp_recpe_rsqrte")
	  (eq_attr "type" "neon_fp_recpe_s_q, neon_fp_rsqrte_s_q,\
			   neon_fp_recpx_s_q,\
			   neon_fp_recpe_d_q, neon_fp_rsqrte_d_q,\
			   neon_fp_recpx_d_q")
	    (const_string "neon_fp_recpe_rsqrte_q")
	  (eq_attr "type" "neon_fp_recps_s, neon_fp_rsqrts_s,\
			   neon_fp_recps_d, neon_fp_rsqrts_d")
	    (const_string "neon_fp_recps_rsqrts")
	  (eq_attr "type" "neon_fp_recps_s_q, neon_fp_rsqrts_s_q,\
			   neon_fp_recps_d_q, neon_fp_rsqrts_d_q")
	    (const_string "neon_fp_recps_rsqrts_q")
	  (eq_attr "type" "neon_bsl, neon_cls, neon_cnt,\
			   neon_rev, neon_permute, neon_rbit,\
			   neon_tbl1, neon_tbl2, neon_zip,\
			   neon_ext, neon_ext_q,\
			   neon_move, neon_move_q, neon_move_narrow_q")
	    (const_string "neon_bitops")
	  (eq_attr "type" "neon_bsl_q, neon_cls_q, neon_cnt_q,\
			   neon_rev_q, neon_permute_q, neon_rbit_q")
	    (const_string "neon_bitops_q")
          (eq_attr "type" "neon_dup, neon_dup_q")
            (const_string "neon_dups")
	  (eq_attr "type" "neon_from_gp,f_mcr,f_mcrr")
	    (const_string "neon_from_gp")
	  (eq_attr "type" "neon_from_gp_q")
	    (const_string "neon_from_gp_q")

	  (eq_attr "type" "f_loads, f_loadd,\
			   neon_load1_1reg, neon_load1_1reg_q,\
			   neon_load1_2reg, neon_load1_2reg_q")
	    (const_string "neon_load_a")
	  (eq_attr "type" "neon_load1_3reg, neon_load1_3reg_q,\
			   neon_load1_4reg, neon_load1_4reg_q")
	    (const_string "neon_load_b")
	  (eq_attr "type" "neon_load1_one_lane, neon_load1_one_lane_q,\
			   neon_load1_all_lanes, neon_load1_all_lanes_q,\
			   neon_load2_2reg, neon_load2_2reg_q,\
			   neon_load2_all_lanes, neon_load2_all_lanes_q")
	    (const_string "neon_load_c")
	  (eq_attr "type" "neon_load2_4reg, neon_load2_4reg_q,\
			   neon_load3_3reg, neon_load3_3reg_q,\
			   neon_load3_one_lane, neon_load3_one_lane_q,\
			   neon_load4_4reg, neon_load4_4reg_q")
	    (const_string "neon_load_d")
	  (eq_attr "type" "neon_load2_one_lane, neon_load2_one_lane_q,\
			   neon_load3_all_lanes, neon_load3_all_lanes_q,\
			   neon_load4_all_lanes, neon_load4_all_lanes_q")
	    (const_string "neon_load_e")
	  (eq_attr "type" "neon_load4_one_lane, neon_load4_one_lane_q")
	    (const_string "neon_load_f")

	  (eq_attr "type" "f_stores, f_stored,\
			   neon_store1_1reg")
	    (const_string "neon_store_a")
	  (eq_attr "type" "neon_store1_2reg, neon_store1_1reg_q")
	    (const_string "neon_store_b")
	  (eq_attr "type" "neon_store1_3reg, neon_store1_3reg_q,\
			   neon_store3_3reg, neon_store3_3reg_q,\
			   neon_store2_4reg, neon_store2_4reg_q,\
			   neon_store4_4reg, neon_store4_4reg_q,\
			   neon_store2_2reg, neon_store2_2reg_q,\
			   neon_store3_one_lane, neon_store3_one_lane_q,\
			   neon_store4_one_lane, neon_store4_one_lane_q,\
			   neon_store1_4reg, neon_store1_4reg_q,\
			   neon_store1_one_lane, neon_store1_one_lane_q,\
			   neon_store2_one_lane, neon_store2_one_lane_q")
	    (const_string "neon_store_complex")]
	  (const_string "unknown")))

;; The ftc66x core has the following functional units

;; 1.  Two pipelines for simple integer operations: SIU1, SIU2

(define_cpu_unit "ftc66x_siu1_issue" "ftc66x")
(define_reservation "ftc66x_siu1" "ftc66x_siu1_issue")

(define_cpu_unit "ftc66x_siu2_issue" "ftc66x")
(define_reservation "ftc66x_siu2" "ftc66x_siu2_issue")

;; 2.  One pipeline for complex integer operations: CIU

(define_cpu_unit "ftc66x_ciu_issue" "ftc66x")
(define_reservation "ftc66x_ciu" "ftc66x_ciu_issue")

;; 3.  Two asymmetric pipelines for Asimd and FP operations: FVU1, FVU2

(define_automaton "ftc66x_fvu")

(define_cpu_unit "ftc66x_fvu1_issue" "ftc66x_fvu")
(define_reservation "ftc66x_fvu1" "ftc66x_fvu1_issue")

(define_cpu_unit "ftc66x_fvu2_issue" "ftc66x_fvu")
(define_reservation "ftc66x_fvu2" "ftc66x_fvu2_issue")

;; 4.  One pipeline for branch operations: BRU

(define_cpu_unit "ftc66x_bru_issue" "ftc66x")
(define_reservation "ftc66x_bru" "ftc66x_bru_issue")

;; 5.  One pipeline for load operations: LDU

(define_cpu_unit "ftc66x_ldu_issue" "ftc66x")
(define_reservation "ftc66x_ldu" "ftc66x_ldu_issue")

;; 6.  One pipeline for store operations: STU

(define_cpu_unit "ftc66x_stu_issue" "ftc66x")
(define_reservation "ftc66x_stu" "ftc66x_stu_issue")

;; Block all issue queues.

(define_reservation "ftc66x_block" "ftc66x_siu1_issue + ftc66x_siu2_issue + ftc66x_ciu_issue
				  + ftc66x_fvu1_issue + ftc66x_fvu2_issue
				  + ftc66x_bru_issue + ftc66x_ldu_issue + ftc66x_stu_issue")

;; Integer operations without extra shift

(define_insn_reservation "ftc66x_si_no_shift" 1
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "alu_imm,logic_imm,\
			alu_sreg,logic_reg,\
			adc_imm,adc_reg,\
			adr,clz,rbit,rev,\
			shift_imm,shift_reg,\
			mov_imm,mov_reg,\
			mvn_imm,mvn_reg,\
			no_insn"))
  "ftc66x_siu1|ftc66x_siu2")

;; Integer operations with extra shift

(define_insn_reservation "ftc66x_si_with_shift" 2
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "extend,\
			alu_shift_imm_lsl_1to4,alu_shift_imm_other,alu_shift_reg,\
			logic_shift_imm,logic_shift_reg,\
			mov_shift,mvn_shift,\
			mov_shift_reg,mvn_shift_reg"))
  "ftc66x_ciu")

;; CRC instructions

(define_insn_reservation "ftc66x_si_crc" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "crc"))
  "ftc66x_ciu")

;; Bitfield insertion instructions

(define_insn_reservation "ftc66x_bfm" 2
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "bfm"))
  "ftc66x_ciu")

;; Integer multiply instructions

(define_insn_reservation "ftc66x_mult32" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "mul32" "yes"))
  "ftc66x_ciu")

(define_insn_reservation "ftc66x_mult64" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "widen_mul64" "yes"))
  "ftc66x_ciu")

;; Integer divide instructions

(define_insn_reservation "ftc66x_div" 12
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "udiv,sdiv"))
  "ftc66x_ciu")

;; Block all issue pipes for a cycle
(define_insn_reservation "ftc66x_block" 1
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "block"))
  "ftc66x_block")

;; Branch instructions

(define_insn_reservation "ftc66x_branch" 1
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "branch"))
  "ftc66x_bru")

;; Load instructions

(define_insn_reservation "ftc66x_load1" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "load_4,load_8"))
  "ftc66x_ldu")

;; Store instructions

(define_insn_reservation "ftc66x_store1" 1
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "store_4,store_8"))
  "ftc66x_stu")

;; Advanced SIMD Integer Arithmetic Instructions, basic

(define_insn_reservation  "ftc66x_neon_abd" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_abd"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_aba" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_arith_acc"))
  "ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_aba_q" 5
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_arith_acc_q"))
  "ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_arith_basic" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_arith_basic"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_reduc_add" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_reduc_add,neon_reduc_add_acc"))
  "ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_reduc_add_l" 7
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_reduc_add_long"))
  "ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_reduc_add_q" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_reduc_add_q"))
  "ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_reduc_minmax" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_reduc_minmax"))
  "ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_reduc_minmax_q" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_reduc_minmax_q"))
  "ftc66x_fvu2")

(define_insn_reservation  "ftc66x_neon_arith_complex" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_arith_complex"))
  "ftc66x_fvu1|ftc66x_fvu2")

;; Advanced SIMD Integer Multiply Instructions, D-form

(define_insn_reservation "ftc66x_neon_multiply" 5
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_multiply"))
  "ftc66x_fvu1")

;; Advanced SIMD Integer Multiply Instructions, Q-form

(define_insn_reservation "ftc66x_neon_multiply_q" 6
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_multiply_q"))
  "ftc66x_fvu1")

;; Advanced SIMD Integer Shift Instructions

(define_insn_reservation
  "ftc66x_neon_shift_acc" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_shift_acc"))
  "ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_shift_basic" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_shift_imm_basic,neon_shift_reg_basic"))
  "ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_shift_imm" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_shift_imm_q,neon_shift_imm_complex"))
  "ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_shift_reg" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_shift_reg_basic_q,neon_shift_reg_complex"))
  "ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_shift_complex_q" 5
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_shift_reg_complex_q"))
  "ftc66x_fvu2")

;; Advanced SIMD Floating Point Instructions

(define_insn_reservation
  "ftc66x_neon_fp_nar" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_nar"))
  "(ftc66x_fvu1|ftc66x_fvu2)")

(define_insn_reservation
  "ftc66x_neon_fp_minmax" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_minmax"))
  "(ftc66x_fvu1|ftc66x_fvu2)")

(define_insn_reservation
  "ftc66x_neon_fp_reduc_minmax" 7
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_reduc_minmax"))
  "(ftc66x_fvu1|ftc66x_fvu2)")

(define_insn_reservation
  "ftc66x_neon_fp_arith" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_arith,neon_fp_arith_q"))
  "(ftc66x_fvu1|ftc66x_fvu2)")

(define_insn_reservation
  "ftc66x_neon_fp_cvt_int" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_cvt_int,neon_fp_cvt_int_q"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_fp_mul" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_mul,neon_fp_mul_q"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_fp_mla" 7
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_mla,neon_fp_mla_q,\
	   neon_fp_recps_rsqrts,neon_fp_recps_rsqrts_q"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_fp_recpe_rsqrte" 5
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_fp_recpe_rsqrte,neon_fp_recpe_rsqrte_q"))
  "ftc66x_fvu1|ftc66x_fvu2")

;; Advanced SIMD Miscellaneous Instructions

(define_insn_reservation
  "ftc66x_neon_bitops" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_bitops,neon_bitops_q"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation
  "ftc66x_neon_dups" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "ftc66x_neon_type" "neon_dups"))
  "(ftc66x_ldu+ftc66x_fvu1)|(ftc66x_ldu+ftc66x_fvu2)")

;; Advanced SIMD Load Instructions

(define_insn_reservation
  "ftc66x_neon_ld1_lane" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load1_one_lane,neon_load1_one_lane_q,\
	   neon_load1_all_lanes,neon_load1_all_lanes_q"))
  "(ftc66x_ldu + ftc66x_fvu1)|(ftc66x_ldu + ftc66x_fvu2)")

(define_insn_reservation
  "ftc66x_neon_ld1_reg1" 5
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "f_loads,f_loadd,neon_load1_1reg,neon_load1_1reg_q"))
  "ftc66x_ldu")

(define_insn_reservation
  "ftc66x_neon_ld1_reg2" 5
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load1_2reg"))
  "ftc66x_ldu")

(define_insn_reservation
  "ftc66x_neon_ld1_reg2_q" 6
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load1_2reg_q"))
  "ftc66x_ldu")

(define_insn_reservation
  "ftc66x_neon_ld1_reg3" 6
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load1_3reg"))
  "ftc66x_ldu")

(define_insn_reservation
  "ftc66x_neon_ld1_reg3_q" 7
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load1_3reg_q"))
  "ftc66x_ldu")

(define_insn_reservation
  "ftc66x_neon_ld1_reg4" 6
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load1_4reg"))
  "ftc66x_ldu")

(define_insn_reservation
  "ftc66x_neon_ld1_reg4_q" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load1_4reg_q"))
  "ftc66x_ldu")

(define_insn_reservation
  "ftc66x_neon_ld2" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load2_2reg,neon_load2_2reg_q,neon_load2_all_lanes,\
	   neon_load2_4reg,neon_load2_4reg_q,\
	   neon_load2_all_lanes_q,neon_load2_one_lane,neon_load2_one_lane_q"))
  "(ftc66x_ldu + ftc66x_fvu1)|(ftc66x_ldu + ftc66x_fvu2)")

(define_insn_reservation
  "ftc66x_neon_ld3" 9
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load3_3reg,neon_load3_3reg_q,\
	   neon_load3_one_lane,neon_load3_one_lane_q,\
	   neon_load3_all_lanes,neon_load3_all_lanes_q"))
  "(ftc66x_ldu + ftc66x_fvu1)|(ftc66x_ldu + ftc66x_fvu2)")

(define_insn_reservation
  "ftc66x_neon_ld4" 9
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_load4_4reg,neon_load4_4reg_q,neon_load4_all_lanes,neon_load4_all_lanes_q,\
	   neon_load4_one_lane,neon_load4_one_lane_q"))
  "(ftc66x_ldu + ftc66x_fvu1)|(ftc66x_ldu + ftc66x_fvu2)")

;; Advanced SIMD Store Instructions

(define_insn_reservation
  "ftc66x_neon_st1_lane" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_one_lane"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st1_lane_q" 1
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_one_lane_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg1" 1
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_1reg"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg1_q" 2
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_1reg_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg2" 2
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_2reg"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg2_q" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_2reg_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg3" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_3reg"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg3_q" 6
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_3reg_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg4" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_4reg"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st1_reg4_q" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store1_4reg_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st2_lane" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store2_one_lane"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st2_lane_q" 2
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store2_one_lane_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st2" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store2_2reg,neon_store2_4reg"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st2_q" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store2_2reg_q,neon_store2_4reg_q"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st3_lane" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store3_one_lane"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st3_lane_q" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store3_one_lane_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st3" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store3_3reg"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st3_q" 6
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store3_3reg_q"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st4_lane" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store4_one_lane"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st4_lane_q" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store4_one_lane_q"))
  "ftc66x_stu")

(define_insn_reservation
  "ftc66x_neon_st4" 4
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store4_4reg"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

(define_insn_reservation
  "ftc66x_neon_st4_q" 8
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "neon_store4_4reg_q"))
  "(ftc66x_fvu1 + ftc66x_stu)|(ftc66x_fvu2 + ftc66x_stu)")

;; Floating-Point instructions

(define_insn_reservation "ftc66x_fp_const" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "fconsts,fconstd,fmov"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation "ftc66x_fp_add_sub" 4
  (and (eq_attr "tune" "ftc66x")
    (eq_attr "type" "fadds,faddd,fmuls,fmuld"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation "ftc66x_fp_mac" 7
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "fmacs,ffmas,fmacd,ffmad"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation "ftc66x_fp_cvt" 5
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "f_cvt"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation "ftc66x_fp_cmp" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "fcmps,fcmpd"))
  "ftc66x_fvu2")

(define_insn_reservation "ftc66x_fp_arith" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "ffariths,ffarithd"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation "ftc66x_fp_div_s" 12
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "fdivs,neon_fp_div_s"))
  "ftc66x_fvu1")

(define_insn_reservation "ftc66x_fp_div_d" 19
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "fdivd,neon_fp_div_d,\
           neon_fp_div_s_q,neon_fp_div_d_q"))
  "ftc66x_fvu1")

(define_insn_reservation "ftc66x_fp_sqrt_s" 12
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "fsqrts,neon_fp_sqrt_s"))
  "ftc66x_fvu1")

(define_insn_reservation "ftc66x_fp_sqrt_d" 19
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "fsqrtd,neon_fp_sqrt_d,\
           neon_fp_sqrt_s_q,neon_fp_sqrt_d_q"))
  "ftc66x_fvu1")

(define_insn_reservation "ftc66x_crypto_aes" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "crypto_aese,crypto_aesmc"))
  "ftc66x_fvu1")
  
(define_insn_reservation "ftc66x_crypto_sha1_xor" 6
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "crypto_sha1_xor"))
  "ftc66x_fvu1|ftc66x_fvu2")

(define_insn_reservation "ftc66x_crypto_sha1_fast" 3
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "crypto_sha1_fast"))
  "ftc66x_fvu1")

;; Call instructions

(define_insn_reservation "ftc66x_call" 1
  (and (eq_attr "tune" "ftc66x")
       (eq_attr "type" "call"))
  "ftc66x_siu1_issue+ftc66x_siu2_issue+ftc66x_ciu_issue+ftc66x_fvu1_issue+ftc66x_fvu2_issue\
    +ftc66x_bru_issue+ftc66x_ldu_issue+ftc66x_stu_issue")

(define_bypass 1 "ftc66x_*"
		 "ftc66x_call,ftc66x_branch")
