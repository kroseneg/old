#!/usr/bin/env python

import socket
import struct
import time

# constants

# request
REQ_ACQ_LOCK 		=	1
REQ_REL_LOCK		=	2
REQ_TRY_LOCK		=	3
REQ_PING		=	4
REQ_ADOPT		=	5
REQ_SYNC		=	6

# reply
REP_LOCK_ACQUIRED	=	128
REP_LOCK_WBLOCK		=	129
REP_LOCK_RELEASED	=	130
REP_PONG		= 	131
REP_ACK			=	132
REP_ERR			=	133
REP_SYNC		=	134

def decode_rep(r):
	"Decodes a response"
	if r == 128:	return 'REP_LOCK_ACQUIRED'
	if r == 129:	return 'REP_LOCK_WBLOCK'
	if r == 130:	return 'REP_LOCK_RELEASED'
	if r == 131:	return 'REP_PONG'
	if r == 132:	return 'REP_ACK'
	if r == 133:	return 'REP_ERR'
	if r == 134:	return 'REP_SYNC'
	return 'UNK_REP_' + str(r)

def send_cmd(fd, op, payload):
	"Sends a command to the lock server"
	header = [0, 0, 0, 0]
	ver = 1
	p = payload + '\0'
	plen = len(p) & 0x000FFFFF

	ver = ord(chr(ver))
	op = ord(chr(op))

	plen4 = plen & 0x0000FF
	plen3 = plen & 0x00FF00
	plen2 = plen & 0x0F0000

	header[0] = ((ver << 4) & 0xF0) + (op >> 4)
	header[1] = ((op << 4) & 0xF0) + (plen2 >> 16)
	header[2] = plen3 >> 8
	header[3] = plen4

	s = struct.pack("BBBB", header[0], header[1], header[2], header[3])
	s += p
	fd.send(s)

def recv_cmd(fd):
	"Gets a command from the lock server"
	raw_header = ''
	while len(raw_header) < 4:
		raw_header += fd.recv(4 - len(raw_header))

	header = struct.unpack("BBBB", raw_header)

	ver = header[0] >> 4
	op = ( (header[0] & 0x0F) << 4 ) + ( (header[1] & 0xF0) >> 4 )
	plen = ((header[1] & 0x0F) << 16) + (header[2] << 8) + header[3]
	
	payload = ''
	while len(payload) < plen:
		payload += fd.recv(plen - len(payload))

	#print ver, decode_rep(op), plen, '::', payload
	return (op, payload)


class LockException(Exception):
	"We use it to raise exceptions"
	pass

class locker:
	"Represents a lock server connection"
	def __init__(self, host = 'localhost', port = 2626):
		self.fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.fd.connect(('localhost', 2626))
	
	def lock(self, obj):
		"Locks an object identified by the string 'obj'"
		send_cmd(self.fd, REQ_ACQ_LOCK, str(obj))
		op, p = recv_cmd(self.fd)
		if op == REP_ACK:
			# wait for a definitive answer
			op, p = recv_cmd(self.fd)
		if op != REP_LOCK_ACQUIRED:
			raise LockException, (op, p)
		return 1
	
	def unlock(self, obj):
		"Unlocks an object identified by the string 'obj'"
		send_cmd(self.fd, REQ_REL_LOCK, str(obj))
		op, p = recv_cmd(self.fd)
		if op != REP_LOCK_RELEASED:
			raise LockException, (op, p)
		return 1
	
	def trylock(self, obj):
		"""Tries to get a lock on an object identified by the string
		'obj', returns 1 if success, 0 if the operation would block"""
		send_cmd(self.fd, REQ_TRY_LOCK, str(obj))
		op, p = recv_cmd(self.fd)
		if op == REP_LOCK_ACQUIRED:
			return 1
		elif op == REP_LOCK_WBLOCK:
			return 0
		else:
			raise LockException, (op, p)
	
	def adopt(self, obj):
		"""Adopts a lock that has been orphaned by its previous holder
		(probably because it died), so we become the new owners of the
		lock"""
		send_cmd(self.fd, REQ_ADOPT, str(obj))
		op, p = recv_cmd(self.fd)
		if op == REP_ACK:
			return 1
		else:
			raise LockException, (op, p)
	
	def ping(self, data = ''):
		"Pings the server"
		send_cmd(self.fd, REQ_PING, data)
		op, p = recv_cmd(self.fd)
		if op == REP_PONG:
			return 1
		else:
			raise LockException, (op, p)
	
	def sync(self):
		"Gets a list of orphan locks - TODO"
		# TODO
		pass


def _perf_testing(times = 10000, host = 'localhost', port = 2626):
	"Used internally to do some performance testings"
	c = locker(host, port)
	count = 0
	
	# unlock first, just in case
	try:
		c.unlock('Performance Test')
	except:
		pass
	
	try:
		ts = time.time()
		for i in range(0, times):
			#c.ping('Performance Test')
			c.lock('Performance Test')
			c.unlock('Performance Test')
			count += 1
	except KeyboardInterrupt:
		pass

	total = time.time() - ts

	print 'Count', count
	print 'Total', total
	print 'CPS', count / total

def _stress_testing(times = 10000, host = 'localhost', port = 2626):
	"Used internally to do some stress testings"
	c = locker(host, port)
	count = 0

	# unclock first, just in case
	for i in range(0, times):
		try:
			c.unlock(i)
		except:
			pass
	
	try:
		ts = time.time()
		for i in range(0, times):
			#c.ping('Performance Test')
			c.lock(i)
		print 'Locked', times
		for i in range(0, times):
			c.unlock(i)
			count += 1
		print 'Unlocked', times
	except KeyboardInterrupt:
		pass

	total = time.time() - ts

	print 'Count', count
	print 'Total', total
	print 'CPS', count / total



if __name__ == '__main__':
	import sys
	
	c = locker()
	
	if len(sys.argv) != 3:
		print "Use: %s cmd obj" % sys.argv[0]
		sys.exit(1)
	
	cmd = sys.argv[1]
	param = sys.argv[2]
	
	if cmd == 'perf':
		times = int(param)
		_perf_testing(times, 'localhost', 2626)
	elif cmd == 'stress':
		times = int(param)
		_stress_testing(times, 'localhost', 2626)
	elif cmd == 'ping':
		c.ping()
		print 'PONG'
	elif cmd == 'lock':
		c.lock(param)
		print 'LOCKED'
	elif cmd == 'unlock':
		c.unlock(param)
		print 'UNLOCKED'
	elif cmd == 'trylock':
		if c.trylock(param):
			print 'LOCKED'
		else:
			print 'WBLOCK'
	elif cmd == 'adopt':
		c.adopt(param)
		print 'ADOPTED'
	else:
		print 'Unknown command'
	

