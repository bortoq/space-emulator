# Legacy end-to-end checks (pre-SYS)

These scripts drive the emulator over the old single data channel `DATA`
(one position after the space, status carried in the tail of the
half-interval). That channel was removed by the SYS rework (roadmap
section 3): transfers now go through the `SYS` service channel plus
dynamically opened data channels, using the record protocol in
`proto.def`.

The scripts are kept for reference only and are not wired into
`tests/run_tests.sh`. They will be retargeted to SYS as part of phase 3
(the in-space request/response driver).
