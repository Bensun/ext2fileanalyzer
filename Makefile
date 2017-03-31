all: run
run:
	gcc generator.c -o generator -ggdb
	./generator disk-image
	python analyzer.py
