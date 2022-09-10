ucrtcringe.dll.html: README.md $(patsubst %.py, %.png, $(wildcard images/asin*.py))
	pandoc -s --toc --high=zenburn $< -t html -o $@

images/asin%.png: images/asin%.py images/utils.py
	python $< $@ /home/rbernon/Code/build-wine
images/asin.png: images/asin.py images/utils.py
	python $< $@ /home/rbernon/Code/build-wine

least-squares: images/approx.py
	python $< $@ /home/rbernon/Code/build-wine 4
.PHONY: least-squares
