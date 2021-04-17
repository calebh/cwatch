//Compiled on 4/16/2021 9:04:03 PM
#include <inttypes.h>
#include <stdbool.h>

#ifndef JUNIPER_H
#define JUNIPER_H

#include <stdlib.h>

namespace juniper
{
    template <typename contained>
    class shared_ptr {
    private:
        contained* ptr_;
        int* ref_count_;

        void inc_ref() {
            if (ref_count_ != nullptr) {
                ++(*ref_count_);
            }
        }

        void dec_ref() {
            if (ref_count_ != nullptr) {
                --(*ref_count_);

                if (*ref_count_ <= 0)
                {
                    if (ptr_ != nullptr)
                    {
                        delete ptr_;
                    }
                    delete ref_count_;
                }
            }
        }

    public:
        shared_ptr()
            : ptr_(nullptr), ref_count_(new int(1))
        {
        }

        shared_ptr(contained* p)
            : ptr_(p), ref_count_(new int(1))
        {
        }

        // Copy constructor
        shared_ptr(const shared_ptr& rhs)
            : ptr_(rhs.ptr_), ref_count_(rhs.ref_count_)
        {
            inc_ref();
        }

        // Move constructor
        shared_ptr(shared_ptr&& dyingObj)
            : ptr_(dyingObj.ptr_), ref_count_(dyingObj.ref_count_) {

            // Clean the dying object
            dyingObj.ptr_ = nullptr;
            dyingObj.ref_count_ = nullptr;
        }

        ~shared_ptr()
        {
            dec_ref();
        }

        void set(contained* p) {
            ptr_ = p;
        }

        contained* get() { return ptr_; }
        const contained* get() const { return ptr_; }

        // Copy assignment
        shared_ptr& operator=(const shared_ptr& rhs) {
            dec_ref();

            this->ptr_ = rhs.ptr_;
            this->ref_count_ = rhs.ref_count_;
            if (rhs.ptr_ != nullptr)
            {
                inc_ref();
            }

            return *this;
        }

        // Move assignment
        shared_ptr& operator=(shared_ptr&& dyingObj) {
            dec_ref();

            this->ptr_ = dyingObj.ptr;
            this->ref_count_ = dyingObj.refCount;

            // Clean the dying object
            dyingObj.ptr_ = nullptr;
            dyingObj.ref_count_ = nullptr;

            return *this;
        }

        contained& operator*() {
            return *ptr_;
        }

        contained* operator->() {
            return ptr_;
        }

        bool operator==(shared_ptr& rhs) {
            return ptr_ == rhs.ptr_;
        }

        bool operator!=(shared_ptr& rhs) {
            return ptr_ != rhs.ptr_;
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
    };

    using smartpointer = shared_ptr<rawpointer_container>;

    smartpointer make_smartpointer(void *initData, function<void, unit(void*)> callback) {
        return smartpointer(new rawpointer_container(initData, callback));
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
    using namespace Io;

}

namespace Vector {
    using namespace Prelude;
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
    using namespace Arcada;
    using namespace Color;

}

namespace juniper::records {
    template<typename T1,typename T2,typename T3,typename T4>
    struct recordt_5 {
        T1 a;
        T2 b;
        T3 g;
        T4 r;

        recordt_5() {}

        recordt_5(T1 init_a, T2 init_b, T3 init_g, T4 init_r)
            : a(init_a), b(init_b), g(init_g), r(init_r) {}

        bool operator==(recordt_5<T1, T2, T3, T4> rhs) {
            return true && a == rhs.a && b == rhs.b && g == rhs.g && r == rhs.r;
        }

        bool operator!=(recordt_5<T1, T2, T3, T4> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename T1,typename T2,typename T3,typename T4>
    struct recordt_7 {
        T1 a;
        T2 h;
        T3 s;
        T4 v;

        recordt_7() {}

        recordt_7(T1 init_a, T2 init_h, T3 init_s, T4 init_v)
            : a(init_a), h(init_h), s(init_s), v(init_v) {}

        bool operator==(recordt_7<T1, T2, T3, T4> rhs) {
            return true && a == rhs.a && h == rhs.h && s == rhs.s && v == rhs.v;
        }

        bool operator!=(recordt_7<T1, T2, T3, T4> rhs) {
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
    struct recordt_4 {
        T1 b;
        T2 g;
        T3 r;

        recordt_4() {}

        recordt_4(T1 init_b, T2 init_g, T3 init_r)
            : b(init_b), g(init_g), r(init_r) {}

        bool operator==(recordt_4<T1, T2, T3> rhs) {
            return true && b == rhs.b && g == rhs.g && r == rhs.r;
        }

        bool operator!=(recordt_4<T1, T2, T3> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename T1>
    struct recordt_3 {
        T1 data;

        recordt_3() {}

        recordt_3(T1 init_data)
            : data(init_data) {}

        bool operator==(recordt_3<T1> rhs) {
            return true && data == rhs.data;
        }

        bool operator!=(recordt_3<T1> rhs) {
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
    struct recordt_10 {
        T1 day;
        T2 dayOfWeek;
        T3 hours;
        T4 minutes;
        T5 month;
        T6 seconds;
        T7 year;

        recordt_10() {}

        recordt_10(T1 init_day, T2 init_dayOfWeek, T3 init_hours, T4 init_minutes, T5 init_month, T6 init_seconds, T7 init_year)
            : day(init_day), dayOfWeek(init_dayOfWeek), hours(init_hours), minutes(init_minutes), month(init_month), seconds(init_seconds), year(init_year) {}

        bool operator==(recordt_10<T1, T2, T3, T4, T5, T6, T7> rhs) {
            return true && day == rhs.day && dayOfWeek == rhs.dayOfWeek && hours == rhs.hours && minutes == rhs.minutes && month == rhs.month && seconds == rhs.seconds && year == rhs.year;
        }

        bool operator!=(recordt_10<T1, T2, T3, T4, T5, T6, T7> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename T1,typename T2,typename T3>
    struct recordt_6 {
        T1 h;
        T2 s;
        T3 v;

        recordt_6() {}

        recordt_6(T1 init_h, T2 init_s, T3 init_v)
            : h(init_h), s(init_s), v(init_v) {}

        bool operator==(recordt_6<T1, T2, T3> rhs) {
            return true && h == rhs.h && s == rhs.s && v == rhs.v;
        }

        bool operator!=(recordt_6<T1, T2, T3> rhs) {
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
    struct __attribute__((__packed__)) recordt_9 {
        T1 dayOfWeek;

        recordt_9() {}

        recordt_9(T1 init_dayOfWeek)
            : dayOfWeek(init_dayOfWeek) {}

        bool operator==(recordt_9<T1> rhs) {
            return true && dayOfWeek == rhs.dayOfWeek;
        }

        bool operator!=(recordt_9<T1> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename T1,typename T2,typename T3,typename T4,typename T5,typename T6>
    struct __attribute__((__packed__)) recordt_8 {
        T1 month;
        T2 day;
        T3 year;
        T4 hours;
        T5 minutes;
        T6 seconds;

        recordt_8() {}

        recordt_8(T1 init_month, T2 init_day, T3 init_year, T4 init_hours, T5 init_minutes, T6 init_seconds)
            : month(init_month), day(init_day), year(init_year), hours(init_hours), minutes(init_minutes), seconds(init_seconds) {}

        bool operator==(recordt_8<T1, T2, T3, T4, T5, T6> rhs) {
            return true && month == rhs.month && day == rhs.day && year == rhs.year && hours == rhs.hours && minutes == rhs.minutes && seconds == rhs.seconds;
        }

        bool operator!=(recordt_8<T1, T2, T3, T4, T5, T6> rhs) {
            return !(rhs == *this);
        }
    };


}

namespace juniper::closures {
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
    using vector = juniper::records::recordt_3<juniper::array<a, n>>;


}

namespace Color {
    using rgb = juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>;


}

namespace Color {
    using rgba = juniper::records::recordt_5<uint8_t, uint8_t, uint8_t, uint8_t>;


}

namespace Color {
    using hsv = juniper::records::recordt_6<float, float, float>;


}

namespace Color {
    using hsva = juniper::records::recordt_7<float, float, float, float>;


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
    using dayDateTimeBLE = juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>;


}

namespace CWatch {
    using dayOfWeekBLE = juniper::records::recordt_9<uint8_t>;


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
    using datetime = juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>;


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
    template<typename t7, typename t3, typename t4, typename t5, typename t6>
    juniper::function<juniper::closures::closuret_0<juniper::function<t5, t4(t3)>, juniper::function<t6, t3(t7)>>, t4(t7)> compose(juniper::function<t5, t4(t3)> f, juniper::function<t6, t3(t7)> g);
}

namespace Prelude {
    template<typename t19, typename t20, typename t17, typename t18>
    juniper::function<juniper::closures::closuret_1<juniper::function<t18, t17(t19, t20)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>, t17(t20)>(t19)> curry(juniper::function<t18, t17(t19, t20)> f);
}

namespace Prelude {
    template<typename t31, typename t32, typename t28, typename t29, typename t30>
    juniper::function<juniper::closures::closuret_1<juniper::function<t29, juniper::function<t30, t28(t32)>(t31)>>, t28(t31, t32)> uncurry(juniper::function<t29, juniper::function<t30, t28(t32)>(t31)> f);
}

namespace Prelude {
    template<typename t45, typename t46, typename t47, typename t43, typename t44>
    juniper::function<juniper::closures::closuret_1<juniper::function<t44, t43(t45, t46, t47)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>, juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)>(t46)>(t45)> curry3(juniper::function<t44, t43(t45, t46, t47)> f);
}

namespace Prelude {
    template<typename t61, typename t62, typename t63, typename t57, typename t58, typename t59, typename t60>
    juniper::function<juniper::closures::closuret_1<juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)>>, t57(t61, t62, t63)> uncurry3(juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)> f);
}

namespace Prelude {
    template<typename t74>
    bool eq(t74 x, t74 y);
}

namespace Prelude {
    template<typename t77>
    bool neq(t77 x, t77 y);
}

namespace Prelude {
    template<typename t79, typename t80>
    bool gt(t79 x, t80 y);
}

namespace Prelude {
    template<typename t82, typename t83>
    bool geq(t82 x, t83 y);
}

namespace Prelude {
    template<typename t85, typename t86>
    bool lt(t85 x, t86 y);
}

namespace Prelude {
    template<typename t88, typename t89>
    bool leq(t88 x, t89 y);
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
    template<typename t99, typename t100, typename t101>
    t100 apply(juniper::function<t101, t100(t99)> f, t99 x);
}

namespace Prelude {
    template<typename t106, typename t107, typename t108, typename t109>
    t108 apply2(juniper::function<t109, t108(t106, t107)> f, juniper::tuple2<t106,t107> tup);
}

namespace Prelude {
    template<typename t119, typename t120, typename t121, typename t122, typename t123>
    t122 apply3(juniper::function<t123, t122(t119, t120, t121)> f, juniper::tuple3<t119,t120,t121> tup);
}

namespace Prelude {
    template<typename t136, typename t137, typename t138, typename t139, typename t140, typename t141>
    t140 apply4(juniper::function<t141, t140(t136, t137, t138, t139)> f, juniper::tuple4<t136,t137,t138,t139> tup);
}

namespace Prelude {
    template<typename t157, typename t158>
    t157 fst(juniper::tuple2<t157,t158> tup);
}

namespace Prelude {
    template<typename t162, typename t163>
    t163 snd(juniper::tuple2<t162,t163> tup);
}

namespace Prelude {
    template<typename t167>
    t167 add(t167 numA, t167 numB);
}

namespace Prelude {
    template<typename t169>
    t169 sub(t169 numA, t169 numB);
}

namespace Prelude {
    template<typename t171>
    t171 mul(t171 numA, t171 numB);
}

namespace Prelude {
    template<typename t173>
    t173 div(t173 numA, t173 numB);
}

namespace Prelude {
    template<typename t176, typename t177>
    juniper::tuple2<t177,t176> swap(juniper::tuple2<t176,t177> tup);
}

namespace Prelude {
    template<typename t181, typename t182, typename t183>
    t181 until(juniper::function<t182, bool(t181)> p, juniper::function<t183, t181(t181)> f, t181 a0);
}

namespace Prelude {
    template<typename t191>
    juniper::unit ignore(t191 val);
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
    template<typename t249>
    uint8_t toUInt8(t249 n);
}

namespace Prelude {
    template<typename t251>
    int8_t toInt8(t251 n);
}

namespace Prelude {
    template<typename t253>
    uint16_t toUInt16(t253 n);
}

namespace Prelude {
    template<typename t255>
    int16_t toInt16(t255 n);
}

namespace Prelude {
    template<typename t257>
    uint32_t toUInt32(t257 n);
}

namespace Prelude {
    template<typename t259>
    int32_t toInt32(t259 n);
}

namespace Prelude {
    template<typename t261>
    float toFloat(t261 n);
}

namespace Prelude {
    template<typename t263>
    double toDouble(t263 n);
}

namespace Prelude {
    template<typename t265>
    t265 fromUInt8(uint8_t n);
}

namespace Prelude {
    template<typename t267>
    t267 fromInt8(int8_t n);
}

namespace Prelude {
    template<typename t269>
    t269 fromUInt16(uint16_t n);
}

namespace Prelude {
    template<typename t271>
    t271 fromInt16(int16_t n);
}

namespace Prelude {
    template<typename t273>
    t273 fromUInt32(uint32_t n);
}

namespace Prelude {
    template<typename t275>
    t275 fromInt32(int32_t n);
}

namespace Prelude {
    template<typename t277>
    t277 fromFloat(float n);
}

namespace Prelude {
    template<typename t279>
    t279 fromDouble(double n);
}

namespace Prelude {
    template<typename t281, typename t282>
    t282 cast(t281 x);
}

namespace List {
    template<typename t289, typename t285, typename t286, int c2>
    juniper::records::recordt_0<juniper::array<t285, c2>, uint32_t> map(juniper::function<t286, t285(t289)> f, juniper::records::recordt_0<juniper::array<t289, c2>, uint32_t> lst);
}

namespace List {
    template<typename t301, typename t297, typename t298, int c5>
    t297 foldl(juniper::function<t298, t297(t301, t297)> f, t297 initState, juniper::records::recordt_0<juniper::array<t301, c5>, uint32_t> lst);
}

namespace List {
    template<typename t312, typename t308, typename t309, int c7>
    t308 foldr(juniper::function<t309, t308(t312, t308)> f, t308 initState, juniper::records::recordt_0<juniper::array<t312, c7>, uint32_t> lst);
}

namespace List {
    template<typename t328, int c11, int c13, int c14>
    juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t> append(juniper::records::recordt_0<juniper::array<t328, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t328, c13>, uint32_t> lstB);
}

namespace List {
    template<typename t332, int c16>
    t332 nth(uint32_t i, juniper::records::recordt_0<juniper::array<t332, c16>, uint32_t> lst);
}

namespace List {
    template<typename t345, int c21, int c20>
    juniper::records::recordt_0<juniper::array<t345, (c21)*(c20)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t345, c21>, uint32_t>, c20>, uint32_t> listOfLists);
}

namespace List {
    template<typename t351, int c25, int c24>
    juniper::records::recordt_0<juniper::array<t351, c24>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t351, c25>, uint32_t> lst);
}

namespace List {
    template<typename t359, typename t356, int c28>
    bool all(juniper::function<t356, bool(t359)> pred, juniper::records::recordt_0<juniper::array<t359, c28>, uint32_t> lst);
}

namespace List {
    template<typename t368, typename t365, int c30>
    bool any(juniper::function<t365, bool(t368)> pred, juniper::records::recordt_0<juniper::array<t368, c30>, uint32_t> lst);
}

namespace List {
    template<typename t373, int c32>
    juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> pushBack(t373 elem, juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> lst);
}

namespace List {
    template<typename t383, int c36>
    juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> pushOffFront(t383 elem, juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> lst);
}

namespace List {
    template<typename t395, int c38>
    juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> setNth(uint32_t index, t395 elem, juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> lst);
}

namespace List {
    template<typename t400, int c39>
    juniper::records::recordt_0<juniper::array<t400, c39>, uint32_t> replicate(uint32_t numOfElements, t400 elem);
}

namespace List {
    template<typename t410, int c43>
    juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> remove(t410 elem, juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> lst);
}

namespace List {
    template<typename t414, int c44>
    juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t> lst);
}

namespace List {
    template<typename t423, typename t420, int c46>
    juniper::unit foreach(juniper::function<t420, juniper::unit(t423)> f, juniper::records::recordt_0<juniper::array<t423, c46>, uint32_t> lst);
}

namespace List {
    template<typename t433, int c48>
    t433 last(juniper::records::recordt_0<juniper::array<t433, c48>, uint32_t> lst);
}

namespace List {
    template<typename t443, int c52>
    t443 max_(juniper::records::recordt_0<juniper::array<t443, c52>, uint32_t> lst);
}

namespace List {
    template<typename t453, int c56>
    t453 min_(juniper::records::recordt_0<juniper::array<t453, c56>, uint32_t> lst);
}

namespace List {
    template<typename t455, int c58>
    bool member(t455 elem, juniper::records::recordt_0<juniper::array<t455, c58>, uint32_t> lst);
}

namespace List {
    template<typename t467, typename t469, int c62>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t467, c62>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t469, c62>, uint32_t> lstB);
}

namespace List {
    template<typename t480, typename t481, int c66>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t>,juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t480,t481>, c66>, uint32_t> lst);
}

namespace List {
    template<typename t490, int c67>
    t490 sum(juniper::records::recordt_0<juniper::array<t490, c67>, uint32_t> lst);
}

namespace List {
    template<typename t502, int c68>
    t502 average(juniper::records::recordt_0<juniper::array<t502, c68>, uint32_t> lst);
}

namespace Signal {
    template<typename t508, typename t509, typename t510>
    Prelude::sig<t509> map(juniper::function<t510, t509(t508)> f, Prelude::sig<t508> s);
}

namespace Signal {
    template<typename t526, typename t527>
    juniper::unit sink(juniper::function<t527, juniper::unit(t526)> f, Prelude::sig<t526> s);
}

namespace Signal {
    template<typename t532, typename t533>
    Prelude::sig<t532> filter(juniper::function<t533, bool(t532)> f, Prelude::sig<t532> s);
}

namespace Signal {
    template<typename t548>
    Prelude::sig<t548> merge(Prelude::sig<t548> sigA, Prelude::sig<t548> sigB);
}

namespace Signal {
    template<typename t550, int c69>
    Prelude::sig<t550> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t550>, c69>, uint32_t> sigs);
}

namespace Signal {
    template<typename t567, typename t568>
    Prelude::sig<Prelude::either<t567, t568>> join(Prelude::sig<t567> sigA, Prelude::sig<t568> sigB);
}

namespace Signal {
    template<typename t598>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t598> s);
}

namespace Signal {
    template<typename t604, typename t612, typename t606>
    Prelude::sig<t612> foldP(juniper::function<t606, t612(t604, t612)> f, juniper::shared_ptr<t612> state0, Prelude::sig<t604> incoming);
}

namespace Signal {
    template<typename t628>
    Prelude::sig<t628> dropRepeats(juniper::shared_ptr<Prelude::maybe<t628>> maybePrevValue, Prelude::sig<t628> incoming);
}

namespace Signal {
    template<typename t640>
    Prelude::sig<t640> latch(juniper::shared_ptr<t640> prevValue, Prelude::sig<t640> incoming);
}

namespace Signal {
    template<typename t655, typename t659, typename t651, typename t652>
    Prelude::sig<t651> map2(juniper::function<t652, t651(t655, t659)> f, juniper::shared_ptr<juniper::tuple2<t655,t659>> state, Prelude::sig<t655> incomingA, Prelude::sig<t659> incomingB);
}

namespace Signal {
    template<typename t680, int c71>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>> pastValues, Prelude::sig<t680> incoming);
}

namespace Signal {
    template<typename t690>
    Prelude::sig<t690> constant(t690 val);
}

namespace Signal {
    template<typename t698>
    Prelude::sig<Prelude::maybe<t698>> meta(Prelude::sig<t698> sigA);
}

namespace Signal {
    template<typename t703>
    Prelude::sig<t703> unmeta(Prelude::sig<Prelude::maybe<t703>> sigA);
}

namespace Signal {
    template<typename t715, typename t716>
    Prelude::sig<juniper::tuple2<t715,t716>> zip(juniper::shared_ptr<juniper::tuple2<t715,t716>> state, Prelude::sig<t715> sigA, Prelude::sig<t716> sigB);
}

namespace Signal {
    template<typename t747, typename t754>
    juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>> unzip(Prelude::sig<juniper::tuple2<t747,t754>> incoming);
}

namespace Signal {
    template<typename t761, typename t762>
    Prelude::sig<t761> toggle(t761 val1, t761 val2, juniper::shared_ptr<t761> state, Prelude::sig<t762> incoming);
}

namespace Io {
    Io::pinState toggle(Io::pinState p);
}

namespace Io {
    juniper::unit printStr(const char * str);
}

namespace Io {
    template<int c72>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c72>, uint32_t> cl);
}

namespace Io {
    juniper::unit printFloat(float f);
}

namespace Io {
    juniper::unit printInt(int32_t n);
}

namespace Io {
    template<typename t787>
    t787 baseToInt(Io::base b);
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
    template<typename t902, typename t903, typename t904>
    Prelude::maybe<t903> map(juniper::function<t904, t903(t902)> f, Prelude::maybe<t902> maybeVal);
}

namespace Maybe {
    template<typename t914>
    t914 get(Prelude::maybe<t914> maybeVal);
}

namespace Maybe {
    template<typename t916>
    bool isJust(Prelude::maybe<t916> maybeVal);
}

namespace Maybe {
    template<typename t918>
    bool isNothing(Prelude::maybe<t918> maybeVal);
}

namespace Maybe {
    template<typename t923>
    uint8_t count(Prelude::maybe<t923> maybeVal);
}

namespace Maybe {
    template<typename t927, typename t928, typename t929>
    t928 foldl(juniper::function<t929, t928(t927, t928)> f, t928 initState, Prelude::maybe<t927> maybeVal);
}

namespace Maybe {
    template<typename t935, typename t936, typename t937>
    t936 fodlr(juniper::function<t937, t936(t935, t936)> f, t936 initState, Prelude::maybe<t935> maybeVal);
}

namespace Maybe {
    template<typename t944, typename t945>
    juniper::unit iter(juniper::function<t945, juniper::unit(t944)> f, Prelude::maybe<t944> maybeVal);
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
    juniper::tuple2<double,int16_t> frexp_(double x);
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
    juniper::tuple2<double,double> modf_(double x);
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
    double min_(double x, double y);
}

namespace Math {
    double max_(double x, double y);
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2);
}

namespace Math {
    template<typename t1009>
    t1009 clamp(t1009 x, t1009 min, t1009 max);
}

namespace Math {
    template<typename t1014>
    int8_t sign(t1014 n);
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
    template<typename t1056, int c73>
    juniper::records::recordt_3<juniper::array<t1056, c73>> make(juniper::array<t1056, c73> d);
}

namespace Vector {
    template<typename t1058, int c75>
    t1058 get(uint32_t i, juniper::records::recordt_3<juniper::array<t1058, c75>> v);
}

namespace Vector {
    template<typename t1068, int c79>
    juniper::records::recordt_3<juniper::array<t1068, c79>> add(juniper::records::recordt_3<juniper::array<t1068, c79>> v1, juniper::records::recordt_3<juniper::array<t1068, c79>> v2);
}

namespace Vector {
    template<typename t1072, int c80>
    juniper::records::recordt_3<juniper::array<t1072, c80>> zero();
}

namespace Vector {
    template<typename t1081, int c84>
    juniper::records::recordt_3<juniper::array<t1081, c84>> subtract(juniper::records::recordt_3<juniper::array<t1081, c84>> v1, juniper::records::recordt_3<juniper::array<t1081, c84>> v2);
}

namespace Vector {
    template<typename t1089, int c87>
    juniper::records::recordt_3<juniper::array<t1089, c87>> scale(t1089 scalar, juniper::records::recordt_3<juniper::array<t1089, c87>> v);
}

namespace Vector {
    template<typename t1100, int c90>
    t1100 dot(juniper::records::recordt_3<juniper::array<t1100, c90>> v1, juniper::records::recordt_3<juniper::array<t1100, c90>> v2);
}

namespace Vector {
    template<typename t1109, int c93>
    t1109 magnitude2(juniper::records::recordt_3<juniper::array<t1109, c93>> v);
}

namespace Vector {
    template<typename t1111, int c94>
    double magnitude(juniper::records::recordt_3<juniper::array<t1111, c94>> v);
}

namespace Vector {
    template<typename t1125, int c98>
    juniper::records::recordt_3<juniper::array<t1125, c98>> multiply(juniper::records::recordt_3<juniper::array<t1125, c98>> u, juniper::records::recordt_3<juniper::array<t1125, c98>> v);
}

namespace Vector {
    template<typename t1136, int c101>
    juniper::records::recordt_3<juniper::array<t1136, c101>> normalize(juniper::records::recordt_3<juniper::array<t1136, c101>> v);
}

namespace Vector {
    template<typename t1149, int c102>
    double angle(juniper::records::recordt_3<juniper::array<t1149, c102>> v1, juniper::records::recordt_3<juniper::array<t1149, c102>> v2);
}

namespace Vector {
    template<typename t1167>
    juniper::records::recordt_3<juniper::array<t1167, 3>> cross(juniper::records::recordt_3<juniper::array<t1167, 3>> u, juniper::records::recordt_3<juniper::array<t1167, 3>> v);
}

namespace Vector {
    template<typename t1194, int c115>
    juniper::records::recordt_3<juniper::array<t1194, c115>> project(juniper::records::recordt_3<juniper::array<t1194, c115>> a, juniper::records::recordt_3<juniper::array<t1194, c115>> b);
}

namespace Vector {
    template<typename t1207, int c116>
    juniper::records::recordt_3<juniper::array<t1207, c116>> projectPlane(juniper::records::recordt_3<juniper::array<t1207, c116>> a, juniper::records::recordt_3<juniper::array<t1207, c116>> m);
}

namespace CharList {
    template<int c117>
    juniper::records::recordt_0<juniper::array<uint8_t, c117>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c117>, uint32_t> str);
}

namespace CharList {
    template<int c118>
    juniper::records::recordt_0<juniper::array<uint8_t, c118>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c118>, uint32_t> str);
}

namespace CharList {
    template<int c119>
    juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t> i32ToCharList(int32_t m);
}

namespace CharList {
    template<int c120>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c120>, uint32_t> s);
}

