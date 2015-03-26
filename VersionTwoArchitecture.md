# Introduction #

A short doc of how the new version 2.0 architecture will sort out.

# Details #

## The components ##

### grossd ###

Just the core, bloom filters and replication. A super simple client-server protocol for clients to query the bloom filters.

### gservd ###

Extended postfix policyd (multiple queries per tcp connection) and sjsms protocols. Implements the checks including dns queries. Queries grossd via internal protocol

### gmilterd ###

A sendmail milter. Queries gservd via sjsms protocol.

### gpolicyd ###

A postfix policy proxy server. A super simple (and stable) miniclient to query multiple gservd daemons. A generic policy proxy, optional connection pooling.

## The result ##

Massive scalability due to horizontal scaling of gservd daemons. Stability and increased availability with postfix policy protocol. Very small sites can run everything on a single server, and run just grossd and gservd. A major ISP could run a replicated pair of grossd servers and a gservd on each MTA host.

gservd, or optional parts of it, could be written with e.g. Python or Lua.