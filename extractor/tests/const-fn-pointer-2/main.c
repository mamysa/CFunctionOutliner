int myfunc(int * const a) { return *a * 2; }

int main() {
	int a = 12;
	int b = 14;

	int out = 0;
	if (b == 12) { out += 1; }
	else { out += 2; }

	int (* const fnptr)(int * const) = &myfunc;
	if (a == 15) { out += 1; }
	else { out += myfunc(&a); }
	
	fnptr(&out);
	return out;
}