namespace CharList {
    template<int c126, int c128, int c129>
    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, c126>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, c128>, uint32_t> sB);
}

namespace CharList {
    template<int c130, int c131>
    juniper::records::recordt_0<juniper::array<uint8_t, ((c130)+(c131))-(1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, c131>, uint32_t> sB);
}

namespace Random {
    int32_t random_(int32_t low, int32_t high);
}

namespace Random {
    juniper::unit seed(uint32_t n);
}

namespace Random {
    template<typename t1271, int c135>
    t1271 choice(juniper::records::recordt_0<juniper::array<t1271, c135>, uint32_t> lst);
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_6<float, float, float> color);
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> color);
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
    template<int c137>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c137>, uint32_t> bytes);
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
    template<typename t1592>
    uint16_t writeGeneric(Ble::characterstict c, t1592 x);
}

namespace Ble {
    template<typename t1595>
    t1595 readGeneric(Ble::characterstict c);
}

namespace Ble {
    juniper::unit setCharacteristicPermission(Ble::characterstict c, Ble::secureModet readPermission, Ble::secureModet writePermission);
}

namespace Ble {
    juniper::unit setCharacteristicProperties(Ble::characterstict c, Ble::propertiest prop);
}

namespace Ble {
    template<typename t1605>
    juniper::unit setCharacteristicFixedLen(Ble::characterstict c);
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
    juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> secondTick(juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> d);
}

namespace CWatch {
    juniper::unit processBluetoothUpdates();
}

namespace Gfx {
    juniper::unit drawFastHLine565(int16_t x, int16_t y, int16_t w, uint16_t c);
}

namespace Gfx {
    juniper::unit drawVerticalGradient(int16_t x0i, int16_t y0i, int16_t w, int16_t h, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c1, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c2);
}

namespace Gfx {
    template<int c138>
    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, c138>, uint32_t> cl);
}

namespace Gfx {
    template<int c139>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c139>, uint32_t> cl);
}

namespace Gfx {
    juniper::unit setCursor(int16_t x, int16_t y);
}

namespace Gfx {
    juniper::unit setFont(Gfx::font f);
}

namespace Gfx {
    juniper::unit setTextColor(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c);
}

namespace Gfx {
    juniper::unit setTextSize(uint8_t size);
}

namespace CWatch {
    bool loop();
}

namespace Gfx {
    juniper::unit fillScreen(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c);
}

namespace Gfx {
    juniper::unit drawPixel(int16_t x, int16_t y, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c);
}

namespace Gfx {
    juniper::unit drawPixel565(uint16_t x, uint16_t y, uint16_t c);
}

namespace Gfx {
    juniper::unit printString(const char * s);
}

namespace Gfx {
    template<int c160>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c160>, uint32_t> cl);
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
    template<typename t7, typename t3, typename t4, typename t5, typename t6>
    juniper::function<juniper::closures::closuret_0<juniper::function<t5, t4(t3)>, juniper::function<t6, t3(t7)>>, t4(t7)> compose(juniper::function<t5, t4(t3)> f, juniper::function<t6, t3(t7)> g) {
        return (([&]() -> juniper::function<juniper::closures::closuret_0<juniper::function<t5, t4(t3)>, juniper::function<t6, t3(t7)>>, t4(t7)> {
            using a = t7;
            using b = t3;
            using c = t4;
            using closureF = t5;
            using closureG = t6;
            return juniper::function<juniper::closures::closuret_0<juniper::function<t5, t4(t3)>, juniper::function<t6, t3(t7)>>, t4(t7)>(juniper::closures::closuret_0<juniper::function<t5, t4(t3)>, juniper::function<t6, t3(t7)>>(f, g), [](juniper::closures::closuret_0<juniper::function<t5, t4(t3)>, juniper::function<t6, t3(t7)>>& junclosure, t7 x) -> t4 { 
                juniper::function<t5, t4(t3)>& f = junclosure.f;
                juniper::function<t6, t3(t7)>& g = junclosure.g;
                return f(g(x));
             });
        })());
    }
}

namespace Prelude {
    template<typename t19, typename t20, typename t17, typename t18>
    juniper::function<juniper::closures::closuret_1<juniper::function<t18, t17(t19, t20)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>, t17(t20)>(t19)> curry(juniper::function<t18, t17(t19, t20)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t18, t17(t19, t20)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>, t17(t20)>(t19)> {
            using a = t19;
            using b = t20;
            using c = t17;
            using closure = t18;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t18, t17(t19, t20)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>, t17(t20)>(t19)>(juniper::closures::closuret_1<juniper::function<t18, t17(t19, t20)>>(f), [](juniper::closures::closuret_1<juniper::function<t18, t17(t19, t20)>>& junclosure, t19 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>, t17(t20)> { 
                juniper::function<t18, t17(t19, t20)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>, t17(t20)>(juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t18, t17(t19, t20)>, t19>& junclosure, t20 valueB) -> t17 { 
                    juniper::function<t18, t17(t19, t20)>& f = junclosure.f;
                    t19& valueA = junclosure.valueA;
                    return f(valueA, valueB);
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t31, typename t32, typename t28, typename t29, typename t30>
    juniper::function<juniper::closures::closuret_1<juniper::function<t29, juniper::function<t30, t28(t32)>(t31)>>, t28(t31, t32)> uncurry(juniper::function<t29, juniper::function<t30, t28(t32)>(t31)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t29, juniper::function<t30, t28(t32)>(t31)>>, t28(t31, t32)> {
            using a = t31;
            using b = t32;
            using c = t28;
            using closureA = t29;
            using closureB = t30;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t29, juniper::function<t30, t28(t32)>(t31)>>, t28(t31,t32)>(juniper::closures::closuret_1<juniper::function<t29, juniper::function<t30, t28(t32)>(t31)>>(f), [](juniper::closures::closuret_1<juniper::function<t29, juniper::function<t30, t28(t32)>(t31)>>& junclosure, t31 valueA, t32 valueB) -> t28 { 
                juniper::function<t29, juniper::function<t30, t28(t32)>(t31)>& f = junclosure.f;
                return f(valueA)(valueB);
             });
        })());
    }
}

namespace Prelude {
    template<typename t45, typename t46, typename t47, typename t43, typename t44>
    juniper::function<juniper::closures::closuret_1<juniper::function<t44, t43(t45, t46, t47)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>, juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)>(t46)>(t45)> curry3(juniper::function<t44, t43(t45, t46, t47)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t44, t43(t45, t46, t47)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>, juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)>(t46)>(t45)> {
            using a = t45;
            using b = t46;
            using c = t47;
            using d = t43;
            using closureF = t44;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t44, t43(t45, t46, t47)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>, juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)>(t46)>(t45)>(juniper::closures::closuret_1<juniper::function<t44, t43(t45, t46, t47)>>(f), [](juniper::closures::closuret_1<juniper::function<t44, t43(t45, t46, t47)>>& junclosure, t45 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>, juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)>(t46)> { 
                juniper::function<t44, t43(t45, t46, t47)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>, juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)>(t46)>(juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t44, t43(t45, t46, t47)>, t45>& junclosure, t46 valueB) -> juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)> { 
                    juniper::function<t44, t43(t45, t46, t47)>& f = junclosure.f;
                    t45& valueA = junclosure.valueA;
                    return juniper::function<juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>, t43(t47)>(juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>(f, valueA, valueB), [](juniper::closures::closuret_3<juniper::function<t44, t43(t45, t46, t47)>, t45, t46>& junclosure, t47 valueC) -> t43 { 
                        juniper::function<t44, t43(t45, t46, t47)>& f = junclosure.f;
                        t45& valueA = junclosure.valueA;
                        t46& valueB = junclosure.valueB;
                        return f(valueA, valueB, valueC);
                     });
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t61, typename t62, typename t63, typename t57, typename t58, typename t59, typename t60>
    juniper::function<juniper::closures::closuret_1<juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)>>, t57(t61, t62, t63)> uncurry3(juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)>>, t57(t61, t62, t63)> {
            using a = t61;
            using b = t62;
            using c = t63;
            using d = t57;
            using closureA = t58;
            using closureB = t59;
            using closureC = t60;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)>>, t57(t61,t62,t63)>(juniper::closures::closuret_1<juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)>>(f), [](juniper::closures::closuret_1<juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)>>& junclosure, t61 valueA, t62 valueB, t63 valueC) -> t57 { 
                juniper::function<t58, juniper::function<t59, juniper::function<t60, t57(t63)>(t62)>(t61)>& f = junclosure.f;
                return f(valueA)(valueB)(valueC);
             });
        })());
    }
}

