/*
 * Unit tests for the channel frame (SYS + pre-bound device channels).
 *
 * Exercises sys_ctrl directly: data copies through device channels, SEEK/
 * FSTAT/IOCTL control through SYS, CLOSE (of a device channel and of SYS
 * itself -> isolation), the answer available in the tick of the request,
 * and the seed load.
 */
#include "../space.h"
#include "../sys_ctrl.h"
#include "../proto_gen.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if(!(cond)) { \
  fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while(0)

/* device bytes live in space by the same convention bitcpy uses: the
 * first bit of a byte is its high bit (see bitcopy.c) */
static void fill_bytes(size addr, const char *s, size n)
{
  size i;
  for(i = 0; i < n; ++i)
    bitcpy(CHAR_BIT, s + i, 0, space, (size_t)(addr + i * CHAR_BIT));
}

static void read_bytes(size addr, char *s, size n)
{
  bitcpy((size_t)(n * CHAR_BIT), space, (size_t)addr, s, 0);
}

static void clear_proc(void)
{
  size_t i;
  for(i = 0; i < proc_n; ++i)
    processor[i][0] = processor[i][1] = processor[i][2] = 0;
}

static size SYS_POS(void) { return (size)space_nbits; }
static size IN_POS(void)  { return (size)space_nbits + 1; }
static size OUT_POS(void) { return (size)space_nbits + 2; }

/* send one control request through SYS; answer decoded into rsp */
static void send(size req_addr, size rsp_addr, const size *req, size *rsp)
{
  size v[REQ_NFIELDS];
  unsigned i;
  clear_proc();
  for(i = 0; i < REQ_NFIELDS; ++i)
    v[i] = req[i];
  sys_encode_req(req_addr, v);
  processor[0][0] = sys_req_bits();
  processor[0][1] = SYS_POS();          /* dst = SYS: request */
  processor[0][2] = req_addr;
  processor[1][0] = sys_rsp_bits();
  processor[1][1] = rsp_addr;           /* dst = rsp buffer */
  processor[1][2] = SYS_POS();          /* src = SYS: answer read */
  sys_tick();
  sys_decode_rsp(rsp_addr, rsp);
}

/* --- error paths: errno propagation and FSTAT state ------------------- */

/*
 * FSTAT must return the size together with the state of the channel's
 * last data operation. The status of a failed read/write has to survive
 * the fstat itself, and sys_device_pos must name the seeded channels.
 */
static void test_errors(void)
{
  char *argv[] = { "space", "-s", "65520", "-p", "8" };
  FILE *full = fopen("/dev/full", "w");   /* writes fail with ENOSPC */
  FILE *dir = fopen(".", "r");            /* reads fail with EISDIR */
  size req_addr = 1000, rsp_addr = 2000, data = 9000;
  size req[REQ_NFIELDS], rsp[RSP_NFIELDS];

  if(full != NULL)
    setvbuf(full, NULL, _IONBF, 0);
  channel_in = dir;                       /* read error source */
  channel_out = full;                     /* write error sink */
  parse_config(5, argv);
  init_space();

  /* device positions come from the XDEV table in order */
  CHECK(sys_device_pos(0) == IN_POS());
  CHECK(sys_device_pos(1) == OUT_POS());
  CHECK(sys_device_pos(2) == 0);

  /* write error (ENOSPC): status visible through FSTAT afterwards */
  if(full != NULL)
  {
    fill_bytes(data, "X", 1);
    clear_proc();
    processor[0][0] = 8;
    processor[0][1] = OUT_POS();
    processor[0][2] = data;
    sys_tick();
    memset(req, 0, sizeof req);
    req[0] = OP_FSTAT; req[1] = OUT_POS();
    send(req_addr, rsp_addr, req, rsp);
    CHECK(rsp[1] == (size)ENOSPC);
    CHECK(rsp[4] == 0);                   /* nothing moved */
  }

  /* read error (EISDIR): same status path for input */
  if(dir != NULL)
  {
    clear_proc();
    processor[0][0] = 8;
    processor[0][1] = data;
    processor[0][2] = IN_POS();
    sys_tick();
    memset(req, 0, sizeof req);
    req[0] = OP_FSTAT; req[1] = IN_POS();
    send(req_addr, rsp_addr, req, rsp);
    CHECK(rsp[1] == (size)EISDIR);
  }

  /* CLOSE of a position that is not a channel -> EBADF */
  memset(req, 0, sizeof req);
  req[0] = OP_CLOSE; req[1] = (size)space_nbits - 1;
  send(req_addr, rsp_addr, req, rsp);
  CHECK(rsp[0] == OP_CLOSE);
  CHECK(rsp[1] == (size)EBADF);

  if(full != NULL) fclose(full);
  if(dir != NULL) fclose(dir);
  free(space);
  free(processor);
}

