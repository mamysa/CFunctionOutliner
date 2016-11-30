#include <stdlib.h>
struct mystruct {int a; int b; };
int main(void) {
	const struct mystruct x = {13, 14};	
	const struct mystruct y = {10, 20};	
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		out -= x.a + y.a;
	}

	for (i = 0; i < 4; i++) {
		out -= x.b + y.b;
	}


	return out; 
}
