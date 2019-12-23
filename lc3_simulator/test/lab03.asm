;Binary-Dec Method
.ORIG x3000
WH0	LEA R2, BPOWER
	NOT R4, R1
	ADD R4, R4, #1
	ADD R5, R0, R4
	BRn SWITCH
	BRz FIN
	ADD R0, R5, #0
	ADD R3, R4, #0
	
WH1	ADD R3, R3, R3
	BRp SWITCH
	ADD R5, R0, R3
	BRnz WH2
	ADD R0, R5, #0		; a = t
	ADD R2, R2, #1		;
	STR R3, R2, #0		; b_power[++i] = b_power[i]*2
	BRnzp WH1

WH2	BRz FIN
	LDR R3, R2, #0
	BRz ENDWH2
	ADD R2, R2, #-1
	ADD R5, R0, R3
	BRnz WH2
	ADD R0, R5, #0
	BRnzp WH2

ENDWH2	ADD R5, R0, R4
	BRz FIN
	BRn SWITCH
	ADD R0, R1, #0
	ADD R1, R5, #0
	BRnzp WH0

SWITCH	ADD R5, R0, #0
	ADD R0, R1, #0
	ADD R1, R5, #0
	BRnzp WH0

FIN	ADD R0, R1, #0
	TRAP x25
BPOWER	.FILL #0	
 	.BLKW #15
.END