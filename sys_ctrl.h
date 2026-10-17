/*
 * SYS CHANNEL AND DEVICE CHANNELS (host side)
 *
 * The channel table holds SYS at entry 0 (position space_nbits) and the
 * device channels pre-bound at start after it (stdin at space_nbits+1,
 * stdout at space_nbits+2, further inherited descriptors next). There is
 * no dynamic opening.
 *
 * A copy that touches a channel is served here (in sys_tick):
 *   - a copy touching SYS carries a control record: dst = SYS is a
 *     request (src holds its address), src = SYS is an answer read
 *     (dst holds its address);
 *   - a copy touching a device channel moves data: dst = channel writes
 *     data out, src = channel reads data in.
 * SYS is an ordinary entry and may be closed like any other channel;
 * after that the space has no control over the host and cannot read any
 * state. Requests are served before answers in the tick, so a control
 * request and its answer read in one tick both work regardless of order.
 *
 * The protocol (op codes, record fields, pre-bound devices) comes from
 * proto.def via proto_gen.h.
 */
#ifndef SYS_CTRL_H_
#define SYS_CTRL_H_

#include "space.h"

void sys_init(void);                 /* compute widths, bind SYS + devices */
int  sys_is_channel(size pos);       /* pos names an existing channel */
size sys_device_pos(unsigned id);    /* channel position of a pre-bound device */
void sys_tick(void);                 /* service channel instructions */
size sys_req_bits(void);             /* bits in a request record */
size sys_rsp_bits(void);             /* bits in an answer record */
/* encode/decode a record at a bit address (host side, tests and tools) */
void sys_encode_req(size base, const size *v);  /* v[REQ_NFIELDS] */
void sys_decode_rsp(size base, size *v);        /* v[RSP_NFIELDS] */
size sys_req_fields(void);
size sys_rsp_fields(void);

#endif /* SYS_CTRL_H_ */
