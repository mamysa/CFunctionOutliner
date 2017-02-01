int myfunc(int a, int b) {
	int out = 0;
	int i;

	for (i = 0; i < 10; i++) { out += a; }
	for (i = 0; i < 14; i++) { out += b; }

	return out; 
}


int main() {
	return myfunc(3, 4);
}
