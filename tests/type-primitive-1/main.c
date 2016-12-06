int main(void) {
	float  af = 12.f; float  bf = 145.f; float  cf = 0.f;
	double ad = 12.f; double bd = 145.f; double cd = 0.f;

	long al = 123; long bl = 100; long cl = 12;
	unsigned long alu = 123; unsigned long blu = 100; unsigned long clu = 12;

	int i;
	for (i = 0; i < 4; i++) {
		cf = cf + af + bf;
		cd = cd + ad + bd;
		cl = cl + al + bl;
		clu = clu + alu + blu;
	}

	return (int)cl;
}
