int myfunction(int a,
		int b, 
		int c ) {
	int sum = 0;
	for (; a < b; a++) {
		sum += c;	
	}
	return sum;
}


int main(void) {
	return myfunction(1, 6, 3);
}
