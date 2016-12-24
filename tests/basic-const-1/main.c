int test1(void) {
	int a = 0;
	const int myconstant_1 = 1000;
	const int myconstant_2 = 2000;
	const int myconstant_3 = 1000;

	float b = 0.0f;
	const float myfloatconst_1 = 130.f;
	const float myfloatconst_2 = 140.f;
	const float myfloatconst_3 = 150.f;

	int i;
// region start
// inputs: myconstant_1, myconstant_2, myconstant_3,
// myfloatconst_1 myfloatconst_2, i, a, b
	for (i = 0; i < 4; i++) {
		a = a + myconstant_1 + myconstant_2; 
		b = b + myfloatconst_1 + myfloatconst_2; 
	}
//region end
	
	b = myfloatconst_2 + myfloatconst_3;
	return a + (int)b; 
}

// expect myconstant_1, myconstant_2, myconstant_3, out to be inputs.
// myconstant_2 is not passed in - 
// myconstant_2 + myconstant_4;  compiles to
// %add6 = add nsw i32 %5, 3000, !dbg !123
int test2(void) {
	int out = 0;
	const int myconstant_1 = 1000;
	const int myconstant_2 = 2000;
	const int myconstant_3 = 1000;


	if (out == 30) { out += myconstant_3; }
	else { out += myconstant_2; }

	const int myconstant_4 = 1000;
	if (out == 20) { out += myconstant_1; }
	else { out += myconstant_2 + myconstant_4; }

	return out;
}

//inputs:  myconstant_1, myconstant_2, out
//outputs: myconstant_3, myconstant_4
int test3(void) {
	int out = 0;
	const int myconstant_1 = 1000;
	const int myconstant_2 = 2000;

	if (out == 30) { out += myconstant_1; }
	else { out += myconstant_2; }

	const int myconstant_3 = 1000;
	const int myconstant_4 = 1000;
	if (out == 20) { out += myconstant_1; }
	else { out += myconstant_2; }

	return out + myconstant_4;
}

//constants not initialized to literal value are treated like regular variables
//input: myconstant_1, myconstant_2, out
int test4(void) {
	int a = 12;
	int b = 4;

	int out = 0;
	const int myconstant_1 = a + b;
	const int myconstant_2 = b - a;

	if (out == 30) { out += myconstant_1; }
	else { out += myconstant_2; }

	if (out == 20) { out += myconstant_1; }
	else { out += myconstant_2; }

	return out;
}
