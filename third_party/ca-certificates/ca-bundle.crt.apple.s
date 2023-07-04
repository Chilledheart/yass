.section __DATA_CONST,__const
.private_extern __binary_ca_bundle_crt_start
.global __binary_ca_bundle_crt_start
.private_extern __binary_ca_bundle_crt_end
.global __binary_ca_bundle_crt_end
.p2align  6

__binary_ca_bundle_crt_start:
.incbin "ca-bundle.crt"

__binary_ca_bundle_crt_end:
.previous
