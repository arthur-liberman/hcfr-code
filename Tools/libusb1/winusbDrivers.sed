# Sed script for winusb drivers.
# Use winusb by default, and libusb0 on Win2K
     s/#DEFAULT#/LIBUSB0_DEV/
     s/#WIN2K32#/LIBUSB0_DEV/
     s/#WINXP32#/WINUSBCO_DEV/
     s/#WINXP64#/WINUSBCO_DEV/
     s/#VISTA32#/WINUSBCO_DEV/
     s/#VISTA64#/WINUSBCO_DEV/
      s/#WIN732#/WINUSBCO_DEV/
      s/#WIN764#/WINUSBCO_DEV/
