#include <stdlib.h>
struct mystruct {int a; int b; };
int func1(const struct mystruct u, const struct mystruct v) {
	int out = 0;
	int i;
	for (i = 0; i < 4; i++) { out -= u.a + v.a; }
	for (i = 0; i < 4; i++) { out -= u.b + v.b; }

	return out;
}


int main(void) {
	const struct mystruct x = {13, 14};	
	const struct mystruct y = {10, 20};	
	int retval = func1(x, y);
	return retval;	
}
