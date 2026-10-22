/* { dg-do compile } */
/* { dg-options "-O3 -mtune=ftc86x -floop-split-vectorize -fdump-tree-vect-details" } */

unsigned int
variable_start (const unsigned char *a, const unsigned char *b,
		unsigned int start, unsigned int limit)
{
  unsigned int i;
  for (i = start; i < limit && a[i] == b[i]; ++i)
    ;
  return i - start;
}

unsigned int
constant_start (const unsigned char *a, const unsigned char *b,
		unsigned int limit)
{
  unsigned int i;
  for (i = 2; i < limit && a[i] == b[i]; ++i)
    ;
  return i;
}

/* { dg-final { scan-tree-dump "variable-start byte search is not profitable on FTC86X" "vect" } } */
/* { dg-final { scan-tree-dump-times "LOOP VECTORIZED" 1 "vect" } } */
