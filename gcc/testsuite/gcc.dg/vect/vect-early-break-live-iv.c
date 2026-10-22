/* { dg-do compile } */
/* { dg-options "-O3 -floop-split-vectorize -fdump-tree-vect-details" } */
/* { dg-add-options vect_early_break } */
/* { dg-require-effective-target vect_early_break } */
/* { dg-final { scan-tree-dump "LOOP VECTORIZED" "vect" } } */

unsigned int
first_mismatch (const unsigned char *a, const unsigned char *b,
		unsigned int limit)
{
  unsigned int i = 1;
  while (i < limit && a[i] == b[i])
    ++i;
  return i;
}
