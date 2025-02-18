;-----------------------------------------------------------
;
;              Build this with the "Source" menu using
;                     "Build All" option
;
;-----------------------------------------------------------
;
;                           实验二示例程序 

;-----------------------------------------------------------
;                                                          |
;                                                          |
; 功能：控制7段数码管的显示                                |
; 编写：《嵌入式系统原理与实验》课程组                     |
;-----------------------------------------------------------
		DOSSEG
		.MODEL	SMALL		; 设定8086汇编程序使用Small model
		.8086				; 设定采用8086汇编指令集
;-----------------------------------------------------------
;	符号定义                                               |
;-----------------------------------------------------------
;

;-----------------------------------------------------------
;	定义数据段                                             |
;-----------------------------------------------------------
		.data					; 定义数据段;

	;		
;-----------------------------------------------------------
;	定义代码段                                             |
;-----------------------------------------------------------
		.code						; Code segment definition
		.startup					; 定义汇编程序执行入口点

;-----------------------------------------------------------
;主程序部分,向扩展的存储器内放数据                         |
;-----------------------------------------------------------
		MOV AX,8000H                    ;指定DS开始地址
		MOV DS,AX
		MOV BX,0H
     		MOV AL,0
		
L:	        MOV WORD PTR [BX],0BBBBH
		INC BX
		INC BX
		MOV WORD PTR [BX],06666H
		INC BX
		INC BX
		
	        JMP L
WT: 
		JMP WT

		


;-----------------------------------------------------------
;	定义堆栈段                                             |
;-----------------------------------------------------------
		.stack 100h				; 定义256字节容量的堆栈


		END						;指示汇编程序结束编译
