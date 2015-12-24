	PAGE    56,132
;

.386
;
;  SEGMENT definitions and order
;


;*	32 Bit code
_TEXT		SEGMENT DWORD USE32 PUBLIC 'CODE'
_TEXT		ENDS



;*	Contains 32 Bit data
_DATA		SEGMENT DWORD PUBLIC 'DATA'
_DATA		ENDS


	ASSUME CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT


OFFSET32 EQU <OFFSET FLAT:>

_DATA	SEGMENT


CR	EQU	0DH
LF	EQU	0AH


D10		DW	10
D60		DD	60
D3600	DD	3600
D86400	DD	86400

TIMESTAMP	DD	0

NEWVALUE	DW	0
WORD10		DW	10
WORD16		DW	16



;
;	BASIC LINK LEVEL MESSAGE BUFFER LAYOUT
;
MESSAGE	STRUC

MSGCHAIN	DD	?		; CHAIN WORD
MSGPORT		DB	?		; PORT 
MSGLENGTH	DW	?		; LENGTH

MSGDEST		DB	7 DUP (?)	; DESTINATION
MSGORIGIN	DB	7 DUP (?)	; ORIGIN
;
;	 MAY BE UP TO 56 BYTES OF DIGIS
;
MSGCONTROL	DB	?		; CONTROL BYTE
MSGPID		DB	?		; PROTOCOL IDENTIFIER
MSGDATA		DB	?		; START OF LEVEL 2 MESSAGE
;
MESSAGE	ENDS
;
;
;
;	L4FLAGS DEFINITION
;
L4BUSY		EQU	80H		; BNA - DONT SEND ANY MORE
L4NAK		EQU	40H		; NEGATIVE RESPONSE FLAG
L4MORE		EQU	20H		; MORE DATA FOLLOWS - FRAGMENTATION FLAG

L4CREQ		EQU	1		; CONNECT REQUEST
L4CACK		EQU	2		; CONNECT ACK
L4DREQ		EQU	3		; DISCONNECT REQUEST
L4DACK		EQU	4		; DISCONNECT ACK
L4INFO		EQU	5		; INFORMATION
L4IACK		EQU	6		; INFORMATION ACK
;


NULL		EQU	00H
CR		EQU	0DH
LF		EQU	0AH
NETROM_PID	EQU	0CFH
NODES_SIG	EQU	0FFH
;
PORT_MSG	DB	' Port=',NULL
NODES_MSG	DB	' (NetRom Routing)',CR,NULL
NETROM_MSG	DB	' NET/ROM',CR,'  ',NULL
TO_MSG		DB	' To ',NULL
TTL_MSG		DB	' ttl=',NULL
AT_MSG		DB	' at ',NULL
VIA_MSG		DB	' via ',NULL
QUALITY_MSG	DB	' qlty=',NULL
LEN_MSG		DB	' Len=',0

PID_MSG		DB	' pid=',NULL

MYCCT_MSG	DB	' my'
CCT_MSG		DB	' cct=',0
TIM_MSG		DB	' t/o=',0

WINDOW		DB	' w=',0
CONN_REQ_MSG	DB	' <CON REQ>',NULL
CONN_ACK_MSG	DB	' <CON ACK>',NULL
CONN_NAK_MSG	DB	' <CON NAK> - BUSY',NULL
DISC_REQ_MSG	DB	' <DISC REQ>',NULL
DISC_ACK_MSG	DB	' <DISC ACK>',NULL
INFO_MSG	DB	' <INFO S',NULL
INFO_ACK_MSG	DB	' <INFO ACK R',NULL
DUFF_NET_MSG	DB	' <????>',NULL
IP_MSG		DB	' <IP> ',NULL
 
SABM_MSG	DB	'SABM ',NULL
DISC_MSG	DB	'DISC ',NULL
UA_MSG		DB	'UA ',NULL
DM_MSG		DB	'DM ',NULL
RR_MSG		DB	'RR ',NULL
RNR_MSG		DB	'RNR ',NULL
UI_MSG		DB	'UI',NULL
FRMR_MSG	DB	'FRMR ',NULL
REJ_MSG		DB	'REJ ',NULL
;
;	IP AND TCP BITS
;
IPMSG	STRUC
;
;	FORMAT OF IP HEADER
;
;	NOTE THESE FIELDS ARE STORED HI ORDER BYTE FIRST (NOT NORMAL 8086 FORMAT)
;

VERLEN		DB	0		; 4 BITS VERSION, 4 BITS LENGTH
TOS		DB	0		; TYPE OF SERVICE
IPLENGTH	DW	0		; DATAGRAM LENGTH
IPID		DW	0		; IDENTIFICATION
FRAGWORD	DW	0		; 3 BITS FLAGS, 13 BITS OFFSET
IPTTL		DB	0
IPPROTOCOL	DB	0		; HIGHER LEVEL PROTOCOL
IPCHECKSUM	DW	0		; HEADER CHECKSUM
IPSOURCE	DD	0
IPDEST		DD	0
;
;	FORMAT OF TCP HEADER WITHIN AN IP DATAGRAM
;
;	NOTE THESE FIELDS ARE STORED HI ORDER BYTE FIRST (NOT NORMAL 8086 FORMAT)
;

