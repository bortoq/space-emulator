#define _POSIX_C_SOURCE 200112L
/*
 * SYS CHANNEL AND DEVICE CHANNELS (host side). See sys_ctrl.h.
 *
 * No in-space bounds checks: the program guarantees buffer ranges, the
 * emulator trusts it (roadmap section 9). Device bytes are moved directly
 * between the host stream and the in-space buffer; a data channel carries
 * data only, SYS carries control records.
 *
 * REQ fields: op(0) chan(1) flags(2) arg(3) off(4).
 * RSP fields: op(0) status(1) flags(2) size(3) moved(4) eof(5).
 */
#include "sys_ctrl.h"
#include "proto_gen.h"
#include <errno.h>
#include <sys/stat.h>

typedef enum { CH_SYS, CH_DEV } ch_kind;

/* one channel: a bound stream plus its last answer / data-operation state */
typedef struct
{
  FILE   *f;
  int     used;
  ch_kind kind;
  size    op, status, flags, size, moved, eof;
} channel;

/* field offsets and widths, resolved from the space size at init */
static size req_off[REQ_NFIELDS], req_w[REQ_NFIELDS];
static size rsp_off[RSP_NFIELDS], rsp_w[RSP_NFIELDS];
static size req_bits, rsp_bits;
static size capacity;                 /* channel positions incl. SYS */

static channel *channels = NULL;      /* channel c sits at space_nbits + c */
static size nchan = 0;                /* positions in use (SYS included) */

/* device stream for an id from the XDEV table (0/1 are the std streams) */
static FILE *device_stream(unsigned id)
{
  if(id == 0)
    return channel_in;
  if(id == 1)
    return channel_out;
  return NULL;
}

/* resolve one field table against the space size */
static void layout(const proto_field *f, unsigned n, size *off, size *w)
{
  size at = 0;
  unsigned i;
  for(i = 0; i < n; ++i)
  {
    size bits;
    switch(f[i].kind)
    {
      case PK_ADDR: bits = address_bits; break;
      case PK_NF:   bits = nfield_bits;  break;
      default:      bits = (size)f[i].bits; break;
    }
    off[i] = at;
    w[i] = bits;
    at = (size)(at + bits);
  }
}

/* grow the channel table up to the position capacity */
static void need_channel(size idx)
{
  if(idx < nchan)
    return;
  capacity = 1;                       /* recomputed below */
  if(address_bits < (size)(sizeof(size) * CHAR_BIT))
    capacity = ((size)1 << address_bits) - (size)space_nbits;
  if(idx >= capacity)
    return;                           /* out of address room: ignore */
  {
    channel *grown = realloc(channels, (size_t)(idx + 1) * sizeof *channels);
    size i;
    if(grown == NULL)
    {
      fprintf(stderr, "space: out of memory\n");
      exit(EXIT_MEMORY);
    }
    channels = grown;
    for(i = nchan; i <= idx; ++i)
    {
      channels[i].f = NULL;
      channels[i].used = 0;
      channels[i].kind = CH_DEV;
      channels[i].op = (size)OP_NONE;
      channels[i].status = 0;
      channels[i].flags = 0;
      channels[i].size = 0;
      channels[i].moved = 0;
      channels[i].eof = 0;
    }
    nchan = (size)(idx + 1);
  }
}

/* channel index for a position, or -1 when none */
static int channel_index(size pos)
{
  size idx;
  if(pos < (size)space_nbits)
    return -1;
  idx = (size)(pos - (size)space_nbits);
  if(idx >= nchan || !channels[idx].used)
    return -1;
  return (int)idx;
}

int sys_is_channel(size pos)
{
  return channel_index(pos) >= 0;
}

/* channel at a position, or NULL */
static channel *chan_at(size pos)
{
  int idx = channel_index(pos);
  return idx < 0 ? NULL : &channels[idx];
}

/* channel position of a pre-bound device id, 0 when there is none */
size sys_device_pos(unsigned id)
{
  unsigned i;
  for(i = 0; i < PROTO_NDEVS; ++i)
    if(proto_devs[i].id == id)
    {
      size pos = (size)((size)space_nbits + 1 + i);
      if(sys_is_channel(pos))
        return pos;
    }
  return 0;
}

size sys_req_fields(void) { return (size)REQ_NFIELDS; }
size sys_rsp_fields(void) { return (size)RSP_NFIELDS; }

