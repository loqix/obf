#ifndef ithare_obf_obfuscate_h_included
#define ithare_obf_obfuscate_h_included

#include "impl/obf_common.h"
#include "impl/obf_prng.h"
#include "impl/obf_injection.h"
#include "impl/obf_anti_debug.h"
#include "impl/obf_literal.h"

//Usage: 
//  1. Use OBF?() throughout your code to indicate the need for obfuscation
//  1a. OBFX() roughly means 'add no more than 10^(X/2) CPU cycles for obfuscation of this variable'
//      i.e. OBF2() adds up to 10 CPU cycles, OBF3() - up to 30 CPU cycles, 
//           and OBF5() - up to 300 CPU cycles
//  1b. To obfuscate literals, use OBF?I() (for integral literals) and OBF?S() (for string literals)
//  1c. See ../test/official.cpp for examples
//  2. compile your code without -DITHARE_OBF_SEED for debugging and during development
//  3. compile with -DITHARE_OBF_SEED=0x<really-random-64-bit-seed> for deployments

// LIST OF supported #defines:
// MAIN SWITCH:
//   ITHARE_OBF_SEED 0x<some-random-64-bit-number>
//     if not specified - no obfuscation happens
// TODO: add ITHARE_OBF_DISABLED (to work around some compiler's performance hit even when no ITHARE_OBF_SEED is specified)  
//
// COMMON ONES:
//   ITHARE_OBF_SEED2
//   ITHARE_OBF_NO_ANTI_DEBUG
//   ITHARE_OBF_NO_IMPLICIT_ANTI_DEBUG (disables using anti debug in generated obfuscations, but still allows to read it)
//
// DEBUG-ONLY
//    ITHARE_OBF_COMPILE_TIME_TESTS
// DEBUG-ONLY; NOT to be used in production
//   ITHARE_OBF_DBG_ENABLE_DBGPRINT
//   ITHARE_OBF_DBG_RUNTIME_CHECKS
//   ITHARE_OBF_DEBUG_ANTI_DEBUG_ALWAYS_FALSE (to disable anti-debug - use ITHARE_OBF_NO_ANTI_DEBUG or 


#ifdef ITHARE_OBF_SEED

namespace ithare {
	namespace obf {

	//IMPORTANT: ANY API CHANGES MUST BE MIRRORED in obf_literal_dbg<>
	template<class T_, T_ C_, ITHARE_OBF_SEEDTPARAM seed, OBFCYCLES cycles>
	class obf_literal {
		static_assert(std::is_integral<T_>::value);
		using T = typename std::make_unsigned<T_>::type;//from this point on, unsigned only
		static constexpr T C = (T)C_;

		using Context = ObfLiteralContext<T, ITHARE_OBF_NEW_PRNG(seed, 1),cycles>;
		struct InjectionRequirements {
			static constexpr size_t exclude_version = size_t(-1);
			static constexpr bool is_constexpr = true;
			static constexpr bool only_bijections = false;
			static constexpr bool no_substrate_size_increase = false;
			static constexpr bool cross_platform_only = false;//currently there seems to be need to ensure cross-platform compatibility for literals
		};
		using Injection = obf_injection<T, Context, ITHARE_OBF_NEW_PRNG(seed, 2), cycles,InjectionRequirements>;
	public:
		ITHARE_OBF_FORCEINLINE constexpr obf_literal() : val(Injection::template injection<ITHARE_OBF_NEW_PRNG(seed, 3)>(C)) {
		}
		ITHARE_OBF_FORCEINLINE T value() const {
			return Injection::template surjection<ITHARE_OBF_NEW_PRNG(seed, 4)>(val);
		}
		ITHARE_OBF_FORCEINLINE operator T() const {
			return value();
		}

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
		static void dbgPrint(size_t offset = 0, const char* prefix = "") {
			std::cout << std::string(offset, ' ') << prefix << "obf_literal<"<<obf_dbgPrintT<T>()<<"," << C << "," << obf_dbgPrintSeed<seed>() << "," << cycles << ">" << std::endl;
			Injection::dbgPrint(offset + 1);
		}
#endif
	private:
		typename Injection::return_type val;
	};

	//ObfVarContext
	template<class T, ITHARE_OBF_SEEDTPARAM seed,OBFCYCLES cycles>
	struct ObfVarContext {
		using Type = T;
		constexpr static OBFCYCLES context_cycles = 0;
		constexpr static OBFCYCLES calc_cycles(OBFCYCLES inj, OBFCYCLES surj) {
			return inj + surj;//for variables, BOTH injection and surjection are executed in runtime
		}

		constexpr static OBFCYCLES literal_cycles = std::min(cycles/2,50);//TODO: justify (or define?)
		template<class T2, T2 C, ITHARE_OBF_SEEDTPARAM seed2>
		struct literal {
			using LiteralContext = ObfLiteralContext<T2, seed, literal_cycles>;
			using type = obf_literal_ctx<T2, C, LiteralContext, seed2, literal_cycles>;
		};

		template<ITHARE_OBF_SEEDTPARAM seed2>
		ITHARE_OBF_FORCEINLINE static constexpr T final_injection(T x) {
			return x;
		}
		template<ITHARE_OBF_SEEDTPARAM seed2>
		ITHARE_OBF_FORCEINLINE static constexpr T final_surjection(	T y) {
			return y;
		}

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
		static void dbgPrint(size_t offset = 0, const char* prefix = "") {
			std::cout << std::string(offset, ' ') << prefix << "ObfVarContext<" << obf_dbgPrintT<T>() << ">" << std::endl;
		}
#endif
	};
	template<class T, class T0, ITHARE_OBF_SEEDTPARAM seed0, OBFCYCLES cycles0, ITHARE_OBF_SEEDTPARAM seed, OBFCYCLES cycles>
	struct ObfRecursiveContext<T, ObfVarContext<T0,seed0,cycles0>, seed, cycles> {
		using recursive_context_type = ObfVarContext<T,seed,cycles>;
		using intermediate_context_type = ObfVarContext<T,seed,cycles>;
	};

