all: du du_nftw
du: du.c
	gcc -o du -std=c99 -Wall -O1 du.c
du_nftw: du_nftw.c
	gcc -o du_nftw -std=c99 -Wall -O1 du_nftw.c
clean:
	rm -f du
	rm -f du_nftw