//Compiled on 5/31/2023 5:42:56 PM
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
    template<typename t10, typename t11, typename t12, typename t8, typename t9>
    juniper::function<juniper::closures::closuret_0<juniper::function<t10, t9(t8)>, juniper::function<t11, t8(t12)>>, t9(t12)> compose(juniper::function<t10, t9(t8)> f, juniper::function<t11, t8(t12)> g);
}

namespace Prelude {
    template<typename t22, typename t23, typename t24, typename t25>
    juniper::function<juniper::closures::closuret_1<juniper::function<t23, t22(t24, t25)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>, t22(t25)>(t24)> curry(juniper::function<t23, t22(t24, t25)> f);
}

namespace Prelude {
    template<typename t33, typename t34, typename t35, typename t36, typename t37>
    juniper::function<juniper::closures::closuret_1<juniper::function<t34, juniper::function<t35, t33(t37)>(t36)>>, t33(t36, t37)> uncurry(juniper::function<t34, juniper::function<t35, t33(t37)>(t36)> f);
}

namespace Prelude {
    template<typename t48, typename t49, typename t50, typename t51, typename t52>
    juniper::function<juniper::closures::closuret_1<juniper::function<t48, t49(t50, t51, t52)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>, juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)>(t51)>(t50)> curry3(juniper::function<t48, t49(t50, t51, t52)> f);
}

namespace Prelude {
    template<typename t62, typename t63, typename t64, typename t65, typename t66, typename t67, typename t68>
    juniper::function<juniper::closures::closuret_1<juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)>>, t65(t66, t67, t68)> uncurry3(juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)> f);
}

namespace Prelude {
    template<typename t79>
    bool eq(t79 x, t79 y);
}

namespace Prelude {
    template<typename t85>
    bool neq(t85 x, t85 y);
}

namespace Prelude {
    template<typename t91, typename t92>
    bool gt(t91 x, t92 y);
}

namespace Prelude {
    template<typename t98, typename t99>
    bool geq(t98 x, t99 y);
}

namespace Prelude {
    template<typename t105, typename t106>
    bool lt(t105 x, t106 y);
}

namespace Prelude {
    template<typename t112, typename t113>
    bool leq(t112 x, t113 y);
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
    template<typename t135, typename t136, typename t137>
    t136 apply(juniper::function<t137, t136(t135)> f, t135 x);
}

namespace Prelude {
    template<typename t142, typename t143, typename t144, typename t145>
    t144 apply2(juniper::function<t145, t144(t142, t143)> f, juniper::tuple2<t142, t143> tup);
}

namespace Prelude {
    template<typename t155, typename t156, typename t157, typename t158, typename t159>
    t159 apply3(juniper::function<t158, t159(t155, t156, t157)> f, juniper::tuple3<t155, t156, t157> tup);
}

namespace Prelude {
    template<typename t172, typename t173, typename t174, typename t175, typename t176, typename t177>
    t177 apply4(juniper::function<t175, t177(t172, t173, t174, t176)> f, juniper::tuple4<t172, t173, t174, t176> tup);
}

namespace Prelude {
    template<typename t193, typename t194>
    t193 fst(juniper::tuple2<t193, t194> tup);
}

namespace Prelude {
    template<typename t198, typename t199>
    t199 snd(juniper::tuple2<t198, t199> tup);
}

namespace Prelude {
    template<typename t203>
    t203 add(t203 numA, t203 numB);
}

namespace Prelude {
    template<typename t205>
    t205 sub(t205 numA, t205 numB);
}

namespace Prelude {
    template<typename t207>
    t207 mul(t207 numA, t207 numB);
}

namespace Prelude {
    template<typename t209>
    t209 div(t209 numA, t209 numB);
}

namespace Prelude {
    template<typename t215, typename t216>
    juniper::tuple2<t216, t215> swap(juniper::tuple2<t215, t216> tup);
}

namespace Prelude {
    template<typename t222, typename t223, typename t224>
    t222 until(juniper::function<t223, bool(t222)> p, juniper::function<t224, t222(t222)> f, t222 a0);
}

namespace Prelude {
    template<typename t233>
    juniper::unit ignore(t233 val);
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
    template<typename t292>
    uint8_t toUInt8(t292 n);
}

namespace Prelude {
    template<typename t294>
    int8_t toInt8(t294 n);
}

namespace Prelude {
    template<typename t296>
    uint16_t toUInt16(t296 n);
}

namespace Prelude {
    template<typename t298>
    int16_t toInt16(t298 n);
}

namespace Prelude {
    template<typename t300>
    uint32_t toUInt32(t300 n);
}

namespace Prelude {
    template<typename t302>
    int32_t toInt32(t302 n);
}

namespace Prelude {
    template<typename t304>
    float toFloat(t304 n);
}

namespace Prelude {
    template<typename t306>
    double toDouble(t306 n);
}

namespace Prelude {
    template<typename t308>
    t308 fromUInt8(uint8_t n);
}

namespace Prelude {
    template<typename t310>
    t310 fromInt8(int8_t n);
}

namespace Prelude {
    template<typename t312>
    t312 fromUInt16(uint16_t n);
}

namespace Prelude {
    template<typename t314>
    t314 fromInt16(int16_t n);
}

namespace Prelude {
    template<typename t316>
    t316 fromUInt32(uint32_t n);
}

namespace Prelude {
    template<typename t318>
    t318 fromInt32(int32_t n);
}

namespace Prelude {
    template<typename t320>
    t320 fromFloat(float n);
}

namespace Prelude {
    template<typename t322>
    t322 fromDouble(double n);
}

namespace Prelude {
    template<typename t324, typename t325>
    t325 cast(t324 x);
}

namespace List {
    template<typename t329, typename t332, typename t336, int c4>
    juniper::records::recordt_0<juniper::array<t336, c4>, uint32_t> map(juniper::function<t329, t336(t332)> f, juniper::records::recordt_0<juniper::array<t332, c4>, uint32_t> lst);
}

namespace List {
    template<typename t339, typename t340, typename t344, int c7>
    t340 foldl(juniper::function<t339, t340(t344, t340)> f, t340 initState, juniper::records::recordt_0<juniper::array<t344, c7>, uint32_t> lst);
}

namespace List {
    template<typename t350, typename t351, typename t358, int c9>
    t351 foldr(juniper::function<t350, t351(t358, t351)> f, t351 initState, juniper::records::recordt_0<juniper::array<t358, c9>, uint32_t> lst);
}

namespace List {
    template<typename t374, int c11, int c12, int c13>
    juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t> append(juniper::records::recordt_0<juniper::array<t374, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t374, c12>, uint32_t> lstB);
}

namespace List {
    template<typename t378, int c18>
    t378 nth(uint32_t i, juniper::records::recordt_0<juniper::array<t378, c18>, uint32_t> lst);
}

namespace List {
    template<typename t391, int c20, int c21>
    juniper::records::recordt_0<juniper::array<t391, (c21)*(c20)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t391, c20>, uint32_t>, c21>, uint32_t> listOfLists);
}

namespace List {
    template<typename t398, int c26, int c27>
    juniper::records::recordt_0<juniper::array<t398, c26>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t398, c27>, uint32_t> lst);
}

namespace List {
    template<typename t402, typename t406, int c30>
    bool all(juniper::function<t402, bool(t406)> pred, juniper::records::recordt_0<juniper::array<t406, c30>, uint32_t> lst);
}

namespace List {
    template<typename t411, typename t415, int c32>
    bool any(juniper::function<t411, bool(t415)> pred, juniper::records::recordt_0<juniper::array<t415, c32>, uint32_t> lst);
}

namespace List {
    template<typename t420, int c34>
    juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> pushBack(t420 elem, juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> lst);
}

namespace List {
    template<typename t435, int c36>
    juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> pushOffFront(t435 elem, juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> lst);
}

namespace List {
    template<typename t451, int c40>
    juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> setNth(uint32_t index, t451 elem, juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> lst);
}

namespace List {
    template<typename t456, int c42>
    juniper::records::recordt_0<juniper::array<t456, c42>, uint32_t> replicate(uint32_t numOfElements, t456 elem);
}

namespace List {
    template<typename t468, int c43>
    juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> remove(t468 elem, juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> lst);
}

namespace List {
    template<typename t472, int c47>
    juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> lst);
}

namespace List {
    template<typename t477, typename t481, int c48>
    juniper::unit foreach(juniper::function<t477, juniper::unit(t481)> f, juniper::records::recordt_0<juniper::array<t481, c48>, uint32_t> lst);
}

namespace List {
    template<typename t496, int c50>
    Prelude::maybe<t496> last(juniper::records::recordt_0<juniper::array<t496, c50>, uint32_t> lst);
}

namespace List {
    template<typename t510, int c52>
    Prelude::maybe<t510> max_(juniper::records::recordt_0<juniper::array<t510, c52>, uint32_t> lst);
}

namespace List {
    template<typename t527, int c56>
    Prelude::maybe<t527> min_(juniper::records::recordt_0<juniper::array<t527, c56>, uint32_t> lst);
}

namespace List {
    template<typename t535, int c60>
    bool member(t535 elem, juniper::records::recordt_0<juniper::array<t535, c60>, uint32_t> lst);
}

namespace Math {
    template<typename t540>
    t540 min_(t540 x, t540 y);
}

namespace List {
    template<typename t552, typename t554, int c62>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t552, c62>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t554, c62>, uint32_t> lstB);
}

namespace List {
    template<typename t565, typename t566, int c66>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t>, juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t565, t566>, c66>, uint32_t> lst);
}

namespace List {
    template<typename t574, int c70>
    t574 sum(juniper::records::recordt_0<juniper::array<t574, c70>, uint32_t> lst);
}

namespace List {
    template<typename t592, int c72>
    t592 average(juniper::records::recordt_0<juniper::array<t592, c72>, uint32_t> lst);
}

namespace Signal {
    template<typename t598, typename t600, typename t615>
    Prelude::sig<t615> map(juniper::function<t600, t615(t598)> f, Prelude::sig<t598> s);
}

namespace Signal {
    template<typename t622, typename t623>
    juniper::unit sink(juniper::function<t623, juniper::unit(t622)> f, Prelude::sig<t622> s);
}

namespace Signal {
    template<typename t631, typename t645>
    Prelude::sig<t645> filter(juniper::function<t631, bool(t645)> f, Prelude::sig<t645> s);
}

namespace Signal {
    template<typename t652>
    Prelude::sig<t652> merge(Prelude::sig<t652> sigA, Prelude::sig<t652> sigB);
}

namespace Signal {
    template<typename t656, int c74>
    Prelude::sig<t656> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t656>, c74>, uint32_t> sigs);
}

namespace Signal {
    template<typename t679, typename t703>
    Prelude::sig<Prelude::either<t703, t679>> join(Prelude::sig<t703> sigA, Prelude::sig<t679> sigB);
}

namespace Signal {
    template<typename t726>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t726> s);
}

namespace Signal {
    template<typename t732, typename t733, typename t752>
    Prelude::sig<t752> foldP(juniper::function<t733, t752(t732, t752)> f, juniper::shared_ptr<t752> state0, Prelude::sig<t732> incoming);
}

namespace Signal {
    template<typename t762>
    Prelude::sig<t762> dropRepeats(juniper::shared_ptr<Prelude::maybe<t762>> maybePrevValue, Prelude::sig<t762> incoming);
}

namespace Signal {
    template<typename t780>
    Prelude::sig<t780> latch(juniper::shared_ptr<t780> prevValue, Prelude::sig<t780> incoming);
}

namespace Signal {
    template<typename t792, typename t793, typename t796, typename t805>
    Prelude::sig<t792> map2(juniper::function<t793, t792(t796, t805)> f, juniper::shared_ptr<juniper::tuple2<t796, t805>> state, Prelude::sig<t796> incomingA, Prelude::sig<t805> incomingB);
}

namespace Signal {
    template<typename t837, int c76>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t837, c76>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t837, c76>, uint32_t>> pastValues, Prelude::sig<t837> incoming);
}

namespace Signal {
    template<typename t848>
    Prelude::sig<t848> constant(t848 val);
}

namespace Signal {
    template<typename t858>
    Prelude::sig<Prelude::maybe<t858>> meta(Prelude::sig<t858> sigA);
}

namespace Signal {
    template<typename t875>
    Prelude::sig<t875> unmeta(Prelude::sig<Prelude::maybe<t875>> sigA);
}

namespace Signal {
    template<typename t888, typename t889>
    Prelude::sig<juniper::tuple2<t888, t889>> zip(juniper::shared_ptr<juniper::tuple2<t888, t889>> state, Prelude::sig<t888> sigA, Prelude::sig<t889> sigB);
}

namespace Signal {
    template<typename t920, typename t927>
    juniper::tuple2<Prelude::sig<t920>, Prelude::sig<t927>> unzip(Prelude::sig<juniper::tuple2<t920, t927>> incoming);
}

namespace Signal {
    template<typename t934, typename t935>
    Prelude::sig<t934> toggle(t934 val1, t934 val2, juniper::shared_ptr<t934> state, Prelude::sig<t935> incoming);
}

namespace Io {
    Io::pinState toggle(Io::pinState p);
}

namespace Io {
    juniper::unit printStr(const char * str);
}

namespace Io {
    template<int c78>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c78>, uint32_t> cl);
}

namespace Io {
    juniper::unit printFloat(float f);
}

namespace Io {
    juniper::unit printInt(int32_t n);
}

namespace Io {
    template<typename t963>
    t963 baseToInt(Io::base b);
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
    template<typename t1094, typename t1096, typename t1105>
    Prelude::maybe<t1105> map(juniper::function<t1096, t1105(t1094)> f, Prelude::maybe<t1094> maybeVal);
}

namespace Maybe {
    template<typename t1109>
    t1109 get(Prelude::maybe<t1109> maybeVal);
}

namespace Maybe {
    template<typename t1112>
    bool isJust(Prelude::maybe<t1112> maybeVal);
}

namespace Maybe {
    template<typename t1115>
    bool isNothing(Prelude::maybe<t1115> maybeVal);
}

namespace Maybe {
    template<typename t1121>
    uint8_t count(Prelude::maybe<t1121> maybeVal);
}

namespace Maybe {
    template<typename t1126, typename t1127, typename t1128>
    t1127 foldl(juniper::function<t1126, t1127(t1128, t1127)> f, t1127 initState, Prelude::maybe<t1128> maybeVal);
}

