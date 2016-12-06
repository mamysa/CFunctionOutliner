#include <stdlib.h>
int main(void) {
	int a = 0;
	const int myconstant_1 = 3000;
	const int myconstant_2 = 1000;
	const int myconstant_3 = 3000;

	float b = 0.0f;
	const float myfloatconst_1 = 130.f;
	const float myfloatconst_2 = 140.f;
	const float myfloatconst_3 = 140.f;

	char condition = 0;
	if (condition == 1) {
		a = myconstant_1 + myconstant_2; 
	} else {
		b = myfloatconst_1 + myfloatconst_2;
	}

	a = a + (int)b + myconstant_3;
	b = b + (float)a + myfloatconst_3;
	return a - (int)b; 
}
