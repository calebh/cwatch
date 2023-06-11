//Compiled on 6/11/2023 6:36:59 PM
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

namespace Arcada {
    using namespace Prelude;

}

namespace Ble {
    using namespace Prelude;

}

namespace Ble {
    using namespace Prelude;

}

namespace CWatch {
    using namespace Prelude;

}

namespace CWatch {
    using namespace Prelude;

}

namespace Gfx {
    using namespace Prelude;

}

namespace Gfx {
    using namespace Prelude;
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

namespace List {
    template<typename t467, int c26, int c27>
    juniper::records::recordt_0<juniper::array<t467, c26>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t467, c27>, uint32_t> lst);
}

namespace List {
    template<typename t473, typename t476, int c30>
    bool all(juniper::function<t473, bool(t476)> pred, juniper::records::recordt_0<juniper::array<t476, c30>, uint32_t> lst);
}

namespace List {
    template<typename t483, typename t486, int c32>
    bool any(juniper::function<t483, bool(t486)> pred, juniper::records::recordt_0<juniper::array<t486, c32>, uint32_t> lst);
}

namespace List {
    template<typename t491, int c34>
    juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> pushBack(t491 elem, juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> lst);
}

namespace List {
    template<typename t506, int c36>
    juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> pushOffFront(t506 elem, juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> lst);
}

namespace List {
    template<typename t522, int c40>
    juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> setNth(uint32_t index, t522 elem, juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> lst);
}

namespace List {
    template<typename t527, int c42>
    juniper::records::recordt_0<juniper::array<t527, c42>, uint32_t> replicate(uint32_t numOfElements, t527 elem);
}

namespace List {
    template<typename t539, int c43>
    juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> remove(t539 elem, juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> lst);
}

namespace List {
    template<typename t549, int c47>
    juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> lst);
}

namespace List {
    template<typename t558, typename t561, int c50>
    juniper::unit foreach(juniper::function<t558, juniper::unit(t561)> f, juniper::records::recordt_0<juniper::array<t561, c50>, uint32_t> lst);
}

namespace List {
    template<typename t576, int c52>
    Prelude::maybe<t576> last(juniper::records::recordt_0<juniper::array<t576, c52>, uint32_t> lst);
}

namespace List {
    template<typename t590, int c54>
    Prelude::maybe<t590> max_(juniper::records::recordt_0<juniper::array<t590, c54>, uint32_t> lst);
}

namespace List {
    template<typename t607, int c58>
    Prelude::maybe<t607> min_(juniper::records::recordt_0<juniper::array<t607, c58>, uint32_t> lst);
}

namespace List {
    template<typename t615, int c62>
    bool member(t615 elem, juniper::records::recordt_0<juniper::array<t615, c62>, uint32_t> lst);
}

namespace Math {
    template<typename t620>
    t620 min_(t620 x, t620 y);
}

namespace List {
    template<typename t632, typename t634, int c64>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t632, c64>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t634, c64>, uint32_t> lstB);
}

namespace List {
    template<typename t645, typename t646, int c68>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t645, t646>, c68>, uint32_t> lst);
}

namespace List {
    template<typename t654, int c72>
    t654 sum(juniper::records::recordt_0<juniper::array<t654, c72>, uint32_t> lst);
}

namespace List {
    template<typename t672, int c74>
    t672 average(juniper::records::recordt_0<juniper::array<t672, c74>, uint32_t> lst);
}

namespace Signal {
    template<typename t679, typename t681, typename t696>
    Prelude::sig<t696> map(juniper::function<t681, t696(t679)> f, Prelude::sig<t679> s);
}

namespace Signal {
    template<typename t704, typename t705>
    juniper::unit sink(juniper::function<t705, juniper::unit(t704)> f, Prelude::sig<t704> s);
}

namespace Signal {
    template<typename t714, typename t728>
    Prelude::sig<t728> filter(juniper::function<t714, bool(t728)> f, Prelude::sig<t728> s);
}

namespace Signal {
    template<typename t735>
    Prelude::sig<t735> merge(Prelude::sig<t735> sigA, Prelude::sig<t735> sigB);
}

namespace Signal {
    template<typename t739, int c76>
    Prelude::sig<t739> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t739>, c76>, uint32_t> sigs);
}

namespace Signal {
    template<typename t762, typename t786>
    Prelude::sig<Prelude::either<t786, t762>> join(Prelude::sig<t786> sigA, Prelude::sig<t762> sigB);
}

namespace Signal {
    template<typename t809>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t809> s);
}

namespace Signal {
    template<typename t816, typename t818, typename t836>
    Prelude::sig<t836> foldP(juniper::function<t818, t836(t816, t836)> f, juniper::shared_ptr<t836> state0, Prelude::sig<t816> incoming);
}

namespace Signal {
    template<typename t846>
    Prelude::sig<t846> dropRepeats(juniper::shared_ptr<Prelude::maybe<t846>> maybePrevValue, Prelude::sig<t846> incoming);
}

namespace Signal {
    template<typename t864>
    Prelude::sig<t864> latch(juniper::shared_ptr<t864> prevValue, Prelude::sig<t864> incoming);
}

namespace Signal {
    template<typename t877, typename t878, typename t881, typename t890>
    Prelude::sig<t877> map2(juniper::function<t878, t877(t881, t890)> f, juniper::shared_ptr<juniper::tuple2<t881, t890>> state, Prelude::sig<t881> incomingA, Prelude::sig<t890> incomingB);
}

namespace Signal {
    template<typename t922, int c78>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t922, c78>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t922, c78>, uint32_t>> pastValues, Prelude::sig<t922> incoming);
}

namespace Signal {
    template<typename t933>
    Prelude::sig<t933> constant(t933 val);
}

namespace Signal {
    template<typename t943>
    Prelude::sig<Prelude::maybe<t943>> meta(Prelude::sig<t943> sigA);
}

namespace Signal {
    template<typename t960>
    Prelude::sig<t960> unmeta(Prelude::sig<Prelude::maybe<t960>> sigA);
}

namespace Signal {
    template<typename t973, typename t974>
    Prelude::sig<juniper::tuple2<t973, t974>> zip(juniper::shared_ptr<juniper::tuple2<t973, t974>> state, Prelude::sig<t973> sigA, Prelude::sig<t974> sigB);
}

namespace Signal {
    template<typename t1005, typename t1012>
    juniper::tuple2<Prelude::sig<t1005>, Prelude::sig<t1012>> unzip(Prelude::sig<juniper::tuple2<t1005, t1012>> incoming);
}

namespace Signal {
    template<typename t1019, typename t1020>
    Prelude::sig<t1019> toggle(t1019 val1, t1019 val2, juniper::shared_ptr<t1019> state, Prelude::sig<t1020> incoming);
}

namespace Io {
    Io::pinState toggle(Io::pinState p);
}

namespace Io {
    juniper::unit printStr(const char * str);
}

namespace Io {
    template<int c80>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c80>, uint32_t> cl);
}

namespace Io {
    juniper::unit printFloat(float f);
}

namespace Io {
    juniper::unit printInt(int32_t n);
}

namespace Io {
    template<typename t1048>
    t1048 baseToInt(Io::base b);
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
    template<typename t1180, typename t1182, typename t1191>
    Prelude::maybe<t1191> map(juniper::function<t1182, t1191(t1180)> f, Prelude::maybe<t1180> maybeVal);
}

namespace Maybe {
    template<typename t1195>
    t1195 get(Prelude::maybe<t1195> maybeVal);
}

namespace Maybe {
    template<typename t1198>
    bool isJust(Prelude::maybe<t1198> maybeVal);
}

namespace Maybe {
    template<typename t1201>
    bool isNothing(Prelude::maybe<t1201> maybeVal);
}

namespace Maybe {
    template<typename t1207>
    uint8_t count(Prelude::maybe<t1207> maybeVal);
}

namespace Maybe {
    template<typename t1213, typename t1214, typename t1215>
    t1213 foldl(juniper::function<t1215, t1213(t1214, t1213)> f, t1213 initState, Prelude::maybe<t1214> maybeVal);
}

namespace Maybe {
    template<typename t1223, typename t1224, typename t1225>
    t1223 fodlr(juniper::function<t1225, t1223(t1224, t1223)> f, t1223 initState, Prelude::maybe<t1224> maybeVal);
}

namespace Maybe {
    template<typename t1236, typename t1237>
    juniper::unit iter(juniper::function<t1237, juniper::unit(t1236)> f, Prelude::maybe<t1236> maybeVal);
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
    template<typename t1304>
    t1304 max_(t1304 x, t1304 y);
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2);
}

namespace Math {
    template<typename t1307>
    t1307 clamp(t1307 x, t1307 min, t1307 max);
}

namespace Math {
    template<typename t1312>
    int8_t sign(t1312 n);
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
    template<typename t1360, int c81>
    juniper::array<t1360, c81> add(juniper::array<t1360, c81> v1, juniper::array<t1360, c81> v2);
}

namespace Vector {
    template<typename t1362, int c84>
    juniper::array<t1362, c84> zero();
}

namespace Vector {
    template<typename t1368, int c85>
    juniper::array<t1368, c85> subtract(juniper::array<t1368, c85> v1, juniper::array<t1368, c85> v2);
}

namespace Vector {
    template<typename t1372, int c88>
    juniper::array<t1372, c88> scale(t1372 scalar, juniper::array<t1372, c88> v);
}

namespace Vector {
    template<typename t1378, int c90>
    t1378 dot(juniper::array<t1378, c90> v1, juniper::array<t1378, c90> v2);
}

namespace Vector {
    template<typename t1384, int c93>
    t1384 magnitude2(juniper::array<t1384, c93> v);
}

namespace Vector {
    template<typename t1386, int c96>
    double magnitude(juniper::array<t1386, c96> v);
}

namespace Vector {
    template<typename t1402, int c98>
    juniper::array<t1402, c98> multiply(juniper::array<t1402, c98> u, juniper::array<t1402, c98> v);
}

namespace Vector {
    template<typename t1411, int c101>
    juniper::array<t1411, c101> normalize(juniper::array<t1411, c101> v);
}

namespace Vector {
    template<typename t1422, int c105>
    double angle(juniper::array<t1422, c105> v1, juniper::array<t1422, c105> v2);
}

namespace Vector {
    template<typename t1464>
    juniper::array<t1464, 3> cross(juniper::array<t1464, 3> u, juniper::array<t1464, 3> v);
}

namespace Vector {
    template<typename t1466, int c121>
    juniper::array<t1466, c121> project(juniper::array<t1466, c121> a, juniper::array<t1466, c121> b);
}

namespace Vector {
    template<typename t1482, int c125>
    juniper::array<t1482, c125> projectPlane(juniper::array<t1482, c125> a, juniper::array<t1482, c125> m);
}

namespace CharList {
    template<int c128>
    juniper::records::recordt_0<juniper::array<uint8_t, c128>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c128>, uint32_t> str);
}

namespace CharList {
    template<int c129>
    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> str);
}

namespace CharList {
    template<int c130>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t> i32ToCharList(int32_t m);
}

