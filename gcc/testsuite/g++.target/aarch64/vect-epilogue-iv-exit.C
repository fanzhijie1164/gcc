/* { dg-do compile } */
/* { dg-options "-Ofast" } */

extern void *alloc (unsigned long);
extern void release (void *);

struct vec
{
  unsigned int length;
  unsigned int capacity;
  bool *elts;

  void reserve (unsigned int);
};

void
vec::reserve (unsigned int extra)
{
  unsigned int new_capacity = length + extra;
  if (capacity >= new_capacity)
    return;

  unsigned int grown = (unsigned int) ((double) length * 1.25);
  if (new_capacity < grown)
    new_capacity = grown;

  bool *new_elts = (bool *) alloc (new_capacity);
  for (unsigned int i = 0; i < length; ++i)
    new_elts[i] = elts[i];

  release (elts);
  elts = new_elts;
  capacity = new_capacity;
}
