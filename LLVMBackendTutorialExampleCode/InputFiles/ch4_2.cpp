// clang -c ch4_2.cpp -emit-llvm -o ch4_2.bc
// /usr/local/llvm/3.1.test/cpu0/1/cmake_debug_build/bin/llc -march=cpu0 -relocation-model=pic -filetype=asm ch4_2.bc -o ch4_2.cpu0.s
// /usr/local/llvm/3.1.test/cpu0/1/cmake_debug_build/bin/llc -march=cpu0 -relocation-model=pic -filetype=obj ch4_2.bc -o ch4_2.cpu0.o

// /Users/Jonathan/llvm/3.1.test/cpu0/1/cmake_debug_build/bin/Debug/llc -march=cpu0 -relocation-model=pic -filetype=asm ch4_2.bc -o ch4_2.cpu0.s
// /Users/Jonathan/llvm/3.1.test/cpu0/1/cmake_debug_build/bin/Debug/llc -march=cpu0 -relocation-model=static -filetype=asm ch4_2.bc -o ch4_2.cpu0.static.s

int main()
{
	int a = 5;
	int b = 2;
	int c = 0;
	int d = 0;
	int e, f, g, h, i, j, k, l = 0;
	unsigned int a1 = -5, k1 = 0, f1 = 0;

	c = a + b;
	d = a - b;
	e = a * b;
	f = a / b;
//	f1 = a1 / b;
	g = (a & b);
	h = (a | b);
	i = (a ^ b);
	j = (a << 2);
	int j1 = (a1 << 2);
//	k = (a >> 2);
	k1 = (a1 >> 2);

	return c;
}

