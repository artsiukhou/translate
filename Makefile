main:
	clang++ translate.cpp -std=c++11 -O2 -fPIC -shared -o libtranslate.so -lfastcgi-daemon2 -lmongoclient -lboost_system