void sys_encode_req(size base, const size *v)
{
  unsigned i;
  for(i = 0; i < REQ_NFIELDS; ++i)
    space_put(req_w[i], v[i], (size)(base + req_off[i]));
}

void sys_decode_rsp(size base, size *v)
{
  unsigned i;
  for(i = 0; i < RSP_NFIELDS; ++i)
    v[i] = space_get((size)(base + rsp_off[i]), rsp_w[i]);
}

size sys_req_bits(void) { return req_bits; }
size sys_rsp_bits(void) { return rsp_bits; }

/* decode a request record sitting at bit address base */
static void decode_req(size base, size *v)
{
  unsigned i;
  for(i = 0; i < REQ_NFIELDS; ++i)
    v[i] = space_get((size)(base + req_off[i]), req_w[i]);
}

static void rsp_reset(channel *c)
{
  c->op = (size)OP_NONE;
  c->status = 0;
  c->flags = 0;
  c->size = 0;
  c->moved = 0;
  c->eof = 0;
}

/* encode the stored answer of channel c at bit address base */
static void encode_rsp(size base, const channel *c)
{
  size v[RSP_NFIELDS];
  unsigned i;
  v[0] = c->op; v[1] = c->status; v[2] = c->flags;
  v[3] = c->size; v[4] = c->moved; v[5] = c->eof;
  for(i = 0; i < RSP_NFIELDS; ++i)
    space_put(rsp_w[i], v[i], (size)(base + rsp_off[i]));
}

/* copy up to len bytes from the stream into space[ptr..] */
static void do_read(channel *c, size ptr, size len)
{
  unsigned char buf[32768];
  size moved = 0;
  c->status = 0;
  c->eof = 0;
  if(c->f == NULL)
  {
    c->status = (size)EBADF;
    c->moved = 0;
    return;
  }
  clearerr(c->f);
  while(moved < len)
  {
    size want = (size)(len - moved);
    size_t got;
    if(want > (size)sizeof buf)
      want = (size)sizeof buf;
    errno = 0;                        /* status must reflect this call */
    got = fread(buf, 1, (size_t)want, c->f);
    if(got)
      bitcpy((size)got * CHAR_BIT, buf, 0, space, (size_t)(ptr + moved * CHAR_BIT));
    moved = (size)(moved + got);
    if(got < (size_t)want)
    {
      if(ferror(c->f))
        c->status = errno ? (size)errno : (size)ERR_IO;
      if(feof(c->f))
        c->eof = 1;
      break;
    }
  }
  c->moved = moved;
}

/* copy up to len bytes from space[ptr..] into the stream */
static void do_write(channel *c, size ptr, size len)
{
  unsigned char buf[32768];
  size moved = 0;
  c->status = 0;
  if(c->f == NULL)
  {
    c->status = (size)EBADF;
    c->moved = 0;
    return;
  }
  clearerr(c->f);
  while(moved < len)
  {
    size want = (size)(len - moved);
    size_t got;
    if(want > (size)sizeof buf)
      want = (size)sizeof buf;
    bitcpy(want * CHAR_BIT, space, (size_t)(ptr + moved * CHAR_BIT), buf, 0);
    errno = 0;                        /* status must reflect this call */
    got = fwrite(buf, 1, (size_t)want, c->f);
    moved = (size)(moved + got);
    if(got < (size_t)want)
    {
      if(ferror(c->f))
        c->status = errno ? (size)errno : (size)ERR_IO;
      break;
    }
  }
  c->moved = moved;
}

/*
 * FSTAT returns the channel's size together with the state of its last
 * data operation (status/moved/eof). On success it only updates size, so
 * a preceding read/write error stays visible; an fstat failure sets the
 * status itself.
 */
static void do_fstat(channel *c)
{
  struct stat st;
  if(c->f == NULL)
  {
    c->status = (size)EBADF;
    c->size = 0;
    return;
  }
  errno = 0;
  if(fstat(fileno(c->f), &st) != 0)
  {
    c->status = (size)errno;
    c->size = 0;
    return;
  }
  c->size = (size)st.st_size;
}

/*
 * serve one control request decoded into v (REQ_NFIELDS values). cidx is
 * the SYS entry; the answer is stored there. CLOSE may close SYS itself.
 */
