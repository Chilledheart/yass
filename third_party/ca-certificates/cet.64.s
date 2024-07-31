# Always generate .note.gnu.property section for ELF outputs to
# mark Intel CET support since all input files must be marked
# with Intel CET support in order for linker to mark output with
# Intel CET support.
.section ".note.gnu.property", "a"
.p2align 3
.long 1f - 0f
.long 4f - 1f
.long 5
0:
.asciz "GNU"
1:
.p2align 3
.long 0xc0000002
.long 3f - 2f
2:
.long 3
3:
.p2align 3
4:
