.section .rdata
.global _binary_supplementary_ca_bundle_crt_start
.global _binary_supplementary_ca_bundle_crt_end
.align  4

_binary_supplementary_ca_bundle_crt_start:
.incbin "supplementary-ca-bundle.crt"

_binary_supplementary_ca_bundle_crt_end:
