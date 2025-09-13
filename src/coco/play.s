	section code
	export _play_string
	import _strlen

_play_string:
	pshs	u,y,dp
	pshs	x
	jsr	_strlen
	puls	x
	beq	done

playit:
	ldy	#done
	pshs	y
	pshs	b,x
	;; Init DAC
	clrb
	jsr	$a9a2
	jsr	$a976
	puls	b,x

	;; Put terminator on stack
	ldy	#0
	lda	#$ff
	pshs	a,y
	;; Put string on stack
	pshs	b,x
	;; PLAY it!
	jmp	$9A37

done:
	puls	u,y,dp
	rts
