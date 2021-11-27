This is a cut down version of axTLS, just for client operation.
It has been modified to return timeout status rather than
erroring, and to be able to call ssl_read asynchronously
once the handshaking is complete.

It is not used for any security sensitive purpose, but is used purely to
communicate with the ChromeCast.
