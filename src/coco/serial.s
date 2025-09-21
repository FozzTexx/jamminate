	export _serial_on, _sound_on
	section code

_serial_on:
	ORCC	#$50		; Disable all interrupts

	LDB	>$FF20		; Save current DAC value
	STB	last_sound

	; Turn off all sound to avoid loud popping
	LDB	>$FF23		; PIA2 CRB / CB2 - master select switch
	ANDB	#%11110111	; CB2 outputs a high (1)
	STB	>$FF23

	LDB	#$02		; Set RS232 TX back to mark before turning it on
	STB	>$FF20

	LDB	>$FF21		; Get PIA2 CRA
	ANDB	#%11111011	; bit 2 = 0 -> access data direction register
	STB	>$FF21
	; Now, $FF20 is (temporarily) the data direction register
	; instead of the data register
	LDA	#%00000010	; turn off sound and turn on RS232 TX
	STA	>$FF20		; Save into ddra
	; Switch $FF20 back to being the data register
	ORB	#%00000100	; PIA2 CRA: bit 2 = 1 -> access data register
	STB	>$FF21
	RTS

_sound_on:
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

	LDB	last_sound	; Restore previous DAC value
	STB	>$FF20

	; We will still get no sound out unless we activate the
	; master select switch (required no matter which sound
	; source we're selecting).
	LDA	>$FF23		; PIA2 CRB / CB2 - master select switch
	ORA	#%00001000	; CB2 outputs a high (1)
	STA	>$FF23

	ANDCC	#$AF		; Turn interrupts back on
	RTS

last_sound:
	RMB	1
