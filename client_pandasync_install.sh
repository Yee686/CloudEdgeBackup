# get PandaSync client from WSL and install it

echo "--Installing PandaSync client..."
make reconfigure

echo "--Starting make..."
make

echo "--Starting make install..."
make install

echo "--PandaSync client installed."
