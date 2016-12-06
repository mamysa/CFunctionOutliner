#include <stdlib.h>

int main(void) {
	int a = 0;
	const int myconstant_1 = 1000;
	const int myconstant_2 = 2000;
	const int myconstant_3 = 1000;

	float b = 0.0f;
	const float myfloatconst_1 = 130.f;
	const float myfloatconst_2 = 140.f;
	const float myfloatconst_3 = 150.f;

	int i;
	for (i = 0; i < 4; i++) {
		a = a + myconstant_1 + myconstant_2; 
		b = b + myfloatconst_1 + myfloatconst_2; 
	}
	
	b = myfloatconst_2 + myfloatconst_3;
	return a + (int)b; 
}