SOURCEPORT	DW	0
DESTPORT	DW	0

SEQNUM		DD	0
ACKNUM		DD	0

TCPCONTROL	DB	0	; 4 BITS DATA OFFSET 4 RESERVED
TCPFLAGS	DB	0	; (2 RESERVED) URG ACK PSH RST SYN FIN

TCPWINDOW	DW	0
CHECKSUM	DW	0
URGPTR		DW	0
;
;	OPTIONS AND/OR DATA MAY FOLLOW
;
TCPOPTIONS	DB	4 DUP (0)

IPMSG	ENDS
;
;	TCPFLAGS BITS
;
FIN	EQU	1B
SYN	EQU	10B
RST	EQU	100B
PSH	EQU	1000B
ACK	EQU	10000B
URG	EQU	100000B


TCP		DB	'TCP: ',0
ICMP		DB	'ICMP: ',0
LEN		DB	' LEN: ',0

SEQTEXT		DB	' SEQ: ',0
ACKTEXT		DB	'ACK: ',0
WINDTEXT	DB	'WIN: ',0
SYNTEXT		DB	'SYN ',0
FINTEXT		DB	'FIN ',0
RSTTEXT		DB	'RST ',0
PSHTEXT		DB	'PSH ',0
ACKTEXT1	DB	'ACK ',0
URGTEXT		DB	'URG ',0

BADSUM		DB	' CHECKSUM ERROR ',0

;-----------------------------------------------------------------------------;
;          Parameter area for received frame                                  ;
;-----------------------------------------------------------------------------;

PORT_NO		DB	0		; Received port number 0 - 256
VERSION_NO	DB	0		; Version 1 or 2       1,2
POLL_FINAL	DB	0		; Poll or Final ?      P,F
COMM_RESP	DB	0		; Command or Response  C,R
FRAME_TYPE	DB	0		; Frame Type           UI etc in Hex
PID		DB	0		; Protocol ID
FRAME_LENGTH	DD	0		; Length of frame      0 - 65...
NR		DB	0		; N(R) Flag
NS		DB	0		; N(S) Flag
INFO_FLAG	DB	0		; Information Packet ? 0 No, 1 Yes
OPCODE		DB	0		; L4 FRAME TYPE
FRMRFLAG	DB	0

TRACEFLAG	DB	1
MALL		DB	1
HEADERLN	DB	1
MCOM		DB	1
MTX			DB	1
MMASK		DW	0FFFFH
;
;	HDLC COMMANDS (WITHOUT P/F)
;
UI	EQU	3
SABM	EQU	2FH
DISC	EQU	43H
DM	EQU	0FH
UA	EQU	63H
FRMR	EQU	87H
RR	EQU	1
RNR	EQU	5
REJ	EQU	9
;
PFBIT	EQU	10H		; POLL/FINAL BIT IN CONTROL BYTE


CRLF		DB	0DH,0AH

AX25CALL	DB	7 DUP (0)	; WORK AREA FOR AX25 <> NORMAL CALL CONVERSION
NORMCALL	DB	10 DUP (0)	; CALLSIGN IN NORMAL FORMAT
NORMLEN		DD	0		; LENGTH OF CALL IN NORMCALL	
;
TENK	DD	10000
	DD	1000
WORD100	DD	100
DWORD10	DD	10
;
ACCUM		DB	4 DUP (0)
CONVFLAG	DB	0
SUPPRESS	DB	0		; ZERO SUPPRESS FLAG

SAVESI		DD	0
SAVEDI		DD	0

_DATA	ENDS

; 2:Fm GM8BPQ-2 To NODES <UI pid=CF Len=49 >[18:49:56]
;FF BPQ    (NetRom Routing)
;      N5IN-9  N9              N5IN-3    152
;       GB7CB  GALA            N5IN-3    151
; 1:Fm GM8BPQ To APU25N <UI pid=F0 Len=29 >[18:51:48]
;=5828.55N/00612.62W- {UIV32}

; 1:Fm GM8BPQ-1 To BPQ <SABM P>[09:09:51]

_TEXT   SEGMENT PUBLIC 'CODE'

	PUBLIC	_AGWMONDECODE,_AGWMONOPTIONS

_AGWMONOPTIONS:

	MOV	MMASK,AX
	MOV	MTX,BL
	MOV	MCOM,CL

	RET

_AGWMONDECODE:
;
;	esi=message, edi=buffer, return length in ECX amd frame type in EAX
;

	MOV	SAVESI,ESI
	MOV	SAVEDI,EDI

	MOV	TIMESTAMP,EAX

	cmp	TRACEFLAG,0
	jne	TRACEOK


TRACERET:

	ret

TRACEOK:

	MOVZX	ECX,MSGLENGTH[ESI]
;
;	GET THE CONTROL BYTE, TO SEE IF THIS FRAME IS TO BE DISPLAYED
;
	PUSH	ESI
	MOV	ECX,9			; MAX DIGIS + 1
