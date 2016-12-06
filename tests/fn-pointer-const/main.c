void func1(int *a) {
	(*a) = 2 * (*a);
}

int main(void) {
	void (*const myfunction)(int *) = &func1;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		int b = i;
		myfunction(&b);
		out = out + b;
	}

	return out;
}
