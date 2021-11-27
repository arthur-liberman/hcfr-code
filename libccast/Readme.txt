This directory contains the library for sending
rasters to the Google ChromeCast.

Hierarchy:

	ccast.c			Top level actions & receive thread
	ccmes.c			Message handling, uses protobuf
	chan/			protobuf encoding
	ccpacket.c		socket write/read

	ccmdns.c		MDNS sign on
	axTLS			Standalone SSL/TLS library