/* --- boot path ------------------------------------------------------- */

typedef struct { FILE *f; unsigned char byte; int nbit; } bitwriter;

static void bw_put(bitwriter *w, size v, size n)
{
  size k;
  for(k = 0; k < n; ++k)
  {
    w->byte = (unsigned char)((w->byte << 1) | (unsigned char)((v >> k) & 1));
    if(++w->nbit == 8) { fputc(w->byte, w->f); w->byte = 0; w->nbit = 0; }
  }
}

static void bw_flush(bitwriter *w)
{
  if(w->nbit) { w->byte <<= (8 - w->nbit); fputc(w->byte, w->f); }
  w->byte = 0; w->nbit = 0;
}

/* a program of proc_n instructions: instruction 1 copies 7 bits from
 * 54321 to 12345, the rest are NOPs */
static void write_boot_program(FILE *f)
{
  bitwriter w = { f, 0, 0 };
  size i, z = nfield_bits, a = address_bits;
  for(i = 0; i < (size)proc_n; ++i)
  {
    if(i == 1) { bw_put(&w, 7, z); bw_put(&w, 12345, a); bw_put(&w, 54321, a); }
    else       { bw_put(&w, 0, z); bw_put(&w, 0, a);    bw_put(&w, 0, a); }
  }
  bw_flush(&w);
  fflush(f);
}

static void test_boot(void)
{
  char *argv[] = { "space", "-s", "65520", "-p", "8" };
  FILE *prog = tmpfile();
  FILE *out = tmpfile();
  size isz, z, a;

  parse_config(5, argv);
  channel_in = prog;
  channel_out = out;
  init_space();                       /* binds proc as the stdin channel */
  isz = instruction_size; z = nfield_bits; a = address_bits;

  write_boot_program(prog);
  rewind(prog);

  /* tick 1: the seed copy loads the program from the stdin channel */
  CHECK(computation() == 1);
  CHECK(space_get(isz + 0, z) == 7);
  CHECK(space_get(isz + z, a) == 12345);
  CHECK(space_get(isz + z + a, a) == 54321);
  CHECK(space_get(isz + 0, z) != 0);
  /* tick 2: instruction 1 runs the copy */
  space_put(7, 85, 54321);
  CHECK(computation() == 1);
  CHECK(space_get(12345, 7) == 85);
  fclose(prog);
  fclose(out);
  free(space);
  free(processor);
}

