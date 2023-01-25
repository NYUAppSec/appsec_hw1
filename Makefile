default: giftcardreader

giftcardreader: giftcardreader.c giftcard.h
	gcc -g -o giftcardreader giftcardreader.c

asan: giftcardreader.c giftcard.h
	gcc -fsanitize=address -g -o giftcardreader giftcardreader.c

test: giftcardreader
	./runtests.sh

# .PHONY tells make to always assume this target needs
# to be rebuilt
.PHONY: clean
clean:
	rm -f *.o giftcardreader
