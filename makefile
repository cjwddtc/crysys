CXXFLAGS =-std=c++14 -g -Wall -Wno-sign-compare -Wno-reorder
CFLAGS=-g -std=c11
objs=des.o md5.o network.o signdes.o database.o
all: asd.dll main.exe asd.exe man.exe qwe.exe
asd.dll:$(objs)
	g++ $(objs) -lboost_filesystem-mt -lboost_system-mt -lws2_32 -lmswsock -lsqlite3 -shared -static -o asd.dll
main.exe:main.o
	g++ asd.dll main.o -o main.exe
asd.exe:asd.o
	g++ asd.dll asd.o -o asd.exe
man.exe:man.o
	g++ asd.dll man.o -o man.exe
qwe.exe:qwe.o
	g++ asd.dll qwe.o -o qwe.exe
man.o:database.h
qwe.o:network.h
database.o:database.h
des.o: encrypt.h
md5.o :encrypt.h
signdes.o:encrypt.h
network.o:network.h encrypt.h
main.o: network.h database.h
asd.o:network.h
clean:
	rm $(objs) main.o asd.o man.o qwe.o asd.dll
