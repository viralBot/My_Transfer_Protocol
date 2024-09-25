changes in .h:
	--non cyclic buffer, 
	--last received seq no. by m_recvfrom
	--nospace flag
	--seq no. in buf struct

swnd: messages sent but not ACKed
swnd size: how many more can i send
rwnd: messages expected more
rwnd size: size of rwnd window(max-min+1)

RECEIVER thread:

	select call on each socket
 
	recv call on socket, then check if nospace

	--if inorder message(if it is the first msg in rwnd)
		--sets flag nospace if buffer full
		--rwnd, rwnd size updated
		--added to recv buffer
		--ACK sent with rwnd size, seq no.
		--sets flag nospace if buffer full

	--if out of order message(if it is not first msg in rwnd but present in rwnd)
		--same as above(no size change)

	--if msg not in rwnd(out of range or duplicate)
		--ACK sent "" "" "" "" ""
		--msg dropped
	
	--if proper ACK(ACK seq no, is in swnd)
		--remove seq no.s from swnd upto ACK
		--modify swnd size based on rwnd size
			= seq no. + rwnd size - last seq no. in swnd
		--modify send buffer
			
	--if dup ACK(not in swnd)
		--modify swnd size based on rwnd size
			= seq no. + rwnd size - last seq no. in swnd
				
	timeout on select: 
		--nospace updated for all sockets
		--ACK sent


SENDER thread:

	while(1)
		sleep(<T/2)
		
		for all sockets:
			take max time of all msgs in swnd
			if curr - maxT > T
				resend all of swnd

		for all sockets:
			if msg in sendBuf 
			but not in swnd and 
			swnd size > 0 then 
				--push to swnd
				--update time of msg in buf
				--update swnd size
				
GARBAGE thread

probability func simulator
				
				
