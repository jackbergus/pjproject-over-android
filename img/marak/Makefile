SRC=$(wildcard *.svg)
DST=$(SRC:.svg=.pdf)

%.pdf:		%.svg
		inkscape -f $< -A $@
				
dia2pdf:	$(DST)

