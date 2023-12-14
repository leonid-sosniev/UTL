#define CATCH_CONFIG_MAIN
//#include <utl/Catch2/single_include/catch2/catch.hpp>
#define TEST_CASE(...) int main()
#define REQUIRE(x) if (!(x)) { throw std::logic_error{std::to_string(__LINE__)}; }
#include <utl/introspection/StructFields.hpp>
#include <array>
#include <cassert>
#include <sstream>
#include <vector>

enum class E : uint8_t {
    A=37, B=1, C=2
};
std::string to_string(const E& e) {
    switch (e) {
        case E::A: return "E::A";
        case E::B: return "E::B";
        case E::C: return "E::C";
        default: std::abort();
    };
}

struct Visitor {
    std::stringstream str;
    bool commaIsNeeded = false;
public:
    template<class T> typename std::enable_if<std::is_fundamental<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << v;
        commaIsNeeded = true;
    }
    template<class T> typename std::enable_if<std::is_enum<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << "\"" << to_string(v) << "\"";
        commaIsNeeded = true;
    }
    template<class T> typename std::enable_if<std::is_union<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << "union(sizeof=" << sizeof(T) << ")";
        commaIsNeeded = true;
    }
    template<class T> typename std::enable_if<std::is_class<T>::value && !std::is_enum<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << "{";
        commaIsNeeded = false;
        _utl::PodIntrospection::processTopLevelFields(*this, v);
        commaIsNeeded = true;
        str << "}";
    }
    template<
        class T,
        typename = typename std::enable_if<!std::is_void<T>::value>::type
    >
    void process(const T * v) {
        process<T>(*v);
        str << "*";
    }
    void process(const void * v) {
        if (commaIsNeeded) str << ", ";
        if (v) {
            str << std::hex << v;
        } else {
            str << "nullptr";
        }
        commaIsNeeded = true;
    }
    template<class T, class...Args> void process(T(*const v)(Args...)) {
        if (commaIsNeeded) str << ", ";
        str << "(func)";
        commaIsNeeded = true;
    }
    void process(const char * v) {
        if (commaIsNeeded) str << ", ";
        str << '"' << v << '"';
        commaIsNeeded = true;
    }
    void process(const bool v) {
        if (commaIsNeeded) str << ", ";
        str << (v ? "true" : "false");
        commaIsNeeded = true;
    }
    void process(...) {
        if (commaIsNeeded) str << ", ";
        str << "(UNEXPECTED)";
        commaIsNeeded = true;
    }
};

struct S0 {};
struct S1 {
    float d;
};
struct S2 {
    int i;
    double d;
};
struct S3 {
    void * vp;
    bool b;
    bool b2;
    const char * cp;
    const float f;
    E e;
    union { int l; double p; } u;
    S1 *sp;
    S2 pp;
    void (*fp)();
    std::array<char*,4> a
    ;
};

void func() {}
Visitor V;
char emptyStr[1] = "";

#include <iostream>

TEST_CASE("get number of fields in struct", "introspection")
{
    REQUIRE(0  == _utl::PodIntrospection::getFieldCount<S0>());
    REQUIRE(0  == _utl::PodIntrospection::getFieldCountRecursive<S0>());
    REQUIRE(1  == _utl::PodIntrospection::getFieldCount<S1>());
    REQUIRE(1  == _utl::PodIntrospection::getFieldCountRecursive<S1>());
    REQUIRE(2  == _utl::PodIntrospection::getFieldCount<S2>());
    REQUIRE(2  == _utl::PodIntrospection::getFieldCountRecursive<S2>());
    REQUIRE(11 == _utl::PodIntrospection::getFieldCount<S3>());
    REQUIRE(15 == _utl::PodIntrospection::getFieldCountRecursive<S3>());

    S1 s1{ -4.f };
    _utl::PodIntrospection::processTopLevelFields(V, s1);
    V.commaIsNeeded = false; V.str << "\n";

    S2 s2{ 1, 0.5 };
    _utl::PodIntrospection::processTopLevelFields(V, s2);
    V.commaIsNeeded = false; V.str << "\n";

    S3 s3{ nullptr, true, false, "hello", 0.1f, E::A, {-1}, &s1, s2, &func, {emptyStr,emptyStr,emptyStr,emptyStr} };
    _utl::PodIntrospection::processTopLevelFields(V, s3);
    V.commaIsNeeded = false; V.str << "\n";

    std::cout << V.str.str() << std::endl;
}