CTRLLOOP:
	TEST	BYTE PTR MSGCONTROL-1[ESI],1
	JNZ	CTRLFOUND

	ADD	ESI,7
	LOOP	CTRLLOOP
;
;	INVALID FRAME
;
	POP	ESI
	MOV	ECX,0
	RET

CTRLFOUND:

	MOV	AL,MSGCONTROL[ESI]
	POP		ESI
;
	TEST	AL,1			; I FRAME
	JZ	IFRAME

	AND	AL,NOT PFBIT		; CLEAR P/F
	CMP	AL,3			; UI
	JE	OKTOTRACE		; ALWAYS DO UI

	CMP	AL,FRMR
	JE	OKTOTRACE		; ALWAYS DO FRMR

;
;	USEQ/CONTROL - TRACE IF MCOM ON
;
	CMP	MCOM,0
	JNE	OKTOTRACE

	MOV	ECX,0
	RET

;-----------------------------------------------------------------------------;
;       Check for MALL                                                        ;
;-----------------------------------------------------------------------------;

IFRAME:

	cmp	MALL,0
	jne	OKTOTRACE

	MOV	ECX,0
	ret

OKTOTRACE:
;
;-----------------------------------------------------------------------------;
;       Get the port number of the received frame                             ;
;-----------------------------------------------------------------------------;

	mov	CL,MSGPORT[ESI]
	mov	PORT_NO,CL

	TEST	CL,80H
	JZ	NOT_TX
;
;	TRANSMITTED FRAME - SEE IF MTX ON
;
	CMP	MTX,1
	JE	NOT_TX

	MOV	ECX,0
	RET

NOT_TX:

	AND	CL,7FH			; MASK T/R BIT

	DEC	CL
	MOV	AX,1
	SHL	AX,CL			; SHIFT BIT UP

	TEST	MMASK,AX
	JNZ	TRACEOK1

	MOV 	ECX,0
	RET

TRACEOK1:

	MOV	FRMRFLAG,0

	mov	AH,MSGDEST+6[ESI]
	mov	AL,MSGORIGIN+6[ESI]

	mov	COMM_RESP,0			; Clear Command/Response Flag

;-----------------------------------------------------------------------------;
;       Is it a Poll/Final or Command/Response                                ;
;-----------------------------------------------------------------------------;

	test	AH,80H
	mov	COMM_RESP,'C'
	jnz	NOT_RESPONSE
	mov	COMM_RESP,'R'

NOT_RESPONSE:

;-----------------------------------------------------------------------------;
;       Is this version 1 or 2 of AX25 ?                                      ;
;-----------------------------------------------------------------------------;

	xor	AH,AL
	test	AH,80H
	mov	VERSION_NO,1
	je	VERSION_1
	mov	VERSION_NO,2

VERSION_1:


	mov	al,PORT_NO
	and	al,7fh


	cmp	AL,10
	JAE	@F
	
	PUSH	EAX

	MOV	AL,' '
	CALL	PUTCHAR

	POP		EAX
@@:
	call	DISPLAY_BYTE_1

	MOV	AL,':'

	CALL	PUTCHAR

	MOV	AL,'F'
	CALL	PUTCHAR

	MOV	AL,'m'
	CALL	PUTCHAR

	MOV	AL,' '
	CALL	PUTCHAR

;-----------------------------------------------------------------------------;
;       Display Origin Callsign                                               ;
;-----------------------------------------------------------------------------;

	PUSH	ESI

	lea	ESI,MSGORIGIN[ESI]
	call	CONVFROMAX25

	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	POP	ESI

	PUSH	ESI

	mov	EBX,OFFSET TO_MSG
	call	NORMSTR

;-----------------------------------------------------------------------------;
;       Display Destination Callsign                                          ;
;-----------------------------------------------------------------------------;

	lea	ESI,MSGDEST[ESI]
	call	CONVFROMAX25

	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	pop	ESI

	movzx	EAX,MSGLENGTH[ESI]
	mov	FRAME_LENGTH,EAX
	mov	CX,8			; Max number of digi-peaters

;-----------------------------------------------------------------------------;
;       Display any Digi-Peaters                                              ;
;-----------------------------------------------------------------------------;

NEXT_DIGI:

	test	MSGORIGIN+6[ESI],1
	jnz	NO_MORE_DIGIS

	add	ESI,7
	sub	FRAME_LENGTH,7		; Reduce length

	push	ESI

	push	ECX
	lea	ESI,MSGORIGIN[ESI]
	call	CONVFROMAX25		; Convert to call

	push	EAX			; Last byte is in AH

	mov	AL,','
	call	PUTCHAR

	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	pop	EAX

	test	AH,80H
	jz	NOT_REPEATED

	mov	AL,'*'
	call	PUTCHAR

NOT_REPEATED:

	pop	ECX
	pop	ESI
	loop	NEXT_DIGI

NO_MORE_DIGIS:	

	MOV	AL,' '
	CALL	PUTCHAR

;-----------------------------------------------------------------------------;
;       If this is Version 2 get the Poll/Final Bit                           ;
;-----------------------------------------------------------------------------;

	mov	POLL_FINAL,0		; Clear Poll/Final Flag

	mov	AL,MSGCONTROL[ESI]	; Get control byte

	cmp	COMM_RESP,'C'
	jne	NOT_COMM

	test	AL,PFBIT
	je	NOT_POLL

	mov	POLL_FINAL,'P'

