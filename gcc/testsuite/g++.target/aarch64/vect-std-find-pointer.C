/* { dg-do compile } */
/* { dg-options "-O3 -std=gnu++11 -fdump-tree-vect-details" } */

#include <algorithm>

int **
find_ptr_std (int **first, int **last, int *needle)
{
  return std::find (first, last, needle);
}

/* { dg-final { scan-tree-dump "LOOP VECTORIZED" "vect" } } */
/* { dg-final { scan-assembler-times "cmeq\\tv\[0-9\]+\\.2d" 7 } } */
/* { dg-final { scan-assembler "umaxp\\tv\[0-9\]+\\.4s" } } */
