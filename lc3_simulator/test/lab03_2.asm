;Binary method

.ORIG x3000
	;R0, R1, R2
WH0	NOT R2, R1
	ADD R2, R2, #1
	ADD R3, R0, R2
	BRzp WH1
SWITCH	ADD R5, R0, #0
	ADD R0, R1, #0
	ADD R1, R5, #0
	NOT R2, R1
	ADD R2, R2, #1
	ADD R3, R0, R2

WH1	BRz FIN
	ADD R0, R3, #0
	ADD R2, R2, R2
	BRp SWITCH
	ADD R3, R0, R2
	BRzp WH1
	BRn WH0

FIN	ADD R0, R1, #0
	TRAP x25
.END