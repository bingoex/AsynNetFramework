

All:
	make -C comm
	make -C src
	make -C example

clean:
	make clean -C comm
	make clean -C src
	make clean -C example
