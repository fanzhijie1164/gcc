// { dg-do run { target { i?86-*-* x86_64-*-* } } }
// { dg-options "-Ofast -msse2" }

typedef unsigned short xmlch;

static __attribute__((noipa)) unsigned
length (const xmlch *s)
{
  unsigned n = 0;
  while (s[n] != 0)
    ++n;
  return n;
}

static __attribute__((noipa)) bool
matches (const wchar_t *s)
{
  return (s[0] == L't' && s[1] == L'5' && s[2] == L'.'
	  && s[3] == L'x' && s[4] == L'm' && s[5] == L'l'
	  && s[6] == 0);
}

static __attribute__((noipa)) bool
transcode (const xmlch *input)
{
  wchar_t buf[32];
  unsigned len = length (input);

  for (unsigned i = 0; i < len; ++i)
    buf[i] = input[i];
  buf[len] = 0;

  return matches (buf);
}

int
main ()
{
  const xmlch input[] = { 't', '5', '.', 'x', 'm', 'l', 0 };

  if (!transcode (input))
    __builtin_abort ();

  return 0;
}
