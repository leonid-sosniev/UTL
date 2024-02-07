#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <map>

#if defined(USE_CATCH2)

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <utl/Catch2/single_include/catch2/catch.hpp>

#else // #if defined(USE_CATCH2)

namespace _utl
{

class Tester
{
    friend class TesterRegister;
    static std::map<std::string,void(*)()> tests_;
public:
    static void run(const std::string & name = "")
    {
        for (auto & name_func : tests_)
        {
            if (name.size() && name_func.first.find(name,0u) == std::string::npos)
            {
                continue;
            }
            assert(name_func.second);
            std::cerr << "Running '" << name_func.first << "' test case" << std::endl;
            try {
                name_func.second();
            }
            catch (std::exception & exc) {
                std::cerr << "Failed with exception \"" << exc.what() << "\"" << std::endl;
            }
            catch (...) {
                std::cerr << "Failed with custom value thrown" << std::endl;
            }
        }
    }
};

std::map<std::string,void(*)()> Tester::tests_{};

struct TesterRegister
{
    TesterRegister(const std::string & name, void(*action)())
    {
        Tester::tests_.insert({name, action});
    }
};

} // namespace _utl

#define UTL_CONCAT_NAME_LINE_sub1(NAME, LINE) NAME##LINE
#define UTL_CONCAT_NAME_LINE_sub2(NAME, LINE) UTL_CONCAT_NAME_LINE_sub1(NAME, LINE)
#define UTL_CONCAT_NAME_LINE(NAME, LINE) UTL_CONCAT_NAME_LINE_sub2(NAME, LINE)

#define REQUIRE(x) assert(x)

#define TEST_CASE(TEST_NAME, ...) \
    namespace UTL_CONCAT_NAME_LINE(_utl_tester_cases,__LINE__) { \
        static void UTL_CONCAT_NAME_LINE(utl_tester_test_case_,__LINE__)(); \
        static _utl::TesterRegister UTL_CONCAT_NAME_LINE(utl_tester_test_case_register_,__LINE__){ \
            TEST_NAME, \
            UTL_CONCAT_NAME_LINE(utl_tester_test_case_,__LINE__) \
        }; \
    } \
    static void UTL_CONCAT_NAME_LINE(_utl_tester_cases,__LINE__)::UTL_CONCAT_NAME_LINE(utl_tester_test_case_,__LINE__)()


#if defined(CATCH_CONFIG_MAIN)

int main(int argc, char ** argv)
{
    if (argc > 1)
        _utl::Tester::run(argv[1]);
    else
    _utl::Tester::run();
}

#endif // #if defined(CATCH_CONFIG_MAIN)

#endif // #if defined(USE_CATCH2)