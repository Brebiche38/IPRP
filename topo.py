#!/usr/bin/python

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI
from mininet.util import dumpNodeConnections

class MultiSiteTopo(Topo):
	"3 hosts behind 3 connected routers"
	def build(self):
		h1 = self.addHost('H1', ip='10.0.1.1/24')
		h2 = self.addHost('H2', ip='10.0.2.2/24')
		h3 = self.addHost('H3', ip='10.0.3.3/24')
		r1 = self.addHost('R1', ip='10.0.1.10/24')
		r2 = self.addHost('R2', ip='10.0.2.20/24')
		r3 = self.addHost('R3', ip='10.0.3.30/24')
		s1 = self.addHost('S1', ip='10.1.10.1/24')
		s2 = self.addHost('S2', ip='10.2.10.2/24')
		self.addLink(h1, r1)
		self.addLink(h2, r2)
		self.addLink(h3, r3)
		self.addLink(r1, s1)
		self.addLink(r2, s1)
		self.addLink(r3, s1)
		self.addLink(r1, s2)
		self.addLink(r2, s2)
		self.addLink(r3, s2)
		
def basicTest():
	"Ping all hosts"
	net = Mininet(MultiSiteTopo())
	net.start()
	h1, h2, h3 = net.hosts[0], net.hosts[1], net.hosts[2]
	r1, r2, r3 = net.hosts[3], net.hosts[4], net.hosts[5]
	s1, s2 = net.hosts[6], net.hosts[7]
	h1.cmd("h1/conf.sh")
	h2.cmd("h2/conf.sh")
	h3.cmd("h3/conf.sh")
	s1.cmd("s1/conf.sh")
	s2.cmd("s2/conf.sh")
	r1.cmd("r1/conf.sh")
	r2.cmd("r2/conf.sh")
	r3.cmd("r3/conf.sh")
	s1.cmd("compile.sh")
	#r1.cmd("r1/iprp.sh")
	#r2.cmd("r2/iprp.sh")
	#r3.cmd("r3/iprp.sh")
	#net.startTerms()
#	CLI(net).do_xterm("S1 S2 R1 R2 R3 R1 R2 R3")
	CLI(net).do_xterm("S1 R2 R1 H2 H1")
	CLI(net)
	net.stop()

#topos = { 'multisite': MultiSiteTopo }

basicTest()