NOT_POLL:
NOT_COMM:

	cmp	COMM_RESP,'R'
	jne	NOT_RESP

	test	AL,PFBIT
	je	NOT_FINAL

	mov	POLL_FINAL,'F'

NOT_FINAL:
NOT_RESP:

;-----------------------------------------------------------------------------;
;       Start displaying the frame information                                ;
;-----------------------------------------------------------------------------;

	and	AL,NOT PFBIT		; Remove P/F bit
	mov	FRAME_TYPE,AL

	mov	AL,'<'			; Print "<"
	call	PUTCHAR

	mov	NR,0			; Reset all the flags
	mov	NS,0
	mov	INFO_FLAG,0

	mov	AL,FRAME_TYPE

	test	AL,1
	jne	NOT_I_FRAME

;-----------------------------------------------------------------------------;
;       Information frame                                                     ;
;-----------------------------------------------------------------------------;

	mov	AL,'I'
	call	PUTCHAR
	mov	AL,' '
	call	PUTCHAR

	mov	INFO_FLAG,1
	mov	NR,1
	mov	NS,1
	jmp	END_OF_TYPE

NOT_I_FRAME:

;-----------------------------------------------------------------------------;
;       Un-numbered Information Frame                                         ;
;-----------------------------------------------------------------------------;

	cmp	AL,UI
	jne	NOT_UI_FRAME

	mov	EBX,OFFSET UI_MSG
	call	NORMSTR

	mov	INFO_FLAG,1
	jmp	END_OF_TYPE

NOT_UI_FRAME:
	test	AL,10B
	jne	NOT_R_FRAME

;-----------------------------------------------------------------------------;
;       Process supervisory frames                                            ;
;-----------------------------------------------------------------------------;

	mov	NR,1			; All supervisory frames have N(R)

	and	AL,0FH			; Mask the interesting bits
	cmp	AL,RR	
	jne	NOT_RR_FRAME

	mov	EBX,OFFSET RR_MSG
	call	NORMSTR
	jmp	END_OF_TYPE

NOT_RR_FRAME:
	cmp	AL,RNR
	jne	NOT_RNR_FRAME

	mov	EBX,OFFSET RNR_MSG
	call	NORMSTR
	jmp	END_OF_TYPE

NOT_RNR_FRAME:
	cmp	AL,REJ
	jne	NOT_REJ_FRAME

	mov	EBX,OFFSET REJ_MSG
	call	NORMSTR
	jmp	END_OF_TYPE

NOT_REJ_FRAME:
	mov	NR,0			; Don't display sequence number
	mov	AL,'?'			; Print "?"
	call	PUTCHAR
	jmp	END_OF_TYPE

;-----------------------------------------------------------------------------;
;       Process all other frame types                                         ;
;-----------------------------------------------------------------------------;

NOT_R_FRAME:
	cmp	AL,UA
	jne	NOT_UA_FRAME

	mov	EBX,OFFSET UA_MSG
	call	NORMSTR
	jmp	SHORT END_OF_TYPE

NOT_UA_FRAME:
	cmp	AL,DM
	jne	NOT_DM_FRAME

	mov	EBX,OFFSET DM_MSG
	call	NORMSTR
	jmp	SHORT END_OF_TYPE

NOT_DM_FRAME:
	cmp	AL,SABM
	jne	NOT_SABM_FRAME

	mov	EBX,OFFSET SABM_MSG
	call	NORMSTR

	jmp	SHORT END_OF_TYPE

NOT_SABM_FRAME:
	cmp	AL,DISC
	jne	NOT_DISC_FRAME

	mov	EBX,OFFSET DISC_MSG
	call	NORMSTR

	jmp	SHORT END_OF_TYPE

NOT_DISC_FRAME:
	cmp	AL,FRMR
	jne	NOT_FRMR_FRAME

	mov	EBX,OFFSET FRMR_MSG
	call	NORMSTR
	MOV	FRMRFLAG,1
	jmp	SHORT END_OF_TYPE

NOT_FRMR_FRAME:
	mov	AL,'?'
	call	PUTCHAR

END_OF_TYPE:

;----------------------------------------------------------------------------;
;       If Version 2 Then display P/F C/R Information                        ;
;----------------------------------------------------------------------------;

	IF 0

	cmp	VERSION_NO,2
	jne	NOT_VERSION_2

	mov	AL,' '
	call	PUTCHAR

	mov	AL,COMM_RESP		; Print Command/Response Flag
	call	PUTCHAR

	ENDIF
	
	cmp	POLL_FINAL,0
	je	NO_POLL_FINAL


	mov	AL,POLL_FINAL		; Print Poll/Final Flag if Set
	call	PUTCHAR

	mov	AL,' '
	call	PUTCHAR

