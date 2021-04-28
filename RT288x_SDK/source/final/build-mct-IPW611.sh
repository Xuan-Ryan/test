#!/bin/bash
__RELEASE=0

if [ -z "$1" ]; then
	echo "Start building"
elif [ $1 = 'release' ]; then
	__RELEASE=1
fi

cd ..

if [ -L 'ver' ]; then
	rm ver
fi
ln -s final/ver/mct/IPW611.ver ver

if [ -L 'kfs' ]; then
	rm kfs
fi
ln -s final/kfs/mct/IPW611 kfs

if [ ! -d 'kfs/factory' ]; then
	mkdir kfs/factory
fi

if [ ! -e .type ]; then
	echo 'RX' > .type
fi

export __RELEASE

#  build RX image
make rx
make

#  build TX image
make tx
make
