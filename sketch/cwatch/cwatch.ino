//Compiled on 5/21/2023 6:54:34 PM
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

        void *get() { return data; }
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

namespace juniper {
    namespace records {
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
    void * extractptr(juniper::smartpointer p);
}

namespace Prelude {
    template<typename t10, typename t6, typename t7, typename t8, typename t9>
    juniper::function<juniper::closures::closuret_0<juniper::function<t8, t7(t6)>, juniper::function<t9, t6(t10)>>, t7(t10)> compose(juniper::function<t8, t7(t6)> f, juniper::function<t9, t6(t10)> g);
}

namespace Prelude {
    template<typename t20, typename t21, typename t22, typename t23>
    juniper::function<juniper::closures::closuret_1<juniper::function<t21, t20(t22, t23)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>, t20(t23)>(t22)> curry(juniper::function<t21, t20(t22, t23)> f);
}

namespace Prelude {
    template<typename t31, typename t32, typename t33, typename t34, typename t35>
    juniper::function<juniper::closures::closuret_1<juniper::function<t32, juniper::function<t33, t31(t35)>(t34)>>, t31(t34, t35)> uncurry(juniper::function<t32, juniper::function<t33, t31(t35)>(t34)> f);
}

namespace Prelude {
    template<typename t46, typename t47, typename t48, typename t49, typename t50>
    juniper::function<juniper::closures::closuret_1<juniper::function<t46, t47(t48, t49, t50)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>, juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)>(t49)>(t48)> curry3(juniper::function<t46, t47(t48, t49, t50)> f);
}

namespace Prelude {
    template<typename t60, typename t61, typename t62, typename t63, typename t64, typename t65, typename t66>
    juniper::function<juniper::closures::closuret_1<juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)>>, t63(t64, t65, t66)> uncurry3(juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)> f);
}

namespace Prelude {
    template<typename t77>
    bool eq(t77 x, t77 y);
}

namespace Prelude {
    template<typename t82>
    bool neq(t82 x, t82 y);
}

namespace Prelude {
    template<typename t86, typename t87>
    bool gt(t86 x, t87 y);
}

namespace Prelude {
    template<typename t91, typename t92>
    bool geq(t91 x, t92 y);
}

namespace Prelude {
    template<typename t96, typename t97>
    bool lt(t96 x, t97 y);
}

namespace Prelude {
    template<typename t101, typename t102>
    bool leq(t102 x, t101 y);
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
    template<typename t117, typename t118, typename t119>
    t118 apply(juniper::function<t119, t118(t117)> f, t117 x);
}

namespace Prelude {
    template<typename t124, typename t125, typename t126, typename t127>
    t126 apply2(juniper::function<t127, t126(t124, t125)> f, juniper::tuple2<t124,t125> tup);
}

namespace Prelude {
    template<typename t137, typename t138, typename t139, typename t140, typename t141>
    t141 apply3(juniper::function<t140, t141(t137, t138, t139)> f, juniper::tuple3<t137,t138,t139> tup);
}

namespace Prelude {
    template<typename t154, typename t155, typename t156, typename t157, typename t158, typename t159>
    t159 apply4(juniper::function<t157, t159(t154, t155, t156, t158)> f, juniper::tuple4<t154,t155,t156,t158> tup);
}

namespace Prelude {
    template<typename t175, typename t176>
    t175 fst(juniper::tuple2<t175,t176> tup);
}

namespace Prelude {
    template<typename t180, typename t181>
    t181 snd(juniper::tuple2<t180,t181> tup);
}

namespace Prelude {
    template<typename t185>
    t185 add(t185 numA, t185 numB);
}

namespace Prelude {
    template<typename t187>
    t187 sub(t187 numA, t187 numB);
}

namespace Prelude {
    template<typename t189>
    t189 mul(t189 numA, t189 numB);
}

namespace Prelude {
    template<typename t191>
    t191 div(t191 numA, t191 numB);
}

namespace Prelude {
    template<typename t195, typename t196>
    juniper::tuple2<t196,t195> swap(juniper::tuple2<t195,t196> tup);
}

namespace Prelude {
    template<typename t202, typename t203, typename t204>
    t202 until(juniper::function<t203, bool(t202)> p, juniper::function<t204, t202(t202)> f, t202 a0);
}

namespace Prelude {
    template<typename t212>
    juniper::unit ignore(t212 val);
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
    template<typename t270>
    uint8_t toUInt8(t270 n);
}

namespace Prelude {
    template<typename t272>
    int8_t toInt8(t272 n);
}

namespace Prelude {
    template<typename t274>
    uint16_t toUInt16(t274 n);
}

namespace Prelude {
    template<typename t276>
    int16_t toInt16(t276 n);
}

namespace Prelude {
    template<typename t278>
    uint32_t toUInt32(t278 n);
}

namespace Prelude {
    template<typename t280>
    int32_t toInt32(t280 n);
}

namespace Prelude {
    template<typename t282>
    float toFloat(t282 n);
}

namespace Prelude {
    template<typename t284>
    double toDouble(t284 n);
}

namespace Prelude {
    template<typename t>
    t fromUInt8(uint8_t n);
}

namespace Prelude {
    template<typename t>
    t fromInt8(int8_t n);
}

namespace Prelude {
    template<typename t>
    t fromUInt16(uint16_t n);
}

namespace Prelude {
    template<typename t>
    t fromInt16(int16_t n);
}

namespace Prelude {
    template<typename t>
    t fromUInt32(uint32_t n);
}

namespace Prelude {
    template<typename t>
    t fromInt32(int32_t n);
}

namespace Prelude {
    template<typename t>
    t fromFloat(float n);
}

namespace Prelude {
    template<typename t>
    t fromDouble(double n);
}

namespace Prelude {
    template<typename b, typename t294>
    b cast(t294 x);
}

namespace List {
    template<typename t298, typename t301, typename t305, int c4>
    juniper::records::recordt_0<juniper::array<t305, c4>, uint32_t> map(juniper::function<t298, t305(t301)> f, juniper::records::recordt_0<juniper::array<t301, c4>, uint32_t> lst);
}

namespace List {
    template<typename t308, typename t309, typename t313, int c7>
    t309 foldl(juniper::function<t308, t309(t313, t309)> f, t309 initState, juniper::records::recordt_0<juniper::array<t313, c7>, uint32_t> lst);
}

namespace List {
    template<typename t319, typename t320, typename t327, int c9>
    t320 foldr(juniper::function<t319, t320(t327, t320)> f, t320 initState, juniper::records::recordt_0<juniper::array<t327, c9>, uint32_t> lst);
}

namespace List {
    template<typename t343, int c11, int c12, int retCap>
    juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t> append(juniper::records::recordt_0<juniper::array<t343, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t343, c12>, uint32_t> lstB);
}

namespace List {
    template<typename t347, int c17>
    t347 nth(uint32_t i, juniper::records::recordt_0<juniper::array<t347, c17>, uint32_t> lst);
}

namespace List {
    template<typename t360, int c19, int c20>
    juniper::records::recordt_0<juniper::array<t360, (c20)*(c19)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t360, c19>, uint32_t>, c20>, uint32_t> listOfLists);
}

namespace List {
    template<typename t367, int c25, int m>
    juniper::records::recordt_0<juniper::array<t367, m>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t367, c25>, uint32_t> lst);
}

namespace List {
    template<typename t371, typename t375, int c28>
    bool all(juniper::function<t371, bool(t375)> pred, juniper::records::recordt_0<juniper::array<t375, c28>, uint32_t> lst);
}

namespace List {
    template<typename t380, typename t384, int c30>
    bool any(juniper::function<t380, bool(t384)> pred, juniper::records::recordt_0<juniper::array<t384, c30>, uint32_t> lst);
}

namespace List {
    template<typename t389, int c32>
    juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> pushBack(t389 elem, juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> lst);
}

namespace List {
    template<typename t404, int c34>
    juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> pushOffFront(t404 elem, juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> lst);
}

namespace List {
    template<typename t420, int c38>
    juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> setNth(uint32_t index, t420 elem, juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> lst);
}

namespace List {
    template<typename t425, int n>
    juniper::records::recordt_0<juniper::array<t425, n>, uint32_t> replicate(uint32_t numOfElements, t425 elem);
}

namespace List {
    template<typename t437, int c40>
    juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> remove(t437 elem, juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> lst);
}

namespace List {
    template<typename t441, int c44>
    juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> lst);
}

namespace List {
    template<typename t446, typename t450, int c45>
    juniper::unit foreach(juniper::function<t446, juniper::unit(t450)> f, juniper::records::recordt_0<juniper::array<t450, c45>, uint32_t> lst);
}

namespace List {
    template<typename t455, int c47>
    Prelude::maybe<t455> last(juniper::records::recordt_0<juniper::array<t455, c47>, uint32_t> lst);
}

namespace List {
    template<typename t479, int c49>
    Prelude::maybe<t479> max_(juniper::records::recordt_0<juniper::array<t479, c49>, uint32_t> lst);
}

namespace List {
    template<typename t496, int c53>
    Prelude::maybe<t496> min_(juniper::records::recordt_0<juniper::array<t496, c53>, uint32_t> lst);
}

namespace List {
    template<typename t504, int c57>
    bool member(t504 elem, juniper::records::recordt_0<juniper::array<t504, c57>, uint32_t> lst);
}

namespace Math {
    template<typename t509>
    t509 min_(t509 x, t509 y);
}

namespace List {
    template<typename t521, typename t523, int c59>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t521, c59>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t523, c59>, uint32_t> lstB);
}

namespace List {
    template<typename t534, typename t535, int c63>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t>,juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t534,t535>, c63>, uint32_t> lst);
}

namespace List {
    template<typename t543, int c67>
    t543 sum(juniper::records::recordt_0<juniper::array<t543, c67>, uint32_t> lst);
}

namespace List {
    template<typename t560, int c69>
    t560 average(juniper::records::recordt_0<juniper::array<t560, c69>, uint32_t> lst);
}

namespace Signal {
    template<typename t567, typename t569, typename t584>
    Prelude::sig<t584> map(juniper::function<t569, t584(t567)> f, Prelude::sig<t567> s);
}

namespace Signal {
    template<typename t591, typename t592>
    juniper::unit sink(juniper::function<t592, juniper::unit(t591)> f, Prelude::sig<t591> s);
}

namespace Signal {
    template<typename t600, typename t614>
    Prelude::sig<t614> filter(juniper::function<t600, bool(t614)> f, Prelude::sig<t614> s);
}

namespace Signal {
    template<typename t621>
    Prelude::sig<t621> merge(Prelude::sig<t621> sigA, Prelude::sig<t621> sigB);
}

namespace Signal {
    template<typename t625, int c71>
    Prelude::sig<t625> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t625>, c71>, uint32_t> sigs);
}

namespace Signal {
    template<typename t648, typename t672>
    Prelude::sig<Prelude::either<t672, t648>> join(Prelude::sig<t672> sigA, Prelude::sig<t648> sigB);
}

namespace Signal {
    template<typename t695>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t695> s);
}

namespace Signal {
    template<typename t701, typename t702, typename t721>
    Prelude::sig<t721> foldP(juniper::function<t702, t721(t701, t721)> f, juniper::shared_ptr<t721> state0, Prelude::sig<t701> incoming);
}

namespace Signal {
    template<typename t731>
    Prelude::sig<t731> dropRepeats(juniper::shared_ptr<Prelude::maybe<t731>> maybePrevValue, Prelude::sig<t731> incoming);
}

namespace Signal {
    template<typename t749>
    Prelude::sig<t749> latch(juniper::shared_ptr<t749> prevValue, Prelude::sig<t749> incoming);
}

namespace Signal {
    template<typename t761, typename t762, typename t765, typename t774>
    Prelude::sig<t761> map2(juniper::function<t762, t761(t765, t774)> f, juniper::shared_ptr<juniper::tuple2<t765,t774>> state, Prelude::sig<t765> incomingA, Prelude::sig<t774> incomingB);
}

namespace Signal {
    template<typename t806, int c73>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t806, c73>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t806, c73>, uint32_t>> pastValues, Prelude::sig<t806> incoming);
}

namespace Signal {
    template<typename t817>
    Prelude::sig<t817> constant(t817 val);
}

namespace Signal {
    template<typename t827>
    Prelude::sig<Prelude::maybe<t827>> meta(Prelude::sig<t827> sigA);
}

namespace Signal {
    template<typename t844>
    Prelude::sig<t844> unmeta(Prelude::sig<Prelude::maybe<t844>> sigA);
}

namespace Signal {
    template<typename t857, typename t858>
    Prelude::sig<juniper::tuple2<t857,t858>> zip(juniper::shared_ptr<juniper::tuple2<t857,t858>> state, Prelude::sig<t857> sigA, Prelude::sig<t858> sigB);
}

namespace Signal {
    template<typename t889, typename t896>
    juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>> unzip(Prelude::sig<juniper::tuple2<t889,t896>> incoming);
}

namespace Signal {
    template<typename t903, typename t904>
    Prelude::sig<t903> toggle(t903 val1, t903 val2, juniper::shared_ptr<t903> state, Prelude::sig<t904> incoming);
}

namespace Io {
    Io::pinState toggle(Io::pinState p);
}

namespace Io {
    juniper::unit printStr(const char * str);
}

namespace Io {
    template<int c75>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c75>, uint32_t> cl);
}

namespace Io {
    juniper::unit printFloat(float f);
}

namespace Io {
    juniper::unit printInt(int32_t n);
}

namespace Io {
    template<typename t930>
    t930 baseToInt(Io::base b);
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
    template<typename t1061, typename t1063, typename t1072>
    Prelude::maybe<t1072> map(juniper::function<t1063, t1072(t1061)> f, Prelude::maybe<t1061> maybeVal);
}

namespace Maybe {
    template<typename t1076>
    t1076 get(Prelude::maybe<t1076> maybeVal);
}

namespace Maybe {
    template<typename t1079>
    bool isJust(Prelude::maybe<t1079> maybeVal);
}

namespace Maybe {
    template<typename t1082>
    bool isNothing(Prelude::maybe<t1082> maybeVal);
}

namespace Maybe {
    template<typename t1088>
    uint8_t count(Prelude::maybe<t1088> maybeVal);
}

namespace Maybe {
    template<typename t1093, typename t1094, typename t1095>
    t1094 foldl(juniper::function<t1093, t1094(t1095, t1094)> f, t1094 initState, Prelude::maybe<t1095> maybeVal);
}

namespace Maybe {
    template<typename t1102, typename t1103, typename t1104>
    t1103 fodlr(juniper::function<t1102, t1103(t1104, t1103)> f, t1103 initState, Prelude::maybe<t1104> maybeVal);
}

namespace Maybe {
    template<typename t1114, typename t1115>
    juniper::unit iter(juniper::function<t1115, juniper::unit(t1114)> f, Prelude::maybe<t1114> maybeVal);
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
    juniper::tuple2<double,int32_t> frexp_(double x);
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
    template<typename t1181>
    t1181 max_(t1181 x, t1181 y);
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2);
}

namespace Math {
    template<typename t1184>
    t1184 clamp(t1184 x, t1184 min, t1184 max);
}

