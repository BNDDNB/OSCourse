#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE (4096)
#define PAGES     (128)
#define SIZE      (PAGE_SIZE * PAGES / sizeof(int))
#define NUM_REFS  (5)

// unsigned int array[SIZE] = {0};
unsigned int array[SIZE];

int
main()
{
	printf("start of prog\n");
	unsigned int i = 0;
	unsigned int refs = 0;
	printf("first looping section\n");
	for (refs = 0; refs < NUM_REFS-1; refs++) {
		for (i=0; i<SIZE; i++) {
			array[i] = i;
		}
	}
printf("2nd looping section\n");
	for (i=0; i<SIZE; i++) {
		if (array[i] != i) {
			printf("FAILED array[%d] = %u != %d\n", i, array[i], i);
			exit(1);
		}
	}

	printf("SUCCEEDED\n");
	exit(0);
}
