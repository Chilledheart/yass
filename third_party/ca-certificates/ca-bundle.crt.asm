%ifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
global __binary_ca_bundle_crt_start
global __binary_ca_bundle_crt_end
section .rdata

__binary_ca_bundle_crt_start:
ALIGN 4
incbin "ca-bundle.crt"
__binary_ca_bundle_crt_end:
%else
global _binary_ca_bundle_crt_start
global _binary_ca_bundle_crt_end
section .rdata

_binary_ca_bundle_crt_start:
ALIGN 4
incbin "ca-bundle.crt"
_binary_ca_bundle_crt_end:
%endif
