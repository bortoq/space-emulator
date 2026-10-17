/* generated from proto.def by tools/proto_gen.py; do not edit */
#ifndef PROTO_GEN_H_
#define PROTO_GEN_H_

enum {
  OP_NONE = 0,
  OP_CLOSE = 1,
  OP_SEEK = 2,
  OP_FSTAT = 3,
  OP_IOCTL = 4,
};

typedef enum { PK_LIT, PK_ADDR, PK_NF } proto_width_kind;
typedef struct { const char *name; unsigned bits; proto_width_kind kind; } proto_field;

static const proto_field proto_req[] = {
  {"op", 8, PK_LIT},
  {"chan", 0, PK_ADDR},
  {"flags", 16, PK_LIT},
  {"arg", 32, PK_LIT},
  {"off", 64, PK_LIT},
};
#define REQ_NFIELDS 5

static const proto_field proto_rsp[] = {
  {"op", 8, PK_LIT},
  {"status", 8, PK_LIT},
  {"flags", 16, PK_LIT},
  {"size", 64, PK_LIT},
  {"moved", 0, PK_NF},
  {"eof", 1, PK_LIT},
};
#define RSP_NFIELDS 6

typedef struct { unsigned id; const char *name; const char *mode; } proto_dev;
static const proto_dev proto_devs[] = {
  {0, "stdin", "r"},
  {1, "stdout", "w"},
};
#define PROTO_NDEVS 2

#endif /* PROTO_GEN_H_ */
