# Sed script for instruments that doen't work with winusb.
# Use ptlibusb0.sys instead for 64 bit.
     s/#DEFAULT#/LIBUSB0_DEV/
     s/#WIN2K32#/LIBUSB0_DEV/
     s/#WINXP32#/LIBUSB0_DEV/
     s/#WINXP64#/PTLIBUSB0_DEV/
     s/#VISTA32#/LIBUSB0_DEV/
     s/#VISTA64#/PTLIBUSB0_DEV/
      s/#WIN732#/LIBUSB0_DEV/
      s/#WIN764#/PTLIBUSB0_DEV/
