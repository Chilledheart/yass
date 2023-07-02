.section .rdata
.global _binary_ca_bundle_crt_start
.global _binary_ca_bundle_crt_end
.align  4

_binary_ca_bundle_crt_start:
.incbin "ca-bundle.crt"

_binary_ca_bundle_crt_end:
