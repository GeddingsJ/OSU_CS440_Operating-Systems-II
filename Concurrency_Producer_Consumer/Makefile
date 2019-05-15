default: pdf cs444_project1_10

all: pdf cs444_project1_10 clean

pdf:
	pdflatex -synctex=1 -interaction=nonstopmode cs444_project1_10.tex

concurrency:
	gcc -Wall -o concurrency1-10 concurrency1-10.c

clean:
	rm -f *.ps *.log *.aux *.out *.dvi *.bbl *.blg *.gz *.acn *.ist *.acr *.alg *.toc *.glg *.pyg
