; ------------------------------------------------------------------------------
; |                                                                            |
; |   Background 4-note chord player for CoCo I/II controllable from BASIC.    |
; |   https://github.com/cocotownretro/VideoCompanionCode/tree/main/AsmSound   |
; |                                                                            |
; |   Algorithms, tricks, and much of the code from the CoCo III's FIRQV4      |
; |   music player with the following credits:                                 |
; |                                                                            |
; |          *************************************************************     |
; |          * 4-Voice Music Player - adapted by Paul Fiscarelli         *     |
; |          *                        nuked by Simon Jonassen            *     |
; |          *                                                           *     |
; |          * Original author credits to:                               *     |
; |          *  Clell A. Dildy Jr. -'68' Micro Journal, March 1982       *     |
; |          *  Garry and Linda Howard - Color Computer News, July 1982  *     |
; |          *  Larry Konecky - Rainbow Magazine, December 1983          *     |
; |          *  Bob Ludlum - Rainbow Magazine, July 1984, Music+ 3/11/84 *     |
; |          *                                                           *     |
; |          * Original algorithm credits to:                            *     |
; |          *  Hal Chamberlain - Byte Magazine, September 1977          *     |
; |          *  https://archive.org/details/byte-magazine-1977-09        *     |
; |          *                                                           *     |
; |          *************************************************************     |
; |                                                                            |
; |   with some updates by Dave (CoCo Town)                                    |
; |                                                                            |
; |   Modified by FozzTexx – song handling removed, callable from C            |
; |                                                                            |
; ------------------------------------------------------------------------------

	export _stop_playback
	export _init_dac
	export _init_interrupts
	export _restore_interrupts
	export _sound_off
	export _get_freq_value

	export _WaveTableSine256

; ---------------------------------------------------------------
; Waveform page numbers (for use in SetWaveform)
; All waveforms preceded with "Wg" created by Paul Fiscarelli's
; Waveform Generator tool: https://github.com/pfiscarelli/CoCo_Waveform_Generator
; These are scaled down to allow 4 simultaneous voices without clipping
;
; $5E: WaveTableSine256		Sine of multiples-of-4
; $5F: WgSine			Pure sine
; $60: WaveTableSquare256	Extreme low / high square wave
; $61: WgSquare			Square synthesized from odd-harmonic sine waves
; $62: WgTrapez			Trapezoid (I think?) synthesized from odd-harmonic sine waves
; $63: WgOrgan			Synthesized organ
; $64: WgSawtooth		Sawtooth shape (gradual fall + sudden rise)
; $65: WgTriangle		Triangle shape
; $66: WgViolin			Synthesized violin
; $67: WgImpulse		Mostly flat with single peak
;
; You can also "play" arbitrary memory as if it were a waveform.
; Note that these are NOT scaled to work with 4-voices, so you
; will get clipping if you use these with more than one voice.
; Not that it matters.	These all sound pretty terrible even
; with only a single voice.  But go for it.
;
; $00: BASIC working storage
; $01: BASIC vectors, jump addresses, tables
; $04-05: Text screen
; $06-0D: Disk BASIC working storage
; $0E-3D: Hi-res graphics screens
; $80-DF: Various ROM chips, cartridge slot
; $FF: Hardware, I/O
; ---------------------------------------------------------------

		opt	cc
		opt	cd
		opt	ct
; ---------------------------------------------------------------
; SoundOut: HS interrupt handler
; This takes up just enough cycles to get triggered every 3rd
; line (instead of on contiguous lines).  That equates to
; 5,240 times per second.
; ---------------------------------------------------------------
	section	code
	align	256

SoundOut:
		LDD	#SoundOut	; Set DP so int handler can run faster
		TFR	A,DP		; RTI will restore original DP (+ all regs)
WaveOff1	LDD	#$0000		; SMC: A = current offset into wavetable, B = fractional part
FreqValue1	ADDD	#$0664		; SMC: A & B updated based on note's frequency
		STD	<WaveOff1+1	; SMC: store updated A & B as next wave offset
		STA	<SumWaveTbl1+2	; SMC: store MSB of offset as LSB of WaveTable address (summing voices below)
		; Voice 2 calculation
WaveOff2	LDD	#$0000
FreqValue2	ADDD	#$0000
		STD	<WaveOff2+1
		STA	<SumWaveTbl2+2
		; Voice 3 calculation
WaveOff3	LDD	#$0000
FreqValue3	ADDD	#$0000
		STD	<WaveOff3+1
		STA	<SumWaveTbl3+2
		; Voice 4 calculation
WaveOff4	LDD	#$0000
FreqValue4	ADDD	#$0000
		STD	<WaveOff4+1
		STA	<SumWaveTbl4+2
		; Sum voices and play result
SumWaveTbl1	LDA	>_WaveTableSine256		; SMC: Get voice 1 value from wavetable
SumWaveTbl2	ADDA	>$0000		; SMC: Add voice 2 value from wavetable
SumWaveTbl3	ADDA	>$0000		; SMC: Add voice 3 value from wavetable
SumWaveTbl4	ADDA	>$0000		; SMC: Add voice 4 value from wavetable
		STA	>$FF20		; Send sum of all voices out on DAC
		LDA	>$FF00		; ack HS irq
		RTI

; ---------------------------------------------------------------
; Stop all playback by restoring regular FS, and disabling HS
; ---------------------------------------------------------------
_stop_playback:
		ORCC	#$50		; Disable all interrupts
		JSR	_sound_off	; Turn off DAC at master switch
		BSR	_restore_interrupts ; Restore normal vsync
		ANDCC	#$EF		; Enable IRQ
		RTS

