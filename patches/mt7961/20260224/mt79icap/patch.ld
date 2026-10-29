MEMORY
{
	INCLUDE gen/memory.ld
}

SECTIONS
{
	INCLUDE gen/nexmon.ld

	/* patches/common/wrapper.c declares firmware functions for several chips.
	 * Only those carrying an at() for THIS chip+fw pair get a placement rule
	 * in gen/nexmon.ld (above, so they win); the rest are empty dummy bodies
	 * with no address at all.  Drop them, otherwise ld stops with
	 * "no memory region specified for loadable section .text.<name>". */
	/DISCARD/ : { obj/wrapper.o(.text .text.*) }
}
