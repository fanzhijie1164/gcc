/* { dg-do compile } */
/* { dg-additional-options "-O3 -march=armv8-a --param aarch64-autovec-preference=1 -std=gnu99" } */
/* { dg-final { check-function-bodies "**" "" "" } } */

/*
** f1:
** ...
** (
** add	v[0-9]+.4s, v[0-9]+.4s, v[0-9]+.4s
** |
** shl	v[0-9]+.4s, v[0-9]+.4s, 1
** )
** (
** add	v[0-9]+.4s, v[0-9]+.4s, v[0-9]+.4s
** |
** shl	v[0-9]+.4s, v[0-9]+.4s, 1
** )
** (
** add	v[0-9]+.4s, v[0-9]+.4s, v[0-9]+.4s
** |
** shl	v[0-9]+.4s, v[0-9]+.4s, 1
** )
** (
** add	v[0-9]+.4s, v[0-9]+.4s, v[0-9]+.4s
** |
** shl	v[0-9]+.4s, v[0-9]+.4s, 1
** )
** ...
*/
void f1 (int *restrict a, int n)
{
#pragma GCC unroll 16
  for (int i = 0; i < n; i++)
    a[i] *= 2;
}