; ---------------------------------------------------------------
; _init_dac sub
; Talk to the PIA control registers to select the 6-bit DAC
; to output to the TV sound
; ---------------------------------------------------------------
_init_dac:
		; Set PIA1 CA2=0
		LDA	>$FF01		; PIA1 CRA / CA2 - Select sound out
		ANDA	#%11110111	; CA2 outputs a low (0)
		STA	>$FF01
		; Set PIA1 CB2=0
		LDA	>$FF03		; PIA1 CRB / CB2 - Select sound out
		ANDA	#%11110111	; CB2 outputs a low (0)
		STA	>$FF03
		; Now that CA2=0 & CB2=0, selector switches A and B are both
		; in position 0.  For selector switch A, this means route
		; 6=bit DAC to TV sound out.  For selector switch B, this selects
		; right joystick pin 1, but that's an unavoidable byproduct
		;
		; We will still get no sound out unless we activate the
		; master select switch (required no matter which sound
		; source we're selecting).
		LDA	>$FF23		; PIA2 CRB / CB2 - master select switch
		ORA	#%00001000	; CB2 outputs a high (1)
		STA	>$FF23
		; Configure bits 0 & 1 of PIA2 DRA as inputs, so we can
		; send a full 8 bits to $FF20 in the interrupt handler
		; and the lower two bits will automatically be ignored
		LDB	>$FF21		; Get PIA2 CRA
		ANDB	#%11111011	; bit 2 = 0 -> access data direction register
		STB	>$FF21
		; Now, $FF20 is (temporarily) the data direction register
		; instead of the data register
		LDA	#%11111100	; lower two bits set to 0 (input)
		STA	>$FF20		; Save into ddra
		; Switch $FF20 back to being the data register
		ORB	#%00000100	; PIA2 CRA: bit 2 = 1 -> access data register
		STB	>$FF21
		RTS

; ---------------------------------------------------------------
; _init_interrupts sub
; Disable FS, enable HS, init irq vector to SoundOut
; ---------------------------------------------------------------
_init_interrupts:
		; Save original vector for cleanup
		LDA	$10C
		STA	>OrigVectorOp
		LDX	$10D
		STX	>OrigVectorAddr
		; Change vector to SoundOut routine
		LDA	#$7E		; JMP opcode
		STA	>$10C
		LDX	#SoundOut
		STX	>$10D
		; Disable FS IRQ
		LDA	>$FF03
		ANDA	#%11111110
		STA	>$FF03
		; Enable HS IRQ
		LDA	>$FF01
		ORA	#%00000001	; Enable IRQ
		ANDA	#%11111101	; Activate on falling edge
		STA	>$FF01
		LDA	>$FF00		; Clear flag bit (ack HS IRQ)
		RTS

; ---------------------------------------------------------------
; _restore_interrupts sub
; Disable HS, enable FS, init irq vector to original value
; before _init_interrupts was called
; ---------------------------------------------------------------
_restore_interrupts:
		; Disable HS IRQ
		LDA	>$FF01
		ANDA	#%11111110
		STA	>$FF01
		; Enable FS IRQ
		LDA	>$FF03
		ORA	#%00000001
		STA	>$FF03
		LDA	>$FF02		; Clear flag bit (ack FS IRQ)
		; Restore original vector bytes
		LDA	OrigVectorOp
		STA	>$10C
		LDX	OrigVectorAddr
		STX	>$10D
		RTS

OrigVectorOp	RMB	1
OrigVectorAddr	RMB	2

; ---------------------------------------------------------------
; _sound_off sub
; Turns off sound by just deactivating the master select switch
; The positions of selector switches A & B are left unchanged
; ---------------------------------------------------------------
_sound_off:
		LDA	>$FF23		; PIA2 CRB / CB2 - master select switch
		ANDA	#%11110111	; CB2 outputs a low (0)
		STA	>$FF23
		RTS

; ---------------------------------------------------------------
; _get_freq_value sub
; Entry: X -> first character of 3-byte note string
; Exit: D = freq val, or 0 if string not a valid note
; ---------------------------------------------------------------
_get_freq_value:
		LDU	#NoteFreqs	; U -> note table
NoteInfoLoop:
		LDD	,U
		CMPD	,X		; First two characters of note string match?
		BNE	NextNoteInfo	; nope
		LDA	2,U
		CMPA	2,X		; Last character of note string matches?
		BNE	NextNoteInfo	; nope
		; We have a match!
		LDD	NoteInfo.FreqValue,U
		RTS
NextNoteInfo:
		LEAU	sizeof{NoteInfo},U
		TST	,U		; Terminating 0?
		BNE	NoteInfoLoop	; No, loop up
		LDD	#0		; Yes, no match found
		RTS


; Attributes for each note
NoteInfo	STRUCT
Name		RMB	3		; e.g., "C#4" It is assumed this field comes first
FreqValue	RMB	2
		ENDSTRUCT

; Table of NoteInfo structs
NoteFreqs:
		INCLUDE NoteFrequencyValues256.inc

; Waveform tables, 256-bytes each
		ALIGN	$100			; Ensure waveform tables begin on page boundary
_WaveTableSine256:
		INCLUDE WaveTableSine256.inc
;; 		INCLUDE WgSine.asm
;; 		INCLUDE WaveTableSquare256.asm
;; 		INCLUDE WgSquare.asm
;; 		INCLUDE WgTrapez.asm
;; 		INCLUDE WgOrgan.asm
;; 		INCLUDE WgSawtooth.asm
;; 		INCLUDE WgTriangle.asm
;; 		INCLUDE WgViolin.asm
;; 		INCLUDE WgImpulse.asm

		END
