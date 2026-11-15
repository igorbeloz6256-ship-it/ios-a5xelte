/*
 * The one entry of every method herovibe.c watches: the argument registers saved, herovibe_note(self, _cmd, arg2,
 * arg3) told of the call and asked for the method's own implementation, everything put back, and on to it. The
 * method's arguments and result never pass through C, so its signature does not matter here.
 */
	.text
	.p2align 2
	.globl	_herovibe_tramp
_herovibe_tramp:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	sub	sp, sp, #208
	stp	x0, x1, [sp, #0]
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	str	x8, [sp, #64]
	stp	q0, q1, [sp, #80]
	stp	q2, q3, [sp, #112]
	stp	q4, q5, [sp, #144]
	stp	q6, q7, [sp, #176]
	bl	_herovibe_note
	mov	x16, x0
	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	ldr	x8, [sp, #64]
	ldp	q0, q1, [sp, #80]
	ldp	q2, q3, [sp, #112]
	ldp	q4, q5, [sp, #144]
	ldp	q6, q7, [sp, #176]
	add	sp, sp, #208
	ldp	x29, x30, [sp], #16
	br	x16