NO_POLL_FINAL:
NOT_VERSION_2:

	
;----------------------------------------------------------------------------;
;       Display sequence numbers if applicable                               ;
;----------------------------------------------------------------------------;

	cmp	NR,1
	jne	NOT_NR_DATA


	mov	AL,'R'
	call	PUTCHAR

	mov	AL,FRAME_TYPE
	rol	AL,1
	rol	AL,1
	rol	AL,1

	call	DISPLAYSEQ

	mov	AL,' '
	call	PUTCHAR

NOT_NR_DATA:

	cmp	NS,1
	jne	NOT_NS_DATA

	mov	AL,'S'
	call	PUTCHAR

	mov	AL,FRAME_TYPE
	ror	AL,1

	call	DISPLAYSEQ

NOT_NS_DATA:

	CMP	INFO_FLAG,1
	JNE		NO_PID_LEN
	
	
	mov	EBX,OFFSET PID_MSG
	call	NORMSTR

	PUSH	ESI
	lea	ESI,MSGPID[ESI]
	lodsb
	POP		ESI

	CALL	HEXOUT

	mov	EBX,OFFSET LEN_MSG
	call	NORMSTR

	MOV	EAX,FRAME_LENGTH
	SUB	EAX,23

	CALL	DISPLAY_BYTE_1

	mov	AL,' '
	call	PUTCHAR

NO_PID_LEN:

	mov	AL,'>'
	call	PUTCHAR

	mov	AL,'['
	call	PUTCHAR

;
;	DISPLAY TIMESTAMP
;

	MOV	EAX,TIMESTAMP
	mov	EDX,0
	DIV	D86400

	MOV	EAX,EDX
	MOV	edx,0
	DIV	D3600

	CALL	DISPLAY_BYTE_2

	MOV	AL,':'
	CALL	PUTCHAR

	MOV	EAX,EDX
	MOV	EDX,0
	DIV	D60			; MINS IN AX, SECS IN DX

	PUSH	DX

	CALL	DISPLAY_BYTE_2

	MOV	AL,':'
	CALL	PUTCHAR

	POP	AX			; SECS
	CALL	DISPLAY_BYTE_2


	mov	AL,']'
	call	PUTCHAR


	CMP	FRMRFLAG,0
	JE	NOTFRMR
;
;	DISPLAY FRMR BYTES
;
	lea	ESI,MSGPID[ESI]
	MOV	ECX,3			; TESTING
FRMRLOOP:
	lodsb
	CALL	BYTE_TO_HEX

	LOOP	FRMRLOOP

	JMP	NO_INFO

NOTFRMR:
;----------------------------------------------------------------------------;
;       Find the PID if an information frame                                 ;
;----------------------------------------------------------------------------;

	mov	AL,0

	cmp	INFO_FLAG,1
	jne	NO_PID

	lea	ESI,MSGPID[ESI]
	lodsb

NO_PID:
	mov	PID,AL

;----------------------------------------------------------------------------;
;       Is this a NET/ROM message of any sort ?                              ;
;----------------------------------------------------------------------------;

	MOV	ECX,FRAME_LENGTH

	cmp	PID,NETROM_PID
	je	DISPLAY_NETROM

;----------------------------------------------------------------------------;
;       Display the rest of the frame (If Any)                               ;
;----------------------------------------------------------------------------;

DISPLAY_INFO:

	cmp	INFO_FLAG,1		; Is it an information packet ?
	jne	NO_INFO


	XOR	AL,AL			; IN CASE EMPTY

	sub	ECX,23
	JCXZ	NO_INFO			; EMPTY I FRAME
;
;	PUT TEXT ON A NEW LINE
;
	PUSH	ECX
	MOV	AL,0DH
	PUSH	ESI
	CALL	PUTCHAR
	POP	ESI
	POP	ECX

SAMELN:

	cmp	ECX,257
	jb	LENGTH_OK

	mov	ECX,256

LENGTH_OK:

	push	ECX
	lodsb
	
	cmp	AL,0AH
	JE	MONOK
	CMP	AL,0DH
	JE	MONOK

	CMP	AL,20H
	JB	SKIP_MON		; IGNORE OTHER CONTROLS

MONOK:
	call	PUTCHAR

SKIP_MON:

	pop	ECX
	loop	LENGTH_OK

NO_INFO:
;
;	ADD CR UNLESS DATA ALREADY HAS ONE
;
	CMP	AL,CR
	JE	NOTANOTHER
ADD_CR:
	mov	AL,CR
	call	PUTCHAR

NOTANOTHER:
;
	MOV	ECX,EDI
	SUB	ECX,SAVEDI

	MOVZX	EAX,FRAME_TYPE

	RET


;----------------------------------------------------------------------------;
;       Display NET/ROM data                                                 ;
;----------------------------------------------------------------------------;

DISPLAY_NETROM:

	MOV	AL,CR
	CALL	PUTCHAR

	lodsb
	PUSH	EAX
	CALL	HEXOUT
	
	mov	AL,' '
	call	PUTCHAR

	POP		EAX

	cmp	AL,NODES_SIG		; Check NODES message

	JNE	DISPLAY_NETROM_DATA
	
	cmp	FRAME_TYPE, 3
	JNE	DISPLAY_NETROM_DATA	; Not UI, so INP3
	