int main(void)
{
  char *argv[] = { "space", "-s", "65520", "-p", "8" };
  FILE *out = tmpfile();
  FILE *in = tmpfile();
  size req_addr = 1000, rsp_addr = 2000, buf = 8000, data = 9000;
  size req[REQ_NFIELDS], rsp[RSP_NFIELDS];
  unsigned char got[16];

  channel_in = in;
  channel_out = out;
  parse_config(5, argv);
  init_space();

  /* --- channels present at start --- */
  CHECK(sys_is_channel(SYS_POS()));
  CHECK(sys_is_channel(IN_POS()));
  CHECK(sys_is_channel(OUT_POS()));
  CHECK(!sys_is_channel((size)space_nbits - 1));
  CHECK(!sys_is_channel(OUT_POS() + 1));
  CHECK(sys_req_bits() > 0 && sys_rsp_bits() > 0);

  /* --- data write: copy 3 bytes from space to the stdout channel --- */
  fill_bytes(data, "ABC", 3);
  clear_proc();
  processor[0][0] = 3 * CHAR_BIT;
  processor[0][1] = OUT_POS();        /* dst = channel: write out */
  processor[0][2] = data;
  sys_tick();
  fflush(out);
  rewind(out);
  CHECK(fread(got, 1, 16, out) == 3);
  CHECK(memcmp(got, "ABC", 3) == 0);

  /* --- data read: copy 4 bytes from the stdin channel into space --- */
  fwrite("WXYZ", 1, 4, in);
  fflush(in);
  rewind(in);
  clear_proc();
  processor[0][0] = 4 * CHAR_BIT;
  processor[0][1] = buf;              /* dst = space buffer */
  processor[0][2] = IN_POS();         /* src = channel: read in */
  sys_tick();
  read_bytes(buf, (char *)got, 4);
  CHECK(memcmp(got, "WXYZ", 4) == 0);

  /* --- FSTAT stdin: size 4, moved 4 (state via SYS) --- */
  memset(req, 0, sizeof req);
  req[0] = OP_FSTAT;
  req[1] = IN_POS();
  send(req_addr, rsp_addr, req, rsp);
  CHECK(rsp[0] == OP_FSTAT);
  CHECK(rsp[1] == 0);                 /* status ok */
  CHECK(rsp[3] == 4);                 /* size */
  CHECK(rsp[4] == 4);                 /* moved of the last read */
  CHECK(rsp[5] == 0);                 /* eof not set */

  /* --- SEEK stdout to offset 0 --- */
  memset(req, 0, sizeof req);
  req[0] = OP_SEEK;
  req[1] = OUT_POS();
  req[3] = 0;                         /* whence SEEK_SET */
  req[4] = 0;                         /* offset */
  send(req_addr, rsp_addr, req, rsp);
  CHECK(rsp[0] == OP_SEEK);
  CHECK(rsp[1] == 0);

  /* --- unknown op -> EINVAL --- */
  memset(req, 0, sizeof req);
  req[0] = 999;
  send(req_addr, rsp_addr, req, rsp);
  CHECK(rsp[1] != 0);

  /* --- CLOSE stdout: position stops being a channel, data copy is a noop --- */
  memset(req, 0, sizeof req);
  req[0] = OP_CLOSE;
  req[1] = OUT_POS();
  send(req_addr, rsp_addr, req, rsp);
  CHECK(rsp[0] == OP_CLOSE);
  CHECK(rsp[1] == 0);
  CHECK(!sys_is_channel(OUT_POS()));

  /* --- CLOSE SYS: isolation (no control, no status possible) --- */
  memset(req, 0, sizeof req);
  req[0] = OP_CLOSE;
  req[1] = SYS_POS();
  clear_proc();
  sys_encode_req(req_addr, (size[REQ_NFIELDS]){OP_CLOSE, SYS_POS(), 0, 0, 0});
  processor[0][0] = sys_req_bits();
  processor[0][1] = SYS_POS();
  processor[0][2] = req_addr;
  sys_tick();
  CHECK(!sys_is_channel(SYS_POS()));

  /* --- after isolation, a SYS request is silently ignored --- */
  clear_proc();
  processor[0][0] = sys_req_bits();
  processor[0][1] = SYS_POS();
  processor[0][2] = req_addr;
  sys_tick();                         /* must not crash, no channel served */
  CHECK(!sys_is_channel(SYS_POS()));

  fclose(out);
  fclose(in);
  free(space);
  free(processor);

  test_errors();
  test_boot();

  if(failures) { fprintf(stderr, "%d FAILURES\n", failures); return 1; }
  printf("sys: all unit checks passed\n");
  return 0;
}
