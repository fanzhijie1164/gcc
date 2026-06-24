/* { dg-do compile } */
/* { dg-options "-O3 -std=gnu++11 -fdump-tree-vect-details" } */

#include <algorithm>
#include <vector>

bool
release_ptr (int *needle, std::vector<int *> &v)
{
  std::vector<int *>::iterator it = std::find (v.begin (), v.end (), needle);
  if (it == v.end ())
    return false;

  v.erase (it);
  return true;
}

/* { dg-final { scan-tree-dump "LOOP VECTORIZED" "vect" } } */
/* { dg-final { scan-assembler-times "cmeq\\tv\[0-9\]+\\.2d" 7 } } */
/* { dg-final { scan-assembler "umaxp\\tv\[0-9\]+\\.4s" } } */
/* { dg-final { scan-assembler "ldr\\tx\[0-9\]+, \\[x\[0-9\]+\\], 8" } } */
/* { dg-final { scan-assembler "csel\\tx\[0-9\]+, x\[0-9\]+, x\[0-9\]+, ne" } } */