;----------------------------------------------------------------------------;
;       Display NODES broadcast                                              ;
;----------------------------------------------------------------------------;

	push	ECX

	mov	ECX,6
	REP MOVSB			; ALIAS

	mov	EBX,OFFSET NODES_MSG
	call	NORMSTR

	pop	ECX

	sub	ECX,30			; Header, mnemonic and signature length

NODES_LOOP:

	cmp	ECX,0
	jbe	NO_INFO

	push	ECX
	push	ESI			; Global push for each node

	mov	AL,' '
	call	PUTCHAR
	mov	AL,' '
	call	PUTCHAR

	push	ESI

	add	ESI,7			; Display destination mnemonic

	cmp	BYTE PTR [ESI],' '
	je	NO_MNEMONIC

	mov	ECX,6			; Max length

MNEMONIC_LOOP:

	lodsb				; Get character

	cmp	AL,' '			; Short mnemonic ?
	je	END_MNEMONIC

	call	PUTCHAR

	loop	MNEMONIC_LOOP

END_MNEMONIC:

	mov	AL,':'
	call	PUTCHAR

NO_MNEMONIC:

	pop	ESI
	push	ESI

	call	CONVFROMAX25		; Display dest callsign
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	mov	EBX,OFFSET VIA_MSG
	call	NORMSTR

	pop	ESI
	add	ESI,13			; Point to neighbour callsign
	push	ESI

	call	CONVFROMAX25
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	mov	EBX,OFFSET QUALITY_MSG
	call	NORMSTR

	pop	ESI
	add	ESI,7			; Point to quality byte

	mov	AL,[ESI]
	call	DISPLAY_BYTE_1

	mov	AL,CR
	call	PUTCHAR

	pop	ESI
	pop	ECX
	add	ESI,21			; Point to next destination
	sub	ECX,21			; Remove length of each 

	jmp	NODES_LOOP

;----------------------------------------------------------------------------;
;       Display normal NET/ROM transmissions                                 ;
;----------------------------------------------------------------------------;

DISPLAY_NETROM_DATA:

	DEC	ESI			; BACK TO DATA

	mov	EBX,OFFSET NETROM_MSG
	call	NORMSTR

	PUSH	ESI

	call	CONVFROMAX25
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	mov	EBX,OFFSET TO_MSG
	call	NORMSTR

	pop	ESI
	add	ESI,7

	push	ESI

	call	CONVFROMAX25
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR
;
;       Display Time To Live number
;
	mov	EBX,OFFSET TTL_MSG
	call	NORMSTR

	pop	ESI
	add	ESI,7			; Point to TTL counter

	lodsb
	call	DISPLAY_BYTE_1

;
;	DISPLAY CIRCUIT ID
;
	MOV	EBX,OFFSET CCT_MSG
	CALL	NORMSTR

	LODSB
	CALL	BYTE_TO_HEX

	LODSB
	CALL	BYTE_TO_HEX

	INC	ESI
	INC	ESI		; TO OPCODE

;-----------------------------------------------------------------------------;
;       Determine type of Level 4 frame                                       ;
;-----------------------------------------------------------------------------;

	mov	AL,[ESI]
	MOV	OPCODE,AL		; SAVE
	AND	AL,0FH			; Strip off flags

	cmp	AL,L4CREQ
	jne	NOT_L4CREQ

	mov	EBX,OFFSET CONN_REQ_MSG
	call	NORMSTR

	MOV	EBX,OFFSET WINDOW
	CALL	NORMSTR

	INC	ESI
	LODSB			; WINDOW SIZE

	CALL	DISPLAY_BYTE_1

	mov	AL,' '
	call	PUTCHAR

	PUSH	ESI

	call	CONVFROMAX25
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	mov	EBX,OFFSET AT_MSG
	call	NORMSTR

	pop	ESI
	add	ESI,7
	PUSH	ESI

	call	CONVFROMAX25
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR


	POP	ESI
	CMP	FRAME_LENGTH,58
	JE	NOT_BPQ
;
;	BPQ EXTENDED CON REQ - DISPLAY TIMEOUT
;
	MOV	EBX,OFFSET TIM_MSG
	CALL	NORMSTR

	MOV	AX,7[ESI]		; TIMEOUT

	CALL	DISPLAY_BYTE_1
;
NOT_BPQ:

	JMP	ADD_CR

NOT_L4CREQ:

	cmp	AL,L4CACK
	jne	NOT_L4CACK

	TEST	OPCODE,L4BUSY
	JZ	L4CRQ00
;
;	BUSY RETURNED
;
	MOV	EBX,OFFSET CONN_NAK_MSG
	CALL	NORMSTR

	JMP	END_NETROM

L4CRQ00:

	MOV	EBX,OFFSET CONN_ACK_MSG
	CALL	NORMSTR

	MOV	EBX,OFFSET WINDOW
	CALL	NORMSTR

	MOV	AL,1[ESI]			; WINDOW SIZE

	CALL	DISPLAY_BYTE_1

	MOV	EBX,OFFSET MYCCT_MSG
	CALL	NORMSTR

	MOV	AL,-2[ESI]
	CALL	BYTE_TO_HEX

	MOV	AL,-1[ESI]
	CALL	BYTE_TO_HEX

	JMP	ADD_CR

