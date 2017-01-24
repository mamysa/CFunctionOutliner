void myfunc(int *a) { *a = *a * 2; }

int main() {
	int a = 12;
	int b = 14;

	int out = 0;
	if (b == 12) { out += 1; }
	else { out += 2; }

	void (* const fnptr)(int *) = &myfunc;
	if (a == 15) { out += 1; }
	else { out += a; }
	
	fnptr(&out);
	return out;
}
