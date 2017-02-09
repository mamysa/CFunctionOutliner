int main() {
	int a = 12;

	if (a == 12) { a += 1; }
	else { a += 2; }

	int filler = 0;
	extern int x; 
	if (x == 15) { a += 1; }
	else { a += x; }
	
	return a += x;
}
