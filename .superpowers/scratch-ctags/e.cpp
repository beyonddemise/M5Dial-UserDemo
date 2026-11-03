#line 1 "/x/B.ino"
struct Pt { int x; };
static int s1(int a) { return a; }
const char* s2() { return ""; }
struct Pt s3() { Pt p; return p; }
Pt s4(Pt p) { return p; }
unsigned long
s5(void) { return 0; }
template <typename T> T s6(T a) { return a; }
int proto(int);
class C { public: void m(); };
void C::m() {}
namespace N { void nf() {} }
