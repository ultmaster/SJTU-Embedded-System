		DOSSEG
		.MODEL	SMALL
		.8086

	.stack 100h


;
; 8253芯片端口地址 （Port Address):
L8253T0			EQU	100H			; Timer0's port number in I/O space
L8253T1			EQU 	102H			; Timer1's port number in I/O space
L8253T2			EQU 	104H			; Timer2's port number in I/O space
L8253CS			EQU 	106H			; 8253 Control Register's port number in I/O space
;
; 8255芯片端口地址 （Port Address):
L8255PA			EQU	121H			; Port A's port number in I/O space
L8255PB			EQU 	123H			; Port B's port number in I/O space
L8255PC			EQU 	125H			; Port C's port number in I/O space
L8255CS			EQU 	127H			; 8255 Control Register's port number in I/O space
;
;  中断矢量号定义
IRQNum			EQU	2EH			; 中断矢量号,要根据学号计算得到后更新此定义。

Patch_Protues	EQU	IN AL, 0	;	Simulation Patch for Proteus, please ignore this line

; 修补Proteus仿真的BUG，参见程序段中的使用说明
    WaitForHWInt MACRO INTNum		; INTNum is the HW INT number
		MOV AL, INTNum   			;
		OUT 0,AL					;
		STI
		CLI
    ENDM


		.data

DelayShort	dw	10000
DelayLong	dw	4000
                DB 0
		
        ORG 100h
; SEGTAB is the code for displaying "0-F", and some of the codes may not be correct. Find and correct the errors.
	SEGTAB  DB 3FH	; 7-Segment Tube
		DB 06H	;
		DB 5BH	;            a a a
		DB 4FH	;         f         b
		DB 66H	;         f         b
		DB 6DH	;         f         b
		DB 7DH	;            g g g 
		DB 07H	;         e         c
		DB 7FH	;         e         c
		DB 6FH	;         e         c
        	DB 77H	;            d d d     h h h
		DB 7CH	; ----------------------------------
		DB 39H	;       b7 b6 b5 b4 b3 b2 b1 b0
		DB 5EH	;       DP  g  f  e  d  c  b  a
		DB 79H	;
		DB 71H	;

		
		.code						; Code segment definition
		.startup					; Entrance of this program
;------------------------------------------------------------------------
		Patch_Protues					; Simulation Patch for Proteus,
								; Please ignore the above code line.
;------------------------------------------------------------------------

START:								; Modify the following codes accordingly
								; 
		CLI						; Disable interrupts
		MOV AX, @DATA					;
		MOV DS, AX					; Initialize DS

		CALL INIT8255					; Initialize 8255 
		CALL INIT8253					; Initialize 8253
		
		MOV  BL, IRQNum					; BL is used as a parameter to call the procedure INT_INIT
		CALL INT_INIT					; Procedure INT_INIT is used to set up the IVT
		
		;INT IRQNum
		
Display_Again:
		;CALL DISPLAY8255				; Procedure DISPLAY8255 is used to contrl 7-segment tubes
		;CALL POLL_PC0
		CALL DELAY

;===================================================================================
; Attention:
; The following codes is a Patching for Proteus 8086 Hardware Simulation Bug.
; Use these codes in the case you want the 8086 to halt and waiting for HW INT only! 
; You can treat it as if it doesn't exist. 
;
; If you need to use HW INT, please uncomment it, or
; Don't modify it, leave it here just as it is.
;		WaitForHWInt IRQNum				
;====================================================================================
		JMP	Display_Again

		HLT						; 
;=====================================================================================



; INIT 8255
INIT8255 PROC
   ; Init 8255 in Mode x,	L8255PA xPUT, L8255PB xPUT, L8255PCU xPUT, L8255CS xPUT
   MOV AL, 10000001B   ; PORTC LOW READ (TOO)
   MOV DX, L8255CS
   OUT DX, AL

   RET
INIT8255 ENDP


; INIT 8253
INIT8253 PROC

; Set the mode and the initial count for Timer0
   MOV AL, 00110110B
   MOV DX, L8253CS
   OUT DX, AL

   MOV DX, L8253T0
   MOV AL, 10H
   OUT DX, AL
   MOV AL, 27H
   OUT DX, AL
   
; Set the mode and the initial count for Timer1
   MOV AL, 01010110B
   MOV DX, L8253CS
   OUT DX, AL
   
   MOV DX, L8253T1
   MOV AL, 64H
   OUT DX, AL

; Set the mode and the initial count for Timer2


   RET
INIT8253 ENDP


; DISPLAY  STUDENTS ID
DISPLAY8255 PROC
   MOV BH, 0
   MOV BL, 0
   CALL DISPLAY_NUMBER
   MOV BH, 1
   MOV BL, 0
   CALL DISPLAY_NUMBER
   MOV BH, 2
   MOV BL, 1
   CALL DISPLAY_NUMBER
   MOV BH, 3
   MOV BL, 4
   CALL DISPLAY_NUMBER

   RET
DISPLAY8255 ENDP

; DISPLAY_NUMBER [BH] ON [BL]
DISPLAY_NUMBER PROC
   MOV CL, BH
   MOV AL, 1
   SHL AL, CL
   NOT AL
   MOV DX, L8255PA
   OUT DX, AL

   MOV AL, BL
   MOV BX, OFFSET SEGTAB
   XLAT
   MOV DX, L8255PB
   OUT DX, AL

   CALL DELAY

   RET
DISPLAY_NUMBER ENDP

; Poll PC0 and put in on PC6
POLL_PC0 PROC
   MOV DX, L8255PC
   IN AL, DX
   MOV CL, 6
   SHL AL, CL
   OUT DX, AL
POLL_PC0 ENDP


; Function：DELAY FUNCTION 
DELAY 	PROC
    	PUSH CX
    	MOV CX, DelayShort
D1: 	LOOP D1
    	POP CX
    	RET
DELAY 	ENDP

;-------------------------------------------------------------
;                                                             |                                                            |
; Function：INTERRUPT Vector Table INIT						  |
; Input: BL = Interrupt number								  |
; Output: None			                                	  |
;                                                             |
;-------------------------------------------------------------	
INT_INIT	PROC FAR			; The code is not complete and you should finalize the procedure
		CLI				; Disable interrupt
		MOV AX, 0
		MOV ES, AX			; To set up the interrupt vector table
; Put your code here
; Hint: you can use the directives such as SEGMENT,OFFSET to get the segment value and the offset of a label

   MOV BH, 0
   MOV CL, 2
   SHL BX, CL
   MOV WORD PTR ES:[BX], OFFSET MYIRQ
   MOV WORD PTR ES:[BX + 2], SEG MYIRQ
   STI

   RET				; Do not to forget to return back from a procedure		
INT_INIT	ENDP

		
;--------------------------------------------------------------
;                                                             |                                                            |
; FUNCTION: INTERRUPT SERVICE  Routine （ISR）	              | 
; Input::                                                     |
; Output:                                                     |
;                                                             |
;--------------------------------------------------------------	

MYIRQ 	PROC FAR

   MOV AL, DS:[0]
   XOR AL, 80H
   MOV DS:[0], AL
   
   MOV AL, 10000000B
   MOV DX, L8255CS
   OUT DX, AL
   
   MOV DX, L8255PC
   MOV AL, DS:[0]
   OUT DX, AL

   IRET
MYIRQ 	ENDP

	END