namespace Math {
    template<typename t1189>
    int8_t sign(t1189 n);
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
    template<typename t1234, int c76>
    juniper::records::recordt_3<juniper::array<t1234, c76>> make(juniper::array<t1234, c76> d);
}

namespace Vector {
    template<typename t1236, int c77>
    t1236 get(uint32_t i, juniper::records::recordt_3<juniper::array<t1236, c77>> v);
}

namespace Vector {
    template<typename t1245, int c79>
    juniper::records::recordt_3<juniper::array<t1245, c79>> add(juniper::records::recordt_3<juniper::array<t1245, c79>> v1, juniper::records::recordt_3<juniper::array<t1245, c79>> v2);
}

namespace Vector {
    template<typename a, int n>
    juniper::records::recordt_3<juniper::array<a, n>> zero();
}

namespace Vector {
    template<typename t1256, int c83>
    juniper::records::recordt_3<juniper::array<t1256, c83>> subtract(juniper::records::recordt_3<juniper::array<t1256, c83>> v1, juniper::records::recordt_3<juniper::array<t1256, c83>> v2);
}

namespace Vector {
    template<typename t1260, int c87>
    juniper::records::recordt_3<juniper::array<t1260, c87>> scale(t1260 scalar, juniper::records::recordt_3<juniper::array<t1260, c87>> v);
}

namespace Vector {
    template<typename t1273, int c90>
    t1273 dot(juniper::records::recordt_3<juniper::array<t1273, c90>> v1, juniper::records::recordt_3<juniper::array<t1273, c90>> v2);
}

namespace Vector {
    template<typename t1281, int c93>
    t1281 magnitude2(juniper::records::recordt_3<juniper::array<t1281, c93>> v);
}

namespace Vector {
    template<typename t1283, int c96>
    double magnitude(juniper::records::recordt_3<juniper::array<t1283, c96>> v);
}

namespace Vector {
    template<typename t1301, int c98>
    juniper::records::recordt_3<juniper::array<t1301, c98>> multiply(juniper::records::recordt_3<juniper::array<t1301, c98>> u, juniper::records::recordt_3<juniper::array<t1301, c98>> v);
}

namespace Vector {
    template<typename t1312, int c102>
    juniper::records::recordt_3<juniper::array<t1312, c102>> normalize(juniper::records::recordt_3<juniper::array<t1312, c102>> v);
}

namespace Vector {
    template<typename t1325, int c106>
    double angle(juniper::records::recordt_3<juniper::array<t1325, c106>> v1, juniper::records::recordt_3<juniper::array<t1325, c106>> v2);
}

namespace Vector {
    template<typename t1354>
    juniper::records::recordt_3<juniper::array<t1354, 3>> cross(juniper::records::recordt_3<juniper::array<t1354, 3>> u, juniper::records::recordt_3<juniper::array<t1354, 3>> v);
}

namespace Vector {
    template<typename t1381, int c122>
    juniper::records::recordt_3<juniper::array<t1381, c122>> project(juniper::records::recordt_3<juniper::array<t1381, c122>> a, juniper::records::recordt_3<juniper::array<t1381, c122>> b);
}

namespace Vector {
    template<typename t1397, int c126>
    juniper::records::recordt_3<juniper::array<t1397, c126>> projectPlane(juniper::records::recordt_3<juniper::array<t1397, c126>> a, juniper::records::recordt_3<juniper::array<t1397, c126>> m);
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
    template<int n>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t> i32ToCharList(int32_t m);
}

namespace CharList {
    template<int c131>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c131>, uint32_t> s);
}

namespace CharList {
    template<int c132, int c133, int retCap>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c132)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c133)>, uint32_t> sB);
}

namespace CharList {
    template<int c140, int c141>
    juniper::records::recordt_0<juniper::array<uint8_t, ((1)+(c140))+(c141)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c140)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c141)>, uint32_t> sB);
}

namespace Random {
    int32_t random_(int32_t low, int32_t high);
}

namespace Random {
    juniper::unit seed(uint32_t n);
}

namespace Random {
    template<typename t1463, int c145>
    t1463 choice(juniper::records::recordt_0<juniper::array<t1463, c145>, uint32_t> lst);
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
    template<int c147>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c147>, uint32_t> bytes);
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
    template<typename t1792>
    uint16_t writeGeneric(Ble::characterstict c, t1792 x);
}

namespace Ble {
    template<typename t>
    t readGeneric(Ble::characterstict c);
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
    template<int c148>
    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c148)>, uint32_t> cl);
}

namespace Gfx {
    template<int c149>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c149)>, uint32_t> cl);
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
    template<int c174>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c174>, uint32_t> cl);
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
    template<typename t10, typename t6, typename t7, typename t8, typename t9>
    juniper::function<juniper::closures::closuret_0<juniper::function<t8, t7(t6)>, juniper::function<t9, t6(t10)>>, t7(t10)> compose(juniper::function<t8, t7(t6)> f, juniper::function<t9, t6(t10)> g) {
        return (([&]() -> juniper::function<juniper::closures::closuret_0<juniper::function<t8, t7(t6)>, juniper::function<t9, t6(t10)>>, t7(t10)> {
            using a = t10;
            using b = t6;
            using c = t7;
            using closureF = t8;
            using closureG = t9;
            return juniper::function<juniper::closures::closuret_0<juniper::function<t8, t7(t6)>, juniper::function<t9, t6(t10)>>, t7(t10)>(juniper::closures::closuret_0<juniper::function<t8, t7(t6)>, juniper::function<t9, t6(t10)>>(f, g), [](juniper::closures::closuret_0<juniper::function<t8, t7(t6)>, juniper::function<t9, t6(t10)>>& junclosure, t10 x) -> t7 { 
                juniper::function<t8, t7(t6)>& f = junclosure.f;
                juniper::function<t9, t6(t10)>& g = junclosure.g;
                return f(g(x));
             });
        })());
    }
}

namespace Prelude {
    template<typename t20, typename t21, typename t22, typename t23>
    juniper::function<juniper::closures::closuret_1<juniper::function<t21, t20(t22, t23)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>, t20(t23)>(t22)> curry(juniper::function<t21, t20(t22, t23)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t21, t20(t22, t23)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>, t20(t23)>(t22)> {
            using a = t22;
            using b = t23;
            using c = t20;
            using closure = t21;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t21, t20(t22, t23)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>, t20(t23)>(t22)>(juniper::closures::closuret_1<juniper::function<t21, t20(t22, t23)>>(f), [](juniper::closures::closuret_1<juniper::function<t21, t20(t22, t23)>>& junclosure, t22 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>, t20(t23)> { 
                juniper::function<t21, t20(t22, t23)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>, t20(t23)>(juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t21, t20(t22, t23)>, t22>& junclosure, t23 valueB) -> t20 { 
                    juniper::function<t21, t20(t22, t23)>& f = junclosure.f;
                    t22& valueA = junclosure.valueA;
                    return f(valueA, valueB);
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t31, typename t32, typename t33, typename t34, typename t35>
    juniper::function<juniper::closures::closuret_1<juniper::function<t32, juniper::function<t33, t31(t35)>(t34)>>, t31(t34, t35)> uncurry(juniper::function<t32, juniper::function<t33, t31(t35)>(t34)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t32, juniper::function<t33, t31(t35)>(t34)>>, t31(t34, t35)> {
            using a = t34;
            using b = t35;
            using c = t31;
            using closureA = t32;
            using closureB = t33;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t32, juniper::function<t33, t31(t35)>(t34)>>, t31(t34,t35)>(juniper::closures::closuret_1<juniper::function<t32, juniper::function<t33, t31(t35)>(t34)>>(f), [](juniper::closures::closuret_1<juniper::function<t32, juniper::function<t33, t31(t35)>(t34)>>& junclosure, t34 valueA, t35 valueB) -> t31 { 
                juniper::function<t32, juniper::function<t33, t31(t35)>(t34)>& f = junclosure.f;
                return f(valueA)(valueB);
             });
        })());
    }
}

namespace Prelude {
    template<typename t46, typename t47, typename t48, typename t49, typename t50>
    juniper::function<juniper::closures::closuret_1<juniper::function<t46, t47(t48, t49, t50)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>, juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)>(t49)>(t48)> curry3(juniper::function<t46, t47(t48, t49, t50)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t46, t47(t48, t49, t50)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>, juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)>(t49)>(t48)> {
            using a = t48;
            using b = t49;
            using c = t50;
            using closureF = t46;
            using d = t47;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t46, t47(t48, t49, t50)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>, juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)>(t49)>(t48)>(juniper::closures::closuret_1<juniper::function<t46, t47(t48, t49, t50)>>(f), [](juniper::closures::closuret_1<juniper::function<t46, t47(t48, t49, t50)>>& junclosure, t48 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>, juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)>(t49)> { 
                juniper::function<t46, t47(t48, t49, t50)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>, juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)>(t49)>(juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t46, t47(t48, t49, t50)>, t48>& junclosure, t49 valueB) -> juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)> { 
                    juniper::function<t46, t47(t48, t49, t50)>& f = junclosure.f;
                    t48& valueA = junclosure.valueA;
                    return juniper::function<juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>, t47(t50)>(juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>(f, valueA, valueB), [](juniper::closures::closuret_3<juniper::function<t46, t47(t48, t49, t50)>, t48, t49>& junclosure, t50 valueC) -> t47 { 
                        juniper::function<t46, t47(t48, t49, t50)>& f = junclosure.f;
                        t48& valueA = junclosure.valueA;
                        t49& valueB = junclosure.valueB;
                        return f(valueA, valueB, valueC);
                     });
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t60, typename t61, typename t62, typename t63, typename t64, typename t65, typename t66>
    juniper::function<juniper::closures::closuret_1<juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)>>, t63(t64, t65, t66)> uncurry3(juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)>>, t63(t64, t65, t66)> {
            using a = t64;
            using b = t65;
            using c = t66;
            using closureA = t60;
            using closureB = t61;
            using closureC = t62;
            using d = t63;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)>>, t63(t64,t65,t66)>(juniper::closures::closuret_1<juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)>>(f), [](juniper::closures::closuret_1<juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)>>& junclosure, t64 valueA, t65 valueB, t66 valueC) -> t63 { 
                juniper::function<t60, juniper::function<t61, juniper::function<t62, t63(t66)>(t65)>(t64)>& f = junclosure.f;
                return f(valueA)(valueB)(valueC);
             });
        })());
    }
}

namespace Prelude {
    template<typename t77>
    bool eq(t77 x, t77 y) {
        return (([&]() -> bool {
            using a = t77;
            return (x == y);
        })());
    }
}

namespace Prelude {
    template<typename t82>
    bool neq(t82 x, t82 y) {
        return (x != y);
    }
}

namespace Prelude {
    template<typename t86, typename t87>
    bool gt(t86 x, t87 y) {
        return (x > y);
    }
}

namespace Prelude {
    template<typename t91, typename t92>
    bool geq(t91 x, t92 y) {
        return (x >= y);
    }
}

namespace Prelude {
    template<typename t96, typename t97>
    bool lt(t96 x, t97 y) {
        return (x < y);
    }
}

