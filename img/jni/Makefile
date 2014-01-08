SRC=$(wildcard *.dia)
DST=$(SRC:.dia=.png)

%.png:		%.dia
		dia $< -t png -e $@
				
dia2pdf:	$(DST)

