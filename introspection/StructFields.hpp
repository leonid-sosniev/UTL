#pragma once

#include <cstddef> // size_t
#include <type_traits> // enable_if...

namespace _utl
{
    namespace details { namespace StructFields
    {
        template<class Pod> struct GetFieldCount {
        private:
            static constexpr const size_t MAX_POSSIBLE_FIELD_COUNT = sizeof(Pod)*8;
            struct Typer {
                const size_t ix;
                template<class T> constexpr operator T () { return T{}; }
            };
            template<class T> struct Result {
                size_t count;
            };
            template<size_t SearchRegion_Min, size_t SearchRegion_Max, size_t I, size_t...Is>
            static constexpr
            auto cnt(std::index_sequence<I,Is...>) -> Result<decltype(Pod{Typer{I},Typer{Is}...})> {
                /* current count is less or equal to target one */
                enum {
                    currCnt = 1+sizeof...(Is),
                    nextCnt = (currCnt + SearchRegion_Max+1) / 2
                };
                if (currCnt == nextCnt) {
                    return Result<Pod>{currCnt};
                } else {
                    return cnt<currCnt,SearchRegion_Max>( std::make_index_sequence<nextCnt>() );
                }
            }
            template<size_t SearchRegion_Min, size_t SearchRegion_Max, size_t...Is>
            static constexpr
            auto cnt(std::index_sequence<Is...>) -> Result<Pod> {
                /* current count is greater than target one */
                enum {
                    currCnt = sizeof...(Is),
                    nextCnt = (SearchRegion_Min + currCnt) / 2
                };
                return cnt<SearchRegion_Min,currCnt-1>(std::make_index_sequence<nextCnt>());
            }
        public:
            static constexpr size_t value = cnt<0,MAX_POSSIBLE_FIELD_COUNT>( std::make_index_sequence<MAX_POSSIBLE_FIELD_COUNT/2>() ).count;
        };
    }} // details // StructFields

    template<class T> inline constexpr size_t getFieldCount() {
        return details::StructFields::GetFieldCount<T>::value;
    }
} // _utl
