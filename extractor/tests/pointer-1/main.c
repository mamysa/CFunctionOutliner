#include <stdlib.h>
int * vec_add(int *lhs, int *rhs, int size) {
	int *result = malloc(sizeof(int) * size);	
	int i;

	for (i = 0; i < size; i++) { result[i] = lhs[i] + rhs[i]; }
	return result;
}


int main() {
#define SIZE 4
	int *a = malloc(sizeof(int) * SIZE);	
	int *b = malloc(sizeof(int) * SIZE);	

	a[0] = 3; a[1] = 1; a[2] = 5; a[3] = 4;
	b[0] = 2; b[1] = 3; b[2] = 9; b[3] = 1;

	int *result = vec_add(a, b, SIZE);
	int i;
	int out = 0;
	for (i = 0; i < SIZE; i++) {
		out += result[i];	
	}

	free(a);
	free(b);
	free(result);
	return out;
}
