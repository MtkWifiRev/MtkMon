OLD_PWD=$(pwd)
cd $(dirname ${BASH_SOURCE[0]})

export ARCH=nds32

export HOSTUNAME=$(uname -s)
export PLATFORMUNAME=$(uname -m)

export NEXMON_ROOT=$(pwd)

export CC=$NEXMON_ROOT/buildtools/gcc-12.2.0-nolibc/nds32le-linux/bin/nds32le-linux-
export CCPLUGIN=$NEXMON_ROOT/buildtools/gcc-nexmon-plugin/nexmon.so

export Q=@

cd "$OLD_PWD"
