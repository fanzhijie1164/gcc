// { dg-do compile { target aarch64*-*-* } }
// { dg-options "-Ofast -fdump-tree-vect-details" }

#include <algorithm>

typedef int value_type;

bool
release (value_type &value, value_type **first, value_type **last)
{
  value_type **it = std::find (first, last, &value);
  return it != last;
}

// { dg-final { scan-tree-dump "vectorized 1 loops" "vect" } }
