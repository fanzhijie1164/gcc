/* { dg-do compile } */
/* { dg-options "-O3 -fsplit-loops -fdump-tree-lsplit-details" } */
/* { dg-final { scan-tree-dump-times "loop split" 1 "lsplit" } } */

void
f (float *a, const float *b, const float *c, unsigned int n)
{
  for (unsigned int i = 0; i < n; ++i)
    if (i == 0)
      a[i] = b[i] * c[i];
    else
      a[i] = (b[i] + c[i]) * c[i - 1];
}