namespace Maybe {
    template<typename t1135, typename t1136, typename t1137>
    t1136 fodlr(juniper::function<t1135, t1136(t1137, t1136)> f, t1136 initState, Prelude::maybe<t1137> maybeVal);
}

namespace Maybe {
    template<typename t1147, typename t1148>
    juniper::unit iter(juniper::function<t1148, juniper::unit(t1147)> f, Prelude::maybe<t1147> maybeVal);
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
    template<typename t1215>
    t1215 max_(t1215 x, t1215 y);
}

namespace Math {
    double mapRange(double x, double a1, double a2, double b1, double b2);
}

namespace Math {
    template<typename t1218>
    t1218 clamp(t1218 x, t1218 min, t1218 max);
}

namespace Math {
    template<typename t1223>
    int8_t sign(t1223 n);
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
    template<typename t1268, int c79>
    juniper::records::recordt_3<juniper::array<t1268, c79>> make(juniper::array<t1268, c79> d);
}

namespace Vector {
    template<typename t1272, int c80>
    t1272 get(uint32_t i, juniper::records::recordt_3<juniper::array<t1272, c80>> v);
}

namespace Vector {
    template<typename t1279, int c82>
    juniper::records::recordt_3<juniper::array<t1279, c82>> add(juniper::records::recordt_3<juniper::array<t1279, c82>> v1, juniper::records::recordt_3<juniper::array<t1279, c82>> v2);
}

namespace Vector {
    template<typename t1283, int c86>
    juniper::records::recordt_3<juniper::array<t1283, c86>> zero();
}

namespace Vector {
    template<typename t1291, int c87>
    juniper::records::recordt_3<juniper::array<t1291, c87>> subtract(juniper::records::recordt_3<juniper::array<t1291, c87>> v1, juniper::records::recordt_3<juniper::array<t1291, c87>> v2);
}

namespace Vector {
    template<typename t1295, int c91>
    juniper::records::recordt_3<juniper::array<t1295, c91>> scale(t1295 scalar, juniper::records::recordt_3<juniper::array<t1295, c91>> v);
}

namespace Vector {
    template<typename t1308, int c94>
    t1308 dot(juniper::records::recordt_3<juniper::array<t1308, c94>> v1, juniper::records::recordt_3<juniper::array<t1308, c94>> v2);
}

namespace Vector {
    template<typename t1316, int c97>
    t1316 magnitude2(juniper::records::recordt_3<juniper::array<t1316, c97>> v);
}

namespace Vector {
    template<typename t1318, int c100>
    double magnitude(juniper::records::recordt_3<juniper::array<t1318, c100>> v);
}

namespace Vector {
    template<typename t1336, int c102>
    juniper::records::recordt_3<juniper::array<t1336, c102>> multiply(juniper::records::recordt_3<juniper::array<t1336, c102>> u, juniper::records::recordt_3<juniper::array<t1336, c102>> v);
}

namespace Vector {
    template<typename t1347, int c106>
    juniper::records::recordt_3<juniper::array<t1347, c106>> normalize(juniper::records::recordt_3<juniper::array<t1347, c106>> v);
}

namespace Vector {
    template<typename t1360, int c110>
    double angle(juniper::records::recordt_3<juniper::array<t1360, c110>> v1, juniper::records::recordt_3<juniper::array<t1360, c110>> v2);
}

namespace Vector {
    template<typename t1414>
    juniper::records::recordt_3<juniper::array<t1414, 3>> cross(juniper::records::recordt_3<juniper::array<t1414, 3>> u, juniper::records::recordt_3<juniper::array<t1414, 3>> v);
}

namespace Vector {
    template<typename t1416, int c126>
    juniper::records::recordt_3<juniper::array<t1416, c126>> project(juniper::records::recordt_3<juniper::array<t1416, c126>> a, juniper::records::recordt_3<juniper::array<t1416, c126>> b);
}

namespace Vector {
    template<typename t1432, int c130>
    juniper::records::recordt_3<juniper::array<t1432, c130>> projectPlane(juniper::records::recordt_3<juniper::array<t1432, c130>> a, juniper::records::recordt_3<juniper::array<t1432, c130>> m);
}

namespace CharList {
    template<int c133>
    juniper::records::recordt_0<juniper::array<uint8_t, c133>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c133>, uint32_t> str);
}

namespace CharList {
    template<int c134>
    juniper::records::recordt_0<juniper::array<uint8_t, c134>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c134>, uint32_t> str);
}

namespace CharList {
    template<int c135>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> i32ToCharList(int32_t m);
}

namespace CharList {
    template<int c136>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c136>, uint32_t> s);
}

namespace CharList {
    template<int c137, int c138, int c139>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c137)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c138)>, uint32_t> sB);
}

namespace CharList {
    template<int c146, int c147>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c146)*(-1)))+((c147)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c146)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c147)>, uint32_t> sB);
}

namespace Random {
    int32_t random_(int32_t low, int32_t high);
}

namespace Random {
    juniper::unit seed(uint32_t n);
}

namespace Random {
    template<typename t1502, int c151>
    t1502 choice(juniper::records::recordt_0<juniper::array<t1502, c151>, uint32_t> lst);
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
    template<int c153>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c153>, uint32_t> bytes);
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
    template<typename t1852>
    uint16_t writeGeneric(Ble::characterstict c, t1852 x);
}

namespace Ble {
    template<typename t1857>
    t1857 readGeneric(Ble::characterstict c);
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
    template<int c154>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c154)>, uint32_t> cl);
}

namespace Gfx {
    template<int c155>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c155)>, uint32_t> cl);
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
    template<int c180>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c180>, uint32_t> cl);
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
    template<typename t10, typename t11, typename t12, typename t8, typename t9>
    juniper::function<juniper::closures::closuret_0<juniper::function<t10, t9(t8)>, juniper::function<t11, t8(t12)>>, t9(t12)> compose(juniper::function<t10, t9(t8)> f, juniper::function<t11, t8(t12)> g) {
        return (([&]() -> juniper::function<juniper::closures::closuret_0<juniper::function<t10, t9(t8)>, juniper::function<t11, t8(t12)>>, t9(t12)> {
            using a = t12;
            using b = t8;
            using c = t9;
            using closureF = t10;
            using closureG = t11;
            return juniper::function<juniper::closures::closuret_0<juniper::function<t10, t9(t8)>, juniper::function<t11, t8(t12)>>, t9(t12)>(juniper::closures::closuret_0<juniper::function<t10, t9(t8)>, juniper::function<t11, t8(t12)>>(f, g), [](juniper::closures::closuret_0<juniper::function<t10, t9(t8)>, juniper::function<t11, t8(t12)>>& junclosure, t12 x) -> t9 { 
                juniper::function<t10, t9(t8)>& f = junclosure.f;
                juniper::function<t11, t8(t12)>& g = junclosure.g;
                return f(g(x));
             });
        })());
    }
}

namespace Prelude {
    template<typename t22, typename t23, typename t24, typename t25>
    juniper::function<juniper::closures::closuret_1<juniper::function<t23, t22(t24, t25)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>, t22(t25)>(t24)> curry(juniper::function<t23, t22(t24, t25)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t23, t22(t24, t25)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>, t22(t25)>(t24)> {
            using a = t24;
            using b = t25;
            using c = t22;
            using closure = t23;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t23, t22(t24, t25)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>, t22(t25)>(t24)>(juniper::closures::closuret_1<juniper::function<t23, t22(t24, t25)>>(f), [](juniper::closures::closuret_1<juniper::function<t23, t22(t24, t25)>>& junclosure, t24 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>, t22(t25)> { 
                juniper::function<t23, t22(t24, t25)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>, t22(t25)>(juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t23, t22(t24, t25)>, t24>& junclosure, t25 valueB) -> t22 { 
                    juniper::function<t23, t22(t24, t25)>& f = junclosure.f;
                    t24& valueA = junclosure.valueA;
                    return f(valueA, valueB);
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t33, typename t34, typename t35, typename t36, typename t37>
    juniper::function<juniper::closures::closuret_1<juniper::function<t34, juniper::function<t35, t33(t37)>(t36)>>, t33(t36, t37)> uncurry(juniper::function<t34, juniper::function<t35, t33(t37)>(t36)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t34, juniper::function<t35, t33(t37)>(t36)>>, t33(t36, t37)> {
            using a = t36;
            using b = t37;
            using c = t33;
            using closureA = t34;
            using closureB = t35;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t34, juniper::function<t35, t33(t37)>(t36)>>, t33(t36,t37)>(juniper::closures::closuret_1<juniper::function<t34, juniper::function<t35, t33(t37)>(t36)>>(f), [](juniper::closures::closuret_1<juniper::function<t34, juniper::function<t35, t33(t37)>(t36)>>& junclosure, t36 valueA, t37 valueB) -> t33 { 
                juniper::function<t34, juniper::function<t35, t33(t37)>(t36)>& f = junclosure.f;
                return f(valueA)(valueB);
             });
        })());
    }
}

namespace Prelude {
    template<typename t48, typename t49, typename t50, typename t51, typename t52>
    juniper::function<juniper::closures::closuret_1<juniper::function<t48, t49(t50, t51, t52)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>, juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)>(t51)>(t50)> curry3(juniper::function<t48, t49(t50, t51, t52)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t48, t49(t50, t51, t52)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>, juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)>(t51)>(t50)> {
            using a = t50;
            using b = t51;
            using c = t52;
            using closureF = t48;
            using d = t49;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t48, t49(t50, t51, t52)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>, juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)>(t51)>(t50)>(juniper::closures::closuret_1<juniper::function<t48, t49(t50, t51, t52)>>(f), [](juniper::closures::closuret_1<juniper::function<t48, t49(t50, t51, t52)>>& junclosure, t50 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>, juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)>(t51)> { 
                juniper::function<t48, t49(t50, t51, t52)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>, juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)>(t51)>(juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t48, t49(t50, t51, t52)>, t50>& junclosure, t51 valueB) -> juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)> { 
                    juniper::function<t48, t49(t50, t51, t52)>& f = junclosure.f;
                    t50& valueA = junclosure.valueA;
                    return juniper::function<juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>, t49(t52)>(juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>(f, valueA, valueB), [](juniper::closures::closuret_3<juniper::function<t48, t49(t50, t51, t52)>, t50, t51>& junclosure, t52 valueC) -> t49 { 
                        juniper::function<t48, t49(t50, t51, t52)>& f = junclosure.f;
                        t50& valueA = junclosure.valueA;
                        t51& valueB = junclosure.valueB;
                        return f(valueA, valueB, valueC);
                     });
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t62, typename t63, typename t64, typename t65, typename t66, typename t67, typename t68>
    juniper::function<juniper::closures::closuret_1<juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)>>, t65(t66, t67, t68)> uncurry3(juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)>>, t65(t66, t67, t68)> {
            using a = t66;
            using b = t67;
            using c = t68;
            using closureA = t62;
            using closureB = t63;
            using closureC = t64;
            using d = t65;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)>>, t65(t66,t67,t68)>(juniper::closures::closuret_1<juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)>>(f), [](juniper::closures::closuret_1<juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)>>& junclosure, t66 valueA, t67 valueB, t68 valueC) -> t65 { 
                juniper::function<t62, juniper::function<t63, juniper::function<t64, t65(t68)>(t67)>(t66)>& f = junclosure.f;
                return f(valueA)(valueB)(valueC);
             });
        })());
    }
}

namespace Prelude {
    template<typename t79>
    bool eq(t79 x, t79 y) {
        return (([&]() -> bool {
            using a = t79;
            return ((bool) (x == y));
        })());
    }
}

namespace Prelude {
    template<typename t85>
    bool neq(t85 x, t85 y) {
        return ((bool) (x != y));
    }
}

namespace Prelude {
    template<typename t91, typename t92>
    bool gt(t91 x, t92 y) {
        return ((bool) (x > y));
    }
}

namespace Prelude {
    template<typename t98, typename t99>
    bool geq(t98 x, t99 y) {
        return ((bool) (x >= y));
    }
}

namespace Prelude {
    template<typename t105, typename t106>
    bool lt(t105 x, t106 y) {
        return ((bool) (x < y));
    }
}

