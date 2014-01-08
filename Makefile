all:
	cd img && make
	jbtex2 mkworld
	jbrex2 clean

clean:
	cd img && make clean
