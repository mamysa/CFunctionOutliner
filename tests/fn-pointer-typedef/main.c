typedef void(*myfunction_t)(int *); 

void func1(int *a) {
	(*a) = 2 * (*a);
}

int main(void) {
	myfunction_t myfunc = &func1;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		int b = i;
		myfunc(&b);
		out = out + b;
	}

	return out;
}
