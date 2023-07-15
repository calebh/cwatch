//Compiled on 7/15/2023 4:31:58 PM
#include <inttypes.h>
#include <stdbool.h>
#include <new>

#ifndef JUNIPER_H
#define JUNIPER_H

#include <stdlib.h>

#ifdef JUN_CUSTOM_PLACEMENT_NEW
void* operator new(size_t size, void* ptr)
{
    return ptr;
}
#endif

namespace juniper
{
    template<typename contained>
    struct wrapped_ptr {
        int ref_count;
        contained data;

        wrapped_ptr(contained init_data)
            : ref_count(1), data(init_data) {}
    };

    template <typename contained>
    class shared_ptr {
    private:
        wrapped_ptr<contained> *content;

        void inc_ref() {
            if (content != nullptr) {
                content->ref_count++;
            }
        }

        void dec_ref() {
            if (content != nullptr) {
                content->ref_count--;

                if (content->ref_count <= 0) {
                    delete content;
                    content = nullptr;
                }
            }
        }

    public:
        shared_ptr()
            : content(nullptr)
        {
        }

        shared_ptr(contained init_data)
            : content(new wrapped_ptr<contained>(init_data))
        {
        }

        // Copy constructor
        shared_ptr(const shared_ptr& rhs)
            : content(rhs.content)
        {
            inc_ref();
        }

        // Move constructor
        shared_ptr(shared_ptr&& dyingObj)
            : content(dyingObj.content) {

            // Clean the dying object
            dyingObj.content = nullptr;
        }

        ~shared_ptr()
        {
            dec_ref();
        }

        contained* get() {
            if (content != nullptr) {
                return &(content->data);
            } else {
                return nullptr;
            }
        }

        const contained* get() const {
            if (content != nullptr) {
                return &(content->data);
            } else {
                return nullptr;
            }
        }

        // Copy assignment
        shared_ptr& operator=(const shared_ptr& rhs) {
            // We're overwriting the current content with new content,
            // so decrement the current content referene count by 1.
            dec_ref();
            // We're now pointing to new content
            content = rhs.content;
            // Increment the reference count of the new content.
            inc_ref();

            return *this;
        }

        // Move assignment
        shared_ptr& operator=(shared_ptr&& dyingObj) {
            // We're overwriting the current content with new content,
            // so decrement the current content referene count by 1.
            dec_ref();

            content = dyingObj.content;

            // Clean the dying object
            dyingObj.content = nullptr;
            // Don't increment at all because the dying object
            // reduces the reference count by 1, which would cancel out
            // an increment.

            return *this;
        }

        // Two shared ptrs are equal if they point to the same data
        bool operator==(shared_ptr& rhs) {
            return content == rhs.content;
        }

        bool operator!=(shared_ptr& rhs) {
            return content != rhs.content;
        }
    };
    
    template <typename ClosureType, typename Result, typename ...Args>
    class function;

    template <typename Result, typename ...Args>
    class function<void, Result(Args...)> {
    private:
        Result(*F)(Args...);

    public:
        function(Result(*f)(Args...)) : F(f) {}

        Result operator()(Args... args) {
            return F(args...);
        }
    };

    template <typename ClosureType, typename Result, typename ...Args>
    class function<ClosureType, Result(Args...)> {
    private:
        ClosureType Closure;
        Result(*F)(ClosureType&, Args...);

    public:
        function(ClosureType closure, Result(*f)(ClosureType&, Args...)) : Closure(closure), F(f) {}

        Result operator()(Args... args) {
            return F(Closure, args...);
        }
    };

    template<typename T, size_t N>
    class array {
    public:
        array<T, N>& fill(T fillWith) {
            for (size_t i = 0; i < N; i++) {
                data[i] = fillWith;
            }

            return *this;
        }

        T& operator[](int i) {
            return data[i];
        }

        bool operator==(array<T, N>& rhs) {
            for (auto i = 0; i < N; i++) {
                if (data[i] != rhs[i]) {
                    return false;
                }
            }
            return true;
        }

        bool operator!=(array<T, N>& rhs) { return !(rhs == *this); }

        T data[N];
    };

    struct unit {
    public:
        bool operator==(unit rhs) {
            return true;
        }

        bool operator!=(unit rhs) {
            return !(rhs == *this);
        }
    };

    class rawpointer_container {
    private:
        void* data;
        function<void, unit(void*)> destructorCallback;

    public:
        rawpointer_container(void* initData, function<void, unit(void*)> callback)
            : data(initData), destructorCallback(callback) {}

        ~rawpointer_container() {
            destructorCallback(data);
        }

        void *get() { return data; }
    };

    using smartpointer = shared_ptr<rawpointer_container>;

    smartpointer make_smartpointer(void *initData, function<void, unit(void*)> callback) {
        return smartpointer(rawpointer_container(initData, callback));
    }

    template<typename T>
    T quit() {
        exit(1);
    }

    // Equivalent to std::aligned_storage
    template<unsigned int Len, unsigned int Align>
    struct aligned_storage {
        struct type {
            alignas(Align) unsigned char data[Len];
        };
    };

    template <unsigned int arg1, unsigned int ... others>
    struct static_max;

    template <unsigned int arg>
    struct static_max<arg>
    {
        static const unsigned int value = arg;
    };

    template <unsigned int arg1, unsigned int arg2, unsigned int ... others>
    struct static_max<arg1, arg2, others...>
    {
        static const unsigned int value = arg1 >= arg2 ? static_max<arg1, others...>::value :
            static_max<arg2, others...>::value;
    };

    template<class T> struct remove_reference { typedef T type; };
    template<class T> struct remove_reference<T&> { typedef T type; };
    template<class T> struct remove_reference<T&&> { typedef T type; };

    template<unsigned char n, typename... Ts>
    struct variant_helper_rec;

    template<unsigned char n, typename F, typename... Ts>
    struct variant_helper_rec<n, F, Ts...> {
        inline static void destroy(unsigned char id, void* data)
        {
            if (n == id) {
                reinterpret_cast<F*>(data)->~F();
            } else {
                variant_helper_rec<n + 1, Ts...>::destroy(id, data);
            }
        }

        inline static void move(unsigned char id, void* from, void* to)
        {
            if (n == id) {
                // This static_cast and use of remove_reference is equivalent to the use of std::move
                new (to) F(static_cast<typename remove_reference<F>::type&&>(*reinterpret_cast<F*>(from)));
            } else {
                variant_helper_rec<n + 1, Ts...>::move(id, from, to);
            }
        }

        inline static void copy(unsigned char id, const void* from, void* to)
        {
            if (n == id) {
                new (to) F(*reinterpret_cast<const F*>(from));
            } else {
                variant_helper_rec<n + 1, Ts...>::copy(id, from, to);
            }
        }

        inline static bool equal(unsigned char id, void* lhs, void* rhs)
        {
            if (n == id) {
                return (*reinterpret_cast<F*>(lhs)) == (*reinterpret_cast<F*>(rhs));
            } else {
                return variant_helper_rec<n + 1, Ts...>::equal(id, lhs, rhs);
            }
        }
    };

    template<unsigned char n> struct variant_helper_rec<n> {
        inline static void destroy(unsigned char id, void* data) { }
        inline static void move(unsigned char old_t, void* from, void* to) { }
        inline static void copy(unsigned char old_t, const void* from, void* to) { }
        inline static bool equal(unsigned char id, void* lhs, void* rhs) { return false; }
    };

    template<typename... Ts>
    struct variant_helper {
        inline static void destroy(unsigned char id, void* data) {
            variant_helper_rec<0, Ts...>::destroy(id, data);
        }

        inline static void move(unsigned char id, void* from, void* to) {
            variant_helper_rec<0, Ts...>::move(id, from, to);
        }

        inline static void copy(unsigned char id, const void* old_v, void* new_v) {
            variant_helper_rec<0, Ts...>::copy(id, old_v, new_v);
        }

        inline static bool equal(unsigned char id, void* lhs, void* rhs) {
            return variant_helper_rec<0, Ts...>::equal(id, lhs, rhs);
        }
    };

    template<> struct variant_helper<> {
        inline static void destroy(unsigned char id, void* data) { }
        inline static void move(unsigned char old_t, void* old_v, void* new_v) { }
        inline static void copy(unsigned char old_t, const void* old_v, void* new_v) { }
    };

    template<typename F>
    struct variant_helper_static;

    template<typename F>
    struct variant_helper_static {
        inline static void move(void* from, void* to) {
            new (to) F(static_cast<typename remove_reference<F>::type&&>(*reinterpret_cast<F*>(from)));
        }

        inline static void copy(const void* from, void* to) {
            new (to) F(*reinterpret_cast<const F*>(from));
        }
    };

    // Given a unsigned char i, selects the ith type from the list of item types
    template<unsigned char i, typename... Items>
    struct variant_alternative;

    template<typename HeadItem, typename... TailItems>
    struct variant_alternative<0, HeadItem, TailItems...>
    {
        using type = HeadItem;
    };

    template<unsigned char i, typename HeadItem, typename... TailItems>
    struct variant_alternative<i, HeadItem, TailItems...>
    {
        using type = typename variant_alternative<i - 1, TailItems...>::type;
    };

    template<typename... Ts>
    struct variant {
    private:
        static const unsigned int data_size = static_max<sizeof(Ts)...>::value;
        static const unsigned int data_align = static_max<alignof(Ts)...>::value;

        using data_t = typename aligned_storage<data_size, data_align>::type;

        using helper_t = variant_helper<Ts...>;

        template<unsigned char i>
        using alternative = typename variant_alternative<i, Ts...>::type;

        unsigned char variant_id;
        data_t data;

        variant(unsigned char id) : variant_id(id) {}

    public:
        template<unsigned char i>
        static variant create(alternative<i>& value)
        {
            variant ret(i);
            variant_helper_static<alternative<i>>::copy(&value, &ret.data);
            return ret;
        }
        
        template<unsigned char i>
        static variant create(alternative<i>&& value) {
            variant ret(i);
            variant_helper_static<alternative<i>>::move(&value, &ret.data);
            return ret;
        }

        variant() {}

        variant(const variant<Ts...>& from) : variant_id(from.variant_id)
        {
            helper_t::copy(from.variant_id, &from.data, &data);
        }

        variant(variant<Ts...>&& from) : variant_id(from.variant_id)
        {
            helper_t::move(from.variant_id, &from.data, &data);
        }

        variant<Ts...>& operator= (variant<Ts...>& rhs)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = rhs.variant_id;
            helper_t::copy(rhs.variant_id, &rhs.data, &data);
            return *this;
        }