namespace Prelude {
    template<typename t112, typename t113>
    bool leq(t112 x, t113 y) {
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
    template<typename t135, typename t136, typename t137>
    t136 apply(juniper::function<t137, t136(t135)> f, t135 x) {
        return (([&]() -> t136 {
            using a = t135;
            using b = t136;
            using closure = t137;
            return f(x);
        })());
    }
}

namespace Prelude {
    template<typename t142, typename t143, typename t144, typename t145>
    t144 apply2(juniper::function<t145, t144(t142, t143)> f, juniper::tuple2<t142, t143> tup) {
        return (([&]() -> t144 {
            using a = t142;
            using b = t143;
            using c = t144;
            using closure = t145;
            return (([&]() -> t144 {
                juniper::tuple2<t142, t143> guid0 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t143 b = (guid0).e2;
                t142 a = (guid0).e1;
                
                return f(a, b);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t155, typename t156, typename t157, typename t158, typename t159>
    t159 apply3(juniper::function<t158, t159(t155, t156, t157)> f, juniper::tuple3<t155, t156, t157> tup) {
        return (([&]() -> t159 {
            using a = t155;
            using b = t156;
            using c = t157;
            using closure = t158;
            using d = t159;
            return (([&]() -> t159 {
                juniper::tuple3<t155, t156, t157> guid1 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t157 c = (guid1).e3;
                t156 b = (guid1).e2;
                t155 a = (guid1).e1;
                
                return f(a, b, c);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t172, typename t173, typename t174, typename t175, typename t176, typename t177>
    t177 apply4(juniper::function<t175, t177(t172, t173, t174, t176)> f, juniper::tuple4<t172, t173, t174, t176> tup) {
        return (([&]() -> t177 {
            using a = t172;
            using b = t173;
            using c = t174;
            using closure = t175;
            using d = t176;
            using e = t177;
            return (([&]() -> t177 {
                juniper::tuple4<t172, t173, t174, t176> guid2 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t176 d = (guid2).e4;
                t174 c = (guid2).e3;
                t173 b = (guid2).e2;
                t172 a = (guid2).e1;
                
                return f(a, b, c, d);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t193, typename t194>
    t193 fst(juniper::tuple2<t193, t194> tup) {
        return (([&]() -> t193 {
            using a = t193;
            using b = t194;
            return (([&]() -> t193 {
                juniper::tuple2<t193, t194> guid3 = tup;
                return (true ? 
                    (([&]() -> t193 {
                        t193 x = (guid3).e1;
                        return x;
                    })())
                :
                    juniper::quit<t193>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t198, typename t199>
    t199 snd(juniper::tuple2<t198, t199> tup) {
        return (([&]() -> t199 {
            using a = t198;
            using b = t199;
            return (([&]() -> t199 {
                juniper::tuple2<t198, t199> guid4 = tup;
                return (true ? 
                    (([&]() -> t199 {
                        t199 x = (guid4).e2;
                        return x;
                    })())
                :
                    juniper::quit<t199>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t203>
    t203 add(t203 numA, t203 numB) {
        return (([&]() -> t203 {
            using a = t203;
            return ((t203) (numA + numB));
        })());
    }
}

namespace Prelude {
    template<typename t205>
    t205 sub(t205 numA, t205 numB) {
        return (([&]() -> t205 {
            using a = t205;
            return ((t205) (numA - numB));
        })());
    }
}

namespace Prelude {
    template<typename t207>
    t207 mul(t207 numA, t207 numB) {
        return (([&]() -> t207 {
            using a = t207;
            return ((t207) (numA * numB));
        })());
    }
}

namespace Prelude {
    template<typename t209>
    t209 div(t209 numA, t209 numB) {
        return (([&]() -> t209 {
            using a = t209;
            return ((t209) (numA / numB));
        })());
    }
}

namespace Prelude {
    template<typename t215, typename t216>
    juniper::tuple2<t216, t215> swap(juniper::tuple2<t215, t216> tup) {
        return (([&]() -> juniper::tuple2<t216, t215> {
            juniper::tuple2<t215, t216> guid5 = tup;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            t216 beta = (guid5).e2;
            t215 alpha = (guid5).e1;
            
            return (juniper::tuple2<t216,t215>{beta, alpha});
        })());
    }
}

namespace Prelude {
    template<typename t222, typename t223, typename t224>
    t222 until(juniper::function<t223, bool(t222)> p, juniper::function<t224, t222(t222)> f, t222 a0) {
        return (([&]() -> t222 {
            using a = t222;
            return (([&]() -> t222 {
                t222 guid6 = a0;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t222 a = guid6;
                
                (([&]() -> juniper::unit {
                    while (!(p(a))) {
                        (([&]() -> t222 {
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
    template<typename t233>
    juniper::unit ignore(t233 val) {
        return (([&]() -> juniper::unit {
            using a = t233;
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
    template<typename t292>
    uint8_t toUInt8(t292 n) {
        return (([&]() -> uint8_t {
            using t = t292;
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
    template<typename t294>
    int8_t toInt8(t294 n) {
        return (([&]() -> int8_t {
            using t = t294;
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
    template<typename t296>
    uint16_t toUInt16(t296 n) {
        return (([&]() -> uint16_t {
            using t = t296;
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
    template<typename t298>
    int16_t toInt16(t298 n) {
        return (([&]() -> int16_t {
            using t = t298;
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
    template<typename t300>
    uint32_t toUInt32(t300 n) {
        return (([&]() -> uint32_t {
            using t = t300;
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
    template<typename t302>
    int32_t toInt32(t302 n) {
        return (([&]() -> int32_t {
            using t = t302;
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
    template<typename t304>
    float toFloat(t304 n) {
        return (([&]() -> float {
            using t = t304;
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
    template<typename t306>
    double toDouble(t306 n) {
        return (([&]() -> double {
            using t = t306;
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
    template<typename t308>
    t308 fromUInt8(uint8_t n) {
        return (([&]() -> t308 {
            using t = t308;
            return (([&]() -> t308 {
                t308 ret;
                
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
    template<typename t310>
    t310 fromInt8(int8_t n) {
        return (([&]() -> t310 {
            using t = t310;
            return (([&]() -> t310 {
                t310 ret;
                
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
    template<typename t312>
    t312 fromUInt16(uint16_t n) {
        return (([&]() -> t312 {
            using t = t312;
            return (([&]() -> t312 {
                t312 ret;
                
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
    template<typename t314>
    t314 fromInt16(int16_t n) {
        return (([&]() -> t314 {
            using t = t314;
            return (([&]() -> t314 {
                t314 ret;
                
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
    template<typename t316>
    t316 fromUInt32(uint32_t n) {
        return (([&]() -> t316 {
            using t = t316;
            return (([&]() -> t316 {
                t316 ret;
                
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
    template<typename t318>
    t318 fromInt32(int32_t n) {
        return (([&]() -> t318 {
            using t = t318;
            return (([&]() -> t318 {
                t318 ret;
                
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
    template<typename t320>
    t320 fromFloat(float n) {
        return (([&]() -> t320 {
            using t = t320;
            return (([&]() -> t320 {
                t320 ret;
                
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
    template<typename t322>
    t322 fromDouble(double n) {
        return (([&]() -> t322 {
            using t = t322;
            return (([&]() -> t322 {
                t322 ret;
                
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
    template<typename t324, typename t325>
    t325 cast(t324 x) {
        return (([&]() -> t325 {
            using a = t324;
            using b = t325;
            return (([&]() -> t325 {
                t325 ret;
                
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
    template<typename t329, typename t332, typename t336, int c4>
    juniper::records::recordt_0<juniper::array<t336, c4>, uint32_t> map(juniper::function<t329, t336(t332)> f, juniper::records::recordt_0<juniper::array<t332, c4>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t336, c4>, uint32_t> {
            using a = t332;
            using b = t336;
            using closure = t329;
            constexpr int32_t n = c4;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t336, c4>, uint32_t> {
                juniper::array<t336, c4> guid7 = (juniper::array<t336, c4>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t336, c4> ret = guid7;
                
                (([&]() -> juniper::unit {
                    uint32_t guid8 = ((uint32_t) 0);
                    uint32_t guid9 = (lst).length;
                    for (uint32_t i = guid8; i < guid9; i++) {
                        (([&]() -> t336 {
                            return ((ret)[i] = f(((lst).data)[i]));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t336, c4>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t336, c4>, uint32_t> guid10;
                    guid10.data = ret;
                    guid10.length = (lst).length;
                    return guid10;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t339, typename t340, typename t344, int c7>
    t340 foldl(juniper::function<t339, t340(t344, t340)> f, t340 initState, juniper::records::recordt_0<juniper::array<t344, c7>, uint32_t> lst) {
        return (([&]() -> t340 {
            using closure = t339;
            using state = t340;
            using t = t344;
            constexpr int32_t n = c7;
            return (([&]() -> t340 {
                t340 guid11 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t340 s = guid11;
                
                (([&]() -> juniper::unit {
                    uint32_t guid12 = ((uint32_t) 0);
                    uint32_t guid13 = (lst).length;
                    for (uint32_t i = guid12; i < guid13; i++) {
                        (([&]() -> t340 {
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
    template<typename t350, typename t351, typename t358, int c9>
    t351 foldr(juniper::function<t350, t351(t358, t351)> f, t351 initState, juniper::records::recordt_0<juniper::array<t358, c9>, uint32_t> lst) {
        return (([&]() -> t351 {
            using closure = t350;
            using state = t351;
            using t = t358;
            constexpr int32_t n = c9;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> t351 {
                    return initState;
                })())
            :
                (([&]() -> t351 {
                    t351 guid14 = initState;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t351 s = guid14;
                    
                    uint32_t guid15 = (lst).length;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint32_t i = guid15;
                    
                    (([&]() -> juniper::unit {
                        do {
                            (([&]() -> t351 {
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
    template<typename t374, int c11, int c12, int c13>
    juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t> append(juniper::records::recordt_0<juniper::array<t374, c11>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t374, c12>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t> {
            using t = t374;
            constexpr int32_t aCap = c11;
            constexpr int32_t bCap = c12;
            constexpr int32_t retCap = c13;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t> {
                uint32_t guid16 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid16;
                
                juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t> guid17 = (([&]() -> juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t> guid18;
                    guid18.data = (juniper::array<t374, c13>());
                    guid18.length = ((uint32_t) ((lstA).length + (lstB).length));
                    return guid18;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t374, c13>, uint32_t> out = guid17;
                
                (([&]() -> juniper::unit {
                    uint32_t guid19 = ((uint32_t) 0);
                    uint32_t guid20 = (lstA).length;
                    for (uint32_t i = guid19; i < guid20; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((lstA).data)[i]);
                            return (j = ((uint32_t) (j + ((uint32_t) 1))));
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
                            return (j = ((uint32_t) (j + ((uint32_t) 1))));
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
    template<typename t378, int c18>
    t378 nth(uint32_t i, juniper::records::recordt_0<juniper::array<t378, c18>, uint32_t> lst) {
        return (([&]() -> t378 {
            using t = t378;
            constexpr int32_t n = c18;
            return (((bool) (i < (lst).length)) ? 
                (([&]() -> t378 {
                    return ((lst).data)[i];
                })())
            :
                (([&]() -> t378 {
                    return juniper::quit<t378>();
                })()));
        })());
    }
}

namespace List {
    template<typename t391, int c20, int c21>
    juniper::records::recordt_0<juniper::array<t391, (c21)*(c20)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t391, c20>, uint32_t>, c21>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t391, (c21)*(c20)>, uint32_t> {
            using t = t391;
            constexpr int32_t m = c20;
            constexpr int32_t n = c21;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t391, (c21)*(c20)>, uint32_t> {
                juniper::array<t391, (c21)*(c20)> guid23 = (juniper::array<t391, (c21)*(c20)>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t391, (c21)*(c20)> ret = guid23;
                
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
                                        return (index = ((uint32_t) (index + ((uint32_t) 1))));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t391, (c21)*(c20)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t391, (c21)*(c20)>, uint32_t> guid29;
                    guid29.data = ret;
                    guid29.length = index;
                    return guid29;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t398, int c26, int c27>
    juniper::records::recordt_0<juniper::array<t398, c26>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t398, c27>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t398, c26>, uint32_t> {
            using t = t398;
            constexpr int32_t m = c26;
            constexpr int32_t n = c27;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t398, c26>, uint32_t> {
                juniper::array<t398, c26> guid30 = (juniper::array<t398, c26>());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t398, c26> ret = guid30;
                
                (([&]() -> juniper::unit {
                    uint32_t guid31 = ((uint32_t) 0);
                    uint32_t guid32 = (lst).length;
                    for (uint32_t i = guid31; i < guid32; i++) {
                        (([&]() -> t398 {
                            return ((ret)[i] = ((lst).data)[i]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t398, c26>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t398, c26>, uint32_t> guid33;
                    guid33.data = ret;
                    guid33.length = (lst).length;
                    return guid33;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t402, typename t406, int c30>
    bool all(juniper::function<t402, bool(t406)> pred, juniper::records::recordt_0<juniper::array<t406, c30>, uint32_t> lst) {
        return (([&]() -> bool {
            using closure = t402;
            using t = t406;
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
    template<typename t411, typename t415, int c32>
    bool any(juniper::function<t411, bool(t415)> pred, juniper::records::recordt_0<juniper::array<t415, c32>, uint32_t> lst) {
        return (([&]() -> bool {
            using closure = t411;
            using t = t415;
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
    template<typename t420, int c34>
    juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> pushBack(t420 elem, juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> {
            using t = t420;
            constexpr int32_t n = c34;
            return (((bool) ((lst).length >= n)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> {
                    return juniper::quit<juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t>>();
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> guid40 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t420, c34>, uint32_t> ret = guid40;
                    
                    (((ret).data)[(lst).length] = elem);
                    ((ret).length = ((uint32_t) ((lst).length + ((uint32_t) 1))));
                    return ret;
                })()));
        })());
    }
}

namespace List {
    template<typename t435, int c36>
    juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> pushOffFront(t435 elem, juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> {
            using t = t435;
            constexpr int32_t n = c36;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> {
                        return lst;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t435, c36>, uint32_t> ret;
                        
                        (((ret).data)[((uint32_t) 0)] = elem);
                        (([&]() -> juniper::unit {
                            uint32_t guid41 = ((uint32_t) 0);
                            uint32_t guid42 = (lst).length;
                            for (uint32_t i = guid41; i < guid42; i++) {
                                (([&]() -> juniper::unit {
                                    return (([&]() -> juniper::unit {
                                        if (((bool) (((uint32_t) (i + ((uint32_t) 1))) < n))) {
                                            (([&]() -> t435 {
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
    template<typename t451, int c40>
    juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> setNth(uint32_t index, t451 elem, juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> {
            using t = t451;
            constexpr int32_t n = c40;
            return (((bool) (index < (lst).length)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> {
                    juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> guid43 = lst;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> ret = guid43;
                    
                    (((ret).data)[index] = elem);
                    return ret;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t451, c40>, uint32_t> {
                    return lst;
                })()));
        })());
    }
}

namespace List {
    template<typename t456, int c42>
    juniper::records::recordt_0<juniper::array<t456, c42>, uint32_t> replicate(uint32_t numOfElements, t456 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t456, c42>, uint32_t> {
            using t = t456;
            constexpr int32_t n = c42;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t456, c42>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t456, c42>, uint32_t> guid44;
                guid44.data = (juniper::array<t456, c42>().fill(elem));
                guid44.length = numOfElements;
                return guid44;
            })());
        })());
    }
}

namespace List {
    template<typename t468, int c43>
    juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> remove(t468 elem, juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> {
            using t = t468;
            constexpr int32_t n = c43;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> {
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
                    (([&]() -> juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> guid49 = lst;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> ret = guid49;
                        
                        ((ret).length = ((uint32_t) ((lst).length - ((uint32_t) 1))));
                        (([&]() -> juniper::unit {
                            uint32_t guid50 = index;
                            uint32_t guid51 = ((uint32_t) ((lst).length - ((uint32_t) 1)));
                            for (uint32_t i = guid50; i < guid51; i++) {
                                (([&]() -> t468 {
                                    return (((ret).data)[i] = ((lst).data)[((uint32_t) (i + ((uint32_t) 1)))]);
                                })());
                            }
                            return {};
                        })());
                        return ret;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t468, c43>, uint32_t> {
                        return lst;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t472, int c47>
    juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> dropLast(juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> {
            using t = t472;
            constexpr int32_t n = c47;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> {
                    return lst;
                })())
            :
                (([&]() -> juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<t472, c47>, uint32_t> guid52;
                        guid52.data = (lst).data;
                        guid52.length = ((uint32_t) ((lst).length - ((uint32_t) 1)));
                        return guid52;
                    })());
                })()));
        })());
    }
}

namespace List {
    template<typename t477, typename t481, int c48>
    juniper::unit foreach(juniper::function<t477, juniper::unit(t481)> f, juniper::records::recordt_0<juniper::array<t481, c48>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            using closure = t477;
            using t = t481;
            constexpr int32_t n = c48;
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
    template<typename t496, int c50>
    Prelude::maybe<t496> last(juniper::records::recordt_0<juniper::array<t496, c50>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t496> {
            using t = t496;
            constexpr int32_t n = c50;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t496> {
                    return nothing<t496>();
                })())
            :
                (([&]() -> Prelude::maybe<t496> {
                    return just<t496>(((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))]);
                })()));
        })());
    }
}

namespace List {
    template<typename t510, int c52>
    Prelude::maybe<t510> max_(juniper::records::recordt_0<juniper::array<t510, c52>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t510> {
            using t = t510;
            constexpr int32_t n = c52;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t510> {
                    return nothing<t510>();
                })())
            :
                (([&]() -> Prelude::maybe<t510> {
                    t510 guid55 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t510 maxVal = guid55;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid56 = ((uint32_t) 1);
                        uint32_t guid57 = (lst).length;
                        for (uint32_t i = guid56; i < guid57; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] > maxVal))) {
                                        (([&]() -> t510 {
                                            return (maxVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t510>(maxVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t527, int c56>
    Prelude::maybe<t527> min_(juniper::records::recordt_0<juniper::array<t527, c56>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t527> {
            using t = t527;
            constexpr int32_t n = c56;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t527> {
                    return nothing<t527>();
                })())
            :
                (([&]() -> Prelude::maybe<t527> {
                    t527 guid58 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t527 minVal = guid58;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid59 = ((uint32_t) 1);
                        uint32_t guid60 = (lst).length;
                        for (uint32_t i = guid59; i < guid60; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] < minVal))) {
                                        (([&]() -> t527 {
                                            return (minVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t527>(minVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t535, int c60>
    bool member(t535 elem, juniper::records::recordt_0<juniper::array<t535, c60>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t535;
            constexpr int32_t n = c60;
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

namespace Math {
    template<typename t540>
    t540 min_(t540 x, t540 y) {
        return (([&]() -> t540 {
            using a = t540;
            return (((bool) (x > y)) ? 
                (([&]() -> t540 {
                    return y;
                })())
            :
                (([&]() -> t540 {
                    return x;
                })()));
        })());
    }
}

namespace List {
    template<typename t552, typename t554, int c62>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t552, c62>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t554, c62>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t> {
            using a = t552;
            using b = t554;
            constexpr int32_t n = c62;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t> {
                uint32_t guid64 = Math::min_<uint32_t>((lstA).length, (lstB).length);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t outLen = guid64;
                
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t> guid65 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t> guid66;
                    guid66.data = (juniper::array<juniper::tuple2<t552, t554>, c62>());
                    guid66.length = outLen;
                    return guid66;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t552, t554>, c62>, uint32_t> ret = guid65;
                
                (([&]() -> juniper::unit {
                    uint32_t guid67 = ((uint32_t) 0);
                    uint32_t guid68 = outLen;
                    for (uint32_t i = guid67; i < guid68; i++) {
                        (([&]() -> juniper::tuple2<t552, t554> {
                            return (((ret).data)[i] = (juniper::tuple2<t552,t554>{((lstA).data)[i], ((lstB).data)[i]}));
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
    template<typename t565, typename t566, int c66>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t>, juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t565, t566>, c66>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t>, juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t>> {
            using a = t565;
            using b = t566;
            constexpr int32_t n = c66;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t>, juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t> guid69 = (([&]() -> juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t> guid70;
                    guid70.data = (juniper::array<t565, c66>());
                    guid70.length = (lst).length;
                    return guid70;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t> retA = guid69;
                
                juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t> guid71 = (([&]() -> juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t> guid72;
                    guid72.data = (juniper::array<t566, c66>());
                    guid72.length = (lst).length;
                    return guid72;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t> retB = guid71;
                
                (([&]() -> juniper::unit {
                    uint32_t guid73 = ((uint32_t) 0);
                    uint32_t guid74 = (lst).length;
                    for (uint32_t i = guid73; i < guid74; i++) {
                        (([&]() -> t566 {
                            juniper::tuple2<t565, t566> guid75 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t566 b = (guid75).e2;
                            t565 a = (guid75).e1;
                            
                            (((retA).data)[i] = a);
                            return (((retB).data)[i] = b);
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t565, c66>, uint32_t>,juniper::records::recordt_0<juniper::array<t566, c66>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t574, int c70>
    t574 sum(juniper::records::recordt_0<juniper::array<t574, c70>, uint32_t> lst) {
        return (([&]() -> t574 {
            using a = t574;
            constexpr int32_t n = c70;
            return List::foldl<void, t574, t574, c70>(Prelude::add<t574>, ((t574) 0), lst);
        })());
    }
}

namespace List {
    template<typename t592, int c72>
    t592 average(juniper::records::recordt_0<juniper::array<t592, c72>, uint32_t> lst) {
        return (([&]() -> t592 {
            using a = t592;
            constexpr int32_t n = c72;
            return ((t592) (sum<t592, c72>(lst) / cast<uint32_t, t592>((lst).length)));
        })());
    }
}

namespace Signal {
    template<typename t598, typename t600, typename t615>
    Prelude::sig<t615> map(juniper::function<t600, t615(t598)> f, Prelude::sig<t598> s) {
        return (([&]() -> Prelude::sig<t615> {
            using a = t598;
            using b = t615;
            using closure = t600;
            return (([&]() -> Prelude::sig<t615> {
                Prelude::sig<t598> guid76 = s;
                return (((bool) (((bool) ((guid76).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid76).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t615> {
                        t598 val = ((guid76).signal()).just();
                        return signal<t615>(just<t615>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t615> {
                            return signal<t615>(nothing<t615>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t615>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t622, typename t623>
    juniper::unit sink(juniper::function<t623, juniper::unit(t622)> f, Prelude::sig<t622> s) {
        return (([&]() -> juniper::unit {
            using a = t622;
            using closure = t623;
            return (([&]() -> juniper::unit {
                Prelude::sig<t622> guid77 = s;
                return (((bool) (((bool) ((guid77).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid77).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::unit {
                        t622 val = ((guid77).signal()).just();
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
    template<typename t631, typename t645>
    Prelude::sig<t645> filter(juniper::function<t631, bool(t645)> f, Prelude::sig<t645> s) {
        return (([&]() -> Prelude::sig<t645> {
            using a = t645;
            using closure = t631;
            return (([&]() -> Prelude::sig<t645> {
                Prelude::sig<t645> guid78 = s;
                return (((bool) (((bool) ((guid78).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid78).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t645> {
                        t645 val = ((guid78).signal()).just();
                        return (f(val) ? 
                            (([&]() -> Prelude::sig<t645> {
                                return signal<t645>(nothing<t645>());
                            })())
                        :
                            (([&]() -> Prelude::sig<t645> {
                                return s;
                            })()));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t645> {
                            return signal<t645>(nothing<t645>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t645>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t652>
    Prelude::sig<t652> merge(Prelude::sig<t652> sigA, Prelude::sig<t652> sigB) {
        return (([&]() -> Prelude::sig<t652> {
            using a = t652;
            return (([&]() -> Prelude::sig<t652> {
                Prelude::sig<t652> guid79 = sigA;
                return (((bool) (((bool) ((guid79).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid79).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t652> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t652> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t652>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t656, int c74>
    Prelude::sig<t656> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t656>, c74>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t656> {
            using a = t656;
            constexpr int32_t n = c74;
            return (([&]() -> Prelude::sig<t656> {
                Prelude::maybe<t656> guid80 = List::foldl<void, Prelude::maybe<t656>, Prelude::sig<t656>, c74>(juniper::function<void, Prelude::maybe<t656>(Prelude::sig<t656>,Prelude::maybe<t656>)>([](Prelude::sig<t656> sig, Prelude::maybe<t656> accum) -> Prelude::maybe<t656> { 
                    return (([&]() -> Prelude::maybe<t656> {
                        Prelude::maybe<t656> guid81 = accum;
                        return (((bool) (((bool) ((guid81).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::maybe<t656> {
                                return (([&]() -> Prelude::maybe<t656> {
                                    Prelude::sig<t656> guid82 = sig;
                                    if (!(((bool) (((bool) ((guid82).id() == ((uint8_t) 0))) && true)))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t656> heldValue = (guid82).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t656> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t656>>()));
                    })());
                 }), nothing<t656>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t656> ret = guid80;
                
                return signal<t656>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t679, typename t703>
    Prelude::sig<Prelude::either<t703, t679>> join(Prelude::sig<t703> sigA, Prelude::sig<t679> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t703, t679>> {
            using a = t703;
            using b = t679;
            return (([&]() -> Prelude::sig<Prelude::either<t703, t679>> {
                juniper::tuple2<Prelude::sig<t703>, Prelude::sig<t679>> guid83 = (juniper::tuple2<Prelude::sig<t703>,Prelude::sig<t679>>{sigA, sigB});
                return (((bool) (((bool) (((guid83).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid83).e1).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t703, t679>> {
                        t703 value = (((guid83).e1).signal()).just();
                        return signal<Prelude::either<t703, t679>>(just<Prelude::either<t703, t679>>(left<t703, t679>(value)));
                    })())
                :
                    (((bool) (((bool) (((guid83).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid83).e2).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t703, t679>> {
                            t679 value = (((guid83).e2).signal()).just();
                            return signal<Prelude::either<t703, t679>>(just<Prelude::either<t703, t679>>(right<t703, t679>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t703, t679>> {
                                return signal<Prelude::either<t703, t679>>(nothing<Prelude::either<t703, t679>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t703, t679>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t726>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t726> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t726;
            return map<t726, void, juniper::unit>(juniper::function<void, juniper::unit(t726)>([](t726 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t732, typename t733, typename t752>
    Prelude::sig<t752> foldP(juniper::function<t733, t752(t732, t752)> f, juniper::shared_ptr<t752> state0, Prelude::sig<t732> incoming) {
        return (([&]() -> Prelude::sig<t752> {
            using a = t732;
            using closure = t733;
            using state = t752;
            return (([&]() -> Prelude::sig<t752> {
                Prelude::sig<t732> guid84 = incoming;
                return (((bool) (((bool) ((guid84).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid84).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t752> {
                        t732 val = ((guid84).signal()).just();
                        return (([&]() -> Prelude::sig<t752> {
                            t752 guid85 = f(val, (*((state0).get())));
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t752 state1 = guid85;
                            
                            (*((state0).get()) = state1);
                            return signal<t752>(just<t752>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t752> {
                            return signal<t752>(nothing<t752>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t752>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t762>
    Prelude::sig<t762> dropRepeats(juniper::shared_ptr<Prelude::maybe<t762>> maybePrevValue, Prelude::sig<t762> incoming) {
        return (([&]() -> Prelude::sig<t762> {
            using a = t762;
            return filter<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t762>>>, t762>(juniper::function<juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t762>>>, bool(t762)>(juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t762>>>(maybePrevValue), [](juniper::closures::closuret_4<juniper::shared_ptr<Prelude::maybe<t762>>>& junclosure, t762 value) -> bool { 
                juniper::shared_ptr<Prelude::maybe<t762>>& maybePrevValue = junclosure.maybePrevValue;
                return (([&]() -> bool {
                    bool guid86 = (([&]() -> bool {
                        Prelude::maybe<t762> guid87 = (*((maybePrevValue).get()));
                        return (((bool) (((bool) ((guid87).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> bool {
                                return false;
                            })())
                        :
                            (((bool) (((bool) ((guid87).id() == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> bool {
                                    t762 prevValue = (guid87).just();
                                    return ((bool) (value == prevValue));
                                })())
                            :
                                juniper::quit<bool>()));
                    })());
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    bool filtered = guid86;
                    
                    (([&]() -> juniper::unit {
                        if (!(filtered)) {
                            (([&]() -> Prelude::maybe<t762> {
                                return (*((maybePrevValue).get()) = just<t762>(value));
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
    template<typename t780>
    Prelude::sig<t780> latch(juniper::shared_ptr<t780> prevValue, Prelude::sig<t780> incoming) {
        return (([&]() -> Prelude::sig<t780> {
            using a = t780;
            return (([&]() -> Prelude::sig<t780> {
                Prelude::sig<t780> guid88 = incoming;
                return (((bool) (((bool) ((guid88).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid88).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t780> {
                        t780 val = ((guid88).signal()).just();
                        return (([&]() -> Prelude::sig<t780> {
                            (*((prevValue).get()) = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t780> {
                            return signal<t780>(just<t780>((*((prevValue).get()))));
                        })())
                    :
                        juniper::quit<Prelude::sig<t780>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t792, typename t793, typename t796, typename t805>
    Prelude::sig<t792> map2(juniper::function<t793, t792(t796, t805)> f, juniper::shared_ptr<juniper::tuple2<t796, t805>> state, Prelude::sig<t796> incomingA, Prelude::sig<t805> incomingB) {
        return (([&]() -> Prelude::sig<t792> {
            using a = t796;
            using b = t805;
            using c = t792;
            using closure = t793;
            return (([&]() -> Prelude::sig<t792> {
                t796 guid89 = (([&]() -> t796 {
                    Prelude::sig<t796> guid90 = incomingA;
                    return (((bool) (((bool) ((guid90).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid90).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t796 {
                            t796 val1 = ((guid90).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t796 {
                                return fst<t796, t805>((*((state).get())));
                            })())
                        :
                            juniper::quit<t796>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t796 valA = guid89;
                
                t805 guid91 = (([&]() -> t805 {
                    Prelude::sig<t805> guid92 = incomingB;
                    return (((bool) (((bool) ((guid92).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid92).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t805 {
                            t805 val2 = ((guid92).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t805 {
                                return snd<t796, t805>((*((state).get())));
                            })())
                        :
                            juniper::quit<t805>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t805 valB = guid91;
                
                (*((state).get()) = (juniper::tuple2<t796,t805>{valA, valB}));
                return (([&]() -> Prelude::sig<t792> {
                    juniper::tuple2<Prelude::sig<t796>, Prelude::sig<t805>> guid93 = (juniper::tuple2<Prelude::sig<t796>,Prelude::sig<t805>>{incomingA, incomingB});
                    return (((bool) (((bool) (((guid93).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid93).e2).signal()).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid93).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid93).e1).signal()).id() == ((uint8_t) 1))) && true)))))))) ? 
                        (([&]() -> Prelude::sig<t792> {
                            return signal<t792>(nothing<t792>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t792> {
                                return signal<t792>(just<t792>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t792>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t837, int c76>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t837, c76>, uint32_t>> record(juniper::shared_ptr<juniper::records::recordt_0<juniper::array<t837, c76>, uint32_t>> pastValues, Prelude::sig<t837> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t837, c76>, uint32_t>> {
            using a = t837;
            constexpr int32_t n = c76;
            return foldP<t837, void, juniper::records::recordt_0<juniper::array<t837, c76>, uint32_t>>(List::pushOffFront<t837, c76>, pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t848>
    Prelude::sig<t848> constant(t848 val) {
        return (([&]() -> Prelude::sig<t848> {
            using a = t848;
            return signal<t848>(just<t848>(val));
        })());
    }
}

namespace Signal {
    template<typename t858>
    Prelude::sig<Prelude::maybe<t858>> meta(Prelude::sig<t858> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t858>> {
            using a = t858;
            return (([&]() -> Prelude::sig<Prelude::maybe<t858>> {
                Prelude::sig<t858> guid94 = sigA;
                if (!(((bool) (((bool) ((guid94).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t858> val = (guid94).signal();
                
                return constant<Prelude::maybe<t858>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t875>
    Prelude::sig<t875> unmeta(Prelude::sig<Prelude::maybe<t875>> sigA) {
        return (([&]() -> Prelude::sig<t875> {
            using a = t875;
            return (([&]() -> Prelude::sig<t875> {
                Prelude::sig<Prelude::maybe<t875>> guid95 = sigA;
                return (((bool) (((bool) ((guid95).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid95).signal()).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid95).signal()).just()).id() == ((uint8_t) 0))) && true)))))) ? 
                    (([&]() -> Prelude::sig<t875> {
                        t875 val = (((guid95).signal()).just()).just();
                        return constant<t875>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t875> {
                            return signal<t875>(nothing<t875>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t875>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t888, typename t889>
    Prelude::sig<juniper::tuple2<t888, t889>> zip(juniper::shared_ptr<juniper::tuple2<t888, t889>> state, Prelude::sig<t888> sigA, Prelude::sig<t889> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t888, t889>> {
            using a = t888;
            using b = t889;
            return map2<juniper::tuple2<t888, t889>, void, t888, t889>(juniper::function<void, juniper::tuple2<t888, t889>(t888,t889)>([](t888 valA, t889 valB) -> juniper::tuple2<t888, t889> { 
                return (juniper::tuple2<t888,t889>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t920, typename t927>
    juniper::tuple2<Prelude::sig<t920>, Prelude::sig<t927>> unzip(Prelude::sig<juniper::tuple2<t920, t927>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t920>, Prelude::sig<t927>> {
            using a = t920;
            using b = t927;
            return (([&]() -> juniper::tuple2<Prelude::sig<t920>, Prelude::sig<t927>> {
                Prelude::sig<juniper::tuple2<t920, t927>> guid96 = incoming;
                return (((bool) (((bool) ((guid96).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid96).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t920>, Prelude::sig<t927>> {
                        t927 y = (((guid96).signal()).just()).e2;
                        t920 x = (((guid96).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t920>,Prelude::sig<t927>>{signal<t920>(just<t920>(x)), signal<t927>(just<t927>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t920>, Prelude::sig<t927>> {
                            return (juniper::tuple2<Prelude::sig<t920>,Prelude::sig<t927>>{signal<t920>(nothing<t920>()), signal<t927>(nothing<t927>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t920>, Prelude::sig<t927>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t934, typename t935>
    Prelude::sig<t934> toggle(t934 val1, t934 val2, juniper::shared_ptr<t934> state, Prelude::sig<t935> incoming) {
        return (([&]() -> Prelude::sig<t934> {
            using a = t934;
            using b = t935;
            return foldP<t935, juniper::closures::closuret_5<t934, t934>, t934>(juniper::function<juniper::closures::closuret_5<t934, t934>, t934(t935,t934)>(juniper::closures::closuret_5<t934, t934>(val1, val2), [](juniper::closures::closuret_5<t934, t934>& junclosure, t935 event, t934 prevVal) -> t934 { 
                t934& val1 = junclosure.val1;
                t934& val2 = junclosure.val2;
                return (((bool) (prevVal == val1)) ? 
                    (([&]() -> t934 {
                        return val2;
                    })())
                :
                    (([&]() -> t934 {
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
            return (((bool) (((bool) ((guid97).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> Io::pinState {
                    return low();
                })())
            :
                (((bool) (((bool) ((guid97).id() == ((uint8_t) 1))) && true)) ? 
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
    template<int c78>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c78>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c78;
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
    template<typename t963>
    t963 baseToInt(Io::base b) {
        return (([&]() -> t963 {
            Io::base guid98 = b;
            return (((bool) (((bool) ((guid98).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> t963 {
                    return ((t963) 2);
                })())
            :
                (((bool) (((bool) ((guid98).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> t963 {
                        return ((t963) 8);
                    })())
                :
                    (((bool) (((bool) ((guid98).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> t963 {
                            return ((t963) 10);
                        })())
                    :
                        (((bool) (((bool) ((guid98).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> t963 {
                                return ((t963) 16);
                            })())
                        :
                            juniper::quit<t963>()))));
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
            return (((bool) (((bool) ((guid100).id() == ((uint8_t) 1))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid100).id() == ((uint8_t) 0))) && true)) ? 
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
            return (((bool) (((bool) ((guid102).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid102).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    (((bool) (((bool) ((guid102).id() == ((uint8_t) 2))) && true)) ? 
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
            return (((bool) (((bool) (guid103 == ((int32_t) 0))) && true)) ? 
                (([&]() -> Io::mode {
                    return input();
                })())
            :
                (((bool) (((bool) (guid103 == ((int32_t) 1))) && true)) ? 
                    (([&]() -> Io::mode {
                        return output();
                    })())
                :
                    (((bool) (((bool) (guid103 == ((int32_t) 2))) && true)) ? 
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
                    juniper::tuple2<Io::pinState, Io::pinState> guid106 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid106).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid106).e1).id() == ((uint8_t) 0))) && true)))) ? 
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
                    juniper::tuple2<Io::pinState, Io::pinState> guid108 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid108).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid108).e1).id() == ((uint8_t) 1))) && true)))) ? 
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
                    juniper::tuple2<Io::pinState, Io::pinState> guid110 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, (*((prevState).get()))});
                    return (((bool) (((bool) (((guid110).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid110).e1).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        (((bool) (((bool) (((guid110).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid110).e1).id() == ((uint8_t) 1))) && true)))) ? 
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
    template<typename t1094, typename t1096, typename t1105>
    Prelude::maybe<t1105> map(juniper::function<t1096, t1105(t1094)> f, Prelude::maybe<t1094> maybeVal) {
        return (([&]() -> Prelude::maybe<t1105> {
            using a = t1094;
            using b = t1105;
            using closure = t1096;
            return (([&]() -> Prelude::maybe<t1105> {
                Prelude::maybe<t1094> guid111 = maybeVal;
                return (((bool) (((bool) ((guid111).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t1105> {
                        t1094 val = (guid111).just();
                        return just<t1105>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t1105> {
                            return nothing<t1105>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t1105>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1109>
    t1109 get(Prelude::maybe<t1109> maybeVal) {
        return (([&]() -> t1109 {
            using a = t1109;
            return (([&]() -> t1109 {
                Prelude::maybe<t1109> guid112 = maybeVal;
                return (((bool) (((bool) ((guid112).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t1109 {
                        t1109 val = (guid112).just();
                        return val;
                    })())
                :
                    juniper::quit<t1109>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1112>
    bool isJust(Prelude::maybe<t1112> maybeVal) {
        return (([&]() -> bool {
            using a = t1112;
            return (([&]() -> bool {
                Prelude::maybe<t1112> guid113 = maybeVal;
                return (((bool) (((bool) ((guid113).id() == ((uint8_t) 0))) && true)) ? 
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
    template<typename t1115>
    bool isNothing(Prelude::maybe<t1115> maybeVal) {
        return (([&]() -> bool {
            using a = t1115;
            return !(isJust<t1115>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t1121>
    uint8_t count(Prelude::maybe<t1121> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t1121;
            return (([&]() -> uint8_t {
                Prelude::maybe<t1121> guid114 = maybeVal;
                return (((bool) (((bool) ((guid114).id() == ((uint8_t) 0))) && true)) ? 
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
    template<typename t1126, typename t1127, typename t1128>
    t1127 foldl(juniper::function<t1126, t1127(t1128, t1127)> f, t1127 initState, Prelude::maybe<t1128> maybeVal) {
        return (([&]() -> t1127 {
            using closure = t1126;
            using state = t1127;
            using t = t1128;
            return (([&]() -> t1127 {
                Prelude::maybe<t1128> guid115 = maybeVal;
                return (((bool) (((bool) ((guid115).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t1127 {
                        t1128 val = (guid115).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t1127 {
                            return initState;
                        })())
                    :
                        juniper::quit<t1127>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t1135, typename t1136, typename t1137>
    t1136 fodlr(juniper::function<t1135, t1136(t1137, t1136)> f, t1136 initState, Prelude::maybe<t1137> maybeVal) {
        return (([&]() -> t1136 {
            using closure = t1135;
            using state = t1136;
            using t = t1137;
            return foldl<t1135, t1136, t1137>(f, initState, maybeVal);
        })());
    }
}

namespace Maybe {
    template<typename t1147, typename t1148>
    juniper::unit iter(juniper::function<t1148, juniper::unit(t1147)> f, Prelude::maybe<t1147> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t1147;
            using closure = t1148;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t1147> guid116 = maybeVal;
                return (((bool) (((bool) ((guid116).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> juniper::unit {
                        t1147 val = (guid116).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t1147> nothing = guid116;
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
            
            uint32_t guid120 = (((bool) (interval == ((uint32_t) 0))) ? 
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
            uint32_t lastWindow = guid120;
            
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
            double guid121 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid121;
            
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
            double guid122 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid122;
            
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
            double guid123 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid123;
            
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
            double guid124 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid124;
            
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
            double guid125 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid125;
            
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
            double guid126 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid126;
            
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
            double guid127 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid127;
            
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
            double guid128 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid128;
            
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
            double guid129 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid129;
            
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
            double guid130 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid130;
            
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
            double guid131 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid131;
            
            int32_t guid132 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t exponent = guid132;
            
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
            double guid133 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid133;
            
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
            double guid134 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid134;
            
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
            double guid135 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid135;
            
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
            double guid136 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid136;
            
            double guid137 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double integer = guid137;
            
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
            double guid138 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid138;
            
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
            double guid139 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid139;
            
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
            double guid140 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid140;
            
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
            double guid141 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid141;
            
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
            double guid142 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid142;
            
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
            double guid143 = 0.0;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid143;
            
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
    template<typename t1215>
    t1215 max_(t1215 x, t1215 y) {
        return (([&]() -> t1215 {
            using a = t1215;
            return (((bool) (x > y)) ? 
                (([&]() -> t1215 {
                    return x;
                })())
            :
                (([&]() -> t1215 {
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
    template<typename t1218>
    t1218 clamp(t1218 x, t1218 min, t1218 max) {
        return (([&]() -> t1218 {
            using a = t1218;
            return (((bool) (min > x)) ? 
                (([&]() -> t1218 {
                    return min;
                })())
            :
                (((bool) (x > max)) ? 
                    (([&]() -> t1218 {
                        return max;
                    })())
                :
                    (([&]() -> t1218 {
                        return x;
                    })())));
        })());
    }
}

namespace Math {
    template<typename t1223>
    int8_t sign(t1223 n) {
        return (([&]() -> int8_t {
            using a = t1223;
            return (((bool) (n == ((t1223) 0))) ? 
                (([&]() -> int8_t {
                    return ((int8_t) 0);
                })())
            :
                (((bool) (n > ((t1223) 0))) ? 
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
            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid144;
            guid144.actualState = Io::low();
            guid144.lastState = Io::low();
            guid144.lastDebounceTime = ((uint32_t) 0);
            return guid144;
        })()))));
    }
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>> buttonState) {
        return Signal::map<Io::pinState, juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>, Io::pinState>(juniper::function<juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>, Io::pinState(Io::pinState)>(juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>(buttonState, delay), [](juniper::closures::closuret_8<juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>, uint16_t>& junclosure, Io::pinState currentState) -> Io::pinState { 
            juniper::shared_ptr<juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>>& buttonState = junclosure.buttonState;
            uint16_t& delay = junclosure.delay;
            return (([&]() -> Io::pinState {
                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid145 = (*((buttonState).get()));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lastDebounceTime = (guid145).lastDebounceTime;
                Io::pinState lastState = (guid145).lastState;
                Io::pinState actualState = (guid145).actualState;
                
                return (((bool) (currentState != lastState)) ? 
                    (([&]() -> Io::pinState {
                        (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                            juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid146;
                            guid146.actualState = actualState;
                            guid146.lastState = currentState;
                            guid146.lastDebounceTime = Time::now();
                            return guid146;
                        })()));
                        return actualState;
                    })())
                :
                    (((bool) (((bool) (currentState != actualState)) && ((bool) (((uint32_t) (Time::now() - ((buttonState).get())->lastDebounceTime)) > delay)))) ? 
                        (([&]() -> Io::pinState {
                            (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid147;
                                guid147.actualState = currentState;
                                guid147.lastState = currentState;
                                guid147.lastDebounceTime = lastDebounceTime;
                                return guid147;
                            })()));
                            return currentState;
                        })())
                    :
                        (([&]() -> Io::pinState {
                            (*((buttonState).get()) = (([&]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
                                juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid148;
                                guid148.actualState = actualState;
                                guid148.lastState = currentState;
                                guid148.lastDebounceTime = lastDebounceTime;
                                return guid148;
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
    template<typename t1268, int c79>
    juniper::records::recordt_3<juniper::array<t1268, c79>> make(juniper::array<t1268, c79> d) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1268, c79>> {
            using a = t1268;
            constexpr int32_t n = c79;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1268, c79>>{
                juniper::records::recordt_3<juniper::array<t1268, c79>> guid149;
                guid149.data = d;
                return guid149;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1272, int c80>
    t1272 get(uint32_t i, juniper::records::recordt_3<juniper::array<t1272, c80>> v) {
        return (([&]() -> t1272 {
            using a = t1272;
            constexpr int32_t n = c80;
            return ((v).data)[i];
        })());
    }
}

namespace Vector {
    template<typename t1279, int c82>
    juniper::records::recordt_3<juniper::array<t1279, c82>> add(juniper::records::recordt_3<juniper::array<t1279, c82>> v1, juniper::records::recordt_3<juniper::array<t1279, c82>> v2) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1279, c82>> {
            using a = t1279;
            constexpr int32_t n = c82;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1279, c82>> {
                juniper::records::recordt_3<juniper::array<t1279, c82>> guid150 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1279, c82>> result = guid150;
                
                (([&]() -> juniper::unit {
                    int32_t guid151 = ((int32_t) 0);
                    int32_t guid152 = n;
                    for (int32_t i = guid151; i < guid152; i++) {
                        (([&]() -> t1279 {
                            return (((result).data)[i] = ((t1279) (((result).data)[i] + ((v2).data)[i])));
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
    template<typename t1283, int c86>
    juniper::records::recordt_3<juniper::array<t1283, c86>> zero() {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1283, c86>> {
            using a = t1283;
            constexpr int32_t n = c86;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1283, c86>>{
                juniper::records::recordt_3<juniper::array<t1283, c86>> guid153;
                guid153.data = (juniper::array<t1283, c86>().fill(((t1283) 0)));
                return guid153;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1291, int c87>
    juniper::records::recordt_3<juniper::array<t1291, c87>> subtract(juniper::records::recordt_3<juniper::array<t1291, c87>> v1, juniper::records::recordt_3<juniper::array<t1291, c87>> v2) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1291, c87>> {
            using a = t1291;
            constexpr int32_t n = c87;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1291, c87>> {
                juniper::records::recordt_3<juniper::array<t1291, c87>> guid154 = v1;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1291, c87>> result = guid154;
                
                (([&]() -> juniper::unit {
                    int32_t guid155 = ((int32_t) 0);
                    int32_t guid156 = n;
                    for (int32_t i = guid155; i < guid156; i++) {
                        (([&]() -> t1291 {
                            return (((result).data)[i] = ((t1291) (((result).data)[i] - ((v2).data)[i])));
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
    template<typename t1295, int c91>
    juniper::records::recordt_3<juniper::array<t1295, c91>> scale(t1295 scalar, juniper::records::recordt_3<juniper::array<t1295, c91>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1295, c91>> {
            using a = t1295;
            constexpr int32_t n = c91;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1295, c91>> {
                juniper::records::recordt_3<juniper::array<t1295, c91>> guid157 = v;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1295, c91>> result = guid157;
                
                (([&]() -> juniper::unit {
                    int32_t guid158 = ((int32_t) 0);
                    int32_t guid159 = n;
                    for (int32_t i = guid158; i < guid159; i++) {
                        (([&]() -> t1295 {
                            return (((result).data)[i] = ((t1295) (((result).data)[i] * scalar)));
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
    template<typename t1308, int c94>
    t1308 dot(juniper::records::recordt_3<juniper::array<t1308, c94>> v1, juniper::records::recordt_3<juniper::array<t1308, c94>> v2) {
        return (([&]() -> t1308 {
            using a = t1308;
            constexpr int32_t n = c94;
            return (([&]() -> t1308 {
                t1308 guid160 = ((t1308) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1308 sum = guid160;
                
                (([&]() -> juniper::unit {
                    int32_t guid161 = ((int32_t) 0);
                    int32_t guid162 = n;
                    for (int32_t i = guid161; i < guid162; i++) {
                        (([&]() -> t1308 {
                            return (sum = ((t1308) (sum + ((t1308) (((v1).data)[i] * ((v2).data)[i])))));
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
    template<typename t1316, int c97>
    t1316 magnitude2(juniper::records::recordt_3<juniper::array<t1316, c97>> v) {
        return (([&]() -> t1316 {
            using a = t1316;
            constexpr int32_t n = c97;
            return (([&]() -> t1316 {
                t1316 guid163 = ((t1316) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t1316 sum = guid163;
                
                (([&]() -> juniper::unit {
                    int32_t guid164 = ((int32_t) 0);
                    int32_t guid165 = n;
                    for (int32_t i = guid164; i < guid165; i++) {
                        (([&]() -> t1316 {
                            return (sum = ((t1316) (sum + ((t1316) (((v).data)[i] * ((v).data)[i])))));
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
    template<typename t1318, int c100>
    double magnitude(juniper::records::recordt_3<juniper::array<t1318, c100>> v) {
        return (([&]() -> double {
            using a = t1318;
            constexpr int32_t n = c100;
            return sqrt_(toDouble<t1318>(magnitude2<t1318, c100>(v)));
        })());
    }
}

namespace Vector {
    template<typename t1336, int c102>
    juniper::records::recordt_3<juniper::array<t1336, c102>> multiply(juniper::records::recordt_3<juniper::array<t1336, c102>> u, juniper::records::recordt_3<juniper::array<t1336, c102>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1336, c102>> {
            using a = t1336;
            constexpr int32_t n = c102;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1336, c102>> {
                juniper::records::recordt_3<juniper::array<t1336, c102>> guid166 = u;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1336, c102>> result = guid166;
                
                (([&]() -> juniper::unit {
                    int32_t guid167 = ((int32_t) 0);
                    int32_t guid168 = n;
                    for (int32_t i = guid167; i < guid168; i++) {
                        (([&]() -> t1336 {
                            return (((result).data)[i] = ((t1336) (((result).data)[i] * ((v).data)[i])));
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
    template<typename t1347, int c106>
    juniper::records::recordt_3<juniper::array<t1347, c106>> normalize(juniper::records::recordt_3<juniper::array<t1347, c106>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1347, c106>> {
            using a = t1347;
            constexpr int32_t n = c106;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1347, c106>> {
                double guid169 = magnitude<t1347, c106>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid169;
                
                return (((bool) (mag > ((t1347) 0))) ? 
                    (([&]() -> juniper::records::recordt_3<juniper::array<t1347, c106>> {
                        juniper::records::recordt_3<juniper::array<t1347, c106>> guid170 = v;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_3<juniper::array<t1347, c106>> result = guid170;
                        
                        (([&]() -> juniper::unit {
                            int32_t guid171 = ((int32_t) 0);
                            int32_t guid172 = n;
                            for (int32_t i = guid171; i < guid172; i++) {
                                (([&]() -> t1347 {
                                    return (((result).data)[i] = fromDouble<t1347>(((double) (toDouble<t1347>(((result).data)[i]) / mag))));
                                })());
                            }
                            return {};
                        })());
                        return result;
                    })())
                :
                    (([&]() -> juniper::records::recordt_3<juniper::array<t1347, c106>> {
                        return v;
                    })()));
            })());
        })());
    }
}

namespace Vector {
    template<typename t1360, int c110>
    double angle(juniper::records::recordt_3<juniper::array<t1360, c110>> v1, juniper::records::recordt_3<juniper::array<t1360, c110>> v2) {
        return (([&]() -> double {
            using a = t1360;
            constexpr int32_t n = c110;
            return acos_(((double) (toDouble<t1360>(dot<t1360, c110>(v1, v2)) / sqrt_(toDouble<t1360>(((t1360) (magnitude2<t1360, c110>(v1) * magnitude2<t1360, c110>(v2))))))));
        })());
    }
}

namespace Vector {
    template<typename t1414>
    juniper::records::recordt_3<juniper::array<t1414, 3>> cross(juniper::records::recordt_3<juniper::array<t1414, 3>> u, juniper::records::recordt_3<juniper::array<t1414, 3>> v) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1414, 3>> {
            using a = t1414;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1414, 3>>{
                juniper::records::recordt_3<juniper::array<t1414, 3>> guid173;
                guid173.data = (juniper::array<t1414, 3> { {((t1414) (((t1414) (((u).data)[((uint32_t) 1)] * ((v).data)[((uint32_t) 2)])) - ((t1414) (((u).data)[((uint32_t) 2)] * ((v).data)[((uint32_t) 1)])))), ((t1414) (((t1414) (((u).data)[((uint32_t) 2)] * ((v).data)[((uint32_t) 0)])) - ((t1414) (((u).data)[((uint32_t) 0)] * ((v).data)[((uint32_t) 2)])))), ((t1414) (((t1414) (((u).data)[((uint32_t) 0)] * ((v).data)[((uint32_t) 1)])) - ((t1414) (((u).data)[((uint32_t) 1)] * ((v).data)[((uint32_t) 0)]))))} });
                return guid173;
            })());
        })());
    }
}

namespace Vector {
    template<typename t1416, int c126>
    juniper::records::recordt_3<juniper::array<t1416, c126>> project(juniper::records::recordt_3<juniper::array<t1416, c126>> a, juniper::records::recordt_3<juniper::array<t1416, c126>> b) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1416, c126>> {
            using z = t1416;
            constexpr int32_t n = c126;
            return (([&]() -> juniper::records::recordt_3<juniper::array<t1416, c126>> {
                juniper::records::recordt_3<juniper::array<t1416, c126>> guid174 = normalize<t1416, c126>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_3<juniper::array<t1416, c126>> bn = guid174;
                
                return scale<t1416, c126>(dot<t1416, c126>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t1432, int c130>
    juniper::records::recordt_3<juniper::array<t1432, c130>> projectPlane(juniper::records::recordt_3<juniper::array<t1432, c130>> a, juniper::records::recordt_3<juniper::array<t1432, c130>> m) {
        return (([&]() -> juniper::records::recordt_3<juniper::array<t1432, c130>> {
            using z = t1432;
            constexpr int32_t n = c130;
            return subtract<t1432, c130>(a, project<t1432, c130>(a, m));
        })());
    }
}

namespace CharList {
    template<int c133>
    juniper::records::recordt_0<juniper::array<uint8_t, c133>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c133>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c133>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 97))) && ((bool) (c <= ((uint8_t) 122))))) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) (c - ((uint8_t) 32)));
                })())
            :
                (([&]() -> uint8_t {
                    return c;
                })()));
         }), str);
    }
}

namespace CharList {
    template<int c134>
    juniper::records::recordt_0<juniper::array<uint8_t, c134>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c134>, uint32_t> str) {
        return List::map<void, uint8_t, uint8_t, c134>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 65))) && ((bool) (c <= ((uint8_t) 90))))) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) (c + ((uint8_t) 32)));
                })())
            :
                (([&]() -> uint8_t {
                    return c;
                })()));
         }), str);
    }
}

namespace CharList {
    template<int c135>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> i32ToCharList(int32_t m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> {
            constexpr int32_t n = c135;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> guid175 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> guid176;
                    guid176.data = (juniper::array<uint8_t, (1)+(c135)>().fill(((uint8_t) 0)));
                    guid176.length = ((uint32_t) 0);
                    return guid176;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c135)>, uint32_t> ret = guid175;
                
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
    template<int c136>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c136>, uint32_t> s) {
        return (([&]() -> uint32_t {
            constexpr int32_t n = c136;
            return ((uint32_t) ((s).length - ((uint32_t) 1)));
        })());
    }
}

namespace CharList {
    template<int c137, int c138, int c139>
    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c137)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c138)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t> {
            constexpr int32_t aCap = c137;
            constexpr int32_t bCap = c138;
            constexpr int32_t retCap = c139;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t> {
                uint32_t guid177 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid177;
                
                uint32_t guid178 = length<(1)+(c137)>(sA);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenA = guid178;
                
                uint32_t guid179 = length<(1)+(c138)>(sB);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenB = guid179;
                
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t> guid180 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t> guid181;
                    guid181.data = (juniper::array<uint8_t, (1)+(c139)>().fill(((uint8_t) 0)));
                    guid181.length = ((uint32_t) (((uint32_t) (lenA + lenB)) + ((uint32_t) 1)));
                    return guid181;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c139)>, uint32_t> out = guid180;
                
                (([&]() -> juniper::unit {
                    uint32_t guid182 = ((uint32_t) 0);
                    uint32_t guid183 = lenA;
                    for (uint32_t i = guid182; i < guid183; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sA).data)[i]);
                            return (j = ((uint32_t) (j + ((uint32_t) 1))));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid184 = ((uint32_t) 0);
                    uint32_t guid185 = lenB;
                    for (uint32_t i = guid184; i < guid185; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sB).data)[i]);
                            return (j = ((uint32_t) (j + ((uint32_t) 1))));
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
    template<int c146, int c147>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c146)*(-1)))+((c147)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c146)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c147)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c146)*(-1)))+((c147)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t aCap = c146;
            constexpr int32_t bCap = c147;
            return concat<c146, c147, (-1)+((((-1)+((c146)*(-1)))+((c147)*(-1)))*(-1))>(sA, sB);
        })());
    }
}

namespace Random {
    int32_t random_(int32_t low, int32_t high) {
        return (([&]() -> int32_t {
            int32_t guid186 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t ret = guid186;
            
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
    template<typename t1502, int c151>
    t1502 choice(juniper::records::recordt_0<juniper::array<t1502, c151>, uint32_t> lst) {
        return (([&]() -> t1502 {
            using t = t1502;
            constexpr int32_t n = c151;
            return (([&]() -> t1502 {
                int32_t guid187 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid187;
                
                return List::nth<t1502, c151>(i32ToU32(i), lst);
            })());
        })());
    }
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_6<float, float, float> color) {
        return (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> {
            juniper::records::recordt_6<float, float, float> guid188 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float v = (guid188).v;
            float s = (guid188).s;
            float h = (guid188).h;
            
            float guid189 = ((float) (v * s));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float c = guid189;
            
            float guid190 = ((float) (c * toFloat<double>(((double) (1.0 - Math::fabs_(((double) (Math::fmod_(((double) (toDouble<float>(h) / ((double) 60))), 2.0) - 1.0))))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float x = guid190;
            
            float guid191 = ((float) (v - c));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float m = guid191;
            
            juniper::tuple3<float, float, float> guid192 = (((bool) (((bool) (0.0f <= h)) && ((bool) (h < 60.0f)))) ? 
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
            float bPrime = (guid192).e3;
            float gPrime = (guid192).e2;
            float rPrime = (guid192).e1;
            
            float guid193 = ((float) (((float) (rPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r = guid193;
            
            float guid194 = ((float) (((float) (gPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g = guid194;
            
            float guid195 = ((float) (((float) (bPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b = guid195;
            
            return (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
                juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid196;
                guid196.r = toUInt8<float>(r);
                guid196.g = toUInt8<float>(g);
                guid196.b = toUInt8<float>(b);
                return guid196;
            })());
        })());
    }
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> color) {
        return (([&]() -> uint16_t {
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid197 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = (guid197).b;
            uint8_t g = (guid197).g;
            uint8_t r = (guid197).r;
            
            return ((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (u8ToU16(r) & ((uint16_t) 248))) << ((uint32_t) 8))) | ((uint16_t) (((uint16_t) (u8ToU16(g) & ((uint16_t) 252))) << ((uint32_t) 3))))) | ((uint16_t) (u8ToU16(b) >> ((uint32_t) 3)))));
        })());
    }
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> red = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid198;
        guid198.r = ((uint8_t) 255);
        guid198.g = ((uint8_t) 0);
        guid198.b = ((uint8_t) 0);
        return guid198;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> green = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid199;
        guid199.r = ((uint8_t) 0);
        guid199.g = ((uint8_t) 255);
        guid199.b = ((uint8_t) 0);
        return guid199;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> blue = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid200;
        guid200.r = ((uint8_t) 0);
        guid200.g = ((uint8_t) 0);
        guid200.b = ((uint8_t) 255);
        return guid200;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> black = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid201;
        guid201.r = ((uint8_t) 0);
        guid201.g = ((uint8_t) 0);
        guid201.b = ((uint8_t) 0);
        return guid201;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> white = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid202;
        guid202.r = ((uint8_t) 255);
        guid202.g = ((uint8_t) 255);
        guid202.b = ((uint8_t) 255);
        return guid202;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> yellow = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid203;
        guid203.r = ((uint8_t) 255);
        guid203.g = ((uint8_t) 255);
        guid203.b = ((uint8_t) 0);
        return guid203;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> magenta = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid204;
        guid204.r = ((uint8_t) 255);
        guid204.g = ((uint8_t) 0);
        guid204.b = ((uint8_t) 255);
        return guid204;
    })());
}

namespace Color {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> cyan = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid205;
        guid205.r = ((uint8_t) 0);
        guid205.g = ((uint8_t) 255);
        guid205.b = ((uint8_t) 255);
        return guid205;
    })());
}

namespace Arcada {
    bool arcadaBegin() {
        return (([&]() -> bool {
            bool guid206 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid206;
            
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
            uint16_t guid207 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid207;
            
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
            uint16_t guid208 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid208;
            
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
            uint16_t guid209 = displayWidth();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid209;
            
            uint16_t guid210 = displayHeight();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid210;
            
            bool guid211 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid211;
            
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
            bool guid212 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid212;
            
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
            Ble::servicet guid213 = s;
            if (!(((bool) (((bool) ((guid213).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid213).service();
            
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
            Ble::characterstict guid214 = c;
            if (!(((bool) (((bool) ((guid214).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid214).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->begin();
                return {};
            })());
        })());
    }
}

namespace Ble {
    template<int c153>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c153>, uint32_t> bytes) {
        return (([&]() -> uint16_t {
            constexpr int32_t n = c153;
            return (([&]() -> uint16_t {
                juniper::records::recordt_0<juniper::array<uint8_t, c153>, uint32_t> guid215 = bytes;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t length = (guid215).length;
                juniper::array<uint8_t, c153> data = (guid215).data;
                
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
                ret = ((BLECharacteristic *) p)->write32((int) n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    template<typename t1852>
    uint16_t writeGeneric(Ble::characterstict c, t1852 x) {
        return (([&]() -> uint16_t {
            using t = t1852;
            return (([&]() -> uint16_t {
                Ble::characterstict guid226 = c;
                if (!(((bool) (((bool) ((guid226).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid226).characterstic();
                
                uint16_t guid227 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid227;
                
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
    template<typename t1857>
    t1857 readGeneric(Ble::characterstict c) {
        return (([&]() -> t1857 {
            using t = t1857;
            return (([&]() -> t1857 {
                Ble::characterstict guid228 = c;
                if (!(((bool) (((bool) ((guid228).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid228).characterstic();
                
                t1857 ret;
                
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
            Ble::secureModet guid229 = readPermission;
            if (!(((bool) (((bool) ((guid229).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t readN = (guid229).secureMode();
            
            Ble::secureModet guid230 = writePermission;
            if (!(((bool) (((bool) ((guid230).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t writeN = (guid230).secureMode();
            
            Ble::characterstict guid231 = c;
            if (!(((bool) (((bool) ((guid231).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid231).characterstic();
            
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
            Ble::propertiest guid232 = prop;
            if (!(((bool) (((bool) ((guid232).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t propN = (guid232).properties();
            
            Ble::characterstict guid233 = c;
            if (!(((bool) (((bool) ((guid233).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid233).characterstic();
            
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
            Ble::characterstict guid234 = c;
            if (!(((bool) (((bool) ((guid234).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid234).characterstic();
            
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
            Ble::advertisingFlagt guid235 = flag;
            if (!(((bool) (((bool) ((guid235).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t flagNum = (guid235).advertisingFlag();
            
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
            Ble::appearancet guid236 = app;
            if (!(((bool) (((bool) ((guid236).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t flagNum = (guid236).appearance();
            
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
            Ble::servicet guid237 = serv;
            if (!(((bool) (((bool) ((guid237).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid237).service();
            
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
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid238;
        guid238.r = ((uint8_t) 252);
        guid238.g = ((uint8_t) 92);
        guid238.b = ((uint8_t) 125);
        return guid238;
    })());
}

namespace CWatch {
    juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> purpleBlue = (([]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid239;
        guid239.r = ((uint8_t) 106);
        guid239.g = ((uint8_t) 130);
        guid239.b = ((uint8_t) 251);
        return guid239;
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
            CWatch::month guid240 = m;
            return (((bool) (((bool) ((guid240).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 31);
                })())
            :
                (((bool) (((bool) ((guid240).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return (isLeapYear(y) ? 
                            ((uint8_t) 29)
                        :
                            ((uint8_t) 28));
                    })())
                :
                    (((bool) (((bool) ((guid240).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 31);
                        })())
                    :
                        (((bool) (((bool) ((guid240).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> uint8_t {
                                return ((uint8_t) 30);
                            })())
                        :
                            (((bool) (((bool) ((guid240).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> uint8_t {
                                    return ((uint8_t) 31);
                                })())
                            :
                                (((bool) (((bool) ((guid240).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> uint8_t {
                                        return ((uint8_t) 30);
                                    })())
                                :
                                    (((bool) (((bool) ((guid240).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> uint8_t {
                                            return ((uint8_t) 31);
                                        })())
                                    :
                                        (((bool) (((bool) ((guid240).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> uint8_t {
                                                return ((uint8_t) 31);
                                            })())
                                        :
                                            (((bool) (((bool) ((guid240).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> uint8_t {
                                                    return ((uint8_t) 30);
                                                })())
                                            :
                                                (((bool) (((bool) ((guid240).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> uint8_t {
                                                        return ((uint8_t) 31);
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid240).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> uint8_t {
                                                            return ((uint8_t) 30);
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid240).id() == ((uint8_t) 11))) && true)) ? 
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
            CWatch::month guid241 = m;
            return (((bool) (((bool) ((guid241).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::month {
                    return february();
                })())
            :
                (((bool) (((bool) ((guid241).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::month {
                        return march();
                    })())
                :
                    (((bool) (((bool) ((guid241).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::month {
                            return april();
                        })())
                    :
                        (((bool) (((bool) ((guid241).id() == ((uint8_t) 4))) && true)) ? 
                            (([&]() -> CWatch::month {
                                return june();
                            })())
                        :
                            (((bool) (((bool) ((guid241).id() == ((uint8_t) 5))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return july();
                                })())
                            :
                                (((bool) (((bool) ((guid241).id() == ((uint8_t) 7))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return august();
                                    })())
                                :
                                    (((bool) (((bool) ((guid241).id() == ((uint8_t) 8))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return october();
                                        })())
                                    :
                                        (((bool) (((bool) ((guid241).id() == ((uint8_t) 9))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return november();
                                            })())
                                        :
                                            (((bool) (((bool) ((guid241).id() == ((uint8_t) 11))) && true)) ? 
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
            CWatch::dayOfWeek guid242 = dw;
            return (((bool) (((bool) ((guid242).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::dayOfWeek {
                    return monday();
                })())
            :
                (((bool) (((bool) ((guid242).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::dayOfWeek {
                        return tuesday();
                    })())
                :
                    (((bool) (((bool) ((guid242).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::dayOfWeek {
                            return wednesday();
                        })())
                    :
                        (((bool) (((bool) ((guid242).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> CWatch::dayOfWeek {
                                return thursday();
                            })())
                        :
                            (((bool) (((bool) ((guid242).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return friday();
                                })())
                            :
                                (((bool) (((bool) ((guid242).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return saturday();
                                    })())
                                :
                                    (((bool) (((bool) ((guid242).id() == ((uint8_t) 6))) && true)) ? 
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
            CWatch::dayOfWeek guid243 = dw;
            return (((bool) (((bool) ((guid243).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid244;
                        guid244.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid244.length = ((uint32_t) 4);
                        return guid244;
                    })());
                })())
            :
                (((bool) (((bool) ((guid243).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid245;
                            guid245.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 111), ((uint8_t) 110), ((uint8_t) 0)} });
                            guid245.length = ((uint32_t) 4);
                            return guid245;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid243).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid246;
                                guid246.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 117), ((uint8_t) 101), ((uint8_t) 0)} });
                                guid246.length = ((uint32_t) 4);
                                return guid246;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid243).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid247;
                                    guid247.data = (juniper::array<uint8_t, 4> { {((uint8_t) 87), ((uint8_t) 101), ((uint8_t) 100), ((uint8_t) 0)} });
                                    guid247.length = ((uint32_t) 4);
                                    return guid247;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid243).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid248;
                                        guid248.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 104), ((uint8_t) 117), ((uint8_t) 0)} });
                                        guid248.length = ((uint32_t) 4);
                                        return guid248;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid243).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid249;
                                            guid249.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 114), ((uint8_t) 105), ((uint8_t) 0)} });
                                            guid249.length = ((uint32_t) 4);
                                            return guid249;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid243).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid250;
                                                guid250.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 97), ((uint8_t) 116), ((uint8_t) 0)} });
                                                guid250.length = ((uint32_t) 4);
                                                return guid250;
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
            CWatch::month guid251 = m;
            return (((bool) (((bool) ((guid251).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid252;
                        guid252.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 97), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid252.length = ((uint32_t) 4);
                        return guid252;
                    })());
                })())
            :
                (((bool) (((bool) ((guid251).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid253;
                            guid253.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 101), ((uint8_t) 98), ((uint8_t) 0)} });
                            guid253.length = ((uint32_t) 4);
                            return guid253;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid251).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid254;
                                guid254.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 114), ((uint8_t) 0)} });
                                guid254.length = ((uint32_t) 4);
                                return guid254;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid251).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid255;
                                    guid255.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 112), ((uint8_t) 114), ((uint8_t) 0)} });
                                    guid255.length = ((uint32_t) 4);
                                    return guid255;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid251).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid256;
                                        guid256.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 121), ((uint8_t) 0)} });
                                        guid256.length = ((uint32_t) 4);
                                        return guid256;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid251).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid257;
                                            guid257.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                                            guid257.length = ((uint32_t) 4);
                                            return guid257;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid251).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid258;
                                                guid258.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 108), ((uint8_t) 0)} });
                                                guid258.length = ((uint32_t) 4);
                                                return guid258;
                                            })());
                                        })())
                                    :
                                        (((bool) (((bool) ((guid251).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid259;
                                                    guid259.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 117), ((uint8_t) 103), ((uint8_t) 0)} });
                                                    guid259.length = ((uint32_t) 4);
                                                    return guid259;
                                                })());
                                            })())
                                        :
                                            (((bool) (((bool) ((guid251).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid260;
                                                        guid260.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 101), ((uint8_t) 112), ((uint8_t) 0)} });
                                                        guid260.length = ((uint32_t) 4);
                                                        return guid260;
                                                    })());
                                                })())
                                            :
                                                (((bool) (((bool) ((guid251).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid261;
                                                            guid261.data = (juniper::array<uint8_t, 4> { {((uint8_t) 79), ((uint8_t) 99), ((uint8_t) 116), ((uint8_t) 0)} });
                                                            guid261.length = ((uint32_t) 4);
                                                            return guid261;
                                                        })());
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid251).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid262;
                                                                guid262.data = (juniper::array<uint8_t, 4> { {((uint8_t) 78), ((uint8_t) 111), ((uint8_t) 118), ((uint8_t) 0)} });
                                                                guid262.length = ((uint32_t) 4);
                                                                return guid262;
                                                            })());
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid251).id() == ((uint8_t) 11))) && true)) ? 
                                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid263;
                                                                    guid263.data = (juniper::array<uint8_t, 4> { {((uint8_t) 68), ((uint8_t) 101), ((uint8_t) 99), ((uint8_t) 0)} });
                                                                    guid263.length = ((uint32_t) 4);
                                                                    return guid263;
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
            juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid264 = d;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek = (guid264).dayOfWeek;
            uint8_t seconds = (guid264).seconds;
            uint8_t minutes = (guid264).minutes;
            uint8_t hours = (guid264).hours;
            uint32_t year = (guid264).year;
            uint8_t day = (guid264).day;
            CWatch::month month = (guid264).month;
            
            uint8_t guid265 = ((uint8_t) (seconds + ((uint8_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t seconds1 = guid265;
            
            juniper::tuple2<uint8_t, uint8_t> guid266 = (((bool) (seconds1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (minutes + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{seconds1, minutes}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t minutes1 = (guid266).e2;
            uint8_t seconds2 = (guid266).e1;
            
            juniper::tuple2<uint8_t, uint8_t> guid267 = (((bool) (minutes1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (hours + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{minutes1, hours}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t hours1 = (guid267).e2;
            uint8_t minutes2 = (guid267).e1;
            
            juniper::tuple3<uint8_t, uint8_t, CWatch::dayOfWeek> guid268 = (((bool) (hours1 == ((uint8_t) 24))) ? 
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{((uint8_t) 0), ((uint8_t) (day + ((uint8_t) 1))), nextDayOfWeek(dayOfWeek)})
            :
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{hours1, day, dayOfWeek}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek2 = (guid268).e3;
            uint8_t day1 = (guid268).e2;
            uint8_t hours2 = (guid268).e1;
            
            uint8_t guid269 = daysInMonth(year, month);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t daysInCurrentMonth = guid269;
            
            juniper::tuple2<uint8_t, juniper::tuple2<CWatch::month, uint32_t>> guid270 = (((bool) (day1 > daysInCurrentMonth)) ? 
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month, uint32_t>>{((uint8_t) 1), (([&]() -> juniper::tuple2<CWatch::month, uint32_t> {
                    CWatch::month guid271 = month;
                    return (((bool) (((bool) ((guid271).id() == ((uint8_t) 11))) && true)) ? 
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
            uint32_t year2 = ((guid270).e2).e2;
            CWatch::month month2 = ((guid270).e2).e1;
            uint8_t day2 = (guid270).e1;
            
            return (([&]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid272;
                guid272.month = month2;
                guid272.day = day2;
                guid272.year = year2;
                guid272.hours = hours2;
                guid272.minutes = minutes2;
                guid272.seconds = seconds2;
                guid272.dayOfWeek = dayOfWeek2;
                return guid272;
            })());
        })());
    }
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_1<uint32_t>> clockTickerState = Time::state();
}

namespace CWatch {
    juniper::shared_ptr<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>> clockState = (juniper::shared_ptr<juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>>(new juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>((([]() -> juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
        juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid273;
        guid273.month = september();
        guid273.day = ((uint8_t) 9);
        guid273.year = ((uint32_t) 2020);
        guid273.hours = ((uint8_t) 18);
        guid273.minutes = ((uint8_t) 40);
        guid273.seconds = ((uint8_t) 0);
        guid273.dayOfWeek = wednesday();
        return guid273;
    })()))));
}

namespace CWatch {
    juniper::unit processBluetoothUpdates() {
        return (([&]() -> juniper::unit {
            bool guid274 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayDateTime = guid274;
            
            bool guid275 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayOfWeek = guid275;
            
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
                        juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> guid276 = Ble::readGeneric<juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>>(dayDateTimeCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_8<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> bleData = guid276;
                        
                        (((clockState).get())->month = (([&]() -> CWatch::month {
                            uint8_t guid277 = (bleData).month;
                            return (((bool) (((bool) (guid277 == ((int32_t) 0))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return january();
                                })())
                            :
                                (((bool) (((bool) (guid277 == ((int32_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return february();
                                    })())
                                :
                                    (((bool) (((bool) (guid277 == ((int32_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return march();
                                        })())
                                    :
                                        (((bool) (((bool) (guid277 == ((int32_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return april();
                                            })())
                                        :
                                            (((bool) (((bool) (guid277 == ((int32_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::month {
                                                    return may();
                                                })())
                                            :
                                                (((bool) (((bool) (guid277 == ((int32_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::month {
                                                        return june();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid277 == ((int32_t) 6))) && true)) ? 
                                                        (([&]() -> CWatch::month {
                                                            return july();
                                                        })())
                                                    :
                                                        (((bool) (((bool) (guid277 == ((int32_t) 7))) && true)) ? 
                                                            (([&]() -> CWatch::month {
                                                                return august();
                                                            })())
                                                        :
                                                            (((bool) (((bool) (guid277 == ((int32_t) 8))) && true)) ? 
                                                                (([&]() -> CWatch::month {
                                                                    return september();
                                                                })())
                                                            :
                                                                (((bool) (((bool) (guid277 == ((int32_t) 9))) && true)) ? 
                                                                    (([&]() -> CWatch::month {
                                                                        return october();
                                                                    })())
                                                                :
                                                                    (((bool) (((bool) (guid277 == ((int32_t) 10))) && true)) ? 
                                                                        (([&]() -> CWatch::month {
                                                                            return november();
                                                                        })())
                                                                    :
                                                                        (((bool) (((bool) (guid277 == ((int32_t) 11))) && true)) ? 
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
                        juniper::records::recordt_9<uint8_t> guid278 = Ble::readGeneric<juniper::records::recordt_9<uint8_t>>(dayOfWeekCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_9<uint8_t> bleData = guid278;
                        
                        return (((clockState).get())->dayOfWeek = (([&]() -> CWatch::dayOfWeek {
                            uint8_t guid279 = (bleData).dayOfWeek;
                            return (((bool) (((bool) (guid279 == ((int32_t) 0))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return sunday();
                                })())
                            :
                                (((bool) (((bool) (guid279 == ((int32_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return monday();
                                    })())
                                :
                                    (((bool) (((bool) (guid279 == ((int32_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::dayOfWeek {
                                            return tuesday();
                                        })())
                                    :
                                        (((bool) (((bool) (guid279 == ((int32_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::dayOfWeek {
                                                return wednesday();
                                            })())
                                        :
                                            (((bool) (((bool) (guid279 == ((int32_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::dayOfWeek {
                                                    return thursday();
                                                })())
                                            :
                                                (((bool) (((bool) (guid279 == ((int32_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::dayOfWeek {
                                                        return friday();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid279 == ((int32_t) 6))) && true)) ? 
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
    juniper::unit drawVerticalGradient(int16_t x0i, int16_t y0i, int16_t w, int16_t h, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c1, juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> c2) {
        return (([&]() -> juniper::unit {
            int16_t guid280 = toInt16<uint16_t>(Arcada::displayWidth());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispW = guid280;
            
            int16_t guid281 = toInt16<uint16_t>(Arcada::displayHeight());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispH = guid281;
            
            int16_t guid282 = Math::clamp<int16_t>(x0i, ((int16_t) 0), ((int16_t) (dispW - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t x0 = guid282;
            
            int16_t guid283 = Math::clamp<int16_t>(y0i, ((int16_t) 0), ((int16_t) (dispH - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y0 = guid283;
            
            int16_t guid284 = ((int16_t) (y0i + h));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t ymax = guid284;
            
            int16_t guid285 = Math::clamp<int16_t>(ymax, ((int16_t) 0), dispH);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y1 = guid285;
            
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid286 = c1;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b1 = (guid286).b;
            uint8_t g1 = (guid286).g;
            uint8_t r1 = (guid286).r;
            
            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid287 = c2;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b2 = (guid287).b;
            uint8_t g2 = (guid287).g;
            uint8_t r2 = (guid287).r;
            
            float guid288 = toFloat<uint8_t>(r1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r1f = guid288;
            
            float guid289 = toFloat<uint8_t>(g1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g1f = guid289;
            
            float guid290 = toFloat<uint8_t>(b1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b1f = guid290;
            
            float guid291 = toFloat<uint8_t>(r2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r2f = guid291;
            
            float guid292 = toFloat<uint8_t>(g2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g2f = guid292;
            
            float guid293 = toFloat<uint8_t>(b2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b2f = guid293;
            
            return (([&]() -> juniper::unit {
                int16_t guid294 = y0;
                int16_t guid295 = y1;
                for (int16_t y = guid294; y < guid295; y++) {
                    (([&]() -> juniper::unit {
                        float guid296 = ((float) (toFloat<int16_t>(((int16_t) (ymax - y))) / toFloat<int16_t>(h)));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight1 = guid296;
                        
                        float guid297 = ((float) (1.0f - weight1));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight2 = guid297;
                        
                        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid298 = (([&]() -> juniper::records::recordt_4<uint8_t, uint8_t, uint8_t>{
                            juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> guid299;
                            guid299.r = toUInt8<float>(((float) (((float) (r1f * weight1)) + ((float) (r2f * weight2)))));
                            guid299.g = toUInt8<float>(((float) (((float) (g1f * weight1)) + ((float) (g2f * weight2)))));
                            guid299.b = toUInt8<float>(((float) (((float) (b1f * weight1)) + ((float) (g2f * weight2)))));
                            return guid299;
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_4<uint8_t, uint8_t, uint8_t> gradColor = guid298;
                        
                        uint16_t guid300 = Color::rgbToRgb565(gradColor);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint16_t gradColor565 = guid300;
                        
                        return drawFastHLine565(x0, y, w, gradColor565);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace Gfx {
    template<int c154>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c154)>, uint32_t> cl) {
        return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
            constexpr int32_t n = c154;
            return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
                int16_t guid301 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t xret = guid301;
                
                int16_t guid302 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t yret = guid302;
                
                uint16_t guid303 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t wret = guid303;
                
                uint16_t guid304 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t hret = guid304;
                
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
    template<int c155>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c155)>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c155;
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
            Gfx::font guid305 = f;
            return (((bool) (((bool) ((guid305).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::unit {
                    return (([&]() -> juniper::unit {
                        arcada.getCanvas()->setFont();
                        return {};
                    })());
                })())
            :
                (((bool) (((bool) ((guid305).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::unit {
                        return (([&]() -> juniper::unit {
                            arcada.getCanvas()->setFont(&FreeSans9pt7b);
                            return {};
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid305).id() == ((uint8_t) 2))) && true)) ? 
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
            uint16_t guid306 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid306;
            
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
                    juniper::records::recordt_10<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid307 = dt;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    CWatch::dayOfWeek dayOfWeek = (guid307).dayOfWeek;
                    uint8_t seconds = (guid307).seconds;
                    uint8_t minutes = (guid307).minutes;
                    uint8_t hours = (guid307).hours;
                    uint32_t year = (guid307).year;
                    uint8_t day = (guid307).day;
                    CWatch::month month = (guid307).month;
                    
                    int32_t guid308 = toInt32<uint8_t>((((bool) (hours == ((uint8_t) 0))) ? 
                        ((uint8_t) 12)
                    :
                        (((bool) (hours > ((uint8_t) 12))) ? 
                            ((uint8_t) (hours - ((uint8_t) 12)))
                        :
                            hours)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    int32_t displayHours = guid308;
                    
                    Gfx::setTextColor(Color::white);
                    Gfx::setFont(Gfx::freeSans24());
                    Gfx::setTextSize(((uint8_t) 1));
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid309 = CharList::i32ToCharList<2>(displayHours);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> timeHourStr = guid309;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid310 = CharList::safeConcat<2, 1>(timeHourStr, (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid311;
                        guid311.data = (juniper::array<uint8_t, 2> { {((uint8_t) 58), ((uint8_t) 0)} });
                        guid311.length = ((uint32_t) 2);
                        return guid311;
                    })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> timeHourStrColon = guid310;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid312 = (((bool) (minutes < ((uint8_t) 10))) ? 
                        CharList::safeConcat<1, 1>((([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid313;
                            guid313.data = (juniper::array<uint8_t, 2> { {((uint8_t) 48), ((uint8_t) 0)} });
                            guid313.length = ((uint32_t) 2);
                            return guid313;
                        })()), CharList::i32ToCharList<1>(toInt32<uint8_t>(minutes)))
                    :
                        CharList::i32ToCharList<2>(toInt32<uint8_t>(minutes)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> minutesStr = guid312;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> guid314 = CharList::safeConcat<3, 2>(timeHourStrColon, minutesStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> timeStr = guid314;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid315 = Gfx::getCharListBounds<5>(((int16_t) 0), ((int16_t) 0), timeStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h = (guid315).e4;
                    uint16_t w = (guid315).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2)))))));
                    Gfx::printCharList<5>(timeStr);
                    Gfx::setTextSize(((uint8_t) 1));
                    Gfx::setFont(Gfx::freeSans9());
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid316 = (((bool) (hours < ((uint8_t) 12))) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid317;
                            guid317.data = (juniper::array<uint8_t, 3> { {((uint8_t) 65), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid317.length = ((uint32_t) 3);
                            return guid317;
                        })())
                    :
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid318;
                            guid318.data = (juniper::array<uint8_t, 3> { {((uint8_t) 80), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid318.length = ((uint32_t) 3);
                            return guid318;
                        })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> ampm = guid316;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid319 = Gfx::getCharListBounds<2>(((int16_t) 0), ((int16_t) 0), ampm);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h2 = (guid319).e4;
                    uint16_t w2 = (guid319).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w2 / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2))))) + h2)) + ((uint16_t) 5)))));
                    Gfx::printCharList<2>(ampm);
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> guid320 = CharList::safeConcat<9, 2>(CharList::safeConcat<8, 1>(CharList::safeConcat<5, 3>(CharList::safeConcat<3, 2>(dayOfWeekToCharList(dayOfWeek), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid321;
                        guid321.data = (juniper::array<uint8_t, 3> { {((uint8_t) 44), ((uint8_t) 32), ((uint8_t) 0)} });
                        guid321.length = ((uint32_t) 3);
                        return guid321;
                    })())), monthToCharList(month)), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid322;
                        guid322.data = (juniper::array<uint8_t, 2> { {((uint8_t) 32), ((uint8_t) 0)} });
                        guid322.length = ((uint32_t) 2);
                        return guid322;
                    })())), CharList::i32ToCharList<2>(toInt32<uint8_t>(day)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> dateStr = guid320;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid323 = Gfx::getCharListBounds<11>(((int16_t) 0), ((int16_t) 0), dateStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h3 = (guid323).e4;
                    uint16_t w3 = (guid323).e3;
                    
                    Gfx::setCursor(cast<uint16_t, int16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w3 / ((uint16_t) 2)))))), cast<uint16_t, int16_t>(((uint16_t) (((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) - ((uint16_t) (h / ((uint16_t) 2))))) - ((uint16_t) 5)))));
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
            uint16_t guid324 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid324;
            
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
            uint16_t guid325 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid325;
            
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
    template<int c180>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c180>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c180;
            return (([&]() -> juniper::unit {
                juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid326 = Gfx::getCharListBounds<(-1)+(c180)>(((int16_t) 0), ((int16_t) 0), cl);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t h = (guid326).e4;
                uint16_t w = (guid326).e3;
                
                int16_t guid327 = toInt16<uint16_t>(w);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t ws = guid327;
                
                int16_t guid328 = toInt16<uint16_t>(h);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t hs = guid328;
                
                return (([&]() -> juniper::unit {
                    Gfx::align guid329 = align;
                    return (((bool) (((bool) ((guid329).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            return Gfx::setCursor(((int16_t) (x - ((int16_t) (ws / ((int16_t) 2))))), y);
                        })())
                    :
                        (((bool) (((bool) ((guid329).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::unit {
                                return Gfx::setCursor(x, ((int16_t) (y - ((int16_t) (hs / ((int16_t) 2))))));
                            })())
                        :
                            (((bool) (((bool) ((guid329).id() == ((uint8_t) 2))) && true)) ? 
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
