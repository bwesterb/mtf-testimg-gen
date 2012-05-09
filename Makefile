
generate: generate.c
	gcc generate.c -o generate `pkg-config --cflags --libs gsl libpng` \
		-O3 -Wall -DHAVE_INLINE -std=c99 -fomit-frame-pointer \
		-march=native