namespace CharList {
    template<int c131>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c131>, uint32_t> s);
}

namespace CharList {
    template<int c132, int c133, int c134>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c132)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c133)>, uint32_t> sB);
}

namespace CharList {
    template<int c141, int c142>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c141)*(-1)))+((c142)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c141)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c142)>, uint32_t> sB);
}

namespace Random {
    int32_t random_(int32_t low, int32_t high);
}

namespace Random {
    juniper::unit seed(uint32_t n);
}

namespace Random {
    template<typename t1552, int c146>
    t1552 choice(juniper::records::recordt_0<juniper::array<t1552, c146>, uint32_t> lst);
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
    template<int c148>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c148>, uint32_t> bytes);
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
    template<typename t1906>
    uint16_t writeGeneric(Ble::characterstict c, t1906 x);
}

namespace Ble {
    template<typename t1911>
    t1911 readGeneric(Ble::characterstict c);
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
    template<int c149>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c149)>, uint32_t> cl);
}

namespace Gfx {
    template<int c150>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c150)>, uint32_t> cl);
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
    template<int c175>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c175>, uint32_t> cl);
}

namespace Gfx {
    juniper::unit setTextWrap(bool wrap);
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

namespace List {
    template<typename t467, int c26, int c27>
    juniper::records::recordt_0<juniper::array<t467, c26>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t467, c27>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t467, c26>, uint32_t> {
            using t = t467;
            constexpr int32_t m = c26;
            constexpr int32_t n = c27;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t467, c26>, uint32_t> {
                juniper::array<t467, c26> guid30 = (juniper::array<t467, c26>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t467, c26> ret = guid30;
                
                (([&]() -> juniper::unit {
                    uint32_t guid31 = ((uint32_t) 0);
                    uint32_t guid32 = (lst).length;
                    for (uint32_t i = guid31; i < guid32; i++) {
                        (([&]() -> t467 {
                            return ((ret)[i] = ((lst).data)[i]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t467, c26>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t467, c26>, uint32_t> guid33;
                    guid33.data = ret;
                    guid33.length = (lst).length;
                    return guid33;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t473, typename t476, int c30>
    bool all(juniper::function<t473, bool(t476)> pred, juniper::records::recordt_0<juniper::array<t476, c30>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t476;
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
    template<typename t483, typename t486, int c32>
    bool any(juniper::function<t483, bool(t486)> pred, juniper::records::recordt_0<juniper::array<t486, c32>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t486;
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
    template<typename t491, int c34>
    juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> pushBack(t491 elem, juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> {
            using t = t491;
            constexpr int32_t n = c34;
            return (((bool) ((lst).length >= n)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> {
                    return lst;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> guid40 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t491, c34>, uint32_t> ret = guid40;
                    
                    (((ret).data)[(lst).length] = elem);
                    ((ret).length = ((uint32_t) ((lst).length + ((uint32_t) 1))));
                    return ret;
                })()));
        })());
    }
}

namespace List {
    template<typename t506, int c36>
    juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> pushOffFront(t506 elem, juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> {
            using t = t506;
            constexpr int32_t n = c36;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> {
                        return lst;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t506, c36>, uint32_t> ret;
                        
                        (((ret).data)[((uint32_t) 0)] = elem);
                        (([&]() -> juniper::unit {
                            uint32_t guid41 = ((uint32_t) 0);
                            uint32_t guid42 = (lst).length;
                            for (uint32_t i = guid41; i < guid42; i++) {
                                (([&]() -> juniper::unit {
                                    return (([&]() -> juniper::unit {
                                        if (((bool) (((uint32_t) (i + ((uint32_t) 1))) < n))) {
                                            (([&]() -> t506 {
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
                                return ((ret).length = ((uint32_t) ((lst).length + ((uint32_t) 1))));
                            })()));
                        return ret;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t522, int c40>
    juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> setNth(uint32_t index, t522 elem, juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> {
            using t = t522;
            constexpr int32_t n = c40;
            return (((bool) (index < (lst).length)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> guid43 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> ret = guid43;
                    
                    (((ret).data)[index] = elem);
                    return ret;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t522, c40>, uint32_t> {
                    return lst;
                })()));
        })());
    }
}

namespace List {
    template<typename t527, int c42>
    juniper::records::recordt_0<juniper::array<t527, c42>, uint32_t> replicate(uint32_t numOfElements, t527 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t527, c42>, uint32_t> {
            using t = t527;
            constexpr int32_t n = c42;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t527, c42>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t527, c42>, uint32_t> guid44;
                guid44.data = (juniper::array<t527, c42>().fill(elem));
                guid44.length = numOfElements;
                return guid44;
            })());
        })());
    }
}

namespace List {
    template<typename t539, int c43>
    juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> remove(t539 elem, juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> {
            using t = t539;
            constexpr int32_t n = c43;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> {
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
                    (([&]() -> juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> guid49 = lst;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> ret = guid49;
                        
                        ((ret).length = ((uint32_t) ((lst).length - ((uint32_t) 1))));
                        (([&]() -> juniper::unit {
                            uint32_t guid50 = index;
                            uint32_t guid51 = ((uint32_t) ((lst).length - ((uint32_t) 1)));
                            for (uint32_t i = guid50; i < guid51; i++) {
                                (([&]() -> t539 {
                                    return (((ret).data)[i] = ((lst).data)[((uint32_t) (i + ((uint32_t) 1)))]);
                                })());
                            }
                            return {};
                        })());
                        return ret;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t539, c43>, uint32_t> {
                        return lst;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t549, int c47>
    juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> {
            using t = t549;
            constexpr int32_t n = c47;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> {
                    return lst;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t549, c47>, uint32_t> ret;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid52 = ((uint32_t) 0);
                        uint32_t guid53 = ((uint32_t) ((lst).length - ((uint32_t) 1)));
                        for (uint32_t i = guid52; i < guid53; i++) {
                            (([&]() -> t549 {
                                return (((ret).data)[i] = ((lst).data)[i]);
                            })());
                        }
                        return {};
                    })());
                    ((ret).length = ((uint32_t) ((lst).length - ((uint32_t) 1))));
                    return lst;
                })()));
        })());
    }
}

namespace List {
    template<typename t558, typename t561, int c50>
    juniper::unit foreach(juniper::function<t558, juniper::unit(t561)> f, juniper::records::recordt_0<juniper::array<t561, c50>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            using t = t561;
            constexpr int32_t n = c50;
            return (([&]() -> juniper::unit {
                uint32_t guid54 = ((uint32_t) 0);
                uint32_t guid55 = (lst).length;
                for (uint32_t i = guid54; i < guid55; i++) {
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
    template<typename t576, int c52>
    Prelude::maybe<t576> last(juniper::records::recordt_0<juniper::array<t576, c52>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t576> {
            using t = t576;
            constexpr int32_t n = c52;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t576> {
                    return nothing<t576>();
                })())
            :
                (([&]() -> Prelude::maybe<t576> {
                    return just<t576>(((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))]);
                })()));
        })());
    }
}

namespace List {
    template<typename t590, int c54>
    Prelude::maybe<t590> max_(juniper::records::recordt_0<juniper::array<t590, c54>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t590> {
            using t = t590;
            constexpr int32_t n = c54;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t590> {
                    return nothing<t590>();
                })())
            :
                (([&]() -> Prelude::maybe<t590> {
                    t590 guid56 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t590 maxVal = guid56;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid57 = ((uint32_t) 1);
                        uint32_t guid58 = (lst).length;
                        for (uint32_t i = guid57; i < guid58; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] > maxVal))) {
                                        (([&]() -> t590 {
                                            return (maxVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t590>(maxVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t607, int c58>
    Prelude::maybe<t607> min_(juniper::records::recordt_0<juniper::array<t607, c58>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t607> {
            using t = t607;
            constexpr int32_t n = c58;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t607> {
                    return nothing<t607>();
                })())
            :
                (([&]() -> Prelude::maybe<t607> {
                    t607 guid59 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t607 minVal = guid59;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid60 = ((uint32_t) 1);
                        uint32_t guid61 = (lst).length;
                        for (uint32_t i = guid60; i < guid61; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] < minVal))) {
                                        (([&]() -> t607 {
                                            return (minVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t607>(minVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t615, int c62>
    bool member(t615 elem, juniper::records::recordt_0<juniper::array<t615, c62>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t615;
            constexpr int32_t n = c62;
            return (([&]() -> bool {
                bool guid62 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid62;
                
                (([&]() -> juniper::unit {
                    uint32_t guid63 = ((uint32_t) 0);
                    uint32_t guid64 = (lst).length;
                    for (uint32_t i = guid63; i < guid64; i++) {
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

namespace Math {
    template<typename t620>
    t620 min_(t620 x, t620 y) {
        return (([&]() -> t620 {
            using a = t620;
            return (((bool) (x > y)) ? 
                (([&]() -> t620 {
                    return y;
                })())
            :
                (([&]() -> t620 {
                    return x;
                })()));
        })());
    }
}

namespace List {
    template<typename t632, typename t634, int c64>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t632, c64>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t634, c64>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t> {
            using a = t632;
            using b = t634;
            constexpr int32_t n = c64;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t> {
                uint32_t guid65 = Math::min_<uint32_t>((lstA).length, (lstB).length);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t outLen = guid65;
                
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t> guid66 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t> guid67;
                    guid67.data = (juniper::array<juniper::tuple2<t632, t634>, c64>());
                    guid67.length = outLen;
                    return guid67;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t632, t634>, c64>, uint32_t> ret = guid66;
                
                (([&]() -> juniper::unit {
                    uint32_t guid68 = ((uint32_t) 0);
                    uint32_t guid69 = outLen;
                    for (uint32_t i = guid68; i < guid69; i++) {
                        (([&]() -> juniper::tuple2<t632, t634> {
                            return (((ret).data)[i] = (juniper::tuple2<t632,t634>{((lstA).data)[i], ((lstB).data)[i]}));
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
    template<typename t645, typename t646, int c68>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t645, t646>, c68>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t>> {
            using a = t645;
            using b = t646;
            constexpr int32_t n = c68;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t>, juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t> guid70 = (([&]() -> juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t> guid71;
                    guid71.data = (juniper::array<t645, c68>());
                    guid71.length = (lst).length;
                    return guid71;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t> retA = guid70;
                
                juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t> guid72 = (([&]() -> juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t> guid73;
                    guid73.data = (juniper::array<t646, c68>());
                    guid73.length = (lst).length;
                    return guid73;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t> retB = guid72;
                
                (([&]() -> juniper::unit {
                    uint32_t guid74 = ((uint32_t) 0);
                    uint32_t guid75 = (lst).length;
                    for (uint32_t i = guid74; i < guid75; i++) {
                        (([&]() -> t646 {
                            juniper::tuple2<t645, t646> guid76 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t646 b = (guid76).e2;
                            t645 a = (guid76).e1;
                            
                            (((retA).data)[i] = a);
                            return (((retB).data)[i] = b);
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t645, c68>, uint32_t>,juniper::records::recordt_0<juniper::array<t646, c68>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t654, int c72>
    t654 sum(juniper::records::recordt_0<juniper::array<t654, c72>, uint32_t> lst) {
        return (([&]() -> t654 {
            using a = t654;
            constexpr int32_t n = c72;
            return List::foldl<t654, void, t654, c72>(Prelude::add<t654>, ((t654) 0), lst);
        })());
    }
}

namespace List {
    template<typename t672, int c74>
    t672 average(juniper::records::recordt_0<juniper::array<t672, c74>, uint32_t> lst) {
        return (([&]() -> t672 {
            using a = t672;
            constexpr int32_t n = c74;
            return ((t672) (sum<t672, c74>(lst) / cast<uint32_t, t672>((lst).length)));
        })());
    }
}

namespace Signal {
    template<typename t679, typename t681, typename t696>
    Prelude::sig<t696> map(juniper::function<t681, t696(t679)> f, Prelude::sig<t679> s) {
        return (([&]() -> Prelude::sig<t696> {
            using a = t679;
            using b = t696;
            return (([&]() -> Prelude::sig<t696> {
                Prelude::sig<t679> guid77 = s;
                return (((bool) (((bool) ((guid77).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid77).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t696> {
                        t679 val = ((guid77).signal()).just();
                        return signal<t696>(just<t696>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t696> {
                            return signal<t696>(nothing<t696>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t696>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t704, typename t705>
    juniper::unit sink(juniper::function<t705, juniper::unit(t704)> f, Prelude::sig<t704> s) {
        return (([&]() -> juniper::unit {
            using a = t704;
            return (([&]() -> juniper::unit {
                Prelude::sig<t704> guid78 = s;
                return (((bool) (((bool) ((guid78).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid78).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::unit {
                        t704 val = ((guid78).signal()).just();
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
    template<typename t714, typename t728>
    Prelude::sig<t728> filter(juniper::function<t714, bool(t728)> f, Prelude::sig<t728> s) {
        return (([&]() -> Prelude::sig<t728> {
            using a = t728;
            return (([&]() -> Prelude::sig<t728> {
                Prelude::sig<t728> guid79 = s;
                return (((bool) (((bool) ((guid79).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid79).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t728> {
                        t728 val = ((guid79).signal()).just();
                        return (f(val) ? 
                            (([&]() -> Prelude::sig<t728> {
                                return signal<t728>(nothing<t728>());
                            })())
                        :
                            (([&]() -> Prelude::sig<t728> {
                                return s;
                            })()));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t728> {
                            return signal<t728>(nothing<t728>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t728>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t735>
    Prelude::sig<t735> merge(Prelude::sig<t735> sigA, Prelude::sig<t735> sigB) {
        return (([&]() -> Prelude::sig<t735> {
            using a = t735;
            return (([&]() -> Prelude::sig<t735> {
                Prelude::sig<t735> guid80 = sigA;
                return (((bool) (((bool) ((guid80).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid80).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t735> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t735> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t735>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t739, int c76>
    Prelude::sig<t739> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t739>, c76>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t739> {
            using a = t739;
            constexpr int32_t n = c76;
            return (([&]() -> Prelude::sig<t739> {
                Prelude::maybe<t739> guid81 = List::foldl<Prelude::maybe<t739>, void, Prelude::sig<t739>, c76>(juniper::function<void, Prelude::maybe<t739>(Prelude::sig<t739>,Prelude::maybe<t739>)>([](Prelude::sig<t739> sig, Prelude::maybe<t739> accum) -> Prelude::maybe<t739> { 
                    return (([&]() -> Prelude::maybe<t739> {
                        Prelude::maybe<t739> guid82 = accum;
                        return (((bool) (((bool) ((guid82).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::maybe<t739> {
                                return (([&]() -> Prelude::maybe<t739> {
                                    Prelude::sig<t739> guid83 = sig;
                                    if (!(((bool) (((bool) ((guid83).id() == ((uint8_t) 0))) && true)))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t739> heldValue = (guid83).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t739> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t739>>()));
                    })());
                 }), nothing<t739>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t739> ret = guid81;
                
                return signal<t739>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t762, typename t786>
    Prelude::sig<Prelude::either<t786, t762>> join(Prelude::sig<t786> sigA, Prelude::sig<t762> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t786, t762>> {
            using a = t786;
            using b = t762;
            return (([&]() -> Prelude::sig<Prelude::either<t786, t762>> {
                juniper::tuple2<Prelude::sig<t786>, Prelude::sig<t762>> guid84 = (juniper::tuple2<Prelude::sig<t786>,Prelude::sig<t762>>{sigA, sigB});
                return (((bool) (((bool) (((guid84).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid84).e1).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t786, t762>> {
                        t786 value = (((guid84).e1).signal()).just();
                        return signal<Prelude::either<t786, t762>>(just<Prelude::either<t786, t762>>(left<t786, t762>(value)));
                    })())
                :
                    (((bool) (((bool) (((guid84).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid84).e2).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t786, t762>> {
                            t762 value = (((guid84).e2).signal()).just();
                            return signal<Prelude::either<t786, t762>>(just<Prelude::either<t786, t762>>(right<t786, t762>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t786, t762>> {
                                return signal<Prelude::either<t786, t762>>(nothing<Prelude::either<t786, t762>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t786, t762>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t809>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t809> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t809;
            return map<t809, void, juniper::unit>(juniper::function<void, juniper::unit(t809)>([](t809 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t816, typename t818, typename t836>
    Prelude::sig<t836> foldP(juniper::function<t818, t836(t816, t836)> f, juniper::shared_ptr<t836> state0, Prelude::sig<t816> incoming) {
        return (([&]() -> Prelude::sig<t836> {
            using a = t816;
            using state = t836;
            return (([&]() -> Prelude::sig<t836> {
                Prelude::sig<t816> guid85 = incoming;
                return (((bool) (((bool) ((guid85).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid85).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t836> {
                        t816 val = ((guid85).signal()).just();
                        return (([&]() -> Prelude::sig<t836> {
                            t836 guid86 = f(val, (*((state0).get())));
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t836 state1 = guid86;
                            
                            (*((state0).get()) = state1);
                            return signal<t836>(just<t836>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t836> {
                            return signal<t836>(nothing<t836>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t836>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t846>
    Prelude::sig<t846> dropRepeats(juniper::shared_ptr<Prelude::maybe<t846>> maybePrevValue, Prelude::sig<t846> incoming) {
        return (([&]() -> Prelude::sig<t846> {
            using a = t846;
            return filter<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t846>>>, t846>(juniper::function<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t846>>>, bool(t846)>(juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t846>>>(maybePrevValue), [](juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t846>>>& junclosure, t846 value) -> bool { 
                juniper::shared_ptr<Prelude::maybe<t846>>& maybePrevValue = junclosure.maybePrevValue;
                return (([&]() -> bool {
                    bool guid87 = (([&]() -> bool {
                        Prelude::maybe<t846> guid88 = (*((maybePrevValue).get()));
                        return (((bool) (((bool) ((guid88).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> bool {
                                return false;
                            })())
                        :
                            (((bool) (((bool) ((guid88).id() == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> bool {
                                    t846 prevValue = (guid88).just();
                                    return ((bool) (value == prevValue));
                                })())
                            :
                                juniper::quit<bool>()));
                    })());
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    bool filtered = guid87;
                    
                    (([&]() -> juniper::unit {
                        if (!(filtered)) {
                            (([&]() -> Prelude::maybe<t846> {
                                return (*((maybePrevValue).get()) = just<t846>(value));
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
    template<typename t864>
    Prelude::sig<t864> latch(juniper::shared_ptr<t864> prevValue, Prelude::sig<t864> incoming) {
        return (([&]() -> Prelude::sig<t864> {
            using a = t864;
            return (([&]() -> Prelude::sig<t864> {
                Prelude::sig<t864> guid89 = incoming;
                return (((bool) (((bool) ((guid89).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid89).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t864> {
                        t864 val = ((guid89).signal()).just();
                        return (([&]() -> Prelude::sig<t864> {
                            (*((prevValue).get()) = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t864> {
                            return signal<t864>(just<t864>((*((prevValue).get()))));
                        })())
                    :
                        juniper::quit<Prelude::sig<t864>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t877, typename t878, typename t881, typename t890>
    Prelude::sig<t877> map2(juniper::function<t878, t877(t881, t890)> f, juniper::shared_ptr<juniper::tuple2<t881, t890>> state, Prelude::sig<t881> incomingA, Prelude::sig<t890> incomingB) {
        return (([&]() -> Prelude::sig<t877> {
            using a = t881;
            using b = t890;
            using c = t877;
            return (([&]() -> Prelude::sig<t877> {
                t881 guid90 = (([&]() -> t881 {
                    Prelude::sig<t881> guid91 = incomingA;
                    return (((bool) (((bool) ((guid91).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid91).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t881 {
                            t881 val1 = ((guid91).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t881 {
                                return fst<t881, t890>((*((state).get())));
                            })())
                        :
                            juniper::quit<t881>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t881 valA = guid90;
                
                t890 guid92 = (([&]() -> t890 {
                    Prelude::sig<t890> guid93 = incomingB;
                    return (((bool) (((bool) ((guid93).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid93).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t890 {
                            t890 val2 = ((guid93).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t890 {
                                return snd<t881, t890>((*((state).get())));
                            })())
                        :
                            juniper::quit<t890>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t890 valB = guid92;
                
                (*((state).get()) = (juniper::tuple2<t881,t890>{valA, valB}));
                return (([&]() -> Prelude::sig<t877> {
                    juniper::tuple2<Prelude::sig<t881>, Prelude::sig<t890>> guid94 = (juniper::tuple2<Prelude::sig<t881>,Prelude::sig<t890>>{incomingA, incomingB});
                    return (((bool) (((bool) (((guid94).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid94).e2).signal()).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid94).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid94).e1).signal()).id() == ((uint8_t) 1))) && true)))))))) ? 
                        (([&]() -> Prelude::sig<t877> {
                            return signal<t877>(nothing<t877>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t877> {
                                return signal<t877>(just<t877>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t877>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t922, int c78>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t922, c78>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t922, c78>, uint32_t>> pastValues, Prelude::sig<t922> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t922, c78>, uint32_t>> {
            using a = t922;
            constexpr int32_t n = c78;
            return foldP<t922, void, juniper::records::recordt_0<juniper::array<t922, c78>, uint32_t>>(List::pushOffFront<t922, c78>, pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t933>
    Prelude::sig<t933> constant(t933 val) {
        return (([&]() -> Prelude::sig<t933> {
            using a = t933;
            return signal<t933>(just<t933>(val));
        })());
    }
}

namespace Signal {
    template<typename t943>
    Prelude::sig<Prelude::maybe<t943>> meta(Prelude::sig<t943> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t943>> {
            using a = t943;
            return (([&]() -> Prelude::sig<Prelude::maybe<t943>> {
                Prelude::sig<t943> guid95 = sigA;
                if (!(((bool) (((bool) ((guid95).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t943> val = (guid95).signal();
                
                return constant<Prelude::maybe<t943>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t960>
    Prelude::sig<t960> unmeta(Prelude::sig<Prelude::maybe<t960>> sigA) {
        return (([&]() -> Prelude::sig<t960> {
            using a = t960;
            return (([&]() -> Prelude::sig<t960> {
                Prelude::sig<Prelude::maybe<t960>> guid96 = sigA;
                return (((bool) (((bool) ((guid96).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid96).signal()).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid96).signal()).just()).id() == ((uint8_t) 0))) && true)))))) ? 
                    (([&]() -> Prelude::sig<t960> {
                        t960 val = (((guid96).signal()).just()).just();
                        return constant<t960>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t960> {
                            return signal<t960>(nothing<t960>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t960>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t973, typename t974>
    Prelude::sig<juniper::tuple2<t973, t974>> zip(juniper::shared_ptr<juniper::tuple2<t973, t974>> state, Prelude::sig<t973> sigA, Prelude::sig<t974> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t973, t974>> {
            using a = t973;
            using b = t974;
            return map2<juniper::tuple2<t973, t974>, void, t973, t974>(juniper::function<void, juniper::tuple2<t973, t974>(t973,t974)>([](t973 valA, t974 valB) -> juniper::tuple2<t973, t974> { 
                return (juniper::tuple2<t973,t974>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t1005, typename t1012>
    juniper::tuple2<Prelude::sig<t1005>, Prelude::sig<t1012>> unzip(Prelude::sig<juniper::tuple2<t1005, t1012>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t1005>, Prelude::sig<t1012>> {
            using a = t1005;
            using b = t1012;
            return (([&]() -> juniper::tuple2<Prelude::sig<t1005>, Prelude::sig<t1012>> {
                Prelude::sig<juniper::tuple2<t1005, t1012>> guid97 = incoming;
                return (((bool) (((bool) ((guid97).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid97).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t1005>, Prelude::sig<t1012>> {
                        t1012 y = (((guid97).signal()).just()).e2;
                        t1005 x = (((guid97).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t1005>,Prelude::sig<t1012>>{signal<t1005>(just<t1005>(x)), signal<t1012>(just<t1012>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t1005>, Prelude::sig<t1012>> {
                            return (juniper::tuple2<Prelude::sig<t1005>,Prelude::sig<t1012>>{signal<t1005>(nothing<t1005>()), signal<t1012>(nothing<t1012>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t1005>, Prelude::sig<t1012>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1019, typename t1020>
    Prelude::sig<t1019> toggle(t1019 val1, t1019 val2, juniper::shared_ptr<t1019> state, Prelude::sig<t1020> incoming) {
        return (([&]() -> Prelude::sig<t1019> {
            using a = t1019;
            using b = t1020;
            return foldP<t1020, juniper::closures::closuret_5<t1019, t1019>, t1019>(juniper::function<juniper::closures::closuret_5<t1019, t1019>, t1019(t1020,t1019)>(juniper::closures::closuret_5<t1019, t1019>(val1, val2), [](juniper::closures::closuret_5<t1019, t1019>& junclosure, t1020 event, t1019 prevVal) -> t1019 { 
                t1019& val1 = junclosure.val1;
                t1019& val2 = junclosure.val2;
                return (((bool) (prevVal == val1)) ? 
                    (([&]() -> t1019 {
                        return val2;
                    })())
                :
                    (([&]() -> t1019 {
                        return val1;
                    })()));
             }), state, incoming);
        })());
    }
}

namespace Io {
    Io::pinState toggle(Io::pinState p) {
        return (([&]() -> Io::pinState {
            Io::pinState guid98 = p;
            return (((bool) (((bool) ((guid98).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> Io::pinState {
                    return low();
                })())
            :
                (((bool) (((bool) ((guid98).id() == ((uint8_t) 1))) && true)) ? 
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
    template<int c80>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c80>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c80;
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
    template<typename t1048>
    t1048 baseToInt(Io::base b) {
        return (([&]() -> t1048 {
            Io::base guid99 = b;
            return (((bool) (((bool) ((guid99).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> t1048 {
                    return ((t1048) 2);
                })())
            :
                (((bool) (((bool) ((guid99).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> t1048 {
                        return ((t1048) 8);
                    })())
                :
                    (((bool) (((bool) ((guid99).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> t1048 {
                            return ((t1048) 10);
                        })())
                    :
                        (((bool) (((bool) ((guid99).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> t1048 {
                                return ((t1048) 16);
                            })())
                        :
                            juniper::quit<t1048>()))));
        })());
    }
}

namespace Io {
    juniper::unit printIntBase(int32_t n, Io::base b) {
        return (([&]() -> juniper::unit {
            int32_t guid100 = baseToInt<int32_t>(b);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t bint = guid100;
            
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
            Io::pinState guid101 = value;
            return (((bool) (((bool) ((guid101).id() == ((uint8_t) 1))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid101).id() == ((uint8_t) 0))) && true)) ? 
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
            uint8_t guid102 = pinStateToInt(value);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t intVal = guid102;
            
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
            Io::mode guid103 = m;
            return (((bool) (((bool) ((guid103).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid103).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    (((bool) (((bool) ((guid103).id() == ((uint8_t) 2))) && true)) ? 
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
            uint8_t guid104 = m;
            return (((bool) (((bool) (guid104 == ((int32_t) 0))) && true)) ? 
                (([&]() -> Io::mode {
                    return input();
                })())
            :
                (((bool) (((bool) (guid104 == ((int32_t) 1))) && true)) ? 
                    (([&]() -> Io::mode {
                        return output();
                    })())
                :
                    (((bool) (((bool) (guid104 == ((int32_t) 2))) && true)) ? 
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
            uint8_t guid105 = pinModeToInt(m);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t m2 = guid105;
            
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
                bool guid106 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState, Io::pinState> guid107 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid107).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid107).e1).id() == ((uint8_t) 0))) && true)))) ? 
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
                bool ret = guid106;
                
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
                bool guid108 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState, Io::pinState> guid109 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid109).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid109).e1).id() == ((uint8_t) 1))) && true)))) ? 
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
                bool ret = guid108;
                
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
                bool guid110 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState, Io::pinState> guid111 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid111).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid111).e1).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        (((bool) (((bool) (((guid111).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid111).e1).id() == ((uint8_t) 1))) && true)))) ? 
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
                bool ret = guid110;
                
                (*((prevState).get()) = currState);
                return ret;
            })());
         }), sig);
    }
}

namespace Maybe {
    template<typename t1180, typename t1182, typename t1191>
    Prelude::maybe<t1191> map(juniper::function<t1182, t1191(t1180)> f, Prelude::maybe<t1180> maybeVal) {
        return (([&]() -> Prelude::maybe<t1191> {
            using a = t1180;
            using b = t1191;
            return (([&]() -> Prelude::maybe<t1191> {
                Prelude::maybe<t1180> guid112 = maybeVal;
                return (((bool) (((bool) ((guid112).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t1191> {
                        t1180 val = (guid112).just();
                        return just<t1191>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t1191> {
                            return nothing<t1191>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t1191>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1195>
    t1195 get(Prelude::maybe<t1195> maybeVal) {
        return (([&]() -> t1195 {
            using a = t1195;
            return (([&]() -> t1195 {
                Prelude::maybe<t1195> guid113 = maybeVal;
                return (((bool) (((bool) ((guid113).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t1195 {
                        t1195 val = (guid113).just();
                        return val;
                    })())
                :
                    juniper::quit<t1195>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1198>
    bool isJust(Prelude::maybe<t1198> maybeVal) {
        return (([&]() -> bool {
            using a = t1198;
            return (([&]() -> bool {
                Prelude::maybe<t1198> guid114 = maybeVal;
                return (((bool) (((bool) ((guid114).id() == ((uint8_t) 0))) && true)) ? 
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
    template<typename t1201>
    bool isNothing(Prelude::maybe<t1201> maybeVal) {
        return (([&]() -> bool {
            using a = t1201;
            return !(isJust<t1201>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t1207>
    uint8_t count(Prelude::maybe<t1207> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t1207;
            return (([&]() -> uint8_t {
                Prelude::maybe<t1207> guid115 = maybeVal;
                return (((bool) (((bool) ((guid115).id() == ((uint8_t) 0))) && true)) ? 
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
    template<typename t1213, typename t1214, typename t1215>
    t1213 foldl(juniper::function<t1215, t1213(t1214, t1213)> f, t1213 initState, Prelude::maybe<t1214> maybeVal) {
        return (([&]() -> t1213 {
            using state = t1213;
            using t = t1214;
            return (([&]() -> t1213 {
                Prelude::maybe<t1214> guid116 = maybeVal;
                return (((bool) (((bool) ((guid116).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t1213 {
                        t1214 val = (guid116).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t1213 {
                            return initState;
                        })())
                    :
                        juniper::quit<t1213>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1223, typename t1224, typename t1225>
    t1223 fodlr(juniper::function<t1225, t1223(t1224, t1223)> f, t1223 initState, Prelude::maybe<t1224> maybeVal) {
        return (([&]() -> t1223 {
            using state = t1223;
            using t = t1224;
            return foldl<t1223, t1224, t1225>(f, initState, maybeVal);
        })());
    }
}

namespace Maybe {
    template<typename t1236, typename t1237>
    juniper::unit iter(juniper::function<t1237, juniper::unit(t1236)> f, Prelude::maybe<t1236> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t1236;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t1236> guid117 = maybeVal;
                return (((bool) (((bool) ((guid117).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> juniper::unit {
                        t1236 val = (guid117).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t1236> nothing = guid117;
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
            uint32_t guid118 = ((uint32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t ret = guid118;
            
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
            juniper::records::recordt_1<uint32_t> guid119;
            guid119.lastPulse = ((uint32_t) 0);
            return guid119;
        })())));
    }
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> state) {
        return (([&]() -> Prelude::sig<uint32_t> {
            uint32_t guid120 = now();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t t = guid120;
            
            uint32_t guid121 = (((bool) (interval == ((uint32_t) 0))) ? 
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
            uint32_t lastWindow = guid121;
            
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
            double guid122 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid122;
            
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
            double guid123 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid123;
            
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
            double guid124 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid124;
            
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
            double guid125 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid125;
            
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
            double guid126 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid126;
            
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
            double guid127 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid127;
            
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
            double guid128 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid128;
            
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
            double guid129 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid129;
            
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
            double guid130 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid130;
            
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
            double guid131 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid131;
            
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
            double guid132 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid132;
            
            int32_t guid133 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t exponent = guid133;
            
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
            double guid134 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid134;
            
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
            double guid135 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid135;
            
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
            double guid136 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid136;
            
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
            double guid137 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid137;
            
            double guid138 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double integer = guid138;
            
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
            double guid139 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid139;
            
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
            double guid140 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid140;
            
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
            double guid141 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid141;
            
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
            double guid142 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid142;
            
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
            double guid143 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid143;
            
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
            double guid144 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid144;
            
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
    template<typename t1304>
    t1304 max_(t1304 x, t1304 y) {
        return (([&]() -> t1304 {
            using a = t1304;
            return (((bool) (x > y)) ? 
                (([&]() -> t1304 {
                    return x;
                })())
            :
                (([&]() -> t1304 {
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
    template<typename t1307>
    t1307 clamp(t1307 x, t1307 min, t1307 max) {
        return (([&]() -> t1307 {
            using a = t1307;
            return (((bool) (min > x)) ? 
                (([&]() -> t1307 {
                    return min;
                })())
            :
                (((bool) (x > max)) ? 
                    (([&]() -> t1307 {
                        return max;
                    })())
                :
                    (([&]() -> t1307 {
                        return x;
                    })())));
        })());
    }
}

namespace Math {
    template<typename t1312>
    int8_t sign(t1312 n) {
        return (([&]() -> int8_t {
            using a = t1312;
            return (((bool) (n == ((t1312) 0))) ? 
                (([&]() -> int8_t {
                    return ((int8_t) 0);
                })())
            :
                (((bool) (n > ((t1312) 0))) ? 
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
            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid145;
            guid145.actualState = Io::low();
            guid145.lastState = Io::low();
            guid145.lastDebounceTime = ((uint32_t) 0);
            return guid145;
        })())));
    }
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> buttonState) {
        return Signal::map<Io::pinState, juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>, Io::pinState>(juniper::function<juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>, Io::pinState(Io::pinState)>(juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>(buttonState, delay), [](juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>& junclosure, Io::pinState currentState) -> Io::pinState { 
            juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>& buttonState = junclosure.buttonState;
            uint16_t& delay = junclosure.delay;
            return (([&]() -> Io::pinState {
                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid146 = (*((buttonState).get()));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lastDebounceTime = (guid146).lastDebounceTime;
                Io::pinState lastState = (guid146).lastState;
                Io::pinState actualState = (guid146).actualState;
                
                return (((bool) (currentState != lastState)) ? 
                    (([&]() -> Io::pinState {
                        (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid147;
                            guid147.actualState = actualState;
                            guid147.lastState = currentState;
                            guid147.lastDebounceTime = Time::now();
                            return guid147;
                        })()));
                        return actualState;
                    })())
                :
                    (((bool) (((bool) (currentState != actualState)) && ((bool) (((uint32_t) (Time::now() - ((buttonState).get())->lastDebounceTime)) > delay)))) ? 
                        (([&]() -> Io::pinState {
                            (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid148;
                                guid148.actualState = currentState;
                                guid148.lastState = currentState;
                                guid148.lastDebounceTime = lastDebounceTime;
                                return guid148;
                            })()));
                            return currentState;
                        })())
                    :
                        (([&]() -> Io::pinState {
                            (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid149;
                                guid149.actualState = actualState;
                                guid149.lastState = currentState;
                                guid149.lastDebounceTime = lastDebounceTime;
                                return guid149;
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
    template<typename t1360, int c81>
    juniper::array<t1360, c81> add(juniper::array<t1360, c81> v1, juniper::array<t1360, c81> v2) {
        return (([&]() -> juniper::array<t1360, c81> {
            using a = t1360;
            constexpr int32_t n = c81;
            return (([&]() -> juniper::array<t1360, c81> {
                juniper::array<t1360, c81> guid150 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1360, c81> result = guid150;
                
                (([&]() -> juniper::unit {
                    int32_t guid151 = ((int32_t) 0);
                    int32_t guid152 = n;
                    for (int32_t i = guid151; i < guid152; i++) {
                        (([&]() -> t1360 {
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
    template<typename t1362, int c84>
    juniper::array<t1362, c84> zero() {
        return (([&]() -> juniper::array<t1362, c84> {
            using a = t1362;
            constexpr int32_t n = c84;
            return (juniper::array<t1362, c84>().fill(((t1362) 0)));
        })());
    }
}

namespace Vector {
    template<typename t1368, int c85>
    juniper::array<t1368, c85> subtract(juniper::array<t1368, c85> v1, juniper::array<t1368, c85> v2) {
        return (([&]() -> juniper::array<t1368, c85> {
            using a = t1368;
            constexpr int32_t n = c85;
            return (([&]() -> juniper::array<t1368, c85> {
                juniper::array<t1368, c85> guid153 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1368, c85> result = guid153;
                
                (([&]() -> juniper::unit {
                    int32_t guid154 = ((int32_t) 0);
                    int32_t guid155 = n;
                    for (int32_t i = guid154; i < guid155; i++) {
                        (([&]() -> t1368 {
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
    template<typename t1372, int c88>
    juniper::array<t1372, c88> scale(t1372 scalar, juniper::array<t1372, c88> v) {
        return (([&]() -> juniper::array<t1372, c88> {
            using a = t1372;
            constexpr int32_t n = c88;
            return (([&]() -> juniper::array<t1372, c88> {
                juniper::array<t1372, c88> guid156 = v;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1372, c88> result = guid156;
                
                (([&]() -> juniper::unit {
                    int32_t guid157 = ((int32_t) 0);
                    int32_t guid158 = n;
                    for (int32_t i = guid157; i < guid158; i++) {
                        (([&]() -> t1372 {
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
    template<typename t1378, int c90>
    t1378 dot(juniper::array<t1378, c90> v1, juniper::array<t1378, c90> v2) {
        return (([&]() -> t1378 {
            using a = t1378;
            constexpr int32_t n = c90;
            return (([&]() -> t1378 {
                t1378 guid159 = ((t1378) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1378 sum = guid159;
                
                (([&]() -> juniper::unit {
                    int32_t guid160 = ((int32_t) 0);
                    int32_t guid161 = n;
                    for (int32_t i = guid160; i < guid161; i++) {
                        (([&]() -> t1378 {
                            return (sum += ((t1378) ((v1)[i] * (v2)[i])));
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
    template<typename t1384, int c93>
    t1384 magnitude2(juniper::array<t1384, c93> v) {
        return (([&]() -> t1384 {
            using a = t1384;
            constexpr int32_t n = c93;
            return (([&]() -> t1384 {
                t1384 guid162 = ((t1384) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1384 sum = guid162;
                
                (([&]() -> juniper::unit {
                    int32_t guid163 = ((int32_t) 0);
                    int32_t guid164 = n;
                    for (int32_t i = guid163; i < guid164; i++) {
                        (([&]() -> t1384 {
                            return (sum += ((t1384) ((v)[i] * (v)[i])));
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
    template<typename t1386, int c96>
    double magnitude(juniper::array<t1386, c96> v) {
        return (([&]() -> double {
            using a = t1386;
            constexpr int32_t n = c96;
            return sqrt_(toDouble<t1386>(magnitude2<t1386, c96>(v)));
        })());
    }
}

namespace Vector {
    template<typename t1402, int c98>
    juniper::array<t1402, c98> multiply(juniper::array<t1402, c98> u, juniper::array<t1402, c98> v) {
        return (([&]() -> juniper::array<t1402, c98> {
            using a = t1402;
            constexpr int32_t n = c98;
            return (([&]() -> juniper::array<t1402, c98> {
                juniper::array<t1402, c98> guid165 = u;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1402, c98> result = guid165;
                
                (([&]() -> juniper::unit {
                    int32_t guid166 = ((int32_t) 0);
                    int32_t guid167 = n;
                    for (int32_t i = guid166; i < guid167; i++) {
                        (([&]() -> t1402 {
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
    template<typename t1411, int c101>
    juniper::array<t1411, c101> normalize(juniper::array<t1411, c101> v) {
        return (([&]() -> juniper::array<t1411, c101> {
            using a = t1411;
            constexpr int32_t n = c101;
            return (([&]() -> juniper::array<t1411, c101> {
                double guid168 = magnitude<t1411, c101>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid168;
                
                return (((bool) (mag > ((t1411) 0))) ? 
                    (([&]() -> juniper::array<t1411, c101> {
                        juniper::array<t1411, c101> guid169 = v;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::array<t1411, c101> result = guid169;
                        
                        (([&]() -> juniper::unit {
                            int32_t guid170 = ((int32_t) 0);
                            int32_t guid171 = n;
                            for (int32_t i = guid170; i < guid171; i++) {
                                (([&]() -> t1411 {
                                    return ((result)[i] = fromDouble<t1411>(((double) (toDouble<t1411>((result)[i]) / mag))));
                                })());
                            }
                            return {};
                        })());
                        return result;
                    })())
                :
                    (([&]() -> juniper::array<t1411, c101> {
                        return v;
                    })()));
            })());
        })());
    }
}

namespace Vector {
    template<typename t1422, int c105>
    double angle(juniper::array<t1422, c105> v1, juniper::array<t1422, c105> v2) {
        return (([&]() -> double {
            using a = t1422;
            constexpr int32_t n = c105;
            return acos_(((double) (toDouble<t1422>(dot<t1422, c105>(v1, v2)) / sqrt_(toDouble<t1422>(((t1422) (magnitude2<t1422, c105>(v1) * magnitude2<t1422, c105>(v2))))))));
        })());
    }
}

namespace Vector {
    template<typename t1464>
    juniper::array<t1464, 3> cross(juniper::array<t1464, 3> u, juniper::array<t1464, 3> v) {
        return (([&]() -> juniper::array<t1464, 3> {
            using a = t1464;
            return (juniper::array<t1464, 3> { {((t1464) (((t1464) ((u)[y] * (v)[z])) - ((t1464) ((u)[z] * (v)[y])))), ((t1464) (((t1464) ((u)[z] * (v)[x])) - ((t1464) ((u)[x] * (v)[z])))), ((t1464) (((t1464) ((u)[x] * (v)[y])) - ((t1464) ((u)[y] * (v)[x]))))} });
        })());
    }
}

namespace Vector {
    template<typename t1466, int c121>
    juniper::array<t1466, c121> project(juniper::array<t1466, c121> a, juniper::array<t1466, c121> b) {
        return (([&]() -> juniper::array<t1466, c121> {
            using z = t1466;
            constexpr int32_t n = c121;
            return (([&]() -> juniper::array<t1466, c121> {
                juniper::array<t1466, c121> guid172 = normalize<t1466, c121>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t1466, c121> bn = guid172;
                
                return scale<t1466, c121>(dot<t1466, c121>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t1482, int c125>
    juniper::array<t1482, c125> projectPlane(juniper::array<t1482, c125> a, juniper::array<t1482, c125> m) {
        return (([&]() -> juniper::array<t1482, c125> {
            using z = t1482;
            constexpr int32_t n = c125;
            return subtract<t1482, c125>(a, project<t1482, c125>(a, m));
        })());
    }
}

namespace CharList {
    template<int c128>
    juniper::records::recordt_0<juniper::array<uint8_t, c128>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c128>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c128>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 97))) && ((bool) (c <= ((uint8_t) 122))))) ? 
                ((uint8_t) (c - ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c129>
    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c129>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 65))) && ((bool) (c <= ((uint8_t) 90))))) ? 
                ((uint8_t) (c + ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c130>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t> i32ToCharList(int32_t m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t> {
            constexpr int32_t n = c130;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t> guid173 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t> guid174;
                    guid174.data = (juniper::array<uint8_t, (1)+(c130)>().fill(((uint8_t) 0)));
                    guid174.length = ((uint32_t) 0);
                    return guid174;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c130)>, uint32_t> ret = guid173;
                
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
    template<int c131>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c131>, uint32_t> s) {
        return (([&]() -> uint32_t {
            constexpr int32_t n = c131;
            return ((uint32_t) ((s).length - ((uint32_t) 1)));
        })());
    }
}

namespace CharList {
    template<int c132, int c133, int c134>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c132)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c133)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> {
            constexpr int32_t aCap = c132;
            constexpr int32_t bCap = c133;
            constexpr int32_t retCap = c134;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> {
                uint32_t guid175 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid175;
                
                uint32_t guid176 = length<(1)+(c132)>(sA);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenA = guid176;
                
                uint32_t guid177 = length<(1)+(c133)>(sB);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenB = guid177;
                
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> guid178 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> guid179;
                    guid179.data = (juniper::array<uint8_t, (1)+(c134)>().fill(((uint8_t) 0)));
                    guid179.length = ((uint32_t) (((uint32_t) (lenA + lenB)) + ((uint32_t) 1)));
                    return guid179;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c134)>, uint32_t> out = guid178;
                
                (([&]() -> juniper::unit {
                    uint32_t guid180 = ((uint32_t) 0);
                    uint32_t guid181 = lenA;
                    for (uint32_t i = guid180; i < guid181; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sA).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid182 = ((uint32_t) 0);
                    uint32_t guid183 = lenB;
                    for (uint32_t i = guid182; i < guid183; i++) {
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
    template<int c141, int c142>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c141)*(-1)))+((c142)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c141)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c142)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c141)*(-1)))+((c142)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t aCap = c141;
            constexpr int32_t bCap = c142;
            return concat<c141, c142, (-1)+((((-1)+((c141)*(-1)))+((c142)*(-1)))*(-1))>(sA, sB);
        })());
    }
}

namespace Random {
    int32_t random_(int32_t low, int32_t high) {
        return (([&]() -> int32_t {
            int32_t guid184 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t ret = guid184;
            
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
    template<typename t1552, int c146>
    t1552 choice(juniper::records::recordt_0<juniper::array<t1552, c146>, uint32_t> lst) {
        return (([&]() -> t1552 {
            using t = t1552;
            constexpr int32_t n = c146;
            return (([&]() -> t1552 {
                int32_t guid185 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid185;
                
                return Maybe::get<t1552>(List::nth<t1552, c146>(i32ToU32(i), lst));
            })());
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_5<float, float, float> color) {
        return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> {
            juniper::records::recordt_5<float, float, float> guid186 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float v = (guid186).v;
            float s = (guid186).s;
            float h = (guid186).h;
            
            float guid187 = ((float) (v * s));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float c = guid187;
            
            float guid188 = ((float) (c * toFloat<double>(((double) (1.0 - Math::fabs_(((double) (Math::fmod_(((double) (toDouble<float>(h) / ((double) 60))), 2.0) - 1.0))))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float x = guid188;
            
            float guid189 = ((float) (v - c));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float m = guid189;
            
            juniper::tuple3<float, float, float> guid190 = (((bool) (((bool) (0.0f <= h)) && ((bool) (h < 60.0f)))) ? 
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
            float bPrime = (guid190).e3;
            float gPrime = (guid190).e2;
            float rPrime = (guid190).e1;
            
            float guid191 = ((float) (((float) (rPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r = guid191;
            
            float guid192 = ((float) (((float) (gPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g = guid192;
            
            float guid193 = ((float) (((float) (bPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b = guid193;
            
            return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
                juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid194;
                guid194.r = toUInt8<float>(r);
                guid194.g = toUInt8<float>(g);
                guid194.b = toUInt8<float>(b);
                return guid194;
            })());
        })());
    }
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> color) {
        return (([&]() -> uint16_t {
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid195 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = (guid195).b;
            uint8_t g = (guid195).g;
            uint8_t r = (guid195).r;
            
            return ((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (u8ToU16(r) & ((uint16_t) 248))) << ((uint32_t) 8))) | ((uint16_t) (((uint16_t) (u8ToU16(g) & ((uint16_t) 252))) << ((uint32_t) 3))))) | ((uint16_t) (u8ToU16(b) >> ((uint32_t) 3)))));
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> red = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid196;
        guid196.r = ((uint8_t) 255);
        guid196.g = ((uint8_t) 0);
        guid196.b = ((uint8_t) 0);
        return guid196;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> green = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid197;
        guid197.r = ((uint8_t) 0);
        guid197.g = ((uint8_t) 255);
        guid197.b = ((uint8_t) 0);
        return guid197;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> blue = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid198;
        guid198.r = ((uint8_t) 0);
        guid198.g = ((uint8_t) 0);
        guid198.b = ((uint8_t) 255);
        return guid198;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> black = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid199;
        guid199.r = ((uint8_t) 0);
        guid199.g = ((uint8_t) 0);
        guid199.b = ((uint8_t) 0);
        return guid199;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> white = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid200;
        guid200.r = ((uint8_t) 255);
        guid200.g = ((uint8_t) 255);
        guid200.b = ((uint8_t) 255);
        return guid200;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> yellow = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid201;
        guid201.r = ((uint8_t) 255);
        guid201.g = ((uint8_t) 255);
        guid201.b = ((uint8_t) 0);
        return guid201;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> magenta = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid202;
        guid202.r = ((uint8_t) 255);
        guid202.g = ((uint8_t) 0);
        guid202.b = ((uint8_t) 255);
        return guid202;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> cyan = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid203;
        guid203.r = ((uint8_t) 0);
        guid203.g = ((uint8_t) 255);
        guid203.b = ((uint8_t) 255);
        return guid203;
    })());
}

namespace Arcada {
    bool arcadaBegin() {
        return (([&]() -> bool {
            bool guid204 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid204;
            
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
            uint16_t guid205 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid205;
            
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
            uint16_t guid206 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid206;
            
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
            uint16_t guid207 = displayWidth();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid207;
            
            uint16_t guid208 = displayHeight();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid208;
            
            bool guid209 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid209;
            
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
            bool guid210 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid210;
            
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
            Ble::servicet guid211 = s;
            if (!(((bool) (((bool) ((guid211).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid211).service();
            
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
            Ble::characterstict guid212 = c;
            if (!(((bool) (((bool) ((guid212).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid212).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->begin();
                return {};
            })());
        })());
    }
}

namespace Ble {
    template<int c148>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c148>, uint32_t> bytes) {
        return (([&]() -> uint16_t {
            constexpr int32_t n = c148;
            return (([&]() -> uint16_t {
                juniper::records::recordt_0<juniper::array<uint8_t, c148>, uint32_t> guid213 = bytes;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t length = (guid213).length;
                juniper::array<uint8_t, c148> data = (guid213).data;
                
                Ble::characterstict guid214 = c;
                if (!(((bool) (((bool) ((guid214).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid214).characterstic();
                
                uint16_t guid215 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid215;
                
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
            Ble::characterstict guid216 = c;
            if (!(((bool) (((bool) ((guid216).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid216).characterstic();
            
            uint16_t guid217 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid217;
            
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
            Ble::characterstict guid218 = c;
            if (!(((bool) (((bool) ((guid218).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid218).characterstic();
            
            uint16_t guid219 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid219;
            
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
            Ble::characterstict guid220 = c;
            if (!(((bool) (((bool) ((guid220).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid220).characterstic();
            
            uint16_t guid221 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid221;
            
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
            Ble::characterstict guid222 = c;
            if (!(((bool) (((bool) ((guid222).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid222).characterstic();
            
            uint16_t guid223 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid223;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write32((int) n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    template<typename t1906>
    uint16_t writeGeneric(Ble::characterstict c, t1906 x) {
        return (([&]() -> uint16_t {
            using t = t1906;
            return (([&]() -> uint16_t {
                Ble::characterstict guid224 = c;
                if (!(((bool) (((bool) ((guid224).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid224).characterstic();
                
                uint16_t guid225 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid225;
                
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
    template<typename t1911>
    t1911 readGeneric(Ble::characterstict c) {
        return (([&]() -> t1911 {
            using t = t1911;
            return (([&]() -> t1911 {
                Ble::characterstict guid226 = c;
                if (!(((bool) (((bool) ((guid226).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid226).characterstic();
                
                t1911 ret;
                
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
            Ble::secureModet guid227 = readPermission;
            if (!(((bool) (((bool) ((guid227).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t readN = (guid227).secureMode();
            
            Ble::secureModet guid228 = writePermission;
            if (!(((bool) (((bool) ((guid228).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t writeN = (guid228).secureMode();
            
            Ble::characterstict guid229 = c;
            if (!(((bool) (((bool) ((guid229).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid229).characterstic();
            
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
            Ble::propertiest guid230 = prop;
            if (!(((bool) (((bool) ((guid230).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t propN = (guid230).properties();
            
            Ble::characterstict guid231 = c;
            if (!(((bool) (((bool) ((guid231).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid231).characterstic();
            
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
            Ble::characterstict guid232 = c;
            if (!(((bool) (((bool) ((guid232).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid232).characterstic();
            
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
            Ble::advertisingFlagt guid233 = flag;
            if (!(((bool) (((bool) ((guid233).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t flagNum = (guid233).advertisingFlag();
            
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
            Ble::appearancet guid234 = app;
            if (!(((bool) (((bool) ((guid234).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t flagNum = (guid234).appearance();
            
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
            Ble::servicet guid235 = serv;
            if (!(((bool) (((bool) ((guid235).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid235).service();
            
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
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid236;
        guid236.r = ((uint8_t) 252);
        guid236.g = ((uint8_t) 92);
        guid236.b = ((uint8_t) 125);
        return guid236;
    })());
}

namespace CWatch {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> purpleBlue = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid237;
        guid237.r = ((uint8_t) 106);
        guid237.g = ((uint8_t) 130);
        guid237.b = ((uint8_t) 251);
        return guid237;
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
            CWatch::month guid238 = m;
            return (((bool) (((bool) ((guid238).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 31);
                })())
            :
                (((bool) (((bool) ((guid238).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return (isLeapYear(y) ? 
                            ((uint8_t) 29)
                        :
                            ((uint8_t) 28));
                    })())
                :
                    (((bool) (((bool) ((guid238).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 31);
                        })())
                    :
                        (((bool) (((bool) ((guid238).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> uint8_t {
                                return ((uint8_t) 30);
                            })())
                        :
                            (((bool) (((bool) ((guid238).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> uint8_t {
                                    return ((uint8_t) 31);
                                })())
                            :
                                (((bool) (((bool) ((guid238).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> uint8_t {
                                        return ((uint8_t) 30);
                                    })())
                                :
                                    (((bool) (((bool) ((guid238).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> uint8_t {
                                            return ((uint8_t) 31);
                                        })())
                                    :
                                        (((bool) (((bool) ((guid238).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> uint8_t {
                                                return ((uint8_t) 31);
                                            })())
                                        :
                                            (((bool) (((bool) ((guid238).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> uint8_t {
                                                    return ((uint8_t) 30);
                                                })())
                                            :
                                                (((bool) (((bool) ((guid238).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> uint8_t {
                                                        return ((uint8_t) 31);
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid238).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> uint8_t {
                                                            return ((uint8_t) 30);
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid238).id() == ((uint8_t) 11))) && true)) ? 
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
            CWatch::month guid239 = m;
            return (((bool) (((bool) ((guid239).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::month {
                    return february();
                })())
            :
                (((bool) (((bool) ((guid239).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::month {
                        return march();
                    })())
                :
                    (((bool) (((bool) ((guid239).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::month {
                            return april();
                        })())
                    :
                        (((bool) (((bool) ((guid239).id() == ((uint8_t) 4))) && true)) ? 
                            (([&]() -> CWatch::month {
                                return june();
                            })())
                        :
                            (((bool) (((bool) ((guid239).id() == ((uint8_t) 5))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return july();
                                })())
                            :
                                (((bool) (((bool) ((guid239).id() == ((uint8_t) 7))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return august();
                                    })())
                                :
                                    (((bool) (((bool) ((guid239).id() == ((uint8_t) 8))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return october();
                                        })())
                                    :
                                        (((bool) (((bool) ((guid239).id() == ((uint8_t) 9))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return november();
                                            })())
                                        :
                                            (((bool) (((bool) ((guid239).id() == ((uint8_t) 11))) && true)) ? 
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
            CWatch::dayOfWeek guid240 = dw;
            return (((bool) (((bool) ((guid240).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::dayOfWeek {
                    return monday();
                })())
            :
                (((bool) (((bool) ((guid240).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::dayOfWeek {
                        return tuesday();
                    })())
                :
                    (((bool) (((bool) ((guid240).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::dayOfWeek {
                            return wednesday();
                        })())
                    :
                        (((bool) (((bool) ((guid240).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> CWatch::dayOfWeek {
                                return thursday();
                            })())
                        :
                            (((bool) (((bool) ((guid240).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return friday();
                                })())
                            :
                                (((bool) (((bool) ((guid240).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return saturday();
                                    })())
                                :
                                    (((bool) (((bool) ((guid240).id() == ((uint8_t) 6))) && true)) ? 
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
            CWatch::dayOfWeek guid241 = dw;
            return (((bool) (((bool) ((guid241).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid242;
                        guid242.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid242.length = ((uint32_t) 4);
                        return guid242;
                    })());
                })())
            :
                (((bool) (((bool) ((guid241).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid243;
                            guid243.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 111), ((uint8_t) 110), ((uint8_t) 0)} });
                            guid243.length = ((uint32_t) 4);
                            return guid243;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid241).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid244;
                                guid244.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 117), ((uint8_t) 101), ((uint8_t) 0)} });
                                guid244.length = ((uint32_t) 4);
                                return guid244;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid241).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid245;
                                    guid245.data = (juniper::array<uint8_t, 4> { {((uint8_t) 87), ((uint8_t) 101), ((uint8_t) 100), ((uint8_t) 0)} });
                                    guid245.length = ((uint32_t) 4);
                                    return guid245;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid241).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid246;
                                        guid246.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 104), ((uint8_t) 117), ((uint8_t) 0)} });
                                        guid246.length = ((uint32_t) 4);
                                        return guid246;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid241).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid247;
                                            guid247.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 114), ((uint8_t) 105), ((uint8_t) 0)} });
                                            guid247.length = ((uint32_t) 4);
                                            return guid247;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid241).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid248;
                                                guid248.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 97), ((uint8_t) 116), ((uint8_t) 0)} });
                                                guid248.length = ((uint32_t) 4);
                                                return guid248;
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
            CWatch::month guid249 = m;
            return (((bool) (((bool) ((guid249).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid250;
                        guid250.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 97), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid250.length = ((uint32_t) 4);
                        return guid250;
                    })());
                })())
            :
                (((bool) (((bool) ((guid249).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid251;
                            guid251.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 101), ((uint8_t) 98), ((uint8_t) 0)} });
                            guid251.length = ((uint32_t) 4);
                            return guid251;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid249).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid252;
                                guid252.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 114), ((uint8_t) 0)} });
                                guid252.length = ((uint32_t) 4);
                                return guid252;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid249).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid253;
                                    guid253.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 112), ((uint8_t) 114), ((uint8_t) 0)} });
                                    guid253.length = ((uint32_t) 4);
                                    return guid253;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid249).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid254;
                                        guid254.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 121), ((uint8_t) 0)} });
                                        guid254.length = ((uint32_t) 4);
                                        return guid254;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid249).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid255;
                                            guid255.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                                            guid255.length = ((uint32_t) 4);
                                            return guid255;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid249).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid256;
                                                guid256.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 108), ((uint8_t) 0)} });
                                                guid256.length = ((uint32_t) 4);
                                                return guid256;
                                            })());
                                        })())
                                    :
                                        (((bool) (((bool) ((guid249).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid257;
                                                    guid257.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 117), ((uint8_t) 103), ((uint8_t) 0)} });
                                                    guid257.length = ((uint32_t) 4);
                                                    return guid257;
                                                })());
                                            })())
                                        :
                                            (((bool) (((bool) ((guid249).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid258;
                                                        guid258.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 101), ((uint8_t) 112), ((uint8_t) 0)} });
                                                        guid258.length = ((uint32_t) 4);
                                                        return guid258;
                                                    })());
                                                })())
                                            :
                                                (((bool) (((bool) ((guid249).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid259;
                                                            guid259.data = (juniper::array<uint8_t, 4> { {((uint8_t) 79), ((uint8_t) 99), ((uint8_t) 116), ((uint8_t) 0)} });
                                                            guid259.length = ((uint32_t) 4);
                                                            return guid259;
                                                        })());
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid249).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid260;
                                                                guid260.data = (juniper::array<uint8_t, 4> { {((uint8_t) 78), ((uint8_t) 111), ((uint8_t) 118), ((uint8_t) 0)} });
                                                                guid260.length = ((uint32_t) 4);
                                                                return guid260;
                                                            })());
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid249).id() == ((uint8_t) 11))) && true)) ? 
                                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid261;
                                                                    guid261.data = (juniper::array<uint8_t, 4> { {((uint8_t) 68), ((uint8_t) 101), ((uint8_t) 99), ((uint8_t) 0)} });
                                                                    guid261.length = ((uint32_t) 4);
                                                                    return guid261;
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
            juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid262 = d;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek = (guid262).dayOfWeek;
            uint8_t seconds = (guid262).seconds;
            uint8_t minutes = (guid262).minutes;
            uint8_t hours = (guid262).hours;
            uint32_t year = (guid262).year;
            uint8_t day = (guid262).day;
            CWatch::month month = (guid262).month;
            
            uint8_t guid263 = ((uint8_t) (seconds + ((uint8_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t seconds1 = guid263;
            
            juniper::tuple2<uint8_t, uint8_t> guid264 = (((bool) (seconds1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (minutes + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{seconds1, minutes}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t minutes1 = (guid264).e2;
            uint8_t seconds2 = (guid264).e1;
            
            juniper::tuple2<uint8_t, uint8_t> guid265 = (((bool) (minutes1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (hours + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{minutes1, hours}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t hours1 = (guid265).e2;
            uint8_t minutes2 = (guid265).e1;
            
            juniper::tuple3<uint8_t, uint8_t, CWatch::dayOfWeek> guid266 = (((bool) (hours1 == ((uint8_t) 24))) ? 
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{((uint8_t) 0), ((uint8_t) (day + ((uint8_t) 1))), nextDayOfWeek(dayOfWeek)})
            :
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{hours1, day, dayOfWeek}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek2 = (guid266).e3;
            uint8_t day1 = (guid266).e2;
            uint8_t hours2 = (guid266).e1;
            
            uint8_t guid267 = daysInMonth(year, month);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t daysInCurrentMonth = guid267;
            
            juniper::tuple2<uint8_t, juniper::tuple2<CWatch::month, uint32_t>> guid268 = (((bool) (day1 > daysInCurrentMonth)) ? 
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month, uint32_t>>{((uint8_t) 1), (([&]() -> juniper::tuple2<CWatch::month, uint32_t> {
                    CWatch::month guid269 = month;
                    return (((bool) (((bool) ((guid269).id() == ((uint8_t) 11))) && true)) ? 
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
            uint32_t year2 = ((guid268).e2).e2;
            CWatch::month month2 = ((guid268).e2).e1;
            uint8_t day2 = (guid268).e1;
            
            return (([&]() -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid270;
                guid270.month = month2;
                guid270.day = day2;
                guid270.year = year2;
                guid270.hours = hours2;
                guid270.minutes = minutes2;
                guid270.seconds = seconds2;
                guid270.dayOfWeek = dayOfWeek2;
                return guid270;
            })());
        })());
    }
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> clockTickerState = Time::state();
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>> clockState = (juniper::shared_ptr<juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>((([]() -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
        juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid271;
        guid271.month = september();
        guid271.day = ((uint8_t) 9);
        guid271.year = ((uint32_t) 2020);
        guid271.hours = ((uint8_t) 18);
        guid271.minutes = ((uint8_t) 40);
        guid271.seconds = ((uint8_t) 0);
        guid271.dayOfWeek = wednesday();
        return guid271;
    })())));
}

namespace CWatch {
    juniper::unit processBluetoothUpdates() {
        return (([&]() -> juniper::unit {
            bool guid272 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayDateTime = guid272;
            
            bool guid273 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayOfWeek = guid273;
            
            (([&]() -> juniper::unit {
                
    hasNewDayDateTime = rawHasNewDayDateTime;
    rawHasNewDayDateTime = false;
    hasNewDayOfWeek = rawHasNewDayOfWeek;
    rawHasNewDayOfWeek = false;
    
                return {};
            })());
            (([&]() -> juniper::unit {
                if (hasNewDayDateTime) {
                    (([&]() -> uint8_t {
                        juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> guid274 = Ble::readGeneric<juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>>(dayDateTimeCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> bleData = guid274;
                        
                        (((clockState).get())->month = (([&]() -> CWatch::month {
                            uint8_t guid275 = (bleData).month;
                            return (((bool) (((bool) (guid275 == ((int32_t) 0))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return january();
                                })())
                            :
                                (((bool) (((bool) (guid275 == ((int32_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return february();
                                    })())
                                :
                                    (((bool) (((bool) (guid275 == ((int32_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return march();
                                        })())
                                    :
                                        (((bool) (((bool) (guid275 == ((int32_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return april();
                                            })())
                                        :
                                            (((bool) (((bool) (guid275 == ((int32_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::month {
                                                    return may();
                                                })())
                                            :
                                                (((bool) (((bool) (guid275 == ((int32_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::month {
                                                        return june();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid275 == ((int32_t) 6))) && true)) ? 
                                                        (([&]() -> CWatch::month {
                                                            return july();
                                                        })())
                                                    :
                                                        (((bool) (((bool) (guid275 == ((int32_t) 7))) && true)) ? 
                                                            (([&]() -> CWatch::month {
                                                                return august();
                                                            })())
                                                        :
                                                            (((bool) (((bool) (guid275 == ((int32_t) 8))) && true)) ? 
                                                                (([&]() -> CWatch::month {
                                                                    return september();
                                                                })())
                                                            :
                                                                (((bool) (((bool) (guid275 == ((int32_t) 9))) && true)) ? 
                                                                    (([&]() -> CWatch::month {
                                                                        return october();
                                                                    })())
                                                                :
                                                                    (((bool) (((bool) (guid275 == ((int32_t) 10))) && true)) ? 
                                                                        (([&]() -> CWatch::month {
                                                                            return november();
                                                                        })())
                                                                    :
                                                                        (((bool) (((bool) (guid275 == ((int32_t) 11))) && true)) ? 
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
                        juniper::records::recordt_8<uint8_t> guid276 = Ble::readGeneric<juniper::records::recordt_8<uint8_t>>(dayOfWeekCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_8<uint8_t> bleData = guid276;
                        
                        return (((clockState).get())->dayOfWeek = (([&]() -> CWatch::dayOfWeek {
                            uint8_t guid277 = (bleData).dayOfWeek;
                            return (((bool) (((bool) (guid277 == ((int32_t) 0))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return sunday();
                                })())
                            :
                                (((bool) (((bool) (guid277 == ((int32_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return monday();
                                    })())
                                :
                                    (((bool) (((bool) (guid277 == ((int32_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::dayOfWeek {
                                            return tuesday();
                                        })())
                                    :
                                        (((bool) (((bool) (guid277 == ((int32_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::dayOfWeek {
                                                return wednesday();
                                            })())
                                        :
                                            (((bool) (((bool) (guid277 == ((int32_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::dayOfWeek {
                                                    return thursday();
                                                })())
                                            :
                                                (((bool) (((bool) (guid277 == ((int32_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::dayOfWeek {
                                                        return friday();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid277 == ((int32_t) 6))) && true)) ? 
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
            int16_t guid278 = toInt16<uint16_t>(Arcada::displayWidth());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispW = guid278;
            
            int16_t guid279 = toInt16<uint16_t>(Arcada::displayHeight());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispH = guid279;
            
            int16_t guid280 = Math::clamp<int16_t>(x0i, ((int16_t) 0), ((int16_t) (dispW - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t x0 = guid280;
            
            int16_t guid281 = Math::clamp<int16_t>(y0i, ((int16_t) 0), ((int16_t) (dispH - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y0 = guid281;
            
            int16_t guid282 = ((int16_t) (y0i + h));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t ymax = guid282;
            
            int16_t guid283 = Math::clamp<int16_t>(ymax, ((int16_t) 0), dispH);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y1 = guid283;
            
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid284 = c1;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b1 = (guid284).b;
            uint8_t g1 = (guid284).g;
            uint8_t r1 = (guid284).r;
            
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid285 = c2;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b2 = (guid285).b;
            uint8_t g2 = (guid285).g;
            uint8_t r2 = (guid285).r;
            
            float guid286 = toFloat<uint8_t>(r1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r1f = guid286;
            
            float guid287 = toFloat<uint8_t>(g1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g1f = guid287;
            
            float guid288 = toFloat<uint8_t>(b1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b1f = guid288;
            
            float guid289 = toFloat<uint8_t>(r2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r2f = guid289;
            
            float guid290 = toFloat<uint8_t>(g2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g2f = guid290;
            
            float guid291 = toFloat<uint8_t>(b2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b2f = guid291;
            
            return (([&]() -> juniper::unit {
                int16_t guid292 = y0;
                int16_t guid293 = y1;
                for (int16_t y = guid292; y < guid293; y++) {
                    (([&]() -> juniper::unit {
                        float guid294 = ((float) (toFloat<int16_t>(((int16_t) (ymax - y))) / toFloat<int16_t>(h)));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight1 = guid294;
                        
                        float guid295 = ((float) (1.0f - weight1));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight2 = guid295;
                        
                        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid296 = (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
                            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid297;
                            guid297.r = toUInt8<float>(((float) (((float) (r1f * weight1)) + ((float) (r2f * weight2)))));
                            guid297.g = toUInt8<float>(((float) (((float) (g1f * weight1)) + ((float) (g2f * weight2)))));
                            guid297.b = toUInt8<float>(((float) (((float) (b1f * weight1)) + ((float) (g2f * weight2)))));
                            return guid297;
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> gradColor = guid296;
                        
                        uint16_t guid298 = Color::rgbToRgb565(gradColor);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint16_t gradColor565 = guid298;
                        
                        return drawFastHLine565(x0, y, w, gradColor565);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace Gfx {
    template<int c149>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c149)>, uint32_t> cl) {
        return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
            constexpr int32_t n = c149;
            return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
                int16_t guid299 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t xret = guid299;
                
                int16_t guid300 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t yret = guid300;
                
                uint16_t guid301 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t wret = guid301;
                
                uint16_t guid302 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t hret = guid302;
                
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
    template<int c150>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c150)>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c150;
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
            Gfx::font guid303 = f;
            return (((bool) (((bool) ((guid303).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::unit {
                    return (([&]() -> juniper::unit {
                        arcada.getCanvas()->setFont();
                        return {};
                    })());
                })())
            :
                (((bool) (((bool) ((guid303).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::unit {
                        return (([&]() -> juniper::unit {
                            arcada.getCanvas()->setFont(&FreeSans9pt7b);
                            return {};
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid303).id() == ((uint8_t) 2))) && true)) ? 
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
            uint16_t guid304 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid304;
            
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
                    juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid305 = dt;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    CWatch::dayOfWeek dayOfWeek = (guid305).dayOfWeek;
                    uint8_t seconds = (guid305).seconds;
                    uint8_t minutes = (guid305).minutes;
                    uint8_t hours = (guid305).hours;
                    uint32_t year = (guid305).year;
                    uint8_t day = (guid305).day;
                    CWatch::month month = (guid305).month;
                    
                    int32_t guid306 = toInt32<uint8_t>((((bool) (hours == ((uint8_t) 0))) ? 
                        ((uint8_t) 12)
                    :
                        (((bool) (hours > ((uint8_t) 12))) ? 
                            ((uint8_t) (hours - ((uint8_t) 12)))
                        :
                            hours)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    int32_t displayHours = guid306;
                    
                    Gfx::setTextColor(Color::white);
                    Gfx::setFont(Gfx::freeSans24());
                    Gfx::setTextSize(((uint8_t) 1));
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid307 = CharList::i32ToCharList<2>(displayHours);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> timeHourStr = guid307;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid308 = CharList::safeConcat<2, 1>(timeHourStr, (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid309;
                        guid309.data = (juniper::array<uint8_t, 2> { {((uint8_t) 58), ((uint8_t) 0)} });
                        guid309.length = ((uint32_t) 2);
                        return guid309;
                    })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> timeHourStrColon = guid308;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid310 = (((bool) (minutes < ((uint8_t) 10))) ? 
                        CharList::safeConcat<1, 1>((([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid311;
                            guid311.data = (juniper::array<uint8_t, 2> { {((uint8_t) 48), ((uint8_t) 0)} });
                            guid311.length = ((uint32_t) 2);
                            return guid311;
                        })()), CharList::i32ToCharList<1>(toInt32<uint8_t>(minutes)))
                    :
                        CharList::i32ToCharList<2>(toInt32<uint8_t>(minutes)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> minutesStr = guid310;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> guid312 = CharList::safeConcat<3, 2>(timeHourStrColon, minutesStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> timeStr = guid312;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid313 = Gfx::getCharListBounds<5>(((int16_t) 0), ((int16_t) 0), timeStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h = (guid313).e4;
                    uint16_t w = (guid313).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2)))))));
                    Gfx::printCharList<5>(timeStr);
                    Gfx::setTextSize(((uint8_t) 1));
                    Gfx::setFont(Gfx::freeSans9());
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid314 = (((bool) (hours < ((uint8_t) 12))) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid315;
                            guid315.data = (juniper::array<uint8_t, 3> { {((uint8_t) 65), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid315.length = ((uint32_t) 3);
                            return guid315;
                        })())
                    :
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid316;
                            guid316.data = (juniper::array<uint8_t, 3> { {((uint8_t) 80), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid316.length = ((uint32_t) 3);
                            return guid316;
                        })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> ampm = guid314;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid317 = Gfx::getCharListBounds<2>(((int16_t) 0), ((int16_t) 0), ampm);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h2 = (guid317).e4;
                    uint16_t w2 = (guid317).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w2 / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2))))) + h2)) + ((uint16_t) 5)))));
                    Gfx::printCharList<2>(ampm);
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> guid318 = CharList::safeConcat<9, 2>(CharList::safeConcat<8, 1>(CharList::safeConcat<5, 3>(CharList::safeConcat<3, 2>(dayOfWeekToCharList(dayOfWeek), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid319;
                        guid319.data = (juniper::array<uint8_t, 3> { {((uint8_t) 44), ((uint8_t) 32), ((uint8_t) 0)} });
                        guid319.length = ((uint32_t) 3);
                        return guid319;
                    })())), monthToCharList(month)), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid320;
                        guid320.data = (juniper::array<uint8_t, 2> { {((uint8_t) 32), ((uint8_t) 0)} });
                        guid320.length = ((uint32_t) 2);
                        return guid320;
                    })())), CharList::i32ToCharList<2>(toInt32<uint8_t>(day)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> dateStr = guid318;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid321 = Gfx::getCharListBounds<11>(((int16_t) 0), ((int16_t) 0), dateStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h3 = (guid321).e4;
                    uint16_t w3 = (guid321).e3;
                    
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
            uint16_t guid322 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid322;
            
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
            uint16_t guid323 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid323;
            
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
    template<int c175>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c175>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c175;
            return (([&]() -> juniper::unit {
                juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid324 = Gfx::getCharListBounds<(-1)+(c175)>(((int16_t) 0), ((int16_t) 0), cl);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t h = (guid324).e4;
                uint16_t w = (guid324).e3;
                
                int16_t guid325 = toInt16<uint16_t>(w);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t ws = guid325;
                
                int16_t guid326 = toInt16<uint16_t>(h);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t hs = guid326;
                
                return (([&]() -> juniper::unit {
                    Gfx::align guid327 = align;
                    return (((bool) (((bool) ((guid327).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            return Gfx::setCursor(((int16_t) (x - ((int16_t) (ws / ((int16_t) 2))))), y);
                        })())
                    :
                        (((bool) (((bool) ((guid327).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::unit {
                                return Gfx::setCursor(x, ((int16_t) (y - ((int16_t) (hs / ((int16_t) 2))))));
                            })())
                        :
                            (((bool) (((bool) ((guid327).id() == ((uint8_t) 2))) && true)) ? 
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
