;A routine for keyboard interrupt

.ORIG x1100
	STI R0, A
	LDI R0, KBDR
	STI R0, DDR
AGAIN	LDI R0, DSR
	BRzp AGAIN
	LDI R0, A
	RTI

KBDR 	.FILL xFE02
DSR	.FILL xFE04
DDR	.FILL xFE06
A	.BLKW #1
.END