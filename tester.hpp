#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <map>

namespace _utl
{

class Tester
{
    friend class TesterRegister;
    static std::map<std::string,std::function<void()>> tests_;
public:
    static void run()
    {
        for (auto & name_func : tests_)
        {
            assert(name_func.second);
            std::cerr << "Running " << name_func.first << std::endl;
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

std::map<std::string,std::function<void()>> Tester::tests_{};

struct TesterRegister
{
    TesterRegister(const std::string & name, std::function<void()> action)
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
    namespace _utl_tester_cases { \
        void UTL_CONCAT_NAME_LINE(utl_tester_test_case_,__LINE__)(); \
        static _utl::TesterRegister UTL_CONCAT_NAME_LINE(utl_tester_test_case_register_,__LINE__){ \
            TEST_NAME, \
            UTL_CONCAT_NAME_LINE(utl_tester_test_case_,__LINE__) \
        }; \
    } \
    void _utl_tester_cases::UTL_CONCAT_NAME_LINE(utl_tester_test_case_,__LINE__)()


#if defined(CATCH_CONFIG_MAIN)

int main()
{
    _utl::Tester::run();
}

#endif