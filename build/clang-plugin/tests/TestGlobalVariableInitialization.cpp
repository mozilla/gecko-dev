#define MOZ_RUNINIT  __attribute__((annotate("moz_global_var")))
#define MOZ_CONSTINIT [[clang::require_constant_initialization]]
#define MOZ_GLOBAL_CLASS __attribute__((annotate("moz_global_class")))

// POD Type
struct POD {
  int i, j, k;
};

POD g0;

// constexpr constructor
struct ConstexprGlobal {
  int i, j, k;
  constexpr ConstexprGlobal() : i(0), j(1), k(2) {}
};

ConstexprGlobal g1;

// Global with extern constructor
struct Global {
  Global();
};

Global g2; // expected-error {{Global variable has runtime initialisation, try to remove it, make it constexpr or MOZ_CONSTINIT if possible, or as a last resort flag it as MOZ_RUNINIT.}}

// Global with extern constructor *but* marked MOZ_GLOBAL_CLASS
struct MOZ_GLOBAL_CLASS GlobalCls {
  GlobalCls();
};

GlobalCls g3;

// Global with extern constructor *but* marked MOZ_RUNINIT
struct RuninitGlobal {
  RuninitGlobal();
};

MOZ_RUNINIT RuninitGlobal g4;

// Global with constexpr constructor *but* marked MOZ_RUNINIT
struct InvalidRuninitGlobal {
  constexpr InvalidRuninitGlobal() {}
};

MOZ_RUNINIT InvalidRuninitGlobal g5; // expected-error {{Global variable flagged as MOZ_RUNINIT but actually has constinit initialisation. Consider flagging it as constexpr or MOZ_CONSTINIT instead.}}
constexpr InvalidRuninitGlobal g5a;

struct InvalidRuninitGlobal2 {
  int i;
};

MOZ_RUNINIT InvalidRuninitGlobal2 g5b; // expected-error {{Global variable flagged as MOZ_RUNINIT but actually has constant initialisation. Consider removing the annotation or (as a last resort) flagging it as MOZ_GLOBINIT.}}
InvalidRuninitGlobal2 g5c;

// Static variable with extern constructor
Global g6;  // expected-error {{Global variable has runtime initialisation, try to remove it, make it constexpr or MOZ_CONSTINIT if possible, or as a last resort flag it as MOZ_RUNINIT.}}

// Static variable with extern constructor within a function
void foo() { static Global g7; }

// Global variable with extern constructor in a namespace
namespace bar {Global g8;}  // expected-error {{Global variable has runtime initialisation, try to remove it, make it constexpr or MOZ_CONSTINIT if possible, or as a last resort flag it as MOZ_RUNINIT.}}

// Static variable with extern constructor in a class
class foobar {static Global g9;};
Global foobar::g9; // expected-error {{Global variable has runtime initialisation, try to remove it, make it constexpr or MOZ_CONSTINIT if possible, or as a last resort flag it as MOZ_RUNINIT.}}
