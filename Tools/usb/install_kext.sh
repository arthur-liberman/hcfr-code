# Install OS X HCFR codeless kernel extension
echo "Installing OS X HCFR codeless kernel extension"
sudo cp -R Argyll.kext /System/Library/Extensions/
sudo chown -R root:wheel /System/Library/Extensions/Argyll.kext
sudo kextload -t  /System/Library/Extensions/Argyll.kext
echo "Reboot to finish installation" 
