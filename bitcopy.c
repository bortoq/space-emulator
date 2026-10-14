/*
 * BIT COPY (clean fast implementation)
 *
 * Bitwise copying between arrays. Written from scratch,
 * no need to find the author.
 *
 * Three paths by half-interval position inside bytes: with equal
 * positions inside the byte, edges with a single mask, memcpy middle;
 * short shifted copies (all fits into a 64-bit word): assemble the word
 * with parallel shifts and distribute byte by byte;
 * long shifted copies: edges via recursion into the same word, middle
 * with a shift-and-carry loop through the neighbor byte, two bytes per
 * iteration with independent computations, no reading ahead (only bytes
 * holding copied bits are read).
 * No path has per-bit loops.
 * Overlap of copied half-intervals is never checked:
 * overlapping copies are guaranteed never to occur
 * (the space device requires non-overlapping intervals).
 *
 * Bit convention: bit 0 of a byte is the high bit (0x80), then in order.
 */
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * copy bits from one array to another (n, src, s, dst, d)
 *
 * n - number of bits to copy
 * src - source array
 * s - source bit index
 * dst - destination array
 * d - destination bit index
 *
 * intervals [s, s + n) and [d, d + n) must not overlap
 */

void bitcpy(size_t n, const void *src_org, size_t src_offset, void *dst_org, size_t dst_offset)
{
  const unsigned char *src;
  unsigned char *dst;

  if(n == 0 || src_org == NULL || dst_org == NULL)
    return;

  src = (const unsigned char*)src_org;
  dst = (unsigned char*)dst_org;

  /* aligned intervals: same position inside the byte at the start */
  if((src_offset & 7) == (dst_offset & 7))
  {
    /* leading partial byte with a single mask */
    if(dst_offset & 7)
    {
      const unsigned char *sp;
      unsigned char *dp;
      unsigned int mask;
      size_t take, m;
      m = dst_offset & 7;
      take = 8 - m;
      if(take > n)
        take = n;
      mask = (unsigned int)(0xFF >> m);
      if(take < 8 - m)
        mask &= (unsigned int)(0xFF << (8 - m - take));
      sp = src + src_offset / CHAR_BIT;
      dp = dst + dst_offset / CHAR_BIT;
      *dp = (unsigned char)((*dp & ~mask) | (*sp & mask));
      src_offset += take;
      dst_offset += take;
      n -= take;
      if(n == 0)
        return;
    }
    /* whole bytes */
    if(n >= 8)
    {
      size_t bytes = n / CHAR_BIT;
      memcpy(dst + dst_offset / CHAR_BIT, src + src_offset / CHAR_BIT, bytes);
      src_offset += bytes * CHAR_BIT;
      dst_offset += bytes * CHAR_BIT;
      n -= bytes * CHAR_BIT;
    }
    /* trailing partial byte with a single mask */
    if(n)
    {
      const unsigned char *sp;
      unsigned char *dp;
      unsigned int mask;
      mask = (unsigned int)(0xFF << (8 - n));
      sp = src + src_offset / CHAR_BIT;
      dp = dst + dst_offset / CHAR_BIT;
      *dp = (unsigned char)((*dp & ~mask) | (*sp & mask));
    }
    return;
  }

  /* short shifted copies: the src window fits into 64 bits */
  if((src_offset & 7) + n <= 64)
  {
    const unsigned char *sp;
    unsigned char *dp;
    uint64_t w;
    size_t sb, m, rem;
    sb = ((src_offset & 7) + n + 7) / 8;
    sp = src + src_offset / CHAR_BIT;
    switch(sb)
    {
      case 1: w = sp[0]; break;
      case 2: w = ((uint64_t)sp[0] << 8) | sp[1]; break;
      case 3: w = ((uint64_t)sp[0] << 16) | ((uint64_t)sp[1] << 8) | sp[2]; break;
      case 4: w = ((uint64_t)sp[0] << 24) | ((uint64_t)sp[1] << 16)
        | ((uint64_t)sp[2] << 8) | sp[3]; break;
      case 5: w = ((uint64_t)sp[0] << 32) | ((uint64_t)sp[1] << 24)
        | ((uint64_t)sp[2] << 16) | ((uint64_t)sp[3] << 8) | sp[4]; break;
      case 6: w = ((uint64_t)sp[0] << 40) | ((uint64_t)sp[1] << 32)
        | ((uint64_t)sp[2] << 24) | ((uint64_t)sp[3] << 16)
        | ((uint64_t)sp[4] << 8) | sp[5]; break;
      case 7: w = ((uint64_t)sp[0] << 48) | ((uint64_t)sp[1] << 40)
        | ((uint64_t)sp[2] << 32) | ((uint64_t)sp[3] << 24)
        | ((uint64_t)sp[4] << 16) | ((uint64_t)sp[5] << 8) | sp[6]; break;
      default: w = ((uint64_t)sp[0] << 56) | ((uint64_t)sp[1] << 48)
        | ((uint64_t)sp[2] << 40) | ((uint64_t)sp[3] << 32)
        | ((uint64_t)sp[4] << 24) | ((uint64_t)sp[5] << 16)
        | ((uint64_t)sp[6] << 8) | sp[7]; break;
    }
    w >>= sb * 8 - (src_offset & 7) - n;
    if(n < 64)
      w &= (((uint64_t)1 << n) - 1);
    /* distribution: leading partial byte, whole bytes, tail */
    dp = dst + dst_offset / CHAR_BIT;
    m = dst_offset & 7;
    rem = n;
    if(m)
    {
      unsigned int hb, chunk, mask, shift;
      hb = 8 - m;
      if(hb > rem)
        hb = rem;
      shift = 8 - m - hb;
      chunk = (unsigned int)(w >> (rem - hb));
      mask = ((1u << hb) - 1) << shift;
      *dp = (unsigned char)((*dp & ~mask) | ((chunk << shift) & mask));
      ++dp;
      rem -= hb;
    }
    while(rem >= 8)
    {
      rem -= 8;
      *dp++ = (unsigned char)((w >> rem) & 0xFF);
    }
    if(rem)
    {
      unsigned int mask, chunk;
      mask = 0xFFu << (8 - rem);
      chunk = (unsigned int)(w & (((uint64_t)1 << rem) - 1)) << (8 - rem);
      *dp = (unsigned char)((*dp & ~mask) | (chunk & mask));
    }
    return;
  }

  /* long shifted copies: leading edge via window (recursion, take <= 7 there) */
  if(dst_offset & 7)
  {
    size_t take = 8 - (dst_offset & 7);
    bitcpy(take, src_org, src_offset, dst_org, dst_offset);
    src_offset += take;
    dst_offset += take;
    n -= take;
  }

  /* middle with a dual carry loop: two independent chains */
  if(n >= 16)
  {
    const unsigned char *sp;
    unsigned char *dp;
    unsigned int acc, b0, b1;
    size_t r;
    r = src_offset & 7;
    sp = src + src_offset / CHAR_BIT;
    dp = dst + dst_offset / CHAR_BIT;
    acc = (unsigned int)*sp++ << r;
    while(n >= 16)
    {
      b0 = sp[0];
      b1 = sp[1];
      sp += 2;
      dp[0] = (unsigned char)(acc | (b0 >> (8 - r)));
      dp[1] = (unsigned char)(((unsigned int)b0 << r) | (b1 >> (8 - r)));
      acc = (unsigned int)b1 << r;
      dp += 2;
      src_offset += 16;
      dst_offset += 16;
      n -= 16;
    }
  }

  /* remainder (< 16 bits) with the same bitcpy, goes into the window */
  if(n)
    bitcpy(n, src_org, src_offset, dst_org, dst_offset);
}