NOT_L4CACK:

	cmp	AL,L4DREQ
	jne	NOT_L4DREQ

	mov	EBX,OFFSET DISC_REQ_MSG
	call	NORMSTR

	JMP	ADD_CR

NOT_L4DREQ:

	cmp	AL,L4DACK
	jne	NOT_L4DACK

	mov	EBX,OFFSET DISC_ACK_MSG
	call	NORMSTR

	jmp	add_cr

NOT_L4DACK:

	cmp	AL,L4INFO
	jne	NOT_L4INFO

	mov	EBX,OFFSET INFO_MSG
	call	NORMSTR

	mov	AL,-2[ESI]			; Get send sequence number
	call	DISPLAY_BYTE_1

	mov	AL,' '
	call	PUTCHAR
	mov	AL,'R'
	call	PUTCHAR

	mov	AL,-1[ESI]		; Get receive sequence number
	call	DISPLAY_BYTE_1

	mov	AL,'>'
	call	PUTCHAR

	INC	ESI			; TO DATA
	MOV	ECX,FRAME_LENGTH
	sub	ECX,20
	
	CALL	DOL4FLAGS

	jmp	DISPLAY_INFO


NOT_L4INFO:

	cmp	AL,L4IACK
	jne	NOT_L4IACK

	mov	EBX,OFFSET INFO_ACK_MSG
	call	NORMSTR

	mov	AL,-1[ESI]			; Get receive sequence number
	call	DISPLAY_BYTE_1

	mov	AL,'>'
	call	PUTCHAR

	CALL	DOL4FLAGS

	JMP SHORT END_NETROM

NOT_L4IACK:

	OR	AL,AL
	JNZ	NOTIP
;
;	TCP/IP DATAGRAM
;
	mov	EBX,OFFSET IP_MSG
	call	NORMSTR
;
	INC	ESI			; NOW POINTING TO IP HEADER
;
	PUSH	ESI

	LEA	ESI,IPSOURCE[ESI]
	CALL	PRINT4			; PRINT IF ADDR IN 'DOTTED DECIMAL' FORMAT

	POP	ESI

	MOV	AL,'>' 
	CALL	PUTCHAR

	PUSH	ESI

	LEA	ESI,IPDEST[ESI]
	CALL	PRINT4			; PRINT IF ADDR IN 'DOTTED DECIMAL' FORMAT

	MOV	EBX,OFFSET LEN
	CALL	NORMSTR

	POP	ESI

	MOV	AL,BYTE PTR IPLENGTH[ESI]
	CALL	BYTE_TO_HEX

	MOV	AL,BYTE PTR IPLENGTH+1[ESI]
	CALL	BYTE_TO_HEX

	MOV	AL,20H
	CALL	PUTCHAR

	MOV	AL,IPPROTOCOL[ESI]
	CALL	DISPLAY_BYTE_1		; DISPLAY PROTOCOL TYPE

;	mov	AL,CR
;	call	PUTCHAR
;
;	MOV	ECX,39			; TESTING
;IPLOOP:
;	lodsb
;	CALL	BYTE_TO_HEX
;
;	LOOP	IPLOOP

	JMP	ADD_CR

NOTIP:

	mov	EBX,OFFSET DUFF_NET_MSG
	call	NORMSTR


END_NETROM:

	jmp	add_cr

DOL4FLAGS:
;
;	DISPLAY BUSY/NAK/MORE FLAGS
;
	TEST	OPCODE,L4BUSY
	JZ	L4F010

	MOV	AL,'B'
	CALL	PUTCHAR
L4F010:
	TEST	OPCODE,L4NAK
	JZ	L4F020

	MOV	AL,'N'
	CALL	PUTCHAR
L4F020:
	TEST	OPCODE,L4MORE
	JZ	L4F030

	MOV	AL,'M'
	CALL	PUTCHAR
L4F030:
	RET

;----------------------------------------------------------------------------;
;       Display ASCIIZ strings                                               ;
;----------------------------------------------------------------------------;

NORMSTR:
	MOV	AL,[EBX]
	INC	EBX
	cmp	AL,NULL		; End of String ?
	je	NORMSTR_RET	; Yes
	call	PUTCHAR
	jmp	SHORT NORMSTR

NORMSTR_RET:
	ret

;-----------------------------------------------------------------------------;
;       Display sequence numbers                                              ;
;-----------------------------------------------------------------------------;

DISPLAYSEQ:
	and	AL,7
	add	AL,30H
	call	PUTCHAR
	ret

;-----------------------------------------------------------------------------;
;       Display Callsign pointed to by SI                                     ;
;-----------------------------------------------------------------------------;

DISPADDR:
	
	jcxz	DISPADDR_RET

	LODS	NORMCALL
	call	PUTCHAR

	loop	DISPADDR

DISPADDR_RET:
	ret


