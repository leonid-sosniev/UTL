#pragma once

#include <cassert>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace _utl
{
namespace debugging_helpers
{
    class ThreadNames
    {
        mutable std::mutex mux_;
        std::map<std::thread::id,std::string> names_;
    public:
        void setName(const std::string & name, size_t index = size_t(-1))
        {
            std::unique_lock<std::mutex> lock{mux_};
            std::string &saved = names_[std::this_thread::get_id()];
            if (size_t(-1) == index) {
                saved = name;
            } else {
                saved.resize(name.size() + (index < uint16_t(-1) ? 5 : 20));
                auto len = std::snprintf(const_cast<char*>(saved.data()), saved.size(), "%s_%zu", name.c_str(), index);
                assert(len > 0);
                saved.resize(len);
            }
            // pthread_setname_np(pthread_self(), saved.c_str());
        }
        std::string getName(std::thread::id threadId)
        {
            std::unique_lock<std::mutex> lock{mux_};
            auto it = names_.find(threadId);
            return (names_.end() != it) ? it->second : (std::stringstream{} << threadId).str();
        }
        std::string getName()
        {
            return getName(std::this_thread::get_id());
        }
        std::map<std::thread::id,std::string> names() const
        {
            std::unique_lock<std::mutex> lock{mux_};
            return names_;
        }
    };
    template<typename T> class Singleton
    {
    public:
        static T & getInstance()
        {
            static T instance{};
            return instance;
        }
    };
    Singleton<ThreadNames> g_threadNames;

} // namespace debugging_helpers
} // namespace _utl

#if defined(NDEBUG)
 #define DBG(...)
 #define DBG_THREAD_NAME_N(NAME_STR, INDEX)
 #define DBG_THREAD_NAME(NAME_STR)
 #define DBG_THREAD_GET_NAME()              ""
 #define DBG_THREAD_GET_NAME_BY_TID(TID)    ""
#else
 #define DBG(...)                           __VA_ARGS__
 #define DBG_THREAD_NAME_N(NAME_STR, INDEX) _utl::debugging_helpers::g_threadNames.getInstance().setName(NAME_STR, INDEX);
 #define DBG_THREAD_NAME(NAME_STR)          _utl::debugging_helpers::g_threadNames.getInstance().setName(NAME_STR);
 #define DBG_THREAD_GET_NAME()              _utl::debugging_helpers::g_threadNames.getInstance().getName()
 #define DBG_THREAD_GET_NAME_BY_TID(TID)    _utl::debugging_helpers::g_threadNames.getInstance().getName(TID)
#endif

namespace {

template<class T, size_t N> inline void printPack_sfinae(std::ostream & stream, T (&p)[N]) {
    stream << "'";
    for (auto c : p) { stream << c; }
    stream << "'";
}
inline void printPack_sfinae_pchar(std::ostream & stream, const char * p) {
    auto pLengthInvestigator = p;
    while (*pLengthInvestigator && pLengthInvestigator - p < 1024) {
        ++pLengthInvestigator;
    }
    stream << '"' << p << (*pLengthInvestigator ? "..." : "") << '"';
}
inline void printPack_sfinae(std::ostream & stream, const char * p) {
    printPack_sfinae_pchar(stream, p);
}
inline void printPack_sfinae(std::ostream & stream, char * p) {
    printPack_sfinae_pchar(stream, p);
}
inline void printPack_sfinae(std::ostream & stream, std::thread::id && a) {
    stream << a;
}
template<class C, class D> inline void printPack_sfinae(std::ostream & stream, std::chrono::time_point<C,D> && a) {
    stream << std::chrono::duration_cast<std::chrono::nanoseconds>(a.time_since_epoch()).count();
}
template<class T> inline void printPack_sfinae(std::ostream & stream, T && a) {
    stream << a;
}
template<class T, class...Ts> inline void printPack_sfinae(std::ostream & stream, T && fst, Ts &&... args) {
    printPack_sfinae(stream, std::forward<T&&>(fst));
    stream << ", ";
    printPack_sfinae(stream, std::forward<Ts&&>(args)...);
}

} // namespace anonymous

template<class...Ts> inline void printPack(std::ostream & stream, Ts &&... args) {
    stream << "pack:[ ";
    printPack_sfinae(stream, std::forward<Ts&&>(args)...);
    stream << " ]" << std::endl;
}
inline void printPack(std::ostream & stream) {
    stream << "pack:[]" << std::endl;
}
