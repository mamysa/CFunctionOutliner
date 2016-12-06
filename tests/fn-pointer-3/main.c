void func1(void) {
	int a = 12;
}

int main(void) {
	void (*myfunction)(void)= &func1;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		myfunction();
		out = out + i;
	}

	return out;
}
