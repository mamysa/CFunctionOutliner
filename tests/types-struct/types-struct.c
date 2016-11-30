#include <stdlib.h>
struct mystruct {int a; float b; };
typedef struct mystruct mystruct_t;

int main(void) {
	struct mystruct x;	
	x.a = 13; x.b = 14;

	mystruct_t y;
	y.a = 21; y.b = 90;

	struct mystruct *u = (struct mystruct *)malloc(sizeof(struct mystruct));
	u->a = 100; u->b = 101;

	mystruct_t *v = (mystruct_t *)malloc(sizeof(mystruct_t));
	v->a = 200; v->b = 201;

	int i;
	for (i = 0; i < 4; i++) {
		x.a = x.a + u->a + v->b * i;	
	}

	for (i = 0; i < 4; i++) {
		y.a = x.b + u->b + v->a * i;	
	}


	free(u);
	free(v);
	return y.a + x.b;
}
