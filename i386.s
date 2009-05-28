.globl forth_dup
.globl _drop
.globl _fetch
.globl _store

forth_dup:
	leal -4(%esi),%esi
	movl %eax,(%esi)
	ret

_drop:	lodsl
	ret

_fetch:	movl (%eax),%eax
	ret

_store: movl %eax,%edx
	lodsl
	movl %eax,(%edx)
	lodsl
	ret

