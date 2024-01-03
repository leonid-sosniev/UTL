#pragma once

#include <cstdint>
#include <utl/diagnostics/logging.hpp>

namespace _utl { namespace logging {
/*
namespace {

    static constexpr char hundred[100][2] = {
        { '0','0' }, { '0','1' }, { '0','2' }, { '0','3' }, { '0','4' }, { '0','5' }, { '0','6' }, { '0','7' }, { '0','8' }, { '0','9' },
        { '1','0' }, { '1','1' }, { '1','2' }, { '1','3' }, { '1','4' }, { '1','5' }, { '1','6' }, { '1','7' }, { '1','8' }, { '1','9' },
        { '2','0' }, { '2','1' }, { '2','2' }, { '2','3' }, { '2','4' }, { '2','5' }, { '2','6' }, { '2','7' }, { '2','8' }, { '2','9' },
        { '3','0' }, { '3','1' }, { '3','2' }, { '3','3' }, { '3','4' }, { '3','5' }, { '3','6' }, { '3','7' }, { '3','8' }, { '3','9' },
        { '4','0' }, { '4','1' }, { '4','2' }, { '4','3' }, { '4','4' }, { '4','5' }, { '4','6' }, { '4','7' }, { '4','8' }, { '4','9' },
        { '5','0' }, { '5','1' }, { '5','2' }, { '5','3' }, { '5','4' }, { '5','5' }, { '5','6' }, { '5','7' }, { '5','8' }, { '5','9' },
        { '6','0' }, { '6','1' }, { '6','2' }, { '6','3' }, { '6','4' }, { '6','5' }, { '6','6' }, { '6','7' }, { '6','8' }, { '6','9' },
        { '7','0' }, { '7','1' }, { '7','2' }, { '7','3' }, { '7','4' }, { '7','5' }, { '7','6' }, { '7','7' }, { '7','8' }, { '7','9' },
        { '8','0' }, { '8','1' }, { '8','2' }, { '8','3' }, { '8','4' }, { '8','5' }, { '8','6' }, { '8','7' }, { '8','8' }, { '8','9' },
        { '9','0' }, { '9','1' }, { '9','2' }, { '9','3' }, { '9','4' }, { '9','5' }, { '9','6' }, { '9','7' }, { '9','8' }, { '9','9' },
    };
    static constexpr uint8_t decimalDigitsNumberByVarSize[1 + 8] = {
    //  0  1    2    3  4   5  6  7  8
        0, 3+1, 5+1, 0, 10+1, 0, 0, 0, 20+1 // also include 1 for a number sign
    };

    inline auto printDecimal(char * buffer, const float & number, char *& out_end) -> char*
    {
        out_end = buffer + std::snprintf(buffer, 32, "%0.7f", number);
        return buffer;
    }
    inline auto printDecimal(char * buffer, const double & number, char *& out_end) -> char*
    {
        out_end = buffer + std::snprintf(buffer, 32, "%0.16f", number);
        return buffer;
    }
    template<typename T>
    inline auto printDecimal(char * buffer, const T & number, char *& out_end)
        -> typename std::enable_if<std::is_unsigned<T>::value && std::is_integral<T>::value,char*>::type
    {
        using N = typename std::remove_cv<T>::type;
        N num = number;
        register char *p = buffer + decimalDigitsNumberByVarSize[sizeof(T)];
        out_end = p;
        do {
            const char (&pair)[2] = hundred[num % 100];
            p -= 2;
            *reinterpret_cast<uint16_t *>(p) = *reinterpret_cast<const uint16_t *>(pair);
            num /= 100;
        }
        while (num);
        register auto p2 = p + 1;
        return *p == '0' ? p2 : p;
    }
    template<typename T>
    inline auto printDecimal(char * buffer, const T & number, char *& out_end)
        -> typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value,char*>::type
    {
        using U = typename std::make_unsigned<T>::type;
        using N = typename std::remove_cv<T>::type;
        N num2 = -number;
        N num = number;
        char * p = printDecimal(buffer + 1, U(num >= 0 ? num : num2), out_end);
        if (num < 0) *--p = '-';
        return p;
    }

} // anonimous namespace

    class PlainTextEventFormatter : public AbstractEventFormatter {
    public:
        void formatEventAttributes_(const EventAttributes &) override {
            return;
        }
        void formatEvent_(const EventAttributes & attr, const Arg arg[]) override {
            AbstractWriter * writer = nullptr;
            char * fmtStr;
            char * fmtEnd;
            char fmtBuf[64];

            writer->write("[ ", 2);
            writer->write(attr.function.str, attr.function.end - attr.function.str);
            writer->write(" - ", 3);
            writer->write(attr.file.str, attr.file.end - attr.file.str);
            writer->write(": ", 2);
            fmtStr = printDecimal<uint32_t>(fmtBuf, attr.line, fmtEnd);
            writer->write(fmtStr, fmtEnd - fmtStr);
            writer->write(" ]", 2);

            writer->write(" \"", 2);
            writer->write(attr.messageFormat.str, attr.messageFormat.end - attr.messageFormat.str);
            writer->write("\" ", 2);

            for (uint16_t i = 0; i < attr.argumentsExpected; ++i,++arg) {
                writer->write(" // ", 4);
                switch (arg->type) {
                    #define WRITE_ARRAY_OF(TYPE) \
                        for (uint32_t i = 0; i < arg->arrayLength; ++i) { \
                            fmtStr = printDecimal(fmtBuf, static_cast<const TYPE*>(arg->valueOrArray.ArrayPointer)[i], fmtEnd); \
                            *fmtEnd++ = ','; \
                            writer->write(fmtStr,fmtEnd-fmtStr); \
                        }
                    case Arg::TypeID::TI_arrayof_u8:        WRITE_ARRAY_OF(uint8_t);  break;
                    case Arg::TypeID::TI_arrayof_u16:       WRITE_ARRAY_OF(uint16_t); break;
                    case Arg::TypeID::TI_arrayof_u32:       WRITE_ARRAY_OF(uint32_t); break;
                    case Arg::TypeID::TI_arrayof_u64:       WRITE_ARRAY_OF(uint64_t); break;
                    case Arg::TypeID::TI_arrayof_i8:        WRITE_ARRAY_OF(int8_t);   break;
                    case Arg::TypeID::TI_arrayof_i16:       WRITE_ARRAY_OF(int16_t);  break;
                    case Arg::TypeID::TI_arrayof_i32:       WRITE_ARRAY_OF(int32_t);  break;
                    case Arg::TypeID::TI_arrayof_i64:       WRITE_ARRAY_OF(int64_t);  break;
                    case Arg::TypeID::TI_arrayof_f32:       WRITE_ARRAY_OF(float);    break;
                    case Arg::TypeID::TI_arrayof_f64:       WRITE_ARRAY_OF(double);   break;
                    case Arg::TypeID::TI_arrayof_Char:      writer->write((char*) arg->valueOrArray.ArrayPointer, arg->arrayLength); writer->write(",",1); break;
                    case Arg::TypeID::TI_arrayof_Thread:    break;
                    case Arg::TypeID::TI_arrayof_EpochNsec: break;
                    #undef WRITE_ARRAY_OF
                    case Arg::TypeID::TI_u8:        fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u8 , fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_u16:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u16, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_u32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_u64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i8:        fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i8 , fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i16:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i16, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_f32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.f32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_f64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.f64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_Char:      writer->write(&arg->valueOrArray.Char,1); break;
                    case Arg::TypeID::TI_Thread:    break;
                    case Arg::TypeID::TI_EpochNsec: break;
                    default: break;
                }
            }
            writer->write("\n", 1);
        }
    };

    class PlainTextTelemetryFormatter : public AbstractTelemetryFormatter {
    private:
        Arg::TypeID m_typesStaticBuffer[32];
        Arg::TypeID * m_types = m_typesStaticBuffer;
        uint16_t m_sampleLength = 0;
    public:
        ~PlainTextTelemetryFormatter()
        {
            if (m_types != m_typesStaticBuffer) {
                delete[] m_types;
            }
        }
        virtual void formatExpectedTypes(AbstractWriter & wtr, uint16_t count, const Arg::TypeID types[]) final override {
            if (m_sampleLength) {
                throw std::logic_error{ "PlainTextTelemetryFormatter::formatExpectedTypes() has already been called for the instance." };
            } else {
#if defined(DEBUG)
                Arg::TypeID t; for (auto i = count; i-->0;) { t = types[i]; assert(t != Arg::TypeID::TI_UNKNOWN); }
#endif
                if (std::size(m_typesStaticBuffer) < count) m_types = new Arg::TypeID[count];
                m_sampleLength = count;
                std::memcpy(m_types, types, count);
            }
        }
        virtual void formatValues(AbstractWriter & wtr, const Arg arg[]) final override {
    #if defined(DEBUG)
            const Arg *a; for (auto i = m_sampleLength; i-->0;) { a = &arg[i]; assert(a->type == m_types[i]); }
    #endif
            auto writer = &wtr;
            char fmtBuf[32];
            char * fmtStr;
            char * fmtEnd;
            for (uint16_t i = 0; i < m_sampleLength; ++i,++arg) {
                switch (m_types[i]) {
                    #define WRITE_ARRAY_OF(TYPE) \
                        writer->write("{", 1); \
                        for (uint32_t i = 0; i < arg->arrayLength; ++i) { \
                            fmtStr = printDecimal(fmtBuf, static_cast<const TYPE*>(arg->valueOrArray.ArrayPointer)[i], fmtEnd); \
                            *fmtEnd++ = ','; \
                            writer->write(fmtStr,fmtEnd-fmtStr); \
                        } \
                        writer->write("}", 1);
                    case Arg::TypeID::TI_arrayof_u8:        WRITE_ARRAY_OF(uint8_t);  break;
                    case Arg::TypeID::TI_arrayof_u16:       WRITE_ARRAY_OF(uint16_t); break;
                    case Arg::TypeID::TI_arrayof_u32:       WRITE_ARRAY_OF(uint32_t); break;
                    case Arg::TypeID::TI_arrayof_u64:       WRITE_ARRAY_OF(uint64_t); break;
                    case Arg::TypeID::TI_arrayof_i8:        WRITE_ARRAY_OF(int8_t);   break;
                    case Arg::TypeID::TI_arrayof_i16:       WRITE_ARRAY_OF(int16_t);  break;
                    case Arg::TypeID::TI_arrayof_i32:       WRITE_ARRAY_OF(int32_t);  break;
                    case Arg::TypeID::TI_arrayof_i64:       WRITE_ARRAY_OF(int64_t);  break;
                    case Arg::TypeID::TI_arrayof_f32:       WRITE_ARRAY_OF(float);    break;
                    case Arg::TypeID::TI_arrayof_f64:       WRITE_ARRAY_OF(double);   break;
                    case Arg::TypeID::TI_arrayof_Char:      writer->write((char*) arg->valueOrArray.ArrayPointer, arg->arrayLength); writer->write(",",1); break;
                    case Arg::TypeID::TI_arrayof_Thread:    WRITE_ARRAY_OF(ThreadId); break;
                    case Arg::TypeID::TI_arrayof_EpochNsec: WRITE_ARRAY_OF(TimePoint); break;
                    #undef WRITE_ARRAY_OF
                    case Arg::TypeID::TI_u8:        fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u8 , fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_u16:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u16, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_u32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_u64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i8:        fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i8 , fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i16:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i16, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_i64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_f32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.f32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_f64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.f64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_Char:      writer->write(&arg->valueOrArray.Char,1); break;
                    case Arg::TypeID::TI_Thread:    fmtStr = printDecimal(fmtBuf, arg->valueOrArray.Thread,    fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    case Arg::TypeID::TI_EpochNsec: fmtStr = printDecimal(fmtBuf, arg->valueOrArray.EpochNsec, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                    default: break;
                }
            }
            writer->write("\n", 1);
        }
    };
*/
} // namespace logging
} // namespace _utl