namespace Prelude {
    template<typename t74>
    bool eq(t74 x, t74 y) {
        return (([&]() -> bool {
            using a = t74;
            return (x == y);
        })());
    }
}

namespace Prelude {
    template<typename t77>
    bool neq(t77 x, t77 y) {
        return (x != y);
    }
}

namespace Prelude {
    template<typename t79, typename t80>
    bool gt(t79 x, t80 y) {
        return (x > y);
    }
}

namespace Prelude {
    template<typename t82, typename t83>
    bool geq(t82 x, t83 y) {
        return (x >= y);
    }
}

namespace Prelude {
    template<typename t85, typename t86>
    bool lt(t85 x, t86 y) {
        return (x < y);
    }
}

namespace Prelude {
    template<typename t88, typename t89>
    bool leq(t88 x, t89 y) {
        return (x <= y);
    }
}

namespace Prelude {
    bool notf(bool x) {
        return !(x);
    }
}

namespace Prelude {
    bool andf(bool x, bool y) {
        return (x && y);
    }
}

namespace Prelude {
    bool orf(bool x, bool y) {
        return (x || y);
    }
}

namespace Prelude {
    template<typename t99, typename t100, typename t101>
    t100 apply(juniper::function<t101, t100(t99)> f, t99 x) {
        return (([&]() -> t100 {
            using a = t99;
            using b = t100;
            using closure = t101;
            return f(x);
        })());
    }
}