	//obf_var
	//IMPORTANT: ANY API CHANGES MUST BE MIRRORED in obf_var_dbg<>
	template<class T_, ITHARE_OBF_SEEDTPARAM seed, OBFCYCLES cycles,OBFFLAGS flags=0>
	class obf_var {
		static_assert(std::is_integral<T_>::value);
		using T = typename std::make_unsigned<T_>::type;//from this point on (and down the hierarchy), unsigned only
		//using TTraits = ObfTraits<T>;

		using Context = ObfVarContext<T, ITHARE_OBF_NEW_PRNG(seed, 1), cycles>;
		struct InjectionRequirements {
			static constexpr size_t exclude_version = size_t(-1);
			static constexpr bool is_constexpr = false;
			static constexpr bool only_bijections = false;
			static constexpr bool no_substrate_size_increase = false;
			static constexpr bool cross_platform_only = flags & obf_flag_cross_platform_only;
		};
		using Injection = obf_injection<T, Context, ITHARE_OBF_NEW_PRNG(seed, 2), cycles, InjectionRequirements>;

	public:
		ITHARE_OBF_FORCEINLINE obf_var(T_ t) : val(Injection::template injection<ITHARE_OBF_NEW_PRNG(seed, 2)>(T(t))) {
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var(obf_var<T2, seed2, cycles2> t) : val(Injection::template injection<ITHARE_OBF_COMBINED_PRNG(seed,seed2)>(T(T_(t.value())))) {//TODO: randomized injection implementation
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var(obf_literal<T2, C2, seed2, cycles2> t) : val(Injection::template injection<ITHARE_OBF_COMBINED_PRNG(seed,seed2)>(T(T_(t.value())))) {//TODO: randomized injection implementation
		}
		ITHARE_OBF_FORCEINLINE obf_var& operator =(T_ t) {
			val = Injection::template injection<ITHARE_OBF_NEW_PRNG(seed, 3)>(T(t));//TODO: different implementations of the same injection in different contexts
			return *this;
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator =(obf_var<T2, seed2, cycles2> t) {
			val = Injection::template injection<ITHARE_OBF_NEW_PRNG(seed, 4)>(T(T_(t.value())));//TODO: different implementations of the same injection in different contexts
			return *this;
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator =(obf_literal<T2, C2, seed2, cycles2> t) {
			ITHARE_OBF_DECLAREPRNG_INFUNC seedc = ITHARE_OBF_COMBINED_PRNG(seed,seed2);
			val = Injection::template injection<seedc>(T(T_(t.value())));//TODO: different implementations of the same injection in different contexts
			return *this;
		}
		ITHARE_OBF_FORCEINLINE T_ value() const {
			return T_(Injection::template surjection<ITHARE_OBF_NEW_PRNG(seed, 5)>(val));
		}

		ITHARE_OBF_FORCEINLINE operator T_() const { return value(); }
		ITHARE_OBF_FORCEINLINE obf_var& operator ++() { 
			if constexpr(Injection::injection_caps & obf_injection_has_add_mod_max_value_ex) {
				typename Injection::return_type ret = Injection::template injected_add_mod_max_value_ex<ITHARE_OBF_NEW_PRNG(seed, 6)>(val,1);
				ITHARE_OBF_DBG_CHECK_SHORTCUT("++",ret,Injection::template injection<seed>(Injection::template surjection<seed>(val)+1));
				val = ret;
			}
			else {
				*this = value() + 1; 
			}
			return *this;
		}
		ITHARE_OBF_FORCEINLINE obf_var& operator --() { *this = value() - 1; return *this; }
		ITHARE_OBF_FORCEINLINE obf_var operator++(int) { obf_var ret = obf_var(value());  *this = value() + 1; return ret; }
		ITHARE_OBF_FORCEINLINE obf_var operator--(int) { obf_var ret = obf_var(value());  *this = value() + 1; return ret; }

		template<class T2>
		ITHARE_OBF_FORCEINLINE bool operator <(T2 t) { return value() < t; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE bool operator >(T2 t) { return value() > t; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE bool operator ==(T2 t) { return value() == t; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE bool operator !=(T2 t) { return value() != t; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE bool operator <=(T2 t) { return value() <= t; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE bool operator >=(T2 t) { return value() >= t; }

		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator <(obf_var<T2, seed2, cycles2> t) {
			return value() < t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator >(obf_var<T2, seed2, cycles2> t) {
			return value() > t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator ==(obf_var<T2, seed2, cycles2> t) {
			return value() == t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator !=(obf_var<T2, seed2, cycles2> t) {
			return value() != t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator <=(obf_var<T2, seed2, cycles2> t) {
			return value() <= t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator >=(obf_var<T2, seed2, cycles2> t) {
			return value() >= t.value();
		}

		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator <(obf_literal<T2, C2, seed2, cycles2> t) {
			return value() < t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator >(obf_literal<T2, C2, seed2, cycles2> t) {
			return value() > t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator ==(obf_literal<T2, C2, seed2, cycles2> t) {
			return value() == t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator !=(obf_literal<T2, C2, seed2, cycles2> t) {
			return value() != t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator <=(obf_literal<T2, C2, seed2, cycles2> t) {
			return value() <= t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE bool operator >=(obf_literal<T2, C2, seed2, cycles2> t) {
			return value() >= t.value();
		}

		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var& operator +=(T2 t) { *this = value() + t; return *this; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var& operator -=(T2 t) { *this = value() - t; return *this; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var& operator *=(T2 t) { *this = value() * t; return *this; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var& operator /=(T2 t) { *this = value() / t; return *this; }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var& operator %=(T2 t) { *this = value() % t; return *this; }

		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator +=(obf_var<T2, seed2, cycles2> t) {
			return *this += t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator -=(obf_var<T2, seed2, cycles2> t) {
			return *this -= t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator *=(obf_var<T2, seed2, cycles2> t) {
			return *this *= t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator /=(obf_var<T2, seed2, cycles2> t) {
			return *this /= t.value();
		}
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator %=(obf_var<T2, seed2, cycles2> t) {
			return *this %= t.value();
		}

		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator +=(obf_literal<T2, C2, seed2, cycles2> t) {
			return *this += t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator -=(obf_literal<T2, C2, seed2, cycles2> t) {
			return *this -= t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator *=(obf_literal<T2, C2, seed2, cycles2> t) {
			return *this *= t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator /=(obf_literal<T2, C2, seed2, cycles2> t) {
			return *this /= t.value();
		}
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var& operator %=(obf_literal<T2, C2, seed2, cycles2> t) {
			return *this %= t.value();
		}

		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var operator +(T2 t) { return obf_var(value()+t); }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var operator -(T2 t) { return obf_var(value() - t); }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var operator *(T2 t) { return obf_var(value() * t); }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var operator /(T2 t) { return obf_var(value() / t); }
		template<class T2>
		ITHARE_OBF_FORCEINLINE obf_var operator %(T2 t) { return obf_var(value() % t); }
		
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator +(obf_var<T2,seed2,cycles2> t) { return obf_var(value() + t.value()); }
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator -(obf_var<T2, seed2, cycles2> t) { return obf_var(value() - t.value()); }
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator *(obf_var<T2, seed2, cycles2> t) { return obf_var(value() * t.value()); }
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator /(obf_var<T2, seed2, cycles2> t) { return obf_var(value() / t.value()); }
		template<class T2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator %(obf_var<T2, seed2, cycles2> t) { return obf_var(value() % t.value()); }

		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator +(obf_literal<T2, C2, seed2, cycles2> t) { return obf_var(value() + t.value()); }
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator -(obf_literal<T2, C2, seed2, cycles2> t) { return obf_var(value() - t.value()); }
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator *(obf_literal<T2, C2, seed2, cycles2> t) { return obf_var(value() * t.value()); }
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator /(obf_literal<T2, C2, seed2, cycles2> t) { return obf_var(value() / t.value()); }
		template<class T2, T2 C2, ITHARE_OBF_SEEDTPARAM seed2, OBFCYCLES cycles2>
		ITHARE_OBF_FORCEINLINE obf_var operator %(obf_literal<T2, C2, seed2, cycles2> t) { return obf_var(value() % t.value()); }

		//TODO: bitwise

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
		static void dbgPrint(size_t offset = 0, const char* prefix = "") {
			std::cout << std::string(offset, ' ') << prefix << "obf_var<" << obf_dbgPrintT<T>() << "," << obf_dbgPrintSeed<seed>() <<","<<cycles<<">" << std::endl;
			Injection::dbgPrint(offset+1);
		}
#endif

	private:
		typename Injection::return_type val;
	};

	//IMPORTANT: ANY API CHANGES MUST BE MIRRORED in obf_str_literal_dbg<>
	template<ITHARE_OBF_SEEDTPARAM seed, OBFCYCLES cycles, char... C>//TODO! - wchar_t
	struct obf_str_literal {
		//TODO: consider using different contexts beyond current (effectively global var)
		static_assert(sizeof(char) == 1);
		static constexpr size_t origSz = sizeof...(C);
		static constexpr char const str[sizeof...(C)+1] = { C...,'\0' };
		static constexpr size_t sz = obf_strlen(str);
		static_assert(sz > 0);
		static_assert(sz <= 32);
		static constexpr size_t sz4 = (sz+3)/ 4;
		static_assert(sz4 > 0);
		static_assert(sz4 <= 8);//corresponds to max literal = 32, TODO: more later
		static constexpr uint32_t FILLER = uint32_t(ITHARE_OBF_RANDOM_UINT32(seed,1));

		constexpr static std::array<ObfDescriptor, 8> split{
			ObfDescriptor(true,0,100),//Injection0
			ObfDescriptor(true,0,sz4>1?100:0),//Injection1
			ObfDescriptor(true,0,sz4>2 ? 100 : 0),//Injection2
			ObfDescriptor(true,0,sz4>3 ? 100 : 0),//Injection3
			ObfDescriptor(true,0,sz4>4 ? 100 : 0),//Injection4
			ObfDescriptor(true,0,sz4>5 ? 100 : 0),//Injection5
			ObfDescriptor(true,0,sz4>6 ? 100 : 0),//Injection6
			ObfDescriptor(true,0,sz4>7 ? 100 : 0),//Injection7
		};
		static constexpr auto splitCycles = obf_random_split<ITHARE_OBF_NEW_PRNG(seed, 2)>(cycles, split);
		static constexpr OBFCYCLES split0 = splitCycles[0];
		static constexpr OBFCYCLES split1 = splitCycles[1];
		static constexpr OBFCYCLES split2 = splitCycles[2];
		static constexpr OBFCYCLES split3 = splitCycles[3];
		static constexpr OBFCYCLES split4 = splitCycles[4];
		static constexpr OBFCYCLES split5 = splitCycles[5];
		static constexpr OBFCYCLES split6 = splitCycles[6];
		static constexpr OBFCYCLES split7 = splitCycles[7];

		struct InjectionRequirements {
			static constexpr size_t exclude_version = size_t(-1);
			static constexpr bool is_constexpr = true;
			static constexpr bool only_bijections = true;
			static constexpr bool no_substrate_size_increase = false;
			static constexpr bool cross_platform_only = false;//currently there seems to be no need to ensure cross-platform compatibility for literals 
		};

		using Injection0 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 3), std::max(split0,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection0::return_type) == sizeof(uint32_t));//only_bijections
		using Injection1 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 4), std::max(split1,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection1::return_type) == sizeof(uint32_t));//only_bijections
		using Injection2 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 5), std::max(split2,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection2::return_type) == sizeof(uint32_t));//only_bijections
		using Injection3 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 6), std::max(split3,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection3::return_type) == sizeof(uint32_t));//only_bijections
		using Injection4 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 7), std::max(split4,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection4::return_type) == sizeof(uint32_t));//only_bijections
		using Injection5 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 8), std::max(split5,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection5::return_type) == sizeof(uint32_t));//only_bijections
		using Injection6 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 9), std::max(split6,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection6::return_type) == sizeof(uint32_t));//only_bijections
		using Injection7 = obf_injection<uint32_t, ObfZeroLiteralContext<uint32_t>, ITHARE_OBF_NEW_PRNG(seed, 10), std::max(split7,2), InjectionRequirements>;
		static_assert(sizeof(typename Injection7::return_type) == sizeof(uint32_t));//only_bijections

		ITHARE_OBF_FORCEINLINE static constexpr uint32_t little_endian4(const char* str, size_t offset) {//TODO: BIG-ENDIAN
			//replacement for non-constexpr return *(uint32_t*)(str + offset);
			return str[offset] | (uint32_t(str[offset + 1]) << 8) | (uint32_t(str[offset + 2]) << 16) | (uint32_t(str[offset + 3]) << 24);
		}
		ITHARE_OBF_FORCEINLINE static constexpr uint32_t last4(char const str[origSz], size_t offset, uint32_t filler) {
			assert(origSz > offset);
			size_t delta = origSz - offset;
			assert(delta <= 3);
			char buf[4] = {};
			size_t i = 0;
			for (; i < delta; ++i) {
				buf[i] = str[origSz + i];
			}
			for (; i < 4; ++i) {
				buf[i] = char(filler);
				filler >>= 8;
			}
			return little_endian4(buf,0);
		}
		ITHARE_OBF_FORCEINLINE static constexpr uint32_t get4(char const str[origSz], size_t offset) {
			assert(offset < origSz);
			if (offset + 4 < origSz)
				return little_endian4(str, offset);
			else
				return last4(str, offset,FILLER);
		}
		ITHARE_OBF_FORCEINLINE static constexpr std::array<uint32_t, sz4> str_obf() {
			std::array<uint32_t, sz4> ret = {};
			ret[0] = Injection0::template injection<ITHARE_OBF_NEW_PRNG(seed,3)>(get4(str,0));
			if constexpr(sz4 > 1)
				ret[1] = Injection1::template injection<ITHARE_OBF_NEW_PRNG(seed,4)>(get4(str, 4));
			if constexpr(sz4 > 2)
				ret[2] = Injection2::template injection<ITHARE_OBF_NEW_PRNG(seed,5)>(get4(str, 8));
			if constexpr(sz4 > 3)
				ret[3] = Injection3::template injection<ITHARE_OBF_NEW_PRNG(seed,6)>(get4(str, 12));
			if constexpr(sz4 > 4)
				ret[4] = Injection4::template injection<ITHARE_OBF_NEW_PRNG(seed,7)>(get4(str, 16));
			if constexpr(sz4 > 5)
				ret[5] = Injection5::template injection<ITHARE_OBF_NEW_PRNG(seed,8)>(get4(str, 20));
			if constexpr(sz4 > 6)
				ret[6] = Injection6::template injection<ITHARE_OBF_NEW_PRNG(seed,9)>(get4(str, 24));
			if constexpr(sz4 > 7)
				ret[7] = Injection7::template injection<ITHARE_OBF_NEW_PRNG(seed,10)>(get4(str, 28));
			return ret;
		}

		static constexpr std::array<uint32_t, sz4> strC = str_obf();

		static std::array<uint32_t, sz4> c;//TODO: volatile
		ITHARE_OBF_FORCEINLINE std::string value() const {
			char buf[sz4 * 4];
			*(uint32_t*)(buf + 0) = Injection0::template surjection<ITHARE_OBF_NEW_PRNG(seed,11)>(c[0]);
			if constexpr(sz4 > 1)
				*(uint32_t*)(buf + 4) = Injection1::template surjection<ITHARE_OBF_NEW_PRNG(seed,12)>(c[1]);
			if constexpr(sz4 > 2)
				*(uint32_t*)(buf + 8) = Injection2::template surjection<ITHARE_OBF_NEW_PRNG(seed,13)>(c[2]);
			if constexpr(sz4 > 3)
				*(uint32_t*)(buf + 12) = Injection3::template surjection<ITHARE_OBF_NEW_PRNG(seed,14)>(c[3]);
			if constexpr(sz4 > 4)
				*(uint32_t*)(buf + 16) = Injection4::template surjection<ITHARE_OBF_NEW_PRNG(seed,15)>(c[4]);
			if constexpr(sz4 > 5)
				*(uint32_t*)(buf + 20) = Injection5::template surjection<ITHARE_OBF_NEW_PRNG(seed,16)>(c[5]);
			if constexpr(sz4 > 6)
				*(uint32_t*)(buf + 24) = Injection6::template surjection<ITHARE_OBF_NEW_PRNG(seed,17)>(c[6]);
			if constexpr(sz4 > 7)
				*(uint32_t*)(buf + 28) = Injection7::template surjection<ITHARE_OBF_NEW_PRNG(seed,18)>(c[7]);
			return std::string(buf,sz);
		}
		ITHARE_OBF_FORCEINLINE operator std::string() const {
			return value();
		}

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
		static void dbgPrint(size_t offset = 0, const char* prefix = "") {
			std::cout << std::string(offset, ' ') << prefix << "obf_str_literal<'" << str << "'," << obf_dbgPrintSeed<seed>() << "," << cycles << ">" << std::endl;
			Injection0::dbgPrint(offset + 1, "Injection0:");
			if constexpr(sz4 > 1)
				Injection1::dbgPrint(offset+1,"Injection1:");
			if constexpr(sz4 > 2)
				Injection2::dbgPrint(offset + 1, "Injection2:");
			if constexpr(sz4 > 3)
				Injection3::dbgPrint(offset + 1, "Injection3:");
			if constexpr(sz4 > 4)
				Injection4::dbgPrint(offset + 1, "Injection4:");
			if constexpr(sz4 > 5)
				Injection5::dbgPrint(offset + 1, "Injection5:");
			if constexpr(sz4 > 6)
				Injection6::dbgPrint(offset + 1, "Injection6:");
			if constexpr(sz4 > 7)
				Injection7::dbgPrint(offset + 1, "Injection7:");
		}
#endif
	};

	template<ITHARE_OBF_SEEDTPARAM seed, OBFCYCLES cycles, char... C>
	std::array<uint32_t, obf_str_literal<seed,cycles,C...>::sz4> obf_str_literal<seed,cycles,C...>::c = strC;

	//USER-LEVEL:
	/*think about it further //  obfN<> templates
	template<class T,OBFSEED seed>
	class obf0 {
		using Base = obf_var<T, seed, obf_exp_cycles(0)>;

	public:
		obf0() : val() {}
		obf0(T x) : val(x) {}
		obf0 operator =(T x) { val = x; return *this; }
		operator T() const { return val.value(); }

	private:
		Base val;
	};*/

	ITHARE_OBF_FORCEINLINE void obf_init() {
		obf_literal_init();
	}
#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
	inline void obf_dbgPrint() {
		std::cout << "OBF_CONST_A=" << int(OBF_CONST_A) << " OBF_CONST_B=" << int(OBF_CONST_B) << " OBF_CONST_C=" << int(OBF_CONST_C) << std::endl;
		//auto c = obf_const_x(obf_compile_time_prng(ITHARE_OBF_SEED^UINT64_C(0xfb2de18f982a2d55), 1), obf_const_C_excluded);
	}
#endif

}//namespace obf
}//namespace ithare

 //macros; DON'T belong to any namespace...
#define ITHARE_OBFS_HELPER(seed,cycles,s) ithare::obf::obf_str_literal<seed,cycles,(sizeof(s)>0?s[0]:'\0'),(sizeof(s)>1?s[1]:'\0'),(sizeof(s)>2?s[2]:'\0'),(sizeof(s)>3?s[3]:'\0'),\
							(sizeof(s)>4?s[4]:'\0'),(sizeof(s)>5?s[5]:'\0'),(sizeof(s)>6?s[6]:'\0'),(sizeof(s)>7?s[7]:'\0'),\
							(sizeof(s)>8?s[8]:'\0'),(sizeof(s)>9?s[9]:'\0'),(sizeof(s)>10?s[10]:'\0'),(sizeof(s)>11?s[11]:'\0'),\
							(sizeof(s)>12?s[12]:'\0'),(sizeof(s)>13?s[13]:'\0'),(sizeof(s)>14?s[14]:'\0'),(sizeof(s)>15?s[15]:'\0'),\
							(sizeof(s)>16?s[16]:'\0'),(sizeof(s)>17?s[17]:'\0'),(sizeof(s)>18?s[18]:'\0'),(sizeof(s)>19?s[19]:'\0'),\
							(sizeof(s)>20?s[20]:'\0'),(sizeof(s)>21?s[21]:'\0'),(sizeof(s)>22?s[22]:'\0'),(sizeof(s)>23?s[23]:'\0'),\
							(sizeof(s)>24?s[24]:'\0'),(sizeof(s)>25?s[25]:'\0'),(sizeof(s)>26?s[26]:'\0'),(sizeof(s)>27?s[27]:'\0'),\
							(sizeof(s)>28?s[28]:'\0'),(sizeof(s)>29?s[29]:'\0'),(sizeof(s)>30?s[30]:'\0'),(sizeof(s)>31?s[31]:'\0'),\
							(sizeof(s)>32?s[32]:'\0')/*one extra to generate an error if we're over*/>

//direct use of __LINE__ doesn't count as constexpr in MSVC - don't ask why...
//  AND we DO want to align other compilers with MSVC at least for ITHARE_OBF_CONSISTENT_XPLATFORM_IMPLICIT_SEEDS

//along the lines of https://stackoverflow.com/questions/19343205/c-concatenating-file-and-line-macros:
#define ITHARE_OBF_S1(x) #x
#define ITHARE_OBF_S2(x) ITHARE_OBF_S1(x)
#define ITHARE_OBF_LOCATION __FILE__ " : " ITHARE_OBF_S2(__LINE__)

#define ITHARE_OBF0(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+0)>
#define ITHARE_OBF1(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+1)>
#define ITHARE_OBF2(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+2)>
#define ITHARE_OBF3(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+3)>
#define ITHARE_OBF4(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+4)>
#define ITHARE_OBF5(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+5)>
#define ITHARE_OBF6(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+6)>

#define ITHARE_OBF0I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+0)>()
#define ITHARE_OBF1I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+1)>()
#define ITHARE_OBF2I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+2)>()
#define ITHARE_OBF3I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+3)>()
#define ITHARE_OBF4I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+4)>()
#define ITHARE_OBF5I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+5)>()
#define ITHARE_OBF6I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+6)>()

#define ITHARE_OBF0S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+0),s)()
#define ITHARE_OBF1S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+1),s)()
#define ITHARE_OBF2S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+2),s)()
#define ITHARE_OBF3S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+3),s)()
#define ITHARE_OBF4S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+4),s)()
#define ITHARE_OBF5S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+5),s)()
#define ITHARE_OBF6S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(ITHARE_OBF_LOCATION,0,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+6),s)()

/*#else//_MSC_VER
#define ITHARE_OBF0(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+0)>
#define ITHARE_OBF1(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+1)>
#define ITHARE_OBF2(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+2)>
#define ITHARE_OBF3(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+3)>
#define ITHARE_OBF4(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+4)>
#define ITHARE_OBF5(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+5)>
#define ITHARE_OBF6(type) ithare::obf::obf_var<type,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+6)>

#define ITHARE_OBF0I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+0)>()
#define ITHARE_OBF1I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+1)>()
#define ITHARE_OBF2I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+2)>()
#define ITHARE_OBF3I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+3)>()
#define ITHARE_OBF4I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+4)>()
#define ITHARE_OBF5I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+5)>()
#define ITHARE_OBF6I(c) obf_literal<decltype(c),c,ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+6)>()

#define ITHARE_OBF0S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+0),s)().value()
#define ITHARE_OBF1S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+1),s)().value()
#define ITHARE_OBF2S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+2),s)().value()
#define ITHARE_OBF3S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+3),s)().value()
#define ITHARE_OBF4S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+4),s)().value()
#define ITHARE_OBF5S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+5),s)().value()
#define ITHARE_OBF6S(s) ITHARE_OBFS_HELPER(ITHARE_OBF_INIT_PRNG(__FILE__,__LINE__,__COUNTER__),ithare::obf::obf_exp_cycles((ITHARE_OBF_SCALE)+6),s)().value()
#endif*/

#else//ITHARE_OBF_SEED
namespace ithare {
	namespace obf {
		constexpr size_t obf_strlen(const char* s) {
			for (size_t ret = 0; ; ++ret, ++s)
				if (*s == 0)
					return ret;
		}

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
		//dbgPrint helpers
		template<class T>
		std::string obf_dbgPrintT() {
			return std::string("T(sizeof=") + std::to_string(sizeof(T)) + ")";
		}

		inline void obf_dbgPrint() {
		}
#endif

		//obf_literal_dbg
		//IMPORTANT: ANY API CHANGES MUST BE MIRRORED in obf_literal<>
		template<class T, T C>
		class obf_literal_dbg {
			static_assert(std::is_integral<T>::value);

		public:
			constexpr obf_literal_dbg() : val(C) {
			}
			T value() const {
				return val;
			}
			operator T() const {
				return value();
			}

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
			static void dbgPrint(size_t offset = 0, const char* prefix = "") {
				std::cout << std::string(offset, ' ') << prefix << "obf_literal<" << obf_dbgPrintT<T>() << "," << C << std::endl;
			}
#endif
		private:
			T val;
		};

		//obf_var_dbg
		//IMPORTANT: ANY API CHANGES MUST BE MIRRORED in obf_var<>
		template<class T>
		class obf_var_dbg {
			static_assert(std::is_integral<T>::value);

		public:
			obf_var_dbg(T t) : val(t) {
			}
			template<class T2>
			obf_var_dbg(obf_var_dbg<T2> t) : val(T(t.value())) {
			}
			template<class T2,T2 C2>
			obf_var_dbg(obf_literal_dbg<T2,C2> t) : val(T(t.value())) {
			}
			obf_var_dbg& operator =(T t) {
				val = t;
				return *this;
			}
			template<class T2>
			obf_var_dbg& operator =(obf_var_dbg<T2> t) {
				val = T(t.value());
				return *this;
			}
			template<class T2, T2 C2>
			obf_var_dbg& operator =(obf_literal_dbg<T2,C2> t) {
				val = T(t.value());
				return *this;
			}

			T value() const {
				return val;
			}
			operator T() const { return value(); }
			
			obf_var_dbg& operator ++() { *this = value() + 1; return *this; }
			obf_var_dbg& operator --() { *this = value() - 1; return *this; }
			obf_var_dbg operator++(int) { obf_var_dbg ret = obf_var_dbg(value());  *this = value() + 1; return ret; }
			obf_var_dbg operator--(int) { obf_var_dbg ret = obf_var_dbg(value());  *this = value() + 1; return ret; }

			template<class T2>
			bool operator <(T2 t) { return value() < t; }
			template<class T2>
			bool operator >(T2 t) { return value() > t; }
			template<class T2>
			bool operator ==(T2 t) { return value() == t; }
			template<class T2>
			bool operator !=(T2 t) { return value() != t; }
			template<class T2>
			bool operator <=(T2 t) { return value() <= t; }
			template<class T2>
			bool operator >=(T2 t) { return value() >= t; }

			template<class T2>
			bool operator <(obf_var_dbg<T2> t) {
				return value() < t.value();
			}//TODO: template<obf_literal_dbg>(for ALL comparisons)
			template<class T2>
			bool operator >(obf_var_dbg<T2> t) {
				return value() > t.value();
			}
			template<class T2>
			bool operator ==(obf_var_dbg<T2> t) {
				return value() == t.value();
			}
			template<class T2>
			bool operator !=(obf_var_dbg<T2> t) {
				return value() != t.value();
			}
			template<class T2>
			bool operator <=(obf_var_dbg<T2> t) {
				return value() <= t.value();
			}
			template<class T2>
			bool operator >=(obf_var_dbg<T2> t) {
				return value() >= t.value();
			}

			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE bool operator <(obf_literal_dbg<T2, C2> t) {
				return value() < t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE bool operator >(obf_literal_dbg<T2, C2> t) {
				return value() > t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE bool operator ==(obf_literal_dbg<T2, C2> t) {
				return value() == t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE bool operator !=(obf_literal_dbg<T2, C2> t) {
				return value() != t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE bool operator <=(obf_literal_dbg<T2, C2> t) {
				return value() <= t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE bool operator >=(obf_literal_dbg<T2, C2> t) {
				return value() > t.value();
			}

			template<class T2>
			obf_var_dbg& operator +=(T2 t) { *this = value() + t; return *this; }
			template<class T2>
			obf_var_dbg& operator -=(T2 t) { *this = value() - t; return *this; }
			template<class T2>
			obf_var_dbg& operator *=(T2 t) { *this = value() * t; return *this; }
			template<class T2>
			obf_var_dbg& operator /=(T2 t) { *this = value() / t; return *this; }
			template<class T2>
			obf_var_dbg& operator %=(T2 t) { *this = value() % t; return *this; }

			template<class T2>
			obf_var_dbg& operator +=(obf_var_dbg<T2> t) {
				return *this += t.value();
			}
			template<class T2>
			obf_var_dbg& operator -=(obf_var_dbg<T2> t) {
				return *this -= t.value();
			}
			template<class T2>
			obf_var_dbg& operator *=(obf_var_dbg<T2> t) {
				return *this *= t.value();
			}
			template<class T2>
			obf_var_dbg& operator /=(obf_var_dbg<T2> t) {
				return *this /= t.value();
			}
			template<class T2>
			obf_var_dbg& operator %=(obf_var_dbg<T2> t) {
				return *this %= t.value();
			}

			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg& operator +=(obf_literal_dbg<T2, C2> t) {
				return *this += t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg& operator -=(obf_literal_dbg<T2, C2> t) {
				return *this -= t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg& operator *=(obf_literal_dbg<T2, C2> t) {
				return *this *= t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg& operator /=(obf_literal_dbg<T2, C2> t) {
				return *this /= t.value();
			}
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg& operator %=(obf_literal_dbg<T2, C2> t) {
				return *this %= t.value();
			}

			template<class T2>
			obf_var_dbg operator +(T2 t) { return obf_var_dbg(value() + t); }
			template<class T2>
			obf_var_dbg operator -(T2 t) { return obf_var_dbg(value() - t); }
			template<class T2>
			obf_var_dbg operator *(T2 t) { return obf_var_dbg(value() * t); }
			template<class T2>
			obf_var_dbg operator /(T2 t) { return obf_var_dbg(value() / t); }
			template<class T2>
			obf_var_dbg operator %(T2 t) { return obf_var_dbg(value() % t); }

			template<class T2>//TODO: template<obf_literal_dbg>(for ALL binary operations)
			obf_var_dbg operator +(obf_var_dbg<T2> t) { return obf_var_dbg(value() + t.value()); }
			template<class T2>
			obf_var_dbg operator -(obf_var_dbg<T2> t) { return obf_var_dbg(value() - t.value()); }
			template<class T2>
			obf_var_dbg operator *(obf_var_dbg<T2> t) { return obf_var_dbg(value() * t.value()); }
			template<class T2>
			obf_var_dbg operator /(obf_var_dbg<T2> t) { return obf_var_dbg(value() / t.value()); }
			template<class T2>
			obf_var_dbg operator %(obf_var_dbg<T2> t) { return obf_var_dbg(value() % t.value()); }

			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg operator +(obf_literal_dbg<T2, C2> t) { return obf_var_dbg(value() + t.value()); }
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg operator -(obf_literal_dbg<T2, C2> t) { return obf_var_dbg(value() - t.value()); }
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg operator *(obf_literal_dbg<T2, C2> t) { return obf_var_dbg(value() * t.value()); }
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg operator /(obf_literal_dbg<T2, C2> t) { return obf_var_dbg(value() / t.value()); }
			template<class T2, T2 C2>
			ITHARE_OBF_FORCEINLINE obf_var_dbg operator %(obf_literal_dbg<T2, C2> t) { return obf_var_dbg(value() % t.value()); }

			//TODO: bitwise

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
			static void dbgPrint(size_t offset = 0,const char* prefix="") {
				std::cout << std::string(offset, ' ') << prefix << "obf_var_dbg<" << obf_dbgPrintT<T>() << ">" << std::endl;
			}
#endif

		private:
			T val;
		};

		inline void obf_init() {
		}

		//IMPORTANT: ANY API CHANGES MUST BE MIRRORED in obf_str_literal
		template<char... C>
		struct obf_str_literal_dbg {
			static constexpr size_t origSz = sizeof...(C);
			static constexpr char const str[sizeof...(C)+1] = { C...,'\0'};
			static constexpr size_t sz = obf_strlen(str);
			static_assert(sz > 0);
			static_assert(sz <= 32);

			ITHARE_OBF_FORCEINLINE std::string value() const {
				return std::string(str, origSz);
			}
			ITHARE_OBF_FORCEINLINE operator std::string() const {
				return value();
			}

#ifdef ITHARE_OBF_DBG_ENABLE_DBGPRINT
			static void dbgPrint(size_t offset = 0, const char* prefix = "") {
				std::cout << std::string(offset, ' ') << prefix << "obf_str_literal_dbg<'" << str << "'>" << std::endl;
			}
#endif
		};

	}//namespace obf
}//namespace ithare

#define ITHARE_OBF0(type) ithare::obf::obf_var_dbg<type>
#define ITHARE_OBF1(type) ithare::obf::obf_var_dbg<type>
#define ITHARE_OBF2(type) ithare::obf::obf_var_dbg<type>
#define ITHARE_OBF3(type) ithare::obf::obf_var_dbg<type>
#define ITHARE_OBF4(type) ithare::obf::obf_var_dbg<type>
#define ITHARE_OBF5(type) ithare::obf::obf_var_dbg<type>
#define ITHARE_OBF6(type) ithare::obf::obf_var_dbg<type>

#define ITHARE_OBF0I(c) obf_literal_dbg<decltype(c),c>()
#define ITHARE_OBF1I(c) obf_literal_dbg<decltype(c),c>()
#define ITHARE_OBF2I(c) obf_literal_dbg<decltype(c),c>()
#define ITHARE_OBF3I(c) obf_literal_dbg<decltype(c),c>()
#define ITHARE_OBF4I(c) obf_literal_dbg<decltype(c),c>()
#define ITHARE_OBF5I(c) obf_literal_dbg<decltype(c),c>()
#define ITHARE_OBF6I(c) obf_literal_dbg<decltype(c),c>()

#define ITHARE_OBFS_DBG_HELPER(s) ithare::obf::obf_str_literal_dbg<(sizeof(s)>0?s[0]:'\0'),(sizeof(s)>1?s[1]:'\0'),(sizeof(s)>2?s[2]:'\0'),(sizeof(s)>3?s[3]:'\0'),\
							(sizeof(s)>4?s[4]:'\0'),(sizeof(s)>5?s[5]:'\0'),(sizeof(s)>6?s[6]:'\0'),(sizeof(s)>7?s[7]:'\0'),\
							(sizeof(s)>8?s[8]:'\0'),(sizeof(s)>9?s[9]:'\0'),(sizeof(s)>10?s[10]:'\0'),(sizeof(s)>11?s[11]:'\0'),\
							(sizeof(s)>12?s[12]:'\0'),(sizeof(s)>13?s[13]:'\0'),(sizeof(s)>14?s[14]:'\0'),(sizeof(s)>15?s[15]:'\0'),\
							(sizeof(s)>16?s[16]:'\0'),(sizeof(s)>17?s[17]:'\0'),(sizeof(s)>18?s[18]:'\0'),(sizeof(s)>19?s[19]:'\0'),\
							(sizeof(s)>20?s[20]:'\0'),(sizeof(s)>21?s[21]:'\0'),(sizeof(s)>22?s[22]:'\0'),(sizeof(s)>23?s[23]:'\0'),\
							(sizeof(s)>24?s[24]:'\0'),(sizeof(s)>25?s[25]:'\0'),(sizeof(s)>26?s[26]:'\0'),(sizeof(s)>27?s[27]:'\0'),\
							(sizeof(s)>28?s[28]:'\0'),(sizeof(s)>29?s[29]:'\0'),(sizeof(s)>30?s[30]:'\0'),(sizeof(s)>31?s[31]:'\0'),\
							(sizeof(s)>32?s[32]:'\0')/*one extra to generate an error if we're over*/>

#define ITHARE_OBF0S(s) ITHARE_OBFS_DBG_HELPER(s)()
#define ITHARE_OBF1S(s) ITHARE_OBFS_DBG_HELPER(s)()
#define ITHARE_OBF2S(s) ITHARE_OBFS_DBG_HELPER(s)()
#define ITHARE_OBF3S(s) ITHARE_OBFS_DBG_HELPER(s)()
#define ITHARE_OBF4S(s) ITHARE_OBFS_DBG_HELPER(s)()
#define ITHARE_OBF5S(s) ITHARE_OBFS_DBG_HELPER(s)()
#define ITHARE_OBF6S(s) ITHARE_OBFS_DBG_HELPER(s)()

#endif //ITHARE_OBF_SEED

#ifndef ITHARE_OBF_NO_SHORT_DEFINES//#define to avoid polluting global namespace w/o prefix
#define OBF0 ITHARE_OBF0
#define OBF1 ITHARE_OBF1
#define OBF2 ITHARE_OBF2
#define OBF3 ITHARE_OBF3
#define OBF4 ITHARE_OBF4
#define OBF5 ITHARE_OBF5
#define OBF6 ITHARE_OBF6

#define OBF0I ITHARE_OBF0I
#define OBF1I ITHARE_OBF1I
#define OBF2I ITHARE_OBF2I
#define OBF3I ITHARE_OBF3I
#define OBF4I ITHARE_OBF4I
#define OBF5I ITHARE_OBF5I
#define OBF6I ITHARE_OBF6I

#define OBF0S ITHARE_OBF0S
#define OBF1S ITHARE_OBF1S
#define OBF2S ITHARE_OBF2S
#define OBF3S ITHARE_OBF3S
#define OBF4S ITHARE_OBF4S
#define OBF5S ITHARE_OBF5S
#define OBF6S ITHARE_OBF6S
#endif

#endif//ithare_obf_obfuscate_h_included
