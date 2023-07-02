.section .rodata
.global _binary_ca_bundle_crt_start
.type   _binary_ca_bundle_crt_start, %object
.global _binary_ca_bundle_crt_end
.type   _binary_ca_bundle_crt_end, %object
.align  4

_binary_ca_bundle_crt_start:
.incbin "ca-bundle.crt"

_binary_ca_bundle_crt_end:
.previous