        variant<Ts...>& operator= (variant<Ts...>&& rhs)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = rhs.variant_id;
            helper_t::move(rhs.variant_id, &rhs.data, &data);
            return *this;
        }

        unsigned char id() {
            return variant_id;
        }

        template<unsigned char i>
        void set(alternative<i>& value)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = i;
            variant_helper_static<alternative<i>>::copy(&value, &data);
        }

        template<unsigned char i>
        void set(alternative<i>&& value)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = i;
            variant_helper_static<alternative<i>>::move(&value, &data);
        }

        template<unsigned char i>
        alternative<i>& get()
        {
            if (variant_id == i) {
                return *reinterpret_cast<alternative<i>*>(&data);
            } else {
                return quit<alternative<i>&>();
            }
        }

        ~variant() {
            helper_t::destroy(variant_id, &data);
        }

        bool operator==(variant& rhs) {
            if (variant_id == rhs.variant_id) {
                return helper_t::equal(variant_id, &data, &rhs.data);
            } else {
                return false;
            }
        }

        bool operator==(variant&& rhs) {
            if (variant_id == rhs.variant_id) {
                return helper_t::equal(variant_id, &data, &rhs.data);
            } else {
                return false;
            }
        }

        bool operator!=(variant& rhs) {
            return !(this->operator==(rhs));
        }

        bool operator!=(variant&& rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a, typename b>
    struct tuple2 {
        a e1;
        b e2;

        tuple2(a initE1, b initE2) : e1(initE1), e2(initE2) {}

        bool operator==(tuple2<a,b> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2;
        }

        bool operator!=(tuple2<a,b> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c>
    struct tuple3 {
        a e1;
        b e2;
        c e3;

        tuple3(a initE1, b initE2, c initE3) : e1(initE1), e2(initE2), e3(initE3) {}

        bool operator==(tuple3<a,b,c> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3;
        }

        bool operator!=(tuple3<a,b,c> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d>
    struct tuple4 {
        a e1;
        b e2;
        c e3;
        d e4;

        tuple4(a initE1, b initE2, c initE3, d initE4) : e1(initE1), e2(initE2), e3(initE3), e4(initE4) {}

        bool operator==(tuple4<a,b,c,d> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4;
        }

        bool operator!=(tuple4<a,b,c,d> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e>
    struct tuple5 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;

        tuple5(a initE1, b initE2, c initE3, d initE4, e initE5) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5) {}

        bool operator==(tuple5<a,b,c,d,e> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5;
        }

        bool operator!=(tuple5<a,b,c,d,e> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f>
    struct tuple6 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;

        tuple6(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6) {}

        bool operator==(tuple6<a,b,c,d,e,f> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6;
        }

        bool operator!=(tuple6<a,b,c,d,e,f> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g>
    struct tuple7 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;

        tuple7(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7) {}

        bool operator==(tuple7<a,b,c,d,e,f,g> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7;
        }

        bool operator!=(tuple7<a,b,c,d,e,f,g> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g, typename h>
    struct tuple8 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;
        h e8;

        tuple8(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7, h initE8) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7), e8(initE8) {}

        bool operator==(tuple8<a,b,c,d,e,f,g,h> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8;
        }

        bool operator!=(tuple8<a,b,c,d,e,f,g,h> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g, typename h, typename i>
    struct tuple9 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;
        h e8;
        i e9;

        tuple9(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7, h initE8, i initE9) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7), e8(initE8), e9(initE9) {}

        bool operator==(tuple9<a,b,c,d,e,f,g,h,i> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8 && e9 == rhs.e9;
        }

        bool operator!=(tuple9<a,b,c,d,e,f,g,h,i> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g, typename h, typename i, typename j>
    struct tuple10 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;
        h e8;
        i e9;
        j e10;

        tuple10(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7, h initE8, i initE9, j initE10) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7), e8(initE8), e9(initE9), e10(initE10) {}

        bool operator==(tuple10<a,b,c,d,e,f,g,h,i,j> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8 && e9 == rhs.e9 && e10 == rhs.e10;
        }

        bool operator!=(tuple10<a,b,c,d,e,f,g,h,i,j> rhs) {
            return !(rhs == *this);
        }
    };
}

#endif

#include <Arduino.h>
#include <Arduino.h>
#include "Adafruit_Arcada.h"
#include <bluefruit.h>
#include <bluefruit_common.h>
#include <bluefruit.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

namespace Prelude {}
namespace List {}
namespace Signal {}
namespace Io {}
namespace Maybe {}
namespace Time {}
namespace Math {}
namespace Button {}
namespace Vector {}
namespace CharList {}
namespace StringM {}
namespace Random {}
namespace Color {}
namespace Arcada {}
namespace Ble {}
namespace CWatch {}
namespace Gfx {}
namespace List {
    using namespace Prelude;

}

namespace Signal {
    using namespace Prelude;

}

namespace Io {
    using namespace Prelude;

}

namespace Maybe {
    using namespace Prelude;

}

namespace Time {
    using namespace Prelude;

}

namespace Math {
    using namespace Prelude;

}

namespace Button {
    using namespace Prelude;

}

namespace Button {
    using namespace Io;

}

namespace Vector {
    using namespace Prelude;

}

namespace Vector {
    using namespace List;
    using namespace Math;

}

namespace CharList {
    using namespace Prelude;

}

namespace StringM {
    using namespace Prelude;

}

namespace Random {
    using namespace Prelude;

}

namespace Color {
    using namespace Prelude;

}

namespace Arcada {
    using namespace Prelude;

}

namespace Ble {
    using namespace Prelude;

}

namespace CWatch {
    using namespace Prelude;

}

namespace Gfx {
    using namespace Prelude;

}

namespace Gfx {
    using namespace Arcada;
    using namespace Color;

}

namespace juniper {
    namespace records {
        template<typename T1,typename T2,typename T3,typename T4>
        struct recordt_4 {
            T1 a;
            T2 b;
            T3 g;
            T4 r;

            recordt_4() {}

            recordt_4(T1 init_a, T2 init_b, T3 init_g, T4 init_r)
                : a(init_a), b(init_b), g(init_g), r(init_r) {}

            bool operator==(recordt_4<T1, T2, T3, T4> rhs) {
                return true && a == rhs.a && b == rhs.b && g == rhs.g && r == rhs.r;
            }

            bool operator!=(recordt_4<T1, T2, T3, T4> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3,typename T4>
        struct recordt_6 {
            T1 a;
            T2 h;
            T3 s;
            T4 v;

            recordt_6() {}

            recordt_6(T1 init_a, T2 init_h, T3 init_s, T4 init_v)
                : a(init_a), h(init_h), s(init_s), v(init_v) {}

            bool operator==(recordt_6<T1, T2, T3, T4> rhs) {
                return true && a == rhs.a && h == rhs.h && s == rhs.s && v == rhs.v;
            }

            bool operator!=(recordt_6<T1, T2, T3, T4> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3>
        struct recordt_2 {
            T1 actualState;
            T2 lastDebounceTime;
            T3 lastState;

            recordt_2() {}

            recordt_2(T1 init_actualState, T2 init_lastDebounceTime, T3 init_lastState)
                : actualState(init_actualState), lastDebounceTime(init_lastDebounceTime), lastState(init_lastState) {}

            bool operator==(recordt_2<T1, T2, T3> rhs) {
                return true && actualState == rhs.actualState && lastDebounceTime == rhs.lastDebounceTime && lastState == rhs.lastState;
            }

            bool operator!=(recordt_2<T1, T2, T3> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3>
        struct recordt_3 {
            T1 b;
            T2 g;
            T3 r;

            recordt_3() {}

            recordt_3(T1 init_b, T2 init_g, T3 init_r)
                : b(init_b), g(init_g), r(init_r) {}

            bool operator==(recordt_3<T1, T2, T3> rhs) {
                return true && b == rhs.b && g == rhs.g && r == rhs.r;
            }

            bool operator!=(recordt_3<T1, T2, T3> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2>
        struct recordt_0 {
            T1 data;
            T2 length;

            recordt_0() {}

            recordt_0(T1 init_data, T2 init_length)
                : data(init_data), length(init_length) {}

            bool operator==(recordt_0<T1, T2> rhs) {
                return true && data == rhs.data && length == rhs.length;
            }

            bool operator!=(recordt_0<T1, T2> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3,typename T4,typename T5,typename T6,typename T7>
        struct recordt_9 {
            T1 day;
            T2 dayOfWeek;
            T3 hours;
            T4 minutes;
            T5 month;
            T6 seconds;
            T7 year;

            recordt_9() {}

            recordt_9(T1 init_day, T2 init_dayOfWeek, T3 init_hours, T4 init_minutes, T5 init_month, T6 init_seconds, T7 init_year)
                : day(init_day), dayOfWeek(init_dayOfWeek), hours(init_hours), minutes(init_minutes), month(init_month), seconds(init_seconds), year(init_year) {}

            bool operator==(recordt_9<T1, T2, T3, T4, T5, T6, T7> rhs) {
                return true && day == rhs.day && dayOfWeek == rhs.dayOfWeek && hours == rhs.hours && minutes == rhs.minutes && month == rhs.month && seconds == rhs.seconds && year == rhs.year;
            }

            bool operator!=(recordt_9<T1, T2, T3, T4, T5, T6, T7> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3>
        struct recordt_5 {
            T1 h;
            T2 s;
            T3 v;

            recordt_5() {}

            recordt_5(T1 init_h, T2 init_s, T3 init_v)
                : h(init_h), s(init_s), v(init_v) {}

            bool operator==(recordt_5<T1, T2, T3> rhs) {
                return true && h == rhs.h && s == rhs.s && v == rhs.v;
            }

            bool operator!=(recordt_5<T1, T2, T3> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1>
        struct recordt_1 {
            T1 lastPulse;

            recordt_1() {}

            recordt_1(T1 init_lastPulse)
                : lastPulse(init_lastPulse) {}

            bool operator==(recordt_1<T1> rhs) {
                return true && lastPulse == rhs.lastPulse;
            }

            bool operator!=(recordt_1<T1> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1>
        struct __attribute__((__packed__)) recordt_8 {
            T1 dayOfWeek;

            recordt_8() {}

            recordt_8(T1 init_dayOfWeek)
                : dayOfWeek(init_dayOfWeek) {}

            bool operator==(recordt_8<T1> rhs) {
                return true && dayOfWeek == rhs.dayOfWeek;
            }

            bool operator!=(recordt_8<T1> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3,typename T4,typename T5,typename T6>
        struct __attribute__((__packed__)) recordt_7 {
            T1 month;
            T2 day;
            T3 year;
            T4 hours;
            T5 minutes;
            T6 seconds;

            recordt_7() {}

            recordt_7(T1 init_month, T2 init_day, T3 init_year, T4 init_hours, T5 init_minutes, T6 init_seconds)
                : month(init_month), day(init_day), year(init_year), hours(init_hours), minutes(init_minutes), seconds(init_seconds) {}

            bool operator==(recordt_7<T1, T2, T3, T4, T5, T6> rhs) {
                return true && month == rhs.month && day == rhs.day && year == rhs.year && hours == rhs.hours && minutes == rhs.minutes && seconds == rhs.seconds;
            }

            bool operator!=(recordt_7<T1, T2, T3, T4, T5, T6> rhs) {
                return !(rhs == *this);
            }
        };


    }
}

namespace juniper {
    namespace closures {
        template<typename T1,typename T2>
        struct closuret_8 {
            T1 buttonState;
            T2 delay;


            closuret_8(T1 init_buttonState, T2 init_delay) :
                buttonState(init_buttonState), delay(init_delay) {}
        };

        template<typename T1>
        struct closuret_1 {
            T1 f;


            closuret_1(T1 init_f) :
                f(init_f) {}
        };

        template<typename T1,typename T2>
        struct closuret_0 {
            T1 f;
            T2 g;


            closuret_0(T1 init_f, T2 init_g) :
                f(init_f), g(init_g) {}
        };

        template<typename T1,typename T2>
        struct closuret_2 {
            T1 f;
            T2 valueA;


            closuret_2(T1 init_f, T2 init_valueA) :
                f(init_f), valueA(init_valueA) {}
        };

        template<typename T1,typename T2,typename T3>
        struct closuret_3 {
            T1 f;
            T2 valueA;
            T3 valueB;


            closuret_3(T1 init_f, T2 init_valueA, T3 init_valueB) :
                f(init_f), valueA(init_valueA), valueB(init_valueB) {}
        };

        template<typename T1>
        struct closuret_4 {
            T1 maybePrevValue;


            closuret_4(T1 init_maybePrevValue) :
                maybePrevValue(init_maybePrevValue) {}
        };

        template<typename T1>
        struct closuret_6 {
            T1 pin;


            closuret_6(T1 init_pin) :
                pin(init_pin) {}
        };

        template<typename T1>
        struct closuret_7 {
            T1 prevState;


            closuret_7(T1 init_prevState) :
                prevState(init_prevState) {}
        };

        template<typename T1,typename T2>
        struct closuret_5 {
            T1 val1;
            T2 val2;


            closuret_5(T1 init_val1, T2 init_val2) :
                val1(init_val1), val2(init_val2) {}
        };


    }
}

namespace Prelude {
    template<typename a>
    struct maybe {
        juniper::variant<a, uint8_t> data;

        maybe() {}

        maybe(juniper::variant<a, uint8_t> initData) : data(initData) {}

        a just() {
            return data.template get<0>();
        }

        uint8_t nothing() {
            return data.template get<1>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(maybe rhs) {
            return data == rhs.data;
        }

        bool operator!=(maybe rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a>
    Prelude::maybe<a> just(a data0) {
        return Prelude::maybe<a>(juniper::variant<a, uint8_t>::template create<0>(data0));
    }

    template<typename a>
    Prelude::maybe<a> nothing() {
        return Prelude::maybe<a>(juniper::variant<a, uint8_t>::template create<1>(0));
    }


}

namespace Prelude {
    template<typename a, typename b>
    struct either {
        juniper::variant<a, b> data;

        either() {}

        either(juniper::variant<a, b> initData) : data(initData) {}

        a left() {
            return data.template get<0>();
        }

        b right() {
            return data.template get<1>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(either rhs) {
            return data == rhs.data;
        }

        bool operator!=(either rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a, typename b>
    Prelude::either<a, b> left(a data0) {
        return Prelude::either<a, b>(juniper::variant<a, b>::template create<0>(data0));
    }

    template<typename a, typename b>
    Prelude::either<a, b> right(b data0) {
        return Prelude::either<a, b>(juniper::variant<a, b>::template create<1>(data0));
    }


}

namespace Prelude {
    template<typename a, int n>
    using list = juniper::records::recordt_0<juniper::array<a, n>, uint32_t>;


}

namespace Prelude {
    template<int n>
    using charlist = juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t>;


}

namespace Prelude {
    template<typename a>
    struct sig {
        juniper::variant<Prelude::maybe<a>> data;

        sig() {}

        sig(juniper::variant<Prelude::maybe<a>> initData) : data(initData) {}

        Prelude::maybe<a> signal() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(sig rhs) {
            return data == rhs.data;
        }

        bool operator!=(sig rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a>
    Prelude::sig<a> signal(Prelude::maybe<a> data0) {
        return Prelude::sig<a>(juniper::variant<Prelude::maybe<a>>::template create<0>(data0));
    }


}

namespace Io {
    struct pinState {
        juniper::variant<uint8_t, uint8_t> data;

        pinState() {}

        pinState(juniper::variant<uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t high() {
            return data.template get<0>();
        }

        uint8_t low() {
            return data.template get<1>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(pinState rhs) {
            return data == rhs.data;
        }

        bool operator!=(pinState rhs) {
            return !(this->operator==(rhs));
        }
    };

    Io::pinState high() {
        return Io::pinState(juniper::variant<uint8_t, uint8_t>::template create<0>(0));
    }

    Io::pinState low() {
        return Io::pinState(juniper::variant<uint8_t, uint8_t>::template create<1>(0));
    }


}

namespace Io {
    struct mode {
        juniper::variant<uint8_t, uint8_t, uint8_t> data;

        mode() {}

        mode(juniper::variant<uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t input() {
            return data.template get<0>();
        }

        uint8_t output() {
            return data.template get<1>();
        }

        uint8_t inputPullup() {
            return data.template get<2>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(mode rhs) {
            return data == rhs.data;
        }

        bool operator!=(mode rhs) {
            return !(this->operator==(rhs));
        }
    };

    Io::mode input() {
        return Io::mode(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    Io::mode output() {
        return Io::mode(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    Io::mode inputPullup() {
        return Io::mode(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }


}

namespace Io {
    struct base {
        juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t> data;

        base() {}

        base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t binary() {
            return data.template get<0>();
        }

        uint8_t octal() {
            return data.template get<1>();
        }

        uint8_t decimal() {
            return data.template get<2>();
        }

        uint8_t hexadecimal() {
            return data.template get<3>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(base rhs) {
            return data == rhs.data;
        }

        bool operator!=(base rhs) {
            return !(this->operator==(rhs));
        }
    };

    Io::base binary() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    Io::base octal() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    Io::base decimal() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }

    Io::base hexadecimal() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<3>(0));
    }


}

namespace Time {
    using timerState = juniper::records::recordt_1<uint32_t>;


}

namespace Button {
    using buttonState = juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>;


}

namespace Vector {
    template<typename a, int n>
    using vector = juniper::array<a, n>;


}

namespace Color {
    using rgb = juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>;


}

namespace Color {
    using rgba = juniper::records::recordt_4<uint8_t, uint8_t, uint8_t, uint8_t>;


}

namespace Color {
    using hsv = juniper::records::recordt_5<float, float, float>;


}

namespace Color {
    using hsva = juniper::records::recordt_6<float, float, float, float>;


}

namespace Ble {
    struct servicet {
        juniper::variant<void *> data;

        servicet() {}

        servicet(juniper::variant<void *> initData) : data(initData) {}

        void * service() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(servicet rhs) {
            return data == rhs.data;
        }

        bool operator!=(servicet rhs) {
            return !(this->operator==(rhs));
        }
    };

    Ble::servicet service(void * data0) {
        return Ble::servicet(juniper::variant<void *>::template create<0>(data0));
    }


}

namespace Ble {
    struct characterstict {
        juniper::variant<void *> data;

        characterstict() {}

        characterstict(juniper::variant<void *> initData) : data(initData) {}

        void * characterstic() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(characterstict rhs) {
            return data == rhs.data;
        }

        bool operator!=(characterstict rhs) {
            return !(this->operator==(rhs));
        }
    };

    Ble::characterstict characterstic(void * data0) {
        return Ble::characterstict(juniper::variant<void *>::template create<0>(data0));
    }


}

namespace Ble {
    struct advertisingFlagt {
        juniper::variant<uint8_t> data;

        advertisingFlagt() {}

        advertisingFlagt(juniper::variant<uint8_t> initData) : data(initData) {}

        uint8_t advertisingFlag() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(advertisingFlagt rhs) {
            return data == rhs.data;
        }

        bool operator!=(advertisingFlagt rhs) {
            return !(this->operator==(rhs));
        }
    };

    Ble::advertisingFlagt advertisingFlag(uint8_t data0) {
        return Ble::advertisingFlagt(juniper::variant<uint8_t>::template create<0>(data0));
    }


}

namespace Ble {
    struct appearancet {
        juniper::variant<uint16_t> data;

        appearancet() {}

        appearancet(juniper::variant<uint16_t> initData) : data(initData) {}

        uint16_t appearance() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(appearancet rhs) {
            return data == rhs.data;
        }

        bool operator!=(appearancet rhs) {
            return !(this->operator==(rhs));
        }
    };

    Ble::appearancet appearance(uint16_t data0) {
        return Ble::appearancet(juniper::variant<uint16_t>::template create<0>(data0));
    }


}

namespace Ble {
    struct secureModet {
        juniper::variant<uint16_t> data;

        secureModet() {}

        secureModet(juniper::variant<uint16_t> initData) : data(initData) {}

        uint16_t secureMode() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(secureModet rhs) {
            return data == rhs.data;
        }

        bool operator!=(secureModet rhs) {
            return !(this->operator==(rhs));
        }
    };

    Ble::secureModet secureMode(uint16_t data0) {
        return Ble::secureModet(juniper::variant<uint16_t>::template create<0>(data0));
    }


}

namespace Ble {
    struct propertiest {
        juniper::variant<uint8_t> data;

        propertiest() {}

        propertiest(juniper::variant<uint8_t> initData) : data(initData) {}

        uint8_t properties() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(propertiest rhs) {
            return data == rhs.data;
        }

        bool operator!=(propertiest rhs) {
            return !(this->operator==(rhs));
        }
    };

    Ble::propertiest properties(uint8_t data0) {
        return Ble::propertiest(juniper::variant<uint8_t>::template create<0>(data0));
    }


}

namespace CWatch {
    using dayDateTimeBLE = juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>;


}

namespace CWatch {
    using dayOfWeekBLE = juniper::records::recordt_8<uint8_t>;


}

namespace CWatch {
    struct month {
        juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t> data;

        month() {}

        month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t january() {
            return data.template get<0>();
        }

        uint8_t february() {
            return data.template get<1>();
        }

        uint8_t march() {
            return data.template get<2>();
        }

        uint8_t april() {
            return data.template get<3>();
        }

        uint8_t may() {
            return data.template get<4>();
        }

        uint8_t june() {
            return data.template get<5>();
        }

        uint8_t july() {
            return data.template get<6>();
        }

        uint8_t august() {
            return data.template get<7>();
        }

        uint8_t september() {
            return data.template get<8>();
        }

        uint8_t october() {
            return data.template get<9>();
        }

        uint8_t november() {
            return data.template get<10>();
        }

        uint8_t december() {
            return data.template get<11>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(month rhs) {
            return data == rhs.data;
        }

        bool operator!=(month rhs) {
            return !(this->operator==(rhs));
        }
    };

    CWatch::month january() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    CWatch::month february() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    CWatch::month march() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }

    CWatch::month april() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<3>(0));
    }

    CWatch::month may() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<4>(0));
    }

    CWatch::month june() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<5>(0));
    }

    CWatch::month july() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<6>(0));
    }

    CWatch::month august() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<7>(0));
    }

    CWatch::month september() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<8>(0));
    }

    CWatch::month october() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<9>(0));
    }

    CWatch::month november() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<10>(0));
    }

    CWatch::month december() {
        return CWatch::month(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<11>(0));
    }


}

namespace CWatch {
    struct dayOfWeek {
        juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t> data;

        dayOfWeek() {}

        dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t sunday() {
            return data.template get<0>();
        }

        uint8_t monday() {
            return data.template get<1>();
        }

        uint8_t tuesday() {
            return data.template get<2>();
        }

        uint8_t wednesday() {
            return data.template get<3>();
        }

        uint8_t thursday() {
            return data.template get<4>();
        }

        uint8_t friday() {
            return data.template get<5>();
        }

        uint8_t saturday() {
            return data.template get<6>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(dayOfWeek rhs) {
            return data == rhs.data;
        }

        bool operator!=(dayOfWeek rhs) {
            return !(this->operator==(rhs));
        }
    };

    CWatch::dayOfWeek sunday() {
        return CWatch::dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    CWatch::dayOfWeek monday() {
        return CWatch::dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    CWatch::dayOfWeek tuesday() {
        return CWatch::dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }

    CWatch::dayOfWeek wednesday() {
        return CWatch::dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<3>(0));
    }

    CWatch::dayOfWeek thursday() {
        return CWatch::dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<4>(0));
    }

    CWatch::dayOfWeek friday() {
        return CWatch::dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<5>(0));
    }

    CWatch::dayOfWeek saturday() {
        return CWatch::dayOfWeek(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>::template create<6>(0));
    }


}

namespace CWatch {
    using datetime = juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>;


}

namespace Gfx {
    struct font {
        juniper::variant<uint8_t, uint8_t, uint8_t> data;

        font() {}

        font(juniper::variant<uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t defaultFont() {
            return data.template get<0>();
        }

        uint8_t freeSans9() {
            return data.template get<1>();
        }

        uint8_t freeSans24() {
            return data.template get<2>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(font rhs) {
            return data == rhs.data;
        }

        bool operator!=(font rhs) {
            return !(this->operator==(rhs));
        }
    };

    Gfx::font defaultFont() {
        return Gfx::font(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    Gfx::font freeSans9() {
        return Gfx::font(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    Gfx::font freeSans24() {
        return Gfx::font(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }


}

namespace Gfx {
    struct align {
        juniper::variant<uint8_t, uint8_t, uint8_t> data;

        align() {}

        align(juniper::variant<uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t centerHorizontally() {
            return data.template get<0>();
        }

        uint8_t centerVertically() {
            return data.template get<1>();
        }

        uint8_t centerBoth() {
            return data.template get<2>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(align rhs) {
            return data == rhs.data;
        }

        bool operator!=(align rhs) {
            return !(this->operator==(rhs));
        }
    };

    Gfx::align centerHorizontally() {
        return Gfx::align(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    Gfx::align centerVertically() {
        return Gfx::align(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    Gfx::align centerBoth() {
        return Gfx::align(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }


}

namespace Prelude {
    void * extractptr(juniper::smartpointer p);
}

namespace Prelude {
    template<typename t39, typename t40, typename t41, typename t42, typename t44>
    juniper::function<juniper::closures::closuret_0<juniper::function<t41, t40(t39)>, juniper::function<t42, t39(t44)>>, t40(t44)> compose(juniper::function<t41, t40(t39)> f, juniper::function<t42, t39(t44)> g);
}

namespace Prelude {
    template<typename t57, typename t58, typename t61, typename t62>
    juniper::function<juniper::closures::closuret_1<juniper::function<t58, t57(t61, t62)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>, t57(t62)>(t61)> curry(juniper::function<t58, t57(t61, t62)> f);
}

namespace Prelude {
    template<typename t73, typename t74, typename t75, typename t77, typename t78>
    juniper::function<juniper::closures::closuret_1<juniper::function<t74, juniper::function<t75, t73(t78)>(t77)>>, t73(t77, t78)> uncurry(juniper::function<t74, juniper::function<t75, t73(t78)>(t77)> f);
}

namespace Prelude {
    template<typename t100, typename t93, typename t94, typename t98, typename t99>
    juniper::function<juniper::closures::closuret_1<juniper::function<t94, t93(t98, t99, t100)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>, juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)>(t99)>(t98)> curry3(juniper::function<t94, t93(t98, t99, t100)> f);
}

namespace Prelude {
    template<typename t114, typename t115, typename t116, typename t117, typename t119, typename t120, typename t121>
    juniper::function<juniper::closures::closuret_1<juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)>>, t114(t119, t120, t121)> uncurry3(juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)> f);
}

namespace Prelude {
    template<typename t132>
    bool eq(t132 x, t132 y);
}

namespace Prelude {
    template<typename t138>
    bool neq(t138 x, t138 y);
}

namespace Prelude {
    template<typename t144, typename t145>
    bool gt(t144 x, t145 y);
}

namespace Prelude {
    template<typename t151, typename t152>
    bool geq(t151 x, t152 y);
}

namespace Prelude {
    template<typename t158, typename t159>
    bool lt(t158 x, t159 y);
}

namespace Prelude {
    template<typename t165, typename t166>
    bool leq(t165 x, t166 y);
}

namespace Prelude {
    bool notf(bool x);
}

namespace Prelude {
    bool andf(bool x, bool y);
}

namespace Prelude {
    bool orf(bool x, bool y);
}

namespace Prelude {
    template<typename t189, typename t190, typename t191>
    t190 apply(juniper::function<t191, t190(t189)> f, t189 x);
}

namespace Prelude {
    template<typename t197, typename t198, typename t199, typename t200>
    t199 apply2(juniper::function<t200, t199(t197, t198)> f, juniper::tuple2<t197, t198> tup);
}

namespace Prelude {
    template<typename t211, typename t212, typename t213, typename t214, typename t215>
    t214 apply3(juniper::function<t215, t214(t211, t212, t213)> f, juniper::tuple3<t211, t212, t213> tup);
}

namespace Prelude {
    template<typename t229, typename t230, typename t231, typename t232, typename t233, typename t234>
    t233 apply4(juniper::function<t234, t233(t229, t230, t231, t232)> f, juniper::tuple4<t229, t230, t231, t232> tup);
}

namespace Prelude {
    template<typename t250, typename t251>
    t250 fst(juniper::tuple2<t250, t251> tup);
}

namespace Prelude {
    template<typename t255, typename t256>
    t256 snd(juniper::tuple2<t255, t256> tup);
}

namespace Prelude {
    template<typename t260>
    t260 add(t260 numA, t260 numB);
}

namespace Prelude {
    template<typename t262>
    t262 sub(t262 numA, t262 numB);
}

namespace Prelude {
    template<typename t264>
    t264 mul(t264 numA, t264 numB);
}

namespace Prelude {
    template<typename t266>
    t266 div(t266 numA, t266 numB);
}

namespace Prelude {
    template<typename t272, typename t273>
    juniper::tuple2<t273, t272> swap(juniper::tuple2<t272, t273> tup);
}

namespace Prelude {
    template<typename t279, typename t280, typename t281>
    t279 until(juniper::function<t280, bool(t279)> p, juniper::function<t281, t279(t279)> f, t279 a0);
}

namespace Prelude {
    template<typename t290>
    juniper::unit ignore(t290 val);
}

namespace Prelude {
    uint16_t u8ToU16(uint8_t n);
}

namespace Prelude {
    uint32_t u8ToU32(uint8_t n);
}

namespace Prelude {
    int8_t u8ToI8(uint8_t n);
}

namespace Prelude {
    int16_t u8ToI16(uint8_t n);
}

namespace Prelude {
    int32_t u8ToI32(uint8_t n);
}

namespace Prelude {
    float u8ToFloat(uint8_t n);
}

namespace Prelude {
    double u8ToDouble(uint8_t n);
}

namespace Prelude {
    uint8_t u16ToU8(uint16_t n);
}

namespace Prelude {
    uint32_t u16ToU32(uint16_t n);
}

namespace Prelude {
    int8_t u16ToI8(uint16_t n);
}

namespace Prelude {
    int16_t u16ToI16(uint16_t n);
}

namespace Prelude {
    int32_t u16ToI32(uint16_t n);
}

namespace Prelude {
    float u16ToFloat(uint16_t n);
}

namespace Prelude {
    double u16ToDouble(uint16_t n);
}

namespace Prelude {
    uint8_t u32ToU8(uint32_t n);
}

namespace Prelude {
    uint16_t u32ToU16(uint32_t n);
}

namespace Prelude {
    int8_t u32ToI8(uint32_t n);
}

namespace Prelude {
    int16_t u32ToI16(uint32_t n);
}

namespace Prelude {
    int32_t u32ToI32(uint32_t n);
}

namespace Prelude {
    float u32ToFloat(uint32_t n);
}

namespace Prelude {
    double u32ToDouble(uint32_t n);
}

namespace Prelude {
    uint8_t i8ToU8(int8_t n);
}

namespace Prelude {
    uint16_t i8ToU16(int8_t n);
}

namespace Prelude {
    uint32_t i8ToU32(int8_t n);
}

namespace Prelude {
    int16_t i8ToI16(int8_t n);
}

namespace Prelude {
    int32_t i8ToI32(int8_t n);
}

namespace Prelude {
    float i8ToFloat(int8_t n);
}

namespace Prelude {
    double i8ToDouble(int8_t n);
}

namespace Prelude {
    uint8_t i16ToU8(int16_t n);
}

namespace Prelude {
    uint16_t i16ToU16(int16_t n);
}

namespace Prelude {
    uint32_t i16ToU32(int16_t n);
}

namespace Prelude {
    int8_t i16ToI8(int16_t n);
}

namespace Prelude {
    int32_t i16ToI32(int16_t n);
}

namespace Prelude {
    float i16ToFloat(int16_t n);
}

namespace Prelude {
    double i16ToDouble(int16_t n);
}

namespace Prelude {
    uint8_t i32ToU8(int32_t n);
}

namespace Prelude {
    uint16_t i32ToU16(int32_t n);
}

namespace Prelude {
    uint32_t i32ToU32(int32_t n);
}

namespace Prelude {
    int8_t i32ToI8(int32_t n);
}

namespace Prelude {
    int16_t i32ToI16(int32_t n);
}

namespace Prelude {
    float i32ToFloat(int32_t n);
}

namespace Prelude {
    double i32ToDouble(int32_t n);
}

namespace Prelude {
    uint8_t floatToU8(float n);
}

namespace Prelude {
    uint16_t floatToU16(float n);
}

namespace Prelude {
    uint32_t floatToU32(float n);
}

namespace Prelude {
    int8_t floatToI8(float n);
}

namespace Prelude {
    int16_t floatToI16(float n);
}

namespace Prelude {
    int32_t floatToI32(float n);
}

namespace Prelude {
    double floatToDouble(float n);
}

namespace Prelude {
    uint8_t doubleToU8(double n);
}

namespace Prelude {
    uint16_t doubleToU16(double n);
}

namespace Prelude {
    uint32_t doubleToU32(double n);
}

namespace Prelude {
    int8_t doubleToI8(double n);
}

namespace Prelude {
    int16_t doubleToI16(double n);
}

namespace Prelude {
    int32_t doubleToI32(double n);
}

namespace Prelude {
    float doubleToFloat(double n);
}

namespace Prelude {
    template<typename t349>
    uint8_t toUInt8(t349 n);
}

namespace Prelude {
    template<typename t351>
    int8_t toInt8(t351 n);
}

namespace Prelude {
    template<typename t353>
    uint16_t toUInt16(t353 n);
}

namespace Prelude {
    template<typename t355>
    int16_t toInt16(t355 n);
}

namespace Prelude {
    template<typename t357>
    uint32_t toUInt32(t357 n);
}

namespace Prelude {
    template<typename t359>
    int32_t toInt32(t359 n);
}

namespace Prelude {
    template<typename t361>
    float toFloat(t361 n);
}

namespace Prelude {
    template<typename t363>
    double toDouble(t363 n);
}

namespace Prelude {
    template<typename t365>
    t365 fromUInt8(uint8_t n);
}

namespace Prelude {
    template<typename t367>
    t367 fromInt8(int8_t n);
}

namespace Prelude {
    template<typename t369>
    t369 fromUInt16(uint16_t n);
}

namespace Prelude {
    template<typename t371>
    t371 fromInt16(int16_t n);
}

namespace Prelude {
    template<typename t373>
    t373 fromUInt32(uint32_t n);
}

namespace Prelude {
    template<typename t375>
    t375 fromInt32(int32_t n);
}

namespace Prelude {
    template<typename t377>
    t377 fromFloat(float n);
}

namespace Prelude {
    template<typename t379>
    t379 fromDouble(double n);
}

namespace Prelude {
    template<typename t381, typename t382>
    t382 cast(t381 x);
}

namespace List {
    template<typename t387, typename t390, typename t394, int c4>
    juniper::records::recordt_0<juniper::array<t394, c4>, uint32_t> map(juniper::function<t387, t394(t390)> f, juniper::records::recordt_0<juniper::array<t390, c4>, uint32_t> lst);
}

namespace List {
    template<typename t398, typename t400, typename t403, int c7>
    t398 foldl(juniper::function<t400, t398(t403, t398)> f, t398 initState, juniper::records::recordt_0<juniper::array<t403, c7>, uint32_t> lst);
}

namespace List {
    template<typename t410, typename t412, typename t418, int c9>
    t410 foldr(juniper::function<t412, t410(t418, t410)> f, t410 initState, juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t> lst);
}

namespace List {
    template<typename t435, int c11, int c12, int c13>
    juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t> append(juniper::records::recordt_0<juniper::array<t435, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t435, c12>, uint32_t> lstB);
}

namespace List {
    template<typename t448, int c18>
    Prelude::maybe<t448> nth(uint32_t i, juniper::records::recordt_0<juniper::array<t448, c18>, uint32_t> lst);
}

namespace List {
    template<typename t460, int c20, int c21>
    juniper::records::recordt_0<juniper::array<t460, (c21)*(c20)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t460, c20>, uint32_t>, c21>, uint32_t> listOfLists);
}

namespace Math {
    template<typename t464>
    t464 min_(t464 x, t464 y);
}

namespace List {
    template<typename t478, int c26, int c27>
    juniper::records::recordt_0<juniper::array<t478, c26>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t478, c27>, uint32_t> lst);
}

namespace List {
    template<typename t484, typename t487, int c30>
    bool all(juniper::function<t484, bool(t487)> pred, juniper::records::recordt_0<juniper::array<t487, c30>, uint32_t> lst);
}

namespace List {
    template<typename t494, typename t497, int c32>
    bool any(juniper::function<t494, bool(t497)> pred, juniper::records::recordt_0<juniper::array<t497, c32>, uint32_t> lst);
}

namespace List {
    template<typename t502, int c34>
    juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> pushBack(t502 elem, juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> lst);
}

namespace List {
    template<typename t517, int c36>
    juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> pushOffFront(t517 elem, juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> lst);
}

namespace List {
    template<typename t532, int c40>
    juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> setNth(uint32_t index, t532 elem, juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> lst);
}

namespace List {
    template<typename t537, int c42>
    juniper::records::recordt_0<juniper::array<t537, c42>, uint32_t> replicate(uint32_t numOfElements, t537 elem);
}

namespace List {
    template<typename t548, int c43>
    juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> remove(t548 elem, juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> lst);
}

namespace List {
    template<typename t556, int c48>
    juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> lst);
}

namespace List {
    template<typename t566, typename t569, int c50>
    juniper::unit foreach(juniper::function<t566, juniper::unit(t569)> f, juniper::records::recordt_0<juniper::array<t569, c50>, uint32_t> lst);
}

namespace List {
    template<typename t584, int c52>
    Prelude::maybe<t584> last(juniper::records::recordt_0<juniper::array<t584, c52>, uint32_t> lst);
}

namespace List {
    template<typename t598, int c54>
    Prelude::maybe<t598> max_(juniper::records::recordt_0<juniper::array<t598, c54>, uint32_t> lst);
}

namespace List {
    template<typename t615, int c58>
    Prelude::maybe<t615> min_(juniper::records::recordt_0<juniper::array<t615, c58>, uint32_t> lst);
}

namespace List {
    template<typename t623, int c62>
    bool member(t623 elem, juniper::records::recordt_0<juniper::array<t623, c62>, uint32_t> lst);
}

namespace List {
    template<typename t638, typename t640, int c64>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t638, c64>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t640, c64>, uint32_t> lstB);
}

namespace List {
    template<typename t651, typename t652, int c68>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t651, t652>, c68>, uint32_t> lst);
}

namespace List {
    template<typename t660, int c72>
    t660 sum(juniper::records::recordt_0<juniper::array<t660, c72>, uint32_t> lst);
}

namespace List {
    template<typename t678, int c74>
    t678 average(juniper::records::recordt_0<juniper::array<t678, c74>, uint32_t> lst);
}

namespace List {
    template<typename t684, typename t685, typename t686, int c76>
    juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> sort(juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> lst, juniper::function<t684, t685(t686)> key);
}

namespace Signal {
    template<typename t690, typename t692, typename t707>
    Prelude::sig<t707> map(juniper::function<t692, t707(t690)> f, Prelude::sig<t690> s);
}

namespace Signal {
    template<typename t715, typename t716>
    juniper::unit sink(juniper::function<t716, juniper::unit(t715)> f, Prelude::sig<t715> s);
}

namespace Signal {
    template<typename t725, typename t739>
    Prelude::sig<t739> filter(juniper::function<t725, bool(t739)> f, Prelude::sig<t739> s);
}

namespace Signal {
    template<typename t746>
    Prelude::sig<t746> merge(Prelude::sig<t746> sigA, Prelude::sig<t746> sigB);
}

namespace Signal {
    template<typename t750, int c77>
    Prelude::sig<t750> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t750>, c77>, uint32_t> sigs);
}

namespace Signal {
    template<typename t773, typename t797>
    Prelude::sig<Prelude::either<t797, t773>> join(Prelude::sig<t797> sigA, Prelude::sig<t773> sigB);
}

namespace Signal {
    template<typename t820>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t820> s);
}

namespace Signal {
    template<typename t827, typename t829, typename t847>
    Prelude::sig<t847> foldP(juniper::function<t829, t847(t827, t847)> f, juniper::shared_ptr<t847> state0, Prelude::sig<t827> incoming);
}

namespace Signal {
    template<typename t857>
    Prelude::sig<t857> dropRepeats(juniper::shared_ptr<Prelude::maybe<t857>> maybePrevValue, Prelude::sig<t857> incoming);
}

namespace Signal {
    template<typename t875>
    Prelude::sig<t875> latch(juniper::shared_ptr<t875> prevValue, Prelude::sig<t875> incoming);
}

namespace Signal {
    template<typename t888, typename t889, typename t892, typename t901>
    Prelude::sig<t888> map2(juniper::function<t889, t888(t892, t901)> f, juniper::shared_ptr<juniper::tuple2<t892, t901>> state, Prelude::sig<t892> incomingA, Prelude::sig<t901> incomingB);
}

namespace Signal {
    template<typename t933, int c79>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t933, c79>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t933, c79>, uint32_t>> pastValues, Prelude::sig<t933> incoming);
}

namespace Signal {
    template<typename t944>
    Prelude::sig<t944> constant(t944 val);
}

namespace Signal {
    template<typename t954>
    Prelude::sig<Prelude::maybe<t954>> meta(Prelude::sig<t954> sigA);
}

namespace Signal {
    template<typename t971>
    Prelude::sig<t971> unmeta(Prelude::sig<Prelude::maybe<t971>> sigA);
}

namespace Signal {
    template<typename t984, typename t985>
    Prelude::sig<juniper::tuple2<t984, t985>> zip(juniper::shared_ptr<juniper::tuple2<t984, t985>> state, Prelude::sig<t984> sigA, Prelude::sig<t985> sigB);
}

namespace Signal {
    template<typename t1016, typename t1023>
    juniper::tuple2<Prelude::sig<t1016>, Prelude::sig<t1023>> unzip(Prelude::sig<juniper::tuple2<t1016, t1023>> incoming);
}

namespace Signal {
    template<typename t1030, typename t1031>
    Prelude::sig<t1030> toggle(t1030 val1, t1030 val2, juniper::shared_ptr<t1030> state, Prelude::sig<t1031> incoming);
}

namespace Io {
    Io::pinState toggle(Io::pinState p);
}

namespace Io {
    juniper::unit printStr(const char * str);
}

namespace Io {
    template<int c81>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c81>, uint32_t> cl);
}

namespace Io {
    juniper::unit printFloat(float f);
}

namespace Io {
    juniper::unit printInt(int32_t n);
}

namespace Io {
    template<typename t1059>
    t1059 baseToInt(Io::base b);
}

namespace Io {
    juniper::unit printIntBase(int32_t n, Io::base b);
}

namespace Io {
    juniper::unit printFloatPlaces(float f, int32_t numPlaces);
}

namespace Io {
    juniper::unit beginSerial(uint32_t speed);
}

namespace Io {
    uint8_t pinStateToInt(Io::pinState value);
}

namespace Io {
    Io::pinState intToPinState(uint8_t value);
}

namespace Io {
    juniper::unit digWrite(uint16_t pin, Io::pinState value);
}

namespace Io {
    Io::pinState digRead(uint16_t pin);
}

namespace Io {
    Prelude::sig<Io::pinState> digIn(uint16_t pin);
}

namespace Io {
    juniper::unit digOut(uint16_t pin, Prelude::sig<Io::pinState> sig);
}

namespace Io {
    uint16_t anaRead(uint16_t pin);
}

namespace Io {
    juniper::unit anaWrite(uint16_t pin, uint8_t value);
}

namespace Io {
    Prelude::sig<uint16_t> anaIn(uint16_t pin);
}

namespace Io {
    juniper::unit anaOut(uint16_t pin, Prelude::sig<uint8_t> sig);
}

namespace Io {
    uint8_t pinModeToInt(Io::mode m);
}

namespace Io {
    Io::mode intToPinMode(uint8_t m);
}

namespace Io {
    juniper::unit setPinMode(uint16_t pin, Io::mode m);
}

namespace Io {
    Prelude::sig<juniper::unit> risingEdge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState);
}

namespace Io {
    Prelude::sig<juniper::unit> fallingEdge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState);
}

namespace Io {
    Prelude::sig<Io::pinState> edge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState);
}

namespace Maybe {
    template<typename t1191, typename t1193, typename t1202>
    Prelude::maybe<t1202> map(juniper::function<t1193, t1202(t1191)> f, Prelude::maybe<t1191> maybeVal);
}

namespace Maybe {
    template<typename t1206>
    t1206 get(Prelude::maybe<t1206> maybeVal);
}

namespace Maybe {
    template<typename t1209>
    bool isJust(Prelude::maybe<t1209> maybeVal);
}

namespace Maybe {
    template<typename t1212>
    bool isNothing(Prelude::maybe<t1212> maybeVal);
}

namespace Maybe {
    template<typename t1218>
    uint8_t count(Prelude::maybe<t1218> maybeVal);
}

namespace Maybe {
    template<typename t1224, typename t1225, typename t1226>
    t1224 foldl(juniper::function<t1226, t1224(t1225, t1224)> f, t1224 initState, Prelude::maybe<t1225> maybeVal);
}

namespace Maybe {
    template<typename t1234, typename t1235, typename t1236>
    t1234 fodlr(juniper::function<t1236, t1234(t1235, t1234)> f, t1234 initState, Prelude::maybe<t1235> maybeVal);
}

namespace Maybe {
    template<typename t1247, typename t1248>
    juniper::unit iter(juniper::function<t1248, juniper::unit(t1247)> f, Prelude::maybe<t1247> maybeVal);
}

namespace Time {
    juniper::unit wait(uint32_t time);
}

namespace Time {
    uint32_t now();
}

namespace Time {
    juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> state();
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> state);
}

namespace Math {
    double degToRad(double degrees);
}

namespace Math {
    double radToDeg(double radians);
}

namespace Math {
    double acos_(double x);
}

namespace Math {
    double asin_(double x);
}

namespace Math {
    double atan_(double x);
}

namespace Math {
    double atan2_(double y, double x);
}

namespace Math {
    double cos_(double x);
}

namespace Math {
    double cosh_(double x);
}

namespace Math {
    double sin_(double x);
}

namespace Math {
    double sinh_(double x);
}

namespace Math {
    double tan_(double x);
}

namespace Math {
    double tanh_(double x);
}

namespace Math {
    double exp_(double x);
}

namespace Math {
    juniper::tuple2<double, int32_t> frexp_(double x);
}

namespace Math {
    double ldexp_(double x, int16_t exponent);
}

namespace Math {
    double log_(double x);
}

namespace Math {
    double log10_(double x);
}

namespace Math {
    juniper::tuple2<double, double> modf_(double x);
}

namespace Math {
    double pow_(double x, double y);
}

namespace Math {
    double sqrt_(double x);
}

namespace Math {
    double ceil_(double x);
}

namespace Math {
    double fabs_(double x);
}

namespace Math {
    double floor_(double x);
}

namespace Math {
    double fmod_(double x, double y);
}

namespace Math {
    double round_(double x);
}

namespace Math {
    template<typename t1315>
    t1315 max_(t1315 x, t1315 y);
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2);
}

namespace Math {
    template<typename t1318>
    t1318 clamp(t1318 x, t1318 min, t1318 max);
}

namespace Math {
    template<typename t1323>
    int8_t sign(t1323 n);
}

namespace Button {
    juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> state();
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> buttonState);
}

namespace Button {
    Prelude::sig<Io::pinState> debounce(Prelude::sig<Io::pinState> incoming, juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> buttonState);
}

namespace Vector {
    template<typename t1371, int c82>
    juniper::array<t1371, c82> add(juniper::array<t1371, c82> v1, juniper::array<t1371, c82> v2);
}

namespace Vector {
    template<typename t1373, int c85>
    juniper::array<t1373, c85> zero();
}

namespace Vector {
    template<typename t1379, int c86>
    juniper::array<t1379, c86> subtract(juniper::array<t1379, c86> v1, juniper::array<t1379, c86> v2);
}

namespace Vector {
    template<typename t1383, int c89>
    juniper::array<t1383, c89> scale(t1383 scalar, juniper::array<t1383, c89> v);
}

namespace Vector {
    template<typename t1389, int c91>
    t1389 dot(juniper::array<t1389, c91> v1, juniper::array<t1389, c91> v2);
}

namespace Vector {
    template<typename t1395, int c94>
    t1395 magnitude2(juniper::array<t1395, c94> v);
}

namespace Vector {
    template<typename t1397, int c97>
    double magnitude(juniper::array<t1397, c97> v);
}

namespace Vector {
    template<typename t1413, int c99>
    juniper::array<t1413, c99> multiply(juniper::array<t1413, c99> u, juniper::array<t1413, c99> v);
}

namespace Vector {
    template<typename t1422, int c102>
    juniper::array<t1422, c102> normalize(juniper::array<t1422, c102> v);
}

namespace Vector {
    template<typename t1433, int c106>
    double angle(juniper::array<t1433, c106> v1, juniper::array<t1433, c106> v2);
}

namespace Vector {
    template<typename t1475>
    juniper::array<t1475, 3> cross(juniper::array<t1475, 3> u, juniper::array<t1475, 3> v);
}

namespace Vector {
    template<typename t1477, int c122>
    juniper::array<t1477, c122> project(juniper::array<t1477, c122> a, juniper::array<t1477, c122> b);
}

namespace Vector {
    template<typename t1493, int c126>
    juniper::array<t1493, c126> projectPlane(juniper::array<t1493, c126> a, juniper::array<t1493, c126> m);
}

namespace CharList {
    template<int c129>
    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> str);
}

namespace CharList {
    template<int c130>
    juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> str);
}

namespace CharList {
    template<int c131>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t> i32ToCharList(int32_t m);
}

namespace CharList {
    template<int c132>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c132>, uint32_t> s);
}

namespace CharList {
    template<int c133, int c134, int c135>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c133)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> sB);
}

namespace CharList {
    template<int c142, int c143>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c142)*(-1)))+((c143)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c142)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c143)>, uint32_t> sB);
}

namespace Random {
    int32_t random_(int32_t low, int32_t high);
}

namespace Random {
    juniper::unit seed(uint32_t n);
}

namespace Random {
    template<typename t1563, int c147>
    t1563 choice(juniper::records::recordt_0<juniper::array<t1563, c147>, uint32_t> lst);
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_5<float, float, float> color);
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> color);
}

namespace Arcada {
    bool arcadaBegin();
}

namespace Arcada {
    juniper::unit displayBegin();
}

namespace Arcada {
    juniper::unit setBacklight(uint8_t level);
}

namespace Arcada {
    uint16_t displayWidth();
}

namespace Arcada {
    uint16_t displayHeight();
}

namespace Arcada {
    bool createDoubleBuffer();
}

namespace Arcada {
    bool blitDoubleBuffer();
}

namespace Ble {
    juniper::unit beginService(Ble::servicet s);
}

namespace Ble {
    juniper::unit beginCharacterstic(Ble::characterstict c);
}

namespace Ble {
    template<int c149>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c149>, uint32_t> bytes);
}

namespace Ble {
    uint16_t writeCharacterstic8(Ble::characterstict c, uint8_t n);
}

namespace Ble {
    uint16_t writeCharacterstic16(Ble::characterstict c, uint16_t n);
}

namespace Ble {
    uint16_t writeCharacterstic32(Ble::characterstict c, uint32_t n);
}

namespace Ble {
    uint16_t writeCharactersticSigned32(Ble::characterstict c, int32_t n);
}

namespace Ble {
    template<typename t1917>
    uint16_t writeGeneric(Ble::characterstict c, t1917 x);
}

namespace Ble {
    template<typename t1922>
    t1922 readGeneric(Ble::characterstict c);
}

namespace Ble {
    juniper::unit setCharacteristicPermission(Ble::characterstict c, Ble::secureModet readPermission, Ble::secureModet writePermission);
}

namespace Ble {
    juniper::unit setCharacteristicProperties(Ble::characterstict c, Ble::propertiest prop);
}

namespace Ble {
    juniper::unit setCharacteristicFixedLen(Ble::characterstict c, uint32_t size);
}

namespace Ble {
    juniper::unit bluefruitBegin();
}

namespace Ble {
    juniper::unit bluefruitPeriphSetConnInterval(uint16_t minTime, uint16_t maxTime);
}

namespace Ble {
    juniper::unit bluefruitSetTxPower(int8_t power);
}

namespace Ble {
    juniper::unit bluefruitSetName(const char * name);
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddFlags(Ble::advertisingFlagt flag);
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddTxPower();
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddAppearance(Ble::appearancet app);
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddService(Ble::servicet serv);
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddName();
}

namespace Ble {
    juniper::unit bluefruitAdvertisingRestartOnDisconnect(bool restart);
}

namespace Ble {
    juniper::unit bluefruitAdvertisingSetInterval(uint16_t slow, uint16_t fast);
}

namespace Ble {
    juniper::unit bluefruitAdvertisingSetFastTimeout(uint16_t sec);
}

namespace Ble {
    juniper::unit bluefruitAdvertisingStart(uint16_t timeout);
}

namespace CWatch {
    juniper::unit setup();
}

namespace CWatch {
    bool isLeapYear(uint32_t y);
}

namespace CWatch {
    uint8_t daysInMonth(uint32_t y, CWatch::month m);
}

namespace CWatch {
    CWatch::month nextMonth(CWatch::month m);
}

namespace CWatch {
    CWatch::dayOfWeek nextDayOfWeek(CWatch::dayOfWeek dw);
}

namespace CWatch {
    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> dayOfWeekToCharList(CWatch::dayOfWeek dw);
}

namespace CWatch {
    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> monthToCharList(CWatch::month m);
}

namespace CWatch {
    juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> secondTick(juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> d);
}

namespace CWatch {
    juniper::unit processBluetoothUpdates();
}

namespace Gfx {
    juniper::unit drawFastHLine565(int16_t x, int16_t y, int16_t w, uint16_t c);
}

namespace Gfx {
    juniper::unit drawVerticalGradient(int16_t x0i, int16_t y0i, int16_t w, int16_t h, juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c1, juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c2);
}

namespace Gfx {
    template<int c150>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c150)>, uint32_t> cl);
}

namespace Gfx {
    template<int c151>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c151)>, uint32_t> cl);
}

namespace Gfx {
    juniper::unit setCursor(int16_t x, int16_t y);
}

namespace Gfx {
    juniper::unit setFont(Gfx::font f);
}

namespace Gfx {
    juniper::unit setTextColor(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c);
}

namespace Gfx {
    juniper::unit setTextSize(uint8_t size);
}

namespace CWatch {
    bool loop();
}

namespace Gfx {
    juniper::unit fillScreen(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c);
}

namespace Gfx {
    juniper::unit drawPixel(int16_t x, int16_t y, juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c);
}

namespace Gfx {
    juniper::unit drawPixel565(uint16_t x, uint16_t y, uint16_t c);
}

namespace Gfx {
    juniper::unit printString(const char * s);
}

namespace Gfx {
    template<int c176>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c176>, uint32_t> cl);
}

namespace Gfx {
    juniper::unit setTextWrap(bool wrap);
}

namespace List {
    
template<typename t, typename clos, typename keyT, int n>
uint32_t partition(list<t, n>& lst, juniper::function<clos, keyT(t)> keyF, uint32_t lo, uint32_t hi) {
    keyT pivot = keyF(lst.data[hi]);
    
    uint32_t i = lo;

    for (uint32_t j = lo; j < (hi - 1); j++) {
        if (keyF(lst.data[j]) <= pivot) {
            t tmp = lst.data[i];
            lst.data[i] = lst.data[j];
            lst.data[j] = tmp;
            i++;
        }
    }

    i++;

    t tmp = lst.data[i];
    lst.data[i] = lst.data[hi];
    lst.data[hi] = tmp;

    return i;
}

template <typename t, typename clos, typename keyT, int n>
void quicksort(list<t, n>& lst, juniper::function<clos, keyT(t)> keyF, uint32_t lo, uint32_t hi) {
    if (lo >= hi || lo < 0) {
        return;
    }

    uint32_t p = partition<t, clos, keyT, n>(lst, keyF, lo, hi);

    quicksort(lst, lo, p-1);
    quicksort(lst, p+1, hi);
}

}

namespace Arcada {
    
Adafruit_Arcada arcada;

}

namespace CWatch {
    
BLEUuid timeUuid(UUID16_SVC_CURRENT_TIME);
BLEService rawTimeService(timeUuid);

bool rawHasNewDayDateTime = false;
BLEUuid dayDateTimeUuid(UUID16_CHR_DAY_DATE_TIME);
BLECharacteristic rawDayDateTimeCharacterstic(dayDateTimeUuid);
void onWriteDayDateTime(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    rawHasNewDayDateTime = true;
}

bool rawHasNewDayOfWeek = false;
BLEUuid dayOfWeekUuid(UUID16_CHR_DAY_OF_WEEK);
BLECharacteristic rawDayOfWeek(dayOfWeekUuid);
void onWriteDayOfWeek(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    rawHasNewDayOfWeek = true;
}

}

namespace Prelude {
    void * extractptr(juniper::smartpointer p) {
        return (([&]() -> void * {
            void * ret;
            
            (([&]() -> juniper::unit {
                ret = p.get()->get();
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t39, typename t40, typename t41, typename t42, typename t44>
    juniper::function<juniper::closures::closuret_0<juniper::function<t41, t40(t39)>, juniper::function<t42, t39(t44)>>, t40(t44)> compose(juniper::function<t41, t40(t39)> f, juniper::function<t42, t39(t44)> g) {
        return (([&]() -> juniper::function<juniper::closures::closuret_0<juniper::function<t41, t40(t39)>, juniper::function<t42, t39(t44)>>, t40(t44)> {
            using a = t44;
            using b = t39;
            using c = t40;
            return juniper::function<juniper::closures::closuret_0<juniper::function<t41, t40(t39)>, juniper::function<t42, t39(t44)>>, t40(t44)>(juniper::closures::closuret_0<juniper::function<t41, t40(t39)>, juniper::function<t42, t39(t44)>>(f, g), [](juniper::closures::closuret_0<juniper::function<t41, t40(t39)>, juniper::function<t42, t39(t44)>>& junclosure, t44 x) -> t40 { 
                juniper::function<t41, t40(t39)>& f = junclosure.f;
                juniper::function<t42, t39(t44)>& g = junclosure.g;
                return f(g(x));
             });
        })());
    }
}

namespace Prelude {
    template<typename t57, typename t58, typename t61, typename t62>
    juniper::function<juniper::closures::closuret_1<juniper::function<t58, t57(t61, t62)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>, t57(t62)>(t61)> curry(juniper::function<t58, t57(t61, t62)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t58, t57(t61, t62)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>, t57(t62)>(t61)> {
            using a = t61;
            using b = t62;
            using c = t57;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t58, t57(t61, t62)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>, t57(t62)>(t61)>(juniper::closures::closuret_1<juniper::function<t58, t57(t61, t62)>>(f), [](juniper::closures::closuret_1<juniper::function<t58, t57(t61, t62)>>& junclosure, t61 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>, t57(t62)> { 
                juniper::function<t58, t57(t61, t62)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>, t57(t62)>(juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t58, t57(t61, t62)>, t61>& junclosure, t62 valueB) -> t57 { 
                    juniper::function<t58, t57(t61, t62)>& f = junclosure.f;
                    t61& valueA = junclosure.valueA;
                    return f(valueA, valueB);
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t73, typename t74, typename t75, typename t77, typename t78>
    juniper::function<juniper::closures::closuret_1<juniper::function<t74, juniper::function<t75, t73(t78)>(t77)>>, t73(t77, t78)> uncurry(juniper::function<t74, juniper::function<t75, t73(t78)>(t77)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t74, juniper::function<t75, t73(t78)>(t77)>>, t73(t77, t78)> {
            using a = t77;
            using b = t78;
            using c = t73;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t74, juniper::function<t75, t73(t78)>(t77)>>, t73(t77,t78)>(juniper::closures::closuret_1<juniper::function<t74, juniper::function<t75, t73(t78)>(t77)>>(f), [](juniper::closures::closuret_1<juniper::function<t74, juniper::function<t75, t73(t78)>(t77)>>& junclosure, t77 valueA, t78 valueB) -> t73 { 
                juniper::function<t74, juniper::function<t75, t73(t78)>(t77)>& f = junclosure.f;
                return f(valueA)(valueB);
             });
        })());
    }
}

namespace Prelude {
    template<typename t100, typename t93, typename t94, typename t98, typename t99>
    juniper::function<juniper::closures::closuret_1<juniper::function<t94, t93(t98, t99, t100)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>, juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)>(t99)>(t98)> curry3(juniper::function<t94, t93(t98, t99, t100)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t94, t93(t98, t99, t100)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>, juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)>(t99)>(t98)> {
            using a = t98;
            using b = t99;
            using c = t100;
            using d = t93;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t94, t93(t98, t99, t100)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>, juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)>(t99)>(t98)>(juniper::closures::closuret_1<juniper::function<t94, t93(t98, t99, t100)>>(f), [](juniper::closures::closuret_1<juniper::function<t94, t93(t98, t99, t100)>>& junclosure, t98 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>, juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)>(t99)> { 
                juniper::function<t94, t93(t98, t99, t100)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>, juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)>(t99)>(juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t94, t93(t98, t99, t100)>, t98>& junclosure, t99 valueB) -> juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)> { 
                    juniper::function<t94, t93(t98, t99, t100)>& f = junclosure.f;
                    t98& valueA = junclosure.valueA;
                    return juniper::function<juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>, t93(t100)>(juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>(f, valueA, valueB), [](juniper::closures::closuret_3<juniper::function<t94, t93(t98, t99, t100)>, t98, t99>& junclosure, t100 valueC) -> t93 { 
                        juniper::function<t94, t93(t98, t99, t100)>& f = junclosure.f;
                        t98& valueA = junclosure.valueA;
                        t99& valueB = junclosure.valueB;
                        return f(valueA, valueB, valueC);
                     });
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t114, typename t115, typename t116, typename t117, typename t119, typename t120, typename t121>
    juniper::function<juniper::closures::closuret_1<juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)>>, t114(t119, t120, t121)> uncurry3(juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)>>, t114(t119, t120, t121)> {
            using a = t119;
            using b = t120;
            using c = t121;
            using d = t114;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)>>, t114(t119,t120,t121)>(juniper::closures::closuret_1<juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)>>(f), [](juniper::closures::closuret_1<juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)>>& junclosure, t119 valueA, t120 valueB, t121 valueC) -> t114 { 
                juniper::function<t115, juniper::function<t116, juniper::function<t117, t114(t121)>(t120)>(t119)>& f = junclosure.f;
                return f(valueA)(valueB)(valueC);
             });
        })());
    }
}

namespace Prelude {
    template<typename t132>
    bool eq(t132 x, t132 y) {
        return (([&]() -> bool {
            using a = t132;
            return ((bool) (x == y));
        })());
    }
}

namespace Prelude {
    template<typename t138>
    bool neq(t138 x, t138 y) {
        return ((bool) (x != y));
    }
}

namespace Prelude {
    template<typename t144, typename t145>
    bool gt(t144 x, t145 y) {
        return ((bool) (x > y));
    }
}

namespace Prelude {
    template<typename t151, typename t152>
    bool geq(t151 x, t152 y) {
        return ((bool) (x >= y));
    }
}

namespace Prelude {
    template<typename t158, typename t159>
    bool lt(t158 x, t159 y) {
        return ((bool) (x < y));
    }
}

namespace Prelude {
    template<typename t165, typename t166>
    bool leq(t165 x, t166 y) {
        return ((bool) (x <= y));
    }
}

namespace Prelude {
    bool notf(bool x) {
        return !(x);
    }
}

namespace Prelude {
    bool andf(bool x, bool y) {
        return ((bool) (x && y));
    }
}

namespace Prelude {
    bool orf(bool x, bool y) {
        return ((bool) (x || y));
    }
}

namespace Prelude {
    template<typename t189, typename t190, typename t191>
    t190 apply(juniper::function<t191, t190(t189)> f, t189 x) {
        return (([&]() -> t190 {
            using a = t189;
            using b = t190;
            return f(x);
        })());
    }
}

namespace Prelude {
    template<typename t197, typename t198, typename t199, typename t200>
    t199 apply2(juniper::function<t200, t199(t197, t198)> f, juniper::tuple2<t197, t198> tup) {
        return (([&]() -> t199 {
            using a = t197;
            using b = t198;
            using c = t199;
            return (([&]() -> t199 {
                juniper::tuple2<t197, t198> guid0 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t198 b = (guid0).e2;
                t197 a = (guid0).e1;
                
                return f(a, b);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t211, typename t212, typename t213, typename t214, typename t215>
    t214 apply3(juniper::function<t215, t214(t211, t212, t213)> f, juniper::tuple3<t211, t212, t213> tup) {
        return (([&]() -> t214 {
            using a = t211;
            using b = t212;
            using c = t213;
            using d = t214;
            return (([&]() -> t214 {
                juniper::tuple3<t211, t212, t213> guid1 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t213 c = (guid1).e3;
                t212 b = (guid1).e2;
                t211 a = (guid1).e1;
                
                return f(a, b, c);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t229, typename t230, typename t231, typename t232, typename t233, typename t234>
    t233 apply4(juniper::function<t234, t233(t229, t230, t231, t232)> f, juniper::tuple4<t229, t230, t231, t232> tup) {
        return (([&]() -> t233 {
            using a = t229;
            using b = t230;
            using c = t231;
            using d = t232;
            using e = t233;
            return (([&]() -> t233 {
                juniper::tuple4<t229, t230, t231, t232> guid2 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t232 d = (guid2).e4;
                t231 c = (guid2).e3;
                t230 b = (guid2).e2;
                t229 a = (guid2).e1;
                
                return f(a, b, c, d);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t250, typename t251>
    t250 fst(juniper::tuple2<t250, t251> tup) {
        return (([&]() -> t250 {
            using a = t250;
            using b = t251;
            return (([&]() -> t250 {
                juniper::tuple2<t250, t251> guid3 = tup;
                return (true ? 
                    (([&]() -> t250 {
                        t250 x = (guid3).e1;
                        return x;
                    })())
                :
                    juniper::quit<t250>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t255, typename t256>
    t256 snd(juniper::tuple2<t255, t256> tup) {
        return (([&]() -> t256 {
            using a = t255;
            using b = t256;
            return (([&]() -> t256 {
                juniper::tuple2<t255, t256> guid4 = tup;
                return (true ? 
                    (([&]() -> t256 {
                        t256 x = (guid4).e2;
                        return x;
                    })())
                :
                    juniper::quit<t256>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t260>
    t260 add(t260 numA, t260 numB) {
        return (([&]() -> t260 {
            using a = t260;
            return ((t260) (numA + numB));
        })());
    }
}

namespace Prelude {
    template<typename t262>
    t262 sub(t262 numA, t262 numB) {
        return (([&]() -> t262 {
            using a = t262;
            return ((t262) (numA - numB));
        })());
    }
}

namespace Prelude {
    template<typename t264>
    t264 mul(t264 numA, t264 numB) {
        return (([&]() -> t264 {
            using a = t264;
            return ((t264) (numA * numB));
        })());
    }
}

namespace Prelude {
    template<typename t266>
    t266 div(t266 numA, t266 numB) {
        return (([&]() -> t266 {
            using a = t266;
            return ((t266) (numA / numB));
        })());
    }
}

namespace Prelude {
    template<typename t272, typename t273>
    juniper::tuple2<t273, t272> swap(juniper::tuple2<t272, t273> tup) {
        return (([&]() -> juniper::tuple2<t273, t272> {
            juniper::tuple2<t272, t273> guid5 = tup;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            t273 beta = (guid5).e2;
            t272 alpha = (guid5).e1;
            
            return (juniper::tuple2<t273,t272>{beta, alpha});
        })());
    }
}

namespace Prelude {
    template<typename t279, typename t280, typename t281>
    t279 until(juniper::function<t280, bool(t279)> p, juniper::function<t281, t279(t279)> f, t279 a0) {
        return (([&]() -> t279 {
            using a = t279;
            return (([&]() -> t279 {
                t279 guid6 = a0;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t279 a = guid6;
                
                (([&]() -> juniper::unit {
                    while (!(p(a))) {
                        (([&]() -> t279 {
                            return (a = f(a));
                        })());
                    }
                    return {};
                })());
                return a;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t290>
    juniper::unit ignore(t290 val) {
        return (([&]() -> juniper::unit {
            using a = t290;
            return juniper::unit();
        })());
    }
}

namespace Prelude {
    uint16_t u8ToU16(uint8_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t u8ToU32(uint8_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t u8ToI8(uint8_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t u8ToI16(uint8_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t u8ToI32(uint8_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float u8ToFloat(uint8_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double u8ToDouble(uint8_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t u16ToU8(uint16_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t u16ToU32(uint16_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t u16ToI8(uint16_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t u16ToI16(uint16_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t u16ToI32(uint16_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float u16ToFloat(uint16_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double u16ToDouble(uint16_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t u32ToU8(uint32_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t u32ToU16(uint32_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t u32ToI8(uint32_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t u32ToI16(uint32_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t u32ToI32(uint32_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float u32ToFloat(uint32_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double u32ToDouble(uint32_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t i8ToU8(int8_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t i8ToU16(int8_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t i8ToU32(int8_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t i8ToI16(int8_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t i8ToI32(int8_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float i8ToFloat(int8_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double i8ToDouble(int8_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t i16ToU8(int16_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t i16ToU16(int16_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t i16ToU32(int16_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t i16ToI8(int16_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t i16ToI32(int16_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float i16ToFloat(int16_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double i16ToDouble(int16_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t i32ToU8(int32_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t i32ToU16(int32_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t i32ToU32(int32_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t i32ToI8(int32_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t i32ToI16(int32_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float i32ToFloat(int32_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double i32ToDouble(int32_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t floatToU8(float n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t floatToU16(float n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t floatToU32(float n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t floatToI8(float n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t floatToI16(float n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t floatToI32(float n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double floatToDouble(float n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t doubleToU8(double n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t doubleToU16(double n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t doubleToU32(double n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t doubleToI8(double n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t doubleToI16(double n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t doubleToI32(double n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float doubleToFloat(double n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t349>
    uint8_t toUInt8(t349 n) {
        return (([&]() -> uint8_t {
            using t = t349;
            return (([&]() -> uint8_t {
                uint8_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (uint8_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t351>
    int8_t toInt8(t351 n) {
        return (([&]() -> int8_t {
            using t = t351;
            return (([&]() -> int8_t {
                int8_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (int8_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t353>
    uint16_t toUInt16(t353 n) {
        return (([&]() -> uint16_t {
            using t = t353;
            return (([&]() -> uint16_t {
                uint16_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (uint16_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t355>
    int16_t toInt16(t355 n) {
        return (([&]() -> int16_t {
            using t = t355;
            return (([&]() -> int16_t {
                int16_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (int16_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t357>
    uint32_t toUInt32(t357 n) {
        return (([&]() -> uint32_t {
            using t = t357;
            return (([&]() -> uint32_t {
                uint32_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (uint32_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t359>
    int32_t toInt32(t359 n) {
        return (([&]() -> int32_t {
            using t = t359;
            return (([&]() -> int32_t {
                int32_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (int32_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t361>
    float toFloat(t361 n) {
        return (([&]() -> float {
            using t = t361;
            return (([&]() -> float {
                float ret;
                
                (([&]() -> juniper::unit {
                    ret = (float) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t363>
    double toDouble(t363 n) {
        return (([&]() -> double {
            using t = t363;
            return (([&]() -> double {
                double ret;
                
                (([&]() -> juniper::unit {
                    ret = (double) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t365>
    t365 fromUInt8(uint8_t n) {
        return (([&]() -> t365 {
            using t = t365;
            return (([&]() -> t365 {
                t365 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t367>
    t367 fromInt8(int8_t n) {
        return (([&]() -> t367 {
            using t = t367;
            return (([&]() -> t367 {
                t367 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t369>
    t369 fromUInt16(uint16_t n) {
        return (([&]() -> t369 {
            using t = t369;
            return (([&]() -> t369 {
                t369 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t371>
    t371 fromInt16(int16_t n) {
        return (([&]() -> t371 {
            using t = t371;
            return (([&]() -> t371 {
                t371 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t373>
    t373 fromUInt32(uint32_t n) {
        return (([&]() -> t373 {
            using t = t373;
            return (([&]() -> t373 {
                t373 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t375>
    t375 fromInt32(int32_t n) {
        return (([&]() -> t375 {
            using t = t375;
            return (([&]() -> t375 {
                t375 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t377>
    t377 fromFloat(float n) {
        return (([&]() -> t377 {
            using t = t377;
            return (([&]() -> t377 {
                t377 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t379>
    t379 fromDouble(double n) {
        return (([&]() -> t379 {
            using t = t379;
            return (([&]() -> t379 {
                t379 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t381, typename t382>
    t382 cast(t381 x) {
        return (([&]() -> t382 {
            using a = t381;
            using b = t382;
            return (([&]() -> t382 {
                t382 ret;
                
                (([&]() -> juniper::unit {
                    ret = (b) x;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t387, typename t390, typename t394, int c4>
    juniper::records::recordt_0<juniper::array<t394, c4>, uint32_t> map(juniper::function<t387, t394(t390)> f, juniper::records::recordt_0<juniper::array<t390, c4>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t394, c4>, uint32_t> {
            using a = t390;
            using b = t394;
            constexpr int32_t n = c4;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t394, c4>, uint32_t> {
                juniper::array<t394, c4> guid7 = (juniper::array<t394, c4>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t394, c4> ret = guid7;
                
                (([&]() -> juniper::unit {
                    uint32_t guid8 = ((uint32_t) 0);
                    uint32_t guid9 = (lst).length;
                    for (uint32_t i = guid8; i < guid9; i++) {
                        (([&]() -> t394 {
                            return ((ret)[i] = f(((lst).data)[i]));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t394, c4>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t394, c4>, uint32_t> guid10;
                    guid10.data = ret;
                    guid10.length = (lst).length;
                    return guid10;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t398, typename t400, typename t403, int c7>
    t398 foldl(juniper::function<t400, t398(t403, t398)> f, t398 initState, juniper::records::recordt_0<juniper::array<t403, c7>, uint32_t> lst) {
        return (([&]() -> t398 {
            using state = t398;
            using t = t403;
            constexpr int32_t n = c7;
            return (([&]() -> t398 {
                t398 guid11 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t398 s = guid11;
                
                (([&]() -> juniper::unit {
                    uint32_t guid12 = ((uint32_t) 0);
                    uint32_t guid13 = (lst).length;
                    for (uint32_t i = guid12; i < guid13; i++) {
                        (([&]() -> t398 {
                            return (s = f(((lst).data)[i], s));
                        })());
                    }
                    return {};
                })());
                return s;
            })());
        })());
    }
}

namespace List {
    template<typename t410, typename t412, typename t418, int c9>
    t410 foldr(juniper::function<t412, t410(t418, t410)> f, t410 initState, juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t> lst) {
        return (([&]() -> t410 {
            using state = t410;
            using t = t418;
            constexpr int32_t n = c9;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> t410 {
                    return initState;
                })())
            :
                (([&]() -> t410 {
                    t410 guid14 = initState;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t410 s = guid14;
                    
                    uint32_t guid15 = (lst).length;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint32_t i = guid15;
                    
                    (([&]() -> juniper::unit {
                        do {
                            (([&]() -> t410 {
                                (i = ((uint32_t) (i - ((uint32_t) 1))));
                                return (s = f(((lst).data)[i], s));
                            })());
                        } while(((bool) (i > ((uint32_t) 0))));
                        return {};
                    })());
                    return s;
                })()));
        })());
    }
}

namespace List {
    template<typename t435, int c11, int c12, int c13>
    juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t> append(juniper::records::recordt_0<juniper::array<t435, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t435, c12>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t> {
            using t = t435;
            constexpr int32_t aCap = c11;
            constexpr int32_t bCap = c12;
            constexpr int32_t retCap = c13;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t> {
                uint32_t guid16 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid16;
                
                juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t> guid17 = (([&]() -> juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t> guid18;
                    guid18.data = (juniper::array<t435, c13>());
                    guid18.length = ((uint32_t) ((lstA).length + (lstB).length));
                    return guid18;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t435, c13>, uint32_t> out = guid17;
                
                (([&]() -> juniper::unit {
                    uint32_t guid19 = ((uint32_t) 0);
                    uint32_t guid20 = (lstA).length;
                    for (uint32_t i = guid19; i < guid20; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((lstA).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid21 = ((uint32_t) 0);
                    uint32_t guid22 = (lstB).length;
                    for (uint32_t i = guid21; i < guid22; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((lstB).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return out;
            })());
        })());
    }
}

namespace List {
    template<typename t448, int c18>
    Prelude::maybe<t448> nth(uint32_t i, juniper::records::recordt_0<juniper::array<t448, c18>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t448> {
            using t = t448;
            constexpr int32_t n = c18;
            return (((bool) (i < (lst).length)) ? 
                just<t448>(((lst).data)[i])
            :
                nothing<t448>());
        })());
    }
}

namespace List {
    template<typename t460, int c20, int c21>
    juniper::records::recordt_0<juniper::array<t460, (c21)*(c20)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t460, c20>, uint32_t>, c21>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t460, (c21)*(c20)>, uint32_t> {
            using t = t460;
            constexpr int32_t m = c20;
            constexpr int32_t n = c21;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t460, (c21)*(c20)>, uint32_t> {
                juniper::array<t460, (c21)*(c20)> guid23 = (juniper::array<t460, (c21)*(c20)>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t460, (c21)*(c20)> ret = guid23;
                
                uint32_t guid24 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid24;
                
                (([&]() -> juniper::unit {
                    uint32_t guid25 = ((uint32_t) 0);
                    uint32_t guid26 = (listOfLists).length;
                    for (uint32_t i = guid25; i < guid26; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                uint32_t guid27 = ((uint32_t) 0);
                                uint32_t guid28 = (((listOfLists).data)[i]).length;
                                for (uint32_t j = guid27; j < guid28; j++) {
                                    (([&]() -> uint32_t {
                                        ((ret)[index] = ((((listOfLists).data)[i]).data)[j]);
                                        return (index += ((uint32_t) 1));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t460, (c21)*(c20)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t460, (c21)*(c20)>, uint32_t> guid29;
                    guid29.data = ret;
                    guid29.length = index;
                    return guid29;
                })());
            })());
        })());
    }
}

namespace Math {
    template<typename t464>
    t464 min_(t464 x, t464 y) {
        return (([&]() -> t464 {
            using a = t464;
            return (((bool) (x > y)) ? 
                (([&]() -> t464 {
                    return y;
                })())
            :
                (([&]() -> t464 {
                    return x;
                })()));
        })());
    }
}

namespace List {
    template<typename t478, int c26, int c27>
    juniper::records::recordt_0<juniper::array<t478, c26>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t478, c27>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t478, c26>, uint32_t> {
            using t = t478;
            constexpr int32_t m = c26;
            constexpr int32_t n = c27;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t478, c26>, uint32_t> {
                juniper::array<t478, c26> guid30 = (juniper::array<t478, c26>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t478, c26> ret = guid30;
                
                (([&]() -> juniper::unit {
                    uint32_t guid31 = ((uint32_t) 0);
                    uint32_t guid32 = Math::min_<uint32_t>((lst).length, toUInt32<int32_t>(m));
                    for (uint32_t i = guid31; i < guid32; i++) {
                        (([&]() -> t478 {
                            return ((ret)[i] = ((lst).data)[i]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t478, c26>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t478, c26>, uint32_t> guid33;
                    guid33.data = ret;
                    guid33.length = (lst).length;
                    return guid33;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t484, typename t487, int c30>
    bool all(juniper::function<t484, bool(t487)> pred, juniper::records::recordt_0<juniper::array<t487, c30>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t487;
            constexpr int32_t n = c30;
            return (([&]() -> bool {
                bool guid34 = true;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid34;
                
                (([&]() -> juniper::unit {
                    uint32_t guid35 = ((uint32_t) 0);
                    uint32_t guid36 = (lst).length;
                    for (uint32_t i = guid35; i < guid36; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (satisfied) {
                                    (([&]() -> bool {
                                        return (satisfied = pred(((lst).data)[i]));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return satisfied;
            })());
        })());
    }
}

namespace List {
    template<typename t494, typename t497, int c32>
    bool any(juniper::function<t494, bool(t497)> pred, juniper::records::recordt_0<juniper::array<t497, c32>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t497;
            constexpr int32_t n = c32;
            return (([&]() -> bool {
                bool guid37 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid37;
                
                (([&]() -> juniper::unit {
                    uint32_t guid38 = ((uint32_t) 0);
                    uint32_t guid39 = (lst).length;
                    for (uint32_t i = guid38; i < guid39; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (!(satisfied)) {
                                    (([&]() -> bool {
                                        return (satisfied = pred(((lst).data)[i]));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return satisfied;
            })());
        })());
    }
}

namespace List {
    template<typename t502, int c34>
    juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> pushBack(t502 elem, juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> {
            using t = t502;
            constexpr int32_t n = c34;
            return (((bool) ((lst).length >= n)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> {
                    return lst;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> guid40 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t502, c34>, uint32_t> ret = guid40;
                    
                    (((ret).data)[(lst).length] = elem);
                    ((ret).length += ((uint32_t) 1));
                    return ret;
                })()));
        })());
    }
}

namespace List {
    template<typename t517, int c36>
    juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> pushOffFront(t517 elem, juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> {
            using t = t517;
            constexpr int32_t n = c36;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> {
                        return lst;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t517, c36>, uint32_t> ret;
                        
                        (((ret).data)[((uint32_t) 0)] = elem);
                        (([&]() -> juniper::unit {
                            uint32_t guid41 = ((uint32_t) 0);
                            uint32_t guid42 = (lst).length;
                            for (uint32_t i = guid41; i < guid42; i++) {
                                (([&]() -> juniper::unit {
                                    return (([&]() -> juniper::unit {
                                        if (((bool) (((uint32_t) (i + ((uint32_t) 1))) < n))) {
                                            (([&]() -> t517 {
                                                return (((ret).data)[((uint32_t) (i + ((uint32_t) 1)))] = ((lst).data)[i]);
                                            })());
                                        }
                                        return {};
                                    })());
                                })());
                            }
                            return {};
                        })());
                        (((bool) ((lst).length == cast<int32_t, uint32_t>(n))) ? 
                            (([&]() -> uint32_t {
                                return ((ret).length = (lst).length);
                            })())
                        :
                            (([&]() -> uint32_t {
                                return ((ret).length += ((uint32_t) 1));
                            })()));
                        return ret;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t532, int c40>
    juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> setNth(uint32_t index, t532 elem, juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> {
            using t = t532;
            constexpr int32_t n = c40;
            return (((bool) (index < (lst).length)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> guid43 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> ret = guid43;
                    
                    (((ret).data)[index] = elem);
                    return ret;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t532, c40>, uint32_t> {
                    return lst;
                })()));
        })());
    }
}

namespace List {
    template<typename t537, int c42>
    juniper::records::recordt_0<juniper::array<t537, c42>, uint32_t> replicate(uint32_t numOfElements, t537 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t537, c42>, uint32_t> {
            using t = t537;
            constexpr int32_t n = c42;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t537, c42>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t537, c42>, uint32_t> guid44;
                guid44.data = (juniper::array<t537, c42>().fill(elem));
                guid44.length = numOfElements;
                return guid44;
            })());
        })());
    }
}

namespace List {
    template<typename t548, int c43>
    juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> remove(t548 elem, juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> {
            using t = t548;
            constexpr int32_t n = c43;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> {
                uint32_t guid45 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid45;
                
                bool guid46 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid46;
                
                (([&]() -> juniper::unit {
                    uint32_t guid47 = ((uint32_t) 0);
                    uint32_t guid48 = (lst).length;
                    for (uint32_t i = guid47; i < guid48; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (((bool) (!(found) && ((bool) (((lst).data)[i] == elem))))) {
                                    (([&]() -> bool {
                                        (index = i);
                                        return (found = true);
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return (found ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> guid49 = lst;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> ret = guid49;
                        
                        ((ret).length -= ((uint32_t) 1));
                        (([&]() -> juniper::unit {
                            uint32_t guid50 = index;
                            uint32_t guid51 = (lst).length;
                            for (uint32_t i = guid50; i < guid51; i++) {
                                (([&]() -> t548 {
                                    return (((ret).data)[i] = ((lst).data)[((uint32_t) (i + ((uint32_t) 1)))]);
                                })());
                            }
                            return {};
                        })());
                        t548 dummy;
                        
                        (((ret).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))] = dummy);
                        return ret;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t548, c43>, uint32_t> {
                        return lst;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t556, int c48>
    juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> {
            using t = t556;
            constexpr int32_t n = c48;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> {
                    return lst;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> guid52 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t556, c48>, uint32_t> ret = guid52;
                    
                    t556 dummy;
                    
                    ((ret).length -= ((uint32_t) 1));
                    (((ret).data)[(ret).length] = dummy);
                    return ret;
                })()));
        })());
    }
}

namespace List {
    template<typename t566, typename t569, int c50>
    juniper::unit foreach(juniper::function<t566, juniper::unit(t569)> f, juniper::records::recordt_0<juniper::array<t569, c50>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            using t = t569;
            constexpr int32_t n = c50;
            return (([&]() -> juniper::unit {
                uint32_t guid53 = ((uint32_t) 0);
                uint32_t guid54 = (lst).length;
                for (uint32_t i = guid53; i < guid54; i++) {
                    (([&]() -> juniper::unit {
                        return f(((lst).data)[i]);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t584, int c52>
    Prelude::maybe<t584> last(juniper::records::recordt_0<juniper::array<t584, c52>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t584> {
            using t = t584;
            constexpr int32_t n = c52;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t584> {
                    return nothing<t584>();
                })())
            :
                (([&]() -> Prelude::maybe<t584> {
                    return just<t584>(((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))]);
                })()));
        })());
    }
}

namespace List {
    template<typename t598, int c54>
    Prelude::maybe<t598> max_(juniper::records::recordt_0<juniper::array<t598, c54>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t598> {
            using t = t598;
            constexpr int32_t n = c54;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t598> {
                    return nothing<t598>();
                })())
            :
                (([&]() -> Prelude::maybe<t598> {
                    t598 guid55 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t598 maxVal = guid55;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid56 = ((uint32_t) 1);
                        uint32_t guid57 = (lst).length;
                        for (uint32_t i = guid56; i < guid57; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] > maxVal))) {
                                        (([&]() -> t598 {
                                            return (maxVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t598>(maxVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t615, int c58>
    Prelude::maybe<t615> min_(juniper::records::recordt_0<juniper::array<t615, c58>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t615> {
            using t = t615;
            constexpr int32_t n = c58;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t615> {
                    return nothing<t615>();
                })())
            :
                (([&]() -> Prelude::maybe<t615> {
                    t615 guid58 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t615 minVal = guid58;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid59 = ((uint32_t) 1);
                        uint32_t guid60 = (lst).length;
                        for (uint32_t i = guid59; i < guid60; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] < minVal))) {
                                        (([&]() -> t615 {
                                            return (minVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t615>(minVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t623, int c62>
    bool member(t623 elem, juniper::records::recordt_0<juniper::array<t623, c62>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t623;
            constexpr int32_t n = c62;
            return (([&]() -> bool {
                bool guid61 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid61;
                
                (([&]() -> juniper::unit {
                    uint32_t guid62 = ((uint32_t) 0);
                    uint32_t guid63 = (lst).length;
                    for (uint32_t i = guid62; i < guid63; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (((bool) (!(found) && ((bool) (((lst).data)[i] == elem))))) {
                                    (([&]() -> bool {
                                        return (found = true);
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return found;
            })());
        })());
    }
}

namespace List {
    template<typename t638, typename t640, int c64>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t638, c64>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t640, c64>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t> {
            using a = t638;
            using b = t640;
            constexpr int32_t n = c64;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t> {
                uint32_t guid64 = Math::min_<uint32_t>((lstA).length, (lstB).length);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t outLen = guid64;
                
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t> guid65 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t> guid66;
                    guid66.data = (juniper::array<juniper::tuple2<t638, t640>, c64>());
                    guid66.length = outLen;
                    return guid66;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t638, t640>, c64>, uint32_t> ret = guid65;
                
                (([&]() -> juniper::unit {
                    uint32_t guid67 = ((uint32_t) 0);
                    uint32_t guid68 = outLen;
                    for (uint32_t i = guid67; i < guid68; i++) {
                        (([&]() -> juniper::tuple2<t638, t640> {
                            return (((ret).data)[i] = (juniper::tuple2<t638,t640>{((lstA).data)[i], ((lstB).data)[i]}));
                        })());
                    }
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t651, typename t652, int c68>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t651, t652>, c68>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t>> {
            using a = t651;
            using b = t652;
            constexpr int32_t n = c68;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t> guid69 = (([&]() -> juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t> guid70;
                    guid70.data = (juniper::array<t651, c68>());
                    guid70.length = (lst).length;
                    return guid70;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t> retA = guid69;
                
                juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t> guid71 = (([&]() -> juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t> guid72;
                    guid72.data = (juniper::array<t652, c68>());
                    guid72.length = (lst).length;
                    return guid72;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t> retB = guid71;
                
                (([&]() -> juniper::unit {
                    uint32_t guid73 = ((uint32_t) 0);
                    uint32_t guid74 = (lst).length;
                    for (uint32_t i = guid73; i < guid74; i++) {
                        (([&]() -> t652 {
                            juniper::tuple2<t651, t652> guid75 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t652 b = (guid75).e2;
                            t651 a = (guid75).e1;
                            
                            (((retA).data)[i] = a);
                            return (((retB).data)[i] = b);
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t651, c68>, uint32_t>,juniper::records::recordt_0<juniper::array<t652, c68>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t660, int c72>
    t660 sum(juniper::records::recordt_0<juniper::array<t660, c72>, uint32_t> lst) {
        return (([&]() -> t660 {
            using a = t660;
            constexpr int32_t n = c72;
            return List::foldl<t660, void, t660, c72>(Prelude::add<t660>, ((t660) 0), lst);
        })());
    }
}

namespace List {
    template<typename t678, int c74>
    t678 average(juniper::records::recordt_0<juniper::array<t678, c74>, uint32_t> lst) {
        return (([&]() -> t678 {
            using a = t678;
            constexpr int32_t n = c74;
            return ((t678) (sum<t678, c74>(lst) / cast<uint32_t, t678>((lst).length)));
        })());
    }
}

namespace List {
    template<typename t684, typename t685, typename t686, int c76>
    juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> sort(juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> lst, juniper::function<t684, t685(t686)> key) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> {
            using clos = t684;
            using m = t685;
            using t = t686;
            constexpr int32_t n = c76;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> guid76 = lst;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t686, c76>, uint32_t> lstCopy = guid76;
                
                uint32_t guid77 = (lst).length;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t len = guid77;
                
                (([&]() -> juniper::unit {
                    
    quicksort<t, clos, m, n>(lstCopy, key, 0, len - 1);
    
                    return {};
                })());
                return lstCopy;
            })());
        })());
    }
}

namespace Signal {
    template<typename t690, typename t692, typename t707>
    Prelude::sig<t707> map(juniper::function<t692, t707(t690)> f, Prelude::sig<t690> s) {
        return (([&]() -> Prelude::sig<t707> {
            using a = t690;
            using b = t707;
            return (([&]() -> Prelude::sig<t707> {
                Prelude::sig<t690> guid78 = s;
                return (((bool) (((bool) ((guid78).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid78).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t707> {
                        t690 val = ((guid78).signal()).just();
                        return signal<t707>(just<t707>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t707> {
                            return signal<t707>(nothing<t707>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t707>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t715, typename t716>
    juniper::unit sink(juniper::function<t716, juniper::unit(t715)> f, Prelude::sig<t715> s) {
        return (([&]() -> juniper::unit {
            using a = t715;
            return (([&]() -> juniper::unit {
                Prelude::sig<t715> guid79 = s;
                return (((bool) (((bool) ((guid79).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid79).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::unit {
                        t715 val = ((guid79).signal()).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            return juniper::unit();
                        })())
                    :
                        juniper::quit<juniper::unit>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t725, typename t739>
    Prelude::sig<t739> filter(juniper::function<t725, bool(t739)> f, Prelude::sig<t739> s) {
        return (([&]() -> Prelude::sig<t739> {
            using a = t739;
            return (([&]() -> Prelude::sig<t739> {
                Prelude::sig<t739> guid80 = s;
                return (((bool) (((bool) ((guid80).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid80).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t739> {
                        t739 val = ((guid80).signal()).just();
                        return (f(val) ? 
                            (([&]() -> Prelude::sig<t739> {
                                return signal<t739>(nothing<t739>());
                            })())
                        :
                            (([&]() -> Prelude::sig<t739> {
                                return s;
                            })()));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t739> {
                            return signal<t739>(nothing<t739>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t739>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t746>
    Prelude::sig<t746> merge(Prelude::sig<t746> sigA, Prelude::sig<t746> sigB) {
        return (([&]() -> Prelude::sig<t746> {
            using a = t746;
            return (([&]() -> Prelude::sig<t746> {
                Prelude::sig<t746> guid81 = sigA;
                return (((bool) (((bool) ((guid81).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid81).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t746> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t746> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t746>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t750, int c77>
    Prelude::sig<t750> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t750>, c77>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t750> {
            using a = t750;
            constexpr int32_t n = c77;
            return (([&]() -> Prelude::sig<t750> {
                Prelude::maybe<t750> guid82 = List::foldl<Prelude::maybe<t750>, void, Prelude::sig<t750>, c77>(juniper::function<void, Prelude::maybe<t750>(Prelude::sig<t750>,Prelude::maybe<t750>)>([](Prelude::sig<t750> sig, Prelude::maybe<t750> accum) -> Prelude::maybe<t750> { 
                    return (([&]() -> Prelude::maybe<t750> {
                        Prelude::maybe<t750> guid83 = accum;
                        return (((bool) (((bool) ((guid83).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::maybe<t750> {
                                return (([&]() -> Prelude::maybe<t750> {
                                    Prelude::sig<t750> guid84 = sig;
                                    if (!(((bool) (((bool) ((guid84).id() == ((uint8_t) 0))) && true)))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t750> heldValue = (guid84).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t750> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t750>>()));
                    })());
                 }), nothing<t750>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t750> ret = guid82;
                
                return signal<t750>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t773, typename t797>
    Prelude::sig<Prelude::either<t797, t773>> join(Prelude::sig<t797> sigA, Prelude::sig<t773> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t797, t773>> {
            using a = t797;
            using b = t773;
            return (([&]() -> Prelude::sig<Prelude::either<t797, t773>> {
                juniper::tuple2<Prelude::sig<t797>, Prelude::sig<t773>> guid85 = (juniper::tuple2<Prelude::sig<t797>,Prelude::sig<t773>>{sigA, sigB});
                return (((bool) (((bool) (((guid85).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid85).e1).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t797, t773>> {
                        t797 value = (((guid85).e1).signal()).just();
                        return signal<Prelude::either<t797, t773>>(just<Prelude::either<t797, t773>>(left<t797, t773>(value)));
                    })())
                :
                    (((bool) (((bool) (((guid85).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid85).e2).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t797, t773>> {
                            t773 value = (((guid85).e2).signal()).just();
                            return signal<Prelude::either<t797, t773>>(just<Prelude::either<t797, t773>>(right<t797, t773>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t797, t773>> {
                                return signal<Prelude::either<t797, t773>>(nothing<Prelude::either<t797, t773>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t797, t773>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t820>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t820> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t820;
            return map<t820, void, juniper::unit>(juniper::function<void, juniper::unit(t820)>([](t820 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t827, typename t829, typename t847>
    Prelude::sig<t847> foldP(juniper::function<t829, t847(t827, t847)> f, juniper::shared_ptr<t847> state0, Prelude::sig<t827> incoming) {
        return (([&]() -> Prelude::sig<t847> {
            using a = t827;
            using state = t847;
            return (([&]() -> Prelude::sig<t847> {
                Prelude::sig<t827> guid86 = incoming;
                return (((bool) (((bool) ((guid86).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid86).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t847> {
                        t827 val = ((guid86).signal()).just();
                        return (([&]() -> Prelude::sig<t847> {
                            t847 guid87 = f(val, (*((state0).get())));
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t847 state1 = guid87;
                            
                            (*((state0).get()) = state1);
                            return signal<t847>(just<t847>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t847> {
                            return signal<t847>(nothing<t847>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t847>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t857>
    Prelude::sig<t857> dropRepeats(juniper::shared_ptr<Prelude::maybe<t857>> maybePrevValue, Prelude::sig<t857> incoming) {
        return (([&]() -> Prelude::sig<t857> {
            using a = t857;
            return filter<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t857>>>, t857>(juniper::function<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t857>>>, bool(t857)>(juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t857>>>(maybePrevValue), [](juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t857>>>& junclosure, t857 value) -> bool { 
                juniper::shared_ptr<Prelude::maybe<t857>>& maybePrevValue = junclosure.maybePrevValue;
                return (([&]() -> bool {
                    bool guid88 = (([&]() -> bool {
                        Prelude::maybe<t857> guid89 = (*((maybePrevValue).get()));
                        return (((bool) (((bool) ((guid89).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> bool {
                                return false;
                            })())
                        :
                            (((bool) (((bool) ((guid89).id() == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> bool {
                                    t857 prevValue = (guid89).just();
                                    return ((bool) (value == prevValue));
                                })())
                            :
                                juniper::quit<bool>()));
                    })());
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    bool filtered = guid88;
                    
                    (([&]() -> juniper::unit {
                        if (!(filtered)) {
                            (([&]() -> Prelude::maybe<t857> {
                                return (*((maybePrevValue).get()) = just<t857>(value));
                            })());
                        }
                        return {};
                    })());
                    return filtered;
                })());
             }), incoming);
        })());
    }
}

namespace Signal {
    template<typename t875>
    Prelude::sig<t875> latch(juniper::shared_ptr<t875> prevValue, Prelude::sig<t875> incoming) {
        return (([&]() -> Prelude::sig<t875> {
            using a = t875;
            return (([&]() -> Prelude::sig<t875> {
                Prelude::sig<t875> guid90 = incoming;
                return (((bool) (((bool) ((guid90).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid90).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t875> {
                        t875 val = ((guid90).signal()).just();
                        return (([&]() -> Prelude::sig<t875> {
                            (*((prevValue).get()) = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t875> {
                            return signal<t875>(just<t875>((*((prevValue).get()))));
                        })())
                    :
                        juniper::quit<Prelude::sig<t875>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t888, typename t889, typename t892, typename t901>
    Prelude::sig<t888> map2(juniper::function<t889, t888(t892, t901)> f, juniper::shared_ptr<juniper::tuple2<t892, t901>> state, Prelude::sig<t892> incomingA, Prelude::sig<t901> incomingB) {
        return (([&]() -> Prelude::sig<t888> {
            using a = t892;
            using b = t901;
            using c = t888;
            return (([&]() -> Prelude::sig<t888> {
                t892 guid91 = (([&]() -> t892 {
                    Prelude::sig<t892> guid92 = incomingA;
                    return (((bool) (((bool) ((guid92).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid92).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t892 {
                            t892 val1 = ((guid92).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t892 {
                                return fst<t892, t901>((*((state).get())));
                            })())
                        :
                            juniper::quit<t892>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t892 valA = guid91;
                
                t901 guid93 = (([&]() -> t901 {
                    Prelude::sig<t901> guid94 = incomingB;
                    return (((bool) (((bool) ((guid94).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid94).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t901 {
                            t901 val2 = ((guid94).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t901 {
                                return snd<t892, t901>((*((state).get())));
                            })())
                        :
                            juniper::quit<t901>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t901 valB = guid93;
                
                (*((state).get()) = (juniper::tuple2<t892,t901>{valA, valB}));
                return (([&]() -> Prelude::sig<t888> {
                    juniper::tuple2<Prelude::sig<t892>, Prelude::sig<t901>> guid95 = (juniper::tuple2<Prelude::sig<t892>,Prelude::sig<t901>>{incomingA, incomingB});
                    return (((bool) (((bool) (((guid95).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid95).e2).signal()).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid95).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid95).e1).signal()).id() == ((uint8_t) 1))) && true)))))))) ? 
                        (([&]() -> Prelude::sig<t888> {
                            return signal<t888>(nothing<t888>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t888> {
                                return signal<t888>(just<t888>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t888>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t933, int c79>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t933, c79>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t933, c79>, uint32_t>> pastValues, Prelude::sig<t933> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t933, c79>, uint32_t>> {
            using a = t933;
            constexpr int32_t n = c79;
            return foldP<t933, void, juniper::records::recordt_0<juniper::array<t933, c79>, uint32_t>>(List::pushOffFront<t933, c79>, pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t944>
    Prelude::sig<t944> constant(t944 val) {
        return (([&]() -> Prelude::sig<t944> {
            using a = t944;
            return signal<t944>(just<t944>(val));
        })());
    }
}

namespace Signal {
    template<typename t954>
    Prelude::sig<Prelude::maybe<t954>> meta(Prelude::sig<t954> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t954>> {
            using a = t954;
            return (([&]() -> Prelude::sig<Prelude::maybe<t954>> {
                Prelude::sig<t954> guid96 = sigA;
                if (!(((bool) (((bool) ((guid96).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t954> val = (guid96).signal();
                
                return constant<Prelude::maybe<t954>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t971>
    Prelude::sig<t971> unmeta(Prelude::sig<Prelude::maybe<t971>> sigA) {
        return (([&]() -> Prelude::sig<t971> {
            using a = t971;
            return (([&]() -> Prelude::sig<t971> {
                Prelude::sig<Prelude::maybe<t971>> guid97 = sigA;
                return (((bool) (((bool) ((guid97).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid97).signal()).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid97).signal()).just()).id() == ((uint8_t) 0))) && true)))))) ? 
                    (([&]() -> Prelude::sig<t971> {
                        t971 val = (((guid97).signal()).just()).just();
                        return constant<t971>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t971> {
                            return signal<t971>(nothing<t971>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t971>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t984, typename t985>
    Prelude::sig<juniper::tuple2<t984, t985>> zip(juniper::shared_ptr<juniper::tuple2<t984, t985>> state, Prelude::sig<t984> sigA, Prelude::sig<t985> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t984, t985>> {
            using a = t984;
            using b = t985;
            return map2<juniper::tuple2<t984, t985>, void, t984, t985>(juniper::function<void, juniper::tuple2<t984, t985>(t984,t985)>([](t984 valA, t985 valB) -> juniper::tuple2<t984, t985> { 
                return (juniper::tuple2<t984,t985>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t1016, typename t1023>
    juniper::tuple2<Prelude::sig<t1016>, Prelude::sig<t1023>> unzip(Prelude::sig<juniper::tuple2<t1016, t1023>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t1016>, Prelude::sig<t1023>> {
            using a = t1016;
            using b = t1023;
            return (([&]() -> juniper::tuple2<Prelude::sig<t1016>, Prelude::sig<t1023>> {
                Prelude::sig<juniper::tuple2<t1016, t1023>> guid98 = incoming;
                return (((bool) (((bool) ((guid98).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid98).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t1016>, Prelude::sig<t1023>> {
                        t1023 y = (((guid98).signal()).just()).e2;
                        t1016 x = (((guid98).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t1016>,Prelude::sig<t1023>>{signal<t1016>(just<t1016>(x)), signal<t1023>(just<t1023>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t1016>, Prelude::sig<t1023>> {
                            return (juniper::tuple2<Prelude::sig<t1016>,Prelude::sig<t1023>>{signal<t1016>(nothing<t1016>()), signal<t1023>(nothing<t1023>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t1016>, Prelude::sig<t1023>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1030, typename t1031>
    Prelude::sig<t1030> toggle(t1030 val1, t1030 val2, juniper::shared_ptr<t1030> state, Prelude::sig<t1031> incoming) {
        return (([&]() -> Prelude::sig<t1030> {
            using a = t1030;
            using b = t1031;
            return foldP<t1031, juniper::closures::closuret_5<t1030, t1030>, t1030>(juniper::function<juniper::closures::closuret_5<t1030, t1030>, t1030(t1031,t1030)>(juniper::closures::closuret_5<t1030, t1030>(val1, val2), [](juniper::closures::closuret_5<t1030, t1030>& junclosure, t1031 event, t1030 prevVal) -> t1030 { 
                t1030& val1 = junclosure.val1;
                t1030& val2 = junclosure.val2;
                return (((bool) (prevVal == val1)) ? 
                    (([&]() -> t1030 {
                        return val2;
                    })())
                :
                    (([&]() -> t1030 {
                        return val1;
                    })()));
             }), state, incoming);
        })());
    }
}

namespace Io {
    Io::pinState toggle(Io::pinState p) {
        return (([&]() -> Io::pinState {
            Io::pinState guid99 = p;
            return (((bool) (((bool) ((guid99).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> Io::pinState {
                    return low();
                })())
            :
                (((bool) (((bool) ((guid99).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> Io::pinState {
                        return high();
                    })())
                :
                    juniper::quit<Io::pinState>()));
        })());
    }
}

namespace Io {
    juniper::unit printStr(const char * str) {
        return (([&]() -> juniper::unit {
            Serial.print(str);
            return {};
        })());
    }
}

namespace Io {
    template<int c81>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c81>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c81;
            return (([&]() -> juniper::unit {
                Serial.print((char *) &cl.data[0]);
                return {};
            })());
        })());
    }
}

namespace Io {
    juniper::unit printFloat(float f) {
        return (([&]() -> juniper::unit {
            Serial.print(f);
            return {};
        })());
    }
}

namespace Io {
    juniper::unit printInt(int32_t n) {
        return (([&]() -> juniper::unit {
            Serial.print(n);
            return {};
        })());
    }
}

namespace Io {
    template<typename t1059>
    t1059 baseToInt(Io::base b) {
        return (([&]() -> t1059 {
            Io::base guid100 = b;
            return (((bool) (((bool) ((guid100).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> t1059 {
                    return ((t1059) 2);
                })())
            :
                (((bool) (((bool) ((guid100).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> t1059 {
                        return ((t1059) 8);
                    })())
                :
                    (((bool) (((bool) ((guid100).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> t1059 {
                            return ((t1059) 10);
                        })())
                    :
                        (((bool) (((bool) ((guid100).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> t1059 {
                                return ((t1059) 16);
                            })())
                        :
                            juniper::quit<t1059>()))));
        })());
    }
}

namespace Io {
    juniper::unit printIntBase(int32_t n, Io::base b) {
        return (([&]() -> juniper::unit {
            int32_t guid101 = baseToInt<int32_t>(b);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t bint = guid101;
            
            return (([&]() -> juniper::unit {
                Serial.print(n, bint);
                return {};
            })());
        })());
    }
}

namespace Io {
    juniper::unit printFloatPlaces(float f, int32_t numPlaces) {
        return (([&]() -> juniper::unit {
            Serial.print(f, numPlaces);
            return {};
        })());
    }
}

namespace Io {
    juniper::unit beginSerial(uint32_t speed) {
        return (([&]() -> juniper::unit {
            Serial.begin(speed);
            return {};
        })());
    }
}

namespace Io {
    uint8_t pinStateToInt(Io::pinState value) {
        return (([&]() -> uint8_t {
            Io::pinState guid102 = value;
            return (((bool) (((bool) ((guid102).id() == ((uint8_t) 1))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid102).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    juniper::quit<uint8_t>()));
        })());
    }
}

namespace Io {
    Io::pinState intToPinState(uint8_t value) {
        return (((bool) (value == ((uint8_t) 0))) ? 
            low()
        :
            high());
    }
}

namespace Io {
    juniper::unit digWrite(uint16_t pin, Io::pinState value) {
        return (([&]() -> juniper::unit {
            uint8_t guid103 = pinStateToInt(value);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t intVal = guid103;
            
            return (([&]() -> juniper::unit {
                digitalWrite(pin, intVal);
                return {};
            })());
        })());
    }
}

namespace Io {
    Io::pinState digRead(uint16_t pin) {
        return (([&]() -> Io::pinState {
            uint8_t intVal;
            
            (([&]() -> juniper::unit {
                intVal = digitalRead(pin);
                return {};
            })());
            return intToPinState(intVal);
        })());
    }
}

namespace Io {
    Prelude::sig<Io::pinState> digIn(uint16_t pin) {
        return signal<Io::pinState>(just<Io::pinState>(digRead(pin)));
    }
}

namespace Io {
    juniper::unit digOut(uint16_t pin, Prelude::sig<Io::pinState> sig) {
        return Signal::sink<Io::pinState, juniper::closures::closuret_6<uint16_t>>(juniper::function<juniper::closures::closuret_6<uint16_t>, juniper::unit(Io::pinState)>(juniper::closures::closuret_6<uint16_t>(pin), [](juniper::closures::closuret_6<uint16_t>& junclosure, Io::pinState value) -> juniper::unit { 
            uint16_t& pin = junclosure.pin;
            return digWrite(pin, value);
         }), sig);
    }
}

namespace Io {
    uint16_t anaRead(uint16_t pin) {
        return (([&]() -> uint16_t {
            uint16_t value;
            
            (([&]() -> juniper::unit {
                value = analogRead(pin);
                return {};
            })());
            return value;
        })());
    }
}

namespace Io {
    juniper::unit anaWrite(uint16_t pin, uint8_t value) {
        return (([&]() -> juniper::unit {
            analogWrite(pin, value);
            return {};
        })());
    }
}

namespace Io {
    Prelude::sig<uint16_t> anaIn(uint16_t pin) {
        return signal<uint16_t>(just<uint16_t>(anaRead(pin)));
    }
}

namespace Io {
    juniper::unit anaOut(uint16_t pin, Prelude::sig<uint8_t> sig) {
        return Signal::sink<uint8_t, juniper::closures::closuret_6<uint16_t>>(juniper::function<juniper::closures::closuret_6<uint16_t>, juniper::unit(uint8_t)>(juniper::closures::closuret_6<uint16_t>(pin), [](juniper::closures::closuret_6<uint16_t>& junclosure, uint8_t value) -> juniper::unit { 
            uint16_t& pin = junclosure.pin;
            return anaWrite(pin, value);
         }), sig);
    }
}

namespace Io {
    uint8_t pinModeToInt(Io::mode m) {
        return (([&]() -> uint8_t {
            Io::mode guid104 = m;
            return (((bool) (((bool) ((guid104).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid104).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    (((bool) (((bool) ((guid104).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 2);
                        })())
                    :
                        juniper::quit<uint8_t>())));
        })());
    }
}

namespace Io {
    Io::mode intToPinMode(uint8_t m) {
        return (([&]() -> Io::mode {
            uint8_t guid105 = m;
            return (((bool) (((bool) (guid105 == ((int32_t) 0))) && true)) ? 
                (([&]() -> Io::mode {
                    return input();
                })())
            :
                (((bool) (((bool) (guid105 == ((int32_t) 1))) && true)) ? 
                    (([&]() -> Io::mode {
                        return output();
                    })())
                :
                    (((bool) (((bool) (guid105 == ((int32_t) 2))) && true)) ? 
                        (([&]() -> Io::mode {
                            return inputPullup();
                        })())
                    :
                        juniper::quit<Io::mode>())));
        })());
    }
}

namespace Io {
    juniper::unit setPinMode(uint16_t pin, Io::mode m) {
        return (([&]() -> juniper::unit {
            uint8_t guid106 = pinModeToInt(m);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t m2 = guid106;
            
            return (([&]() -> juniper::unit {
                pinMode(pin, m2);
                return {};
            })());
        })());
    }
}

namespace Io {
    Prelude::sig<juniper::unit> risingEdge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState) {
        return Signal::toUnit<Io::pinState>(Signal::filter<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, Io::pinState>(juniper::function<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, bool(Io::pinState)>(juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>(prevState), [](juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>& junclosure, Io::pinState currState) -> bool { 
            juniper::shared_ptr<Io::pinState>& prevState = junclosure.prevState;
            return (([&]() -> bool {
                bool guid107 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState, Io::pinState> guid108 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid108).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid108).e1).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        (true ? 
                            (([&]() -> bool {
                                return true;
                            })())
                        :
                            juniper::quit<bool>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool ret = guid107;
                
                (*((prevState).get()) = currState);
                return ret;
            })());
         }), sig));
    }
}

namespace Io {
    Prelude::sig<juniper::unit> fallingEdge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState) {
        return Signal::toUnit<Io::pinState>(Signal::filter<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, Io::pinState>(juniper::function<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, bool(Io::pinState)>(juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>(prevState), [](juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>& junclosure, Io::pinState currState) -> bool { 
            juniper::shared_ptr<Io::pinState>& prevState = junclosure.prevState;
            return (([&]() -> bool {
                bool guid109 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState, Io::pinState> guid110 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid110).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid110).e1).id() == ((uint8_t) 1))) && true)))) ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        (true ? 
                            (([&]() -> bool {
                                return true;
                            })())
                        :
                            juniper::quit<bool>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool ret = guid109;
                
                (*((prevState).get()) = currState);
                return ret;
            })());
         }), sig));
    }
}

namespace Io {
    Prelude::sig<Io::pinState> edge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState) {
        return Signal::filter<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, Io::pinState>(juniper::function<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, bool(Io::pinState)>(juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>(prevState), [](juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>& junclosure, Io::pinState currState) -> bool { 
            juniper::shared_ptr<Io::pinState>& prevState = junclosure.prevState;
            return (([&]() -> bool {
                bool guid111 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState, Io::pinState> guid112 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid112).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid112).e1).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        (((bool) (((bool) (((guid112).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid112).e1).id() == ((uint8_t) 1))) && true)))) ? 
                            (([&]() -> bool {
                                return false;
                            })())
                        :
                            (true ? 
                                (([&]() -> bool {
                                    return true;
                                })())
                            :
                                juniper::quit<bool>())));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool ret = guid111;
                
                (*((prevState).get()) = currState);
                return ret;
            })());
         }), sig);
    }
}

namespace Maybe {
    template<typename t1191, typename t1193, typename t1202>
    Prelude::maybe<t1202> map(juniper::function<t1193, t1202(t1191)> f, Prelude::maybe<t1191> maybeVal) {
        return (([&]() -> Prelude::maybe<t1202> {
            using a = t1191;
            using b = t1202;
            return (([&]() -> Prelude::maybe<t1202> {
                Prelude::maybe<t1191> guid113 = maybeVal;
                return (((bool) (((bool) ((guid113).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t1202> {
                        t1191 val = (guid113).just();
                        return just<t1202>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t1202> {
                            return nothing<t1202>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t1202>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1206>
    t1206 get(Prelude::maybe<t1206> maybeVal) {
        return (([&]() -> t1206 {
            using a = t1206;
            return (([&]() -> t1206 {
                Prelude::maybe<t1206> guid114 = maybeVal;
                return (((bool) (((bool) ((guid114).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t1206 {
                        t1206 val = (guid114).just();
                        return val;
                    })())
                :
                    juniper::quit<t1206>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1209>
    bool isJust(Prelude::maybe<t1209> maybeVal) {
        return (([&]() -> bool {
            using a = t1209;
            return (([&]() -> bool {
                Prelude::maybe<t1209> guid115 = maybeVal;
                return (((bool) (((bool) ((guid115).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> bool {
                        return true;
                    })())
                :
                    (true ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        juniper::quit<bool>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1212>
    bool isNothing(Prelude::maybe<t1212> maybeVal) {
        return (([&]() -> bool {
            using a = t1212;
            return !(isJust<t1212>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t1218>
    uint8_t count(Prelude::maybe<t1218> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t1218;
            return (([&]() -> uint8_t {
                Prelude::maybe<t1218> guid116 = maybeVal;
                return (((bool) (((bool) ((guid116).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    (true ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 0);
                        })())
                    :
                        juniper::quit<uint8_t>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1224, typename t1225, typename t1226>
    t1224 foldl(juniper::function<t1226, t1224(t1225, t1224)> f, t1224 initState, Prelude::maybe<t1225> maybeVal) {
        return (([&]() -> t1224 {
            using state = t1224;
            using t = t1225;
            return (([&]() -> t1224 {
                Prelude::maybe<t1225> guid117 = maybeVal;
                return (((bool) (((bool) ((guid117).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t1224 {
                        t1225 val = (guid117).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t1224 {
                            return initState;
                        })())
                    :
                        juniper::quit<t1224>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1234, typename t1235, typename t1236>
    t1234 fodlr(juniper::function<t1236, t1234(t1235, t1234)> f, t1234 initState, Prelude::maybe<t1235> maybeVal) {
        return (([&]() -> t1234 {
            using state = t1234;
            using t = t1235;
            return foldl<t1234, t1235, t1236>(f, initState, maybeVal);
        })());
    }
}

namespace Maybe {
    template<typename t1247, typename t1248>
    juniper::unit iter(juniper::function<t1248, juniper::unit(t1247)> f, Prelude::maybe<t1247> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t1247;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t1247> guid118 = maybeVal;
                return (((bool) (((bool) ((guid118).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> juniper::unit {
                        t1247 val = (guid118).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t1247> nothing = guid118;
                            return juniper::unit();
                        })())
                    :
                        juniper::quit<juniper::unit>()));
            })());
        })());
    }
}

namespace Time {
    juniper::unit wait(uint32_t time) {
        return (([&]() -> juniper::unit {
            delay(time);
            return {};
        })());
    }
}

namespace Time {
    uint32_t now() {
        return (([&]() -> uint32_t {
            uint32_t guid119 = ((uint32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t ret = guid119;
            
            (([&]() -> juniper::unit {
                ret = millis();
                return {};
            })());
            return ret;
        })());
    }
}

namespace Time {
    juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> state() {
        return (juniper::shared_ptr<juniper::records::recordt_1<uint32_t>>((([&]() -> juniper::records::recordt_1<uint32_t>{
            juniper::records::recordt_1<uint32_t> guid120;
            guid120.lastPulse = ((uint32_t) 0);
            return guid120;
        })())));
    }
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> state) {
        return (([&]() -> Prelude::sig<uint32_t> {
            uint32_t guid121 = now();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t t = guid121;
            
            uint32_t guid122 = (((bool) (interval == ((uint32_t) 0))) ? 
                (([&]() -> uint32_t {
                    return t;
                })())
            :
                (([&]() -> uint32_t {
                    return ((uint32_t) (((uint32_t) (t / interval)) * interval));
                })()));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t lastWindow = guid122;
            
            return (((bool) (((state).get())->lastPulse >= lastWindow)) ? 
                (([&]() -> Prelude::sig<uint32_t> {
                    return signal<uint32_t>(nothing<uint32_t>());
                })())
            :
                (([&]() -> Prelude::sig<uint32_t> {
                    (((state).get())->lastPulse = t);
                    return signal<uint32_t>(just<uint32_t>(t));
                })()));
        })());
    }
}

namespace Math {
    double pi = 3.141592653589793238462643383279502884197169399375105820974;
}

namespace Math {
    double e = 2.718281828459045235360287471352662497757247093699959574966;
}

namespace Math {
    double degToRad(double degrees) {
        return ((double) (degrees * 0.017453292519943295769236907684886));
    }
}

namespace Math {
    double radToDeg(double radians) {
        return ((double) (radians * 57.295779513082320876798154814105));
    }
}

namespace Math {
    double acos_(double x) {
        return (([&]() -> double {
            double guid123 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid123;
            
            (([&]() -> juniper::unit {
                ret = acos(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double asin_(double x) {
        return (([&]() -> double {
            double guid124 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid124;
            
            (([&]() -> juniper::unit {
                ret = asin(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double atan_(double x) {
        return (([&]() -> double {
            double guid125 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid125;
            
            (([&]() -> juniper::unit {
                ret = atan(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double atan2_(double y, double x) {
        return (([&]() -> double {
            double guid126 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid126;
            
            (([&]() -> juniper::unit {
                ret = atan2(y, x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double cos_(double x) {
        return (([&]() -> double {
            double guid127 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid127;
            
            (([&]() -> juniper::unit {
                ret = cos(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double cosh_(double x) {
        return (([&]() -> double {
            double guid128 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid128;
            
            (([&]() -> juniper::unit {
                ret = cosh(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double sin_(double x) {
        return (([&]() -> double {
            double guid129 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid129;
            
            (([&]() -> juniper::unit {
                ret = sin(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double sinh_(double x) {
        return (([&]() -> double {
            double guid130 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid130;
            
            (([&]() -> juniper::unit {
                ret = sinh(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double tan_(double x) {
        return ((double) (sin_(x) / cos_(x)));
    }
}

namespace Math {
    double tanh_(double x) {
        return (([&]() -> double {
            double guid131 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid131;
            
            (([&]() -> juniper::unit {
                ret = tanh(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double exp_(double x) {
        return (([&]() -> double {
            double guid132 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid132;
            
            (([&]() -> juniper::unit {
                ret = exp(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    juniper::tuple2<double, int32_t> frexp_(double x) {
        return (([&]() -> juniper::tuple2<double, int32_t> {
            double guid133 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid133;
            
            int32_t guid134 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t exponent = guid134;
            
            (([&]() -> juniper::unit {
                int exponent2;
    ret = frexp(x, &exponent2);
    exponent = (int32_t) exponent2;
                return {};
            })());
            return (juniper::tuple2<double,int32_t>{ret, exponent});
        })());
    }
}

namespace Math {
    double ldexp_(double x, int16_t exponent) {
        return (([&]() -> double {
            double guid135 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid135;
            
            (([&]() -> juniper::unit {
                ret = ldexp(x, exponent);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double log_(double x) {
        return (([&]() -> double {
            double guid136 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid136;
            
            (([&]() -> juniper::unit {
                ret = log(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double log10_(double x) {
        return (([&]() -> double {
            double guid137 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid137;
            
            (([&]() -> juniper::unit {
                ret = log10(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    juniper::tuple2<double, double> modf_(double x) {
        return (([&]() -> juniper::tuple2<double, double> {
            double guid138 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid138;
            
            double guid139 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double integer = guid139;
            
            (([&]() -> juniper::unit {
                ret = modf(x, &integer);
                return {};
            })());
            return (juniper::tuple2<double,double>{ret, integer});
        })());
    }
}

namespace Math {
    double pow_(double x, double y) {
        return (([&]() -> double {
            double guid140 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid140;
            
            (([&]() -> juniper::unit {
                ret = pow(x, y);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double sqrt_(double x) {
        return (([&]() -> double {
            double guid141 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid141;
            
            (([&]() -> juniper::unit {
                ret = sqrt(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double ceil_(double x) {
        return (([&]() -> double {
            double guid142 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid142;
            
            (([&]() -> juniper::unit {
                ret = ceil(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double fabs_(double x) {
        return (([&]() -> double {
            double guid143 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid143;
            
            (([&]() -> juniper::unit {
                ret = fabs(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double floor_(double x) {
        return (([&]() -> double {
            double guid144 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid144;
            
            (([&]() -> juniper::unit {
                ret = floor(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double fmod_(double x, double y) {
        return (([&]() -> double {
            double guid145 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid145;
            
            (([&]() -> juniper::unit {
                ret = fmod(x, y);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double round_(double x) {
        return floor_(((double) (x + 0.5)));
    }
}

namespace Math {
    template<typename t1315>
    t1315 max_(t1315 x, t1315 y) {
        return (([&]() -> t1315 {
            using a = t1315;
            return (((bool) (x > y)) ? 
                (([&]() -> t1315 {
                    return x;
                })())
            :
                (([&]() -> t1315 {
                    return y;
                })()));
        })());
    }
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2) {
        return ((double) (b1 + ((double) (((double) (((double) (x - a1)) * ((double) (b2 - b1)))) / ((double) (a2 - a1))))));
    }
}

namespace Math {
    template<typename t1318>
    t1318 clamp(t1318 x, t1318 min, t1318 max) {
        return (([&]() -> t1318 {
            using a = t1318;
            return (((bool) (min > x)) ? 
                (([&]() -> t1318 {
                    return min;
                })())
            :
                (((bool) (x > max)) ? 
                    (([&]() -> t1318 {
                        return max;
                    })())
                :
                    (([&]() -> t1318 {
                        return x;
                    })())));
        })());
    }
}

namespace Math {
    template<typename t1323>
    int8_t sign(t1323 n) {
        return (([&]() -> int8_t {
            using a = t1323;
            return (((bool) (n == ((t1323) 0))) ? 
                (([&]() -> int8_t {
                    return ((int8_t) 0);
                })())
            :
                (((bool) (n > ((t1323) 0))) ? 
                    (([&]() -> int8_t {
                        return ((int8_t) 1);
                    })())
                :
                    (([&]() -> int8_t {
                        return -(((int8_t) 1));
                    })())));
        })());
    }
}

namespace Button {
    juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> state() {
        return (juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>((([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid146;
            guid146.actualState = Io::low();
            guid146.lastState = Io::low();
            guid146.lastDebounceTime = ((uint32_t) 0);
            return guid146;
        })())));
    }
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> buttonState) {
        return Signal::map<Io::pinState, juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>, Io::pinState>(juniper::function<juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>, Io::pinState(Io::pinState)>(juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>(buttonState, delay), [](juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>& junclosure, Io::pinState currentState) -> Io::pinState { 
            juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>& buttonState = junclosure.buttonState;
            uint16_t& delay = junclosure.delay;
            return (([&]() -> Io::pinState {
                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid147 = (*((buttonState).get()));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lastDebounceTime = (guid147).lastDebounceTime;
                Io::pinState lastState = (guid147).lastState;
                Io::pinState actualState = (guid147).actualState;
                
                return (((bool) (currentState != lastState)) ? 
                    (([&]() -> Io::pinState {
                        (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid148;
                            guid148.actualState = actualState;
                            guid148.lastState = currentState;
                            guid148.lastDebounceTime = Time::now();
                            return guid148;
                        })()));
                        return actualState;
                    })())
                :
                    (((bool) (((bool) (currentState != actualState)) && ((bool) (((uint32_t) (Time::now() - ((buttonState).get())->lastDebounceTime)) > delay)))) ? 
                        (([&]() -> Io::pinState {
                            (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid149;
                                guid149.actualState = currentState;
                                guid149.lastState = currentState;
                                guid149.lastDebounceTime = lastDebounceTime;
                                return guid149;
                            })()));
                            return currentState;
                        })())
                    :
                        (([&]() -> Io::pinState {
                            (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid150;
                                guid150.actualState = actualState;
                                guid150.lastState = currentState;
                                guid150.lastDebounceTime = lastDebounceTime;
                                return guid150;
                            })()));
                            return actualState;
                        })())));
            })());
         }), incoming);
    }
}

namespace Button {
    Prelude::sig<Io::pinState> debounce(Prelude::sig<Io::pinState> incoming, juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> buttonState) {
        return debounceDelay(incoming, ((uint16_t) 50), buttonState);
    }
}

namespace Vector {
    uint8_t x = ((uint8_t) 0);
}

namespace Vector {
    uint8_t y = ((uint8_t) 1);
}

namespace Vector {
    uint8_t z = ((uint8_t) 2);
}

namespace Vector {
    template<typename t1371, int c82>
    juniper::array<t1371, c82> add(juniper::array<t1371, c82> v1, juniper::array<t1371, c82> v2) {
        return (([&]() -> juniper::array<t1371, c82> {
            using a = t1371;
            constexpr int32_t n = c82;
            return (([&]() -> juniper::array<t1371, c82> {
                juniper::array<t1371, c82> guid151 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1371, c82> result = guid151;
                
                (([&]() -> juniper::unit {
                    int32_t guid152 = ((int32_t) 0);
                    int32_t guid153 = n;
                    for (int32_t i = guid152; i < guid153; i++) {
                        (([&]() -> t1371 {
                            return ((result)[i] += (v2)[i]);
                        })());
                    }
                    return {};
                })());
                return result;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1373, int c85>
    juniper::array<t1373, c85> zero() {
        return (([&]() -> juniper::array<t1373, c85> {
            using a = t1373;
            constexpr int32_t n = c85;
            return (juniper::array<t1373, c85>().fill(((t1373) 0)));
        })());
    }
}

namespace Vector {
    template<typename t1379, int c86>
    juniper::array<t1379, c86> subtract(juniper::array<t1379, c86> v1, juniper::array<t1379, c86> v2) {
        return (([&]() -> juniper::array<t1379, c86> {
            using a = t1379;
            constexpr int32_t n = c86;
            return (([&]() -> juniper::array<t1379, c86> {
                juniper::array<t1379, c86> guid154 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1379, c86> result = guid154;
                
                (([&]() -> juniper::unit {
                    int32_t guid155 = ((int32_t) 0);
                    int32_t guid156 = n;
                    for (int32_t i = guid155; i < guid156; i++) {
                        (([&]() -> t1379 {
                            return ((result)[i] -= (v2)[i]);
                        })());
                    }
                    return {};
                })());
                return result;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1383, int c89>
    juniper::array<t1383, c89> scale(t1383 scalar, juniper::array<t1383, c89> v) {
        return (([&]() -> juniper::array<t1383, c89> {
            using a = t1383;
            constexpr int32_t n = c89;
            return (([&]() -> juniper::array<t1383, c89> {
                juniper::array<t1383, c89> guid157 = v;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1383, c89> result = guid157;
                
                (([&]() -> juniper::unit {
                    int32_t guid158 = ((int32_t) 0);
                    int32_t guid159 = n;
                    for (int32_t i = guid158; i < guid159; i++) {
                        (([&]() -> t1383 {
                            return ((result)[i] *= scalar);
                        })());
                    }
                    return {};
                })());
                return result;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1389, int c91>
    t1389 dot(juniper::array<t1389, c91> v1, juniper::array<t1389, c91> v2) {
        return (([&]() -> t1389 {
            using a = t1389;
            constexpr int32_t n = c91;
            return (([&]() -> t1389 {
                t1389 guid160 = ((t1389) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1389 sum = guid160;
                
                (([&]() -> juniper::unit {
                    int32_t guid161 = ((int32_t) 0);
                    int32_t guid162 = n;
                    for (int32_t i = guid161; i < guid162; i++) {
                        (([&]() -> t1389 {
                            return (sum += ((t1389) ((v1)[i] * (v2)[i])));
                        })());
                    }
                    return {};
                })());
                return sum;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1395, int c94>
    t1395 magnitude2(juniper::array<t1395, c94> v) {
        return (([&]() -> t1395 {
            using a = t1395;
            constexpr int32_t n = c94;
            return (([&]() -> t1395 {
                t1395 guid163 = ((t1395) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1395 sum = guid163;
                
                (([&]() -> juniper::unit {
                    int32_t guid164 = ((int32_t) 0);
                    int32_t guid165 = n;
                    for (int32_t i = guid164; i < guid165; i++) {
                        (([&]() -> t1395 {
                            return (sum += ((t1395) ((v)[i] * (v)[i])));
                        })());
                    }
                    return {};
                })());
                return sum;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1397, int c97>
    double magnitude(juniper::array<t1397, c97> v) {
        return (([&]() -> double {
            using a = t1397;
            constexpr int32_t n = c97;
            return sqrt_(toDouble<t1397>(magnitude2<t1397, c97>(v)));
        })());
    }
}

namespace Vector {
    template<typename t1413, int c99>
    juniper::array<t1413, c99> multiply(juniper::array<t1413, c99> u, juniper::array<t1413, c99> v) {
        return (([&]() -> juniper::array<t1413, c99> {
            using a = t1413;
            constexpr int32_t n = c99;
            return (([&]() -> juniper::array<t1413, c99> {
                juniper::array<t1413, c99> guid166 = u;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1413, c99> result = guid166;
                
                (([&]() -> juniper::unit {
                    int32_t guid167 = ((int32_t) 0);
                    int32_t guid168 = n;
                    for (int32_t i = guid167; i < guid168; i++) {
                        (([&]() -> t1413 {
                            return ((result)[i] *= (v)[i]);
                        })());
                    }
                    return {};
                })());
                return result;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1422, int c102>
    juniper::array<t1422, c102> normalize(juniper::array<t1422, c102> v) {
        return (([&]() -> juniper::array<t1422, c102> {
            using a = t1422;
            constexpr int32_t n = c102;
            return (([&]() -> juniper::array<t1422, c102> {
                double guid169 = magnitude<t1422, c102>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid169;
                
                return (((bool) (mag > ((t1422) 0))) ? 
                    (([&]() -> juniper::array<t1422, c102> {
                        juniper::array<t1422, c102> guid170 = v;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::array<t1422, c102> result = guid170;
                        
                        (([&]() -> juniper::unit {
                            int32_t guid171 = ((int32_t) 0);
                            int32_t guid172 = n;
                            for (int32_t i = guid171; i < guid172; i++) {
                                (([&]() -> t1422 {
                                    return ((result)[i] = fromDouble<t1422>(((double) (toDouble<t1422>((result)[i]) / mag))));
                                })());
                            }
                            return {};
                        })());
                        return result;
                    })())
                :
                    (([&]() -> juniper::array<t1422, c102> {
                        return v;
                    })()));
            })());
        })());
    }
}

namespace Vector {
    template<typename t1433, int c106>
    double angle(juniper::array<t1433, c106> v1, juniper::array<t1433, c106> v2) {
        return (([&]() -> double {
            using a = t1433;
            constexpr int32_t n = c106;
            return acos_(((double) (toDouble<t1433>(dot<t1433, c106>(v1, v2)) / sqrt_(toDouble<t1433>(((t1433) (magnitude2<t1433, c106>(v1) * magnitude2<t1433, c106>(v2))))))));
        })());
    }
}

namespace Vector {
    template<typename t1475>
    juniper::array<t1475, 3> cross(juniper::array<t1475, 3> u, juniper::array<t1475, 3> v) {
        return (([&]() -> juniper::array<t1475, 3> {
            using a = t1475;
            return (juniper::array<t1475, 3> { {((t1475) (((t1475) ((u)[y] * (v)[z])) - ((t1475) ((u)[z] * (v)[y])))), ((t1475) (((t1475) ((u)[z] * (v)[x])) - ((t1475) ((u)[x] * (v)[z])))), ((t1475) (((t1475) ((u)[x] * (v)[y])) - ((t1475) ((u)[y] * (v)[x]))))} });
        })());
    }
}

namespace Vector {
    template<typename t1477, int c122>
    juniper::array<t1477, c122> project(juniper::array<t1477, c122> a, juniper::array<t1477, c122> b) {
        return (([&]() -> juniper::array<t1477, c122> {
            using z = t1477;
            constexpr int32_t n = c122;
            return (([&]() -> juniper::array<t1477, c122> {
                juniper::array<t1477, c122> guid173 = normalize<t1477, c122>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1477, c122> bn = guid173;
                
                return scale<t1477, c122>(dot<t1477, c122>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t1493, int c126>
    juniper::array<t1493, c126> projectPlane(juniper::array<t1493, c126> a, juniper::array<t1493, c126> m) {
        return (([&]() -> juniper::array<t1493, c126> {
            using z = t1493;
            constexpr int32_t n = c126;
            return subtract<t1493, c126>(a, project<t1493, c126>(a, m));
        })());
    }
}

namespace CharList {
    template<int c129>
    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c129>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 97))) && ((bool) (c <= ((uint8_t) 122))))) ? 
                ((uint8_t) (c - ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c130>
    juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c130>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 65))) && ((bool) (c <= ((uint8_t) 90))))) ? 
                ((uint8_t) (c + ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c131>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t> i32ToCharList(int32_t m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t> {
            constexpr int32_t n = c131;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t> guid174 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t> guid175;
                    guid175.data = (juniper::array<uint8_t, (1)+(c131)>().fill(((uint8_t) 0)));
                    guid175.length = ((uint32_t) 0);
                    return guid175;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c131)>, uint32_t> ret = guid174;
                
                (([&]() -> juniper::unit {
                    
    int charsPrinted = 1 + snprintf((char *) &ret.data[0], n + 1, "%d", m);
    ret.length = charsPrinted >= (n + 1) ? (n + 1) : charsPrinted;
    
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace CharList {
    template<int c132>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c132>, uint32_t> s) {
        return (([&]() -> uint32_t {
            constexpr int32_t n = c132;
            return ((uint32_t) ((s).length - ((uint32_t) 1)));
        })());
    }
}

namespace CharList {
    template<int c133, int c134, int c135>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c133)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> {
            constexpr int32_t aCap = c133;
            constexpr int32_t bCap = c134;
            constexpr int32_t retCap = c135;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> {
                uint32_t guid176 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid176;
                
                uint32_t guid177 = length<(1)+(c133)>(sA);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenA = guid177;
                
                uint32_t guid178 = length<(1)+(c134)>(sB);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenB = guid178;
                
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> guid179 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> guid180;
                    guid180.data = (juniper::array<uint8_t, (1)+(c135)>().fill(((uint8_t) 0)));
                    guid180.length = ((uint32_t) (((uint32_t) (lenA + lenB)) + ((uint32_t) 1)));
                    return guid180;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> out = guid179;
                
                (([&]() -> juniper::unit {
                    uint32_t guid181 = ((uint32_t) 0);
                    uint32_t guid182 = lenA;
                    for (uint32_t i = guid181; i < guid182; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sA).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid183 = ((uint32_t) 0);
                    uint32_t guid184 = lenB;
                    for (uint32_t i = guid183; i < guid184; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sB).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return out;
            })());
        })());
    }
}

namespace CharList {
    template<int c142, int c143>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c142)*(-1)))+((c143)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c142)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c143)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c142)*(-1)))+((c143)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t aCap = c142;
            constexpr int32_t bCap = c143;
            return concat<c142, c143, (-1)+((((-1)+((c142)*(-1)))+((c143)*(-1)))*(-1))>(sA, sB);
        })());
    }
}

namespace Random {
    int32_t random_(int32_t low, int32_t high) {
        return (([&]() -> int32_t {
            int32_t guid185 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t ret = guid185;
            
            (([&]() -> juniper::unit {
                ret = random(low, high);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Random {
    juniper::unit seed(uint32_t n) {
        return (([&]() -> juniper::unit {
            randomSeed(n);
            return {};
        })());
    }
}

namespace Random {
    template<typename t1563, int c147>
    t1563 choice(juniper::records::recordt_0<juniper::array<t1563, c147>, uint32_t> lst) {
        return (([&]() -> t1563 {
            using t = t1563;
            constexpr int32_t n = c147;
            return (([&]() -> t1563 {
                int32_t guid186 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid186;
                
                return Maybe::get<t1563>(List::nth<t1563, c147>(i32ToU32(i), lst));
            })());
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_5<float, float, float> color) {
        return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> {
            juniper::records::recordt_5<float, float, float> guid187 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float v = (guid187).v;
            float s = (guid187).s;
            float h = (guid187).h;
            
            float guid188 = ((float) (v * s));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float c = guid188;
            
            float guid189 = ((float) (c * toFloat<double>(((double) (1.0 - Math::fabs_(((double) (Math::fmod_(((double) (toDouble<float>(h) / ((double) 60))), 2.0) - 1.0))))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float x = guid189;
            
            float guid190 = ((float) (v - c));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float m = guid190;
            
            juniper::tuple3<float, float, float> guid191 = (((bool) (((bool) (0.0f <= h)) && ((bool) (h < 60.0f)))) ? 
                (juniper::tuple3<float,float,float>{c, x, 0.0f})
            :
                (((bool) (((bool) (60.0f <= h)) && ((bool) (h < 120.0f)))) ? 
                    (juniper::tuple3<float,float,float>{x, c, 0.0f})
                :
                    (((bool) (((bool) (120.0f <= h)) && ((bool) (h < 180.0f)))) ? 
                        (juniper::tuple3<float,float,float>{0.0f, c, x})
                    :
                        (((bool) (((bool) (180.0f <= h)) && ((bool) (h < 240.0f)))) ? 
                            (juniper::tuple3<float,float,float>{0.0f, x, c})
                        :
                            (((bool) (((bool) (240.0f <= h)) && ((bool) (h < 300.0f)))) ? 
                                (juniper::tuple3<float,float,float>{x, 0.0f, c})
                            :
                                (juniper::tuple3<float,float,float>{c, 0.0f, x}))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float bPrime = (guid191).e3;
            float gPrime = (guid191).e2;
            float rPrime = (guid191).e1;
            
            float guid192 = ((float) (((float) (rPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r = guid192;
            
            float guid193 = ((float) (((float) (gPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g = guid193;
            
            float guid194 = ((float) (((float) (bPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b = guid194;
            
            return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
                juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid195;
                guid195.r = toUInt8<float>(r);
                guid195.g = toUInt8<float>(g);
                guid195.b = toUInt8<float>(b);
                return guid195;
            })());
        })());
    }
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> color) {
        return (([&]() -> uint16_t {
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid196 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = (guid196).b;
            uint8_t g = (guid196).g;
            uint8_t r = (guid196).r;
            
            return ((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (u8ToU16(r) & ((uint16_t) 248))) << ((uint32_t) 8))) | ((uint16_t) (((uint16_t) (u8ToU16(g) & ((uint16_t) 252))) << ((uint32_t) 3))))) | ((uint16_t) (u8ToU16(b) >> ((uint32_t) 3)))));
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> red = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid197;
        guid197.r = ((uint8_t) 255);
        guid197.g = ((uint8_t) 0);
        guid197.b = ((uint8_t) 0);
        return guid197;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> green = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid198;
        guid198.r = ((uint8_t) 0);
        guid198.g = ((uint8_t) 255);
        guid198.b = ((uint8_t) 0);
        return guid198;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> blue = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid199;
        guid199.r = ((uint8_t) 0);
        guid199.g = ((uint8_t) 0);
        guid199.b = ((uint8_t) 255);
        return guid199;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> black = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid200;
        guid200.r = ((uint8_t) 0);
        guid200.g = ((uint8_t) 0);
        guid200.b = ((uint8_t) 0);
        return guid200;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> white = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid201;
        guid201.r = ((uint8_t) 255);
        guid201.g = ((uint8_t) 255);
        guid201.b = ((uint8_t) 255);
        return guid201;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> yellow = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid202;
        guid202.r = ((uint8_t) 255);
        guid202.g = ((uint8_t) 255);
        guid202.b = ((uint8_t) 0);
        return guid202;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> magenta = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid203;
        guid203.r = ((uint8_t) 255);
        guid203.g = ((uint8_t) 0);
        guid203.b = ((uint8_t) 255);
        return guid203;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> cyan = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid204;
        guid204.r = ((uint8_t) 0);
        guid204.g = ((uint8_t) 255);
        guid204.b = ((uint8_t) 255);
        return guid204;
    })());
}

namespace Arcada {
    bool arcadaBegin() {
        return (([&]() -> bool {
            bool guid205 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid205;
            
            (([&]() -> juniper::unit {
                ret = arcada.arcadaBegin();
                return {};
            })());
            return ret;
        })());
    }
}

namespace Arcada {
    juniper::unit displayBegin() {
        return (([&]() -> juniper::unit {
            arcada.displayBegin();
            return {};
        })());
    }
}

namespace Arcada {
    juniper::unit setBacklight(uint8_t level) {
        return (([&]() -> juniper::unit {
            arcada.setBacklight(level);
            return {};
        })());
    }
}

namespace Arcada {
    uint16_t displayWidth() {
        return (([&]() -> uint16_t {
            uint16_t guid206 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid206;
            
            (([&]() -> juniper::unit {
                w = arcada.display->width();
                return {};
            })());
            return w;
        })());
    }
}

namespace Arcada {
    uint16_t displayHeight() {
        return (([&]() -> uint16_t {
            uint16_t guid207 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid207;
            
            (([&]() -> juniper::unit {
                h = arcada.display->width();
                return {};
            })());
            return h;
        })());
    }
}

namespace Arcada {
    bool createDoubleBuffer() {
        return (([&]() -> bool {
            uint16_t guid208 = displayWidth();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid208;
            
            uint16_t guid209 = displayHeight();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid209;
            
            bool guid210 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid210;
            
            (([&]() -> juniper::unit {
                ret = arcada.createFrameBuffer(w, h);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Arcada {
    bool blitDoubleBuffer() {
        return (([&]() -> bool {
            bool guid211 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid211;
            
            (([&]() -> juniper::unit {
                ret = arcada.blitFrameBuffer(0, 0, true, false);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    Ble::advertisingFlagt bleGapAdvFlagLeLimitedDiscMode = (([]() -> Ble::advertisingFlagt {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE;
            return {};
        })());
        return advertisingFlag(n);
    })());
}

namespace Ble {
    Ble::advertisingFlagt bleGapAdvFlagLeGeneralDiscMode = (([]() -> Ble::advertisingFlagt {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE;
            return {};
        })());
        return advertisingFlag(n);
    })());
}

namespace Ble {
    Ble::advertisingFlagt bleGapAdvFlagBrEdrNotSupported = (([]() -> Ble::advertisingFlagt {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
            return {};
        })());
        return advertisingFlag(n);
    })());
}

namespace Ble {
    Ble::advertisingFlagt bleGapAdvFlagLeBrEdrController = (([]() -> Ble::advertisingFlagt {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_GAP_ADV_FLAG_LE_BR_EDR_CONTROLLER;
            return {};
        })());
        return advertisingFlag(n);
    })());
}

namespace Ble {
    Ble::advertisingFlagt bleGapAdvFlagLeBrEdrHost = (([]() -> Ble::advertisingFlagt {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_GAP_ADV_FLAG_LE_BR_EDR_HOST;
            return {};
        })());
        return advertisingFlag(n);
    })());
}

namespace Ble {
    Ble::advertisingFlagt bleGapAdvFlagsLeOnlyLimitedDiscMode = (([]() -> Ble::advertisingFlagt {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
            return {};
        })());
        return advertisingFlag(n);
    })());
}

namespace Ble {
    Ble::advertisingFlagt bleGapAdvFlagsLeOnlyGeneralDiscMode = (([]() -> Ble::advertisingFlagt {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
            return {};
        })());
        return advertisingFlag(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceUnknown = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_UNKNOWN;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericPhone = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_PHONE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericComputer = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_COMPUTER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericWatch = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_WATCH;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericSportsWatch = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_WATCH_SPORTS_WATCH;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericClock = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_CLOCK;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericDisplay = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_DISPLAY;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericRemoteControl = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_REMOTE_CONTROL;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericEyeGlasses = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_EYE_GLASSES;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericTag = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_TAG;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericKeyring = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_KEYRING;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericMediaPlayer = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_MEDIA_PLAYER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericBarcodeScanner = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_BARCODE_SCANNER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericThermometer = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_THERMOMETER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceThermometerEar = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_THERMOMETER_EAR;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericHeartRateSensor = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_HEART_RATE_SENSOR;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHeartRateSensorHeartRateBelt = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HEART_RATE_SENSOR_HEART_RATE_BELT;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericBloodPressure = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_BLOOD_PRESSURE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceBloodPressureArm = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_BLOOD_PRESSURE_ARM;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceBloodPressureWrist = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_BLOOD_PRESSURE_WRIST;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericHid = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_HID;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidKeyboard = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_KEYBOARD;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidMouse = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_MOUSE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidJoystick = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_JOYSTICK;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidGamepad = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_GAMEPAD;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidDigitizerSubtype = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_DIGITIZERSUBTYPE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidCardReader = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_CARD_READER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidDigitalPen = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_DIGITAL_PEN;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceHidBarcode = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_HID_BARCODE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericGlucoseMeter = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_GLUCOSE_METER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericRunningWalkingSensor = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_RUNNING_WALKING_SENSOR;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceRunningWalkingSensorInShoe = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_RUNNING_WALKING_SENSOR_IN_SHOE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceRunningWalkingSensorOnShoe = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_RUNNING_WALKING_SENSOR_ON_SHOE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceRunningWalkingSensorOnHip = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_RUNNING_WALKING_SENSOR_ON_HIP;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericCycling = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_CYCLING;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceCyclingCyclingComputer = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_CYCLING_CYCLING_COMPUTER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceCyclingSpeedSensor = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_CYCLING_SPEED_SENSOR;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceCyclingCadenceSensor = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_CYCLING_CADENCE_SENSOR;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceCyclingPowerSensor = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_CYCLING_POWER_SENSOR;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceCyclingSpeedCadenceSensor = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_CYCLING_SPEED_CADENCE_SENSOR;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericPulseOximeter = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_PULSE_OXIMETER;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearancePulseOximiterFingertip = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_PULSE_OXIMETER_FINGERTIP;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearancePluseOximeterWristWorn = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_PULSE_OXIMETER_WRIST_WORN;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericWeightScale = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_WEIGHT_SCALE;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceGenericOutdoorSportsAct = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_GENERIC_OUTDOOR_SPORTS_ACT;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceOutdoorSportsActLocDisp = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_DISP;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceOutdoorSportsActLocAndNavDisp = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_AND_NAV_DISP;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceOutdoorSportsActLocPod = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_POD;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::appearancet appearanceOutdoorSportsActLocAndNavPod = (([]() -> Ble::appearancet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_AND_NAV_POD;
            return {};
        })());
        return appearance(n);
    })());
}

namespace Ble {
    Ble::secureModet secModeNoAccess = (([]() -> Ble::secureModet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = (uint16_t) SECMODE_NO_ACCESS;
            return {};
        })());
        return secureMode(n);
    })());
}

namespace Ble {
    Ble::secureModet secModeOpen = (([]() -> Ble::secureModet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = (uint16_t) SECMODE_OPEN;
            return {};
        })());
        return secureMode(n);
    })());
}

namespace Ble {
    Ble::secureModet secModeEncNoMITM = (([]() -> Ble::secureModet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = (uint16_t) SECMODE_ENC_NO_MITM;
            return {};
        })());
        return secureMode(n);
    })());
}

namespace Ble {
    Ble::secureModet secModeEncWithMITM = (([]() -> Ble::secureModet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = (uint16_t) SECMODE_ENC_WITH_MITM;
            return {};
        })());
        return secureMode(n);
    })());
}

namespace Ble {
    Ble::secureModet secModeSignedNoMITM = (([]() -> Ble::secureModet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = (uint16_t) SECMODE_SIGNED_NO_MITM;
            return {};
        })());
        return secureMode(n);
    })());
}

namespace Ble {
    Ble::secureModet secModeSignedWithMITM = (([]() -> Ble::secureModet {
        uint16_t n;
        
        (([&]() -> juniper::unit {
            n = (uint16_t) SECMODE_SIGNED_WITH_MITM;
            return {};
        })());
        return secureMode(n);
    })());
}

namespace Ble {
    Ble::propertiest propertiesBroadcast = (([]() -> Ble::propertiest {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = (uint8_t) CHR_PROPS_BROADCAST;
            return {};
        })());
        return properties(n);
    })());
}

namespace Ble {
    Ble::propertiest propertiesRead = (([]() -> Ble::propertiest {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = (uint8_t) CHR_PROPS_READ;
            return {};
        })());
        return properties(n);
    })());
}

namespace Ble {
    Ble::propertiest propertiesWriteWoResp = (([]() -> Ble::propertiest {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = (uint8_t) CHR_PROPS_WRITE_WO_RESP;
            return {};
        })());
        return properties(n);
    })());
}

namespace Ble {
    Ble::propertiest propertiesWrite = (([]() -> Ble::propertiest {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = (uint8_t) CHR_PROPS_WRITE;
            return {};
        })());
        return properties(n);
    })());
}

namespace Ble {
    Ble::propertiest propertiesNotify = (([]() -> Ble::propertiest {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = (uint8_t) CHR_PROPS_NOTIFY;
            return {};
        })());
        return properties(n);
    })());
}

namespace Ble {
    Ble::propertiest propertiesIndicate = (([]() -> Ble::propertiest {
        uint8_t n;
        
        (([&]() -> juniper::unit {
            n = (uint8_t) CHR_PROPS_INDICATE;
            return {};
        })());
        return properties(n);
    })());
}

namespace Ble {
    juniper::unit beginService(Ble::servicet s) {
        return (([&]() -> juniper::unit {
            Ble::servicet guid212 = s;
            if (!(((bool) (((bool) ((guid212).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid212).service();
            
            return (([&]() -> juniper::unit {
                ((BLEService *) p)->begin();
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit beginCharacterstic(Ble::characterstict c) {
        return (([&]() -> juniper::unit {
            Ble::characterstict guid213 = c;
            if (!(((bool) (((bool) ((guid213).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid213).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->begin();
                return {};
            })());
        })());
    }
}

namespace Ble {
    template<int c149>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c149>, uint32_t> bytes) {
        return (([&]() -> uint16_t {
            constexpr int32_t n = c149;
            return (([&]() -> uint16_t {
                juniper::records::recordt_0<juniper::array<uint8_t, c149>, uint32_t> guid214 = bytes;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t length = (guid214).length;
                juniper::array<uint8_t, c149> data = (guid214).data;
                
                Ble::characterstict guid215 = c;
                if (!(((bool) (((bool) ((guid215).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid215).characterstic();
                
                uint16_t guid216 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid216;
                
                (([&]() -> juniper::unit {
                    ret = ((BLECharacteristic *) p)->write((void *) &data[0], length);
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Ble {
    uint16_t writeCharacterstic8(Ble::characterstict c, uint8_t n) {
        return (([&]() -> uint16_t {
            Ble::characterstict guid217 = c;
            if (!(((bool) (((bool) ((guid217).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid217).characterstic();
            
            uint16_t guid218 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid218;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write8(n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    uint16_t writeCharacterstic16(Ble::characterstict c, uint16_t n) {
        return (([&]() -> uint16_t {
            Ble::characterstict guid219 = c;
            if (!(((bool) (((bool) ((guid219).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid219).characterstic();
            
            uint16_t guid220 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid220;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write16(n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    uint16_t writeCharacterstic32(Ble::characterstict c, uint32_t n) {
        return (([&]() -> uint16_t {
            Ble::characterstict guid221 = c;
            if (!(((bool) (((bool) ((guid221).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid221).characterstic();
            
            uint16_t guid222 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid222;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write32(n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    uint16_t writeCharactersticSigned32(Ble::characterstict c, int32_t n) {
        return (([&]() -> uint16_t {
            Ble::characterstict guid223 = c;
            if (!(((bool) (((bool) ((guid223).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid223).characterstic();
            
            uint16_t guid224 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid224;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write32((int) n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    template<typename t1917>
    uint16_t writeGeneric(Ble::characterstict c, t1917 x) {
        return (([&]() -> uint16_t {
            using t = t1917;
            return (([&]() -> uint16_t {
                Ble::characterstict guid225 = c;
                if (!(((bool) (((bool) ((guid225).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid225).characterstic();
                
                uint16_t guid226 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid226;
                
                (([&]() -> juniper::unit {
                    ret = ((BLECharacteristic *) p)->write((void *) &x, sizeof(t));
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Ble {
    template<typename t1922>
    t1922 readGeneric(Ble::characterstict c) {
        return (([&]() -> t1922 {
            using t = t1922;
            return (([&]() -> t1922 {
                Ble::characterstict guid227 = c;
                if (!(((bool) (((bool) ((guid227).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid227).characterstic();
                
                t1922 ret;
                
                (([&]() -> juniper::unit {
                    ((BLECharacteristic *) p)->read((void *) &ret, sizeof(t));
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Ble {
    juniper::unit setCharacteristicPermission(Ble::characterstict c, Ble::secureModet readPermission, Ble::secureModet writePermission) {
        return (([&]() -> juniper::unit {
            Ble::secureModet guid228 = readPermission;
            if (!(((bool) (((bool) ((guid228).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t readN = (guid228).secureMode();
            
            Ble::secureModet guid229 = writePermission;
            if (!(((bool) (((bool) ((guid229).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t writeN = (guid229).secureMode();
            
            Ble::characterstict guid230 = c;
            if (!(((bool) (((bool) ((guid230).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid230).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->setPermission((SecureMode_t) readN, (SecureMode_t) writeN);
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit setCharacteristicProperties(Ble::characterstict c, Ble::propertiest prop) {
        return (([&]() -> juniper::unit {
            Ble::propertiest guid231 = prop;
            if (!(((bool) (((bool) ((guid231).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t propN = (guid231).properties();
            
            Ble::characterstict guid232 = c;
            if (!(((bool) (((bool) ((guid232).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid232).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->setProperties(propN);
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit setCharacteristicFixedLen(Ble::characterstict c, uint32_t size) {
        return (([&]() -> juniper::unit {
            Ble::characterstict guid233 = c;
            if (!(((bool) (((bool) ((guid233).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid233).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->setFixedLen(size);
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitBegin() {
        return (([&]() -> juniper::unit {
            Bluefruit.begin();
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitPeriphSetConnInterval(uint16_t minTime, uint16_t maxTime) {
        return (([&]() -> juniper::unit {
            Bluefruit.Periph.setConnInterval(minTime, maxTime);
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitSetTxPower(int8_t power) {
        return (([&]() -> juniper::unit {
            Bluefruit.setTxPower(power);
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitSetName(const char * name) {
        return (([&]() -> juniper::unit {
            Bluefruit.setName(name);
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddFlags(Ble::advertisingFlagt flag) {
        return (([&]() -> juniper::unit {
            Ble::advertisingFlagt guid234 = flag;
            if (!(((bool) (((bool) ((guid234).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t flagNum = (guid234).advertisingFlag();
            
            return (([&]() -> juniper::unit {
                Bluefruit.Advertising.addFlags(flagNum);
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddTxPower() {
        return (([&]() -> juniper::unit {
            Bluefruit.Advertising.addTxPower();
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddAppearance(Ble::appearancet app) {
        return (([&]() -> juniper::unit {
            Ble::appearancet guid235 = app;
            if (!(((bool) (((bool) ((guid235).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t flagNum = (guid235).appearance();
            
            return (([&]() -> juniper::unit {
                Bluefruit.Advertising.addAppearance(flagNum);
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddService(Ble::servicet serv) {
        return (([&]() -> juniper::unit {
            Ble::servicet guid236 = serv;
            if (!(((bool) (((bool) ((guid236).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid236).service();
            
            return (([&]() -> juniper::unit {
                Bluefruit.Advertising.addService(*((BLEService *) p));
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingAddName() {
        return (([&]() -> juniper::unit {
            Bluefruit.Advertising.addName();
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingRestartOnDisconnect(bool restart) {
        return (([&]() -> juniper::unit {
            Bluefruit.Advertising.restartOnDisconnect(restart);
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingSetInterval(uint16_t slow, uint16_t fast) {
        return (([&]() -> juniper::unit {
            Bluefruit.Advertising.setInterval(slow, fast);
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingSetFastTimeout(uint16_t sec) {
        return (([&]() -> juniper::unit {
            Bluefruit.Advertising.setFastTimeout(sec);
            return {};
        })());
    }
}

namespace Ble {
    juniper::unit bluefruitAdvertisingStart(uint16_t timeout) {
        return (([&]() -> juniper::unit {
            Bluefruit.Advertising.start(timeout);
            return {};
        })());
    }
}

namespace CWatch {
    Ble::servicet timeService = (([]() -> Ble::servicet {
        void * p;
        
        (([&]() -> juniper::unit {
            p = (void *) &rawTimeService;
            return {};
        })());
        return Ble::service(p);
    })());
}

namespace CWatch {
    Ble::characterstict dayDateTimeCharacterstic = (([]() -> Ble::characterstict {
        void * p;
        
        (([&]() -> juniper::unit {
            p = (void *) &rawDayDateTimeCharacterstic;
            return {};
        })());
        return Ble::characterstic(p);
    })());
}

namespace CWatch {
    Ble::characterstict dayOfWeekCharacterstic = (([]() -> Ble::characterstict {
        void * p;
        
        (([&]() -> juniper::unit {
            p = (void *) &rawDayOfWeek;
            return {};
        })());
        return Ble::characterstic(p);
    })());
}

namespace CWatch {
    juniper::unit setup() {
        return (([&]() -> juniper::unit {
            Arcada::arcadaBegin();
            Arcada::displayBegin();
            Arcada::setBacklight(((uint8_t) 255));
            Arcada::createDoubleBuffer();
            Ble::bluefruitBegin();
            Ble::bluefruitPeriphSetConnInterval(((uint16_t) 9), ((uint16_t) 16));
            Ble::bluefruitSetTxPower(((int8_t) 4));
            Ble::bluefruitSetName(((const PROGMEM char *)("CWatch")));
            Ble::setCharacteristicProperties(dayDateTimeCharacterstic, Ble::propertiesWrite);
            Ble::setCharacteristicProperties(dayOfWeekCharacterstic, Ble::propertiesWrite);
            Ble::setCharacteristicPermission(dayDateTimeCharacterstic, Ble::secModeNoAccess, Ble::secModeOpen);
            Ble::setCharacteristicPermission(dayOfWeekCharacterstic, Ble::secModeNoAccess, Ble::secModeOpen);
            Ble::beginService(timeService);
            Ble::beginCharacterstic(dayDateTimeCharacterstic);
            Ble::beginCharacterstic(dayOfWeekCharacterstic);
            Ble::setCharacteristicFixedLen(dayDateTimeCharacterstic, sizeof(juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>));
            Ble::setCharacteristicFixedLen(dayOfWeekCharacterstic, sizeof(juniper::records::recordt_8<uint8_t>));
            (([&]() -> juniper::unit {
                
    rawDayDateTimeCharacterstic.setWriteCallback(onWriteDayDateTime);
    rawDayOfWeek.setWriteCallback(onWriteDayOfWeek);
    
                return {};
            })());
            Ble::bluefruitAdvertisingAddFlags(Ble::bleGapAdvFlagsLeOnlyGeneralDiscMode);
            Ble::bluefruitAdvertisingAddTxPower();
            Ble::bluefruitAdvertisingAddAppearance(Ble::appearanceGenericWatch);
            Ble::bluefruitAdvertisingAddService(timeService);
            Ble::bluefruitAdvertisingAddName();
            Ble::bluefruitAdvertisingRestartOnDisconnect(true);
            Ble::bluefruitAdvertisingSetInterval(((uint16_t) 32), ((uint16_t) 244));
            Ble::bluefruitAdvertisingSetFastTimeout(((uint16_t) 30));
            return Ble::bluefruitAdvertisingStart(((uint16_t) 0));
        })());
    }
}

namespace CWatch {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> pink = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid237;
        guid237.r = ((uint8_t) 252);
        guid237.g = ((uint8_t) 92);
        guid237.b = ((uint8_t) 125);
        return guid237;
    })());
}

namespace CWatch {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> purpleBlue = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid238;
        guid238.r = ((uint8_t) 106);
        guid238.g = ((uint8_t) 130);
        guid238.b = ((uint8_t) 251);
        return guid238;
    })());
}

namespace CWatch {
    bool isLeapYear(uint32_t y) {
        return (((bool) (((uint32_t) (y % ((uint32_t) 4))) != ((uint32_t) 0))) ? 
            false
        :
            (((bool) (((uint32_t) (y % ((uint32_t) 100))) != ((uint32_t) 0))) ? 
                true
            :
                (((bool) (((uint32_t) (y % ((uint32_t) 400))) == ((uint32_t) 0))) ? 
                    true
                :
                    false)));
    }
}

namespace CWatch {
    uint8_t daysInMonth(uint32_t y, CWatch::month m) {
        return (([&]() -> uint8_t {
            CWatch::month guid239 = m;
            return (((bool) (((bool) ((guid239).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 31);
                })())
            :
                (((bool) (((bool) ((guid239).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return (isLeapYear(y) ? 
                            ((uint8_t) 29)
                        :
                            ((uint8_t) 28));
                    })())
                :
                    (((bool) (((bool) ((guid239).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 31);
                        })())
                    :
                        (((bool) (((bool) ((guid239).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> uint8_t {
                                return ((uint8_t) 30);
                            })())
                        :
                            (((bool) (((bool) ((guid239).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> uint8_t {
                                    return ((uint8_t) 31);
                                })())
                            :
                                (((bool) (((bool) ((guid239).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> uint8_t {
                                        return ((uint8_t) 30);
                                    })())
                                :
                                    (((bool) (((bool) ((guid239).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> uint8_t {
                                            return ((uint8_t) 31);
                                        })())
                                    :
                                        (((bool) (((bool) ((guid239).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> uint8_t {
                                                return ((uint8_t) 31);
                                            })())
                                        :
                                            (((bool) (((bool) ((guid239).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> uint8_t {
                                                    return ((uint8_t) 30);
                                                })())
                                            :
                                                (((bool) (((bool) ((guid239).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> uint8_t {
                                                        return ((uint8_t) 31);
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid239).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> uint8_t {
                                                            return ((uint8_t) 30);
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid239).id() == ((uint8_t) 11))) && true)) ? 
                                                            (([&]() -> uint8_t {
                                                                return ((uint8_t) 31);
                                                            })())
                                                        :
                                                            juniper::quit<uint8_t>()))))))))))));
        })());
    }
}

namespace CWatch {
    CWatch::month nextMonth(CWatch::month m) {
        return (([&]() -> CWatch::month {
            CWatch::month guid240 = m;
            return (((bool) (((bool) ((guid240).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::month {
                    return february();
                })())
            :
                (((bool) (((bool) ((guid240).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::month {
                        return march();
                    })())
                :
                    (((bool) (((bool) ((guid240).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::month {
                            return april();
                        })())
                    :
                        (((bool) (((bool) ((guid240).id() == ((uint8_t) 4))) && true)) ? 
                            (([&]() -> CWatch::month {
                                return june();
                            })())
                        :
                            (((bool) (((bool) ((guid240).id() == ((uint8_t) 5))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return july();
                                })())
                            :
                                (((bool) (((bool) ((guid240).id() == ((uint8_t) 7))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return august();
                                    })())
                                :
                                    (((bool) (((bool) ((guid240).id() == ((uint8_t) 8))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return october();
                                        })())
                                    :
                                        (((bool) (((bool) ((guid240).id() == ((uint8_t) 9))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return november();
                                            })())
                                        :
                                            (((bool) (((bool) ((guid240).id() == ((uint8_t) 11))) && true)) ? 
                                                (([&]() -> CWatch::month {
                                                    return january();
                                                })())
                                            :
                                                juniper::quit<CWatch::month>())))))))));
        })());
    }
}

namespace CWatch {
    CWatch::dayOfWeek nextDayOfWeek(CWatch::dayOfWeek dw) {
        return (([&]() -> CWatch::dayOfWeek {
            CWatch::dayOfWeek guid241 = dw;
            return (((bool) (((bool) ((guid241).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::dayOfWeek {
                    return monday();
                })())
            :
                (((bool) (((bool) ((guid241).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::dayOfWeek {
                        return tuesday();
                    })())
                :
                    (((bool) (((bool) ((guid241).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::dayOfWeek {
                            return wednesday();
                        })())
                    :
                        (((bool) (((bool) ((guid241).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> CWatch::dayOfWeek {
                                return thursday();
                            })())
                        :
                            (((bool) (((bool) ((guid241).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return friday();
                                })())
                            :
                                (((bool) (((bool) ((guid241).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return saturday();
                                    })())
                                :
                                    (((bool) (((bool) ((guid241).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> CWatch::dayOfWeek {
                                            return sunday();
                                        })())
                                    :
                                        juniper::quit<CWatch::dayOfWeek>())))))));
        })());
    }
}

namespace CWatch {
    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> dayOfWeekToCharList(CWatch::dayOfWeek dw) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
            CWatch::dayOfWeek guid242 = dw;
            return (((bool) (((bool) ((guid242).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid243;
                        guid243.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid243.length = ((uint32_t) 4);
                        return guid243;
                    })());
                })())
            :
                (((bool) (((bool) ((guid242).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid244;
                            guid244.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 111), ((uint8_t) 110), ((uint8_t) 0)} });
                            guid244.length = ((uint32_t) 4);
                            return guid244;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid242).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid245;
                                guid245.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 117), ((uint8_t) 101), ((uint8_t) 0)} });
                                guid245.length = ((uint32_t) 4);
                                return guid245;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid242).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid246;
                                    guid246.data = (juniper::array<uint8_t, 4> { {((uint8_t) 87), ((uint8_t) 101), ((uint8_t) 100), ((uint8_t) 0)} });
                                    guid246.length = ((uint32_t) 4);
                                    return guid246;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid242).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid247;
                                        guid247.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 104), ((uint8_t) 117), ((uint8_t) 0)} });
                                        guid247.length = ((uint32_t) 4);
                                        return guid247;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid242).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid248;
                                            guid248.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 114), ((uint8_t) 105), ((uint8_t) 0)} });
                                            guid248.length = ((uint32_t) 4);
                                            return guid248;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid242).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid249;
                                                guid249.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 97), ((uint8_t) 116), ((uint8_t) 0)} });
                                                guid249.length = ((uint32_t) 4);
                                                return guid249;
                                            })());
                                        })())
                                    :
                                        juniper::quit<juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>>())))))));
        })());
    }
}

namespace CWatch {
    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> monthToCharList(CWatch::month m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
            CWatch::month guid250 = m;
            return (((bool) (((bool) ((guid250).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid251;
                        guid251.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 97), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid251.length = ((uint32_t) 4);
                        return guid251;
                    })());
                })())
            :
                (((bool) (((bool) ((guid250).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid252;
                            guid252.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 101), ((uint8_t) 98), ((uint8_t) 0)} });
                            guid252.length = ((uint32_t) 4);
                            return guid252;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid250).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid253;
                                guid253.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 114), ((uint8_t) 0)} });
                                guid253.length = ((uint32_t) 4);
                                return guid253;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid250).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid254;
                                    guid254.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 112), ((uint8_t) 114), ((uint8_t) 0)} });
                                    guid254.length = ((uint32_t) 4);
                                    return guid254;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid250).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid255;
                                        guid255.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 121), ((uint8_t) 0)} });
                                        guid255.length = ((uint32_t) 4);
                                        return guid255;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid250).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid256;
                                            guid256.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                                            guid256.length = ((uint32_t) 4);
                                            return guid256;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid250).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid257;
                                                guid257.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 108), ((uint8_t) 0)} });
                                                guid257.length = ((uint32_t) 4);
                                                return guid257;
                                            })());
                                        })())
                                    :
                                        (((bool) (((bool) ((guid250).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid258;
                                                    guid258.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 117), ((uint8_t) 103), ((uint8_t) 0)} });
                                                    guid258.length = ((uint32_t) 4);
                                                    return guid258;
                                                })());
                                            })())
                                        :
                                            (((bool) (((bool) ((guid250).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid259;
                                                        guid259.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 101), ((uint8_t) 112), ((uint8_t) 0)} });
                                                        guid259.length = ((uint32_t) 4);
                                                        return guid259;
                                                    })());
                                                })())
                                            :
                                                (((bool) (((bool) ((guid250).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid260;
                                                            guid260.data = (juniper::array<uint8_t, 4> { {((uint8_t) 79), ((uint8_t) 99), ((uint8_t) 116), ((uint8_t) 0)} });
                                                            guid260.length = ((uint32_t) 4);
                                                            return guid260;
                                                        })());
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid250).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid261;
                                                                guid261.data = (juniper::array<uint8_t, 4> { {((uint8_t) 78), ((uint8_t) 111), ((uint8_t) 118), ((uint8_t) 0)} });
                                                                guid261.length = ((uint32_t) 4);
                                                                return guid261;
                                                            })());
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid250).id() == ((uint8_t) 11))) && true)) ? 
                                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid262;
                                                                    guid262.data = (juniper::array<uint8_t, 4> { {((uint8_t) 68), ((uint8_t) 101), ((uint8_t) 99), ((uint8_t) 0)} });
                                                                    guid262.length = ((uint32_t) 4);
                                                                    return guid262;
                                                                })());
                                                            })())
                                                        :
                                                            juniper::quit<juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>>()))))))))))));
        })());
    }
}

namespace CWatch {
    juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> secondTick(juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> d) {
        return (([&]() -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> {
            juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid263 = d;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek = (guid263).dayOfWeek;
            uint8_t seconds = (guid263).seconds;
            uint8_t minutes = (guid263).minutes;
            uint8_t hours = (guid263).hours;
            uint32_t year = (guid263).year;
            uint8_t day = (guid263).day;
            CWatch::month month = (guid263).month;
            
            uint8_t guid264 = ((uint8_t) (seconds + ((uint8_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t seconds1 = guid264;
            
            juniper::tuple2<uint8_t, uint8_t> guid265 = (((bool) (seconds1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (minutes + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{seconds1, minutes}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t minutes1 = (guid265).e2;
            uint8_t seconds2 = (guid265).e1;
            
            juniper::tuple2<uint8_t, uint8_t> guid266 = (((bool) (minutes1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (hours + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{minutes1, hours}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t hours1 = (guid266).e2;
            uint8_t minutes2 = (guid266).e1;
            
            juniper::tuple3<uint8_t, uint8_t, CWatch::dayOfWeek> guid267 = (((bool) (hours1 == ((uint8_t) 24))) ? 
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{((uint8_t) 0), ((uint8_t) (day + ((uint8_t) 1))), nextDayOfWeek(dayOfWeek)})
            :
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{hours1, day, dayOfWeek}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek2 = (guid267).e3;
            uint8_t day1 = (guid267).e2;
            uint8_t hours2 = (guid267).e1;
            
            uint8_t guid268 = daysInMonth(year, month);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t daysInCurrentMonth = guid268;
            
            juniper::tuple2<uint8_t, juniper::tuple2<CWatch::month, uint32_t>> guid269 = (((bool) (day1 > daysInCurrentMonth)) ? 
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month, uint32_t>>{((uint8_t) 1), (([&]() -> juniper::tuple2<CWatch::month, uint32_t> {
                    CWatch::month guid270 = month;
                    return (((bool) (((bool) ((guid270).id() == ((uint8_t) 11))) && true)) ? 
                        (([&]() -> juniper::tuple2<CWatch::month, uint32_t> {
                            return (juniper::tuple2<CWatch::month,uint32_t>{january(), ((uint32_t) (year + ((uint32_t) 1)))});
                        })())
                    :
                        (true ? 
                            (([&]() -> juniper::tuple2<CWatch::month, uint32_t> {
                                return (juniper::tuple2<CWatch::month,uint32_t>{nextMonth(month), year});
                            })())
                        :
                            juniper::quit<juniper::tuple2<CWatch::month, uint32_t>>()));
                })())})
            :
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month, uint32_t>>{day1, (juniper::tuple2<CWatch::month,uint32_t>{month, year})}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t year2 = ((guid269).e2).e2;
            CWatch::month month2 = ((guid269).e2).e1;
            uint8_t day2 = (guid269).e1;
            
            return (([&]() -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid271;
                guid271.month = month2;
                guid271.day = day2;
                guid271.year = year2;
                guid271.hours = hours2;
                guid271.minutes = minutes2;
                guid271.seconds = seconds2;
                guid271.dayOfWeek = dayOfWeek2;
                return guid271;
            })());
        })());
    }
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> clockTickerState = Time::state();
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>> clockState = (juniper::shared_ptr<juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>((([]() -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
        juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid272;
        guid272.month = september();
        guid272.day = ((uint8_t) 9);
        guid272.year = ((uint32_t) 2020);
        guid272.hours = ((uint8_t) 18);
        guid272.minutes = ((uint8_t) 40);
        guid272.seconds = ((uint8_t) 0);
        guid272.dayOfWeek = wednesday();
        return guid272;
    })())));
}

namespace CWatch {
    juniper::unit processBluetoothUpdates() {
        return (([&]() -> juniper::unit {
            bool guid273 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayDateTime = guid273;
            
            bool guid274 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayOfWeek = guid274;
            
            (([&]() -> juniper::unit {
                
    // rawHasNewDayDateTime and rawHasNewDayOfWeek are C/C++ global
    // variables that we defined previously. These variables are set
    // to true when the smartphone has finished writing data
    hasNewDayDateTime = rawHasNewDayDateTime;
    rawHasNewDayDateTime = false;
    hasNewDayOfWeek = rawHasNewDayOfWeek;
    rawHasNewDayOfWeek = false;
    
                return {};
            })());
            (([&]() -> juniper::unit {
                if (hasNewDayDateTime) {
                    (([&]() -> uint8_t {
                        juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> guid275 = Ble::readGeneric<juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>>(dayDateTimeCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> bleData = guid275;
                        
                        (((clockState).get())->month = (([&]() -> CWatch::month {
                            uint8_t guid276 = (bleData).month;
                            return (((bool) (((bool) (guid276 == ((int32_t) 0))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return january();
                                })())
                            :
                                (((bool) (((bool) (guid276 == ((int32_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return february();
                                    })())
                                :
                                    (((bool) (((bool) (guid276 == ((int32_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return march();
                                        })())
                                    :
                                        (((bool) (((bool) (guid276 == ((int32_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return april();
                                            })())
                                        :
                                            (((bool) (((bool) (guid276 == ((int32_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::month {
                                                    return may();
                                                })())
                                            :
                                                (((bool) (((bool) (guid276 == ((int32_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::month {
                                                        return june();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid276 == ((int32_t) 6))) && true)) ? 
                                                        (([&]() -> CWatch::month {
                                                            return july();
                                                        })())
                                                    :
                                                        (((bool) (((bool) (guid276 == ((int32_t) 7))) && true)) ? 
                                                            (([&]() -> CWatch::month {
                                                                return august();
                                                            })())
                                                        :
                                                            (((bool) (((bool) (guid276 == ((int32_t) 8))) && true)) ? 
                                                                (([&]() -> CWatch::month {
                                                                    return september();
                                                                })())
                                                            :
                                                                (((bool) (((bool) (guid276 == ((int32_t) 9))) && true)) ? 
                                                                    (([&]() -> CWatch::month {
                                                                        return october();
                                                                    })())
                                                                :
                                                                    (((bool) (((bool) (guid276 == ((int32_t) 10))) && true)) ? 
                                                                        (([&]() -> CWatch::month {
                                                                            return november();
                                                                        })())
                                                                    :
                                                                        (((bool) (((bool) (guid276 == ((int32_t) 11))) && true)) ? 
                                                                            (([&]() -> CWatch::month {
                                                                                return december();
                                                                            })())
                                                                        :
                                                                            (true ? 
                                                                                (([&]() -> CWatch::month {
                                                                                    return january();
                                                                                })())
                                                                            :
                                                                                juniper::quit<CWatch::month>())))))))))))));
                        })()));
                        (((clockState).get())->day = (bleData).day);
                        (((clockState).get())->year = (bleData).year);
                        (((clockState).get())->hours = (bleData).hours);
                        (((clockState).get())->minutes = (bleData).minutes);
                        return (((clockState).get())->seconds = (bleData).seconds);
                    })());
                }
                return {};
            })());
            return (([&]() -> juniper::unit {
                if (hasNewDayOfWeek) {
                    (([&]() -> CWatch::dayOfWeek {
                        juniper::records::recordt_8<uint8_t> guid277 = Ble::readGeneric<juniper::records::recordt_8<uint8_t>>(dayOfWeekCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_8<uint8_t> bleData = guid277;
                        
                        return (((clockState).get())->dayOfWeek = (([&]() -> CWatch::dayOfWeek {
                            uint8_t guid278 = (bleData).dayOfWeek;
                            return (((bool) (((bool) (guid278 == ((int32_t) 0))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return sunday();
                                })())
                            :
                                (((bool) (((bool) (guid278 == ((int32_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return monday();
                                    })())
                                :
                                    (((bool) (((bool) (guid278 == ((int32_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::dayOfWeek {
                                            return tuesday();
                                        })())
                                    :
                                        (((bool) (((bool) (guid278 == ((int32_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::dayOfWeek {
                                                return wednesday();
                                            })())
                                        :
                                            (((bool) (((bool) (guid278 == ((int32_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::dayOfWeek {
                                                    return thursday();
                                                })())
                                            :
                                                (((bool) (((bool) (guid278 == ((int32_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::dayOfWeek {
                                                        return friday();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid278 == ((int32_t) 6))) && true)) ? 
                                                        (([&]() -> CWatch::dayOfWeek {
                                                            return saturday();
                                                        })())
                                                    :
                                                        (true ? 
                                                            (([&]() -> CWatch::dayOfWeek {
                                                                return sunday();
                                                            })())
                                                        :
                                                            juniper::quit<CWatch::dayOfWeek>()))))))));
                        })()));
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace Gfx {
    juniper::unit drawFastHLine565(int16_t x, int16_t y, int16_t w, uint16_t c) {
        return (([&]() -> juniper::unit {
            arcada.getCanvas()->drawFastHLine(x, y, w, c);
            return {};
        })());
    }
}

namespace Gfx {
    juniper::unit drawVerticalGradient(int16_t x0i, int16_t y0i, int16_t w, int16_t h, juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c1, juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c2) {
        return (([&]() -> juniper::unit {
            int16_t guid279 = toInt16<uint16_t>(Arcada::displayWidth());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispW = guid279;
            
            int16_t guid280 = toInt16<uint16_t>(Arcada::displayHeight());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispH = guid280;
            
            int16_t guid281 = Math::clamp<int16_t>(x0i, ((int16_t) 0), ((int16_t) (dispW - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t x0 = guid281;
            
            int16_t guid282 = Math::clamp<int16_t>(y0i, ((int16_t) 0), ((int16_t) (dispH - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y0 = guid282;
            
            int16_t guid283 = ((int16_t) (y0i + h));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t ymax = guid283;
            
            int16_t guid284 = Math::clamp<int16_t>(ymax, ((int16_t) 0), dispH);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y1 = guid284;
            
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid285 = c1;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b1 = (guid285).b;
            uint8_t g1 = (guid285).g;
            uint8_t r1 = (guid285).r;
            
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid286 = c2;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b2 = (guid286).b;
            uint8_t g2 = (guid286).g;
            uint8_t r2 = (guid286).r;
            
            float guid287 = toFloat<uint8_t>(r1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r1f = guid287;
            
            float guid288 = toFloat<uint8_t>(g1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g1f = guid288;
            
            float guid289 = toFloat<uint8_t>(b1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b1f = guid289;
            
            float guid290 = toFloat<uint8_t>(r2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r2f = guid290;
            
            float guid291 = toFloat<uint8_t>(g2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g2f = guid291;
            
            float guid292 = toFloat<uint8_t>(b2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b2f = guid292;
            
            return (([&]() -> juniper::unit {
                int16_t guid293 = y0;
                int16_t guid294 = y1;
                for (int16_t y = guid293; y < guid294; y++) {
                    (([&]() -> juniper::unit {
                        float guid295 = ((float) (toFloat<int16_t>(((int16_t) (ymax - y))) / toFloat<int16_t>(h)));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight1 = guid295;
                        
                        float guid296 = ((float) (1.0f - weight1));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight2 = guid296;
                        
                        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid297 = (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
                            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid298;
                            guid298.r = toUInt8<float>(((float) (((float) (r1f * weight1)) + ((float) (r2f * weight2)))));
                            guid298.g = toUInt8<float>(((float) (((float) (g1f * weight1)) + ((float) (g2f * weight2)))));
                            guid298.b = toUInt8<float>(((float) (((float) (b1f * weight1)) + ((float) (g2f * weight2)))));
                            return guid298;
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> gradColor = guid297;
                        
                        uint16_t guid299 = Color::rgbToRgb565(gradColor);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint16_t gradColor565 = guid299;
                        
                        return drawFastHLine565(x0, y, w, gradColor565);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace Gfx {
    template<int c150>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c150)>, uint32_t> cl) {
        return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
            constexpr int32_t n = c150;
            return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
                int16_t guid300 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t xret = guid300;
                
                int16_t guid301 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t yret = guid301;
                
                uint16_t guid302 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t wret = guid302;
                
                uint16_t guid303 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t hret = guid303;
                
                (([&]() -> juniper::unit {
                    arcada.getCanvas()->getTextBounds((const char *) &cl.data[0], x, y, &xret, &yret, &wret, &hret);
                    return {};
                })());
                return (juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t>{xret, yret, wret, hret});
            })());
        })());
    }
}

namespace Gfx {
    template<int c151>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c151)>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c151;
            return (([&]() -> juniper::unit {
                arcada.getCanvas()->print((char *) &cl.data[0]);
                return {};
            })());
        })());
    }
}

namespace Gfx {
    juniper::unit setCursor(int16_t x, int16_t y) {
        return (([&]() -> juniper::unit {
            arcada.getCanvas()->setCursor(x, y);
            return {};
        })());
    }
}

namespace Gfx {
    juniper::unit setFont(Gfx::font f) {
        return (([&]() -> juniper::unit {
            Gfx::font guid304 = f;
            return (((bool) (((bool) ((guid304).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::unit {
                    return (([&]() -> juniper::unit {
                        arcada.getCanvas()->setFont();
                        return {};
                    })());
                })())
            :
                (((bool) (((bool) ((guid304).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::unit {
                        return (([&]() -> juniper::unit {
                            arcada.getCanvas()->setFont(&FreeSans9pt7b);
                            return {};
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid304).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                arcada.getCanvas()->setFont(&FreeSans24pt7b);
                                return {};
                            })());
                        })())
                    :
                        juniper::quit<juniper::unit>())));
        })());
    }
}

namespace Gfx {
    juniper::unit setTextColor(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c) {
        return (([&]() -> juniper::unit {
            uint16_t guid305 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid305;
            
            return (([&]() -> juniper::unit {
                arcada.getCanvas()->setTextColor(cPrime);
                return {};
            })());
        })());
    }
}

namespace Gfx {
    juniper::unit setTextSize(uint8_t size) {
        return (([&]() -> juniper::unit {
            arcada.getCanvas()->setTextSize(size);
            return {};
        })());
    }
}

namespace CWatch {
    bool loop() {
        return (([&]() -> bool {
            Gfx::drawVerticalGradient(((int16_t) 0), ((int16_t) 0), toInt16<uint16_t>(Arcada::displayWidth()), toInt16<uint16_t>(Arcada::displayHeight()), pink, purpleBlue);
            processBluetoothUpdates();
            Signal::sink<juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>, void>(juniper::function<void, juniper::unit(juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>)>([](juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> dt) -> juniper::unit { 
                return (([&]() -> juniper::unit {
                    juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid306 = dt;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    CWatch::dayOfWeek dayOfWeek = (guid306).dayOfWeek;
                    uint8_t seconds = (guid306).seconds;
                    uint8_t minutes = (guid306).minutes;
                    uint8_t hours = (guid306).hours;
                    uint32_t year = (guid306).year;
                    uint8_t day = (guid306).day;
                    CWatch::month month = (guid306).month;
                    
                    int32_t guid307 = toInt32<uint8_t>((((bool) (hours == ((uint8_t) 0))) ? 
                        ((uint8_t) 12)
                    :
                        (((bool) (hours > ((uint8_t) 12))) ? 
                            ((uint8_t) (hours - ((uint8_t) 12)))
                        :
                            hours)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    int32_t displayHours = guid307;
                    
                    Gfx::setTextColor(Color::white);
                    Gfx::setFont(Gfx::freeSans24());
                    Gfx::setTextSize(((uint8_t) 1));
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid308 = CharList::i32ToCharList<2>(displayHours);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> timeHourStr = guid308;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid309 = CharList::safeConcat<2, 1>(timeHourStr, (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid310;
                        guid310.data = (juniper::array<uint8_t, 2> { {((uint8_t) 58), ((uint8_t) 0)} });
                        guid310.length = ((uint32_t) 2);
                        return guid310;
                    })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> timeHourStrColon = guid309;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid311 = (((bool) (minutes < ((uint8_t) 10))) ? 
                        CharList::safeConcat<1, 1>((([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid312;
                            guid312.data = (juniper::array<uint8_t, 2> { {((uint8_t) 48), ((uint8_t) 0)} });
                            guid312.length = ((uint32_t) 2);
                            return guid312;
                        })()), CharList::i32ToCharList<1>(toInt32<uint8_t>(minutes)))
                    :
                        CharList::i32ToCharList<2>(toInt32<uint8_t>(minutes)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> minutesStr = guid311;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> guid313 = CharList::safeConcat<3, 2>(timeHourStrColon, minutesStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> timeStr = guid313;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid314 = Gfx::getCharListBounds<5>(((int16_t) 0), ((int16_t) 0), timeStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h = (guid314).e4;
                    uint16_t w = (guid314).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2)))))));
                    Gfx::printCharList<5>(timeStr);
                    Gfx::setTextSize(((uint8_t) 1));
                    Gfx::setFont(Gfx::freeSans9());
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid315 = (((bool) (hours < ((uint8_t) 12))) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid316;
                            guid316.data = (juniper::array<uint8_t, 3> { {((uint8_t) 65), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid316.length = ((uint32_t) 3);
                            return guid316;
                        })())
                    :
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid317;
                            guid317.data = (juniper::array<uint8_t, 3> { {((uint8_t) 80), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid317.length = ((uint32_t) 3);
                            return guid317;
                        })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> ampm = guid315;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid318 = Gfx::getCharListBounds<2>(((int16_t) 0), ((int16_t) 0), ampm);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h2 = (guid318).e4;
                    uint16_t w2 = (guid318).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w2 / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2))))) + h2)) + ((uint16_t) 5)))));
                    Gfx::printCharList<2>(ampm);
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> guid319 = CharList::safeConcat<9, 2>(CharList::safeConcat<8, 1>(CharList::safeConcat<5, 3>(CharList::safeConcat<3, 2>(dayOfWeekToCharList(dayOfWeek), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid320;
                        guid320.data = (juniper::array<uint8_t, 3> { {((uint8_t) 44), ((uint8_t) 32), ((uint8_t) 0)} });
                        guid320.length = ((uint32_t) 3);
                        return guid320;
                    })())), monthToCharList(month)), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid321;
                        guid321.data = (juniper::array<uint8_t, 2> { {((uint8_t) 32), ((uint8_t) 0)} });
                        guid321.length = ((uint32_t) 2);
                        return guid321;
                    })())), CharList::i32ToCharList<2>(toInt32<uint8_t>(day)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> dateStr = guid319;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid322 = Gfx::getCharListBounds<11>(((int16_t) 0), ((int16_t) 0), dateStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h3 = (guid322).e4;
                    uint16_t w3 = (guid322).e3;
                    
                    Gfx::setCursor(cast<uint16_t, int16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w3 / ((uint16_t) 2)))))), cast<uint16_t, int16_t>(((uint16_t) (((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) - ((uint16_t) (h / ((uint16_t) 2))))) - ((uint16_t) 5)))));
                    return Gfx::printCharList<11>(dateStr);
                })());
             }), Signal::latch<juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(clockState, Signal::foldP<uint32_t, void, juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(juniper::function<void, juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>(uint32_t,juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>)>([](uint32_t t, juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> dt) -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> { 
                return secondTick(dt);
             }), clockState, Time::every(((uint32_t) 1000), clockTickerState))));
            return Arcada::blitDoubleBuffer();
        })());
    }
}

namespace Gfx {
    juniper::unit fillScreen(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c) {
        return (([&]() -> juniper::unit {
            uint16_t guid323 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid323;
            
            return (([&]() -> juniper::unit {
                arcada.getCanvas()->fillScreen(cPrime);
                return {};
            })());
        })());
    }
}

namespace Gfx {
    juniper::unit drawPixel(int16_t x, int16_t y, juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> c) {
        return (([&]() -> juniper::unit {
            uint16_t guid324 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid324;
            
            return (([&]() -> juniper::unit {
                arcada.getCanvas()->drawPixel(x, y, cPrime);
                return {};
            })());
        })());
    }
}

namespace Gfx {
    juniper::unit drawPixel565(uint16_t x, uint16_t y, uint16_t c) {
        return (([&]() -> juniper::unit {
            arcada.getCanvas()->drawPixel(x, y, c);
            return {};
        })());
    }
}

namespace Gfx {
    juniper::unit printString(const char * s) {
        return (([&]() -> juniper::unit {
            arcada.getCanvas()->print(s);
            return {};
        })());
    }
}

namespace Gfx {
    template<int c176>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c176>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c176;
            return (([&]() -> juniper::unit {
                juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid325 = Gfx::getCharListBounds<(-1)+(c176)>(((int16_t) 0), ((int16_t) 0), cl);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t h = (guid325).e4;
                uint16_t w = (guid325).e3;
                
                int16_t guid326 = toInt16<uint16_t>(w);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t ws = guid326;
                
                int16_t guid327 = toInt16<uint16_t>(h);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t hs = guid327;
                
                return (([&]() -> juniper::unit {
                    Gfx::align guid328 = align;
                    return (((bool) (((bool) ((guid328).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            return Gfx::setCursor(((int16_t) (x - ((int16_t) (ws / ((int16_t) 2))))), y);
                        })())
                    :
                        (((bool) (((bool) ((guid328).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::unit {
                                return Gfx::setCursor(x, ((int16_t) (y - ((int16_t) (hs / ((int16_t) 2))))));
                            })())
                        :
                            (((bool) (((bool) ((guid328).id() == ((uint8_t) 2))) && true)) ? 
                                (([&]() -> juniper::unit {
                                    return Gfx::setCursor(((int16_t) (x - ((int16_t) (ws / ((int16_t) 2))))), ((int16_t) (y - ((int16_t) (hs / ((int16_t) 2))))));
                                })())
                            :
                                juniper::quit<juniper::unit>())));
                })());
            })());
        })());
    }
}

namespace Gfx {
    juniper::unit setTextWrap(bool wrap) {
        return (([&]() -> juniper::unit {
            arcada.getCanvas()->setTextWrap(wrap);
            return {};
        })());
    }
}

void setup() {
    CWatch::setup();
}
void loop() {
    CWatch::loop();
}
