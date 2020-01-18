HOME=~
EMSDKDIR=`pwd`/emsdk

if [ ! -s .emscripten ]
then
	echo "import os" > .emscripten
	echo "LLVM_ROOT='${EMSDKDIR}/clang/e1.38.21_64bit'" >> .emscripten
	echo "EMSCRIPTEN_NATIVE_OPTIMIZER='${EMSDKDIR}/clang/e1.38.21_64bit/optimizer'" >> .emscripten
	echo "BINARYEN_ROOT='${EMSDKDIR}/clang/e1.38.21_64bit/binaryen'" >> .emscripten
	echo "NODE_JS='/${EMSDKDIR}/node/8.9.1_64bit/bin/node'" >> .emscripten
	echo "EMSCRIPTEN_ROOT='${EMSDKDIR}/emscripten/1.38.21'" >> .emscripten
	echo "SPIDERMONKEY_ENGINE = ''" >> .emscripten
	echo "V8_ENGINE = ''" >> .emscripten
	echo "TEMP_DIR = '/tmp'" >> .emscripten
	echo "COMPILER_ENGINE = NODE_JS" >> .emscripten
	echo "JS_ENGINES = [NODE_JS]" >> .emscripten
fi

export PATH="${EMSDKDIR}:${EMSDKDIR}/clang/e1.38.21_64bit:${EMSDKDIR}/node/8.9.1_64bit/bin:${EMSDKDIR}/emscripten/1.38.21:${HOME}/bin:${HOME}/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games"
export EMSDK="${EMSDKDIR}"
export EM_CONFIG="${EMSDKDIR}/.emscripten"
export LLVM_ROOT="${EMSDKDIR}/clang/e1.38.21_64bit"
export EMSCRIPTEN_NATIVE_OPTIMIZER="${EMSDKDIR}/clang/e1.38.21_64bit/optimizer"
export BINARYEN_ROOT="${EMSDKDIR}/clang/e1.38.21_64bit/binaryen"
export EMSDK_NODE="${EMSDKDIR}/node/8.9.1_64bit/bin/node"
export EMSCRIPTEN="${EMSDKDIR}/emscripten/1.38.21"