namespace Prelude {
    template<typename t101, typename t102>
    bool leq(t102 x, t101 y) {
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
    template<typename t117, typename t118, typename t119>
    t118 apply(juniper::function<t119, t118(t117)> f, t117 x) {
        return (([&]() -> t118 {
            using a = t117;
            using b = t118;
            using closure = t119;
            return f(x);
        })());
    }
}

namespace Prelude {
    template<typename t124, typename t125, typename t126, typename t127>
    t126 apply2(juniper::function<t127, t126(t124, t125)> f, juniper::tuple2<t124,t125> tup) {
        return (([&]() -> t126 {
            using a = t124;
            using b = t125;
            using c = t126;
            using closure = t127;
            return (([&]() -> t126 {
                juniper::tuple2<t124,t125> guid0 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t125 b = (guid0).e2;
                t124 a = (guid0).e1;
                
                return f(a, b);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t137, typename t138, typename t139, typename t140, typename t141>
    t141 apply3(juniper::function<t140, t141(t137, t138, t139)> f, juniper::tuple3<t137,t138,t139> tup) {
        return (([&]() -> t141 {
            using a = t137;
            using b = t138;
            using c = t139;
            using closure = t140;
            using d = t141;
            return (([&]() -> t141 {
                juniper::tuple3<t137,t138,t139> guid1 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t139 c = (guid1).e3;
                t138 b = (guid1).e2;
                t137 a = (guid1).e1;
                
                return f(a, b, c);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t154, typename t155, typename t156, typename t157, typename t158, typename t159>
    t159 apply4(juniper::function<t157, t159(t154, t155, t156, t158)> f, juniper::tuple4<t154,t155,t156,t158> tup) {
        return (([&]() -> t159 {
            using a = t154;
            using b = t155;
            using c = t156;
            using closure = t157;
            using d = t158;
            using e = t159;
            return (([&]() -> t159 {
                juniper::tuple4<t154,t155,t156,t158> guid2 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t158 d = (guid2).e4;
                t156 c = (guid2).e3;
                t155 b = (guid2).e2;
                t154 a = (guid2).e1;
                
                return f(a, b, c, d);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t175, typename t176>
    t175 fst(juniper::tuple2<t175,t176> tup) {
        return (([&]() -> t175 {
            using a = t175;
            using b = t176;
            return (([&]() -> t175 {
                juniper::tuple2<t175,t176> guid3 = tup;
                return (true ? 
                    (([&]() -> t175 {
                        t175 x = (guid3).e1;
                        return x;
                    })())
                :
                    juniper::quit<t175>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t180, typename t181>
    t181 snd(juniper::tuple2<t180,t181> tup) {
        return (([&]() -> t181 {
            using a = t180;
            using b = t181;
            return (([&]() -> t181 {
                juniper::tuple2<t180,t181> guid4 = tup;
                return (true ? 
                    (([&]() -> t181 {
                        t181 x = (guid4).e2;
                        return x;
                    })())
                :
                    juniper::quit<t181>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t185>
    t185 add(t185 numA, t185 numB) {
        return (([&]() -> t185 {
            using a = t185;
            return (numA + numB);
        })());
    }
}

namespace Prelude {
    template<typename t187>
    t187 sub(t187 numA, t187 numB) {
        return (([&]() -> t187 {
            using a = t187;
            return (numA - numB);
        })());
    }
}

namespace Prelude {
    template<typename t189>
    t189 mul(t189 numA, t189 numB) {
        return (([&]() -> t189 {
            using a = t189;
            return (numA * numB);
        })());
    }
}

namespace Prelude {
    template<typename t191>
    t191 div(t191 numA, t191 numB) {
        return (([&]() -> t191 {
            using a = t191;
            return (numA / numB);
        })());
    }
}

namespace Prelude {
    template<typename t195, typename t196>
    juniper::tuple2<t196,t195> swap(juniper::tuple2<t195,t196> tup) {
        return (([&]() -> juniper::tuple2<t196,t195> {
            juniper::tuple2<t195,t196> guid5 = tup;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            t196 beta = (guid5).e2;
            t195 alpha = (guid5).e1;
            
            return (juniper::tuple2<t196,t195>{beta, alpha});
        })());
    }
}

namespace Prelude {
    template<typename t202, typename t203, typename t204>
    t202 until(juniper::function<t203, bool(t202)> p, juniper::function<t204, t202(t202)> f, t202 a0) {
        return (([&]() -> t202 {
            using a = t202;
            return (([&]() -> t202 {
                t202 guid6 = a0;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t202 a = guid6;
                
                (([&]() -> juniper::unit {
                    while (!(p(a))) {
                        (([&]() -> t202 {
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
    template<typename t212>
    juniper::unit ignore(t212 val) {
        return (([&]() -> juniper::unit {
            using a = t212;
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
    template<typename t270>
    uint8_t toUInt8(t270 n) {
        return (([&]() -> uint8_t {
            using t = t270;
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
    template<typename t272>
    int8_t toInt8(t272 n) {
        return (([&]() -> int8_t {
            using t = t272;
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
    template<typename t274>
    uint16_t toUInt16(t274 n) {
        return (([&]() -> uint16_t {
            using t = t274;
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
    template<typename t276>
    int16_t toInt16(t276 n) {
        return (([&]() -> int16_t {
            using t = t276;
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
    template<typename t278>
    uint32_t toUInt32(t278 n) {
        return (([&]() -> uint32_t {
            using t = t278;
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
    template<typename t280>
    int32_t toInt32(t280 n) {
        return (([&]() -> int32_t {
            using t = t280;
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
    template<typename t282>
    float toFloat(t282 n) {
        return (([&]() -> float {
            using t = t282;
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
    template<typename t284>
    double toDouble(t284 n) {
        return (([&]() -> double {
            using t = t284;
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
    template<typename t>
    t fromUInt8(uint8_t n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t>
    t fromInt8(int8_t n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t>
    t fromUInt16(uint16_t n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t>
    t fromInt16(int16_t n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t>
    t fromUInt32(uint32_t n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t>
    t fromInt32(int32_t n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t>
    t fromFloat(float n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t>
    t fromDouble(double n) {
        return (([&]() -> t {
            t ret;
            
            (([&]() -> juniper::unit {
                ret = (t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename b, typename t294>
    b cast(t294 x) {
        return (([&]() -> b {
            using a = t294;
            return (([&]() -> b {
                b ret;
                
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
    template<typename t298, typename t301, typename t305, int c4>
    juniper::records::recordt_0<juniper::array<t305, c4>, uint32_t> map(juniper::function<t298, t305(t301)> f, juniper::records::recordt_0<juniper::array<t301, c4>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t305, c4>, uint32_t> {
            using a = t301;
            using b = t305;
            using closure = t298;
            constexpr int32_t n = c4;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t305, c4>, uint32_t> {
                juniper::array<t305, c4> guid7 = (juniper::array<t305, c4>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t305, c4> ret = guid7;
                
                (([&]() -> juniper::unit {
                    uint32_t guid8 = ((uint32_t) 0);
                    uint32_t guid9 = (lst).length;
                    for (uint32_t i = guid8; i < guid9; i++) {
                        (([&]() -> t305 {
                            return ((ret)[i] = f(((lst).data)[i]));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t305, c4>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t305, c4>, uint32_t> guid10;
                    guid10.data = ret;
                    guid10.length = (lst).length;
                    return guid10;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t308, typename t309, typename t313, int c7>
    t309 foldl(juniper::function<t308, t309(t313, t309)> f, t309 initState, juniper::records::recordt_0<juniper::array<t313, c7>, uint32_t> lst) {
        return (([&]() -> t309 {
            using closure = t308;
            using state = t309;
            using t = t313;
            constexpr int32_t n = c7;
            return (([&]() -> t309 {
                t309 guid11 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t309 s = guid11;
                
                (([&]() -> juniper::unit {
                    uint32_t guid12 = ((uint32_t) 0);
                    uint32_t guid13 = (lst).length;
                    for (uint32_t i = guid12; i < guid13; i++) {
                        (([&]() -> t309 {
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
    template<typename t319, typename t320, typename t327, int c9>
    t320 foldr(juniper::function<t319, t320(t327, t320)> f, t320 initState, juniper::records::recordt_0<juniper::array<t327, c9>, uint32_t> lst) {
        return (([&]() -> t320 {
            using closure = t319;
            using state = t320;
            using t = t327;
            constexpr int32_t n = c9;
            return (((lst).length == ((uint32_t) 0)) ? 
                (([&]() -> t320 {
                    return initState;
                })())
            :
                (([&]() -> t320 {
                    t320 guid14 = initState;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t320 s = guid14;
                    
                    uint32_t guid15 = (lst).length;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint32_t i = guid15;
                    
                    (([&]() -> juniper::unit {
                        do {
                            (([&]() -> t320 {
                                (i = (i - ((uint32_t) 1)));
                                return (s = f(((lst).data)[i], s));
                            })());
                        } while((i > ((uint32_t) 0)));
                        return {};
                    })());
                    return s;
                })()));
        })());
    }
}

namespace List {
    template<typename t343, int c11, int c12, int retCap>
    juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t> append(juniper::records::recordt_0<juniper::array<t343, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t343, c12>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t> {
            using t = t343;
            constexpr int32_t aCap = c11;
            constexpr int32_t bCap = c12;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t> {
                uint32_t guid16 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid16;
                
                juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t> guid17 = (([&]() -> juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t> guid18;
                    guid18.data = (juniper::array<t343, retCap>());
                    guid18.length = ((lstA).length + (lstB).length);
                    return guid18;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t343, retCap>, uint32_t> out = guid17;
                
                (([&]() -> juniper::unit {
                    uint32_t guid19 = ((uint32_t) 0);
                    uint32_t guid20 = (lstA).length;
                    for (uint32_t i = guid19; i < guid20; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((lstA).data)[i]);
                            return (j = (j + ((uint32_t) 1)));
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
                            return (j = (j + ((uint32_t) 1)));
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
    template<typename t347, int c17>
    t347 nth(uint32_t i, juniper::records::recordt_0<juniper::array<t347, c17>, uint32_t> lst) {
        return (([&]() -> t347 {
            using t = t347;
            constexpr int32_t n = c17;
            return ((i < (lst).length) ? 
                (([&]() -> t347 {
                    return ((lst).data)[i];
                })())
            :
                (([&]() -> t347 {
                    return juniper::quit<t347>();
                })()));
        })());
    }
}

namespace List {
    template<typename t360, int c19, int c20>
    juniper::records::recordt_0<juniper::array<t360, (c20)*(c19)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t360, c19>, uint32_t>, c20>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t360, (c20)*(c19)>, uint32_t> {
            using t = t360;
            constexpr int32_t m = c19;
            constexpr int32_t n = c20;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t360, (c20)*(c19)>, uint32_t> {
                juniper::array<t360, (c20)*(c19)> guid23 = (juniper::array<t360, (c20)*(c19)>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t360, (c20)*(c19)> ret = guid23;
                
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
                                        return (index = (index + ((uint32_t) 1)));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t360, (c20)*(c19)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t360, (c20)*(c19)>, uint32_t> guid29;
                    guid29.data = ret;
                    guid29.length = index;
                    return guid29;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t367, int c25, int m>
    juniper::records::recordt_0<juniper::array<t367, m>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t367, c25>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t367, m>, uint32_t> {
            using t = t367;
            constexpr int32_t n = c25;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t367, m>, uint32_t> {
                juniper::array<t367, m> guid30 = (juniper::array<t367, m>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t367, m> ret = guid30;
                
                (([&]() -> juniper::unit {
                    uint32_t guid31 = ((uint32_t) 0);
                    uint32_t guid32 = (lst).length;
                    for (uint32_t i = guid31; i < guid32; i++) {
                        (([&]() -> t367 {
                            return ((ret)[i] = ((lst).data)[i]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t367, m>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t367, m>, uint32_t> guid33;
                    guid33.data = ret;
                    guid33.length = (lst).length;
                    return guid33;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t371, typename t375, int c28>
    bool all(juniper::function<t371, bool(t375)> pred, juniper::records::recordt_0<juniper::array<t375, c28>, uint32_t> lst) {
        return (([&]() -> bool {
            using closure = t371;
            using t = t375;
            constexpr int32_t n = c28;
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
                            return (satisfied ? 
                                (([&]() -> juniper::unit {
                                    (satisfied = pred(((lst).data)[i]));
                                    return juniper::unit();
                                })())
                            :
                                (([&]() -> juniper::unit {
                                    return juniper::unit();
                                })()));
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
    template<typename t380, typename t384, int c30>
    bool any(juniper::function<t380, bool(t384)> pred, juniper::records::recordt_0<juniper::array<t384, c30>, uint32_t> lst) {
        return (([&]() -> bool {
            using closure = t380;
            using t = t384;
            constexpr int32_t n = c30;
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
                            return (!(satisfied) ? 
                                (([&]() -> juniper::unit {
                                    (satisfied = pred(((lst).data)[i]));
                                    return juniper::unit();
                                })())
                            :
                                (([&]() -> juniper::unit {
                                    return juniper::unit();
                                })()));
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
    template<typename t389, int c32>
    juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> pushBack(t389 elem, juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> {
            using t = t389;
            constexpr int32_t n = c32;
            return (((lst).length >= n) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> {
                    return juniper::quit<juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t>>();
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> guid40 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t389, c32>, uint32_t> ret = guid40;
                    
                    (((ret).data)[(lst).length] = elem);
                    ((ret).length = ((lst).length + ((uint32_t) 1)));
                    return ret;
                })()));
        })());
    }
}

namespace List {
    template<typename t404, int c34>
    juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> pushOffFront(t404 elem, juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> {
            using t = t404;
            constexpr int32_t n = c34;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> {
                return ((n <= ((int32_t) 0)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> {
                        return lst;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t404, c34>, uint32_t> ret;
                        
                        (((ret).data)[((uint32_t) 0)] = elem);
                        (([&]() -> juniper::unit {
                            uint32_t guid41 = ((uint32_t) 0);
                            uint32_t guid42 = (lst).length;
                            for (uint32_t i = guid41; i < guid42; i++) {
                                (([&]() -> juniper::unit {
                                    return (((i + ((uint32_t) 1)) < n) ? 
                                        (([&]() -> juniper::unit {
                                            (((ret).data)[(i + ((uint32_t) 1))] = ((lst).data)[i]);
                                            return juniper::unit();
                                        })())
                                    :
                                        (([&]() -> juniper::unit {
                                            return juniper::unit();
                                        })()));
                                })());
                            }
                            return {};
                        })());
                        (((lst).length == cast<uint32_t, int32_t>(n)) ? 
                            (([&]() -> uint32_t {
                                return ((ret).length = (lst).length);
                            })())
                        :
                            (([&]() -> uint32_t {
                                return ((ret).length = ((lst).length + ((uint32_t) 1)));
                            })()));
                        return ret;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t420, int c38>
    juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> setNth(uint32_t index, t420 elem, juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> {
            using t = t420;
            constexpr int32_t n = c38;
            return ((index < (lst).length) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> guid43 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> ret = guid43;
                    
                    (((ret).data)[index] = elem);
                    return ret;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t420, c38>, uint32_t> {
                    return lst;
                })()));
        })());
    }
}

namespace List {
    template<typename t425, int n>
    juniper::records::recordt_0<juniper::array<t425, n>, uint32_t> replicate(uint32_t numOfElements, t425 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t425, n>, uint32_t> {
            using t = t425;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t425, n>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t425, n>, uint32_t> guid44;
                guid44.data = (juniper::array<t425, n>().fill(elem));
                guid44.length = numOfElements;
                return guid44;
            })());
        })());
    }
}

namespace List {
    template<typename t437, int c40>
    juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> remove(t437 elem, juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> {
            using t = t437;
            constexpr int32_t n = c40;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> {
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
                            return ((!(found) && (((lst).data)[i] == elem)) ? 
                                (([&]() -> juniper::unit {
                                    (index = i);
                                    (found = true);
                                    return juniper::unit();
                                })())
                            :
                                (([&]() -> juniper::unit {
                                    return juniper::unit();
                                })()));
                        })());
                    }
                    return {};
                })());
                return (found ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> guid49 = lst;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> ret = guid49;
                        
                        ((ret).length = ((lst).length - ((uint32_t) 1)));
                        (([&]() -> juniper::unit {
                            uint32_t guid50 = index;
                            uint32_t guid51 = ((lst).length - ((uint32_t) 1));
                            for (uint32_t i = guid50; i < guid51; i++) {
                                (([&]() -> t437 {
                                    return (((ret).data)[i] = ((lst).data)[(i + ((uint32_t) 1))]);
                                })());
                            }
                            return {};
                        })());
                        return ret;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t437, c40>, uint32_t> {
                        return lst;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t441, int c44>
    juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> {
            using t = t441;
            constexpr int32_t n = c44;
            return (((lst).length == ((uint32_t) 0)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> {
                    return lst;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<t441, c44>, uint32_t> guid52;
                        guid52.data = (lst).data;
                        guid52.length = ((lst).length - ((uint32_t) 1));
                        return guid52;
                    })());
                })()));
        })());
    }
}

namespace List {
    template<typename t446, typename t450, int c45>
    juniper::unit foreach(juniper::function<t446, juniper::unit(t450)> f, juniper::records::recordt_0<juniper::array<t450, c45>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            using closure = t446;
            using t = t450;
            constexpr int32_t n = c45;
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
    template<typename t455, int c47>
    Prelude::maybe<t455> last(juniper::records::recordt_0<juniper::array<t455, c47>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t455> {
            using t = t455;
            constexpr int32_t n = c47;
            return (((lst).length == ((uint32_t) 0)) ? 
                (([&]() -> Prelude::maybe<t455> {
                    return nothing<t455>();
                })())
            :
                (([&]() -> Prelude::maybe<t455> {
                    return just<t455>(((lst).data)[((lst).length - ((uint32_t) 1))]);
                })()));
        })());
    }
}

namespace List {
    template<typename t479, int c49>
    Prelude::maybe<t479> max_(juniper::records::recordt_0<juniper::array<t479, c49>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t479> {
            using t = t479;
            constexpr int32_t n = c49;
            return ((((lst).length == ((uint32_t) 0)) || (n == ((int32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t479> {
                    return nothing<t479>();
                })())
            :
                (([&]() -> Prelude::maybe<t479> {
                    t479 guid55 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t479 maxVal = guid55;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid56 = ((uint32_t) 1);
                        uint32_t guid57 = (lst).length;
                        for (uint32_t i = guid56; i < guid57; i++) {
                            (([&]() -> juniper::unit {
                                return ((((lst).data)[i] > maxVal) ? 
                                    (([&]() -> juniper::unit {
                                        (maxVal = ((lst).data)[i]);
                                        return juniper::unit();
                                    })())
                                :
                                    (([&]() -> juniper::unit {
                                        return juniper::unit();
                                    })()));
                            })());
                        }
                        return {};
                    })());
                    return just<t479>(maxVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t496, int c53>
    Prelude::maybe<t496> min_(juniper::records::recordt_0<juniper::array<t496, c53>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t496> {
            using t = t496;
            constexpr int32_t n = c53;
            return ((((lst).length == ((uint32_t) 0)) || (n == ((int32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t496> {
                    return nothing<t496>();
                })())
            :
                (([&]() -> Prelude::maybe<t496> {
                    t496 guid58 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t496 minVal = guid58;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid59 = ((uint32_t) 1);
                        uint32_t guid60 = (lst).length;
                        for (uint32_t i = guid59; i < guid60; i++) {
                            (([&]() -> juniper::unit {
                                return ((((lst).data)[i] < minVal) ? 
                                    (([&]() -> juniper::unit {
                                        (minVal = ((lst).data)[i]);
                                        return juniper::unit();
                                    })())
                                :
                                    (([&]() -> juniper::unit {
                                        return juniper::unit();
                                    })()));
                            })());
                        }
                        return {};
                    })());
                    return just<t496>(minVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t504, int c57>
    bool member(t504 elem, juniper::records::recordt_0<juniper::array<t504, c57>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t504;
            constexpr int32_t n = c57;
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
                            return ((!(found) && (((lst).data)[i] == elem)) ? 
                                (([&]() -> juniper::unit {
                                    (found = true);
                                    return juniper::unit();
                                })())
                            :
                                (([&]() -> juniper::unit {
                                    return juniper::unit();
                                })()));
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
    template<typename t509>
    t509 min_(t509 x, t509 y) {
        return (([&]() -> t509 {
            using a = t509;
            return ((x > y) ? 
                (([&]() -> t509 {
                    return y;
                })())
            :
                (([&]() -> t509 {
                    return x;
                })()));
        })());
    }
}

namespace List {
    template<typename t521, typename t523, int c59>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t521, c59>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t523, c59>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t> {
            using a = t521;
            using b = t523;
            constexpr int32_t n = c59;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t> {
                uint32_t guid64 = Math::min_<uint32_t>((lstA).length, (lstB).length);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t outLen = guid64;
                
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t> guid65 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t> guid66;
                    guid66.data = (juniper::array<juniper::tuple2<t521,t523>, c59>());
                    guid66.length = outLen;
                    return guid66;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t521,t523>, c59>, uint32_t> ret = guid65;
                
                (([&]() -> juniper::unit {
                    uint32_t guid67 = ((uint32_t) 0);
                    uint32_t guid68 = outLen;
                    for (uint32_t i = guid67; i < guid68; i++) {
                        (([&]() -> juniper::tuple2<t521,t523> {
                            return (((ret).data)[i] = (juniper::tuple2<t521,t523>{((lstA).data)[i], ((lstB).data)[i]}));
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
    template<typename t534, typename t535, int c63>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t>,juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t534,t535>, c63>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t>,juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t>> {
            using a = t534;
            using b = t535;
            constexpr int32_t n = c63;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t>,juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t> guid69 = (([&]() -> juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t> guid70;
                    guid70.data = (juniper::array<t534, c63>());
                    guid70.length = (lst).length;
                    return guid70;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t> retA = guid69;
                
                juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t> guid71 = (([&]() -> juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t> guid72;
                    guid72.data = (juniper::array<t535, c63>());
                    guid72.length = (lst).length;
                    return guid72;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t> retB = guid71;
                
                (([&]() -> juniper::unit {
                    uint32_t guid73 = ((uint32_t) 0);
                    uint32_t guid74 = (lst).length;
                    for (uint32_t i = guid73; i < guid74; i++) {
                        (([&]() -> t535 {
                            juniper::tuple2<t534,t535> guid75 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t535 b = (guid75).e2;
                            t534 a = (guid75).e1;
                            
                            (((retA).data)[i] = a);
                            return (((retB).data)[i] = b);
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t534, c63>, uint32_t>,juniper::records::recordt_0<juniper::array<t535, c63>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t543, int c67>
    t543 sum(juniper::records::recordt_0<juniper::array<t543, c67>, uint32_t> lst) {
        return (([&]() -> t543 {
            using a = t543;
            constexpr int32_t n = c67;
            return List::foldl<void, t543, t543, c67>(juniper::function<void, t543(t543, t543)>(add<t543>), ((t543) 0), lst);
        })());
    }
}

namespace List {
    template<typename t560, int c69>
    t560 average(juniper::records::recordt_0<juniper::array<t560, c69>, uint32_t> lst) {
        return (([&]() -> t560 {
            using a = t560;
            constexpr int32_t n = c69;
            return (sum<t560, c69>(lst) / cast<t560, uint32_t>((lst).length));
        })());
    }
}

namespace Signal {
    template<typename t567, typename t569, typename t584>
    Prelude::sig<t584> map(juniper::function<t569, t584(t567)> f, Prelude::sig<t567> s) {
        return (([&]() -> Prelude::sig<t584> {
            using a = t567;
            using b = t584;
            using closure = t569;
            return (([&]() -> Prelude::sig<t584> {
                Prelude::sig<t567> guid76 = s;
                return ((((guid76).id() == ((uint8_t) 0)) && ((((guid76).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t584> {
                        t567 val = ((guid76).signal()).just();
                        return signal<t584>(just<t584>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t584> {
                            return signal<t584>(nothing<t584>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t584>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t591, typename t592>
    juniper::unit sink(juniper::function<t592, juniper::unit(t591)> f, Prelude::sig<t591> s) {
        return (([&]() -> juniper::unit {
            using a = t591;
            using closure = t592;
            return (([&]() -> juniper::unit {
                Prelude::sig<t591> guid77 = s;
                return ((((guid77).id() == ((uint8_t) 0)) && ((((guid77).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> juniper::unit {
                        t591 val = ((guid77).signal()).just();
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
    template<typename t600, typename t614>
    Prelude::sig<t614> filter(juniper::function<t600, bool(t614)> f, Prelude::sig<t614> s) {
        return (([&]() -> Prelude::sig<t614> {
            using a = t614;
            using closure = t600;
            return (([&]() -> Prelude::sig<t614> {
                Prelude::sig<t614> guid78 = s;
                return ((((guid78).id() == ((uint8_t) 0)) && ((((guid78).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t614> {
                        t614 val = ((guid78).signal()).just();
                        return (f(val) ? 
                            (([&]() -> Prelude::sig<t614> {
                                return signal<t614>(nothing<t614>());
                            })())
                        :
                            (([&]() -> Prelude::sig<t614> {
                                return s;
                            })()));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t614> {
                            return signal<t614>(nothing<t614>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t614>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t621>
    Prelude::sig<t621> merge(Prelude::sig<t621> sigA, Prelude::sig<t621> sigB) {
        return (([&]() -> Prelude::sig<t621> {
            using a = t621;
            return (([&]() -> Prelude::sig<t621> {
                Prelude::sig<t621> guid79 = sigA;
                return ((((guid79).id() == ((uint8_t) 0)) && ((((guid79).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t621> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t621> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t621>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t625, int c71>
    Prelude::sig<t625> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t625>, c71>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t625> {
            using a = t625;
            constexpr int32_t n = c71;
            return (([&]() -> Prelude::sig<t625> {
                Prelude::maybe<t625> guid80 = List::foldl<void, Prelude::maybe<t625>, Prelude::sig<t625>, c71>(juniper::function<void, Prelude::maybe<t625>(Prelude::sig<t625>,Prelude::maybe<t625>)>([](Prelude::sig<t625> sig, Prelude::maybe<t625> accum) -> Prelude::maybe<t625> { 
                    return (([&]() -> Prelude::maybe<t625> {
                        Prelude::maybe<t625> guid81 = accum;
                        return ((((guid81).id() == ((uint8_t) 1)) && true) ? 
                            (([&]() -> Prelude::maybe<t625> {
                                return (([&]() -> Prelude::maybe<t625> {
                                    Prelude::sig<t625> guid82 = sig;
                                    if (!((((guid82).id() == ((uint8_t) 0)) && true))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t625> heldValue = (guid82).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t625> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t625>>()));
                    })());
                 }), nothing<t625>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t625> ret = guid80;
                
                return signal<t625>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t648, typename t672>
    Prelude::sig<Prelude::either<t672, t648>> join(Prelude::sig<t672> sigA, Prelude::sig<t648> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t672, t648>> {
            using a = t672;
            using b = t648;
            return (([&]() -> Prelude::sig<Prelude::either<t672, t648>> {
                juniper::tuple2<Prelude::sig<t672>,Prelude::sig<t648>> guid83 = (juniper::tuple2<Prelude::sig<t672>,Prelude::sig<t648>>{sigA, sigB});
                return (((((guid83).e1).id() == ((uint8_t) 0)) && (((((guid83).e1).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t672, t648>> {
                        t672 value = (((guid83).e1).signal()).just();
                        return signal<Prelude::either<t672, t648>>(just<Prelude::either<t672, t648>>(left<t672, t648>(value)));
                    })())
                :
                    (((((guid83).e2).id() == ((uint8_t) 0)) && (((((guid83).e2).signal()).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t672, t648>> {
                            t648 value = (((guid83).e2).signal()).just();
                            return signal<Prelude::either<t672, t648>>(just<Prelude::either<t672, t648>>(right<t672, t648>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t672, t648>> {
                                return signal<Prelude::either<t672, t648>>(nothing<Prelude::either<t672, t648>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t672, t648>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t695>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t695> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t695;
            return map<t695, void, juniper::unit>(juniper::function<void, juniper::unit(t695)>([](t695 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t701, typename t702, typename t721>
    Prelude::sig<t721> foldP(juniper::function<t702, t721(t701, t721)> f, juniper::shared_ptr<t721> state0, Prelude::sig<t701> incoming) {
        return (([&]() -> Prelude::sig<t721> {
            using a = t701;
            using closure = t702;
            using state = t721;
            return (([&]() -> Prelude::sig<t721> {
                Prelude::sig<t701> guid84 = incoming;
                return ((((guid84).id() == ((uint8_t) 0)) && ((((guid84).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t721> {
                        t701 val = ((guid84).signal()).just();
                        return (([&]() -> Prelude::sig<t721> {
                            t721 guid85 = f(val, (*((state0).get())));
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t721 state1 = guid85;
                            
                            (*((state0).get()) = state1);
                            return signal<t721>(just<t721>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t721> {
                            return signal<t721>(nothing<t721>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t721>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t731>
    Prelude::sig<t731> dropRepeats(juniper::shared_ptr<Prelude::maybe<t731>> maybePrevValue, Prelude::sig<t731> incoming) {
        return (([&]() -> Prelude::sig<t731> {
            using a = t731;
            return filter<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t731>>>, t731>(juniper::function<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t731>>>, bool(t731)>(juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t731>>>(maybePrevValue), [](juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t731>>>& junclosure, t731 value) -> bool { 
                juniper::shared_ptr<Prelude::maybe<t731>>& maybePrevValue = junclosure.maybePrevValue;
                return (([&]() -> bool {
                    bool guid86 = (([&]() -> bool {
                        Prelude::maybe<t731> guid87 = (*((maybePrevValue).get()));
                        return ((((guid87).id() == ((uint8_t) 1)) && true) ? 
                            (([&]() -> bool {
                                return false;
                            })())
                        :
                            ((((guid87).id() == ((uint8_t) 0)) && true) ? 
                                (([&]() -> bool {
                                    t731 prevValue = (guid87).just();
                                    return (value == prevValue);
                                })())
                            :
                                juniper::quit<bool>()));
                    })());
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    bool filtered = guid86;
                    
                    (!(filtered) ? 
                        (([&]() -> juniper::unit {
                            (*((maybePrevValue).get()) = just<t731>(value));
                            return juniper::unit();
                        })())
                    :
                        (([&]() -> juniper::unit {
                            return juniper::unit();
                        })()));
                    return filtered;
                })());
             }), incoming);
        })());
    }
}

namespace Signal {
    template<typename t749>
    Prelude::sig<t749> latch(juniper::shared_ptr<t749> prevValue, Prelude::sig<t749> incoming) {
        return (([&]() -> Prelude::sig<t749> {
            using a = t749;
            return (([&]() -> Prelude::sig<t749> {
                Prelude::sig<t749> guid88 = incoming;
                return ((((guid88).id() == ((uint8_t) 0)) && ((((guid88).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> Prelude::sig<t749> {
                        t749 val = ((guid88).signal()).just();
                        return (([&]() -> Prelude::sig<t749> {
                            (*((prevValue).get()) = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t749> {
                            return signal<t749>(just<t749>((*((prevValue).get()))));
                        })())
                    :
                        juniper::quit<Prelude::sig<t749>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t761, typename t762, typename t765, typename t774>
    Prelude::sig<t761> map2(juniper::function<t762, t761(t765, t774)> f, juniper::shared_ptr<juniper::tuple2<t765,t774>> state, Prelude::sig<t765> incomingA, Prelude::sig<t774> incomingB) {
        return (([&]() -> Prelude::sig<t761> {
            using a = t765;
            using b = t774;
            using c = t761;
            using closure = t762;
            return (([&]() -> Prelude::sig<t761> {
                t765 guid89 = (([&]() -> t765 {
                    Prelude::sig<t765> guid90 = incomingA;
                    return ((((guid90).id() == ((uint8_t) 0)) && ((((guid90).signal()).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> t765 {
                            t765 val1 = ((guid90).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t765 {
                                return fst<t765, t774>((*((state).get())));
                            })())
                        :
                            juniper::quit<t765>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t765 valA = guid89;
                
                t774 guid91 = (([&]() -> t774 {
                    Prelude::sig<t774> guid92 = incomingB;
                    return ((((guid92).id() == ((uint8_t) 0)) && ((((guid92).signal()).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> t774 {
                            t774 val2 = ((guid92).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t774 {
                                return snd<t765, t774>((*((state).get())));
                            })())
                        :
                            juniper::quit<t774>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t774 valB = guid91;
                
                (*((state).get()) = (juniper::tuple2<t765,t774>{valA, valB}));
                return (([&]() -> Prelude::sig<t761> {
                    juniper::tuple2<Prelude::sig<t765>,Prelude::sig<t774>> guid93 = (juniper::tuple2<Prelude::sig<t765>,Prelude::sig<t774>>{incomingA, incomingB});
                    return (((((guid93).e2).id() == ((uint8_t) 0)) && (((((guid93).e2).signal()).id() == ((uint8_t) 1)) && ((((guid93).e1).id() == ((uint8_t) 0)) && (((((guid93).e1).signal()).id() == ((uint8_t) 1)) && true)))) ? 
                        (([&]() -> Prelude::sig<t761> {
                            return signal<t761>(nothing<t761>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t761> {
                                return signal<t761>(just<t761>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t761>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t806, int c73>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t806, c73>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t806, c73>, uint32_t>> pastValues, Prelude::sig<t806> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t806, c73>, uint32_t>> {
            using a = t806;
            constexpr int32_t n = c73;
            return foldP<t806, void, juniper::records::recordt_0<juniper::array<t806, c73>, uint32_t>>(List::pushOffFront<t806, c73>, pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t817>
    Prelude::sig<t817> constant(t817 val) {
        return (([&]() -> Prelude::sig<t817> {
            using a = t817;
            return signal<t817>(just<t817>(val));
        })());
    }
}

namespace Signal {
    template<typename t827>
    Prelude::sig<Prelude::maybe<t827>> meta(Prelude::sig<t827> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t827>> {
            using a = t827;
            return (([&]() -> Prelude::sig<Prelude::maybe<t827>> {
                Prelude::sig<t827> guid94 = sigA;
                if (!((((guid94).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t827> val = (guid94).signal();
                
                return constant<Prelude::maybe<t827>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t844>
    Prelude::sig<t844> unmeta(Prelude::sig<Prelude::maybe<t844>> sigA) {
        return (([&]() -> Prelude::sig<t844> {
            using a = t844;
            return (([&]() -> Prelude::sig<t844> {
                Prelude::sig<Prelude::maybe<t844>> guid95 = sigA;
                return ((((guid95).id() == ((uint8_t) 0)) && ((((guid95).signal()).id() == ((uint8_t) 0)) && (((((guid95).signal()).just()).id() == ((uint8_t) 0)) && true))) ? 
                    (([&]() -> Prelude::sig<t844> {
                        t844 val = (((guid95).signal()).just()).just();
                        return constant<t844>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t844> {
                            return signal<t844>(nothing<t844>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t844>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t857, typename t858>
    Prelude::sig<juniper::tuple2<t857,t858>> zip(juniper::shared_ptr<juniper::tuple2<t857,t858>> state, Prelude::sig<t857> sigA, Prelude::sig<t858> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t857,t858>> {
            using a = t857;
            using b = t858;
            return map2<juniper::tuple2<t857,t858>, void, t857, t858>(juniper::function<void, juniper::tuple2<t857,t858>(t857,t858)>([](t857 valA, t858 valB) -> juniper::tuple2<t857,t858> { 
                return (juniper::tuple2<t857,t858>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t889, typename t896>
    juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>> unzip(Prelude::sig<juniper::tuple2<t889,t896>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>> {
            using a = t889;
            using b = t896;
            return (([&]() -> juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>> {
                Prelude::sig<juniper::tuple2<t889,t896>> guid96 = incoming;
                return ((((guid96).id() == ((uint8_t) 0)) && ((((guid96).signal()).id() == ((uint8_t) 0)) && true)) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>> {
                        t896 y = (((guid96).signal()).just()).e2;
                        t889 x = (((guid96).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>>{signal<t889>(just<t889>(x)), signal<t896>(just<t896>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>> {
                            return (juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>>{signal<t889>(nothing<t889>()), signal<t896>(nothing<t896>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t889>,Prelude::sig<t896>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t903, typename t904>
    Prelude::sig<t903> toggle(t903 val1, t903 val2, juniper::shared_ptr<t903> state, Prelude::sig<t904> incoming) {
        return (([&]() -> Prelude::sig<t903> {
            using a = t903;
            using b = t904;
            return foldP<t904, juniper::closures::closuret_5<t903, t903>, t903>(juniper::function<juniper::closures::closuret_5<t903, t903>, t903(t904,t903)>(juniper::closures::closuret_5<t903, t903>(val1, val2), [](juniper::closures::closuret_5<t903, t903>& junclosure, t904 event, t903 prevVal) -> t903 { 
                t903& val1 = junclosure.val1;
                t903& val2 = junclosure.val2;
                return ((prevVal == val1) ? 
                    (([&]() -> t903 {
                        return val2;
                    })())
                :
                    (([&]() -> t903 {
                        return val1;
                    })()));
             }), state, incoming);
        })());
    }
}

namespace Io {
    Io::pinState toggle(Io::pinState p) {
        return (([&]() -> Io::pinState {
            Io::pinState guid97 = p;
            return ((((guid97).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> Io::pinState {
                    return low();
                })())
            :
                ((((guid97).id() == ((uint8_t) 1)) && true) ? 
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
    template<int c75>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c75>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c75;
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
    template<typename t930>
    t930 baseToInt(Io::base b) {
        return (([&]() -> t930 {
            Io::base guid98 = b;
            return ((((guid98).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> t930 {
                    return ((t930) 2);
                })())
            :
                ((((guid98).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> t930 {
                        return ((t930) 8);
                    })())
                :
                    ((((guid98).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> t930 {
                            return ((t930) 10);
                        })())
                    :
                        ((((guid98).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> t930 {
                                return ((t930) 16);
                            })())
                        :
                            juniper::quit<t930>()))));
        })());
    }
}

namespace Io {
    juniper::unit printIntBase(int32_t n, Io::base b) {
        return (([&]() -> juniper::unit {
            int32_t guid99 = baseToInt<int32_t>(b);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t bint = guid99;
            
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
            Io::pinState guid100 = value;
            return ((((guid100).id() == ((uint8_t) 1)) && true) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                ((((guid100).id() == ((uint8_t) 0)) && true) ? 
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
            (([&]() -> Io::pinState {
                return low();
            })())
        :
            (([&]() -> Io::pinState {
                return high();
            })()));
    }
}

namespace Io {
    juniper::unit digWrite(uint16_t pin, Io::pinState value) {
        return (([&]() -> juniper::unit {
            uint8_t guid101 = pinStateToInt(value);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t intVal = guid101;
            
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
            Io::mode guid102 = m;
            return ((((guid102).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                ((((guid102).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    ((((guid102).id() == ((uint8_t) 2)) && true) ? 
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
            uint8_t guid103 = m;
            return (((guid103 == ((int32_t) 0)) && true) ? 
                (([&]() -> Io::mode {
                    return input();
                })())
            :
                (((guid103 == ((int32_t) 1)) && true) ? 
                    (([&]() -> Io::mode {
                        return output();
                    })())
                :
                    (((guid103 == ((int32_t) 2)) && true) ? 
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
            uint8_t guid104 = pinModeToInt(m);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t m2 = guid104;
            
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
                bool guid105 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState,Io::pinState> guid106 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((((guid106).e2).id() == ((uint8_t) 1)) && ((((guid106).e1).id() == ((uint8_t) 0)) && true)) ? 
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
                bool ret = guid105;
                
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
                bool guid107 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState,Io::pinState> guid108 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((((guid108).e2).id() == ((uint8_t) 0)) && ((((guid108).e1).id() == ((uint8_t) 1)) && true)) ? 
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
    Prelude::sig<Io::pinState> edge(Prelude::sig<Io::pinState> sig, juniper::shared_ptr<Io::pinState> prevState) {
        return Signal::filter<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, Io::pinState>(juniper::function<juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>, bool(Io::pinState)>(juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>(prevState), [](juniper::closures::closuret_7<juniper::shared_ptr<Io::pinState>>& junclosure, Io::pinState currState) -> bool { 
            juniper::shared_ptr<Io::pinState>& prevState = junclosure.prevState;
            return (([&]() -> bool {
                bool guid109 = (([&]() -> bool {
                    juniper::tuple2<Io::pinState,Io::pinState> guid110 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((((guid110).e2).id() == ((uint8_t) 1)) && ((((guid110).e1).id() == ((uint8_t) 0)) && true)) ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        (((((guid110).e2).id() == ((uint8_t) 0)) && ((((guid110).e1).id() == ((uint8_t) 1)) && true)) ? 
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
                bool ret = guid109;
                
                (*((prevState).get()) = currState);
                return ret;
            })());
         }), sig);
    }
}

namespace Maybe {
    template<typename t1061, typename t1063, typename t1072>
    Prelude::maybe<t1072> map(juniper::function<t1063, t1072(t1061)> f, Prelude::maybe<t1061> maybeVal) {
        return (([&]() -> Prelude::maybe<t1072> {
            using a = t1061;
            using b = t1072;
            using closure = t1063;
            return (([&]() -> Prelude::maybe<t1072> {
                Prelude::maybe<t1061> guid111 = maybeVal;
                return ((((guid111).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> Prelude::maybe<t1072> {
                        t1061 val = (guid111).just();
                        return just<t1072>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t1072> {
                            return nothing<t1072>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t1072>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1076>
    t1076 get(Prelude::maybe<t1076> maybeVal) {
        return (([&]() -> t1076 {
            using a = t1076;
            return (([&]() -> t1076 {
                Prelude::maybe<t1076> guid112 = maybeVal;
                return ((((guid112).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> t1076 {
                        t1076 val = (guid112).just();
                        return val;
                    })())
                :
                    juniper::quit<t1076>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1079>
    bool isJust(Prelude::maybe<t1079> maybeVal) {
        return (([&]() -> bool {
            using a = t1079;
            return (([&]() -> bool {
                Prelude::maybe<t1079> guid113 = maybeVal;
                return ((((guid113).id() == ((uint8_t) 0)) && true) ? 
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
    template<typename t1082>
    bool isNothing(Prelude::maybe<t1082> maybeVal) {
        return (([&]() -> bool {
            using a = t1082;
            return !(isJust<t1082>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t1088>
    uint8_t count(Prelude::maybe<t1088> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t1088;
            return (([&]() -> uint8_t {
                Prelude::maybe<t1088> guid114 = maybeVal;
                return ((((guid114).id() == ((uint8_t) 0)) && true) ? 
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
    template<typename t1093, typename t1094, typename t1095>
    t1094 foldl(juniper::function<t1093, t1094(t1095, t1094)> f, t1094 initState, Prelude::maybe<t1095> maybeVal) {
        return (([&]() -> t1094 {
            using closure = t1093;
            using state = t1094;
            using t = t1095;
            return (([&]() -> t1094 {
                Prelude::maybe<t1095> guid115 = maybeVal;
                return ((((guid115).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> t1094 {
                        t1095 val = (guid115).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t1094 {
                            return initState;
                        })())
                    :
                        juniper::quit<t1094>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1102, typename t1103, typename t1104>
    t1103 fodlr(juniper::function<t1102, t1103(t1104, t1103)> f, t1103 initState, Prelude::maybe<t1104> maybeVal) {
        return (([&]() -> t1103 {
            using closure = t1102;
            using state = t1103;
            using t = t1104;
            return foldl<t1102, t1103, t1104>(f, initState, maybeVal);
        })());
    }
}

namespace Maybe {
    template<typename t1114, typename t1115>
    juniper::unit iter(juniper::function<t1115, juniper::unit(t1114)> f, Prelude::maybe<t1114> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t1114;
            using closure = t1115;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t1114> guid116 = maybeVal;
                return ((((guid116).id() == ((uint8_t) 0)) && true) ? 
                    (([&]() -> juniper::unit {
                        t1114 val = (guid116).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t1114> nothing = guid116;
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
            uint32_t guid117 = ((uint32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t ret = guid117;
            
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
            juniper::records::recordt_1<uint32_t> guid118;
            guid118.lastPulse = ((uint32_t) 0);
            return guid118;
        })()))));
    }
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> state) {
        return (([&]() -> Prelude::sig<uint32_t> {
            uint32_t guid119 = now();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t t = guid119;
            
            uint32_t guid120 = ((interval == ((uint32_t) 0)) ? 
                (([&]() -> uint32_t {
                    return t;
                })())
            :
                (([&]() -> uint32_t {
                    return ((t / interval) * interval);
                })()));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t lastWindow = guid120;
            
            return ((((state).get())->lastPulse >= lastWindow) ? 
                (([&]() -> Prelude::sig<uint32_t> {
                    return signal<uint32_t>(nothing<uint32_t>());
                })())
            :
                (([&]() -> Prelude::sig<uint32_t> {
                    (*((state).get()) = (([&]() -> juniper::records::recordt_1<uint32_t>{
                        juniper::records::recordt_1<uint32_t> guid121;
                        guid121.lastPulse = t;
                        return guid121;
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
            double guid122 = 0.000000;
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
            double guid123 = 0.000000;
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
            double guid124 = 0.000000;
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
            double guid125 = 0.000000;
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
            double guid126 = 0.000000;
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
            double guid127 = 0.000000;
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
            double guid128 = 0.000000;
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
            double guid129 = 0.000000;
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
        return (sin_(x) / cos_(x));
    }
}

namespace Math {
    double tanh_(double x) {
        return (([&]() -> double {
            double guid130 = 0.000000;
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
            double guid131 = 0.000000;
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
    juniper::tuple2<double,int32_t> frexp_(double x) {
        return (([&]() -> juniper::tuple2<double,int32_t> {
            double guid132 = 0.000000;
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
            double guid134 = 0.000000;
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
            double guid135 = 0.000000;
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
            double guid136 = 0.000000;
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
    juniper::tuple2<double,double> modf_(double x) {
        return (([&]() -> juniper::tuple2<double,double> {
            double guid137 = 0.000000;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid137;
            
            double guid138 = 0.000000;
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
            double guid139 = 0.000000;
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
            double guid140 = 0.000000;
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
            double guid141 = 0.000000;
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
            double guid142 = 0.000000;
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
            double guid143 = 0.000000;
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
            double guid144 = 0.000000;
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
        return floor_((x + 0.500000));
    }
}

namespace Math {
    template<typename t1181>
    t1181 max_(t1181 x, t1181 y) {
        return (([&]() -> t1181 {
            using a = t1181;
            return ((x > y) ? 
                (([&]() -> t1181 {
                    return x;
                })())
            :
                (([&]() -> t1181 {
                    return y;
                })()));
        })());
    }
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2) {
        return (b1 + (((x - a1) * (b2 - b1)) / (a2 - a1)));
    }
}

namespace Math {
    template<typename t1184>
    t1184 clamp(t1184 x, t1184 min, t1184 max) {
        return (([&]() -> t1184 {
            using a = t1184;
            return ((min > x) ? 
                (([&]() -> t1184 {
                    return min;
                })())
            :
                ((x > max) ? 
                    (([&]() -> t1184 {
                        return max;
                    })())
                :
                    (([&]() -> t1184 {
                        return x;
                    })())));
        })());
    }
}

namespace Math {
    template<typename t1189>
    int8_t sign(t1189 n) {
        return (([&]() -> int8_t {
            using a = t1189;
            return ((n == ((t1189) 0)) ? 
                (([&]() -> int8_t {
                    return ((int8_t) 0);
                })())
            :
                ((n > ((t1189) 0)) ? 
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
        return (juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>(new juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>((([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid145;
            guid145.actualState = Io::low();
            guid145.lastState = Io::low();
            guid145.lastDebounceTime = ((uint32_t) 0);
            return guid145;
        })()))));
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
                
                return ((currentState != lastState) ? 
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
                    (((currentState != actualState) && ((Time::now() - ((buttonState).get())->lastDebounceTime) > delay)) ? 
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
    template<typename t1234, int c76>
    juniper::records::recordt_3<juniper::array<t1234, c76>> make(juniper::array<t1234, c76> d) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1234, c76>> {
            using a = t1234;
            constexpr int32_t n = c76;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1234, c76>>{
                juniper::records::recordt_3<juniper::array<t1234, c76>> guid150;
                guid150.data = d;
                return guid150;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1236, int c77>
    t1236 get(uint32_t i, juniper::records::recordt_3<juniper::array<t1236, c77>> v) {
        return (([&]() -> t1236 {
            using a = t1236;
            constexpr int32_t n = c77;
            return ((v).data)[i];
        })());
    }
}

namespace Vector {
    template<typename t1245, int c79>
    juniper::records::recordt_3<juniper::array<t1245, c79>> add(juniper::records::recordt_3<juniper::array<t1245, c79>> v1, juniper::records::recordt_3<juniper::array<t1245, c79>> v2) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1245, c79>> {
            using a = t1245;
            constexpr int32_t n = c79;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1245, c79>> {
                juniper::records::recordt_3<juniper::array<t1245, c79>> guid151 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1245, c79>> result = guid151;
                
                (([&]() -> juniper::unit {
                    int32_t guid152 = ((int32_t) 0);
                    int32_t guid153 = n;
                    for (int32_t i = guid152; i < guid153; i++) {
                        (([&]() -> t1245 {
                            return (((result).data)[i] = (((result).data)[i] + ((v2).data)[i]));
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
    template<typename a, int n>
    juniper::records::recordt_3<juniper::array<a, n>> zero() {
        return (([&]() -> juniper::records::recordt_3<juniper::array<a, n>>{
            juniper::records::recordt_3<juniper::array<a, n>> guid154;
            guid154.data = (juniper::array<a, n>().fill(((a) 0)));
            return guid154;
        })());
    }
}

namespace Vector {
    template<typename t1256, int c83>
    juniper::records::recordt_3<juniper::array<t1256, c83>> subtract(juniper::records::recordt_3<juniper::array<t1256, c83>> v1, juniper::records::recordt_3<juniper::array<t1256, c83>> v2) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1256, c83>> {
            using a = t1256;
            constexpr int32_t n = c83;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1256, c83>> {
                juniper::records::recordt_3<juniper::array<t1256, c83>> guid155 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1256, c83>> result = guid155;
                
                (([&]() -> juniper::unit {
                    int32_t guid156 = ((int32_t) 0);
                    int32_t guid157 = n;
                    for (int32_t i = guid156; i < guid157; i++) {
                        (([&]() -> t1256 {
                            return (((result).data)[i] = (((result).data)[i] - ((v2).data)[i]));
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
    template<typename t1260, int c87>
    juniper::records::recordt_3<juniper::array<t1260, c87>> scale(t1260 scalar, juniper::records::recordt_3<juniper::array<t1260, c87>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1260, c87>> {
            using a = t1260;
            constexpr int32_t n = c87;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1260, c87>> {
                juniper::records::recordt_3<juniper::array<t1260, c87>> guid158 = v;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1260, c87>> result = guid158;
                
                (([&]() -> juniper::unit {
                    int32_t guid159 = ((int32_t) 0);
                    int32_t guid160 = n;
                    for (int32_t i = guid159; i < guid160; i++) {
                        (([&]() -> t1260 {
                            return (((result).data)[i] = (((result).data)[i] * scalar));
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
    template<typename t1273, int c90>
    t1273 dot(juniper::records::recordt_3<juniper::array<t1273, c90>> v1, juniper::records::recordt_3<juniper::array<t1273, c90>> v2) {
        return (([&]() -> t1273 {
            using a = t1273;
            constexpr int32_t n = c90;
            return (([&]() -> t1273 {
                t1273 guid161 = ((t1273) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1273 sum = guid161;
                
                (([&]() -> juniper::unit {
                    int32_t guid162 = ((int32_t) 0);
                    int32_t guid163 = n;
                    for (int32_t i = guid162; i < guid163; i++) {
                        (([&]() -> t1273 {
                            return (sum = (sum + (((v1).data)[i] * ((v2).data)[i])));
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
    template<typename t1281, int c93>
    t1281 magnitude2(juniper::records::recordt_3<juniper::array<t1281, c93>> v) {
        return (([&]() -> t1281 {
            using a = t1281;
            constexpr int32_t n = c93;
            return (([&]() -> t1281 {
                t1281 guid164 = ((t1281) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1281 sum = guid164;
                
                (([&]() -> juniper::unit {
                    int32_t guid165 = ((int32_t) 0);
                    int32_t guid166 = n;
                    for (int32_t i = guid165; i < guid166; i++) {
                        (([&]() -> t1281 {
                            return (sum = (sum + (((v).data)[i] * ((v).data)[i])));
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
    template<typename t1283, int c96>
    double magnitude(juniper::records::recordt_3<juniper::array<t1283, c96>> v) {
        return (([&]() -> double {
            using a = t1283;
            constexpr int32_t n = c96;
            return sqrt_(toDouble<t1283>(magnitude2<t1283, c96>(v)));
        })());
    }
}

namespace Vector {
    template<typename t1301, int c98>
    juniper::records::recordt_3<juniper::array<t1301, c98>> multiply(juniper::records::recordt_3<juniper::array<t1301, c98>> u, juniper::records::recordt_3<juniper::array<t1301, c98>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1301, c98>> {
            using a = t1301;
            constexpr int32_t n = c98;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1301, c98>> {
                juniper::records::recordt_3<juniper::array<t1301, c98>> guid167 = u;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1301, c98>> result = guid167;
                
                (([&]() -> juniper::unit {
                    int32_t guid168 = ((int32_t) 0);
                    int32_t guid169 = n;
                    for (int32_t i = guid168; i < guid169; i++) {
                        (([&]() -> t1301 {
                            return (((result).data)[i] = (((result).data)[i] * ((v).data)[i]));
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
    template<typename t1312, int c102>
    juniper::records::recordt_3<juniper::array<t1312, c102>> normalize(juniper::records::recordt_3<juniper::array<t1312, c102>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1312, c102>> {
            using a = t1312;
            constexpr int32_t n = c102;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1312, c102>> {
                double guid170 = magnitude<t1312, c102>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid170;
                
                return ((mag > ((t1312) 0)) ? 
                    (([&]() -> juniper::records::recordt_3<juniper::array<t1312, c102>> {
                        juniper::records::recordt_3<juniper::array<t1312, c102>> guid171 = v;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_3<juniper::array<t1312, c102>> result = guid171;
                        
                        (([&]() -> juniper::unit {
                            int32_t guid172 = ((int32_t) 0);
                            int32_t guid173 = n;
                            for (int32_t i = guid172; i < guid173; i++) {
                                (([&]() -> juniper::unit {
                                    (((result).data)[i] = fromDouble<t1312>((toDouble<t1312>(((result).data)[i]) / mag)));
                                    return juniper::unit();
                                })());
                            }
                            return {};
                        })());
                        return result;
                    })())
                :
                    (([&]() -> juniper::records::recordt_3<juniper::array<t1312, c102>> {
                        return v;
                    })()));
            })());
        })());
    }
}

namespace Vector {
    template<typename t1325, int c106>
    double angle(juniper::records::recordt_3<juniper::array<t1325, c106>> v1, juniper::records::recordt_3<juniper::array<t1325, c106>> v2) {
        return (([&]() -> double {
            using a = t1325;
            constexpr int32_t n = c106;
            return acos_((toDouble<t1325>(dot<t1325, c106>(v1, v2)) / sqrt_(toDouble<t1325>((magnitude2<t1325, c106>(v1) * magnitude2<t1325, c106>(v2))))));
        })());
    }
}

namespace Vector {
    template<typename t1354>
    juniper::records::recordt_3<juniper::array<t1354, 3>> cross(juniper::records::recordt_3<juniper::array<t1354, 3>> u, juniper::records::recordt_3<juniper::array<t1354, 3>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1354, 3>> {
            using a = t1354;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1354, 3>>{
                juniper::records::recordt_3<juniper::array<t1354, 3>> guid174;
                guid174.data = (juniper::array<t1354, 3> { {((((u).data)[((uint32_t) 1)] * ((v).data)[((uint32_t) 2)]) - (((u).data)[((uint32_t) 2)] * ((v).data)[((uint32_t) 1)])), ((((u).data)[((uint32_t) 2)] * ((v).data)[((uint32_t) 0)]) - (((u).data)[((uint32_t) 0)] * ((v).data)[((uint32_t) 2)])), ((((u).data)[((uint32_t) 0)] * ((v).data)[((uint32_t) 1)]) - (((u).data)[((uint32_t) 1)] * ((v).data)[((uint32_t) 0)]))} });
                return guid174;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1381, int c122>
    juniper::records::recordt_3<juniper::array<t1381, c122>> project(juniper::records::recordt_3<juniper::array<t1381, c122>> a, juniper::records::recordt_3<juniper::array<t1381, c122>> b) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1381, c122>> {
            using z = t1381;
            constexpr int32_t n = c122;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1381, c122>> {
                juniper::records::recordt_3<juniper::array<t1381, c122>> guid175 = normalize<t1381, c122>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1381, c122>> bn = guid175;
                
                return scale<t1381, c122>(dot<t1381, c122>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t1397, int c126>
    juniper::records::recordt_3<juniper::array<t1397, c126>> projectPlane(juniper::records::recordt_3<juniper::array<t1397, c126>> a, juniper::records::recordt_3<juniper::array<t1397, c126>> m) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1397, c126>> {
            using z = t1397;
            constexpr int32_t n = c126;
            return subtract<t1397, c126>(a, project<t1397, c126>(a, m));
        })());
    }
}

namespace CharList {
    template<int c129>
    juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c129>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c129>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((c >= ((uint8_t) 97)) && (c <= ((uint8_t) 122))) ? 
                (([&]() -> uint8_t {
                    return (c - ((uint8_t) 32));
                })())
            :
                (([&]() -> uint8_t {
                    return c;
                })()));
         }), str);
    }
}

namespace CharList {
    template<int c130>
    juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c130>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c130>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((c >= ((uint8_t) 65)) && (c <= ((uint8_t) 90))) ? 
                (([&]() -> uint8_t {
                    return (c + ((uint8_t) 32));
                })())
            :
                (([&]() -> uint8_t {
                    return c;
                })()));
         }), str);
    }
}

namespace CharList {
    template<int n>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t> i32ToCharList(int32_t m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t> {
            juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t> guid176 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t>{
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t> guid177;
                guid177.data = (juniper::array<uint8_t, (1)+(n)>().fill(((uint8_t) 0)));
                guid177.length = ((uint32_t) 0);
                return guid177;
            })());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t> ret = guid176;
            
            (([&]() -> juniper::unit {
                
    int charsPrinted = 1 + snprintf((char *) &ret.data[0], n + 1, "%d", m);
    ret.length = charsPrinted >= (n + 1) ? (n + 1) : charsPrinted;
    
                return {};
            })());
            return ret;
        })());
    }
}

namespace CharList {
    template<int c131>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c131>, uint32_t> s) {
        return (([&]() -> uint32_t {
            constexpr int32_t n = c131;
            return ((s).length - ((uint32_t) 1));
        })());
    }
}

namespace CharList {
    template<int c132, int c133, int retCap>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c132)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c133)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t> {
            constexpr int32_t aCap = c132;
            constexpr int32_t bCap = c133;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t> {
                uint32_t guid178 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid178;
                
                uint32_t guid179 = length<(1)+(c132)>(sA);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenA = guid179;
                
                uint32_t guid180 = length<(1)+(c133)>(sB);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenB = guid180;
                
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t> guid181 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t> guid182;
                    guid182.data = (juniper::array<uint8_t, (1)+(retCap)>().fill(((uint8_t) 0)));
                    guid182.length = ((lenA + lenB) + ((uint32_t) 1));
                    return guid182;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(retCap)>, uint32_t> out = guid181;
                
                (([&]() -> juniper::unit {
                    uint32_t guid183 = ((uint32_t) 0);
                    uint32_t guid184 = lenA;
                    for (uint32_t i = guid183; i < guid184; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sA).data)[i]);
                            return (j = (j + ((uint32_t) 1)));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid185 = ((uint32_t) 0);
                    uint32_t guid186 = lenB;
                    for (uint32_t i = guid185; i < guid186; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sB).data)[i]);
                            return (j = (j + ((uint32_t) 1)));
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
    template<int c140, int c141>
    juniper::records::recordt_0<juniper::array<uint8_t, ((1)+(c140))+(c141)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c140)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c141)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((1)+(c140))+(c141)>, uint32_t> {
            constexpr int32_t aCap = c140;
            constexpr int32_t bCap = c141;
            return concat<c140, c141, (c140)+(c141)>(sA, sB);
        })());
    }
}

namespace Random {
    int32_t random_(int32_t low, int32_t high) {
        return (([&]() -> int32_t {
            int32_t guid187 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t ret = guid187;
            
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
    template<typename t1463, int c145>
    t1463 choice(juniper::records::recordt_0<juniper::array<t1463, c145>, uint32_t> lst) {
        return (([&]() -> t1463 {
            using t = t1463;
            constexpr int32_t n = c145;
            return (([&]() -> t1463 {
                int32_t guid188 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid188;
                
                return List::nth<t1463, c145>(i32ToU32(i), lst);
            })());
        })());
    }
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_6<float, float, float> color) {
        return (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> {
            juniper::records::recordt_6<float, float, float> guid189 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float v = (guid189).v;
            float s = (guid189).s;
            float h = (guid189).h;
            
            float guid190 = (v * s);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float c = guid190;
            
            float guid191 = (c * toFloat<double>((1.000000 - Math::fabs_((Math::fmod_((toDouble<float>(h) / ((double) 60)), 2.000000) - 1.000000)))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float x = guid191;
            
            float guid192 = (v - c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float m = guid192;
            
            juniper::tuple3<float,float,float> guid193 = (((0.000000 <= h) && (h < 60.000000)) ? 
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
            float bPrime = (guid193).e3;
            float gPrime = (guid193).e2;
            float rPrime = (guid193).e1;
            
            float guid194 = ((rPrime + m) * 255.000000);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r = guid194;
            
            float guid195 = ((gPrime + m) * 255.000000);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g = guid195;
            
            float guid196 = ((bPrime + m) * 255.000000);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b = guid196;
            
            return (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
                juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid197;
                guid197.r = toUInt8<float>(r);
                guid197.g = toUInt8<float>(g);
                guid197.b = toUInt8<float>(b);
                return guid197;
            })());
        })());
    }
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> color) {
        return (([&]() -> uint16_t {
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid198 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = (guid198).b;
            uint8_t g = (guid198).g;
            uint8_t r = (guid198).r;
            
            return ((((u8ToU16(r) & ((uint16_t) 248)) << ((uint32_t) 8)) | ((u8ToU16(g) & ((uint16_t) 252)) << ((uint32_t) 3))) | (u8ToU16(b) >> ((uint32_t) 3)));
        })());
    }
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> red = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid199;
        guid199.r = ((uint8_t) 255);
        guid199.g = ((uint8_t) 0);
        guid199.b = ((uint8_t) 0);
        return guid199;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> green = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid200;
        guid200.r = ((uint8_t) 0);
        guid200.g = ((uint8_t) 255);
        guid200.b = ((uint8_t) 0);
        return guid200;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> blue = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid201;
        guid201.r = ((uint8_t) 0);
        guid201.g = ((uint8_t) 0);
        guid201.b = ((uint8_t) 255);
        return guid201;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> black = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid202;
        guid202.r = ((uint8_t) 0);
        guid202.g = ((uint8_t) 0);
        guid202.b = ((uint8_t) 0);
        return guid202;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> white = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid203;
        guid203.r = ((uint8_t) 255);
        guid203.g = ((uint8_t) 255);
        guid203.b = ((uint8_t) 255);
        return guid203;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> yellow = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid204;
        guid204.r = ((uint8_t) 255);
        guid204.g = ((uint8_t) 255);
        guid204.b = ((uint8_t) 0);
        return guid204;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> magenta = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid205;
        guid205.r = ((uint8_t) 255);
        guid205.g = ((uint8_t) 0);
        guid205.b = ((uint8_t) 255);
        return guid205;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> cyan = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid206;
        guid206.r = ((uint8_t) 0);
        guid206.g = ((uint8_t) 255);
        guid206.b = ((uint8_t) 255);
        return guid206;
    })());
}

namespace Arcada {
    bool arcadaBegin() {
        return (([&]() -> bool {
            bool guid207 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid207;
            
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
            uint16_t guid208 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid208;
            
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
            uint16_t guid209 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid209;
            
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
            uint16_t guid210 = displayWidth();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid210;
            
            uint16_t guid211 = displayHeight();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid211;
            
            bool guid212 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid212;
            
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
            bool guid213 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid213;
            
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
            Ble::servicet guid214 = s;
            if (!((((guid214).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid214).service();
            
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
            Ble::characterstict guid215 = c;
            if (!((((guid215).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid215).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->begin();
                return {};
            })());
        })());
    }
}

namespace Ble {
    template<int c147>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c147>, uint32_t> bytes) {
        return (([&]() -> uint16_t {
            constexpr int32_t n = c147;
            return (([&]() -> uint16_t {
                juniper::records::recordt_0<juniper::array<uint8_t, c147>, uint32_t> guid216 = bytes;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t length = (guid216).length;
                juniper::array<uint8_t, c147> data = (guid216).data;
                
                Ble::characterstict guid217 = c;
                if (!((((guid217).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid217).characterstic();
                
                uint16_t guid218 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid218;
                
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
            Ble::characterstict guid219 = c;
            if (!((((guid219).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid219).characterstic();
            
            uint16_t guid220 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid220;
            
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
            Ble::characterstict guid221 = c;
            if (!((((guid221).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid221).characterstic();
            
            uint16_t guid222 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid222;
            
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
            Ble::characterstict guid223 = c;
            if (!((((guid223).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid223).characterstic();
            
            uint16_t guid224 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid224;
            
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
            Ble::characterstict guid225 = c;
            if (!((((guid225).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid225).characterstic();
            
            uint16_t guid226 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid226;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write32((int) n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    template<typename t1792>
    uint16_t writeGeneric(Ble::characterstict c, t1792 x) {
        return (([&]() -> uint16_t {
            using t = t1792;
            return (([&]() -> uint16_t {
                Ble::characterstict guid227 = c;
                if (!((((guid227).id() == ((uint8_t) 0)) && true))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid227).characterstic();
                
                uint16_t guid228 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid228;
                
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
    template<typename t>
    t readGeneric(Ble::characterstict c) {
        return (([&]() -> t {
            Ble::characterstict guid229 = c;
            if (!((((guid229).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid229).characterstic();
            
            t ret;
            
            (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->read((void *) &ret, sizeof(t));
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    juniper::unit setCharacteristicPermission(Ble::characterstict c, Ble::secureModet readPermission, Ble::secureModet writePermission) {
        return (([&]() -> juniper::unit {
            Ble::secureModet guid230 = readPermission;
            if (!((((guid230).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t readN = (guid230).secureMode();
            
            Ble::secureModet guid231 = writePermission;
            if (!((((guid231).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t writeN = (guid231).secureMode();
            
            Ble::characterstict guid232 = c;
            if (!((((guid232).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid232).characterstic();
            
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
            Ble::propertiest guid233 = prop;
            if (!((((guid233).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t propN = (guid233).properties();
            
            Ble::characterstict guid234 = c;
            if (!((((guid234).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid234).characterstic();
            
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
            Ble::characterstict guid235 = c;
            if (!((((guid235).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid235).characterstic();
            
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
            Ble::advertisingFlagt guid236 = flag;
            if (!((((guid236).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t flagNum = (guid236).advertisingFlag();
            
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
            Ble::appearancet guid237 = app;
            if (!((((guid237).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t flagNum = (guid237).appearance();
            
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
            Ble::servicet guid238 = serv;
            if (!((((guid238).id() == ((uint8_t) 0)) && true))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid238).service();
            
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
            Ble::setCharacteristicFixedLen(dayDateTimeCharacterstic, sizeof(juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>));
            Ble::setCharacteristicFixedLen(dayOfWeekCharacterstic, sizeof(juniper::records::recordt_9<uint8_t>));
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
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid239;
        guid239.r = ((uint8_t) 252);
        guid239.g = ((uint8_t) 92);
        guid239.b = ((uint8_t) 125);
        return guid239;
    })());
}

namespace CWatch {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> purpleBlue = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid240;
        guid240.r = ((uint8_t) 106);
        guid240.g = ((uint8_t) 130);
        guid240.b = ((uint8_t) 251);
        return guid240;
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
            CWatch::month guid241 = m;
            return ((((guid241).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 31);
                })())
            :
                ((((guid241).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> uint8_t {
                        return (isLeapYear(y) ? 
                            ((uint8_t) 29)
                        :
                            ((uint8_t) 28));
                    })())
                :
                    ((((guid241).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 31);
                        })())
                    :
                        ((((guid241).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> uint8_t {
                                return ((uint8_t) 30);
                            })())
                        :
                            ((((guid241).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> uint8_t {
                                    return ((uint8_t) 31);
                                })())
                            :
                                ((((guid241).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> uint8_t {
                                        return ((uint8_t) 30);
                                    })())
                                :
                                    ((((guid241).id() == ((uint8_t) 6)) && true) ? 
                                        (([&]() -> uint8_t {
                                            return ((uint8_t) 31);
                                        })())
                                    :
                                        ((((guid241).id() == ((uint8_t) 7)) && true) ? 
                                            (([&]() -> uint8_t {
                                                return ((uint8_t) 31);
                                            })())
                                        :
                                            ((((guid241).id() == ((uint8_t) 8)) && true) ? 
                                                (([&]() -> uint8_t {
                                                    return ((uint8_t) 30);
                                                })())
                                            :
                                                ((((guid241).id() == ((uint8_t) 9)) && true) ? 
                                                    (([&]() -> uint8_t {
                                                        return ((uint8_t) 31);
                                                    })())
                                                :
                                                    ((((guid241).id() == ((uint8_t) 10)) && true) ? 
                                                        (([&]() -> uint8_t {
                                                            return ((uint8_t) 30);
                                                        })())
                                                    :
                                                        ((((guid241).id() == ((uint8_t) 11)) && true) ? 
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
            CWatch::month guid242 = m;
            return ((((guid242).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> CWatch::month {
                    return february();
                })())
            :
                ((((guid242).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> CWatch::month {
                        return march();
                    })())
                :
                    ((((guid242).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> CWatch::month {
                            return april();
                        })())
                    :
                        ((((guid242).id() == ((uint8_t) 4)) && true) ? 
                            (([&]() -> CWatch::month {
                                return june();
                            })())
                        :
                            ((((guid242).id() == ((uint8_t) 5)) && true) ? 
                                (([&]() -> CWatch::month {
                                    return july();
                                })())
                            :
                                ((((guid242).id() == ((uint8_t) 7)) && true) ? 
                                    (([&]() -> CWatch::month {
                                        return august();
                                    })())
                                :
                                    ((((guid242).id() == ((uint8_t) 8)) && true) ? 
                                        (([&]() -> CWatch::month {
                                            return october();
                                        })())
                                    :
                                        ((((guid242).id() == ((uint8_t) 9)) && true) ? 
                                            (([&]() -> CWatch::month {
                                                return november();
                                            })())
                                        :
                                            ((((guid242).id() == ((uint8_t) 11)) && true) ? 
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
            CWatch::dayOfWeek guid243 = dw;
            return ((((guid243).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> CWatch::dayOfWeek {
                    return monday();
                })())
            :
                ((((guid243).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> CWatch::dayOfWeek {
                        return tuesday();
                    })())
                :
                    ((((guid243).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> CWatch::dayOfWeek {
                            return wednesday();
                        })())
                    :
                        ((((guid243).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> CWatch::dayOfWeek {
                                return thursday();
                            })())
                        :
                            ((((guid243).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return friday();
                                })())
                            :
                                ((((guid243).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return saturday();
                                    })())
                                :
                                    ((((guid243).id() == ((uint8_t) 6)) && true) ? 
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
            CWatch::dayOfWeek guid244 = dw;
            return ((((guid244).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid245;
                        guid245.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid245.length = ((uint32_t) 4);
                        return guid245;
                    })());
                })())
            :
                ((((guid244).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid246;
                            guid246.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 111), ((uint8_t) 110), ((uint8_t) 0)} });
                            guid246.length = ((uint32_t) 4);
                            return guid246;
                        })());
                    })())
                :
                    ((((guid244).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid247;
                                guid247.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 117), ((uint8_t) 101), ((uint8_t) 0)} });
                                guid247.length = ((uint32_t) 4);
                                return guid247;
                            })());
                        })())
                    :
                        ((((guid244).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid248;
                                    guid248.data = (juniper::array<uint8_t, 4> { {((uint8_t) 87), ((uint8_t) 101), ((uint8_t) 100), ((uint8_t) 0)} });
                                    guid248.length = ((uint32_t) 4);
                                    return guid248;
                                })());
                            })())
                        :
                            ((((guid244).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid249;
                                        guid249.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 104), ((uint8_t) 117), ((uint8_t) 0)} });
                                        guid249.length = ((uint32_t) 4);
                                        return guid249;
                                    })());
                                })())
                            :
                                ((((guid244).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid250;
                                            guid250.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 114), ((uint8_t) 105), ((uint8_t) 0)} });
                                            guid250.length = ((uint32_t) 4);
                                            return guid250;
                                        })());
                                    })())
                                :
                                    ((((guid244).id() == ((uint8_t) 6)) && true) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid251;
                                                guid251.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 97), ((uint8_t) 116), ((uint8_t) 0)} });
                                                guid251.length = ((uint32_t) 4);
                                                return guid251;
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
            CWatch::month guid252 = m;
            return ((((guid252).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid253;
                        guid253.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 97), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid253.length = ((uint32_t) 4);
                        return guid253;
                    })());
                })())
            :
                ((((guid252).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid254;
                            guid254.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 101), ((uint8_t) 98), ((uint8_t) 0)} });
                            guid254.length = ((uint32_t) 4);
                            return guid254;
                        })());
                    })())
                :
                    ((((guid252).id() == ((uint8_t) 2)) && true) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid255;
                                guid255.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 114), ((uint8_t) 0)} });
                                guid255.length = ((uint32_t) 4);
                                return guid255;
                            })());
                        })())
                    :
                        ((((guid252).id() == ((uint8_t) 3)) && true) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid256;
                                    guid256.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 112), ((uint8_t) 114), ((uint8_t) 0)} });
                                    guid256.length = ((uint32_t) 4);
                                    return guid256;
                                })());
                            })())
                        :
                            ((((guid252).id() == ((uint8_t) 4)) && true) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid257;
                                        guid257.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 121), ((uint8_t) 0)} });
                                        guid257.length = ((uint32_t) 4);
                                        return guid257;
                                    })());
                                })())
                            :
                                ((((guid252).id() == ((uint8_t) 5)) && true) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid258;
                                            guid258.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                                            guid258.length = ((uint32_t) 4);
                                            return guid258;
                                        })());
                                    })())
                                :
                                    ((((guid252).id() == ((uint8_t) 6)) && true) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid259;
                                                guid259.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 108), ((uint8_t) 0)} });
                                                guid259.length = ((uint32_t) 4);
                                                return guid259;
                                            })());
                                        })())
                                    :
                                        ((((guid252).id() == ((uint8_t) 7)) && true) ? 
                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid260;
                                                    guid260.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 117), ((uint8_t) 103), ((uint8_t) 0)} });
                                                    guid260.length = ((uint32_t) 4);
                                                    return guid260;
                                                })());
                                            })())
                                        :
                                            ((((guid252).id() == ((uint8_t) 8)) && true) ? 
                                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid261;
                                                        guid261.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 101), ((uint8_t) 112), ((uint8_t) 0)} });
                                                        guid261.length = ((uint32_t) 4);
                                                        return guid261;
                                                    })());
                                                })())
                                            :
                                                ((((guid252).id() == ((uint8_t) 9)) && true) ? 
                                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid262;
                                                            guid262.data = (juniper::array<uint8_t, 4> { {((uint8_t) 79), ((uint8_t) 99), ((uint8_t) 116), ((uint8_t) 0)} });
                                                            guid262.length = ((uint32_t) 4);
                                                            return guid262;
                                                        })());
                                                    })())
                                                :
                                                    ((((guid252).id() == ((uint8_t) 10)) && true) ? 
                                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid263;
                                                                guid263.data = (juniper::array<uint8_t, 4> { {((uint8_t) 78), ((uint8_t) 111), ((uint8_t) 118), ((uint8_t) 0)} });
                                                                guid263.length = ((uint32_t) 4);
                                                                return guid263;
                                                            })());
                                                        })())
                                                    :
                                                        ((((guid252).id() == ((uint8_t) 11)) && true) ? 
                                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid264;
                                                                    guid264.data = (juniper::array<uint8_t, 4> { {((uint8_t) 68), ((uint8_t) 101), ((uint8_t) 99), ((uint8_t) 0)} });
                                                                    guid264.length = ((uint32_t) 4);
                                                                    return guid264;
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
            juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid265 = d;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek = (guid265).dayOfWeek;
            uint8_t seconds = (guid265).seconds;
            uint8_t minutes = (guid265).minutes;
            uint8_t hours = (guid265).hours;
            uint32_t year = (guid265).year;
            uint8_t day = (guid265).day;
            CWatch::month month = (guid265).month;
            
            uint8_t guid266 = (seconds + ((uint8_t) 1));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t seconds1 = guid266;
            
            juniper::tuple2<uint8_t,uint8_t> guid267 = ((seconds1 == ((uint8_t) 60)) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), (minutes + ((uint8_t) 1))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{seconds1, minutes}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t minutes1 = (guid267).e2;
            uint8_t seconds2 = (guid267).e1;
            
            juniper::tuple2<uint8_t,uint8_t> guid268 = ((minutes1 == ((uint8_t) 60)) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), (hours + ((uint8_t) 1))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{minutes1, hours}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t hours1 = (guid268).e2;
            uint8_t minutes2 = (guid268).e1;
            
            juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek> guid269 = ((hours1 == ((uint8_t) 24)) ? 
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{((uint8_t) 0), (day + ((uint8_t) 1)), nextDayOfWeek(dayOfWeek)})
            :
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{hours1, day, dayOfWeek}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek2 = (guid269).e3;
            uint8_t day1 = (guid269).e2;
            uint8_t hours2 = (guid269).e1;
            
            uint8_t guid270 = daysInMonth(year, month);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t daysInCurrentMonth = guid270;
            
            juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month,uint32_t>> guid271 = ((day1 > daysInCurrentMonth) ? 
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month,uint32_t>>{((uint8_t) 1), (([&]() -> juniper::tuple2<CWatch::month,uint32_t> {
                    CWatch::month guid272 = month;
                    return ((((guid272).id() == ((uint8_t) 11)) && true) ? 
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
            uint32_t year2 = ((guid271).e2).e2;
            CWatch::month month2 = ((guid271).e2).e1;
            uint8_t day2 = (guid271).e1;
            
            return (([&]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid273;
                guid273.month = month2;
                guid273.day = day2;
                guid273.year = year2;
                guid273.hours = hours2;
                guid273.minutes = minutes2;
                guid273.seconds = seconds2;
                guid273.dayOfWeek = dayOfWeek2;
                return guid273;
            })());
        })());
    }
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> clockTickerState = Time::state();
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>> clockState = (juniper::shared_ptr<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(new juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>((([]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
        juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid274;
        guid274.month = september();
        guid274.day = ((uint8_t) 9);
        guid274.year = ((uint32_t) 2020);
        guid274.hours = ((uint8_t) 18);
        guid274.minutes = ((uint8_t) 40);
        guid274.seconds = ((uint8_t) 0);
        guid274.dayOfWeek = wednesday();
        return guid274;
    })()))));
}

namespace CWatch {
    juniper::unit processBluetoothUpdates() {
        return (([&]() -> juniper::unit {
            bool guid275 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayDateTime = guid275;
            
            bool guid276 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayOfWeek = guid276;
            
            (([&]() -> juniper::unit {
                
    hasNewDayDateTime = rawHasNewDayDateTime;
    rawHasNewDayDateTime = false;
    hasNewDayOfWeek = rawHasNewDayOfWeek;
    rawHasNewDayOfWeek = false;
    
                return {};
            })());
            (([&]() -> juniper::unit {
                if (hasNewDayDateTime) {
                    (([&]() -> CWatch::month {
                        juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> guid277 = Ble::readGeneric<juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>>(dayDateTimeCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> bleData = guid277;
                        
                        return (((clockState).get())->month = (([&]() -> CWatch::month {
                            uint8_t guid278 = (bleData).month;
                            return (((guid278 == ((int32_t) 0)) && true) ? 
                                (([&]() -> CWatch::month {
                                    return january();
                                })())
                            :
                                (((guid278 == ((int32_t) 1)) && true) ? 
                                    (([&]() -> CWatch::month {
                                        return february();
                                    })())
                                :
                                    (((guid278 == ((int32_t) 2)) && true) ? 
                                        (([&]() -> CWatch::month {
                                            return march();
                                        })())
                                    :
                                        (((guid278 == ((int32_t) 3)) && true) ? 
                                            (([&]() -> CWatch::month {
                                                return april();
                                            })())
                                        :
                                            (((guid278 == ((int32_t) 4)) && true) ? 
                                                (([&]() -> CWatch::month {
                                                    return may();
                                                })())
                                            :
                                                (((guid278 == ((int32_t) 5)) && true) ? 
                                                    (([&]() -> CWatch::month {
                                                        return june();
                                                    })())
                                                :
                                                    (((guid278 == ((int32_t) 6)) && true) ? 
                                                        (([&]() -> CWatch::month {
                                                            return july();
                                                        })())
                                                    :
                                                        (((guid278 == ((int32_t) 7)) && true) ? 
                                                            (([&]() -> CWatch::month {
                                                                return august();
                                                            })())
                                                        :
                                                            (((guid278 == ((int32_t) 8)) && true) ? 
                                                                (([&]() -> CWatch::month {
                                                                    return september();
                                                                })())
                                                            :
                                                                (((guid278 == ((int32_t) 9)) && true) ? 
                                                                    (([&]() -> CWatch::month {
                                                                        return october();
                                                                    })())
                                                                :
                                                                    (((guid278 == ((int32_t) 10)) && true) ? 
                                                                        (([&]() -> CWatch::month {
                                                                            return november();
                                                                        })())
                                                                    :
                                                                        (((guid278 == ((int32_t) 11)) && true) ? 
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
                    })());
                }
                return {};
            })());
            return (([&]() -> juniper::unit {
                if (hasNewDayOfWeek) {
                    (([&]() -> juniper::unit {
                        juniper::records::recordt_9<uint8_t> guid279 = Ble::readGeneric<juniper::records::recordt_9<uint8_t>>(dayOfWeekCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_9<uint8_t> bleData = guid279;
                        
                        (((clockState).get())->dayOfWeek = (([&]() -> CWatch::dayOfWeek {
                            uint8_t guid280 = (bleData).dayOfWeek;
                            return (((guid280 == ((int32_t) 0)) && true) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return sunday();
                                })())
                            :
                                (((guid280 == ((int32_t) 1)) && true) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return monday();
                                    })())
                                :
                                    (((guid280 == ((int32_t) 2)) && true) ? 
                                        (([&]() -> CWatch::dayOfWeek {
                                            return tuesday();
                                        })())
                                    :
                                        (((guid280 == ((int32_t) 3)) && true) ? 
                                            (([&]() -> CWatch::dayOfWeek {
                                                return wednesday();
                                            })())
                                        :
                                            (((guid280 == ((int32_t) 4)) && true) ? 
                                                (([&]() -> CWatch::dayOfWeek {
                                                    return thursday();
                                                })())
                                            :
                                                (((guid280 == ((int32_t) 5)) && true) ? 
                                                    (([&]() -> CWatch::dayOfWeek {
                                                        return friday();
                                                    })())
                                                :
                                                    (((guid280 == ((int32_t) 6)) && true) ? 
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
                        return juniper::unit();
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
    juniper::unit drawVerticalGradient(int16_t x0i, int16_t y0i, int16_t w, int16_t h, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c1, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c2) {
        return (([&]() -> juniper::unit {
            int16_t guid281 = toInt16<uint16_t>(Arcada::displayWidth());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispW = guid281;
            
            int16_t guid282 = toInt16<uint16_t>(Arcada::displayHeight());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispH = guid282;
            
            int16_t guid283 = Math::clamp<int16_t>(x0i, ((int16_t) 0), (dispW - ((int16_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t x0 = guid283;
            
            int16_t guid284 = Math::clamp<int16_t>(y0i, ((int16_t) 0), (dispH - ((int16_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y0 = guid284;
            
            int16_t guid285 = (y0i + h);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t ymax = guid285;
            
            int16_t guid286 = Math::clamp<int16_t>(ymax, ((int16_t) 0), dispH);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y1 = guid286;
            
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid287 = c1;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b1 = (guid287).b;
            uint8_t g1 = (guid287).g;
            uint8_t r1 = (guid287).r;
            
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid288 = c2;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b2 = (guid288).b;
            uint8_t g2 = (guid288).g;
            uint8_t r2 = (guid288).r;
            
            float guid289 = toFloat<uint8_t>(r1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r1f = guid289;
            
            float guid290 = toFloat<uint8_t>(g1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g1f = guid290;
            
            float guid291 = toFloat<uint8_t>(b1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b1f = guid291;
            
            float guid292 = toFloat<uint8_t>(r2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r2f = guid292;
            
            float guid293 = toFloat<uint8_t>(g2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g2f = guid293;
            
            float guid294 = toFloat<uint8_t>(b2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b2f = guid294;
            
            return (([&]() -> juniper::unit {
                int16_t guid295 = y0;
                int16_t guid296 = y1;
                for (int16_t y = guid295; y < guid296; y++) {
                    (([&]() -> juniper::unit {
                        float guid297 = (toFloat<int16_t>((ymax - y)) / toFloat<int16_t>(h));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight1 = guid297;
                        
                        float guid298 = (1.000000 - weight1);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight2 = guid298;
                        
                        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid299 = (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
                            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid300;
                            guid300.r = toUInt8<float>(((r1f * weight1) + (r2f * weight2)));
                            guid300.g = toUInt8<float>(((g1f * weight1) + (g2f * weight2)));
                            guid300.b = toUInt8<float>(((b1f * weight1) + (g2f * weight2)));
                            return guid300;
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> gradColor = guid299;
                        
                        uint16_t guid301 = Color::rgbToRgb565(gradColor);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint16_t gradColor565 = guid301;
                        
                        return drawFastHLine565(x0, y, w, gradColor565);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace Gfx {
    template<int c148>
    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c148)>, uint32_t> cl) {
        return (([&]() -> juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> {
            constexpr int32_t n = c148;
            return (([&]() -> juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> {
                int16_t guid302 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t xret = guid302;
                
                int16_t guid303 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t yret = guid303;
                
                uint16_t guid304 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t wret = guid304;
                
                uint16_t guid305 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t hret = guid305;
                
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
    template<int c149>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c149)>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c149;
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
            Gfx::font guid306 = f;
            return ((((guid306).id() == ((uint8_t) 0)) && true) ? 
                (([&]() -> juniper::unit {
                    return (([&]() -> juniper::unit {
                        arcada.getCanvas()->setFont();
                        return {};
                    })());
                })())
            :
                ((((guid306).id() == ((uint8_t) 1)) && true) ? 
                    (([&]() -> juniper::unit {
                        return (([&]() -> juniper::unit {
                            arcada.getCanvas()->setFont(&FreeSans9pt7b);
                            return {};
                        })());
                    })())
                :
                    ((((guid306).id() == ((uint8_t) 2)) && true) ? 
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
            uint16_t guid307 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid307;
            
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
                    juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid308 = dt;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    CWatch::dayOfWeek dayOfWeek = (guid308).dayOfWeek;
                    uint8_t seconds = (guid308).seconds;
                    uint8_t minutes = (guid308).minutes;
                    uint8_t hours = (guid308).hours;
                    uint32_t year = (guid308).year;
                    uint8_t day = (guid308).day;
                    CWatch::month month = (guid308).month;
                    
                    int32_t guid309 = toInt32<uint8_t>(((hours == ((uint8_t) 0)) ? 
                        ((uint8_t) 12)
                    :
                        ((hours > ((uint8_t) 12)) ? 
                            (hours - ((uint8_t) 12))
                        :
                            hours)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    int32_t displayHours = guid309;
                    
                    Gfx::setTextColor(Color::white);
                    Gfx::setFont(Gfx::freeSans24());
                    Gfx::setTextSize(((uint8_t) 1));
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid310 = CharList::i32ToCharList<2>(displayHours);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> timeHourStr = guid310;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid311 = CharList::safeConcat<2, 1>(timeHourStr, (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid312;
                        guid312.data = (juniper::array<uint8_t, 2> { {((uint8_t) 58), ((uint8_t) 0)} });
                        guid312.length = ((uint32_t) 2);
                        return guid312;
                    })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> timeHourStrColon = guid311;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid313 = ((minutes < ((uint8_t) 10)) ? 
                        CharList::safeConcat<1, 1>((([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid314;
                            guid314.data = (juniper::array<uint8_t, 2> { {((uint8_t) 48), ((uint8_t) 0)} });
                            guid314.length = ((uint32_t) 2);
                            return guid314;
                        })()), CharList::i32ToCharList<1>(toInt32<uint8_t>(minutes)))
                    :
                        CharList::i32ToCharList<2>(toInt32<uint8_t>(minutes)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> minutesStr = guid313;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> guid315 = CharList::safeConcat<3, 2>(timeHourStrColon, minutesStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> timeStr = guid315;
                    
                    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid316 = Gfx::getCharListBounds<5>(((int16_t) 0), ((int16_t) 0), timeStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h = (guid316).e4;
                    uint16_t w = (guid316).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((Arcada::displayWidth() / ((uint16_t) 2)) - (w / ((uint16_t) 2)))), toInt16<uint16_t>(((Arcada::displayHeight() / ((uint16_t) 2)) + (h / ((uint16_t) 2)))));
                    Gfx::printCharList<5>(timeStr);
                    Gfx::setTextSize(((uint8_t) 1));
                    Gfx::setFont(Gfx::freeSans9());
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid317 = ((hours < ((uint8_t) 12)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid318;
                            guid318.data = (juniper::array<uint8_t, 3> { {((uint8_t) 65), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid318.length = ((uint32_t) 3);
                            return guid318;
                        })())
                    :
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid319;
                            guid319.data = (juniper::array<uint8_t, 3> { {((uint8_t) 80), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid319.length = ((uint32_t) 3);
                            return guid319;
                        })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> ampm = guid317;
                    
                    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid320 = Gfx::getCharListBounds<2>(((int16_t) 0), ((int16_t) 0), ampm);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h2 = (guid320).e4;
                    uint16_t w2 = (guid320).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((Arcada::displayWidth() / ((uint16_t) 2)) - (w2 / ((uint16_t) 2)))), toInt16<uint16_t>(((((Arcada::displayHeight() / ((uint16_t) 2)) + (h / ((uint16_t) 2))) + h2) + ((uint16_t) 5))));
                    Gfx::printCharList<2>(ampm);
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> guid321 = CharList::safeConcat<9, 2>(CharList::safeConcat<8, 1>(CharList::safeConcat<5, 3>(CharList::safeConcat<3, 2>(dayOfWeekToCharList(dayOfWeek), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid322;
                        guid322.data = (juniper::array<uint8_t, 3> { {((uint8_t) 44), ((uint8_t) 32), ((uint8_t) 0)} });
                        guid322.length = ((uint32_t) 3);
                        return guid322;
                    })())), monthToCharList(month)), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid323;
                        guid323.data = (juniper::array<uint8_t, 2> { {((uint8_t) 32), ((uint8_t) 0)} });
                        guid323.length = ((uint32_t) 2);
                        return guid323;
                    })())), CharList::i32ToCharList<2>(toInt32<uint8_t>(day)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> dateStr = guid321;
                    
                    juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid324 = Gfx::getCharListBounds<11>(((int16_t) 0), ((int16_t) 0), dateStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h3 = (guid324).e4;
                    uint16_t w3 = (guid324).e3;
                    
                    Gfx::setCursor(cast<int16_t, uint16_t>(((Arcada::displayWidth() / ((uint16_t) 2)) - (w3 / ((uint16_t) 2)))), cast<int16_t, uint16_t>((((Arcada::displayHeight() / ((uint16_t) 2)) - (h / ((uint16_t) 2))) - ((uint16_t) 5))));
                    return Gfx::printCharList<11>(dateStr);
                })());
             }), Signal::latch<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(clockState, Signal::foldP<uint32_t, void, juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(juniper::function<void, juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>(uint32_t,juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>)>([](uint32_t t, juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> dt) -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> { 
                return secondTick(dt);
             }), clockState, Time::every(((uint32_t) 1000), clockTickerState))));
            return Arcada::blitDoubleBuffer();
        })());
    }
}

namespace Gfx {
    juniper::unit fillScreen(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c) {
        return (([&]() -> juniper::unit {
            uint16_t guid325 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid325;
            
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
            uint16_t guid326 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid326;
            
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
    template<int c174>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c174>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c174;
            return (([&]() -> juniper::unit {
                juniper::tuple4<int16_t,int16_t,uint16_t,uint16_t> guid327 = Gfx::getCharListBounds<(-1)+(c174)>(((int16_t) 0), ((int16_t) 0), cl);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t h = (guid327).e4;
                uint16_t w = (guid327).e3;
                
                int16_t guid328 = toInt16<uint16_t>(w);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t ws = guid328;
                
                int16_t guid329 = toInt16<uint16_t>(h);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t hs = guid329;
                
                return (([&]() -> juniper::unit {
                    Gfx::align guid330 = align;
                    return ((((guid330).id() == ((uint8_t) 0)) && true) ? 
                        (([&]() -> juniper::unit {
                            return Gfx::setCursor((x - (ws / ((int16_t) 2))), y);
                        })())
                    :
                        ((((guid330).id() == ((uint8_t) 1)) && true) ? 
                            (([&]() -> juniper::unit {
                                return Gfx::setCursor(x, (y - (hs / ((int16_t) 2))));
                            })())
                        :
                            ((((guid330).id() == ((uint8_t) 2)) && true) ? 
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