namespace Prelude {
    template<typename t106, typename t107, typename t108, typename t109>
    t108 apply2(juniper::function<t109, t108(t106, t107)> f, juniper::tuple2<t106,t107> tup) {
        return (([&]() -> t108 {
            using a = t106;
            using b = t107;
            using c = t108;
            using closure = t109;
            return (([&]() -> t108 {
                juniper::tuple2<t106,t107> guid0 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t107 b = (guid0).e2;
                t106 a = (guid0).e1;
                
                return f(a, b);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t119, typename t120, typename t121, typename t122, typename t123>
    t122 apply3(juniper::function<t123, t122(t119, t120, t121)> f, juniper::tuple3<t119,t120,t121> tup) {
        return (([&]() -> t122 {
            using a = t119;
            using b = t120;
            using c = t121;
            using d = t122;
            using closure = t123;
            return (([&]() -> t122 {
                juniper::tuple3<t119,t120,t121> guid1 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t121 c = (guid1).e3;
                t120 b = (guid1).e2;
                t119 a = (guid1).e1;
                
                return f(a, b, c);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t136, typename t137, typename t138, typename t139, typename t140, typename t141>
    t140 apply4(juniper::function<t141, t140(t136, t137, t138, t139)> f, juniper::tuple4<t136,t137,t138,t139> tup) {
        return (([&]() -> t140 {
            using a = t136;
            using b = t137;
            using c = t138;
            using d = t139;
            using e = t140;
            using closure = t141;
            return (([&]() -> t140 {
                juniper::tuple4<t136,t137,t138,t139> guid2 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t139 d = (guid2).e4;
                t138 c = (guid2).e3;
                t137 b = (guid2).e2;
                t136 a = (guid2).e1;
                
                return f(a, b, c, d);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t157, typename t158>
    t157 fst(juniper::tuple2<t157,t158> tup) {
        return (([&]() -> t157 {
            using a = t157;
            using b = t158;
            return (([&]() -> t157 {
                juniper::tuple2<t157,t158> guid3 = tup;
                return (true ? 
                    (([&]() -> t157 {
                        t157 x = (guid3).e1;
                        return x;
                    })())
                :
                    juniper::quit<t157>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t162, typename t163>
    t163 snd(juniper::tuple2<t162,t163> tup) {
        return (([&]() -> t163 {
            using a = t162;
            using b = t163;
            return (([&]() -> t163 {
                juniper::tuple2<t162,t163> guid4 = tup;
                return (true ? 
                    (([&]() -> t163 {
                        t163 x = (guid4).e2;
                        return x;
                    })())
                :
                    juniper::quit<t163>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t167>
    t167 add(t167 numA, t167 numB) {
        return (([&]() -> t167 {
            using a = t167;
            return (numA + numB);
        })());
    }
}

namespace Prelude {
    template<typename t169>
    t169 sub(t169 numA, t169 numB) {
        return (([&]() -> t169 {
            using a = t169;
            return (numA - numB);
        })());
    }
}

namespace Prelude {
    template<typename t171>
    t171 mul(t171 numA, t171 numB) {
        return (([&]() -> t171 {
            using a = t171;
            return (numA * numB);
        })());
    }
}

namespace Prelude {
    template<typename t173>
    t173 div(t173 numA, t173 numB) {
        return (([&]() -> t173 {
            using a = t173;
            return (numA / numB);
        })());
    }
}

namespace Prelude {
    template<typename t176, typename t177>
    juniper::tuple2<t177,t176> swap(juniper::tuple2<t176,t177> tup) {
        return (([&]() -> juniper::tuple2<t177,t176> {
            juniper::tuple2<t176,t177> guid5 = tup;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            t177 beta = (guid5).e2;
            t176 alpha = (guid5).e1;
            
            return (juniper::tuple2<t177,t176>{beta, alpha});
        })());
    }
}

namespace Prelude {
    template<typename t181, typename t182, typename t183>
    t181 until(juniper::function<t182, bool(t181)> p, juniper::function<t183, t181(t181)> f, t181 a0) {
        return (([&]() -> t181 {
            using a = t181;
            using closureP = t182;
            using closureF = t183;
            return (([&]() -> t181 {
                t181 guid6 = a0;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t181 a = guid6;
                
                (([&]() -> juniper::unit {
                    while (!(p(a))) {
                        (([&]() -> juniper::unit {
                            (a = f(a));
                            return juniper::unit();
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
    template<typename t191>
    juniper::unit ignore(t191 val) {
        return (([&]() -> juniper::unit {
            using a = t191;
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
    template<typename t249>
    uint8_t toUInt8(t249 n) {
        return (([&]() -> uint8_t {
            using t = t249;
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
    template<typename t251>
    int8_t toInt8(t251 n) {
        return (([&]() -> int8_t {
            using t = t251;
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
    template<typename t253>
    uint16_t toUInt16(t253 n) {
        return (([&]() -> uint16_t {
            using t = t253;
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
    template<typename t255>
    int16_t toInt16(t255 n) {
        return (([&]() -> int16_t {
            using t = t255;
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
    template<typename t257>
    uint32_t toUInt32(t257 n) {
        return (([&]() -> uint32_t {
            using t = t257;
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
    template<typename t259>
    int32_t toInt32(t259 n) {
        return (([&]() -> int32_t {
            using t = t259;
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
    template<typename t261>
    float toFloat(t261 n) {
        return (([&]() -> float {
            using t = t261;
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
    template<typename t263>
    double toDouble(t263 n) {
        return (([&]() -> double {
            using t = t263;
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
    template<typename t265>
    t265 fromUInt8(uint8_t n) {
        return (([&]() -> t265 {
            using t = t265;
            return (([&]() -> t265 {
                t265 ret;
                
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
    template<typename t267>
    t267 fromInt8(int8_t n) {
        return (([&]() -> t267 {
            using t = t267;
            return (([&]() -> t267 {
                t267 ret;
                
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
    template<typename t269>
    t269 fromUInt16(uint16_t n) {
        return (([&]() -> t269 {
            using t = t269;
            return (([&]() -> t269 {
                t269 ret;
                
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
    template<typename t271>
    t271 fromInt16(int16_t n) {
        return (([&]() -> t271 {
            using t = t271;
            return (([&]() -> t271 {
                t271 ret;
                
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
    template<typename t273>
    t273 fromUInt32(uint32_t n) {
        return (([&]() -> t273 {
            using t = t273;
            return (([&]() -> t273 {
                t273 ret;
                
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
    template<typename t275>
    t275 fromInt32(int32_t n) {
        return (([&]() -> t275 {
            using t = t275;
            return (([&]() -> t275 {
                t275 ret;
                
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
    template<typename t277>
    t277 fromFloat(float n) {
        return (([&]() -> t277 {
            using t = t277;
            return (([&]() -> t277 {
                t277 ret;
                
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
    template<typename t279>
    t279 fromDouble(double n) {
        return (([&]() -> t279 {
            using t = t279;
            return (([&]() -> t279 {
                t279 ret;
                
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
    template<typename t281, typename t282>
    t282 cast(t281 x) {
        return (([&]() -> t282 {
            using a = t281;
            using b = t282;
            return (([&]() -> t282 {
                t282 ret;
                
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
    template<typename t289, typename t285, typename t286, int c2>
    juniper::records::recordt_0<juniper::array<t285, c2>, uint32_t> map(juniper::function<t286, t285(t289)> f, juniper::records::recordt_0<juniper::array<t289, c2>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t285, c2>, uint32_t> {
            constexpr int32_t n = c2;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t285, c2>, uint32_t> {
                juniper::array<t285, c2> guid7 = (juniper::array<t285, c2>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t285, c2> ret = guid7;
                
                (([&]() -> juniper::unit {
                    uint32_t guid8 = ((uint32_t) 0);
                    uint32_t guid9 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid8; i <= guid9; i++) {
                        (([&]() -> juniper::unit {
                            ((ret)[i] = f(((lst).data)[i]));
                            return juniper::unit();
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t285, c2>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t285, c2>, uint32_t> guid10;
                    guid10.data = ret;
                    guid10.length = (lst).length;
                    return guid10;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t301, typename t297, typename t298, int c5>
    t297 foldl(juniper::function<t298, t297(t301, t297)> f, t297 initState, juniper::records::recordt_0<juniper::array<t301, c5>, uint32_t> lst) {
        return (([&]() -> t297 {
            constexpr int32_t n = c5;
            return (([&]() -> t297 {
                t297 guid11 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t297 s = guid11;
                
                (([&]() -> juniper::unit {
                    uint32_t guid12 = ((uint32_t) 0);
                    uint32_t guid13 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid12; i <= guid13; i++) {
                        (([&]() -> juniper::unit {
                            (s = f(((lst).data)[i], s));
                            return juniper::unit();
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
    template<typename t312, typename t308, typename t309, int c7>
    t308 foldr(juniper::function<t309, t308(t312, t308)> f, t308 initState, juniper::records::recordt_0<juniper::array<t312, c7>, uint32_t> lst) {
        return (([&]() -> t308 {
            constexpr int32_t n = c7;
            return (([&]() -> t308 {
                t308 guid14 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t308 s = guid14;
                
                (([&]() -> juniper::unit {
                    uint32_t guid15 = ((lst).length - ((uint32_t) 1));
                    uint32_t guid16 = ((uint32_t) 0);
                    for (uint32_t i = guid15; i >= guid16; i--) {
                        (([&]() -> juniper::unit {
                            (s = f(((lst).data)[i], s));
                            return juniper::unit();
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
    template<typename t328, int c11, int c13, int c14>
    juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t> append(juniper::records::recordt_0<juniper::array<t328, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t328, c13>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t> {
            constexpr int32_t aCap = c11;
            constexpr int32_t bCap = c13;
            constexpr int32_t retCap = c14;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t> {
                uint32_t guid17 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid17;
                
                juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t> guid18 = (([&]() -> juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t> guid19;
                    guid19.data = (juniper::array<t328, c14>());
                    guid19.length = ((lstA).length + (lstB).length);
                    return guid19;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t328, c14>, uint32_t> out = guid18;
                
                (([&]() -> juniper::unit {
                    uint32_t guid20 = ((uint32_t) 0);
                    uint32_t guid21 = ((lstA).length - ((uint32_t) 1));
                    for (uint32_t i = guid20; i <= guid21; i++) {
                        (([&]() -> juniper::unit {
                            (((out).data)[j] = ((lstA).data)[i]);
                            (j = (j + ((uint32_t) 1)));
                            return juniper::unit();
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid22 = ((uint32_t) 0);
                    uint32_t guid23 = ((lstB).length - ((uint32_t) 1));
                    for (uint32_t i = guid22; i <= guid23; i++) {
                        (([&]() -> juniper::unit {
                            (((out).data)[j] = ((lstB).data)[i]);
                            (j = (j + ((uint32_t) 1)));
                            return juniper::unit();
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
    template<typename t332, int c16>
    t332 nth(uint32_t i, juniper::records::recordt_0<juniper::array<t332, c16>, uint32_t> lst) {
        return (([&]() -> t332 {
            constexpr int32_t n = c16;
            return ((i < (lst).length) ? 
                ((lst).data)[i]
            :
                juniper::quit<t332>());
        })());
    }
}

namespace List {
    template<typename t345, int c21, int c20>
    juniper::records::recordt_0<juniper::array<t345, (c21)*(c20)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t345, c21>, uint32_t>, c20>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t345, (c21)*(c20)>, uint32_t> {
            constexpr int32_t m = c21;
            constexpr int32_t n = c20;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t345, (c21)*(c20)>, uint32_t> {
                juniper::array<t345, (c21)*(c20)> guid24 = (juniper::array<t345, (c21)*(c20)>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t345, (c21)*(c20)> ret = guid24;
                
                uint32_t guid25 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid25;
                
                (([&]() -> juniper::unit {
                    uint32_t guid26 = ((uint32_t) 0);
                    uint32_t guid27 = ((listOfLists).length - ((uint32_t) 1));
                    for (uint32_t i = guid26; i <= guid27; i++) {
                        (([&]() -> juniper::unit {
                            uint32_t guid28 = ((uint32_t) 0);
                            uint32_t guid29 = ((((listOfLists).data)[i]).length - ((uint32_t) 1));
                            for (uint32_t j = guid28; j <= guid29; j++) {
                                (([&]() -> juniper::unit {
                                    ((ret)[index] = ((((listOfLists).data)[i]).data)[j]);
                                    (index = (index + ((uint32_t) 1)));
                                    return juniper::unit();
                                })());
                            }
                            return {};
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t345, (c21)*(c20)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t345, (c21)*(c20)>, uint32_t> guid30;
                    guid30.data = ret;
                    guid30.length = index;
                    return guid30;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t351, int c25, int c24>
    juniper::records::recordt_0<juniper::array<t351, c24>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t351, c25>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t351, c24>, uint32_t> {
            constexpr int32_t n = c25;
            constexpr int32_t m = c24;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t351, c24>, uint32_t> {
                juniper::array<t351, c24> guid31 = (juniper::array<t351, c24>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t351, c24> ret = guid31;
                
                (([&]() -> juniper::unit {
                    uint32_t guid32 = ((uint32_t) 0);
                    uint32_t guid33 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid32; i <= guid33; i++) {
                        (([&]() -> juniper::unit {
                            ((ret)[i] = ((lst).data)[i]);
                            return juniper::unit();
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t351, c24>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t351, c24>, uint32_t> guid34;
                    guid34.data = ret;
                    guid34.length = (lst).length;
                    return guid34;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t359, typename t356, int c28>
    bool all(juniper::function<t356, bool(t359)> pred, juniper::records::recordt_0<juniper::array<t359, c28>, uint32_t> lst) {
        return (([&]() -> bool {
            constexpr int32_t n = c28;
            return (([&]() -> bool {
                bool guid35 = true;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid35;
                
                (([&]() -> juniper::unit {
                    uint32_t guid36 = ((uint32_t) 0);
                    uint32_t guid37 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid36; i <= guid37; i++) {
                        (satisfied ? 
                            (([&]() -> juniper::unit {
                                (satisfied = pred(((lst).data)[i]));
                                return juniper::unit();
                            })())
                        :
                            juniper::unit());
                    }
                    return {};
                })());
                return satisfied;
            })());
        })());
    }
}

namespace List {
    template<typename t368, typename t365, int c30>
    bool any(juniper::function<t365, bool(t368)> pred, juniper::records::recordt_0<juniper::array<t368, c30>, uint32_t> lst) {
        return (([&]() -> bool {
            constexpr int32_t n = c30;
            return (([&]() -> bool {
                bool guid38 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid38;
                
                (([&]() -> juniper::unit {
                    uint32_t guid39 = ((uint32_t) 0);
                    uint32_t guid40 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid39; i <= guid40; i++) {
                        (!(satisfied) ? 
                            (([&]() -> juniper::unit {
                                (satisfied = pred(((lst).data)[i]));
                                return juniper::unit();
                            })())
                        :
                            juniper::unit());
                    }
                    return {};
                })());
                return satisfied;
            })());
        })());
    }
}

namespace List {
    template<typename t373, int c32>
    juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> pushBack(t373 elem, juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> {
            constexpr int32_t n = c32;
            return (((lst).length >= n) ? 
                juniper::quit<juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t>>()
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> guid41 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t373, c32>, uint32_t> ret = guid41;
                    
                    (((ret).data)[(lst).length] = elem);
                    ((ret).length = ((lst).length + ((uint32_t) 1)));
                    return ret;
                })()));
        })());
    }
}

namespace List {
    template<typename t383, int c36>
    juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> pushOffFront(t383 elem, juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> {
            constexpr int32_t n = c36;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> guid42 = lst;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> ret = guid42;
                
                (([&]() -> juniper::unit {
                    int32_t guid43 = (n - ((int32_t) 2));
                    int32_t guid44 = ((int32_t) 0);
                    for (int32_t i = guid43; i >= guid44; i--) {
                        (([&]() -> juniper::unit {
                            (((ret).data)[(i + ((int32_t) 1))] = ((ret).data)[i]);
                            return juniper::unit();
                        })());
                    }
                    return {};
                })());
                (((ret).data)[((uint32_t) 0)] = elem);
                return (((ret).length == i32ToU32(n)) ? 
                    ret
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t383, c36>, uint32_t> {
                        ((ret).length = ((lst).length + ((uint32_t) 1)));
                        return ret;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t395, int c38>
    juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> setNth(uint32_t index, t395 elem, juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> {
            constexpr int32_t n = c38;
            return (((lst).length <= index) ? 
                juniper::quit<juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t>>()
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> guid45 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t395, c38>, uint32_t> ret = guid45;
                    
                    (((ret).data)[index] = elem);
                    return ret;
                })()));
        })());
    }
}

namespace List {
    template<typename t400, int c39>
    juniper::records::recordt_0<juniper::array<t400, c39>, uint32_t> replicate(uint32_t numOfElements, t400 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t400, c39>, uint32_t> {
            constexpr int32_t n = c39;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t400, c39>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t400, c39>, uint32_t> guid46;
                guid46.data = (juniper::array<t400, c39>().fill(elem));
                guid46.length = numOfElements;
                return guid46;
            })());
        })());
    }
}

namespace List {
    template<typename t410, int c43>
    juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> remove(t410 elem, juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> {
            constexpr int32_t n = c43;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> {
                uint32_t guid47 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid47;
                
                bool guid48 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid48;
                
                (([&]() -> juniper::unit {
                    uint32_t guid49 = ((uint32_t) 0);
                    uint32_t guid50 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid49; i <= guid50; i++) {
                        ((!(found) && (((lst).data)[i] == elem)) ? 
                            (([&]() -> juniper::unit {
                                (index = i);
                                (found = true);
                                return juniper::unit();
                            })())
                        :
                            juniper::unit());
                    }
                    return {};
                })());
                return (found ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> guid51 = lst;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_0<juniper::array<t410, c43>, uint32_t> ret = guid51;
                        
                        ((ret).length = ((lst).length - ((uint32_t) 1)));
                        (([&]() -> juniper::unit {
                            uint32_t guid52 = index;
                            uint32_t guid53 = ((lst).length - ((uint32_t) 2));
                            for (uint32_t i = guid52; i <= guid53; i++) {
                                (([&]() -> juniper::unit {
                                    (((ret).data)[i] = ((lst).data)[(i + ((uint32_t) 1))]);
                                    return juniper::unit();
                                })());
                            }
                            return {};
                        })());
                        return ret;
                    })())
                :
                    lst);
            })());
        })());
    }
}

namespace List {
    template<typename t414, int c44>
    juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t> {
            constexpr int32_t n = c44;
            return (((lst).length == ((uint32_t) 0)) ? 
                juniper::quit<juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t>>()
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t414, c44>, uint32_t> guid54;
                    guid54.data = (lst).data;
                    guid54.length = ((lst).length - ((uint32_t) 1));
                    return guid54;
                })()));
        })());
    }
}

namespace List {
    template<typename t423, typename t420, int c46>
    juniper::unit foreach(juniper::function<t420, juniper::unit(t423)> f, juniper::records::recordt_0<juniper::array<t423, c46>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c46;
            return (([&]() -> juniper::unit {
                uint32_t guid55 = ((uint32_t) 0);
                uint32_t guid56 = ((lst).length - ((uint32_t) 1));
                for (uint32_t i = guid55; i <= guid56; i++) {
                    f(((lst).data)[i]);
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t433, int c48>
    t433 last(juniper::records::recordt_0<juniper::array<t433, c48>, uint32_t> lst) {
        return (([&]() -> t433 {
            constexpr int32_t n = c48;
            return (((lst).length == ((uint32_t) 0)) ? 
                juniper::quit<t433>()
            :
                ((lst).data)[((lst).length - ((uint32_t) 1))]);
        })());
    }
}

namespace List {
    template<typename t443, int c52>
    t443 max_(juniper::records::recordt_0<juniper::array<t443, c52>, uint32_t> lst) {
        return (([&]() -> t443 {
            constexpr int32_t n = c52;
            return ((((lst).length == ((uint32_t) 0)) || (n == ((int32_t) 0))) ? 
                juniper::quit<t443>()
            :
                (([&]() -> t443 {
                    t443 guid57 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t443 maxVal = guid57;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid58 = ((uint32_t) 1);
                        uint32_t guid59 = ((lst).length - ((uint32_t) 1));
                        for (uint32_t i = guid58; i <= guid59; i++) {
                            ((((lst).data)[i] > maxVal) ? 
                                (([&]() -> juniper::unit {
                                    (maxVal = ((lst).data)[i]);
                                    return juniper::unit();
                                })())
                            :
                                juniper::unit());
                        }
                        return {};
                    })());
                    return maxVal;
                })()));
        })());
    }
}

namespace List {
    template<typename t453, int c56>
    t453 min_(juniper::records::recordt_0<juniper::array<t453, c56>, uint32_t> lst) {
        return (([&]() -> t453 {
            constexpr int32_t n = c56;
            return ((((lst).length == ((uint32_t) 0)) || (n == ((int32_t) 0))) ? 
                juniper::quit<t453>()
            :
                (([&]() -> t453 {
                    t453 guid60 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t453 minVal = guid60;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid61 = ((uint32_t) 1);
                        uint32_t guid62 = ((lst).length - ((uint32_t) 1));
                        for (uint32_t i = guid61; i <= guid62; i++) {
                            ((((lst).data)[i] < minVal) ? 
                                (([&]() -> juniper::unit {
                                    (minVal = ((lst).data)[i]);
                                    return juniper::unit();
                                })())
                            :
                                juniper::unit());
                        }
                        return {};
                    })());
                    return minVal;
                })()));
        })());
    }
}

namespace List {
    template<typename t455, int c58>
    bool member(t455 elem, juniper::records::recordt_0<juniper::array<t455, c58>, uint32_t> lst) {
        return (([&]() -> bool {
            constexpr int32_t n = c58;
            return (([&]() -> bool {
                bool guid63 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid63;
                
                (([&]() -> juniper::unit {
                    uint32_t guid64 = ((uint32_t) 0);
                    uint32_t guid65 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid64; i <= guid65; i++) {
                        ((!(found) && (((lst).data)[i] == elem)) ? 
                            (([&]() -> juniper::unit {
                                (found = true);
                                return juniper::unit();
                            })())
                        :
                            juniper::unit());
                    }
                    return {};
                })());
                return found;
            })());
        })());
    }
}

namespace List {
    template<typename t467, typename t469, int c62>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t467, c62>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t469, c62>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t> {
            constexpr int32_t n = c62;
            return (((lstA).length == (lstB).length) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t> guid66 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t> guid67;
                        guid67.data = (juniper::array<juniper::tuple2<t467,t469>, c62>());
                        guid67.length = (lstA).length;
                        return guid67;
                    })());
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t> ret = guid66;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid68 = ((uint32_t) 0);
                        uint32_t guid69 = (lstA).length;
                        for (uint32_t i = guid68; i <= guid69; i++) {
                            (([&]() -> juniper::unit {
                                (((ret).data)[i] = (juniper::tuple2<t467,t469>{((lstA).data)[i], ((lstB).data)[i]}));
                                return juniper::unit();
                            })());
                        }
                        return {};
                    })());
                    return ret;
                })())
            :
                juniper::quit<juniper::records::recordt_0<juniper::array<juniper::tuple2<t467,t469>, c62>, uint32_t>>());
        })());
    }
}

namespace List {
    template<typename t480, typename t481, int c66>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t>,juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t480,t481>, c66>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t>,juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t>> {
            constexpr int32_t n = c66;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t>,juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t> guid70 = (([&]() -> juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t> guid71;
                    guid71.data = (juniper::array<t480, c66>());
                    guid71.length = (lst).length;
                    return guid71;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t> retA = guid70;
                
                juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t> guid72 = (([&]() -> juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t> guid73;
                    guid73.data = (juniper::array<t481, c66>());
                    guid73.length = (lst).length;
                    return guid73;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t> retB = guid72;
                
                (([&]() -> juniper::unit {
                    uint32_t guid74 = ((uint32_t) 0);
                    uint32_t guid75 = ((lst).length - ((uint32_t) 1));
                    for (uint32_t i = guid74; i <= guid75; i++) {
                        (([&]() -> juniper::unit {
                            juniper::tuple2<t480,t481> guid76 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t481 b = (guid76).e2;
                            t480 a = (guid76).e1;
                            
                            (((retA).data)[i] = a);
                            (((retB).data)[i] = b);
                            return juniper::unit();
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t480, c66>, uint32_t>,juniper::records::recordt_0<juniper::array<t481, c66>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t490, int c67>
    t490 sum(juniper::records::recordt_0<juniper::array<t490, c67>, uint32_t> lst) {
        return (([&]() -> t490 {
            constexpr int32_t n = c67;
            return List::foldl<t490, t490, void, c67>(juniper::function<void, t490(t490, t490)>(add<t490>), ((t490) 0), lst);
        })());
    }
}

namespace List {
    template<typename t502, int c68>
    t502 average(juniper::records::recordt_0<juniper::array<t502, c68>, uint32_t> lst) {
        return (([&]() -> t502 {
            constexpr int32_t n = c68;
            return (sum<t502, c68>(lst) / cast<uint32_t, t502>((lst).length));
        })());
    }
}

namespace Signal {
    template<typename t508, typename t509, typename t510>
    Prelude::sig<t509> map(juniper::function<t510, t509(t508)> f, Prelude::sig<t508> s) {
        return (([&]() -> Prelude::sig<t509> {
            using a = t508;
            using b = t509;
            using closure = t510;
            return (([&]() -> Prelude::sig<t509> {
                Prelude::sig<t508> guid77 = s;
                return ((((guid77).id() == ((uint8_t) 0)) && ((((guid77).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t509> {
                        t508 val = ((guid77).signal()).just();
                        return signal<t509>(just<t509>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t509> {
                            return signal<t509>(nothing<t509>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t509>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t526, typename t527>
    juniper::unit sink(juniper::function<t527, juniper::unit(t526)> f, Prelude::sig<t526> s) {
        return (([&]() -> juniper::unit {
            using a = t526;
            using closure = t527;
            return (([&]() -> juniper::unit {
                Prelude::sig<t526> guid78 = s;
                return ((((guid78).id() == ((uint8_t) 0)) && ((((guid78).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> juniper::unit {
                        t526 val = ((guid78).signal()).just();
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
    template<typename t532, typename t533>
    Prelude::sig<t532> filter(juniper::function<t533, bool(t532)> f, Prelude::sig<t532> s) {
        return (([&]() -> Prelude::sig<t532> {
            using a = t532;
            using closure = t533;
            return (([&]() -> Prelude::sig<t532> {
                Prelude::sig<t532> guid79 = s;
                return ((((guid79).id() == ((uint8_t) 0)) && ((((guid79).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t532> {
                        t532 val = ((guid79).signal()).just();
                        return (f(val) ? 
                            signal<t532>(nothing<t532>())
                        :
                            s);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t532> {
                            return signal<t532>(nothing<t532>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t532>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t548>
    Prelude::sig<t548> merge(Prelude::sig<t548> sigA, Prelude::sig<t548> sigB) {
        return (([&]() -> Prelude::sig<t548> {
            using a = t548;
            return (([&]() -> Prelude::sig<t548> {
                Prelude::sig<t548> guid80 = sigA;
                return ((((guid80).id() == ((uint8_t) 0)) && ((((guid80).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t548> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t548> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t548>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t550, int c69>
    Prelude::sig<t550> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t550>, c69>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t550> {
            constexpr int32_t n = c69;
            return (([&]() -> Prelude::sig<t550> {
                Prelude::maybe<t550> guid81 = List::foldl<Prelude::sig<t550>, Prelude::maybe<t550>, void, c69>(juniper::function<void, Prelude::maybe<t550>(Prelude::sig<t550>,Prelude::maybe<t550>)>([](Prelude::sig<t550> sig, Prelude::maybe<t550> accum) -> Prelude::maybe<t550> { 
                    return (([&]() -> Prelude::maybe<t550> {
                        Prelude::maybe<t550> guid82 = accum;
                        return ((((guid82).id() == ((uint8_t) 1)) && true) ? 
                            (([&]() -> Prelude::maybe<t550> {
                                return (([&]() -> Prelude::maybe<t550> {
                                    Prelude::sig<t550> guid83 = sig;
                                    if (!((((guid83).id() == ((uint8_t) 0)) && true))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t550> heldValue = (guid83).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t550> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t550>>()));
                    })());
                 }), nothing<t550>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t550> ret = guid81;
                
                return signal<t550>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t567, typename t568>
    Prelude::sig<Prelude::either<t567, t568>> join(Prelude::sig<t567> sigA, Prelude::sig<t568> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t567, t568>> {
            using a = t567;
            using b = t568;
            return (([&]() -> Prelude::sig<Prelude::either<t567, t568>> {
                juniper::tuple2<Prelude::sig<t567>,Prelude::sig<t568>> guid84 = (juniper::tuple2<Prelude::sig<t567>,Prelude::sig<t568>>{sigA, sigB});
                return (((((guid84).e1).id() == ((uint8_t) 0)) && (((((guid84).e1).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t567, t568>> {
                        t567 value = (((guid84).e1).signal()).just();
                        return signal<Prelude::either<t567, t568>>(just<Prelude::either<t567, t568>>(left<t567, t568>(value)));
                    })())
                :
                    (((((guid84).e2).id() == ((uint8_t) 0)) && (((((guid84).e2).signal()).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t567, t568>> {
                            t568 value = (((guid84).e2).signal()).just();
                            return signal<Prelude::either<t567, t568>>(just<Prelude::either<t567, t568>>(right<t567, t568>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t567, t568>> {
                                return signal<Prelude::either<t567, t568>>(nothing<Prelude::either<t567, t568>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t567, t568>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t598>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t598> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t598;
            return map<t598, juniper::unit, void>(juniper::function<void, juniper::unit(t598)>([](t598 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t604, typename t612, typename t606>
    Prelude::sig<t612> foldP(juniper::function<t606, t612(t604, t612)> f, juniper::shared_ptr<t612> state0, Prelude::sig<t604> incoming) {
        return (([&]() -> Prelude::sig<t612> {
            using a = t604;
            using state = t612;
            using closure = t606;
            return (([&]() -> Prelude::sig<t612> {
                Prelude::sig<t604> guid85 = incoming;
                return ((((guid85).id() == ((uint8_t) 0)) && ((((guid85).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t612> {
                        t604 val = ((guid85).signal()).just();
                        return (([&]() -> Prelude::sig<t612> {
                            t612 guid86 = f(val, (*((state0).get())));
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t612 state1 = guid86;
                            
                            (*((t612*) (state0.get())) = state1);
                            return signal<t612>(just<t612>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t612> {
                            return signal<t612>(nothing<t612>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t612>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t628>
    Prelude::sig<t628> dropRepeats(juniper::shared_ptr<Prelude::maybe<t628>> maybePrevValue, Prelude::sig<t628> incoming) {
        return (([&]() -> Prelude::sig<t628> {
            using a = t628;
            return filter<t628, juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t628>>>>(juniper::function<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t628>>>, bool(t628)>(juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t628>>>(maybePrevValue), [](juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t628>>>& junclosure, t628 value) -> bool { 
                juniper::shared_ptr<Prelude::maybe<t628>>& maybePrevValue = junclosure.maybePrevValue;
                return (([&]() -> bool {
                    bool guid87 = (([&]() -> bool {
                        Prelude::maybe<t628> guid88 = (*((maybePrevValue).get()));
                        return ((((guid88).id() == ((uint8_t) 1)) && true) ? 
                            (([&]() -> bool {
                                return false;
                            })())
                        :
                            ((((guid88).id() == ((uint8_t) 0)) && true) ? 
                                (([&]() -> bool {
                                    t628 prevValue = (guid88).just();
                                    return (value == prevValue);
                                })())
                            :
                                juniper::quit<bool>()));
                    })());
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    bool filtered = guid87;
                    
                    (!(filtered) ? 
                        (([&]() -> juniper::unit {
                            (*((Prelude::maybe<t628>*) (maybePrevValue.get())) = just<t628>(value));
                            return juniper::unit();
                        })())
                    :
                        juniper::unit());
                    return filtered;
                })());
             }), incoming);
        })());
    }
}

namespace Signal {
    template<typename t640>
    Prelude::sig<t640> latch(juniper::shared_ptr<t640> prevValue, Prelude::sig<t640> incoming) {
        return (([&]() -> Prelude::sig<t640> {
            using a = t640;
            return (([&]() -> Prelude::sig<t640> {
                Prelude::sig<t640> guid89 = incoming;
                return ((((guid89).id() == ((uint8_t) 0)) && ((((guid89).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t640> {
                        t640 val = ((guid89).signal()).just();
                        return (([&]() -> Prelude::sig<t640> {
                            (*((t640*) (prevValue.get())) = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t640> {
                            return signal<t640>(just<t640>((*((prevValue).get()))));
                        })())
                    :
                        juniper::quit<Prelude::sig<t640>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t655, typename t659, typename t651, typename t652>
    Prelude::sig<t651> map2(juniper::function<t652, t651(t655, t659)> f, juniper::shared_ptr<juniper::tuple2<t655,t659>> state, Prelude::sig<t655> incomingA, Prelude::sig<t659> incomingB) {
        return (([&]() -> Prelude::sig<t651> {
            using a = t655;
            using b = t659;
            using c = t651;
            using closure = t652;
            return (([&]() -> Prelude::sig<t651> {
                t655 guid90 = (([&]() -> t655 {
                    Prelude::sig<t655> guid91 = incomingA;
                    return ((((guid91).id() == ((uint8_t) 0)) && ((((guid91).signal()).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> t655 {
                            t655 val1 = ((guid91).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t655 {
                                return fst<t655, t659>((*((state).get())));
                            })())
                        :
                            juniper::quit<t655>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t655 valA = guid90;
                
                t659 guid92 = (([&]() -> t659 {
                    Prelude::sig<t659> guid93 = incomingB;
                    return ((((guid93).id() == ((uint8_t) 0)) && ((((guid93).signal()).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> t659 {
                            t659 val2 = ((guid93).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t659 {
                                return snd<t655, t659>((*((state).get())));
                            })())
                        :
                            juniper::quit<t659>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t659 valB = guid92;
                
                (*((juniper::tuple2<t655,t659>*) (state.get())) = (juniper::tuple2<t655,t659>{valA, valB}));
                return (([&]() -> Prelude::sig<t651> {
                    juniper::tuple2<Prelude::sig<t655>,Prelude::sig<t659>> guid94 = (juniper::tuple2<Prelude::sig<t655>,Prelude::sig<t659>>{incomingA, incomingB});
                    return (((((guid94).e2).id() == ((uint8_t) 0)) && (((((guid94).e2).signal()).id() == ((uint8_t) 1)) && ((((guid94).e1).id() == ((uint8_t) 0)) && (((((guid94).e1).signal()).id() == ((uint8_t) 1)) && true)))) ? 
                        (([&]() -> Prelude::sig<t651> {
                            return signal<t651>(nothing<t651>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t651> {
                                return signal<t651>(just<t651>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t651>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t680, int c71>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>> pastValues, Prelude::sig<t680> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>> {
            constexpr int32_t n = c71;
            return foldP<t680, juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>, void>(juniper::function<void, juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>(t680, juniper::records::recordt_0<juniper::array<t680, c71>, uint32_t>)>(List::pushOffFront<t680, c71>), pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t690>
    Prelude::sig<t690> constant(t690 val) {
        return (([&]() -> Prelude::sig<t690> {
            using a = t690;
            return signal<t690>(just<t690>(val));
        })());
    }
}

namespace Signal {
    template<typename t698>
    Prelude::sig<Prelude::maybe<t698>> meta(Prelude::sig<t698> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t698>> {
            using a = t698;
            return (([&]() -> Prelude::sig<Prelude::maybe<t698>> {
                Prelude::sig<t698> guid95 = sigA;
                if (!((((guid95).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t698> val = (guid95).signal();
                
                return constant<Prelude::maybe<t698>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t703>
    Prelude::sig<t703> unmeta(Prelude::sig<Prelude::maybe<t703>> sigA) {
        return (([&]() -> Prelude::sig<t703> {
            using a = t703;
            return (([&]() -> Prelude::sig<t703> {
                Prelude::sig<Prelude::maybe<t703>> guid96 = sigA;
                return ((((guid96).id() == ((uint8_t) 0)) && ((((guid96).signal()).id() == ((uint8_t) 0)) && (((((guid96).signal()).just()).id() == ((uint8_t) 0)) && true))) ? 
                    (([&]() -> Prelude::sig<t703> {
                        t703 val = (((guid96).signal()).just()).just();
                        return constant<t703>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t703> {
                            return signal<t703>(nothing<t703>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t703>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t715, typename t716>
    Prelude::sig<juniper::tuple2<t715,t716>> zip(juniper::shared_ptr<juniper::tuple2<t715,t716>> state, Prelude::sig<t715> sigA, Prelude::sig<t716> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t715,t716>> {
            using a = t715;
            using b = t716;
            return map2<t715, t716, juniper::tuple2<t715,t716>, void>(juniper::function<void, juniper::tuple2<t715,t716>(t715,t716)>([](t715 valA, t716 valB) -> juniper::tuple2<t715,t716> { 
                return (juniper::tuple2<t715,t716>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t747, typename t754>
    juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>> unzip(Prelude::sig<juniper::tuple2<t747,t754>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>> {
            using a = t747;
            using b = t754;
            return (([&]() -> juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>> {
                Prelude::sig<juniper::tuple2<t747,t754>> guid97 = incoming;
                return ((((guid97).id() == ((uint8_t) 0)) && ((((guid97).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>> {
                        t754 y = (((guid97).signal()).just()).e2;
                        t747 x = (((guid97).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>>{signal<t747>(just<t747>(x)), signal<t754>(just<t754>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>> {
                            return (juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>>{signal<t747>(nothing<t747>()), signal<t754>(nothing<t754>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t747>,Prelude::sig<t754>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t761, typename t762>
    Prelude::sig<t761> toggle(t761 val1, t761 val2, juniper::shared_ptr<t761> state, Prelude::sig<t762> incoming) {
        return (([&]() -> Prelude::sig<t761> {
            using a = t761;
            using b = t762;
            return foldP<t762, t761, juniper::closures::closuret_5<t761, t761>>(juniper::function<juniper::closures::closuret_5<t761, t761>, t761(t762,t761)>(juniper::closures::closuret_5<t761, t761>(val1, val2), [](juniper::closures::closuret_5<t761, t761>& junclosure, t762 event, t761 prevVal) -> t761 { 
                t761& val1 = junclosure.val1;
                t761& val2 = junclosure.val2;
                return ((prevVal == val1) ? 
                    val2
                :
                    val1);
             }), state, incoming);
        })());
    }
}

namespace Io {
    Io::pinState toggle(Io::pinState p) {
        return (([&]() -> Io::pinState {
            Io::pinState guid98 = p;
            return ((((guid98).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> Io::pinState {
                    return low();
                })())
            :
                ((((guid98).id() == ((uint8_t) 1)) && true) ? 
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
    template<int c72>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c72>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c72;
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
    template<typename t787>
    t787 baseToInt(Io::base b) {
        return (([&]() -> t787 {
            Io::base guid99 = b;
            return ((((guid99).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> t787 {
                    return ((t787) 2);
                })())
            :
                ((((guid99).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> t787 {
                        return ((t787) 8);
                    })())
                :
                    ((((guid99).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> t787 {
                            return ((t787) 10);
                        })())
                    :
                        ((((guid99).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> t787 {
                                return ((t787) 16);
                            })())
                        :
                            juniper::quit<t787>()))));
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
            return ((((guid101).id() == ((uint8_t) 1)) && true) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                ((((guid101).id() == ((uint8_t) 0)) && true) ? 
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
        return ((value == ((uint8_t) 0)) ? 
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
            return ((((guid103).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                ((((guid103).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    ((((guid103).id() == ((uint8_t) 2)) && true) ? 
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
            return (((guid104 == ((int32_t) 0)) && true) ? 
                (([&]() -> Io::mode {
                    return input();
                })())
            :
                (((guid104 == ((int32_t) 1)) && true) ? 
                    (([&]() -> Io::mode {
                        return output();
                    })())
                :
                    (((guid104 == ((int32_t) 2)) && true) ? 
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
        return Signal::toUnit<Io::pinState>(Signal::filter<Io::pinState, juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>>(juniper::function<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, bool(Io::pinState)>(juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>(prevState), [](juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>& junclosure, Io::pinState currState) -> bool { 
            juniper::shared_ptr<Io::pinState>& prevState = junclosure.prevState;
            return (([&]() -> bool {
                bool guid106 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState,Io::pinState> guid107 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((((guid107).e2).id() == ((uint8_t) 1)) && ((((guid107).e1).id() == ((uint8_t) 0)) && true)) ? 
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
                
                (*((Io::pinState*) (prevState.get())) = currState);
                return ret;
            })());
         }), sig));
    }
}

namespace Io {
    Prelude::sig<juniper::unit> fallingEdge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState) {
        return Signal::toUnit<Io::pinState>(Signal::filter<Io::pinState, juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>>(juniper::function<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, bool(Io::pinState)>(juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>(prevState), [](juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>& junclosure, Io::pinState currState) -> bool { 
            juniper::shared_ptr<Io::pinState>& prevState = junclosure.prevState;
            return (([&]() -> bool {
                bool guid108 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState,Io::pinState> guid109 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((((guid109).e2).id() == ((uint8_t) 0)) && ((((guid109).e1).id() == ((uint8_t) 1)) && true)) ? 
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
                
                (*((Io::pinState*) (prevState.get())) = currState);
                return ret;
            })());
         }), sig));
    }
}

namespace Io {
    Prelude::sig<Io::pinState> edge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState) {
        return Signal::filter<Io::pinState, juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>>(juniper::function<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, bool(Io::pinState)>(juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>(prevState), [](juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>& junclosure, Io::pinState currState) -> bool { 
            juniper::shared_ptr<Io::pinState>& prevState = junclosure.prevState;
            return (([&]() -> bool {
                bool guid110 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState,Io::pinState> guid111 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((((guid111).e2).id() == ((uint8_t) 1)) && ((((guid111).e1).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        (((((guid111).e2).id() == ((uint8_t) 0)) && ((((guid111).e1).id() == ((uint8_t) 1)) && true)) ? 
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
                
                (*((Io::pinState*) (prevState.get())) = currState);
                return ret;
            })());
         }), sig);
    }
}

namespace Maybe {
    template<typename t902, typename t903, typename t904>
    Prelude::maybe<t903> map(juniper::function<t904, t903(t902)> f, Prelude::maybe<t902> maybeVal) {
        return (([&]() -> Prelude::maybe<t903> {
            using a = t902;
            using b = t903;
            using closure = t904;
            return (([&]() -> Prelude::maybe<t903> {
                Prelude::maybe<t902> guid112 = maybeVal;
                return ((((guid112).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> Prelude::maybe<t903> {
                        t902 val = (guid112).just();
                        return just<t903>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t903> {
                            return nothing<t903>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t903>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t914>
    t914 get(Prelude::maybe<t914> maybeVal) {
        return (([&]() -> t914 {
            using a = t914;
            return (([&]() -> t914 {
                Prelude::maybe<t914> guid113 = maybeVal;
                return ((((guid113).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> t914 {
                        t914 val = (guid113).just();
                        return val;
                    })())
                :
                    juniper::quit<t914>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t916>
    bool isJust(Prelude::maybe<t916> maybeVal) {
        return (([&]() -> bool {
            using a = t916;
            return (([&]() -> bool {
                Prelude::maybe<t916> guid114 = maybeVal;
                return ((((guid114).id() == ((uint8_t) 0)) && true) ? 
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
    template<typename t918>
    bool isNothing(Prelude::maybe<t918> maybeVal) {
        return (([&]() -> bool {
            using a = t918;
            return !(isJust<t918>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t923>
    uint8_t count(Prelude::maybe<t923> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t923;
            return (([&]() -> uint8_t {
                Prelude::maybe<t923> guid115 = maybeVal;
                return ((((guid115).id() == ((uint8_t) 0)) && true) ? 
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
    template<typename t927, typename t928, typename t929>
    t928 foldl(juniper::function<t929, t928(t927, t928)> f, t928 initState, Prelude::maybe<t927> maybeVal) {
        return (([&]() -> t928 {
            using t = t927;
            using state = t928;
            using closure = t929;
            return (([&]() -> t928 {
                Prelude::maybe<t927> guid116 = maybeVal;
                return ((((guid116).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> t928 {
                        t927 val = (guid116).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t928 {
                            return initState;
                        })())
                    :
                        juniper::quit<t928>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t935, typename t936, typename t937>
    t936 fodlr(juniper::function<t937, t936(t935, t936)> f, t936 initState, Prelude::maybe<t935> maybeVal) {
        return (([&]() -> t936 {
            using t = t935;
            using state = t936;
            using closure = t937;
            return foldl<t935, t936, t937>(f, initState, maybeVal);
        })());
    }
}

namespace Maybe {
    template<typename t944, typename t945>
    juniper::unit iter(juniper::function<t945, juniper::unit(t944)> f, Prelude::maybe<t944> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t944;
            using closure = t945;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t944> guid117 = maybeVal;
                return ((((guid117).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> juniper::unit {
                        t944 val = (guid117).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t944> nothing = guid117;
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
        return (juniper::shared_ptr<juniper::records::recordt_1<uint32_t>>(new juniper::records::recordt_1<uint32_t>((([&]() -> juniper::records::recordt_1<uint32_t>{
            juniper::records::recordt_1<uint32_t> guid119;
            guid119.lastPulse = ((uint32_t) 0);
            return guid119;
        })()))));
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
            
            uint32_t guid121 = ((interval == ((uint32_t) 0)) ? 
                t
            :
                ((t / interval) * interval));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t lastWindow = guid121;
            
            return ((((*((state).get()))).lastPulse >= lastWindow) ? 
                signal<uint32_t>(nothing<uint32_t>())
            :
                (([&]() -> Prelude::sig<uint32_t> {
                    (*((juniper::records::recordt_1<uint32_t>*) (state.get())) = (([&]() -> juniper::records::recordt_1<uint32_t>{
                        juniper::records::recordt_1<uint32_t> guid122;
                        guid122.lastPulse = t;
                        return guid122;
                    })()));
                    return signal<uint32_t>(just<uint32_t>(t));
                })()));
        })());
    }
}

namespace Math {
    double pi = 3.141593;
}

namespace Math {
    double e = 2.718282;
}

namespace Math {
    double degToRad(double degrees) {
        return (degrees * 0.017453);
    }
}

namespace Math {
    double radToDeg(double radians) {
        return (radians * 57.295780);
    }
}

namespace Math {
    double acos_(double x) {
        return (([&]() -> double {
            double guid123 = 0.000000;
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
            double guid124 = 0.000000;
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
            double guid125 = 0.000000;
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
            double guid126 = 0.000000;
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
            double guid127 = 0.000000;
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
            double guid128 = 0.000000;
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
            double guid129 = 0.000000;
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
            double guid130 = 0.000000;
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
        return (sin_(x) / cos_(x));
    }
}

namespace Math {
    double tanh_(double x) {
        return (([&]() -> double {
            double guid131 = 0.000000;
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
            double guid132 = 0.000000;
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
    juniper::tuple2<double,int16_t> frexp_(double x) {
        return (([&]() -> juniper::tuple2<double,int16_t> {
            double guid133 = 0.000000;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid133;
            
            int16_t guid134 = ((int16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t exponent = guid134;
            
            (([&]() -> juniper::unit {
                int exponent2 = (int) exponent;
    ret = frexp(x, &exponent2);
                return {};
            })());
            return (juniper::tuple2<double,int16_t>{ret, exponent});
        })());
    }
}

namespace Math {
    double ldexp_(double x, int16_t exponent) {
        return (([&]() -> double {
            double guid135 = 0.000000;
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
            double guid136 = 0.000000;
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
            double guid137 = 0.000000;
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
    juniper::tuple2<double,double> modf_(double x) {
        return (([&]() -> juniper::tuple2<double,double> {
            double guid138 = 0.000000;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid138;
            
            double guid139 = 0.000000;
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
            double guid140 = 0.000000;
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
            double guid141 = 0.000000;
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
            double guid142 = 0.000000;
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
            double guid143 = 0.000000;
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
            double guid144 = 0.000000;
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
            double guid145 = 0.000000;
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
        return floor_((x + 0.500000));
    }
}

namespace Math {
    double min_(double x, double y) {
        return ((x > y) ? 
            y
        :
            x);
    }
}

namespace Math {
    double max_(double x, double y) {
        return ((x > y) ? 
            x
        :
            y);
    }
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2) {
        return (b1 + (((x - a1) * (b2 - b1)) / (a2 - a1)));
    }
}

namespace Math {
    template<typename t1009>
    t1009 clamp(t1009 x, t1009 min, t1009 max) {
        return (([&]() -> t1009 {
            using a = t1009;
            return ((min > x) ? 
                min
            :
                ((x > max) ? 
                    max
                :
                    x));
        })());
    }
}

namespace Math {
    template<typename t1014>
    int8_t sign(t1014 n) {
        return (([&]() -> int8_t {
            using a = t1014;
            return ((n == ((t1014) 0)) ? 
                ((int8_t) 0)
            :
                ((n > ((t1014) 0)) ? 
                    ((int8_t) 1)
                :
                    -(((int8_t) 1))));
        })());
    }
}

namespace Button {
    juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> state() {
        return (juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>(new juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>((([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid146;
            guid146.actualState = Io::low();
            guid146.lastState = Io::low();
            guid146.lastDebounceTime = ((uint32_t) 0);
            return guid146;
        })()))));
    }
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> buttonState) {
        return Signal::map<Io::pinState, Io::pinState, juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>>(juniper::function<juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>, Io::pinState(Io::pinState)>(juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>(buttonState, delay), [](juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>& junclosure, Io::pinState currentState) -> Io::pinState { 
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
                
                return ((currentState != lastState) ? 
                    (([&]() -> Io::pinState {
                        (*((juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>*) (buttonState.get())) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid148;
                            guid148.actualState = actualState;
                            guid148.lastState = currentState;
                            guid148.lastDebounceTime = Time::now();
                            return guid148;
                        })()));
                        return actualState;
                    })())
                :
                    (((currentState != actualState) && ((Time::now() - ((*((buttonState).get()))).lastDebounceTime) > delay)) ? 
                        (([&]() -> Io::pinState {
                            (*((juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>*) (buttonState.get())) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
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
                            (*((juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>*) (buttonState.get())) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
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
    template<typename t1056, int c73>
    juniper::records::recordt_3<juniper::array<t1056, c73>> make(juniper::array<t1056, c73> d) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1056, c73>> {
            constexpr int32_t n = c73;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1056, c73>>{
                juniper::records::recordt_3<juniper::array<t1056, c73>> guid151;
                guid151.data = d;
                return guid151;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1058, int c75>
    t1058 get(uint32_t i, juniper::records::recordt_3<juniper::array<t1058, c75>> v) {
        return (([&]() -> t1058 {
            constexpr int32_t n = c75;
            return ((v).data)[i];
        })());
    }
}

namespace Vector {
    template<typename t1068, int c79>
    juniper::records::recordt_3<juniper::array<t1068, c79>> add(juniper::records::recordt_3<juniper::array<t1068, c79>> v1, juniper::records::recordt_3<juniper::array<t1068, c79>> v2) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1068, c79>> {
            constexpr int32_t n = c79;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1068, c79>> {
                juniper::records::recordt_3<juniper::array<t1068, c79>> guid152 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1068, c79>> result = guid152;
                
                (([&]() -> juniper::unit {
                    int32_t guid153 = ((int32_t) 0);
                    int32_t guid154 = (n - ((int32_t) 1));
                    for (int32_t i = guid153; i <= guid154; i++) {
                        (([&]() -> juniper::unit {
                            (((result).data)[i] = (((result).data)[i] + ((v2).data)[i]));
                            return juniper::unit();
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
    template<typename t1072, int c80>
    juniper::records::recordt_3<juniper::array<t1072, c80>> zero() {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1072, c80>> {
            constexpr int32_t n = c80;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1072, c80>>{
                juniper::records::recordt_3<juniper::array<t1072, c80>> guid155;
                guid155.data = (juniper::array<t1072, c80>().fill(((t1072) 0)));
                return guid155;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1081, int c84>
    juniper::records::recordt_3<juniper::array<t1081, c84>> subtract(juniper::records::recordt_3<juniper::array<t1081, c84>> v1, juniper::records::recordt_3<juniper::array<t1081, c84>> v2) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1081, c84>> {
            constexpr int32_t n = c84;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1081, c84>> {
                juniper::records::recordt_3<juniper::array<t1081, c84>> guid156 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1081, c84>> result = guid156;
                
                (([&]() -> juniper::unit {
                    int32_t guid157 = ((int32_t) 0);
                    int32_t guid158 = (n - ((int32_t) 1));
                    for (int32_t i = guid157; i <= guid158; i++) {
                        (([&]() -> juniper::unit {
                            (((result).data)[i] = (((result).data)[i] - ((v2).data)[i]));
                            return juniper::unit();
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
    template<typename t1089, int c87>
    juniper::records::recordt_3<juniper::array<t1089, c87>> scale(t1089 scalar, juniper::records::recordt_3<juniper::array<t1089, c87>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1089, c87>> {
            constexpr int32_t n = c87;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1089, c87>> {
                juniper::records::recordt_3<juniper::array<t1089, c87>> guid159 = v;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1089, c87>> result = guid159;
                
                (([&]() -> juniper::unit {
                    int32_t guid160 = ((int32_t) 0);
                    int32_t guid161 = (n - ((int32_t) 1));
                    for (int32_t i = guid160; i <= guid161; i++) {
                        (([&]() -> juniper::unit {
                            (((result).data)[i] = (((result).data)[i] * scalar));
                            return juniper::unit();
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
    template<typename t1100, int c90>
    t1100 dot(juniper::records::recordt_3<juniper::array<t1100, c90>> v1, juniper::records::recordt_3<juniper::array<t1100, c90>> v2) {
        return (([&]() -> t1100 {
            constexpr int32_t n = c90;
            return (([&]() -> t1100 {
                t1100 guid162 = ((t1100) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1100 sum = guid162;
                
                (([&]() -> juniper::unit {
                    int32_t guid163 = ((int32_t) 0);
                    int32_t guid164 = (n - ((int32_t) 1));
                    for (int32_t i = guid163; i <= guid164; i++) {
                        (([&]() -> juniper::unit {
                            (sum = (sum + (((v1).data)[i] * ((v2).data)[i])));
                            return juniper::unit();
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
    template<typename t1109, int c93>
    t1109 magnitude2(juniper::records::recordt_3<juniper::array<t1109, c93>> v) {
        return (([&]() -> t1109 {
            constexpr int32_t n = c93;
            return (([&]() -> t1109 {
                t1109 guid165 = ((t1109) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1109 sum = guid165;
                
                (([&]() -> juniper::unit {
                    int32_t guid166 = ((int32_t) 0);
                    int32_t guid167 = (n - ((int32_t) 1));
                    for (int32_t i = guid166; i <= guid167; i++) {
                        (([&]() -> juniper::unit {
                            (sum = (sum + (((v).data)[i] * ((v).data)[i])));
                            return juniper::unit();
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
    template<typename t1111, int c94>
    double magnitude(juniper::records::recordt_3<juniper::array<t1111, c94>> v) {
        return (([&]() -> double {
            constexpr int32_t n = c94;
            return sqrt_(magnitude2<t1111, c94>(v));
        })());
    }
}

namespace Vector {
    template<typename t1125, int c98>
    juniper::records::recordt_3<juniper::array<t1125, c98>> multiply(juniper::records::recordt_3<juniper::array<t1125, c98>> u, juniper::records::recordt_3<juniper::array<t1125, c98>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1125, c98>> {
            constexpr int32_t n = c98;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1125, c98>> {
                juniper::records::recordt_3<juniper::array<t1125, c98>> guid168 = u;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1125, c98>> result = guid168;
                
                (([&]() -> juniper::unit {
                    int32_t guid169 = ((int32_t) 0);
                    int32_t guid170 = (n - ((int32_t) 1));
                    for (int32_t i = guid169; i <= guid170; i++) {
                        (([&]() -> juniper::unit {
                            (((result).data)[i] = (((result).data)[i] * ((v).data)[i]));
                            return juniper::unit();
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
    template<typename t1136, int c101>
    juniper::records::recordt_3<juniper::array<t1136, c101>> normalize(juniper::records::recordt_3<juniper::array<t1136, c101>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1136, c101>> {
            constexpr int32_t n = c101;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1136, c101>> {
                double guid171 = magnitude<t1136, c101>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid171;
                
                return ((mag > ((t1136) 0)) ? 
                    (([&]() -> juniper::records::recordt_3<juniper::array<t1136, c101>> {
                        juniper::records::recordt_3<juniper::array<t1136, c101>> guid172 = v;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_3<juniper::array<t1136, c101>> result = guid172;
                        
                        (([&]() -> juniper::unit {
                            int32_t guid173 = ((int32_t) 0);
                            int32_t guid174 = (n - ((int32_t) 1));
                            for (int32_t i = guid173; i <= guid174; i++) {
                                (([&]() -> juniper::unit {
                                    (((result).data)[i] = fromDouble<t1136>((toDouble<t1136>(((result).data)[i]) / mag)));
                                    return juniper::unit();
                                })());
                            }
                            return {};
                        })());
                        return result;
                    })())
                :
                    v);
            })());
        })());
    }
}

namespace Vector {
    template<typename t1149, int c102>
    double angle(juniper::records::recordt_3<juniper::array<t1149, c102>> v1, juniper::records::recordt_3<juniper::array<t1149, c102>> v2) {
        return (([&]() -> double {
            constexpr int32_t n = c102;
            return acos_((dot<t1149, c102>(v1, v2) / sqrt_((magnitude2<t1149, c102>(v1) * magnitude2<t1149, c102>(v2)))));
        })());
    }
}

namespace Vector {
    template<typename t1167>
    juniper::records::recordt_3<juniper::array<t1167, 3>> cross(juniper::records::recordt_3<juniper::array<t1167, 3>> u, juniper::records::recordt_3<juniper::array<t1167, 3>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1167, 3>> {
            using a = t1167;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1167, 3>>{
                juniper::records::recordt_3<juniper::array<t1167, 3>> guid175;
                guid175.data = (juniper::array<t1167, 3> { {((((u).data)[((uint32_t) 1)] * ((v).data)[((uint32_t) 2)]) - (((u).data)[((uint32_t) 2)] * ((v).data)[((uint32_t) 1)])), ((((u).data)[((uint32_t) 2)] * ((v).data)[((uint32_t) 0)]) - (((u).data)[((uint32_t) 0)] * ((v).data)[((uint32_t) 2)])), ((((u).data)[((uint32_t) 0)] * ((v).data)[((uint32_t) 1)]) - (((u).data)[((uint32_t) 1)] * ((v).data)[((uint32_t) 0)]))} });
                return guid175;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1194, int c115>
    juniper::records::recordt_3<juniper::array<t1194, c115>> project(juniper::records::recordt_3<juniper::array<t1194, c115>> a, juniper::records::recordt_3<juniper::array<t1194, c115>> b) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1194, c115>> {
            constexpr int32_t n = c115;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1194, c115>> {
                juniper::records::recordt_3<juniper::array<t1194, c115>> guid176 = normalize<t1194, c115>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1194, c115>> bn = guid176;
                
                return scale<t1194, c115>(dot<t1194, c115>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t1207, int c116>
    juniper::records::recordt_3<juniper::array<t1207, c116>> projectPlane(juniper::records::recordt_3<juniper::array<t1207, c116>> a, juniper::records::recordt_3<juniper::array<t1207, c116>> m) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1207, c116>> {
            constexpr int32_t n = c116;
            return subtract<t1207, c116>(a, project<t1207, c116>(a, m));
        })());
    }
}

namespace CharList {
    template<int c117>
    juniper::records::recordt_0<juniper::array<uint8_t, c117>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c117>, uint32_t> str) {
        return List::map<uint8_t, uint8_t, void, c117>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((c >= ((uint8_t) 97)) && (c <= ((uint8_t) 122))) ? 
                (c - ((uint8_t) 32))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c118>
    juniper::records::recordt_0<juniper::array<uint8_t, c118>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c118>, uint32_t> str) {
        return List::map<uint8_t, uint8_t, void, c118>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((c >= ((uint8_t) 65)) && (c <= ((uint8_t) 90))) ? 
                (c + ((uint8_t) 32))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c119>
    juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t> i32ToCharList(int32_t m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t> {
            constexpr int32_t n = c119;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t> guid177 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t> guid178;
                    guid178.data = (juniper::array<uint8_t, (c119)+(1)>().fill(((uint8_t) 0)));
                    guid178.length = ((uint32_t) 0);
                    return guid178;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (c119)+(1)>, uint32_t> ret = guid177;
                
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
    template<int c120>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c120>, uint32_t> s) {
        return (([&]() -> uint32_t {
            constexpr int32_t n = c120;
            return ((s).length - ((uint32_t) 1));
        })());
    }
}

namespace CharList {
    template<int c126, int c128, int c129>
    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, c126>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, c128>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> {
            constexpr int32_t aCap = c126;
            constexpr int32_t bCap = c128;
            constexpr int32_t retCap = c129;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> {
                uint32_t guid179 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid179;
                
                uint32_t guid180 = length<c126>(sA);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenA = guid180;
                
                uint32_t guid181 = length<c128>(sB);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenB = guid181;
                
                juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> guid182 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> guid183;
                    guid183.data = (juniper::array<uint8_t, c129>().fill(((uint8_t) 0)));
                    guid183.length = ((lenA + lenB) + ((uint32_t) 1));
                    return guid183;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> out = guid182;
                
                (([&]() -> juniper::unit {
                    uint32_t guid184 = ((uint32_t) 0);
                    uint32_t guid185 = (lenA - ((uint32_t) 1));
                    for (uint32_t i = guid184; i <= guid185; i++) {
                        (([&]() -> juniper::unit {
                            (((out).data)[j] = ((sA).data)[i]);
                            (j = (j + ((uint32_t) 1)));
                            return juniper::unit();
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid186 = ((uint32_t) 0);
                    uint32_t guid187 = (lenB - ((uint32_t) 1));
                    for (uint32_t i = guid186; i <= guid187; i++) {
                        (([&]() -> juniper::unit {
                            (((out).data)[j] = ((sB).data)[i]);
                            (j = (j + ((uint32_t) 1)));
                            return juniper::unit();
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
    template<int c130, int c131>
    juniper::records::recordt_0<juniper::array<uint8_t, ((c130)+(c131))-(1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, c131>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((c130)+(c131))-(1)>, uint32_t> {
            constexpr int32_t aCap = c130;
            constexpr int32_t bCap = c131;
            return concat<c130, c131, ((c130)+(c131))-(1)>(sA, sB);
        })());
    }
}

namespace Random {
    int32_t random_(int32_t low, int32_t high) {
        return (([&]() -> int32_t {
            int32_t guid188 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t ret = guid188;
            
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
    template<typename t1271, int c135>
    t1271 choice(juniper::records::recordt_0<juniper::array<t1271, c135>, uint32_t> lst) {
        return (([&]() -> t1271 {
            constexpr int32_t n = c135;
            return (([&]() -> t1271 {
                int32_t guid189 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid189;
                
                return List::nth<t1271, c135>(i32ToU32(i), lst);
            })());
        })());
    }
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_6<float, float, float> color) {
        return (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> {
            juniper::records::recordt_6<float, float, float> guid190 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float v = (guid190).v;
            float s = (guid190).s;
            float h = (guid190).h;
            
            float guid191 = (v * s);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float c = guid191;
            
            float guid192 = (c * toFloat<double>((1.000000 - Math::fabs_((Math::fmod_((toDouble<float>(h) / ((double) 60)), 2.000000) - 1.000000)))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float x = guid192;
            
            float guid193 = (v - c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float m = guid193;
            
            juniper::tuple3<float,float,float> guid194 = (((0.000000 <= h) && (h < 60.000000)) ? 
                (juniper::tuple3<float,float,float>{c, x, 0.000000})
            :
                (((60.000000 <= h) && (h < 120.000000)) ? 
                    (juniper::tuple3<float,float,float>{x, c, 0.000000})
                :
                    (((120.000000 <= h) && (h < 180.000000)) ? 
                        (juniper::tuple3<float,float,float>{0.000000, c, x})
                    :
                        (((180.000000 <= h) && (h < 240.000000)) ? 
                            (juniper::tuple3<float,float,float>{0.000000, x, c})
                        :
                            (((240.000000 <= h) && (h < 300.000000)) ? 
                                (juniper::tuple3<float,float,float>{x, 0.000000, c})
                            :
                                (juniper::tuple3<float,float,float>{c, 0.000000, x}))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float bPrime = (guid194).e3;
            float gPrime = (guid194).e2;
            float rPrime = (guid194).e1;
            
            float guid195 = ((rPrime + m) * 255.000000);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r = guid195;
            
            float guid196 = ((gPrime + m) * 255.000000);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g = guid196;
            
            float guid197 = ((bPrime + m) * 255.000000);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b = guid197;
            
            return (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
                juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid198;
                guid198.r = toUInt8<float>(r);
                guid198.g = toUInt8<float>(g);
                guid198.b = toUInt8<float>(b);
                return guid198;
            })());
        })());
    }
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> color) {
        return (([&]() -> uint16_t {
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid199 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = (guid199).b;
            uint8_t g = (guid199).g;
            uint8_t r = (guid199).r;
            
            return ((((u8ToU16(r) & ((uint16_t) 248)) << ((uint32_t) 8)) | ((u8ToU16(g) & ((uint16_t) 252)) << ((uint32_t) 3))) | (u8ToU16(b) >> ((uint32_t) 3)));
        })());
    }
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> red = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid200;
        guid200.r = ((uint8_t) 255);
        guid200.g = ((uint8_t) 0);
        guid200.b = ((uint8_t) 0);
        return guid200;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> green = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid201;
        guid201.r = ((uint8_t) 0);
        guid201.g = ((uint8_t) 255);
        guid201.b = ((uint8_t) 0);
        return guid201;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> blue = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid202;
        guid202.r = ((uint8_t) 0);
        guid202.g = ((uint8_t) 0);
        guid202.b = ((uint8_t) 255);
        return guid202;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> black = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid203;
        guid203.r = ((uint8_t) 0);
        guid203.g = ((uint8_t) 0);
        guid203.b = ((uint8_t) 0);
        return guid203;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> white = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid204;
        guid204.r = ((uint8_t) 255);
        guid204.g = ((uint8_t) 255);
        guid204.b = ((uint8_t) 255);
        return guid204;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> yellow = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid205;
        guid205.r = ((uint8_t) 255);
        guid205.g = ((uint8_t) 255);
        guid205.b = ((uint8_t) 0);
        return guid205;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> magenta = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid206;
        guid206.r = ((uint8_t) 255);
        guid206.g = ((uint8_t) 0);
        guid206.b = ((uint8_t) 255);
        return guid206;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> cyan = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid207;
        guid207.r = ((uint8_t) 0);
        guid207.g = ((uint8_t) 255);
        guid207.b = ((uint8_t) 255);
        return guid207;
    })());
}

namespace Arcada {
    bool arcadaBegin() {
        return (([&]() -> bool {
            bool guid208 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid208;
            
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
            uint16_t guid209 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid209;
            
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
            uint16_t guid210 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid210;
            
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
            uint16_t guid211 = displayWidth();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid211;
            
            uint16_t guid212 = displayHeight();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid212;
            
            bool guid213 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid213;
            
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
            bool guid214 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid214;
            
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
            Ble::servicet guid215 = s;
            if (!((((guid215).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid215).service();
            
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
            Ble::characterstict guid216 = c;
            if (!((((guid216).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid216).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->begin();
                return {};
            })());
        })());
    }
}

namespace Ble {
    template<int c137>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c137>, uint32_t> bytes) {
        return (([&]() -> uint16_t {
            constexpr int32_t n = c137;
            return (([&]() -> uint16_t {
                juniper::records::recordt_0<juniper::array<uint8_t, c137>, uint32_t> guid217 = bytes;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t length = (guid217).length;
                juniper::array<uint8_t, c137> data = (guid217).data;
                
                Ble::characterstict guid218 = c;
                if (!((((guid218).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid218).characterstic();
                
                uint16_t guid219 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid219;
                
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
            Ble::characterstict guid220 = c;
            if (!((((guid220).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid220).characterstic();
            
            uint16_t guid221 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid221;
            
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
            Ble::characterstict guid222 = c;
            if (!((((guid222).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid222).characterstic();
            
            uint16_t guid223 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid223;
            
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
            Ble::characterstict guid224 = c;
            if (!((((guid224).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid224).characterstic();
            
            uint16_t guid225 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid225;
            
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
            Ble::characterstict guid226 = c;
            if (!((((guid226).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid226).characterstic();
            
            uint16_t guid227 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid227;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write32((int) n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    template<typename t1592>
    uint16_t writeGeneric(Ble::characterstict c, t1592 x) {
        return (([&]() -> uint16_t {
            using t = t1592;
            return (([&]() -> uint16_t {
                Ble::characterstict guid228 = c;
                if (!((((guid228).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid228).characterstic();
                
                uint16_t guid229 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid229;
                
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
    template<typename t1595>
    t1595 readGeneric(Ble::characterstict c) {
        return (([&]() -> t1595 {
            using t = t1595;
            return (([&]() -> t1595 {
                Ble::characterstict guid230 = c;
                if (!((((guid230).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid230).characterstic();
                
                t1595 ret;
                
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
            Ble::secureModet guid231 = readPermission;
            if (!((((guid231).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t readN = (guid231).secureMode();
            
            Ble::secureModet guid232 = writePermission;
            if (!((((guid232).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t writeN = (guid232).secureMode();
            
            Ble::characterstict guid233 = c;
            if (!((((guid233).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid233).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->setPermission((BleSecurityMode) readN, (BleSecurityMode) writeN);
                return {};
            })());
        })());
    }
}

namespace Ble {
    juniper::unit setCharacteristicProperties(Ble::characterstict c, Ble::propertiest prop) {
        return (([&]() -> juniper::unit {
            Ble::propertiest guid234 = prop;
            if (!((((guid234).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t propN = (guid234).properties();
            
            Ble::characterstict guid235 = c;
            if (!((((guid235).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid235).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->setProperties(propN);
                return {};
            })());
        })());
    }
}

namespace Ble {
    template<typename t1605>
    juniper::unit setCharacteristicFixedLen(Ble::characterstict c) {
        return (([&]() -> juniper::unit {
            using t = t1605;
            return (([&]() -> juniper::unit {
                Ble::characterstict guid236 = c;
                if (!((((guid236).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid236).characterstic();
                
                return (([&]() -> juniper::unit {
                    ((BLECharacteristic *) p)->setFixedLen(sizeof(t));
                    return {};
                })());
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
            Bluefruit.Periph.setConnInterval(9, 16);
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
            Ble::advertisingFlagt guid237 = flag;
            if (!((((guid237).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t flagNum = (guid237).advertisingFlag();
            
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
            Ble::appearancet guid238 = app;
            if (!((((guid238).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t flagNum = (guid238).appearance();
            
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
            Ble::servicet guid239 = serv;
            if (!((((guid239).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid239).service();
            
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
            Ble::setCharacteristicFixedLen<juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>>(dayDateTimeCharacterstic);
            Ble::setCharacteristicFixedLen<juniper::records::recordt_9<uint8_t>>(dayOfWeekCharacterstic);
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
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> pink = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid240;
        guid240.r = ((uint8_t) 252);
        guid240.g = ((uint8_t) 92);
        guid240.b = ((uint8_t) 125);
        return guid240;
    })());
}

namespace CWatch {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> purpleBlue = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid241;
        guid241.r = ((uint8_t) 106);
        guid241.g = ((uint8_t) 130);
        guid241.b = ((uint8_t) 251);
        return guid241;
    })());
}

namespace CWatch {
    bool isLeapYear(uint32_t y) {
        return (((y % ((uint32_t) 4)) != ((uint32_t) 0)) ? 
            false
        :
            (((y % ((uint32_t) 100)) != ((uint32_t) 0)) ? 
                true
            :
                (((y % ((uint32_t) 400)) == ((uint32_t) 0)) ? 
                    true
                :
                    false)));
    }
}

namespace CWatch {
    uint8_t daysInMonth(uint32_t y, CWatch::month m) {
        return (([&]() -> uint8_t {
            CWatch::month guid242 = m;
            return ((((guid242).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 31);
                })())
            :
                ((((guid242).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> uint8_t {
                        return (isLeapYear(y) ? 
                            ((uint8_t) 29)
                        :
                            ((uint8_t) 28));
                    })())
                :
                    ((((guid242).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 31);
                        })())
                    :
                        ((((guid242).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> uint8_t {
                                return ((uint8_t) 30);
                            })())
                        :
                            ((((guid242).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> uint8_t {
                                    return ((uint8_t) 31);
                                })())
                            :
                                ((((guid242).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> uint8_t {
                                        return ((uint8_t) 30);
                                    })())
                                :
                                    ((((guid242).id() == ((uint8_t) 6)) && true) ? 
                                        (([&]() -> uint8_t {
                                            return ((uint8_t) 31);
                                        })())
                                    :
                                        ((((guid242).id() == ((uint8_t) 7)) && true) ? 
                                            (([&]() -> uint8_t {
                                                return ((uint8_t) 31);
                                            })())
                                        :
                                            ((((guid242).id() == ((uint8_t) 8)) && true) ? 
                                                (([&]() -> uint8_t {
                                                    return ((uint8_t) 30);
                                                })())
                                            :
                                                ((((guid242).id() == ((uint8_t) 9)) && true) ? 
                                                    (([&]() -> uint8_t {
                                                        return ((uint8_t) 31);
                                                    })())
                                                :
                                                    ((((guid242).id() == ((uint8_t) 10)) && true) ? 
                                                        (([&]() -> uint8_t {
                                                            return ((uint8_t) 30);
                                                        })())
                                                    :
                                                        ((((guid242).id() == ((uint8_t) 11)) && true) ? 
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
            CWatch::month guid243 = m;
            return ((((guid243).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> CWatch::month {
                    return february();
                })())
            :
                ((((guid243).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> CWatch::month {
                        return march();
                    })())
                :
                    ((((guid243).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> CWatch::month {
                            return april();
                        })())
                    :
                        ((((guid243).id() == ((uint8_t) 4)) && true) ? 
                            (([&]() -> CWatch::month {
                                return june();
                            })())
                        :
                            ((((guid243).id() == ((uint8_t) 5)) && true) ? 
                                (([&]() -> CWatch::month {
                                    return july();
                                })())
                            :
                                ((((guid243).id() == ((uint8_t) 7)) && true) ? 
                                    (([&]() -> CWatch::month {
                                        return august();
                                    })())
                                :
                                    ((((guid243).id() == ((uint8_t) 8)) && true) ? 
                                        (([&]() -> CWatch::month {
                                            return october();
                                        })())
                                    :
                                        ((((guid243).id() == ((uint8_t) 9)) && true) ? 
                                            (([&]() -> CWatch::month {
                                                return november();
                                            })())
                                        :
                                            ((((guid243).id() == ((uint8_t) 11)) && true) ? 
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
            CWatch::dayOfWeek guid244 = dw;
            return ((((guid244).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> CWatch::dayOfWeek {
                    return monday();
                })())
            :
                ((((guid244).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> CWatch::dayOfWeek {
                        return tuesday();
                    })())
                :
                    ((((guid244).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> CWatch::dayOfWeek {
                            return wednesday();
                        })())
                    :
                        ((((guid244).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> CWatch::dayOfWeek {
                                return thursday();
                            })())
                        :
                            ((((guid244).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return friday();
                                })())
                            :
                                ((((guid244).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return saturday();
                                    })())
                                :
                                    ((((guid244).id() == ((uint8_t) 6)) && true) ? 
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
            CWatch::dayOfWeek guid245 = dw;
            return ((((guid245).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid246;
                        guid246.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid246.length = ((uint32_t) 4);
                        return guid246;
                    })());
                })())
            :
                ((((guid245).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid247;
                            guid247.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 111), ((uint8_t) 110), ((uint8_t) 0)} });
                            guid247.length = ((uint32_t) 4);
                            return guid247;
                        })());
                    })())
                :
                    ((((guid245).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid248;
                                guid248.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 117), ((uint8_t) 101), ((uint8_t) 0)} });
                                guid248.length = ((uint32_t) 4);
                                return guid248;
                            })());
                        })())
                    :
                        ((((guid245).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid249;
                                    guid249.data = (juniper::array<uint8_t, 4> { {((uint8_t) 87), ((uint8_t) 101), ((uint8_t) 100), ((uint8_t) 0)} });
                                    guid249.length = ((uint32_t) 4);
                                    return guid249;
                                })());
                            })())
                        :
                            ((((guid245).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid250;
                                        guid250.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 104), ((uint8_t) 117), ((uint8_t) 0)} });
                                        guid250.length = ((uint32_t) 4);
                                        return guid250;
                                    })());
                                })())
                            :
                                ((((guid245).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid251;
                                            guid251.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 114), ((uint8_t) 105), ((uint8_t) 0)} });
                                            guid251.length = ((uint32_t) 4);
                                            return guid251;
                                        })());
                                    })())
                                :
                                    ((((guid245).id() == ((uint8_t) 6)) && true) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid252;
                                                guid252.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 97), ((uint8_t) 116), ((uint8_t) 0)} });
                                                guid252.length = ((uint32_t) 4);
                                                return guid252;
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
            CWatch::month guid253 = m;
            return ((((guid253).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid254;
                        guid254.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 97), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid254.length = ((uint32_t) 4);
                        return guid254;
                    })());
                })())
            :
                ((((guid253).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid255;
                            guid255.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 101), ((uint8_t) 98), ((uint8_t) 0)} });
                            guid255.length = ((uint32_t) 4);
                            return guid255;
                        })());
                    })())
                :
                    ((((guid253).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid256;
                                guid256.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 114), ((uint8_t) 0)} });
                                guid256.length = ((uint32_t) 4);
                                return guid256;
                            })());
                        })())
                    :
                        ((((guid253).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid257;
                                    guid257.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 112), ((uint8_t) 114), ((uint8_t) 0)} });
                                    guid257.length = ((uint32_t) 4);
                                    return guid257;
                                })());
                            })())
                        :
                            ((((guid253).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid258;
                                        guid258.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 121), ((uint8_t) 0)} });
                                        guid258.length = ((uint32_t) 4);
                                        return guid258;
                                    })());
                                })())
                            :
                                ((((guid253).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid259;
                                            guid259.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                                            guid259.length = ((uint32_t) 4);
                                            return guid259;
                                        })());
                                    })())
                                :
                                    ((((guid253).id() == ((uint8_t) 6)) && true) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid260;
                                                guid260.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 108), ((uint8_t) 0)} });
                                                guid260.length = ((uint32_t) 4);
                                                return guid260;
                                            })());
                                        })())
                                    :
                                        ((((guid253).id() == ((uint8_t) 7)) && true) ? 
                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid261;
                                                    guid261.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 117), ((uint8_t) 103), ((uint8_t) 0)} });
                                                    guid261.length = ((uint32_t) 4);
                                                    return guid261;
                                                })());
                                            })())
                                        :
                                            ((((guid253).id() == ((uint8_t) 8)) && true) ? 
                                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid262;
                                                        guid262.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 101), ((uint8_t) 112), ((uint8_t) 0)} });
                                                        guid262.length = ((uint32_t) 4);
                                                        return guid262;
                                                    })());
                                                })())
                                            :
                                                ((((guid253).id() == ((uint8_t) 9)) && true) ? 
                                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid263;
                                                            guid263.data = (juniper::array<uint8_t, 4> { {((uint8_t) 79), ((uint8_t) 99), ((uint8_t) 116), ((uint8_t) 0)} });
                                                            guid263.length = ((uint32_t) 4);
                                                            return guid263;
                                                        })());
                                                    })())
                                                :
                                                    ((((guid253).id() == ((uint8_t) 10)) && true) ? 
                                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid264;
                                                                guid264.data = (juniper::array<uint8_t, 4> { {((uint8_t) 78), ((uint8_t) 111), ((uint8_t) 118), ((uint8_t) 0)} });
                                                                guid264.length = ((uint32_t) 4);
                                                                return guid264;
                                                            })());
                                                        })())
                                                    :
                                                        ((((guid253).id() == ((uint8_t) 11)) && true) ? 
                                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid265;
                                                                    guid265.data = (juniper::array<uint8_t, 4> { {((uint8_t) 68), ((uint8_t) 101), ((uint8_t) 99), ((uint8_t) 0)} });
                                                                    guid265.length = ((uint32_t) 4);
                                                                    return guid265;
                                                                })());
                                                            })())
                                                        :
                                                            juniper::quit<juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>>()))))))))))));
        })());
    }
}

namespace CWatch {
    juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> secondTick(juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> d) {
        return (([&]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> {
            juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid266 = d;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek = (guid266).dayOfWeek;
            uint8_t seconds = (guid266).seconds;
            uint8_t minutes = (guid266).minutes;
            uint8_t hours = (guid266).hours;
            uint32_t year = (guid266).year;
            uint8_t day = (guid266).day;
            CWatch::month month = (guid266).month;
            
            uint8_t guid267 = (seconds + ((uint8_t) 1));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t seconds1 = guid267;
            
            juniper::tuple2<uint8_t,uint8_t> guid268 = ((seconds1 == ((uint8_t) 60)) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), (minutes + ((uint8_t) 1))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{seconds1, minutes}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t minutes1 = (guid268).e2;
            uint8_t seconds2 = (guid268).e1;
            
            juniper::tuple2<uint8_t,uint8_t> guid269 = ((minutes1 == ((uint8_t) 60)) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), (hours + ((uint8_t) 1))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{minutes1, hours}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t hours1 = (guid269).e2;
            uint8_t minutes2 = (guid269).e1;
            
            juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek> guid270 = ((hours1 == ((uint8_t) 24)) ? 
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{((uint8_t) 0), (day + ((uint8_t) 1)), nextDayOfWeek(dayOfWeek)})
            :
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{hours1, day, dayOfWeek}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek2 = (guid270).e3;
            uint8_t day1 = (guid270).e2;
            uint8_t hours2 = (guid270).e1;
            
            uint8_t guid271 = daysInMonth(year, month);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t daysInCurrentMonth = guid271;
            
            juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month,uint32_t>> guid272 = ((day1 > daysInCurrentMonth) ? 
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month,uint32_t>>{((uint8_t) 1), (([&]() -> juniper::tuple2<CWatch::month,uint32_t> {
                    CWatch::month guid273 = month;
                    return ((((guid273).id() == ((uint8_t) 11)) && true) ? 
                        (([&]() -> juniper::tuple2<CWatch::month,uint32_t> {
                            return (juniper::tuple2<CWatch::month,uint32_t>{january(), (year + ((uint32_t) 1))});
                        })())
                    :
                        (true ? 
                            (([&]() -> juniper::tuple2<CWatch::month,uint32_t> {
                                return (juniper::tuple2<CWatch::month,uint32_t>{nextMonth(month), year});
                            })())
                        :
                            juniper::quit<juniper::tuple2<CWatch::month,uint32_t>>()));
                })())})
            :
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month,uint32_t>>{day1, (juniper::tuple2<CWatch::month,uint32_t>{month, year})}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t year2 = ((guid272).e2).e2;
            CWatch::month month2 = ((guid272).e2).e1;
            uint8_t day2 = (guid272).e1;
            
            return (([&]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid274;
                guid274.month = month2;
                guid274.day = day2;
                guid274.year = year2;
                guid274.hours = hours2;
                guid274.minutes = minutes2;
                guid274.seconds = seconds2;
                guid274.dayOfWeek = dayOfWeek2;
                return guid274;
            })());
        })());
    }
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> clockTickerState = Time::state();
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>> clockState = (juniper::shared_ptr<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(new juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>((([]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
        juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid275;
        guid275.month = september();
        guid275.day = ((uint8_t) 9);
        guid275.year = ((uint32_t) 2020);
        guid275.hours = ((uint8_t) 18);
        guid275.minutes = ((uint8_t) 40);
        guid275.seconds = ((uint8_t) 0);
        guid275.dayOfWeek = wednesday();
        return guid275;
    })()))));
}

namespace CWatch {
    juniper::unit processBluetoothUpdates() {
        return (([&]() -> juniper::unit {
            bool guid276 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayDateTime = guid276;
            
            bool guid277 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayOfWeek = guid277;
            
            (([&]() -> juniper::unit {
                
    hasNewDayDateTime = rawHasNewDayDateTime;
    rawHasNewDayDateTime = false;
    hasNewDayOfWeek = rawHasNewDayOfWeek;
    rawHasNewDayOfWeek = false;
    
                return {};
            })());
            (hasNewDayDateTime ? 
                (([&]() -> juniper::unit {
                    juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> guid278 = Ble::readGeneric<juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>>(dayDateTimeCharacterstic);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> bleData = guid278;
                    
                    (*((juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>*) (clockState.get())) = (([&]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                        juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid279;
                        guid279.month = (([&]() -> CWatch::month {
                            uint8_t guid280 = (bleData).month;
                            return (((guid280 == ((int32_t) 0)) && true) ? 
                                (([&]() -> CWatch::month {
                                    return january();
                                })())
                            :
                                (((guid280 == ((int32_t) 1)) && true) ? 
                                    (([&]() -> CWatch::month {
                                        return february();
                                    })())
                                :
                                    (((guid280 == ((int32_t) 2)) && true) ? 
                                        (([&]() -> CWatch::month {
                                            return march();
                                        })())
                                    :
                                        (((guid280 == ((int32_t) 3)) && true) ? 
                                            (([&]() -> CWatch::month {
                                                return april();
                                            })())
                                        :
                                            (((guid280 == ((int32_t) 4)) && true) ? 
                                                (([&]() -> CWatch::month {
                                                    return may();
                                                })())
                                            :
                                                (((guid280 == ((int32_t) 5)) && true) ? 
                                                    (([&]() -> CWatch::month {
                                                        return june();
                                                    })())
                                                :
                                                    (((guid280 == ((int32_t) 6)) && true) ? 
                                                        (([&]() -> CWatch::month {
                                                            return july();
                                                        })())
                                                    :
                                                        (((guid280 == ((int32_t) 7)) && true) ? 
                                                            (([&]() -> CWatch::month {
                                                                return august();
                                                            })())
                                                        :
                                                            (((guid280 == ((int32_t) 8)) && true) ? 
                                                                (([&]() -> CWatch::month {
                                                                    return september();
                                                                })())
                                                            :
                                                                (((guid280 == ((int32_t) 9)) && true) ? 
                                                                    (([&]() -> CWatch::month {
                                                                        return october();
                                                                    })())
                                                                :
                                                                    (((guid280 == ((int32_t) 10)) && true) ? 
                                                                        (([&]() -> CWatch::month {
                                                                            return november();
                                                                        })())
                                                                    :
                                                                        (((guid280 == ((int32_t) 11)) && true) ? 
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
                        })());
                        guid279.day = (bleData).day;
                        guid279.year = (bleData).year;
                        guid279.hours = (bleData).hours;
                        guid279.minutes = (bleData).minutes;
                        guid279.seconds = (bleData).seconds;
                        guid279.dayOfWeek = ((*((clockState).get()))).dayOfWeek;
                        return guid279;
                    })()));
                    return juniper::unit();
                })())
            :
                juniper::unit());
            return (hasNewDayOfWeek ? 
                (([&]() -> juniper::unit {
                    juniper::records::recordt_9<uint8_t> guid281 = Ble::readGeneric<juniper::records::recordt_9<uint8_t>>(dayOfWeekCharacterstic);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_9<uint8_t> bleData = guid281;
                    
                    (*((juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>*) (clockState.get())) = (([&]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                        juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid282;
                        guid282.month = ((*((clockState).get()))).month;
                        guid282.day = ((*((clockState).get()))).day;
                        guid282.year = ((*((clockState).get()))).year;
                        guid282.hours = ((*((clockState).get()))).hours;
                        guid282.minutes = ((*((clockState).get()))).minutes;
                        guid282.seconds = ((*((clockState).get()))).seconds;
                        guid282.dayOfWeek = (([&]() -> CWatch::dayOfWeek {
                            uint8_t guid283 = (bleData).dayOfWeek;
                            return (((guid283 == ((int32_t) 0)) && true) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return sunday();
                                })())
                            :
                                (((guid283 == ((int32_t) 1)) && true) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return monday();
                                    })())
                                :
                                    (((guid283 == ((int32_t) 2)) && true) ? 
                                        (([&]() -> CWatch::dayOfWeek {
                                            return tuesday();
                                        })())
                                    :
                                        (((guid283 == ((int32_t) 3)) && true) ? 
                                            (([&]() -> CWatch::dayOfWeek {
                                                return wednesday();
                                            })())
                                        :
                                            (((guid283 == ((int32_t) 4)) && true) ? 
                                                (([&]() -> CWatch::dayOfWeek {
                                                    return thursday();
                                                })())
                                            :
                                                (((guid283 == ((int32_t) 5)) && true) ? 
                                                    (([&]() -> CWatch::dayOfWeek {
                                                        return friday();
                                                    })())
                                                :
                                                    (((guid283 == ((int32_t) 6)) && true) ? 
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
                        })());
                        return guid282;
                    })()));
                    return juniper::unit();
                })())
            :
                juniper::unit());
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
    juniper::unit drawVerticalGradient(int16_t x0i, int16_t y0i, int16_t w, int16_t h, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c1, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c2) {
        return (([&]() -> juniper::unit {
            int16_t guid284 = toInt16<uint16_t>(Arcada::displayWidth());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispW = guid284;
            
            int16_t guid285 = toInt16<uint16_t>(Arcada::displayHeight());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispH = guid285;
            
            int16_t guid286 = Math::clamp<int16_t>(x0i, ((int16_t) 0), (dispW - ((int16_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t x0 = guid286;
            
            int16_t guid287 = Math::clamp<int16_t>(y0i, ((int16_t) 0), (dispH - ((int16_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y0 = guid287;
            
            int16_t guid288 = ((y0i + h) - ((int16_t) 1));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t ymax = guid288;
            
            int16_t guid289 = Math::clamp<int16_t>(ymax, ((int16_t) 0), (dispH - ((int16_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y1 = guid289;
            
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid290 = c1;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b1 = (guid290).b;
            uint8_t g1 = (guid290).g;
            uint8_t r1 = (guid290).r;
            
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid291 = c2;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b2 = (guid291).b;
            uint8_t g2 = (guid291).g;
            uint8_t r2 = (guid291).r;
            
            float guid292 = toFloat<uint8_t>(r1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r1f = guid292;
            
            float guid293 = toFloat<uint8_t>(g1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g1f = guid293;
            
            float guid294 = toFloat<uint8_t>(b1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b1f = guid294;
            
            float guid295 = toFloat<uint8_t>(r2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r2f = guid295;
            
            float guid296 = toFloat<uint8_t>(g2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g2f = guid296;
            
            float guid297 = toFloat<uint8_t>(b2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b2f = guid297;
            
            return (([&]() -> juniper::unit {
                int16_t guid298 = y0;
                int16_t guid299 = y1;
                for (int16_t y = guid298; y <= guid299; y++) {
                    (([&]() -> juniper::unit {
                        float guid300 = (toFloat<int16_t>((ymax - y)) / toFloat<int16_t>(h));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight1 = guid300;
                        
                        float guid301 = (1.000000 - weight1);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight2 = guid301;
                        
                        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid302 = (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
                            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid303;
                            guid303.r = toUInt8<float>(((r1f * weight1) + (r2f * weight2)));
                            guid303.g = toUInt8<float>(((g1f * weight1) + (g2f * weight2)));
                            guid303.b = toUInt8<float>(((b1f * weight1) + (g2f * weight2)));
                            return guid303;
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> gradColor = guid302;
                        
                        uint16_t guid304 = Color::rgbToRgb565(gradColor);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint16_t gradColor565 = guid304;
                        
                        return drawFastHLine565(x0, y, w, gradColor565);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace Gfx {
    template<int c138>
    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, c138>, uint32_t> cl) {
        return (([&]() -> juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> {
            constexpr int32_t n = c138;
            return (([&]() -> juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> {
                int16_t guid305 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t xret = guid305;
                
                int16_t guid306 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t yret = guid306;
                
                uint16_t guid307 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t wret = guid307;
                
                uint16_t guid308 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t hret = guid308;
                
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
    template<int c139>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c139>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c139;
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
            Gfx::font guid309 = f;
            return ((((guid309).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> juniper::unit {
                    return (([&]() -> juniper::unit {
                        arcada.getCanvas()->setFont();
                        return {};
                    })());
                })())
            :
                ((((guid309).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> juniper::unit {
                        return (([&]() -> juniper::unit {
                            arcada.getCanvas()->setFont(&FreeSans9pt7b);
                            return {};
                        })());
                    })())
                :
                    ((((guid309).id() == ((uint8_t) 2)) && true) ? 
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
    juniper::unit setTextColor(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c) {
        return (([&]() -> juniper::unit {
            uint16_t guid310 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid310;
            
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
            Signal::sink<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>, void>(juniper::function<void, juniper::unit(juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>)>([](juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> dt) -> juniper::unit { 
                return (([&]() -> juniper::unit {
                    juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid311 = dt;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    CWatch::dayOfWeek dayOfWeek = (guid311).dayOfWeek;
                    uint8_t seconds = (guid311).seconds;
                    uint8_t minutes = (guid311).minutes;
                    uint8_t hours = (guid311).hours;
                    uint32_t year = (guid311).year;
                    uint8_t day = (guid311).day;
                    CWatch::month month = (guid311).month;
                    
                    int32_t guid312 = toInt32<uint8_t>(((hours == ((uint8_t) 0)) ? 
                        ((uint8_t) 12)
                    :
                        ((hours > ((uint8_t) 12)) ? 
                            (hours - ((uint8_t) 12))
                        :
                            hours)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    int32_t displayHours = guid312;
                    
                    Gfx::setTextColor(Color::white);
                    Gfx::setFont(Gfx::freeSans24());
                    Gfx::setTextSize(((uint8_t) 1));
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid313 = CharList::i32ToCharList<2>(displayHours);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> timeHourStr = guid313;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid314 = CharList::safeConcat<3, 2>(timeHourStr, (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid315;
                        guid315.data = (juniper::array<uint8_t, 2> { {((uint8_t) 58), ((uint8_t) 0)} });
                        guid315.length = ((uint32_t) 2);
                        return guid315;
                    })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> timeHourStrColon = guid314;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid316 = ((minutes < ((uint8_t) 10)) ? 
                        CharList::safeConcat<2, 2>((([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid317;
                            guid317.data = (juniper::array<uint8_t, 2> { {((uint8_t) 48), ((uint8_t) 0)} });
                            guid317.length = ((uint32_t) 2);
                            return guid317;
                        })()), CharList::i32ToCharList<1>(toInt32<uint8_t>(minutes)))
                    :
                        CharList::i32ToCharList<2>(toInt32<uint8_t>(minutes)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> minutesStr = guid316;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> guid318 = CharList::safeConcat<4, 3>(timeHourStrColon, minutesStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> timeStr = guid318;
                    
                    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid319 = Gfx::getCharListBounds<6>(((int16_t) 0), ((int16_t) 0), timeStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h = (guid319).e4;
                    uint16_t w = (guid319).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((Arcada::displayWidth() / ((uint16_t) 2)) - (w / ((uint16_t) 2)))), toInt16<uint16_t>(((Arcada::displayHeight() / ((uint16_t) 2)) + (h / ((uint16_t) 2)))));
                    Gfx::printCharList<6>(timeStr);
                    Gfx::setTextSize(((uint8_t) 1));
                    Gfx::setFont(Gfx::freeSans9());
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid320 = ((hours < ((uint8_t) 12)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid321;
                            guid321.data = (juniper::array<uint8_t, 3> { {((uint8_t) 65), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid321.length = ((uint32_t) 3);
                            return guid321;
                        })())
                    :
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid322;
                            guid322.data = (juniper::array<uint8_t, 3> { {((uint8_t) 80), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid322.length = ((uint32_t) 3);
                            return guid322;
                        })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> ampm = guid320;
                    
                    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid323 = Gfx::getCharListBounds<3>(((int16_t) 0), ((int16_t) 0), ampm);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h2 = (guid323).e4;
                    uint16_t w2 = (guid323).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((Arcada::displayWidth() / ((uint16_t) 2)) - (w2 / ((uint16_t) 2)))), toInt16<uint16_t>(((((Arcada::displayHeight() / ((uint16_t) 2)) + (h / ((uint16_t) 2))) + h2) + ((uint16_t) 5))));
                    Gfx::printCharList<3>(ampm);
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> guid324 = CharList::safeConcat<10, 3>(CharList::safeConcat<9, 2>(CharList::safeConcat<6, 4>(CharList::safeConcat<4, 3>(dayOfWeekToCharList(dayOfWeek), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid325;
                        guid325.data = (juniper::array<uint8_t, 3> { {((uint8_t) 44), ((uint8_t) 32), ((uint8_t) 0)} });
                        guid325.length = ((uint32_t) 3);
                        return guid325;
                    })())), monthToCharList(month)), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid326;
                        guid326.data = (juniper::array<uint8_t, 2> { {((uint8_t) 32), ((uint8_t) 0)} });
                        guid326.length = ((uint32_t) 2);
                        return guid326;
                    })())), CharList::i32ToCharList<2>(toInt32<uint8_t>(day)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> dateStr = guid324;
                    
                    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid327 = Gfx::getCharListBounds<12>(((int16_t) 0), ((int16_t) 0), dateStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h3 = (guid327).e4;
                    uint16_t w3 = (guid327).e3;
                    
                    Gfx::setCursor(cast<uint16_t, int16_t>(((Arcada::displayWidth() / ((uint16_t) 2)) - (w3 / ((uint16_t) 2)))), cast<uint16_t, int16_t>((((Arcada::displayHeight() / ((uint16_t) 2)) - (h / ((uint16_t) 2))) - ((uint16_t) 5))));
                    return Gfx::printCharList<12>(dateStr);
                })());
             }), Signal::latch<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(clockState, Signal::foldP<uint32_t, juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>, void>(juniper::function<void, juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>(uint32_t,juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>)>([](uint32_t t, juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> dt) -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> { 
                return secondTick(dt);
             }), clockState, Time::every(((uint32_t) 1000), clockTickerState))));
            return Arcada::blitDoubleBuffer();
        })());
    }
}

namespace Gfx {
    juniper::unit fillScreen(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c) {
        return (([&]() -> juniper::unit {
            uint16_t guid328 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid328;
            
            return (([&]() -> juniper::unit {
                arcada.getCanvas()->fillScreen(cPrime);
                return {};
            })());
        })());
    }
}

namespace Gfx {
    juniper::unit drawPixel(int16_t x, int16_t y, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c) {
        return (([&]() -> juniper::unit {
            uint16_t guid329 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid329;
            
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
    template<int c160>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c160>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c160;
            return (([&]() -> juniper::unit {
                juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid330 = Gfx::getCharListBounds<c160>(((int16_t) 0), ((int16_t) 0), cl);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t h = (guid330).e4;
                uint16_t w = (guid330).e3;
                
                int16_t guid331 = toInt16<uint16_t>(w);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t ws = guid331;
                
                int16_t guid332 = toInt16<uint16_t>(h);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t hs = guid332;
                
                return (([&]() -> juniper::unit {
                    Gfx::align guid333 = align;
                    return ((((guid333).id() == ((uint8_t) 0)) && true) ? 
                        (([&]() -> juniper::unit {
                            return Gfx::setCursor((x - (ws / ((int16_t) 2))), y);
                        })())
                    :
                        ((((guid333).id() == ((uint8_t) 1)) && true) ? 
                            (([&]() -> juniper::unit {
                                return Gfx::setCursor(x, (y - (hs / ((int16_t) 2))));
                            })())
                        :
                            ((((guid333).id() == ((uint8_t) 2)) && true) ? 
                                (([&]() -> juniper::unit {
                                    return Gfx::setCursor((x - (ws / ((int16_t) 2))), (y - (hs / ((int16_t) 2))));
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
