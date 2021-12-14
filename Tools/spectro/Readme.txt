This directory containts the routines to operate a variety
of color instruments.

If you make use of the instrument driver code here, please note
that it is the author(s) of the code who take responsibility
for its operation. Any problems or queries regarding driving
instruments with the Argyll drivers, should be directed to
the Argyll's author(s), and not to any other party.

If there is some instrument feature or function that you
would like supported here, it is recommended that you
contact Argyll's author(s) first, rather than attempt to
modify the software yourself, if you don't have firm knowledge
of the instrument communicate protocols. There is a chance
that an instrument could be damaged by an incautious command
sequence, and the instrument companies generally cannot and
will not support developers that they have not qualified
and agreed to support.


chartread.exe	Is used to read the test chart and create
			    the chart readings file.

filmread.exe	Is used to read the film test chart and create
			    the chart readings file.

dispcal.exe		Calibrate a display

dispread.exe	Read test chart values for a display

fakeread.exe	Fake reading of a device, using a profile instead.

spec2cie.exe	Convert a spectral reading file into a CIE tristimulus file.

spotread.exe	Read spot values.

instlib.txt	Explanation of how to extract and build a standalone instrument library.