PRINT4:
;
;	DISPLAY IP ADDR IN DOTTED DECIMAL FORMAT
;

	LODSB
	CALL	DISPLAY_BYTE_1
	MOV	AL,'.'
	CALL	PUTCHAR

	LODSB
	CALL	DISPLAY_BYTE_1
	MOV	AL,'.'
	CALL	PUTCHAR

	LODSB
	CALL	DISPLAY_BYTE_1
	MOV	AL,'.'
	CALL	PUTCHAR

	LODSB
	CALL	DISPLAY_BYTE_1

	RET


	
;-----------------------------------------------------------------------------;
;       Convert byte in AL to nnn, nn or n format                             ;
;-----------------------------------------------------------------------------;

DISPLAY_BYTE_1:

	cmp	AL,100
	jb	TENS_1

	mov	AH,0

HUNDREDS_LOOP_1:
	cmp	AL,100
	jb	HUNDREDS_LOOP_END_1

	sub	AL,100
	inc	AH
	jmp	SHORT HUNDREDS_LOOP_1

HUNDREDS_LOOP_END_1:
	push	AX
	mov	AL,AH
	add	AL,30H
	call	PUTCHAR
	pop	AX
	jmp	SHORT TENS_PRINT_1

TENS_1:
	cmp	AL,10
	jb	UNITS_1

TENS_PRINT_1:
	mov	AH,0

TENS_LOOP_1:
	cmp	AL,10
	jb	TENS_LOOP_END_1

	sub	AL,10
	inc	AH
	jmp	SHORT TENS_LOOP_1

TENS_LOOP_END_1:
	push	AX
	mov	AL,AH
	add	AL,30H
	call	PUTCHAR
	pop	AX

UNITS_1:
	add	AL,30H
	call	PUTCHAR

	ret

;-----------------------------------------------------------------------------;
;       Convert byte in AL to nn format                                       ;
;-----------------------------------------------------------------------------;

DISPLAY_BYTE_2:
	cmp	AL,100
	jb	TENS_2

	sub	AL,100
	jmp	SHORT DISPLAY_BYTE_2

TENS_2:
	mov	AH,0

TENS_LOOP_2:
	cmp	AL,10
	jb	TENS_LOOP_END_2

	sub	AL,10
	inc	AH
	jmp	SHORT TENS_LOOP_2

TENS_LOOP_END_2:
	push	AX
	mov	AL,AH
	add	AL,30H
	call	PUTCHAR
	pop	AX

UNITS_2:
	add	AL,30H
	call	PUTCHAR

	ret

;-----------------------------------------------------------------------------;
;       Convert byte in AL to Hex display                                     ;
;-----------------------------------------------------------------------------;

BYTE_TO_HEX:
	push	AX
	shr	AL,1
	shr	AL,1
	shr	AL,1
	shr	AL,1
	call	NIBBLE_TO_HEX
	pop	AX
	call	NIBBLE_TO_HEX
	ret

NIBBLE_TO_HEX:
	and	AL,0FH
	cmp	AL,10

	jb	LESS_THAN_10
	add	AL,7

LESS_THAN_10:
	add	AL,30H
	call	PUTCHAR
	ret



CONVFROMAX25:
;
;	CONVERT AX25 FORMAT CALL IN [SI] TO NORMAL FORMAT IN NORMCALL
;	   RETURNS LENGTH IN CX AND NZ IF LAST ADDRESS BIT IS SET
;
	PUSH	EDI		; SAVE BUFFER

	PUSH	ESI			; SAVE
	MOV	EDI,OFFSET NORMCALL
	MOV	ECX,10			; MAX ALPHANUMERICS
	MOV	AL,20H
	REP STOSB			; CLEAR IN CASE SHORT CALL
	MOV	EDI,OFFSET NORMCALL
	MOV	CL,6
CONVAX50:
	LODSB
	CMP	AL,40H
	JE	CONVAX60		; END IF CALL - DO SSID

	SHR	AL,1
	STOSB
	LOOP	CONVAX50
CONVAX60:
	POP	ESI
	ADD	ESI,6			; TO SSID
	LODSB
	MOV	AH,AL			; SAVE FOR LAST BIT TEST
	SHR	AL,1
	AND	AL,0FH
	JZ	CONVAX90		; NO SSID - FINISHED
;
	MOV	BYTE PTR [EDI],'-'
	INC	EDI
	CMP	AL,10
	JB	CONVAX70
	SUB	AL,10
	MOV	BYTE PTR [EDI],'1'
	INC	EDI
CONVAX70:
	ADD	AL,30H			; CONVERT TO DIGIT
	STOSB
CONVAX90:
	MOV	ECX,EDI
	SUB	ECX,OFFSET NORMCALL
	MOV	NORMLEN,ECX		; SIGNIFICANT LENGTH

	TEST	AH,1			; LAST BIT SET?

	POP	EDI
	RET

HEXOUT:

	PUSH	AX
	PUSH	AX
	sar	al,1
	sar	al,1
	sar	al,1
	sar	al,1
	call	hexout1
	pop	ax
	call	hexout1
	POP	AX
	ret

hexout1:
	and	al,0fh
	cmp	al,10
	jl	hexout5
	add	al,7
hexout5:
	add	al,30h
	STOSB
;
	ret

PUTCHAR:
	STOSB
	RET

_TEXT	ENDS
;               
	END
