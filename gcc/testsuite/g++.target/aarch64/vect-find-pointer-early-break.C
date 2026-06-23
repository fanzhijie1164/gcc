/* { dg-do compile } */
/* { dg-options "-O3 -fdump-tree-vect-details" } */

extern "C" int **
find_ptr (int **first, int **last, int *needle)
{
  for (; first != last; ++first)
    if (*first == needle)
      return first;
  return last;
}

/* { dg-final { scan-tree-dump "LOOP VECTORIZED" "vect" } } */
/* { dg-final { scan-assembler "cmeq\\tv\[0-9\]+\\.2d" } } */
/* { dg-final { scan-assembler "umaxp\\tv\[0-9\]+\\.4s" } } */