static void service(size cidx, size *v)
{
  size op = v[0];
  size chan = v[1];
  rsp_reset(&channels[cidx]);
  channels[cidx].op = op;
  switch(op)
  {
    case OP_NONE:
      break;
    case OP_CLOSE:
    {
      int idx = channel_index(chan);
      if(idx < 0)
        channels[cidx].status = (size)EBADF;
      else
      {
        channels[idx].f = NULL;
        channels[idx].used = 0;
        channels[idx].kind = CH_DEV;
      }
      break;
    }
    case OP_SEEK:
    {
      channel *t = chan_at(chan);
      if(t == NULL || t->f == NULL)
        channels[cidx].status = (size)EBADF;
      else if(fseek(t->f, (long)v[4], (int)v[3]) != 0)
        channels[cidx].status = (size)errno;
      break;
    }
    case OP_FSTAT:
    {
      channel *t = chan_at(chan);
      if(t == NULL)
      {
        channels[cidx].status = (size)EBADF;
        break;
      }
      do_fstat(t);
      channels[cidx].size = t->size;
      channels[cidx].moved = t->moved;
      channels[cidx].eof = t->eof;
      channels[cidx].status = t->status;
      break;
    }
    case OP_IOCTL:
      channels[cidx].status = (size)ENOTTY;
      break;
    default:
      channels[cidx].status = (size)EINVAL;
      break;
  }
}

/* bytes moved by a data copy of n bits (whole bytes; program aligns) */
static size data_bytes(size n)
{
  return (size)((n + (CHAR_BIT - 1)) / CHAR_BIT);
}

/* service the channel instructions currently in the processor */
void sys_tick(void)
{
  size i;
  if(nchan == 0)
    return;
  /* requests and data first (dst is a channel), answers next (src = SYS) */
  for(i = 0; i < (size)proc_n; ++i)
  {
    int didx, sidx;
    if(processor[i][0] == 0)
      continue;
    didx = channel_index(processor[i][1]);
    sidx = channel_index(processor[i][2]);
    if(didx == 0)                       /* dst = SYS: control request */
    {
      size v[REQ_NFIELDS];
      decode_req(processor[i][2], v);
      service(0, v);
    }
    else if(didx > 0)                   /* dst = device: write data out */
      do_write(&channels[didx], processor[i][2],
               data_bytes(processor[i][0]));
    if(sidx > 0)                        /* src = device: read data in */
      do_read(&channels[sidx], processor[i][1],
              data_bytes(processor[i][0]));
  }
  for(i = 0; i < (size)proc_n; ++i)
  {
    int sidx;
    if(processor[i][0] == 0)
      continue;
    sidx = channel_index(processor[i][2]);
    if(sidx == 0)                       /* src = SYS: read the answer */
      encode_rsp(processor[i][1], &channels[0]);
  }
}

void sys_init(void)
{
  unsigned i;
  layout(proto_req, REQ_NFIELDS, req_off, req_w);
  layout(proto_rsp, RSP_NFIELDS, rsp_off, rsp_w);
  {
    unsigned j;
    req_bits = 0;
    for(j = 0; j < REQ_NFIELDS; ++j)
      req_bits = (size)(req_bits + req_w[j]);
    rsp_bits = 0;
    for(j = 0; j < RSP_NFIELDS; ++j)
      rsp_bits = (size)(rsp_bits + rsp_w[j]);
  }
  nchan = 0;
  need_channel(0);                    /* SYS is always present */
  if(nchan == 0)
  {
    fprintf(stderr, "space: no room for the SYS channel (-s)\n");
    exit(EXIT_USAGE);
  }
  channels[0].f = NULL;
  channels[0].used = 1;               /* SYS: present, control, no device */
  channels[0].kind = CH_SYS;
  rsp_reset(&channels[0]);
  /* pre-bind the device channels from the XDEV table, in table order */
  for(i = 0; i < PROTO_NDEVS; ++i)
  {
    size idx = (size)(1 + i);
    FILE *f = device_stream(proto_devs[i].id);
    need_channel(idx);
    if(idx >= nchan)
    {
      fprintf(stderr, "space: no room for a device channel (-s)\n");
      exit(EXIT_USAGE);
    }
    channels[idx].f = f;
    channels[idx].used = (f != NULL);
    channels[idx].kind = CH_DEV;
    rsp_reset(&channels[idx]);
  }
}
