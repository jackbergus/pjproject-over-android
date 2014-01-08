SRC=$(wildcard *.dia)
DST=$(SRC:.dia=.png)

%.png:		%.dia
		dia -e $@ $<
				
dia2pdf:	$(DST)

