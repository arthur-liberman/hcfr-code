HCFR
----

Windows Users
-------------

Welcome to the HCFR project.

To compile the source, open the solution file ColorHCFR2010.sln in Visual Studio 2010 or greater (for newer VS versions, allow any project conversions to happen).

The addition of HDR support has two new prerequisites:
1) Download the latest NVAPI, and place the "NVAPI" directory under the "Tools" directory.
2) a) Make sure that you have Windows 10 SDK version 10.0.15063.0 or greater installed.
   b) Go to "libHDR" directory.
   c) Create a file named "win10sdk.h", and add #include for 'dxgi1_5.h', 'd3d12.h' and 'vector' defined in the Windows 10 SDK.

Select ColorHCFR as main project and build. Make sure that you build in Release configuration.

In order to run you may need to copy the dlls from the devlib directory into your Release directories.

Mac Users
---------

Instructions coming