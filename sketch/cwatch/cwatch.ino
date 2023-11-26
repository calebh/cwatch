//Compiled on 11/25/2023 9:04:55 PM
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
    template<typename A>
    struct counted_cell {
        int ref_count;
        A data;

        counted_cell(A init_data)
            : ref_count(1), data(init_data) {}

        void destroy() {
            data.destroy();
        }
    };

    template <template <typename> class T, typename contained>
    class shared_ptr {
    private:
        counted_cell<T<contained>>* content;

        void inc_ref() {
            if (content != nullptr) {
                content->ref_count++;
            }
        }

        void dec_ref() {
            if (content != nullptr) {
                content->ref_count--;

                if (content->ref_count <= 0) {
                    content->destroy();
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

        shared_ptr(T<contained> init_data)
            : content(new counted_cell<T<contained>>(init_data))
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
                return content->data.get();
            }
            else {
                return nullptr;
            }
        }

        const contained* get() const {
            if (content != nullptr) {
                return content->data.get();
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

    // A basic container that doesn't do anything special to destroy
    // the contained data when the reference count drops to 0. This
    // is used for ref types.
    template <typename contained>
    class basic_container {
    private:
        contained data;

    public:
        basic_container(contained initData)
            : data(initData) {}

        void destroy() {}

        contained *get() { return &data;  }
    };

    // A container that calls a finalizer once the reference count
    // drops to 0. This is used for rcptr types.
    template <typename contained>
    class finalized_container {
    private:
        contained data;
        function<void, unit(void*)> finalizer;

    public:
        finalized_container(void* initData, function<void, unit(contained)> initFinalizer)
            : data(initData), finalizer(initFinalizer) {}

        void destroy() {
            finalizer(data);
        }

        contained *get() { return &data; }
    };

    template <typename contained>
    using refcell = shared_ptr<basic_container, contained>;

    using rcptr = shared_ptr<finalized_container, void*>;

    rcptr make_rcptr(void* initData, function<void, unit(void*)> finalizer) {
        return rcptr(finalized_container<void *>(initData, finalizer));
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
            }
            else {
                variant_helper_rec<n + 1, Ts...>::destroy(id, data);
            }
        }

        inline static void move(unsigned char id, void* from, void* to)
        {
            if (n == id) {
                // This static_cast and use of remove_reference is equivalent to the use of std::move
                new (to) F(static_cast<typename remove_reference<F>::type&&>(*reinterpret_cast<F*>(from)));
            }
            else {
                variant_helper_rec<n + 1, Ts...>::move(id, from, to);
            }
        }

        inline static void copy(unsigned char id, const void* from, void* to)
        {
            if (n == id) {
                new (to) F(*reinterpret_cast<const F*>(from));
            }
            else {
                variant_helper_rec<n + 1, Ts...>::copy(id, from, to);
            }
        }

        inline static bool equal(unsigned char id, void* lhs, void* rhs)
        {
            if (n == id) {
                return (*reinterpret_cast<F*>(lhs)) == (*reinterpret_cast<F*>(rhs));
            }
            else {
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
            }
            else {
                return quit<alternative<i>&>();
            }
        }

        ~variant() {
            helper_t::destroy(variant_id, &data);
        }

        bool operator==(variant& rhs) {
            if (variant_id == rhs.variant_id) {
                return helper_t::equal(variant_id, &data, &rhs.data);
            }
            else {
                return false;
            }
        }

        bool operator==(variant&& rhs) {
            if (variant_id == rhs.variant_id) {
                return helper_t::equal(variant_id, &data, &rhs.data);
            }
            else {
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

        bool operator==(tuple2<a, b> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2;
        }

        bool operator!=(tuple2<a, b> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c>
    struct tuple3 {
        a e1;
        b e2;
        c e3;

        tuple3(a initE1, b initE2, c initE3) : e1(initE1), e2(initE2), e3(initE3) {}

        bool operator==(tuple3<a, b, c> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3;
        }

        bool operator!=(tuple3<a, b, c> rhs) {
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

        bool operator==(tuple4<a, b, c, d> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4;
        }

        bool operator!=(tuple4<a, b, c, d> rhs) {
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

        bool operator==(tuple5<a, b, c, d, e> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5;
        }

        bool operator!=(tuple5<a, b, c, d, e> rhs) {
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

        bool operator==(tuple6<a, b, c, d, e, f> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6;
        }

        bool operator!=(tuple6<a, b, c, d, e, f> rhs) {
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

        bool operator==(tuple7<a, b, c, d, e, f, g> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7;
        }

        bool operator!=(tuple7<a, b, c, d, e, f, g> rhs) {
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

        bool operator==(tuple8<a, b, c, d, e, f, g, h> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8;
        }

        bool operator!=(tuple8<a, b, c, d, e, f, g, h> rhs) {
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

        bool operator==(tuple9<a, b, c, d, e, f, g, h, i> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8 && e9 == rhs.e9;
        }

        bool operator!=(tuple9<a, b, c, d, e, f, g, h, i> rhs) {
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

        bool operator==(tuple10<a, b, c, d, e, f, g, h, i, j> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8 && e9 == rhs.e9 && e10 == rhs.e10;
        }

        bool operator!=(tuple10<a, b, c, d, e, f, g, h, i, j> rhs) {
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
        struct closuret_5 {
            T1 pin;


            closuret_5(T1 init_pin) :
                pin(init_pin) {}
        };

        template<typename T1,typename T2>
        struct closuret_4 {
            T1 val1;
            T2 val2;


            closuret_4(T1 init_val1, T2 init_val2) :
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
    void * extractptr(juniper::rcptr p);
}

namespace Prelude {
    juniper::rcptr makerc(void * p, juniper::function<void, juniper::unit(void *)> finalizer);
}

namespace Prelude {
    template<typename t48, typename t49, typename t50, typename t51, typename t53>
    juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)> compose(juniper::function<t51, t49(t48)> f, juniper::function<t50, t48(t53)> g);
}

namespace Prelude {
    template<typename t61>
    t61 id(t61 x);
}

namespace Prelude {
    template<typename t68, typename t69, typename t72, typename t73>
    juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)> curry(juniper::function<t69, t68(t72, t73)> f);
}

namespace Prelude {
    template<typename t84, typename t85, typename t86, typename t88, typename t89>
    juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88, t89)> uncurry(juniper::function<t85, juniper::function<t86, t84(t89)>(t88)> f);
}

namespace Prelude {
    template<typename t104, typename t106, typename t109, typename t110, typename t111>
    juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)> curry3(juniper::function<t106, t104(t109, t110, t111)> f);
}

namespace Prelude {
    template<typename t125, typename t126, typename t127, typename t128, typename t130, typename t131, typename t132>
    juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130, t131, t132)> uncurry3(juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)> f);
}

namespace Prelude {
    template<typename t143>
    bool eq(t143 x, t143 y);
}

namespace Prelude {
    template<typename t148>
    bool neq(t148 x, t148 y);
}

namespace Prelude {
    template<typename t155, typename t156>
    bool gt(t156 x, t155 y);
}

namespace Prelude {
    template<typename t162, typename t163>
    bool geq(t163 x, t162 y);
}

namespace Prelude {
    template<typename t169, typename t170>
    bool lt(t170 x, t169 y);
}

namespace Prelude {
    template<typename t176, typename t177>
    bool leq(t177 x, t176 y);
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
    template<typename t200, typename t201, typename t202>
    t201 apply(juniper::function<t202, t201(t200)> f, t200 x);
}

namespace Prelude {
    template<typename t208, typename t209, typename t210, typename t211>
    t210 apply2(juniper::function<t211, t210(t208, t209)> f, juniper::tuple2<t208, t209> tup);
}

namespace Prelude {
    template<typename t222, typename t223, typename t224, typename t225, typename t226>
    t225 apply3(juniper::function<t226, t225(t222, t223, t224)> f, juniper::tuple3<t222, t223, t224> tup);
}

namespace Prelude {
    template<typename t240, typename t241, typename t242, typename t243, typename t244, typename t245>
    t244 apply4(juniper::function<t245, t244(t240, t241, t242, t243)> f, juniper::tuple4<t240, t241, t242, t243> tup);
}

namespace Prelude {
    template<typename t261, typename t262>
    t261 fst(juniper::tuple2<t261, t262> tup);
}

namespace Prelude {
    template<typename t266, typename t267>
    t267 snd(juniper::tuple2<t266, t267> tup);
}

namespace Prelude {
    template<typename t271>
    t271 add(t271 numA, t271 numB);
}

namespace Prelude {
    template<typename t273>
    t273 sub(t273 numA, t273 numB);
}

namespace Prelude {
    template<typename t275>
    t275 mul(t275 numA, t275 numB);
}

namespace Prelude {
    template<typename t277>
    t277 div(t277 numA, t277 numB);
}

namespace Prelude {
    template<typename t283, typename t284>
    juniper::tuple2<t284, t283> swap(juniper::tuple2<t283, t284> tup);
}

namespace Prelude {
    template<typename t290, typename t291, typename t292>
    t290 until(juniper::function<t292, bool(t290)> p, juniper::function<t291, t290(t290)> f, t290 a0);
}

namespace Prelude {
    template<typename t301>
    juniper::unit ignore(t301 val);
}

namespace Prelude {
    template<typename t304>
    juniper::unit clear(t304& val);
}

namespace Prelude {
    template<typename t306, int c4>
    juniper::array<t306, c4> array(t306 elem);
}

namespace Prelude {
    template<typename t312, int c6>
    juniper::array<t312, c6> zeros();
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
    template<typename t375>
    uint8_t toUInt8(t375 n);
}

namespace Prelude {
    template<typename t377>
    int8_t toInt8(t377 n);
}

namespace Prelude {
    template<typename t379>
    uint16_t toUInt16(t379 n);
}

namespace Prelude {
    template<typename t381>
    int16_t toInt16(t381 n);
}

namespace Prelude {
    template<typename t383>
    uint32_t toUInt32(t383 n);
}

namespace Prelude {
    template<typename t385>
    int32_t toInt32(t385 n);
}

namespace Prelude {
    template<typename t387>
    float toFloat(t387 n);
}

namespace Prelude {
    template<typename t389>
    double toDouble(t389 n);
}

namespace Prelude {
    template<typename t391>
    t391 fromUInt8(uint8_t n);
}

namespace Prelude {
    template<typename t393>
    t393 fromInt8(int8_t n);
}

namespace Prelude {
    template<typename t395>
    t395 fromUInt16(uint16_t n);
}

namespace Prelude {
    template<typename t397>
    t397 fromInt16(int16_t n);
}

namespace Prelude {
    template<typename t399>
    t399 fromUInt32(uint32_t n);
}

namespace Prelude {
    template<typename t401>
    t401 fromInt32(int32_t n);
}

namespace Prelude {
    template<typename t403>
    t403 fromFloat(float n);
}

namespace Prelude {
    template<typename t405>
    t405 fromDouble(double n);
}

namespace Prelude {
    template<typename t407, typename t408>
    t408 cast(t407 x);
}

namespace List {
    template<typename t411, int c7>
    juniper::records::recordt_0<juniper::array<t411, c7>, uint32_t> empty();
}

namespace List {
    template<typename t418, typename t419, typename t425, int c9>
    juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t> map(juniper::function<t419, t418(t425)> f, juniper::records::recordt_0<juniper::array<t425, c9>, uint32_t> lst);
}

namespace List {
    template<typename t433, typename t435, typename t438, int c13>
    t433 fold(juniper::function<t435, t433(t438, t433)> f, t433 initState, juniper::records::recordt_0<juniper::array<t438, c13>, uint32_t> lst);
}

namespace List {
    template<typename t445, typename t447, typename t453, int c15>
    t445 foldBack(juniper::function<t447, t445(t453, t445)> f, t445 initState, juniper::records::recordt_0<juniper::array<t453, c15>, uint32_t> lst);
}

namespace List {
    template<typename t461, typename t463, int c17>
    t463 reduce(juniper::function<t461, t463(t463, t463)> f, juniper::records::recordt_0<juniper::array<t463, c17>, uint32_t> lst);
}

namespace List {
    template<typename t474, typename t475, int c20>
    Prelude::maybe<t474> tryReduce(juniper::function<t475, t474(t474, t474)> f, juniper::records::recordt_0<juniper::array<t474, c20>, uint32_t> lst);
}

namespace List {
    template<typename t518, typename t522, int c22>
    t522 reduceBack(juniper::function<t518, t522(t522, t522)> f, juniper::records::recordt_0<juniper::array<t522, c22>, uint32_t> lst);
}

namespace List {
    template<typename t534, typename t535, int c25>
    Prelude::maybe<t534> tryReduceBack(juniper::function<t535, t534(t534, t534)> f, juniper::records::recordt_0<juniper::array<t534, c25>, uint32_t> lst);
}

namespace List {
    template<typename t590, int c27, int c28, int c29>
    juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t> concat(juniper::records::recordt_0<juniper::array<t590, c27>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t590, c28>, uint32_t> lstB);
}

namespace List {
    template<typename t595, int c35, int c36>
    juniper::records::recordt_0<juniper::array<t595, (((c35)*(-1))+((c36)*(-1)))*(-1)>, uint32_t> concatSafe(juniper::records::recordt_0<juniper::array<t595, c35>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t595, c36>, uint32_t> lstB);
}

namespace List {
    template<typename t603, typename t605, int c40>
    t605 get(t603 i, juniper::records::recordt_0<juniper::array<t605, c40>, uint32_t> lst);
}

namespace List {
    template<typename t608, typename t616, int c42>
    Prelude::maybe<t616> tryGet(t608 i, juniper::records::recordt_0<juniper::array<t616, c42>, uint32_t> lst);
}

namespace List {
    template<typename t649, int c44, int c45, int c46>
    juniper::records::recordt_0<juniper::array<t649, c46>, uint32_t> flatten(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t649, c44>, uint32_t>, c45>, uint32_t> listOfLists);
}

namespace List {
    template<typename t653, int c52, int c53>
    juniper::records::recordt_0<juniper::array<t653, (c53)*(c52)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t653, c52>, uint32_t>, c53>, uint32_t> listOfLists);
}

namespace Math {
    template<typename t659>
    t659 min_(t659 x, t659 y);
}

namespace List {
    template<typename t676, int c57, int c58>
    juniper::records::recordt_0<juniper::array<t676, c57>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t676, c58>, uint32_t> lst);
}

namespace List {
    template<typename t682, typename t685, int c62>
    bool all(juniper::function<t682, bool(t685)> pred, juniper::records::recordt_0<juniper::array<t685, c62>, uint32_t> lst);
}

namespace List {
    template<typename t692, typename t695, int c64>
    bool any(juniper::function<t692, bool(t695)> pred, juniper::records::recordt_0<juniper::array<t695, c64>, uint32_t> lst);
}

namespace List {
    template<typename t700, int c66>
    juniper::unit append(t700 elem, juniper::records::recordt_0<juniper::array<t700, c66>, uint32_t>& lst);
}

namespace List {
    template<typename t708, int c68>
    juniper::records::recordt_0<juniper::array<t708, c68>, uint32_t> appendPure(t708 elem, juniper::records::recordt_0<juniper::array<t708, c68>, uint32_t> lst);
}

namespace List {
    template<typename t716, int c70>
    juniper::records::recordt_0<juniper::array<t716, ((-1)+((c70)*(-1)))*(-1)>, uint32_t> appendSafe(t716 elem, juniper::records::recordt_0<juniper::array<t716, c70>, uint32_t> lst);
}

namespace List {
    template<typename t731, int c74>
    juniper::unit prepend(t731 elem, juniper::records::recordt_0<juniper::array<t731, c74>, uint32_t>& lst);
}

namespace List {
    template<typename t748, int c78>
    juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> prependPure(t748 elem, juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> lst);
}

namespace List {
    template<typename t763, int c82>
    juniper::unit set(uint32_t index, t763 elem, juniper::records::recordt_0<juniper::array<t763, c82>, uint32_t>& lst);
}

namespace List {
    template<typename t768, int c84>
    juniper::records::recordt_0<juniper::array<t768, c84>, uint32_t> setPure(uint32_t index, t768 elem, juniper::records::recordt_0<juniper::array<t768, c84>, uint32_t> lst);
}

namespace List {
    template<typename t774, int c86>
    juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t> replicate(uint32_t numOfElements, t774 elem);
}

namespace List {
    template<typename t795, int c89>
    juniper::unit remove(t795 elem, juniper::records::recordt_0<juniper::array<t795, c89>, uint32_t>& lst);
}

namespace List {
    template<typename t802, int c94>
    juniper::records::recordt_0<juniper::array<t802, c94>, uint32_t> removePure(t802 elem, juniper::records::recordt_0<juniper::array<t802, c94>, uint32_t> lst);
}

namespace List {
    template<typename t814, int c96>
    juniper::unit pop(juniper::records::recordt_0<juniper::array<t814, c96>, uint32_t>& lst);
}

namespace List {
    template<typename t821, int c98>
    juniper::records::recordt_0<juniper::array<t821, c98>, uint32_t> popPure(juniper::records::recordt_0<juniper::array<t821, c98>, uint32_t> lst);
}

namespace List {
    template<typename t829, typename t832, int c100>
    juniper::unit iter(juniper::function<t829, juniper::unit(t832)> f, juniper::records::recordt_0<juniper::array<t832, c100>, uint32_t> lst);
}

namespace List {
    template<typename t841, int c102>
    t841 last(juniper::records::recordt_0<juniper::array<t841, c102>, uint32_t> lst);
}

namespace List {
    template<typename t853, int c104>
    Prelude::maybe<t853> tryLast(juniper::records::recordt_0<juniper::array<t853, c104>, uint32_t> lst);
}

namespace List {
    template<typename t889, int c106>
    Prelude::maybe<t889> tryMax(juniper::records::recordt_0<juniper::array<t889, c106>, uint32_t> lst);
}

namespace List {
    template<typename t928, int c110>
    Prelude::maybe<t928> tryMin(juniper::records::recordt_0<juniper::array<t928, c110>, uint32_t> lst);
}

namespace List {
    template<typename t958, int c114>
    bool member(t958 elem, juniper::records::recordt_0<juniper::array<t958, c114>, uint32_t> lst);
}

namespace List {
    template<typename t976, typename t978, int c116>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t976, c116>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t978, c116>, uint32_t> lstB);
}

namespace List {
    template<typename t995, typename t996, int c121>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t>, juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t995, t996>, c121>, uint32_t> lst);
}

namespace List {
    template<typename t1004, int c127>
    t1004 sum(juniper::records::recordt_0<juniper::array<t1004, c127>, uint32_t> lst);
}

namespace List {
    template<typename t1022, int c129>
    t1022 average(juniper::records::recordt_0<juniper::array<t1022, c129>, uint32_t> lst);
}

namespace List {
    uint32_t iLeftChild(uint32_t i);
}

namespace List {
    uint32_t iRightChild(uint32_t i);
}

namespace List {
    uint32_t iParent(uint32_t i);
}

namespace List {
    template<typename t1047, typename t1049, typename t1051, typename t1082, int c131>
    juniper::unit siftDown(juniper::records::recordt_0<juniper::array<t1082, c131>, uint32_t>& lst, juniper::function<t1051, t1047(t1082)> key, uint32_t root, t1049 end);
}

namespace List {
    template<typename t1093, typename t1102, typename t1104, int c140>
    juniper::unit heapify(juniper::records::recordt_0<juniper::array<t1093, c140>, uint32_t>& lst, juniper::function<t1104, t1102(t1093)> key);
}

namespace List {
    template<typename t1115, typename t1117, typename t1130, int c142>
    juniper::unit sort(juniper::function<t1117, t1115(t1130)> key, juniper::records::recordt_0<juniper::array<t1130, c142>, uint32_t>& lst);
}

namespace List {
    template<typename t1148, typename t1149, typename t1150, int c149>
    juniper::records::recordt_0<juniper::array<t1149, c149>, uint32_t> sorted(juniper::function<t1150, t1148(t1149)> key, juniper::records::recordt_0<juniper::array<t1149, c149>, uint32_t> lst);
}

namespace Signal {
    template<typename t1160, typename t1162, typename t1177>
    Prelude::sig<t1177> map(juniper::function<t1162, t1177(t1160)> f, Prelude::sig<t1160> s);
}

namespace Signal {
    template<typename t1246, typename t1247>
    juniper::unit sink(juniper::function<t1247, juniper::unit(t1246)> f, Prelude::sig<t1246> s);
}

namespace Signal {
    template<typename t1270, typename t1284>
    Prelude::sig<t1284> filter(juniper::function<t1270, bool(t1284)> f, Prelude::sig<t1284> s);
}

namespace Signal {
    template<typename t1364>
    Prelude::sig<t1364> merge(Prelude::sig<t1364> sigA, Prelude::sig<t1364> sigB);
}

namespace Signal {
    template<typename t1400, int c151>
    Prelude::sig<t1400> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t1400>, c151>, uint32_t> sigs);
}

namespace Signal {
    template<typename t1518, typename t1542>
    Prelude::sig<Prelude::either<t1542, t1518>> join(Prelude::sig<t1542> sigA, Prelude::sig<t1518> sigB);
}

namespace Signal {
    template<typename t1831>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t1831> s);
}

namespace Signal {
    template<typename t1865, typename t1867, typename t1883>
    Prelude::sig<t1883> foldP(juniper::function<t1867, t1883(t1865, t1883)> f, t1883& state0, Prelude::sig<t1865> incoming);
}

namespace Signal {
    template<typename t1960>
    Prelude::sig<t1960> dropRepeats(Prelude::maybe<t1960>& maybePrevValue, Prelude::sig<t1960> incoming);
}

namespace Signal {
    template<typename t2059>
    Prelude::sig<t2059> latch(t2059& prevValue, Prelude::sig<t2059> incoming);
}

namespace Signal {
    template<typename t2119, typename t2120, typename t2123, typename t2131>
    Prelude::sig<t2119> map2(juniper::function<t2120, t2119(t2123, t2131)> f, juniper::tuple2<t2123, t2131>& state, Prelude::sig<t2123> incomingA, Prelude::sig<t2131> incomingB);
}

namespace Signal {
    template<typename t2273, int c153>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t2273, c153>, uint32_t>> record(juniper::records::recordt_0<juniper::array<t2273, c153>, uint32_t>& pastValues, Prelude::sig<t2273> incoming);
}

namespace Signal {
    template<typename t2311>
    Prelude::sig<t2311> constant(t2311 val);
}

namespace Signal {
    template<typename t2346>
    Prelude::sig<Prelude::maybe<t2346>> meta(Prelude::sig<t2346> sigA);
}

namespace Signal {
    template<typename t2413>
    Prelude::sig<t2413> unmeta(Prelude::sig<Prelude::maybe<t2413>> sigA);
}

namespace Signal {
    template<typename t2489, typename t2490>
    Prelude::sig<juniper::tuple2<t2489, t2490>> zip(juniper::tuple2<t2489, t2490>& state, Prelude::sig<t2489> sigA, Prelude::sig<t2490> sigB);
}

namespace Signal {
    template<typename t2560, typename t2567>
    juniper::tuple2<Prelude::sig<t2560>, Prelude::sig<t2567>> unzip(Prelude::sig<juniper::tuple2<t2560, t2567>> incoming);
}

namespace Signal {
    template<typename t2694, typename t2695>
    Prelude::sig<t2694> toggle(t2694 val1, t2694 val2, t2694& state, Prelude::sig<t2695> incoming);
}

namespace Io {
    Io::pinState toggle(Io::pinState p);
}

namespace Io {
    juniper::unit printStr(const char * str);
}

namespace Io {
    template<int c155>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c155>, uint32_t> cl);
}

namespace Io {
    juniper::unit printFloat(float f);
}

namespace Io {
    juniper::unit printInt(int32_t n);
}

namespace Io {
    template<typename t2750>
    t2750 baseToInt(Io::base b);
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
    Prelude::sig<juniper::unit> risingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState);
}

namespace Io {
    Prelude::sig<juniper::unit> fallingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState);
}

namespace Io {
    Prelude::sig<juniper::unit> edge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState);
}

namespace Maybe {
    template<typename t3204, typename t3206, typename t3215>
    Prelude::maybe<t3215> map(juniper::function<t3206, t3215(t3204)> f, Prelude::maybe<t3204> maybeVal);
}

namespace Maybe {
    template<typename t3245>
    t3245 get(Prelude::maybe<t3245> maybeVal);
}

namespace Maybe {
    template<typename t3254>
    bool isJust(Prelude::maybe<t3254> maybeVal);
}

namespace Maybe {
    template<typename t3265>
    bool isNothing(Prelude::maybe<t3265> maybeVal);
}

namespace Maybe {
    template<typename t3279>
    uint8_t count(Prelude::maybe<t3279> maybeVal);
}

namespace Maybe {
    template<typename t3293, typename t3294, typename t3295>
    t3293 fold(juniper::function<t3295, t3293(t3294, t3293)> f, t3293 initState, Prelude::maybe<t3294> maybeVal);
}

namespace Maybe {
    template<typename t3311, typename t3312>
    juniper::unit iter(juniper::function<t3312, juniper::unit(t3311)> f, Prelude::maybe<t3311> maybeVal);
}

namespace Time {
    juniper::unit wait(uint32_t time);
}

namespace Time {
    uint32_t now();
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::records::recordt_1<uint32_t>& state);
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
    template<typename t3456>
    t3456 abs_(t3456 x);
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
    template<typename t3467>
    t3467 max_(t3467 x, t3467 y);
}

namespace Math {
    template<typename t3469>
    t3469 mapRange(t3469 x, t3469 a1, t3469 a2, t3469 b1, t3469 b2);
}

namespace Math {
    template<typename t3471>
    t3471 clamp(t3471 x, t3471 min, t3471 max);
}

namespace Math {
    template<typename t3476>
    int8_t sign(t3476 n);
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState);
}

namespace Button {
    Prelude::sig<Io::pinState> debounce(Prelude::sig<Io::pinState> incoming, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState);
}

namespace Vector {
    template<typename t3596, int c156>
    juniper::array<t3596, c156> add(juniper::array<t3596, c156> v1, juniper::array<t3596, c156> v2);
}

namespace Vector {
    template<typename t3600, int c159>
    juniper::array<t3600, c159> zero();
}

namespace Vector {
    template<typename t3608, int c161>
    juniper::array<t3608, c161> subtract(juniper::array<t3608, c161> v1, juniper::array<t3608, c161> v2);
}

namespace Vector {
    template<typename t3612, int c164>
    juniper::array<t3612, c164> scale(t3612 scalar, juniper::array<t3612, c164> v);
}

namespace Vector {
    template<typename t3618, int c166>
    t3618 dot(juniper::array<t3618, c166> v1, juniper::array<t3618, c166> v2);
}

namespace Vector {
    template<typename t3624, int c169>
    t3624 magnitude2(juniper::array<t3624, c169> v);
}

namespace Vector {
    template<typename t3626, int c172>
    double magnitude(juniper::array<t3626, c172> v);
}

namespace Vector {
    template<typename t3642, int c174>
    juniper::array<t3642, c174> multiply(juniper::array<t3642, c174> u, juniper::array<t3642, c174> v);
}

namespace Vector {
    template<typename t3651, int c177>
    juniper::array<t3651, c177> normalize(juniper::array<t3651, c177> v);
}

namespace Vector {
    template<typename t3662, int c181>
    double angle(juniper::array<t3662, c181> v1, juniper::array<t3662, c181> v2);
}

namespace Vector {
    template<typename t3704>
    juniper::array<t3704, 3> cross(juniper::array<t3704, 3> u, juniper::array<t3704, 3> v);
}

namespace Vector {
    template<typename t3706, int c197>
    juniper::array<t3706, c197> project(juniper::array<t3706, c197> a, juniper::array<t3706, c197> b);
}

namespace Vector {
    template<typename t3722, int c201>
    juniper::array<t3722, c201> projectPlane(juniper::array<t3722, c201> a, juniper::array<t3722, c201> m);
}

namespace CharList {
    template<int c204>
    juniper::records::recordt_0<juniper::array<uint8_t, c204>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c204>, uint32_t> str);
}

namespace CharList {
    template<int c205>
    juniper::records::recordt_0<juniper::array<uint8_t, c205>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c205>, uint32_t> str);
}

namespace CharList {
    template<int c206>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t> i32ToCharList(int32_t m);
}

namespace CharList {
    template<int c208>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c208>, uint32_t> s);
}

namespace CharList {
    template<int c209, int c210, int c211>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c209)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c210)>, uint32_t> sB);
}

namespace CharList {
    template<int c219, int c220>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c219)*(-1)))+((c220)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c219)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c220)>, uint32_t> sB);
}

namespace Random {
    int32_t random_(int32_t low, int32_t high);
}

namespace Random {
    juniper::unit seed(uint32_t n);
}

namespace Random {
    template<typename t3800, int c224>
    t3800 choice(juniper::records::recordt_0<juniper::array<t3800, c224>, uint32_t> lst);
}

namespace Random {
    template<typename t3820, int c226>
    Prelude::maybe<t3820> tryChoice(juniper::records::recordt_0<juniper::array<t3820, c226>, uint32_t> lst);
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
    template<int c228>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c228>, uint32_t> bytes);
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
    template<typename t4201>
    uint16_t writeGeneric(Ble::characterstict c, t4201 x);
}

namespace Ble {
    template<typename t4206>
    t4206 readGeneric(Ble::characterstict c);
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
    juniper::rcptr test();
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
    template<int c229>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c229)>, uint32_t> cl);
}

namespace Gfx {
    template<int c230>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c230)>, uint32_t> cl);
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
    template<int c255>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c255>, uint32_t> cl);
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
    void * extractptr(juniper::rcptr p) {
        return (([&]() -> void * {
            void * ret;
            
            (([&]() -> juniper::unit {
                ret = *p.get();
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    juniper::rcptr makerc(void * p, juniper::function<void, juniper::unit(void *)> finalizer) {
        return (([&]() -> juniper::rcptr {
            juniper::rcptr ret;
            
            (([&]() -> juniper::unit {
                ret = juniper::make_rcptr(p, finalizer);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t48, typename t49, typename t50, typename t51, typename t53>
    juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)> compose(juniper::function<t51, t49(t48)> f, juniper::function<t50, t48(t53)> g) {
        return (([&]() -> juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)> {
            using a = t53;
            using b = t48;
            using c = t49;
            return juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)>(juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>(f, g), [](juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>& junclosure, t53 x) -> t49 { 
                juniper::function<t51, t49(t48)>& f = junclosure.f;
                juniper::function<t50, t48(t53)>& g = junclosure.g;
                return f(g(x));
             });
        })());
    }
}

namespace Prelude {
    template<typename t61>
    t61 id(t61 x) {
        return (([&]() -> t61 {
            using a = t61;
            return x;
        })());
    }
}

namespace Prelude {
    template<typename t68, typename t69, typename t72, typename t73>
    juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)> curry(juniper::function<t69, t68(t72, t73)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)> {
            using a = t72;
            using b = t73;
            using c = t68;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)>(juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>(f), [](juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>& junclosure, t72 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)> { 
                juniper::function<t69, t68(t72, t73)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>& junclosure, t73 valueB) -> t68 { 
                    juniper::function<t69, t68(t72, t73)>& f = junclosure.f;
                    t72& valueA = junclosure.valueA;
                    return f(valueA, valueB);
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t84, typename t85, typename t86, typename t88, typename t89>
    juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88, t89)> uncurry(juniper::function<t85, juniper::function<t86, t84(t89)>(t88)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88, t89)> {
            using a = t88;
            using b = t89;
            using c = t84;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88,t89)>(juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>(f), [](juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>& junclosure, t88 valueA, t89 valueB) -> t84 { 
                juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>& f = junclosure.f;
                return f(valueA)(valueB);
             });
        })());
    }
}

namespace Prelude {
    template<typename t104, typename t106, typename t109, typename t110, typename t111>
    juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)> curry3(juniper::function<t106, t104(t109, t110, t111)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)> {
            using a = t109;
            using b = t110;
            using c = t111;
            using d = t104;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)>(juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>(f), [](juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>& junclosure, t109 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)> { 
                juniper::function<t106, t104(t109, t110, t111)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>& junclosure, t110 valueB) -> juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)> { 
                    juniper::function<t106, t104(t109, t110, t111)>& f = junclosure.f;
                    t109& valueA = junclosure.valueA;
                    return juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>(f, valueA, valueB), [](juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>& junclosure, t111 valueC) -> t104 { 
                        juniper::function<t106, t104(t109, t110, t111)>& f = junclosure.f;
                        t109& valueA = junclosure.valueA;
                        t110& valueB = junclosure.valueB;
                        return f(valueA, valueB, valueC);
                     });
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t125, typename t126, typename t127, typename t128, typename t130, typename t131, typename t132>
    juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130, t131, t132)> uncurry3(juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130, t131, t132)> {
            using a = t130;
            using b = t131;
            using c = t132;
            using d = t125;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130,t131,t132)>(juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>(f), [](juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>& junclosure, t130 valueA, t131 valueB, t132 valueC) -> t125 { 
                juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>& f = junclosure.f;
                return f(valueA)(valueB)(valueC);
             });
        })());
    }
}

namespace Prelude {
    template<typename t143>
    bool eq(t143 x, t143 y) {
        return (([&]() -> bool {
            using a = t143;
            return ((bool) (x == y));
        })());
    }
}

namespace Prelude {
    template<typename t148>
    bool neq(t148 x, t148 y) {
        return ((bool) (x != y));
    }
}

namespace Prelude {
    template<typename t155, typename t156>
    bool gt(t156 x, t155 y) {
        return ((bool) (x > y));
    }
}

namespace Prelude {
    template<typename t162, typename t163>
    bool geq(t163 x, t162 y) {
        return ((bool) (x >= y));
    }
}

namespace Prelude {
    template<typename t169, typename t170>
    bool lt(t170 x, t169 y) {
        return ((bool) (x < y));
    }
}

namespace Prelude {
    template<typename t176, typename t177>
    bool leq(t177 x, t176 y) {
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
    template<typename t200, typename t201, typename t202>
    t201 apply(juniper::function<t202, t201(t200)> f, t200 x) {
        return (([&]() -> t201 {
            using a = t200;
            using b = t201;
            return f(x);
        })());
    }
}

namespace Prelude {
    template<typename t208, typename t209, typename t210, typename t211>
    t210 apply2(juniper::function<t211, t210(t208, t209)> f, juniper::tuple2<t208, t209> tup) {
        return (([&]() -> t210 {
            using a = t208;
            using b = t209;
            using c = t210;
            return (([&]() -> t210 {
                juniper::tuple2<t208, t209> guid0 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t209 b = (guid0).e2;
                t208 a = (guid0).e1;
                
                return f(a, b);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t222, typename t223, typename t224, typename t225, typename t226>
    t225 apply3(juniper::function<t226, t225(t222, t223, t224)> f, juniper::tuple3<t222, t223, t224> tup) {
        return (([&]() -> t225 {
            using a = t222;
            using b = t223;
            using c = t224;
            using d = t225;
            return (([&]() -> t225 {
                juniper::tuple3<t222, t223, t224> guid1 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t224 c = (guid1).e3;
                t223 b = (guid1).e2;
                t222 a = (guid1).e1;
                
                return f(a, b, c);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t240, typename t241, typename t242, typename t243, typename t244, typename t245>
    t244 apply4(juniper::function<t245, t244(t240, t241, t242, t243)> f, juniper::tuple4<t240, t241, t242, t243> tup) {
        return (([&]() -> t244 {
            using a = t240;
            using b = t241;
            using c = t242;
            using d = t243;
            using e = t244;
            return (([&]() -> t244 {
                juniper::tuple4<t240, t241, t242, t243> guid2 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t243 d = (guid2).e4;
                t242 c = (guid2).e3;
                t241 b = (guid2).e2;
                t240 a = (guid2).e1;
                
                return f(a, b, c, d);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t261, typename t262>
    t261 fst(juniper::tuple2<t261, t262> tup) {
        return (([&]() -> t261 {
            using a = t261;
            using b = t262;
            return (([&]() -> t261 {
                juniper::tuple2<t261, t262> guid3 = tup;
                return (true ? 
                    (([&]() -> t261 {
                        t261 x = (guid3).e1;
                        return x;
                    })())
                :
                    juniper::quit<t261>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t266, typename t267>
    t267 snd(juniper::tuple2<t266, t267> tup) {
        return (([&]() -> t267 {
            using a = t266;
            using b = t267;
            return (([&]() -> t267 {
                juniper::tuple2<t266, t267> guid4 = tup;
                return (true ? 
                    (([&]() -> t267 {
                        t267 x = (guid4).e2;
                        return x;
                    })())
                :
                    juniper::quit<t267>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t271>
    t271 add(t271 numA, t271 numB) {
        return (([&]() -> t271 {
            using a = t271;
            return ((t271) (numA + numB));
        })());
    }
}

namespace Prelude {
    template<typename t273>
    t273 sub(t273 numA, t273 numB) {
        return (([&]() -> t273 {
            using a = t273;
            return ((t273) (numA - numB));
        })());
    }
}

namespace Prelude {
    template<typename t275>
    t275 mul(t275 numA, t275 numB) {
        return (([&]() -> t275 {
            using a = t275;
            return ((t275) (numA * numB));
        })());
    }
}

namespace Prelude {
    template<typename t277>
    t277 div(t277 numA, t277 numB) {
        return (([&]() -> t277 {
            using a = t277;
            return ((t277) (numA / numB));
        })());
    }
}

namespace Prelude {
    template<typename t283, typename t284>
    juniper::tuple2<t284, t283> swap(juniper::tuple2<t283, t284> tup) {
        return (([&]() -> juniper::tuple2<t284, t283> {
            juniper::tuple2<t283, t284> guid5 = tup;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            t284 beta = (guid5).e2;
            t283 alpha = (guid5).e1;
            
            return (juniper::tuple2<t284,t283>{beta, alpha});
        })());
    }
}

namespace Prelude {
    template<typename t290, typename t291, typename t292>
    t290 until(juniper::function<t292, bool(t290)> p, juniper::function<t291, t290(t290)> f, t290 a0) {
        return (([&]() -> t290 {
            using a = t290;
            return (([&]() -> t290 {
                t290 guid6 = a0;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t290 a = guid6;
                
                (([&]() -> juniper::unit {
                    while (!(p(a))) {
                        (([&]() -> t290 {
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
    template<typename t301>
    juniper::unit ignore(t301 val) {
        return (([&]() -> juniper::unit {
            using a = t301;
            return juniper::unit();
        })());
    }
}

namespace Prelude {
    template<typename t304>
    juniper::unit clear(t304& val) {
        return (([&]() -> juniper::unit {
            using a = t304;
            return (([&]() -> juniper::unit {
                
    val.~a();
    memset(&val, 0, sizeof(val));
    
                return {};
            })());
        })());
    }
}

namespace Prelude {
    template<typename t306, int c4>
    juniper::array<t306, c4> array(t306 elem) {
        return (([&]() -> juniper::array<t306, c4> {
            using a = t306;
            constexpr int32_t n = c4;
            return (([&]() -> juniper::array<t306, c4> {
                juniper::array<t306, c4> ret;
                
                (([&]() -> juniper::unit {
                    int32_t guid7 = ((int32_t) 0);
                    int32_t guid8 = n;
                    for (int32_t i = guid7; i < guid8; i++) {
                        (([&]() -> t306 {
                            return ((ret)[i] = elem);
                        })());
                    }
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t312, int c6>
    juniper::array<t312, c6> zeros() {
        return (([&]() -> juniper::array<t312, c6> {
            using a = t312;
            constexpr int32_t n = c6;
            return (([&]() -> juniper::array<t312, c6> {
                juniper::array<t312, c6> ret;
                
                clear<juniper::array<t312, c6>>(ret);
                return ret;
            })());
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
    template<typename t375>
    uint8_t toUInt8(t375 n) {
        return (([&]() -> uint8_t {
            using t = t375;
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
    template<typename t377>
    int8_t toInt8(t377 n) {
        return (([&]() -> int8_t {
            using t = t377;
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
    template<typename t379>
    uint16_t toUInt16(t379 n) {
        return (([&]() -> uint16_t {
            using t = t379;
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
    template<typename t381>
    int16_t toInt16(t381 n) {
        return (([&]() -> int16_t {
            using t = t381;
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
    template<typename t383>
    uint32_t toUInt32(t383 n) {
        return (([&]() -> uint32_t {
            using t = t383;
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
    template<typename t385>
    int32_t toInt32(t385 n) {
        return (([&]() -> int32_t {
            using t = t385;
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
    template<typename t387>
    float toFloat(t387 n) {
        return (([&]() -> float {
            using t = t387;
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
    template<typename t389>
    double toDouble(t389 n) {
        return (([&]() -> double {
            using t = t389;
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
    template<typename t391>
    t391 fromUInt8(uint8_t n) {
        return (([&]() -> t391 {
            using t = t391;
            return (([&]() -> t391 {
                t391 ret;
                
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
    template<typename t393>
    t393 fromInt8(int8_t n) {
        return (([&]() -> t393 {
            using t = t393;
            return (([&]() -> t393 {
                t393 ret;
                
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
    template<typename t395>
    t395 fromUInt16(uint16_t n) {
        return (([&]() -> t395 {
            using t = t395;
            return (([&]() -> t395 {
                t395 ret;
                
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
    template<typename t397>
    t397 fromInt16(int16_t n) {
        return (([&]() -> t397 {
            using t = t397;
            return (([&]() -> t397 {
                t397 ret;
                
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
    template<typename t399>
    t399 fromUInt32(uint32_t n) {
        return (([&]() -> t399 {
            using t = t399;
            return (([&]() -> t399 {
                t399 ret;
                
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
    template<typename t401>
    t401 fromInt32(int32_t n) {
        return (([&]() -> t401 {
            using t = t401;
            return (([&]() -> t401 {
                t401 ret;
                
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
    template<typename t403>
    t403 fromFloat(float n) {
        return (([&]() -> t403 {
            using t = t403;
            return (([&]() -> t403 {
                t403 ret;
                
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
    template<typename t405>
    t405 fromDouble(double n) {
        return (([&]() -> t405 {
            using t = t405;
            return (([&]() -> t405 {
                t405 ret;
                
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
    template<typename t407, typename t408>
    t408 cast(t407 x) {
        return (([&]() -> t408 {
            using a = t407;
            using b = t408;
            return (([&]() -> t408 {
                t408 ret;
                
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
    template<typename t411, int c7>
    juniper::records::recordt_0<juniper::array<t411, c7>, uint32_t> empty() {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t411, c7>, uint32_t> {
            using a = t411;
            constexpr int32_t n = c7;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t411, c7>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t411, c7>, uint32_t> guid9;
                guid9.data = zeros<t411, c7>();
                guid9.length = ((uint32_t) 0);
                return guid9;
            })());
        })());
    }
}

namespace List {
    template<typename t418, typename t419, typename t425, int c9>
    juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t> map(juniper::function<t419, t418(t425)> f, juniper::records::recordt_0<juniper::array<t425, c9>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t> {
            using a = t425;
            using b = t418;
            constexpr int32_t n = c9;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t> {
                juniper::array<t418, c9> guid10 = zeros<t418, c9>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t418, c9> ret = guid10;
                
                (([&]() -> juniper::unit {
                    uint32_t guid11 = ((uint32_t) 0);
                    uint32_t guid12 = (lst).length;
                    for (uint32_t i = guid11; i < guid12; i++) {
                        (([&]() -> t418 {
                            return ((ret)[i] = f(((lst).data)[i]));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t418, c9>, uint32_t> guid13;
                    guid13.data = ret;
                    guid13.length = (lst).length;
                    return guid13;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t433, typename t435, typename t438, int c13>
    t433 fold(juniper::function<t435, t433(t438, t433)> f, t433 initState, juniper::records::recordt_0<juniper::array<t438, c13>, uint32_t> lst) {
        return (([&]() -> t433 {
            using state = t433;
            using t = t438;
            constexpr int32_t n = c13;
            return (([&]() -> t433 {
                t433 guid14 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t433 s = guid14;
                
                (([&]() -> juniper::unit {
                    uint32_t guid15 = ((uint32_t) 0);
                    uint32_t guid16 = (lst).length;
                    for (uint32_t i = guid15; i < guid16; i++) {
                        (([&]() -> t433 {
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
    template<typename t445, typename t447, typename t453, int c15>
    t445 foldBack(juniper::function<t447, t445(t453, t445)> f, t445 initState, juniper::records::recordt_0<juniper::array<t453, c15>, uint32_t> lst) {
        return (([&]() -> t445 {
            using state = t445;
            using t = t453;
            constexpr int32_t n = c15;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> t445 {
                    return initState;
                })())
            :
                (([&]() -> t445 {
                    t445 guid17 = initState;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t445 s = guid17;
                    
                    uint32_t guid18 = (lst).length;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint32_t i = guid18;
                    
                    (([&]() -> juniper::unit {
                        do {
                            (([&]() -> t445 {
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
    template<typename t461, typename t463, int c17>
    t463 reduce(juniper::function<t461, t463(t463, t463)> f, juniper::records::recordt_0<juniper::array<t463, c17>, uint32_t> lst) {
        return (([&]() -> t463 {
            using t = t463;
            constexpr int32_t n = c17;
            return (([&]() -> t463 {
                t463 guid19 = ((lst).data)[((uint32_t) 0)];
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t463 s = guid19;
                
                (([&]() -> juniper::unit {
                    uint32_t guid20 = ((uint32_t) 1);
                    uint32_t guid21 = (lst).length;
                    for (uint32_t i = guid20; i < guid21; i++) {
                        (([&]() -> t463 {
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
    template<typename t474, typename t475, int c20>
    Prelude::maybe<t474> tryReduce(juniper::function<t475, t474(t474, t474)> f, juniper::records::recordt_0<juniper::array<t474, c20>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t474> {
            using t = t474;
            constexpr int32_t n = c20;
            return (([&]() -> Prelude::maybe<t474> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t474> {
                        return nothing<t474>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t474> {
                        return just<t474>(reduce<t475, t474, c20>(f, lst));
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t518, typename t522, int c22>
    t522 reduceBack(juniper::function<t518, t522(t522, t522)> f, juniper::records::recordt_0<juniper::array<t522, c22>, uint32_t> lst) {
        return (([&]() -> t522 {
            using t = t522;
            constexpr int32_t n = c22;
            return (([&]() -> t522 {
                t522 guid22 = ((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))];
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t522 s = guid22;
                
                uint32_t guid23 = ((uint32_t) ((lst).length - ((uint32_t) 1)));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t i = guid23;
                
                (([&]() -> juniper::unit {
                    do {
                        (([&]() -> t522 {
                            (i = ((uint32_t) (i - ((uint32_t) 1))));
                            return (s = f(((lst).data)[i], s));
                        })());
                    } while(((bool) (i > ((uint32_t) 0))));
                    return {};
                })());
                return s;
            })());
        })());
    }
}

namespace List {
    template<typename t534, typename t535, int c25>
    Prelude::maybe<t534> tryReduceBack(juniper::function<t535, t534(t534, t534)> f, juniper::records::recordt_0<juniper::array<t534, c25>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t534> {
            using t = t534;
            constexpr int32_t n = c25;
            return (([&]() -> Prelude::maybe<t534> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t534> {
                        return nothing<t534>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t534> {
                        return just<t534>(reduceBack<t535, t534, c25>(f, lst));
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t590, int c27, int c28, int c29>
    juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t> concat(juniper::records::recordt_0<juniper::array<t590, c27>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t590, c28>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t> {
            using t = t590;
            constexpr int32_t aCap = c27;
            constexpr int32_t bCap = c28;
            constexpr int32_t retCap = c29;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t> {
                uint32_t guid24 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid24;
                
                juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t> guid25 = (([&]() -> juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t> guid26;
                    guid26.data = zeros<t590, c29>();
                    guid26.length = ((uint32_t) ((lstA).length + (lstB).length));
                    return guid26;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t590, c29>, uint32_t> out = guid25;
                
                (([&]() -> juniper::unit {
                    uint32_t guid27 = ((uint32_t) 0);
                    uint32_t guid28 = (lstA).length;
                    for (uint32_t i = guid27; i < guid28; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((lstA).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid29 = ((uint32_t) 0);
                    uint32_t guid30 = (lstB).length;
                    for (uint32_t i = guid29; i < guid30; i++) {
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
    template<typename t595, int c35, int c36>
    juniper::records::recordt_0<juniper::array<t595, (((c35)*(-1))+((c36)*(-1)))*(-1)>, uint32_t> concatSafe(juniper::records::recordt_0<juniper::array<t595, c35>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t595, c36>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t595, (((c35)*(-1))+((c36)*(-1)))*(-1)>, uint32_t> {
            using t = t595;
            constexpr int32_t aCap = c35;
            constexpr int32_t bCap = c36;
            return concat<t595, c35, c36, (((c35)*(-1))+((c36)*(-1)))*(-1)>(lstA, lstB);
        })());
    }
}

namespace List {
    template<typename t603, typename t605, int c40>
    t605 get(t603 i, juniper::records::recordt_0<juniper::array<t605, c40>, uint32_t> lst) {
        return (([&]() -> t605 {
            using t = t605;
            using u = t603;
            constexpr int32_t n = c40;
            return ((lst).data)[i];
        })());
    }
}

namespace List {
    template<typename t608, typename t616, int c42>
    Prelude::maybe<t616> tryGet(t608 i, juniper::records::recordt_0<juniper::array<t616, c42>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t616> {
            using t = t616;
            using u = t608;
            constexpr int32_t n = c42;
            return (((bool) (i < (lst).length)) ? 
                just<t616>(((lst).data)[i])
            :
                nothing<t616>());
        })());
    }
}

namespace List {
    template<typename t649, int c44, int c45, int c46>
    juniper::records::recordt_0<juniper::array<t649, c46>, uint32_t> flatten(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t649, c44>, uint32_t>, c45>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t649, c46>, uint32_t> {
            using t = t649;
            constexpr int32_t m = c44;
            constexpr int32_t n = c45;
            constexpr int32_t retCap = c46;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t649, c46>, uint32_t> {
                juniper::array<t649, c46> guid31 = zeros<t649, c46>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t649, c46> ret = guid31;
                
                uint32_t guid32 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid32;
                
                (([&]() -> juniper::unit {
                    uint32_t guid33 = ((uint32_t) 0);
                    uint32_t guid34 = (listOfLists).length;
                    for (uint32_t i = guid33; i < guid34; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                uint32_t guid35 = ((uint32_t) 0);
                                uint32_t guid36 = (((listOfLists).data)[i]).length;
                                for (uint32_t j = guid35; j < guid36; j++) {
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
                return (([&]() -> juniper::records::recordt_0<juniper::array<t649, c46>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t649, c46>, uint32_t> guid37;
                    guid37.data = ret;
                    guid37.length = index;
                    return guid37;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t653, int c52, int c53>
    juniper::records::recordt_0<juniper::array<t653, (c53)*(c52)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t653, c52>, uint32_t>, c53>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t653, (c53)*(c52)>, uint32_t> {
            using t = t653;
            constexpr int32_t m = c52;
            constexpr int32_t n = c53;
            return flatten<t653, c52, c53, (c53)*(c52)>(listOfLists);
        })());
    }
}

namespace Math {
    template<typename t659>
    t659 min_(t659 x, t659 y) {
        return (([&]() -> t659 {
            using a = t659;
            return (((bool) (x > y)) ? 
                (([&]() -> t659 {
                    return y;
                })())
            :
                (([&]() -> t659 {
                    return x;
                })()));
        })());
    }
}

namespace List {
    template<typename t676, int c57, int c58>
    juniper::records::recordt_0<juniper::array<t676, c57>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t676, c58>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t676, c57>, uint32_t> {
            using t = t676;
            constexpr int32_t m = c57;
            constexpr int32_t n = c58;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t676, c57>, uint32_t> {
                juniper::array<t676, c57> guid38 = zeros<t676, c57>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t676, c57> ret = guid38;
                
                (([&]() -> juniper::unit {
                    uint32_t guid39 = ((uint32_t) 0);
                    uint32_t guid40 = Math::min_<uint32_t>((lst).length, toUInt32<int32_t>(m));
                    for (uint32_t i = guid39; i < guid40; i++) {
                        (([&]() -> t676 {
                            return ((ret)[i] = ((lst).data)[i]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t676, c57>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t676, c57>, uint32_t> guid41;
                    guid41.data = ret;
                    guid41.length = (lst).length;
                    return guid41;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t682, typename t685, int c62>
    bool all(juniper::function<t682, bool(t685)> pred, juniper::records::recordt_0<juniper::array<t685, c62>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t685;
            constexpr int32_t n = c62;
            return (([&]() -> bool {
                bool guid42 = true;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid42;
                
                (([&]() -> juniper::unit {
                    uint32_t guid43 = ((uint32_t) 0);
                    uint32_t guid44 = (lst).length;
                    for (uint32_t i = guid43; i < guid44; i++) {
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
    template<typename t692, typename t695, int c64>
    bool any(juniper::function<t692, bool(t695)> pred, juniper::records::recordt_0<juniper::array<t695, c64>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t695;
            constexpr int32_t n = c64;
            return (([&]() -> bool {
                bool guid45 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid45;
                
                (([&]() -> juniper::unit {
                    uint32_t guid46 = ((uint32_t) 0);
                    uint32_t guid47 = (lst).length;
                    for (uint32_t i = guid46; i < guid47; i++) {
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
    template<typename t700, int c66>
    juniper::unit append(t700 elem, juniper::records::recordt_0<juniper::array<t700, c66>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t700;
            constexpr int32_t n = c66;
            return (([&]() -> juniper::unit {
                if (((bool) ((lst).length < n))) {
                    (([&]() -> uint32_t {
                        (((lst).data)[(lst).length] = elem);
                        return ((lst).length += ((uint32_t) 1));
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t708, int c68>
    juniper::records::recordt_0<juniper::array<t708, c68>, uint32_t> appendPure(t708 elem, juniper::records::recordt_0<juniper::array<t708, c68>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t708, c68>, uint32_t> {
            using t = t708;
            constexpr int32_t n = c68;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t708, c68>, uint32_t> {
                (([&]() -> juniper::unit {
                    if (((bool) ((lst).length < n))) {
                        (([&]() -> uint32_t {
                            (((lst).data)[(lst).length] = elem);
                            return ((lst).length += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t716, int c70>
    juniper::records::recordt_0<juniper::array<t716, ((-1)+((c70)*(-1)))*(-1)>, uint32_t> appendSafe(t716 elem, juniper::records::recordt_0<juniper::array<t716, c70>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t716, ((-1)+((c70)*(-1)))*(-1)>, uint32_t> {
            using t = t716;
            constexpr int32_t n = c70;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t716, ((-1)+((c70)*(-1)))*(-1)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t716, ((-1)+((c70)*(-1)))*(-1)>, uint32_t> guid48 = resize<t716, ((-1)+((c70)*(-1)))*(-1), c70>(lst);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t716, ((-1)+((c70)*(-1)))*(-1)>, uint32_t> ret = guid48;
                
                (((ret).data)[(lst).length] = elem);
                ((ret).length += ((uint32_t) 1));
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t731, int c74>
    juniper::unit prepend(t731 elem, juniper::records::recordt_0<juniper::array<t731, c74>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t731;
            constexpr int32_t n = c74;
            return (([&]() -> juniper::unit {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::unit {
                        return juniper::unit();
                    })())
                :
                    (([&]() -> juniper::unit {
                        uint32_t guid49 = (lst).length;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint32_t i = guid49;
                        
                        (([&]() -> juniper::unit {
                            do {
                                (([&]() -> uint32_t {
                                    (([&]() -> juniper::unit {
                                        if (((bool) (i < n))) {
                                            (([&]() -> t731 {
                                                return (((lst).data)[i] = ((lst).data)[((uint32_t) (i - ((uint32_t) 1)))]);
                                            })());
                                        }
                                        return {};
                                    })());
                                    return (i -= ((uint32_t) 1));
                                })());
                            } while(((bool) (i > ((uint32_t) 0))));
                            return {};
                        })());
                        (((lst).data)[((uint32_t) 0)] = elem);
                        return (([&]() -> juniper::unit {
                            if (((bool) ((lst).length < n))) {
                                (([&]() -> uint32_t {
                                    return ((lst).length += ((uint32_t) 1));
                                })());
                            }
                            return {};
                        })());
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t748, int c78>
    juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> prependPure(t748 elem, juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> {
            using t = t748;
            constexpr int32_t n = c78;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> {
                        return lst;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t748, c78>, uint32_t> ret;
                        
                        (((ret).data)[((uint32_t) 0)] = elem);
                        (([&]() -> juniper::unit {
                            uint32_t guid50 = ((uint32_t) 0);
                            uint32_t guid51 = (lst).length;
                            for (uint32_t i = guid50; i < guid51; i++) {
                                (([&]() -> juniper::unit {
                                    return (([&]() -> juniper::unit {
                                        if (((bool) (((uint32_t) (i + ((uint32_t) 1))) < n))) {
                                            (([&]() -> t748 {
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
    template<typename t763, int c82>
    juniper::unit set(uint32_t index, t763 elem, juniper::records::recordt_0<juniper::array<t763, c82>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t763;
            constexpr int32_t n = c82;
            return (([&]() -> juniper::unit {
                if (((bool) (index < (lst).length))) {
                    (([&]() -> t763 {
                        return (((lst).data)[index] = elem);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t768, int c84>
    juniper::records::recordt_0<juniper::array<t768, c84>, uint32_t> setPure(uint32_t index, t768 elem, juniper::records::recordt_0<juniper::array<t768, c84>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t768, c84>, uint32_t> {
            using t = t768;
            constexpr int32_t n = c84;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t768, c84>, uint32_t> {
                (([&]() -> juniper::unit {
                    if (((bool) (index < (lst).length))) {
                        (([&]() -> t768 {
                            return (((lst).data)[index] = elem);
                        })());
                    }
                    return {};
                })());
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t774, int c86>
    juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t> replicate(uint32_t numOfElements, t774 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t> {
            using t = t774;
            constexpr int32_t n = c86;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t> guid52 = (([&]() -> juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t> guid53;
                    guid53.data = zeros<t774, c86>();
                    guid53.length = numOfElements;
                    return guid53;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t774, c86>, uint32_t> ret = guid52;
                
                (([&]() -> juniper::unit {
                    uint32_t guid54 = ((uint32_t) 0);
                    uint32_t guid55 = numOfElements;
                    for (uint32_t i = guid54; i < guid55; i++) {
                        (([&]() -> t774 {
                            return (((ret).data)[i] = elem);
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
    template<typename t795, int c89>
    juniper::unit remove(t795 elem, juniper::records::recordt_0<juniper::array<t795, c89>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t795;
            constexpr int32_t n = c89;
            return (([&]() -> juniper::unit {
                uint32_t guid56 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid56;
                
                bool guid57 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid57;
                
                (([&]() -> juniper::unit {
                    uint32_t guid58 = ((uint32_t) 0);
                    uint32_t guid59 = (lst).length;
                    for (uint32_t i = guid58; i < guid59; i++) {
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
                return (([&]() -> juniper::unit {
                    if (found) {
                        (([&]() -> juniper::unit {
                            ((lst).length -= ((uint32_t) 1));
                            (([&]() -> juniper::unit {
                                uint32_t guid60 = index;
                                uint32_t guid61 = (lst).length;
                                for (uint32_t i = guid60; i < guid61; i++) {
                                    (([&]() -> t795 {
                                        return (((lst).data)[i] = ((lst).data)[((uint32_t) (i + ((uint32_t) 1)))]);
                                    })());
                                }
                                return {};
                            })());
                            return clear<t795>(((lst).data)[(lst).length]);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t802, int c94>
    juniper::records::recordt_0<juniper::array<t802, c94>, uint32_t> removePure(t802 elem, juniper::records::recordt_0<juniper::array<t802, c94>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t802, c94>, uint32_t> {
            using t = t802;
            constexpr int32_t n = c94;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t802, c94>, uint32_t> {
                remove<t802, c94>(elem, lst);
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t814, int c96>
    juniper::unit pop(juniper::records::recordt_0<juniper::array<t814, c96>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t814;
            constexpr int32_t n = c96;
            return (([&]() -> juniper::unit {
                if (((bool) ((lst).length > ((uint32_t) 0)))) {
                    (([&]() -> juniper::unit {
                        ((lst).length -= ((uint32_t) 1));
                        return clear<t814>(((lst).data)[(lst).length]);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t821, int c98>
    juniper::records::recordt_0<juniper::array<t821, c98>, uint32_t> popPure(juniper::records::recordt_0<juniper::array<t821, c98>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t821, c98>, uint32_t> {
            using t = t821;
            constexpr int32_t n = c98;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t821, c98>, uint32_t> {
                pop<t821, c98>(lst);
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t829, typename t832, int c100>
    juniper::unit iter(juniper::function<t829, juniper::unit(t832)> f, juniper::records::recordt_0<juniper::array<t832, c100>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            using t = t832;
            constexpr int32_t n = c100;
            return (([&]() -> juniper::unit {
                uint32_t guid62 = ((uint32_t) 0);
                uint32_t guid63 = (lst).length;
                for (uint32_t i = guid62; i < guid63; i++) {
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
    template<typename t841, int c102>
    t841 last(juniper::records::recordt_0<juniper::array<t841, c102>, uint32_t> lst) {
        return (([&]() -> t841 {
            using t = t841;
            constexpr int32_t n = c102;
            return ((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))];
        })());
    }
}

namespace List {
    template<typename t853, int c104>
    Prelude::maybe<t853> tryLast(juniper::records::recordt_0<juniper::array<t853, c104>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t853> {
            using t = t853;
            constexpr int32_t n = c104;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t853> {
                    return nothing<t853>();
                })())
            :
                (([&]() -> Prelude::maybe<t853> {
                    return just<t853>(((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))]);
                })()));
        })());
    }
}

namespace List {
    template<typename t889, int c106>
    Prelude::maybe<t889> tryMax(juniper::records::recordt_0<juniper::array<t889, c106>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t889> {
            using t = t889;
            constexpr int32_t n = c106;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t889> {
                    return nothing<t889>();
                })())
            :
                (([&]() -> Prelude::maybe<t889> {
                    t889 guid64 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t889 maxVal = guid64;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid65 = ((uint32_t) 1);
                        uint32_t guid66 = (lst).length;
                        for (uint32_t i = guid65; i < guid66; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] > maxVal))) {
                                        (([&]() -> t889 {
                                            return (maxVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t889>(maxVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t928, int c110>
    Prelude::maybe<t928> tryMin(juniper::records::recordt_0<juniper::array<t928, c110>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t928> {
            using t = t928;
            constexpr int32_t n = c110;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t928> {
                    return nothing<t928>();
                })())
            :
                (([&]() -> Prelude::maybe<t928> {
                    t928 guid67 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t928 minVal = guid67;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid68 = ((uint32_t) 1);
                        uint32_t guid69 = (lst).length;
                        for (uint32_t i = guid68; i < guid69; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] < minVal))) {
                                        (([&]() -> t928 {
                                            return (minVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t928>(minVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t958, int c114>
    bool member(t958 elem, juniper::records::recordt_0<juniper::array<t958, c114>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t958;
            constexpr int32_t n = c114;
            return (([&]() -> bool {
                bool guid70 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid70;
                
                (([&]() -> juniper::unit {
                    uint32_t guid71 = ((uint32_t) 0);
                    uint32_t guid72 = (lst).length;
                    for (uint32_t i = guid71; i < guid72; i++) {
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
    template<typename t976, typename t978, int c116>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t976, c116>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t978, c116>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t> {
            using a = t976;
            using b = t978;
            constexpr int32_t n = c116;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t> {
                uint32_t guid73 = Math::min_<uint32_t>((lstA).length, (lstB).length);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t outLen = guid73;
                
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t> guid74 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t> guid75;
                    guid75.data = zeros<juniper::tuple2<t976, t978>, c116>();
                    guid75.length = outLen;
                    return guid75;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t976, t978>, c116>, uint32_t> ret = guid74;
                
                (([&]() -> juniper::unit {
                    uint32_t guid76 = ((uint32_t) 0);
                    uint32_t guid77 = outLen;
                    for (uint32_t i = guid76; i < guid77; i++) {
                        (([&]() -> juniper::tuple2<t976, t978> {
                            return (((ret).data)[i] = (juniper::tuple2<t976,t978>{((lstA).data)[i], ((lstB).data)[i]}));
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
    template<typename t995, typename t996, int c121>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t>, juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t995, t996>, c121>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t>, juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t>> {
            using a = t995;
            using b = t996;
            constexpr int32_t n = c121;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t>, juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t> guid78 = (([&]() -> juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t> guid79;
                    guid79.data = zeros<t995, c121>();
                    guid79.length = (lst).length;
                    return guid79;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t> retA = guid78;
                
                juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t> guid80 = (([&]() -> juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t> guid81;
                    guid81.data = zeros<t996, c121>();
                    guid81.length = (lst).length;
                    return guid81;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t> retB = guid80;
                
                (([&]() -> juniper::unit {
                    uint32_t guid82 = ((uint32_t) 0);
                    uint32_t guid83 = (lst).length;
                    for (uint32_t i = guid82; i < guid83; i++) {
                        (([&]() -> t996 {
                            juniper::tuple2<t995, t996> guid84 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t996 b = (guid84).e2;
                            t995 a = (guid84).e1;
                            
                            (((retA).data)[i] = a);
                            return (((retB).data)[i] = b);
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t995, c121>, uint32_t>,juniper::records::recordt_0<juniper::array<t996, c121>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t1004, int c127>
    t1004 sum(juniper::records::recordt_0<juniper::array<t1004, c127>, uint32_t> lst) {
        return (([&]() -> t1004 {
            using a = t1004;
            constexpr int32_t n = c127;
            return List::fold<t1004, void, t1004, c127>(Prelude::add<t1004>, ((t1004) 0), lst);
        })());
    }
}

namespace List {
    template<typename t1022, int c129>
    t1022 average(juniper::records::recordt_0<juniper::array<t1022, c129>, uint32_t> lst) {
        return (([&]() -> t1022 {
            using a = t1022;
            constexpr int32_t n = c129;
            return ((t1022) (sum<t1022, c129>(lst) / cast<uint32_t, t1022>((lst).length)));
        })());
    }
}

namespace List {
    uint32_t iLeftChild(uint32_t i) {
        return ((uint32_t) (((uint32_t) (((uint32_t) 2) * i)) + ((uint32_t) 1)));
    }
}

namespace List {
    uint32_t iRightChild(uint32_t i) {
        return ((uint32_t) (((uint32_t) (((uint32_t) 2) * i)) + ((uint32_t) 2)));
    }
}

namespace List {
    uint32_t iParent(uint32_t i) {
        return ((uint32_t) (((uint32_t) (i - ((uint32_t) 1))) / ((uint32_t) 2)));
    }
}

namespace List {
    template<typename t1047, typename t1049, typename t1051, typename t1082, int c131>
    juniper::unit siftDown(juniper::records::recordt_0<juniper::array<t1082, c131>, uint32_t>& lst, juniper::function<t1051, t1047(t1082)> key, uint32_t root, t1049 end) {
        return (([&]() -> juniper::unit {
            using m = t1047;
            using t = t1082;
            constexpr int32_t n = c131;
            return (([&]() -> juniper::unit {
                bool guid85 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool done = guid85;
                
                return (([&]() -> juniper::unit {
                    while (((bool) (((bool) (iLeftChild(root) < end)) && !(done)))) {
                        (([&]() -> juniper::unit {
                            uint32_t guid86 = iLeftChild(root);
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            uint32_t child = guid86;
                            
                            (([&]() -> juniper::unit {
                                if (((bool) (((bool) (((uint32_t) (child + ((uint32_t) 1))) < end)) && ((bool) (key(((lst).data)[child]) < key(((lst).data)[((uint32_t) (child + ((uint32_t) 1)))])))))) {
                                    (([&]() -> uint32_t {
                                        return (child += ((uint32_t) 1));
                                    })());
                                }
                                return {};
                            })());
                            return (((bool) (key(((lst).data)[root]) < key(((lst).data)[child]))) ? 
                                (([&]() -> juniper::unit {
                                    t1082 guid87 = ((lst).data)[root];
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    t1082 tmp = guid87;
                                    
                                    (((lst).data)[root] = ((lst).data)[child]);
                                    (((lst).data)[child] = tmp);
                                    (root = child);
                                    return juniper::unit();
                                })())
                            :
                                (([&]() -> juniper::unit {
                                    (done = true);
                                    return juniper::unit();
                                })()));
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1093, typename t1102, typename t1104, int c140>
    juniper::unit heapify(juniper::records::recordt_0<juniper::array<t1093, c140>, uint32_t>& lst, juniper::function<t1104, t1102(t1093)> key) {
        return (([&]() -> juniper::unit {
            using t = t1093;
            constexpr int32_t n = c140;
            return (([&]() -> juniper::unit {
                uint32_t guid88 = iParent(((uint32_t) ((lst).length + ((uint32_t) 1))));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t start = guid88;
                
                return (([&]() -> juniper::unit {
                    while (((bool) (start > ((uint32_t) 0)))) {
                        (([&]() -> juniper::unit {
                            (start -= ((uint32_t) 1));
                            return siftDown<t1102, uint32_t, t1104, t1093, c140>(lst, key, start, (lst).length);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1115, typename t1117, typename t1130, int c142>
    juniper::unit sort(juniper::function<t1117, t1115(t1130)> key, juniper::records::recordt_0<juniper::array<t1130, c142>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using m = t1115;
            using t = t1130;
            constexpr int32_t n = c142;
            return (([&]() -> juniper::unit {
                heapify<t1130, t1115, t1117, c142>(lst, key);
                uint32_t guid89 = (lst).length;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t end = guid89;
                
                return (([&]() -> juniper::unit {
                    while (((bool) (end > ((uint32_t) 1)))) {
                        (([&]() -> juniper::unit {
                            (end -= ((uint32_t) 1));
                            t1130 guid90 = ((lst).data)[((uint32_t) 0)];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t1130 tmp = guid90;
                            
                            (((lst).data)[((uint32_t) 0)] = ((lst).data)[end]);
                            (((lst).data)[end] = tmp);
                            return siftDown<t1115, uint32_t, t1117, t1130, c142>(lst, key, ((uint32_t) 0), end);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1148, typename t1149, typename t1150, int c149>
    juniper::records::recordt_0<juniper::array<t1149, c149>, uint32_t> sorted(juniper::function<t1150, t1148(t1149)> key, juniper::records::recordt_0<juniper::array<t1149, c149>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t1149, c149>, uint32_t> {
            using m = t1148;
            using t = t1149;
            constexpr int32_t n = c149;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t1149, c149>, uint32_t> {
                sort<t1148, t1150, t1149, c149>(key, lst);
                return lst;
            })());
        })());
    }
}

namespace Signal {
    template<typename t1160, typename t1162, typename t1177>
    Prelude::sig<t1177> map(juniper::function<t1162, t1177(t1160)> f, Prelude::sig<t1160> s) {
        return (([&]() -> Prelude::sig<t1177> {
            using a = t1160;
            using b = t1177;
            return (([&]() -> Prelude::sig<t1177> {
                Prelude::sig<t1160> guid91 = s;
                return (((bool) (((bool) ((guid91).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid91).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1177> {
                        t1160 val = ((guid91).signal()).just();
                        return signal<t1177>(just<t1177>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1177> {
                            return signal<t1177>(nothing<t1177>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1177>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1246, typename t1247>
    juniper::unit sink(juniper::function<t1247, juniper::unit(t1246)> f, Prelude::sig<t1246> s) {
        return (([&]() -> juniper::unit {
            using a = t1246;
            return (([&]() -> juniper::unit {
                Prelude::sig<t1246> guid92 = s;
                return (((bool) (((bool) ((guid92).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid92).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::unit {
                        t1246 val = ((guid92).signal()).just();
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
    template<typename t1270, typename t1284>
    Prelude::sig<t1284> filter(juniper::function<t1270, bool(t1284)> f, Prelude::sig<t1284> s) {
        return (([&]() -> Prelude::sig<t1284> {
            using a = t1284;
            return (([&]() -> Prelude::sig<t1284> {
                Prelude::sig<t1284> guid93 = s;
                return (((bool) (((bool) ((guid93).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid93).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1284> {
                        t1284 val = ((guid93).signal()).just();
                        return (f(val) ? 
                            (([&]() -> Prelude::sig<t1284> {
                                return signal<t1284>(nothing<t1284>());
                            })())
                        :
                            (([&]() -> Prelude::sig<t1284> {
                                return s;
                            })()));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1284> {
                            return signal<t1284>(nothing<t1284>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1284>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1364>
    Prelude::sig<t1364> merge(Prelude::sig<t1364> sigA, Prelude::sig<t1364> sigB) {
        return (([&]() -> Prelude::sig<t1364> {
            using a = t1364;
            return (([&]() -> Prelude::sig<t1364> {
                Prelude::sig<t1364> guid94 = sigA;
                return (((bool) (((bool) ((guid94).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid94).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1364> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1364> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t1364>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1400, int c151>
    Prelude::sig<t1400> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t1400>, c151>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t1400> {
            using a = t1400;
            constexpr int32_t n = c151;
            return (([&]() -> Prelude::sig<t1400> {
                Prelude::maybe<t1400> guid95 = List::fold<Prelude::maybe<t1400>, void, Prelude::sig<t1400>, c151>(juniper::function<void, Prelude::maybe<t1400>(Prelude::sig<t1400>,Prelude::maybe<t1400>)>([](Prelude::sig<t1400> sig, Prelude::maybe<t1400> accum) -> Prelude::maybe<t1400> { 
                    return (([&]() -> Prelude::maybe<t1400> {
                        Prelude::maybe<t1400> guid96 = accum;
                        return (((bool) (((bool) ((guid96).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::maybe<t1400> {
                                return (([&]() -> Prelude::maybe<t1400> {
                                    Prelude::sig<t1400> guid97 = sig;
                                    if (!(((bool) (((bool) ((guid97).id() == ((uint8_t) 0))) && true)))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t1400> heldValue = (guid97).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t1400> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t1400>>()));
                    })());
                 }), nothing<t1400>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t1400> ret = guid95;
                
                return signal<t1400>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t1518, typename t1542>
    Prelude::sig<Prelude::either<t1542, t1518>> join(Prelude::sig<t1542> sigA, Prelude::sig<t1518> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t1542, t1518>> {
            using a = t1542;
            using b = t1518;
            return (([&]() -> Prelude::sig<Prelude::either<t1542, t1518>> {
                juniper::tuple2<Prelude::sig<t1542>, Prelude::sig<t1518>> guid98 = (juniper::tuple2<Prelude::sig<t1542>,Prelude::sig<t1518>>{sigA, sigB});
                return (((bool) (((bool) (((guid98).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid98).e1).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t1542, t1518>> {
                        t1542 value = (((guid98).e1).signal()).just();
                        return signal<Prelude::either<t1542, t1518>>(just<Prelude::either<t1542, t1518>>(left<t1542, t1518>(value)));
                    })())
                :
                    (((bool) (((bool) (((guid98).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid98).e2).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t1542, t1518>> {
                            t1518 value = (((guid98).e2).signal()).just();
                            return signal<Prelude::either<t1542, t1518>>(just<Prelude::either<t1542, t1518>>(right<t1542, t1518>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t1542, t1518>> {
                                return signal<Prelude::either<t1542, t1518>>(nothing<Prelude::either<t1542, t1518>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t1542, t1518>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1831>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t1831> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t1831;
            return map<t1831, void, juniper::unit>(juniper::function<void, juniper::unit(t1831)>([](t1831 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t1865, typename t1867, typename t1883>
    Prelude::sig<t1883> foldP(juniper::function<t1867, t1883(t1865, t1883)> f, t1883& state0, Prelude::sig<t1865> incoming) {
        return (([&]() -> Prelude::sig<t1883> {
            using a = t1865;
            using state = t1883;
            return (([&]() -> Prelude::sig<t1883> {
                Prelude::sig<t1865> guid99 = incoming;
                return (((bool) (((bool) ((guid99).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid99).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1883> {
                        t1865 val = ((guid99).signal()).just();
                        return (([&]() -> Prelude::sig<t1883> {
                            t1883 guid100 = f(val, state0);
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t1883 state1 = guid100;
                            
                            (state0 = state1);
                            return signal<t1883>(just<t1883>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1883> {
                            return signal<t1883>(nothing<t1883>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1883>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1960>
    Prelude::sig<t1960> dropRepeats(Prelude::maybe<t1960>& maybePrevValue, Prelude::sig<t1960> incoming) {
        return (([&]() -> Prelude::sig<t1960> {
            using a = t1960;
            return (([&]() -> Prelude::sig<t1960> {
                Prelude::sig<t1960> guid101 = incoming;
                return (((bool) (((bool) ((guid101).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid101).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1960> {
                        t1960 value = ((guid101).signal()).just();
                        return (([&]() -> Prelude::sig<t1960> {
                            Prelude::sig<t1960> guid102 = (([&]() -> Prelude::sig<t1960> {
                                Prelude::maybe<t1960> guid103 = maybePrevValue;
                                return (((bool) (((bool) ((guid103).id() == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> Prelude::sig<t1960> {
                                        return incoming;
                                    })())
                                :
                                    (((bool) (((bool) ((guid103).id() == ((uint8_t) 0))) && true)) ? 
                                        (([&]() -> Prelude::sig<t1960> {
                                            t1960 prevValue = (guid103).just();
                                            return (((bool) (value == prevValue)) ? 
                                                signal<t1960>(nothing<t1960>())
                                            :
                                                incoming);
                                        })())
                                    :
                                        juniper::quit<Prelude::sig<t1960>>()));
                            })());
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            Prelude::sig<t1960> ret = guid102;
                            
                            (maybePrevValue = just<t1960>(value));
                            return ret;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1960> {
                            return incoming;
                        })())
                    :
                        juniper::quit<Prelude::sig<t1960>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2059>
    Prelude::sig<t2059> latch(t2059& prevValue, Prelude::sig<t2059> incoming) {
        return (([&]() -> Prelude::sig<t2059> {
            using a = t2059;
            return (([&]() -> Prelude::sig<t2059> {
                Prelude::sig<t2059> guid104 = incoming;
                return (((bool) (((bool) ((guid104).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid104).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t2059> {
                        t2059 val = ((guid104).signal()).just();
                        return (([&]() -> Prelude::sig<t2059> {
                            (prevValue = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t2059> {
                            return signal<t2059>(just<t2059>(prevValue));
                        })())
                    :
                        juniper::quit<Prelude::sig<t2059>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2119, typename t2120, typename t2123, typename t2131>
    Prelude::sig<t2119> map2(juniper::function<t2120, t2119(t2123, t2131)> f, juniper::tuple2<t2123, t2131>& state, Prelude::sig<t2123> incomingA, Prelude::sig<t2131> incomingB) {
        return (([&]() -> Prelude::sig<t2119> {
            using a = t2123;
            using b = t2131;
            using c = t2119;
            return (([&]() -> Prelude::sig<t2119> {
                t2123 guid105 = (([&]() -> t2123 {
                    Prelude::sig<t2123> guid106 = incomingA;
                    return (((bool) (((bool) ((guid106).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid106).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t2123 {
                            t2123 val1 = ((guid106).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t2123 {
                                return fst<t2123, t2131>(state);
                            })())
                        :
                            juniper::quit<t2123>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t2123 valA = guid105;
                
                t2131 guid107 = (([&]() -> t2131 {
                    Prelude::sig<t2131> guid108 = incomingB;
                    return (((bool) (((bool) ((guid108).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid108).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t2131 {
                            t2131 val2 = ((guid108).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t2131 {
                                return snd<t2123, t2131>(state);
                            })())
                        :
                            juniper::quit<t2131>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t2131 valB = guid107;
                
                (state = (juniper::tuple2<t2123,t2131>{valA, valB}));
                return (([&]() -> Prelude::sig<t2119> {
                    juniper::tuple2<Prelude::sig<t2123>, Prelude::sig<t2131>> guid109 = (juniper::tuple2<Prelude::sig<t2123>,Prelude::sig<t2131>>{incomingA, incomingB});
                    return (((bool) (((bool) (((guid109).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid109).e2).signal()).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid109).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid109).e1).signal()).id() == ((uint8_t) 1))) && true)))))))) ? 
                        (([&]() -> Prelude::sig<t2119> {
                            return signal<t2119>(nothing<t2119>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t2119> {
                                return signal<t2119>(just<t2119>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t2119>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t2273, int c153>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t2273, c153>, uint32_t>> record(juniper::records::recordt_0<juniper::array<t2273, c153>, uint32_t>& pastValues, Prelude::sig<t2273> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t2273, c153>, uint32_t>> {
            using a = t2273;
            constexpr int32_t n = c153;
            return foldP<t2273, void, juniper::records::recordt_0<juniper::array<t2273, c153>, uint32_t>>(List::prependPure<t2273, c153>, pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t2311>
    Prelude::sig<t2311> constant(t2311 val) {
        return (([&]() -> Prelude::sig<t2311> {
            using a = t2311;
            return signal<t2311>(just<t2311>(val));
        })());
    }
}

namespace Signal {
    template<typename t2346>
    Prelude::sig<Prelude::maybe<t2346>> meta(Prelude::sig<t2346> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t2346>> {
            using a = t2346;
            return (([&]() -> Prelude::sig<Prelude::maybe<t2346>> {
                Prelude::sig<t2346> guid110 = sigA;
                if (!(((bool) (((bool) ((guid110).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t2346> val = (guid110).signal();
                
                return constant<Prelude::maybe<t2346>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t2413>
    Prelude::sig<t2413> unmeta(Prelude::sig<Prelude::maybe<t2413>> sigA) {
        return (([&]() -> Prelude::sig<t2413> {
            using a = t2413;
            return (([&]() -> Prelude::sig<t2413> {
                Prelude::sig<Prelude::maybe<t2413>> guid111 = sigA;
                return (((bool) (((bool) ((guid111).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid111).signal()).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid111).signal()).just()).id() == ((uint8_t) 0))) && true)))))) ? 
                    (([&]() -> Prelude::sig<t2413> {
                        t2413 val = (((guid111).signal()).just()).just();
                        return constant<t2413>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t2413> {
                            return signal<t2413>(nothing<t2413>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t2413>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2489, typename t2490>
    Prelude::sig<juniper::tuple2<t2489, t2490>> zip(juniper::tuple2<t2489, t2490>& state, Prelude::sig<t2489> sigA, Prelude::sig<t2490> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t2489, t2490>> {
            using a = t2489;
            using b = t2490;
            return map2<juniper::tuple2<t2489, t2490>, void, t2489, t2490>(juniper::function<void, juniper::tuple2<t2489, t2490>(t2489,t2490)>([](t2489 valA, t2490 valB) -> juniper::tuple2<t2489, t2490> { 
                return (juniper::tuple2<t2489,t2490>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t2560, typename t2567>
    juniper::tuple2<Prelude::sig<t2560>, Prelude::sig<t2567>> unzip(Prelude::sig<juniper::tuple2<t2560, t2567>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t2560>, Prelude::sig<t2567>> {
            using a = t2560;
            using b = t2567;
            return (([&]() -> juniper::tuple2<Prelude::sig<t2560>, Prelude::sig<t2567>> {
                Prelude::sig<juniper::tuple2<t2560, t2567>> guid112 = incoming;
                return (((bool) (((bool) ((guid112).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid112).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t2560>, Prelude::sig<t2567>> {
                        t2567 y = (((guid112).signal()).just()).e2;
                        t2560 x = (((guid112).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t2560>,Prelude::sig<t2567>>{signal<t2560>(just<t2560>(x)), signal<t2567>(just<t2567>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t2560>, Prelude::sig<t2567>> {
                            return (juniper::tuple2<Prelude::sig<t2560>,Prelude::sig<t2567>>{signal<t2560>(nothing<t2560>()), signal<t2567>(nothing<t2567>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t2560>, Prelude::sig<t2567>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2694, typename t2695>
    Prelude::sig<t2694> toggle(t2694 val1, t2694 val2, t2694& state, Prelude::sig<t2695> incoming) {
        return (([&]() -> Prelude::sig<t2694> {
            using a = t2694;
            using b = t2695;
            return foldP<t2695, juniper::closures::closuret_4<t2694, t2694>, t2694>(juniper::function<juniper::closures::closuret_4<t2694, t2694>, t2694(t2695,t2694)>(juniper::closures::closuret_4<t2694, t2694>(val1, val2), [](juniper::closures::closuret_4<t2694, t2694>& junclosure, t2695 event, t2694 prevVal) -> t2694 { 
                t2694& val1 = junclosure.val1;
                t2694& val2 = junclosure.val2;
                return (((bool) (prevVal == val1)) ? 
                    (([&]() -> t2694 {
                        return val2;
                    })())
                :
                    (([&]() -> t2694 {
                        return val1;
                    })()));
             }), state, incoming);
        })());
    }
}

namespace Io {
    Io::pinState toggle(Io::pinState p) {
        return (([&]() -> Io::pinState {
            Io::pinState guid113 = p;
            return (((bool) (((bool) ((guid113).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> Io::pinState {
                    return low();
                })())
            :
                (((bool) (((bool) ((guid113).id() == ((uint8_t) 1))) && true)) ? 
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
    template<int c155>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c155>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c155;
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
    template<typename t2750>
    t2750 baseToInt(Io::base b) {
        return (([&]() -> t2750 {
            Io::base guid114 = b;
            return (((bool) (((bool) ((guid114).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> t2750 {
                    return ((t2750) 2);
                })())
            :
                (((bool) (((bool) ((guid114).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> t2750 {
                        return ((t2750) 8);
                    })())
                :
                    (((bool) (((bool) ((guid114).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> t2750 {
                            return ((t2750) 10);
                        })())
                    :
                        (((bool) (((bool) ((guid114).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> t2750 {
                                return ((t2750) 16);
                            })())
                        :
                            juniper::quit<t2750>()))));
        })());
    }
}

namespace Io {
    juniper::unit printIntBase(int32_t n, Io::base b) {
        return (([&]() -> juniper::unit {
            int32_t guid115 = baseToInt<int32_t>(b);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t bint = guid115;
            
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
            Io::pinState guid116 = value;
            return (((bool) (((bool) ((guid116).id() == ((uint8_t) 1))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid116).id() == ((uint8_t) 0))) && true)) ? 
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
            uint8_t guid117 = pinStateToInt(value);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t intVal = guid117;
            
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
        return Signal::sink<Io::pinState, juniper::closures::closuret_5<uint16_t>>(juniper::function<juniper::closures::closuret_5<uint16_t>, juniper::unit(Io::pinState)>(juniper::closures::closuret_5<uint16_t>(pin), [](juniper::closures::closuret_5<uint16_t>& junclosure, Io::pinState value) -> juniper::unit { 
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
        return Signal::sink<uint8_t, juniper::closures::closuret_5<uint16_t>>(juniper::function<juniper::closures::closuret_5<uint16_t>, juniper::unit(uint8_t)>(juniper::closures::closuret_5<uint16_t>(pin), [](juniper::closures::closuret_5<uint16_t>& junclosure, uint8_t value) -> juniper::unit { 
            uint16_t& pin = junclosure.pin;
            return anaWrite(pin, value);
         }), sig);
    }
}

namespace Io {
    uint8_t pinModeToInt(Io::mode m) {
        return (([&]() -> uint8_t {
            Io::mode guid118 = m;
            return (((bool) (((bool) ((guid118).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid118).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    (((bool) (((bool) ((guid118).id() == ((uint8_t) 2))) && true)) ? 
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
            uint8_t guid119 = m;
            return (((bool) (((bool) (guid119 == ((uint8_t) 0))) && true)) ? 
                (([&]() -> Io::mode {
                    return input();
                })())
            :
                (((bool) (((bool) (guid119 == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> Io::mode {
                        return output();
                    })())
                :
                    (((bool) (((bool) (guid119 == ((uint8_t) 2))) && true)) ? 
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
            uint8_t guid120 = pinModeToInt(m);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t m2 = guid120;
            
            return (([&]() -> juniper::unit {
                pinMode(pin, m2);
                return {};
            })());
        })());
    }
}

namespace Io {
    Prelude::sig<juniper::unit> risingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            Prelude::sig<Io::pinState> guid121 = sig;
            return (((bool) (((bool) ((guid121).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid121).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<juniper::unit> {
                    Io::pinState currState = ((guid121).signal()).just();
                    return (([&]() -> Prelude::sig<juniper::unit> {
                        Prelude::maybe<juniper::unit> guid122 = (([&]() -> Prelude::maybe<juniper::unit> {
                            juniper::tuple2<Io::pinState, Io::pinState> guid123 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, prevState});
                            return (((bool) (((bool) (((guid123).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid123).e1).id() == ((uint8_t) 0))) && true)))) ? 
                                (([&]() -> Prelude::maybe<juniper::unit> {
                                    return just<juniper::unit>(juniper::unit());
                                })())
                            :
                                (true ? 
                                    (([&]() -> Prelude::maybe<juniper::unit> {
                                        return nothing<juniper::unit>();
                                    })())
                                :
                                    juniper::quit<Prelude::maybe<juniper::unit>>()));
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<juniper::unit> ret = guid122;
                        
                        (prevState = currState);
                        return signal<juniper::unit>(ret);
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<juniper::unit> {
                        return signal<juniper::unit>(nothing<juniper::unit>());
                    })())
                :
                    juniper::quit<Prelude::sig<juniper::unit>>()));
        })());
    }
}

namespace Io {
    Prelude::sig<juniper::unit> fallingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            Prelude::sig<Io::pinState> guid124 = sig;
            return (((bool) (((bool) ((guid124).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid124).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<juniper::unit> {
                    Io::pinState currState = ((guid124).signal()).just();
                    return (([&]() -> Prelude::sig<juniper::unit> {
                        Prelude::maybe<juniper::unit> guid125 = (([&]() -> Prelude::maybe<juniper::unit> {
                            juniper::tuple2<Io::pinState, Io::pinState> guid126 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, prevState});
                            return (((bool) (((bool) (((guid126).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid126).e1).id() == ((uint8_t) 1))) && true)))) ? 
                                (([&]() -> Prelude::maybe<juniper::unit> {
                                    return just<juniper::unit>(juniper::unit());
                                })())
                            :
                                (true ? 
                                    (([&]() -> Prelude::maybe<juniper::unit> {
                                        return nothing<juniper::unit>();
                                    })())
                                :
                                    juniper::quit<Prelude::maybe<juniper::unit>>()));
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<juniper::unit> ret = guid125;
                        
                        (prevState = currState);
                        return signal<juniper::unit>(ret);
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<juniper::unit> {
                        return signal<juniper::unit>(nothing<juniper::unit>());
                    })())
                :
                    juniper::quit<Prelude::sig<juniper::unit>>()));
        })());
    }
}

namespace Io {
    Prelude::sig<juniper::unit> edge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            Prelude::sig<Io::pinState> guid127 = sig;
            return (((bool) (((bool) ((guid127).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid127).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<juniper::unit> {
                    Io::pinState currState = ((guid127).signal()).just();
                    return (([&]() -> Prelude::sig<juniper::unit> {
                        Prelude::maybe<juniper::unit> guid128 = (([&]() -> Prelude::maybe<juniper::unit> {
                            juniper::tuple2<Io::pinState, Io::pinState> guid129 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, prevState});
                            return (((bool) (((bool) (((guid129).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid129).e1).id() == ((uint8_t) 1))) && true)))) ? 
                                (([&]() -> Prelude::maybe<juniper::unit> {
                                    return just<juniper::unit>(juniper::unit());
                                })())
                            :
                                (((bool) (((bool) (((guid129).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid129).e1).id() == ((uint8_t) 0))) && true)))) ? 
                                    (([&]() -> Prelude::maybe<juniper::unit> {
                                        return just<juniper::unit>(juniper::unit());
                                    })())
                                :
                                    (true ? 
                                        (([&]() -> Prelude::maybe<juniper::unit> {
                                            return nothing<juniper::unit>();
                                        })())
                                    :
                                        juniper::quit<Prelude::maybe<juniper::unit>>())));
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<juniper::unit> ret = guid128;
                        
                        (prevState = currState);
                        return signal<juniper::unit>(ret);
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<juniper::unit> {
                        return signal<juniper::unit>(nothing<juniper::unit>());
                    })())
                :
                    juniper::quit<Prelude::sig<juniper::unit>>()));
        })());
    }
}

namespace Maybe {
    template<typename t3204, typename t3206, typename t3215>
    Prelude::maybe<t3215> map(juniper::function<t3206, t3215(t3204)> f, Prelude::maybe<t3204> maybeVal) {
        return (([&]() -> Prelude::maybe<t3215> {
            using a = t3204;
            using b = t3215;
            return (([&]() -> Prelude::maybe<t3215> {
                Prelude::maybe<t3204> guid130 = maybeVal;
                return (((bool) (((bool) ((guid130).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t3215> {
                        t3204 val = (guid130).just();
                        return just<t3215>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t3215> {
                            return nothing<t3215>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t3215>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3245>
    t3245 get(Prelude::maybe<t3245> maybeVal) {
        return (([&]() -> t3245 {
            using a = t3245;
            return (([&]() -> t3245 {
                Prelude::maybe<t3245> guid131 = maybeVal;
                return (((bool) (((bool) ((guid131).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t3245 {
                        t3245 val = (guid131).just();
                        return val;
                    })())
                :
                    juniper::quit<t3245>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3254>
    bool isJust(Prelude::maybe<t3254> maybeVal) {
        return (([&]() -> bool {
            using a = t3254;
            return (([&]() -> bool {
                Prelude::maybe<t3254> guid132 = maybeVal;
                return (((bool) (((bool) ((guid132).id() == ((uint8_t) 0))) && true)) ? 
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
    template<typename t3265>
    bool isNothing(Prelude::maybe<t3265> maybeVal) {
        return (([&]() -> bool {
            using a = t3265;
            return !(isJust<t3265>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t3279>
    uint8_t count(Prelude::maybe<t3279> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t3279;
            return (([&]() -> uint8_t {
                Prelude::maybe<t3279> guid133 = maybeVal;
                return (((bool) (((bool) ((guid133).id() == ((uint8_t) 0))) && true)) ? 
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
    template<typename t3293, typename t3294, typename t3295>
    t3293 fold(juniper::function<t3295, t3293(t3294, t3293)> f, t3293 initState, Prelude::maybe<t3294> maybeVal) {
        return (([&]() -> t3293 {
            using state = t3293;
            using t = t3294;
            return (([&]() -> t3293 {
                Prelude::maybe<t3294> guid134 = maybeVal;
                return (((bool) (((bool) ((guid134).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t3293 {
                        t3294 val = (guid134).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t3293 {
                            return initState;
                        })())
                    :
                        juniper::quit<t3293>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3311, typename t3312>
    juniper::unit iter(juniper::function<t3312, juniper::unit(t3311)> f, Prelude::maybe<t3311> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t3311;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t3311> guid135 = maybeVal;
                return (((bool) (((bool) ((guid135).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> juniper::unit {
                        t3311 val = (guid135).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t3311> nothing = guid135;
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
            uint32_t guid136 = ((uint32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t ret = guid136;
            
            (([&]() -> juniper::unit {
                ret = millis();
                return {};
            })());
            return ret;
        })());
    }
}

namespace Time {
    juniper::records::recordt_1<uint32_t> state = (([]() -> juniper::records::recordt_1<uint32_t>{
        juniper::records::recordt_1<uint32_t> guid137;
        guid137.lastPulse = ((uint32_t) 0);
        return guid137;
    })());
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::records::recordt_1<uint32_t>& state) {
        return (([&]() -> Prelude::sig<uint32_t> {
            uint32_t guid138 = now();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t t = guid138;
            
            uint32_t guid139 = (((bool) (interval == ((uint32_t) 0))) ? 
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
            uint32_t lastWindow = guid139;
            
            return (((bool) ((state).lastPulse >= lastWindow)) ? 
                (([&]() -> Prelude::sig<uint32_t> {
                    return signal<uint32_t>(nothing<uint32_t>());
                })())
            :
                (([&]() -> Prelude::sig<uint32_t> {
                    ((state).lastPulse = t);
                    return signal<uint32_t>(just<uint32_t>(t));
                })()));
        })());
    }
}

namespace Math {
    double pi = ((double) 3.141592653589793238462643383279502884197169399375105820974);
}

namespace Math {
    double e = ((double) 2.718281828459045235360287471352662497757247093699959574966);
}

namespace Math {
    double degToRad(double degrees) {
        return ((double) (degrees * ((double) 0.017453292519943295769236907684886)));
    }
}

namespace Math {
    double radToDeg(double radians) {
        return ((double) (radians * ((double) 57.295779513082320876798154814105)));
    }
}

namespace Math {
    double acos_(double x) {
        return (([&]() -> double {
            double guid140 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid140;
            
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
            double guid141 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid141;
            
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
            double guid142 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid142;
            
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
            double guid143 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid143;
            
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
            double guid144 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid144;
            
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
            double guid145 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid145;
            
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
            double guid146 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid146;
            
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
            double guid147 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid147;
            
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
            double guid148 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid148;
            
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
            double guid149 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid149;
            
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
            double guid150 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid150;
            
            int32_t guid151 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t exponent = guid151;
            
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
            double guid152 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid152;
            
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
            double guid153 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid153;
            
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
            double guid154 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid154;
            
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
            double guid155 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid155;
            
            double guid156 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double integer = guid156;
            
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
            double ret;
            
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
            double ret;
            
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
            double ret;
            
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
            double ret;
            
            (([&]() -> juniper::unit {
                ret = fabs(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    template<typename t3456>
    t3456 abs_(t3456 x) {
        return (([&]() -> t3456 {
            using t = t3456;
            return (([&]() -> t3456 {
                return (((bool) (x < ((t3456) 0))) ? 
                    (([&]() -> t3456 {
                        return -(x);
                    })())
                :
                    (([&]() -> t3456 {
                        return x;
                    })()));
            })());
        })());
    }
}

namespace Math {
    double floor_(double x) {
        return (([&]() -> double {
            double ret;
            
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
            double guid157 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid157;
            
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
        return floor_(((double) (x + ((double) 0.5))));
    }
}

namespace Math {
    template<typename t3467>
    t3467 max_(t3467 x, t3467 y) {
        return (([&]() -> t3467 {
            using a = t3467;
            return (((bool) (x > y)) ? 
                (([&]() -> t3467 {
                    return x;
                })())
            :
                (([&]() -> t3467 {
                    return y;
                })()));
        })());
    }
}

namespace Math {
    template<typename t3469>
    t3469 mapRange(t3469 x, t3469 a1, t3469 a2, t3469 b1, t3469 b2) {
        return (([&]() -> t3469 {
            using t = t3469;
            return ((t3469) (b1 + ((t3469) (((t3469) (((t3469) (x - a1)) * ((t3469) (b2 - b1)))) / ((t3469) (a2 - a1))))));
        })());
    }
}

namespace Math {
    template<typename t3471>
    t3471 clamp(t3471 x, t3471 min, t3471 max) {
        return (([&]() -> t3471 {
            using a = t3471;
            return (((bool) (min > x)) ? 
                (([&]() -> t3471 {
                    return min;
                })())
            :
                (((bool) (x > max)) ? 
                    (([&]() -> t3471 {
                        return max;
                    })())
                :
                    (([&]() -> t3471 {
                        return x;
                    })())));
        })());
    }
}

namespace Math {
    template<typename t3476>
    int8_t sign(t3476 n) {
        return (([&]() -> int8_t {
            using a = t3476;
            return (((bool) (n == ((t3476) 0))) ? 
                (([&]() -> int8_t {
                    return ((int8_t) 0);
                })())
            :
                (((bool) (n > ((t3476) 0))) ? 
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
    juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> state = (([]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
        juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid158;
        guid158.actualState = Io::low();
        guid158.lastState = Io::low();
        guid158.lastDebounceTime = ((uint32_t) 0);
        return guid158;
    })());
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState) {
        return (([&]() -> Prelude::sig<Io::pinState> {
            Prelude::sig<Io::pinState> guid159 = incoming;
            return (((bool) (((bool) ((guid159).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid159).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<Io::pinState> {
                    Io::pinState currentState = ((guid159).signal()).just();
                    return (([&]() -> Prelude::sig<Io::pinState> {
                        juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid160 = buttonState;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint32_t lastDebounceTime = (guid160).lastDebounceTime;
                        Io::pinState lastState = (guid160).lastState;
                        Io::pinState actualState = (guid160).actualState;
                        
                        Io::pinState guid161 = (((bool) (currentState != lastState)) ? 
                            (([&]() -> Io::pinState {
                                ((buttonState).lastState = currentState);
                                ((buttonState).lastDebounceTime = Time::now());
                                return actualState;
                            })())
                        :
                            (((bool) (((bool) (currentState != actualState)) && ((bool) (((uint32_t) (Time::now() - (buttonState).lastDebounceTime)) > delay)))) ? 
                                (([&]() -> Io::pinState {
                                    ((buttonState).actualState = currentState);
                                    ((buttonState).lastState = currentState);
                                    return currentState;
                                })())
                            :
                                (([&]() -> Io::pinState {
                                    ((buttonState).lastState = currentState);
                                    return actualState;
                                })())));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Io::pinState retState = guid161;
                        
                        return signal<Io::pinState>(just<Io::pinState>(retState));
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<Io::pinState> {
                        return incoming;
                    })())
                :
                    juniper::quit<Prelude::sig<Io::pinState>>()));
        })());
    }
}

namespace Button {
    Prelude::sig<Io::pinState> debounce(Prelude::sig<Io::pinState> incoming, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState) {
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
    template<typename t3596, int c156>
    juniper::array<t3596, c156> add(juniper::array<t3596, c156> v1, juniper::array<t3596, c156> v2) {
        return (([&]() -> juniper::array<t3596, c156> {
            using a = t3596;
            constexpr int32_t n = c156;
            return (([&]() -> juniper::array<t3596, c156> {
                (([&]() -> juniper::unit {
                    int32_t guid162 = ((int32_t) 0);
                    int32_t guid163 = n;
                    for (int32_t i = guid162; i < guid163; i++) {
                        (([&]() -> t3596 {
                            return ((v1)[i] += (v2)[i]);
                        })());
                    }
                    return {};
                })());
                return v1;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3600, int c159>
    juniper::array<t3600, c159> zero() {
        return (([&]() -> juniper::array<t3600, c159> {
            using a = t3600;
            constexpr int32_t n = c159;
            return array<t3600, c159>(((t3600) 0));
        })());
    }
}

namespace Vector {
    template<typename t3608, int c161>
    juniper::array<t3608, c161> subtract(juniper::array<t3608, c161> v1, juniper::array<t3608, c161> v2) {
        return (([&]() -> juniper::array<t3608, c161> {
            using a = t3608;
            constexpr int32_t n = c161;
            return (([&]() -> juniper::array<t3608, c161> {
                (([&]() -> juniper::unit {
                    int32_t guid164 = ((int32_t) 0);
                    int32_t guid165 = n;
                    for (int32_t i = guid164; i < guid165; i++) {
                        (([&]() -> t3608 {
                            return ((v1)[i] -= (v2)[i]);
                        })());
                    }
                    return {};
                })());
                return v1;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3612, int c164>
    juniper::array<t3612, c164> scale(t3612 scalar, juniper::array<t3612, c164> v) {
        return (([&]() -> juniper::array<t3612, c164> {
            using a = t3612;
            constexpr int32_t n = c164;
            return (([&]() -> juniper::array<t3612, c164> {
                (([&]() -> juniper::unit {
                    int32_t guid166 = ((int32_t) 0);
                    int32_t guid167 = n;
                    for (int32_t i = guid166; i < guid167; i++) {
                        (([&]() -> t3612 {
                            return ((v)[i] *= scalar);
                        })());
                    }
                    return {};
                })());
                return v;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3618, int c166>
    t3618 dot(juniper::array<t3618, c166> v1, juniper::array<t3618, c166> v2) {
        return (([&]() -> t3618 {
            using a = t3618;
            constexpr int32_t n = c166;
            return (([&]() -> t3618 {
                t3618 guid168 = ((t3618) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t3618 sum = guid168;
                
                (([&]() -> juniper::unit {
                    int32_t guid169 = ((int32_t) 0);
                    int32_t guid170 = n;
                    for (int32_t i = guid169; i < guid170; i++) {
                        (([&]() -> t3618 {
                            return (sum += ((t3618) ((v1)[i] * (v2)[i])));
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
    template<typename t3624, int c169>
    t3624 magnitude2(juniper::array<t3624, c169> v) {
        return (([&]() -> t3624 {
            using a = t3624;
            constexpr int32_t n = c169;
            return (([&]() -> t3624 {
                t3624 guid171 = ((t3624) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t3624 sum = guid171;
                
                (([&]() -> juniper::unit {
                    int32_t guid172 = ((int32_t) 0);
                    int32_t guid173 = n;
                    for (int32_t i = guid172; i < guid173; i++) {
                        (([&]() -> t3624 {
                            return (sum += ((t3624) ((v)[i] * (v)[i])));
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
    template<typename t3626, int c172>
    double magnitude(juniper::array<t3626, c172> v) {
        return (([&]() -> double {
            using a = t3626;
            constexpr int32_t n = c172;
            return sqrt_(toDouble<t3626>(magnitude2<t3626, c172>(v)));
        })());
    }
}

namespace Vector {
    template<typename t3642, int c174>
    juniper::array<t3642, c174> multiply(juniper::array<t3642, c174> u, juniper::array<t3642, c174> v) {
        return (([&]() -> juniper::array<t3642, c174> {
            using a = t3642;
            constexpr int32_t n = c174;
            return (([&]() -> juniper::array<t3642, c174> {
                (([&]() -> juniper::unit {
                    int32_t guid174 = ((int32_t) 0);
                    int32_t guid175 = n;
                    for (int32_t i = guid174; i < guid175; i++) {
                        (([&]() -> t3642 {
                            return ((u)[i] *= (v)[i]);
                        })());
                    }
                    return {};
                })());
                return u;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3651, int c177>
    juniper::array<t3651, c177> normalize(juniper::array<t3651, c177> v) {
        return (([&]() -> juniper::array<t3651, c177> {
            using a = t3651;
            constexpr int32_t n = c177;
            return (([&]() -> juniper::array<t3651, c177> {
                double guid176 = magnitude<t3651, c177>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid176;
                
                (([&]() -> juniper::unit {
                    if (((bool) (mag > ((t3651) 0)))) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                int32_t guid177 = ((int32_t) 0);
                                int32_t guid178 = n;
                                for (int32_t i = guid177; i < guid178; i++) {
                                    (([&]() -> t3651 {
                                        return ((v)[i] = fromDouble<t3651>(((double) (toDouble<t3651>((v)[i]) / mag))));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return v;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3662, int c181>
    double angle(juniper::array<t3662, c181> v1, juniper::array<t3662, c181> v2) {
        return (([&]() -> double {
            using a = t3662;
            constexpr int32_t n = c181;
            return acos_(((double) (toDouble<t3662>(dot<t3662, c181>(v1, v2)) / sqrt_(toDouble<t3662>(((t3662) (magnitude2<t3662, c181>(v1) * magnitude2<t3662, c181>(v2))))))));
        })());
    }
}

namespace Vector {
    template<typename t3704>
    juniper::array<t3704, 3> cross(juniper::array<t3704, 3> u, juniper::array<t3704, 3> v) {
        return (([&]() -> juniper::array<t3704, 3> {
            using a = t3704;
            return (juniper::array<t3704, 3> { {((t3704) (((t3704) ((u)[y] * (v)[z])) - ((t3704) ((u)[z] * (v)[y])))), ((t3704) (((t3704) ((u)[z] * (v)[x])) - ((t3704) ((u)[x] * (v)[z])))), ((t3704) (((t3704) ((u)[x] * (v)[y])) - ((t3704) ((u)[y] * (v)[x]))))} });
        })());
    }
}

namespace Vector {
    template<typename t3706, int c197>
    juniper::array<t3706, c197> project(juniper::array<t3706, c197> a, juniper::array<t3706, c197> b) {
        return (([&]() -> juniper::array<t3706, c197> {
            using z = t3706;
            constexpr int32_t n = c197;
            return (([&]() -> juniper::array<t3706, c197> {
                juniper::array<t3706, c197> guid179 = normalize<t3706, c197>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t3706, c197> bn = guid179;
                
                return scale<t3706, c197>(dot<t3706, c197>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t3722, int c201>
    juniper::array<t3722, c201> projectPlane(juniper::array<t3722, c201> a, juniper::array<t3722, c201> m) {
        return (([&]() -> juniper::array<t3722, c201> {
            using z = t3722;
            constexpr int32_t n = c201;
            return subtract<t3722, c201>(a, project<t3722, c201>(a, m));
        })());
    }
}

namespace CharList {
    template<int c204>
    juniper::records::recordt_0<juniper::array<uint8_t, c204>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c204>, uint32_t> str) {
        return List::map<uint8_t, void, uint8_t, c204>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 97))) && ((bool) (c <= ((uint8_t) 122))))) ? 
                ((uint8_t) (c - ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c205>
    juniper::records::recordt_0<juniper::array<uint8_t, c205>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c205>, uint32_t> str) {
        return List::map<uint8_t, void, uint8_t, c205>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 65))) && ((bool) (c <= ((uint8_t) 90))))) ? 
                ((uint8_t) (c + ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c206>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t> i32ToCharList(int32_t m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t n = c206;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t> guid180 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t> guid181;
                    guid181.data = array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>(((uint8_t) 0));
                    guid181.length = ((uint32_t) 0);
                    return guid181;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c206)*(-1)))*(-1)>, uint32_t> ret = guid180;
                
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
    template<int c208>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c208>, uint32_t> s) {
        return (([&]() -> uint32_t {
            constexpr int32_t n = c208;
            return ((uint32_t) ((s).length - ((uint32_t) 1)));
        })());
    }
}

namespace CharList {
    template<int c209, int c210, int c211>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c209)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c210)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t aCap = c209;
            constexpr int32_t bCap = c210;
            constexpr int32_t retCap = c211;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t> {
                uint32_t guid182 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid182;
                
                uint32_t guid183 = length<(1)+(c209)>(sA);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenA = guid183;
                
                uint32_t guid184 = length<(1)+(c210)>(sB);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenB = guid184;
                
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t> guid185 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t> guid186;
                    guid186.data = array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>(((uint8_t) 0));
                    guid186.length = ((uint32_t) (((uint32_t) (lenA + lenB)) + ((uint32_t) 1)));
                    return guid186;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c211)*(-1)))*(-1)>, uint32_t> out = guid185;
                
                (([&]() -> juniper::unit {
                    uint32_t guid187 = ((uint32_t) 0);
                    uint32_t guid188 = lenA;
                    for (uint32_t i = guid187; i < guid188; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sA).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid189 = ((uint32_t) 0);
                    uint32_t guid190 = lenB;
                    for (uint32_t i = guid189; i < guid190; i++) {
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
    template<int c219, int c220>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c219)*(-1)))+((c220)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c219)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c220)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c219)*(-1)))+((c220)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t aCap = c219;
            constexpr int32_t bCap = c220;
            return concat<c219, c220, (((c219)*(-1))+((c220)*(-1)))*(-1)>(sA, sB);
        })());
    }
}

namespace Random {
    int32_t random_(int32_t low, int32_t high) {
        return (([&]() -> int32_t {
            int32_t guid191 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t ret = guid191;
            
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
    template<typename t3800, int c224>
    t3800 choice(juniper::records::recordt_0<juniper::array<t3800, c224>, uint32_t> lst) {
        return (([&]() -> t3800 {
            using t = t3800;
            constexpr int32_t n = c224;
            return (([&]() -> t3800 {
                int32_t guid192 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid192;
                
                return List::get<uint32_t, t3800, c224>(i32ToU32(i), lst);
            })());
        })());
    }
}

namespace Random {
    template<typename t3820, int c226>
    Prelude::maybe<t3820> tryChoice(juniper::records::recordt_0<juniper::array<t3820, c226>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t3820> {
            using t = t3820;
            constexpr int32_t n = c226;
            return (([&]() -> Prelude::maybe<t3820> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t3820> {
                        return nothing<t3820>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t3820> {
                        int32_t guid193 = random_(((int32_t) 0), u32ToI32((lst).length));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        int32_t i = guid193;
                        
                        return List::tryGet<uint32_t, t3820, c226>(i32ToU32(i), lst);
                    })()));
            })());
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_5<float, float, float> color) {
        return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> {
            juniper::records::recordt_5<float, float, float> guid194 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float v = (guid194).v;
            float s = (guid194).s;
            float h = (guid194).h;
            
            float guid195 = ((float) (v * s));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float c = guid195;
            
            float guid196 = ((float) (c * toFloat<double>(((double) (((double) 1.0) - Math::fabs_(((double) (Math::fmod_(((double) (toDouble<float>(h) / ((double) 60))), ((double) 2.0)) - ((double) 1.0)))))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float x = guid196;
            
            float guid197 = ((float) (v - c));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float m = guid197;
            
            juniper::tuple3<float, float, float> guid198 = (((bool) (((bool) (0.0f <= h)) && ((bool) (h < 60.0f)))) ? 
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
            float bPrime = (guid198).e3;
            float gPrime = (guid198).e2;
            float rPrime = (guid198).e1;
            
            float guid199 = ((float) (((float) (rPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r = guid199;
            
            float guid200 = ((float) (((float) (gPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g = guid200;
            
            float guid201 = ((float) (((float) (bPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b = guid201;
            
            return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
                juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid202;
                guid202.r = toUInt8<float>(r);
                guid202.g = toUInt8<float>(g);
                guid202.b = toUInt8<float>(b);
                return guid202;
            })());
        })());
    }
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> color) {
        return (([&]() -> uint16_t {
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid203 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = (guid203).b;
            uint8_t g = (guid203).g;
            uint8_t r = (guid203).r;
            
            return ((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (u8ToU16(r) & ((uint16_t) 248))) << ((uint32_t) 8))) | ((uint16_t) (((uint16_t) (u8ToU16(g) & ((uint16_t) 252))) << ((uint32_t) 3))))) | ((uint16_t) (u8ToU16(b) >> ((uint32_t) 3)))));
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> red = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid204;
        guid204.r = ((uint8_t) 255);
        guid204.g = ((uint8_t) 0);
        guid204.b = ((uint8_t) 0);
        return guid204;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> green = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid205;
        guid205.r = ((uint8_t) 0);
        guid205.g = ((uint8_t) 255);
        guid205.b = ((uint8_t) 0);
        return guid205;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> blue = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid206;
        guid206.r = ((uint8_t) 0);
        guid206.g = ((uint8_t) 0);
        guid206.b = ((uint8_t) 255);
        return guid206;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> black = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid207;
        guid207.r = ((uint8_t) 0);
        guid207.g = ((uint8_t) 0);
        guid207.b = ((uint8_t) 0);
        return guid207;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> white = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid208;
        guid208.r = ((uint8_t) 255);
        guid208.g = ((uint8_t) 255);
        guid208.b = ((uint8_t) 255);
        return guid208;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> yellow = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid209;
        guid209.r = ((uint8_t) 255);
        guid209.g = ((uint8_t) 255);
        guid209.b = ((uint8_t) 0);
        return guid209;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> magenta = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid210;
        guid210.r = ((uint8_t) 255);
        guid210.g = ((uint8_t) 0);
        guid210.b = ((uint8_t) 255);
        return guid210;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> cyan = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid211;
        guid211.r = ((uint8_t) 0);
        guid211.g = ((uint8_t) 255);
        guid211.b = ((uint8_t) 255);
        return guid211;
    })());
}

namespace Arcada {
    bool arcadaBegin() {
        return (([&]() -> bool {
            bool guid212 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid212;
            
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
            uint16_t guid213 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid213;
            
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
            uint16_t guid214 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid214;
            
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
            uint16_t guid215 = displayWidth();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t w = guid215;
            
            uint16_t guid216 = displayHeight();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t h = guid216;
            
            bool guid217 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid217;
            
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
            bool guid218 = true;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid218;
            
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
            Ble::servicet guid219 = s;
            if (!(((bool) (((bool) ((guid219).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid219).service();
            
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
            Ble::characterstict guid220 = c;
            if (!(((bool) (((bool) ((guid220).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid220).characterstic();
            
            return (([&]() -> juniper::unit {
                ((BLECharacteristic *) p)->begin();
                return {};
            })());
        })());
    }
}

namespace Ble {
    template<int c228>
    uint16_t writeCharactersticBytes(Ble::characterstict c, juniper::records::recordt_0<juniper::array<uint8_t, c228>, uint32_t> bytes) {
        return (([&]() -> uint16_t {
            constexpr int32_t n = c228;
            return (([&]() -> uint16_t {
                juniper::records::recordt_0<juniper::array<uint8_t, c228>, uint32_t> guid221 = bytes;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t length = (guid221).length;
                juniper::array<uint8_t, c228> data = (guid221).data;
                
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
            Ble::characterstict guid228 = c;
            if (!(((bool) (((bool) ((guid228).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid228).characterstic();
            
            uint16_t guid229 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid229;
            
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
            Ble::characterstict guid230 = c;
            if (!(((bool) (((bool) ((guid230).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid230).characterstic();
            
            uint16_t guid231 = ((uint16_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t ret = guid231;
            
            (([&]() -> juniper::unit {
                ret = ((BLECharacteristic *) p)->write32((int) n);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Ble {
    template<typename t4201>
    uint16_t writeGeneric(Ble::characterstict c, t4201 x) {
        return (([&]() -> uint16_t {
            using t = t4201;
            return (([&]() -> uint16_t {
                Ble::characterstict guid232 = c;
                if (!(((bool) (((bool) ((guid232).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid232).characterstic();
                
                uint16_t guid233 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t ret = guid233;
                
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
    template<typename t4206>
    t4206 readGeneric(Ble::characterstict c) {
        return (([&]() -> t4206 {
            using t = t4206;
            return (([&]() -> t4206 {
                Ble::characterstict guid234 = c;
                if (!(((bool) (((bool) ((guid234).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                void * p = (guid234).characterstic();
                
                t4206 ret;
                
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
            Ble::secureModet guid235 = readPermission;
            if (!(((bool) (((bool) ((guid235).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t readN = (guid235).secureMode();
            
            Ble::secureModet guid236 = writePermission;
            if (!(((bool) (((bool) ((guid236).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t writeN = (guid236).secureMode();
            
            Ble::characterstict guid237 = c;
            if (!(((bool) (((bool) ((guid237).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid237).characterstic();
            
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
            Ble::propertiest guid238 = prop;
            if (!(((bool) (((bool) ((guid238).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t propN = (guid238).properties();
            
            Ble::characterstict guid239 = c;
            if (!(((bool) (((bool) ((guid239).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid239).characterstic();
            
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
            Ble::characterstict guid240 = c;
            if (!(((bool) (((bool) ((guid240).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid240).characterstic();
            
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
            Ble::advertisingFlagt guid241 = flag;
            if (!(((bool) (((bool) ((guid241).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t flagNum = (guid241).advertisingFlag();
            
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
            Ble::appearancet guid242 = app;
            if (!(((bool) (((bool) ((guid242).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint16_t flagNum = (guid242).appearance();
            
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
            Ble::servicet guid243 = serv;
            if (!(((bool) (((bool) ((guid243).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid243).service();
            
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
    juniper::rcptr test() {
        return (([&]() -> juniper::rcptr {
            void * myPtr;
            
            (([&]() -> juniper::unit {
                myPtr = (void *) new int(10);
                return {};
            })());
            return makerc(myPtr, juniper::function<void, juniper::unit(void *)>([](void * p) -> juniper::unit { 
                return (([&]() -> juniper::unit {
                    delete ((int *) p);
                    return {};
                })());
             }));
        })());
    }
}

namespace CWatch {
    juniper::refcell<uint32_t> foo = (juniper::refcell<uint32_t>(((uint32_t) 0)));
}

namespace CWatch {
    juniper::unit setup() {
        return (([&]() -> juniper::unit {
            (*((foo).get()) = ((uint32_t) 10));
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
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid244;
        guid244.r = ((uint8_t) 252);
        guid244.g = ((uint8_t) 92);
        guid244.b = ((uint8_t) 125);
        return guid244;
    })());
}

namespace CWatch {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> purpleBlue = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid245;
        guid245.r = ((uint8_t) 106);
        guid245.g = ((uint8_t) 130);
        guid245.b = ((uint8_t) 251);
        return guid245;
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
            CWatch::month guid246 = m;
            return (((bool) (((bool) ((guid246).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 31);
                })())
            :
                (((bool) (((bool) ((guid246).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return (isLeapYear(y) ? 
                            ((uint8_t) 29)
                        :
                            ((uint8_t) 28));
                    })())
                :
                    (((bool) (((bool) ((guid246).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 31);
                        })())
                    :
                        (((bool) (((bool) ((guid246).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> uint8_t {
                                return ((uint8_t) 30);
                            })())
                        :
                            (((bool) (((bool) ((guid246).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> uint8_t {
                                    return ((uint8_t) 31);
                                })())
                            :
                                (((bool) (((bool) ((guid246).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> uint8_t {
                                        return ((uint8_t) 30);
                                    })())
                                :
                                    (((bool) (((bool) ((guid246).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> uint8_t {
                                            return ((uint8_t) 31);
                                        })())
                                    :
                                        (((bool) (((bool) ((guid246).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> uint8_t {
                                                return ((uint8_t) 31);
                                            })())
                                        :
                                            (((bool) (((bool) ((guid246).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> uint8_t {
                                                    return ((uint8_t) 30);
                                                })())
                                            :
                                                (((bool) (((bool) ((guid246).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> uint8_t {
                                                        return ((uint8_t) 31);
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid246).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> uint8_t {
                                                            return ((uint8_t) 30);
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid246).id() == ((uint8_t) 11))) && true)) ? 
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
            CWatch::month guid247 = m;
            return (((bool) (((bool) ((guid247).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::month {
                    return february();
                })())
            :
                (((bool) (((bool) ((guid247).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::month {
                        return march();
                    })())
                :
                    (((bool) (((bool) ((guid247).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::month {
                            return april();
                        })())
                    :
                        (((bool) (((bool) ((guid247).id() == ((uint8_t) 4))) && true)) ? 
                            (([&]() -> CWatch::month {
                                return june();
                            })())
                        :
                            (((bool) (((bool) ((guid247).id() == ((uint8_t) 5))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return july();
                                })())
                            :
                                (((bool) (((bool) ((guid247).id() == ((uint8_t) 7))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return august();
                                    })())
                                :
                                    (((bool) (((bool) ((guid247).id() == ((uint8_t) 8))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return october();
                                        })())
                                    :
                                        (((bool) (((bool) ((guid247).id() == ((uint8_t) 9))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return november();
                                            })())
                                        :
                                            (((bool) (((bool) ((guid247).id() == ((uint8_t) 11))) && true)) ? 
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
            CWatch::dayOfWeek guid248 = dw;
            return (((bool) (((bool) ((guid248).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> CWatch::dayOfWeek {
                    return monday();
                })())
            :
                (((bool) (((bool) ((guid248).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> CWatch::dayOfWeek {
                        return tuesday();
                    })())
                :
                    (((bool) (((bool) ((guid248).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> CWatch::dayOfWeek {
                            return wednesday();
                        })())
                    :
                        (((bool) (((bool) ((guid248).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> CWatch::dayOfWeek {
                                return thursday();
                            })())
                        :
                            (((bool) (((bool) ((guid248).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return friday();
                                })())
                            :
                                (((bool) (((bool) ((guid248).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return saturday();
                                    })())
                                :
                                    (((bool) (((bool) ((guid248).id() == ((uint8_t) 6))) && true)) ? 
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
            CWatch::dayOfWeek guid249 = dw;
            return (((bool) (((bool) ((guid249).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid250;
                        guid250.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid250.length = ((uint32_t) 4);
                        return guid250;
                    })());
                })())
            :
                (((bool) (((bool) ((guid249).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid251;
                            guid251.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 111), ((uint8_t) 110), ((uint8_t) 0)} });
                            guid251.length = ((uint32_t) 4);
                            return guid251;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid249).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid252;
                                guid252.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 117), ((uint8_t) 101), ((uint8_t) 0)} });
                                guid252.length = ((uint32_t) 4);
                                return guid252;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid249).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid253;
                                    guid253.data = (juniper::array<uint8_t, 4> { {((uint8_t) 87), ((uint8_t) 101), ((uint8_t) 100), ((uint8_t) 0)} });
                                    guid253.length = ((uint32_t) 4);
                                    return guid253;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid249).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid254;
                                        guid254.data = (juniper::array<uint8_t, 4> { {((uint8_t) 84), ((uint8_t) 104), ((uint8_t) 117), ((uint8_t) 0)} });
                                        guid254.length = ((uint32_t) 4);
                                        return guid254;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid249).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid255;
                                            guid255.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 114), ((uint8_t) 105), ((uint8_t) 0)} });
                                            guid255.length = ((uint32_t) 4);
                                            return guid255;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid249).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid256;
                                                guid256.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 97), ((uint8_t) 116), ((uint8_t) 0)} });
                                                guid256.length = ((uint32_t) 4);
                                                return guid256;
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
            CWatch::month guid257 = m;
            return (((bool) (((bool) ((guid257).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid258;
                        guid258.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 97), ((uint8_t) 110), ((uint8_t) 0)} });
                        guid258.length = ((uint32_t) 4);
                        return guid258;
                    })());
                })())
            :
                (((bool) (((bool) ((guid257).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid259;
                            guid259.data = (juniper::array<uint8_t, 4> { {((uint8_t) 70), ((uint8_t) 101), ((uint8_t) 98), ((uint8_t) 0)} });
                            guid259.length = ((uint32_t) 4);
                            return guid259;
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid257).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid260;
                                guid260.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 114), ((uint8_t) 0)} });
                                guid260.length = ((uint32_t) 4);
                                return guid260;
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid257).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid261;
                                    guid261.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 112), ((uint8_t) 114), ((uint8_t) 0)} });
                                    guid261.length = ((uint32_t) 4);
                                    return guid261;
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid257).id() == ((uint8_t) 4))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid262;
                                        guid262.data = (juniper::array<uint8_t, 4> { {((uint8_t) 77), ((uint8_t) 97), ((uint8_t) 121), ((uint8_t) 0)} });
                                        guid262.length = ((uint32_t) 4);
                                        return guid262;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid257).id() == ((uint8_t) 5))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid263;
                                            guid263.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 110), ((uint8_t) 0)} });
                                            guid263.length = ((uint32_t) 4);
                                            return guid263;
                                        })());
                                    })())
                                :
                                    (((bool) (((bool) ((guid257).id() == ((uint8_t) 6))) && true)) ? 
                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid264;
                                                guid264.data = (juniper::array<uint8_t, 4> { {((uint8_t) 74), ((uint8_t) 117), ((uint8_t) 108), ((uint8_t) 0)} });
                                                guid264.length = ((uint32_t) 4);
                                                return guid264;
                                            })());
                                        })())
                                    :
                                        (((bool) (((bool) ((guid257).id() == ((uint8_t) 7))) && true)) ? 
                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid265;
                                                    guid265.data = (juniper::array<uint8_t, 4> { {((uint8_t) 65), ((uint8_t) 117), ((uint8_t) 103), ((uint8_t) 0)} });
                                                    guid265.length = ((uint32_t) 4);
                                                    return guid265;
                                                })());
                                            })())
                                        :
                                            (((bool) (((bool) ((guid257).id() == ((uint8_t) 8))) && true)) ? 
                                                (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                    return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                        juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid266;
                                                        guid266.data = (juniper::array<uint8_t, 4> { {((uint8_t) 83), ((uint8_t) 101), ((uint8_t) 112), ((uint8_t) 0)} });
                                                        guid266.length = ((uint32_t) 4);
                                                        return guid266;
                                                    })());
                                                })())
                                            :
                                                (((bool) (((bool) ((guid257).id() == ((uint8_t) 9))) && true)) ? 
                                                    (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                            juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid267;
                                                            guid267.data = (juniper::array<uint8_t, 4> { {((uint8_t) 79), ((uint8_t) 99), ((uint8_t) 116), ((uint8_t) 0)} });
                                                            guid267.length = ((uint32_t) 4);
                                                            return guid267;
                                                        })());
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid257).id() == ((uint8_t) 10))) && true)) ? 
                                                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid268;
                                                                guid268.data = (juniper::array<uint8_t, 4> { {((uint8_t) 78), ((uint8_t) 111), ((uint8_t) 118), ((uint8_t) 0)} });
                                                                guid268.length = ((uint32_t) 4);
                                                                return guid268;
                                                            })());
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid257).id() == ((uint8_t) 11))) && true)) ? 
                                                            (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> {
                                                                return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t>{
                                                                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid269;
                                                                    guid269.data = (juniper::array<uint8_t, 4> { {((uint8_t) 68), ((uint8_t) 101), ((uint8_t) 99), ((uint8_t) 0)} });
                                                                    guid269.length = ((uint32_t) 4);
                                                                    return guid269;
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
            juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid270 = d;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek = (guid270).dayOfWeek;
            uint8_t seconds = (guid270).seconds;
            uint8_t minutes = (guid270).minutes;
            uint8_t hours = (guid270).hours;
            uint32_t year = (guid270).year;
            uint8_t day = (guid270).day;
            CWatch::month month = (guid270).month;
            
            uint8_t guid271 = ((uint8_t) (seconds + ((uint8_t) 1)));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t seconds1 = guid271;
            
            juniper::tuple2<uint8_t, uint8_t> guid272 = (((bool) (seconds1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (minutes + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{seconds1, minutes}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t minutes1 = (guid272).e2;
            uint8_t seconds2 = (guid272).e1;
            
            juniper::tuple2<uint8_t, uint8_t> guid273 = (((bool) (minutes1 == ((uint8_t) 60))) ? 
                (juniper::tuple2<uint8_t,uint8_t>{((uint8_t) 0), ((uint8_t) (hours + ((uint8_t) 1)))})
            :
                (juniper::tuple2<uint8_t,uint8_t>{minutes1, hours}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t hours1 = (guid273).e2;
            uint8_t minutes2 = (guid273).e1;
            
            juniper::tuple3<uint8_t, uint8_t, CWatch::dayOfWeek> guid274 = (((bool) (hours1 == ((uint8_t) 24))) ? 
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{((uint8_t) 0), ((uint8_t) (day + ((uint8_t) 1))), nextDayOfWeek(dayOfWeek)})
            :
                (juniper::tuple3<uint8_t,uint8_t,CWatch::dayOfWeek>{hours1, day, dayOfWeek}));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            CWatch::dayOfWeek dayOfWeek2 = (guid274).e3;
            uint8_t day1 = (guid274).e2;
            uint8_t hours2 = (guid274).e1;
            
            uint8_t guid275 = daysInMonth(year, month);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t daysInCurrentMonth = guid275;
            
            juniper::tuple2<uint8_t, juniper::tuple2<CWatch::month, uint32_t>> guid276 = (((bool) (day1 > daysInCurrentMonth)) ? 
                (juniper::tuple2<uint8_t,juniper::tuple2<CWatch::month, uint32_t>>{((uint8_t) 1), (([&]() -> juniper::tuple2<CWatch::month, uint32_t> {
                    CWatch::month guid277 = month;
                    return (((bool) (((bool) ((guid277).id() == ((uint8_t) 11))) && true)) ? 
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
            uint32_t year2 = ((guid276).e2).e2;
            CWatch::month month2 = ((guid276).e2).e1;
            uint8_t day2 = (guid276).e1;
            
            return (([&]() -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
                juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid278;
                guid278.month = month2;
                guid278.day = day2;
                guid278.year = year2;
                guid278.hours = hours2;
                guid278.minutes = minutes2;
                guid278.seconds = seconds2;
                guid278.dayOfWeek = dayOfWeek2;
                return guid278;
            })());
        })());
    }
}

namespace CWatch {
    juniper::records::recordt_1<uint32_t> clockTickerState = Time::state;
}

namespace CWatch {
    juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> clockState = (([]() -> juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t>{
        juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid279;
        guid279.month = september();
        guid279.day = ((uint8_t) 9);
        guid279.year = ((uint32_t) 2020);
        guid279.hours = ((uint8_t) 18);
        guid279.minutes = ((uint8_t) 40);
        guid279.seconds = ((uint8_t) 0);
        guid279.dayOfWeek = wednesday();
        return guid279;
    })());
}

namespace CWatch {
    juniper::unit processBluetoothUpdates() {
        return (([&]() -> juniper::unit {
            bool guid280 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayDateTime = guid280;
            
            bool guid281 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool hasNewDayOfWeek = guid281;
            
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
                        juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> guid282 = Ble::readGeneric<juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>>(dayDateTimeCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_7<uint8_t, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t> bleData = guid282;
                        
                        ((clockState).month = (([&]() -> CWatch::month {
                            uint8_t guid283 = (bleData).month;
                            return (((bool) (((bool) (guid283 == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> CWatch::month {
                                    return january();
                                })())
                            :
                                (((bool) (((bool) (guid283 == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::month {
                                        return february();
                                    })())
                                :
                                    (((bool) (((bool) (guid283 == ((uint8_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::month {
                                            return march();
                                        })())
                                    :
                                        (((bool) (((bool) (guid283 == ((uint8_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::month {
                                                return april();
                                            })())
                                        :
                                            (((bool) (((bool) (guid283 == ((uint8_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::month {
                                                    return may();
                                                })())
                                            :
                                                (((bool) (((bool) (guid283 == ((uint8_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::month {
                                                        return june();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid283 == ((uint8_t) 6))) && true)) ? 
                                                        (([&]() -> CWatch::month {
                                                            return july();
                                                        })())
                                                    :
                                                        (((bool) (((bool) (guid283 == ((uint8_t) 7))) && true)) ? 
                                                            (([&]() -> CWatch::month {
                                                                return august();
                                                            })())
                                                        :
                                                            (((bool) (((bool) (guid283 == ((uint8_t) 8))) && true)) ? 
                                                                (([&]() -> CWatch::month {
                                                                    return september();
                                                                })())
                                                            :
                                                                (((bool) (((bool) (guid283 == ((uint8_t) 9))) && true)) ? 
                                                                    (([&]() -> CWatch::month {
                                                                        return october();
                                                                    })())
                                                                :
                                                                    (((bool) (((bool) (guid283 == ((uint8_t) 10))) && true)) ? 
                                                                        (([&]() -> CWatch::month {
                                                                            return november();
                                                                        })())
                                                                    :
                                                                        (((bool) (((bool) (guid283 == ((uint8_t) 11))) && true)) ? 
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
                        ((clockState).day = (bleData).day);
                        ((clockState).year = (bleData).year);
                        ((clockState).hours = (bleData).hours);
                        ((clockState).minutes = (bleData).minutes);
                        return ((clockState).seconds = (bleData).seconds);
                    })());
                }
                return {};
            })());
            return (([&]() -> juniper::unit {
                if (hasNewDayOfWeek) {
                    (([&]() -> CWatch::dayOfWeek {
                        juniper::records::recordt_8<uint8_t> guid284 = Ble::readGeneric<juniper::records::recordt_8<uint8_t>>(dayOfWeekCharacterstic);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_8<uint8_t> bleData = guid284;
                        
                        return ((clockState).dayOfWeek = (([&]() -> CWatch::dayOfWeek {
                            uint8_t guid285 = (bleData).dayOfWeek;
                            return (((bool) (((bool) (guid285 == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> CWatch::dayOfWeek {
                                    return sunday();
                                })())
                            :
                                (((bool) (((bool) (guid285 == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> CWatch::dayOfWeek {
                                        return monday();
                                    })())
                                :
                                    (((bool) (((bool) (guid285 == ((uint8_t) 2))) && true)) ? 
                                        (([&]() -> CWatch::dayOfWeek {
                                            return tuesday();
                                        })())
                                    :
                                        (((bool) (((bool) (guid285 == ((uint8_t) 3))) && true)) ? 
                                            (([&]() -> CWatch::dayOfWeek {
                                                return wednesday();
                                            })())
                                        :
                                            (((bool) (((bool) (guid285 == ((uint8_t) 4))) && true)) ? 
                                                (([&]() -> CWatch::dayOfWeek {
                                                    return thursday();
                                                })())
                                            :
                                                (((bool) (((bool) (guid285 == ((uint8_t) 5))) && true)) ? 
                                                    (([&]() -> CWatch::dayOfWeek {
                                                        return friday();
                                                    })())
                                                :
                                                    (((bool) (((bool) (guid285 == ((uint8_t) 6))) && true)) ? 
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
            int16_t guid286 = toInt16<uint16_t>(Arcada::displayWidth());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispW = guid286;
            
            int16_t guid287 = toInt16<uint16_t>(Arcada::displayHeight());
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t dispH = guid287;
            
            int16_t guid288 = Math::clamp<int16_t>(x0i, ((int16_t) 0), ((int16_t) (dispW - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t x0 = guid288;
            
            int16_t guid289 = Math::clamp<int16_t>(y0i, ((int16_t) 0), ((int16_t) (dispH - ((int16_t) 1))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y0 = guid289;
            
            int16_t guid290 = ((int16_t) (y0i + h));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t ymax = guid290;
            
            int16_t guid291 = Math::clamp<int16_t>(ymax, ((int16_t) 0), dispH);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int16_t y1 = guid291;
            
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid292 = c1;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b1 = (guid292).b;
            uint8_t g1 = (guid292).g;
            uint8_t r1 = (guid292).r;
            
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid293 = c2;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b2 = (guid293).b;
            uint8_t g2 = (guid293).g;
            uint8_t r2 = (guid293).r;
            
            float guid294 = toFloat<uint8_t>(r1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r1f = guid294;
            
            float guid295 = toFloat<uint8_t>(g1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g1f = guid295;
            
            float guid296 = toFloat<uint8_t>(b1);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b1f = guid296;
            
            float guid297 = toFloat<uint8_t>(r2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r2f = guid297;
            
            float guid298 = toFloat<uint8_t>(g2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g2f = guid298;
            
            float guid299 = toFloat<uint8_t>(b2);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b2f = guid299;
            
            return (([&]() -> juniper::unit {
                int16_t guid300 = y0;
                int16_t guid301 = y1;
                for (int16_t y = guid300; y < guid301; y++) {
                    (([&]() -> juniper::unit {
                        float guid302 = ((float) (toFloat<int16_t>(((int16_t) (ymax - y))) / toFloat<int16_t>(h)));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight1 = guid302;
                        
                        float guid303 = ((float) (1.0f - weight1));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        float weight2 = guid303;
                        
                        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid304 = (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
                            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid305;
                            guid305.r = toUInt8<float>(((float) (((float) (r1f * weight1)) + ((float) (r2f * weight2)))));
                            guid305.g = toUInt8<float>(((float) (((float) (g1f * weight1)) + ((float) (g2f * weight2)))));
                            guid305.b = toUInt8<float>(((float) (((float) (b1f * weight1)) + ((float) (g2f * weight2)))));
                            return guid305;
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> gradColor = guid304;
                        
                        uint16_t guid306 = Color::rgbToRgb565(gradColor);
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint16_t gradColor565 = guid306;
                        
                        return drawFastHLine565(x0, y, w, gradColor565);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace Gfx {
    template<int c229>
    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> getCharListBounds(int16_t x, int16_t y, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c229)>, uint32_t> cl) {
        return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
            constexpr int32_t n = c229;
            return (([&]() -> juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> {
                int16_t guid307 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t xret = guid307;
                
                int16_t guid308 = ((int16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t yret = guid308;
                
                uint16_t guid309 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t wret = guid309;
                
                uint16_t guid310 = ((uint16_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t hret = guid310;
                
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
    template<int c230>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c230)>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c230;
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
            Gfx::font guid311 = f;
            return (((bool) (((bool) ((guid311).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> juniper::unit {
                    return (([&]() -> juniper::unit {
                        arcada.getCanvas()->setFont();
                        return {};
                    })());
                })())
            :
                (((bool) (((bool) ((guid311).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> juniper::unit {
                        return (([&]() -> juniper::unit {
                            arcada.getCanvas()->setFont(&FreeSans9pt7b);
                            return {};
                        })());
                    })())
                :
                    (((bool) (((bool) ((guid311).id() == ((uint8_t) 2))) && true)) ? 
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
            uint16_t guid312 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid312;
            
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
                    juniper::records::recordt_9<uint8_t, CWatch::dayOfWeek, uint8_t, uint8_t, CWatch::month, uint8_t, uint32_t> guid313 = dt;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    CWatch::dayOfWeek dayOfWeek = (guid313).dayOfWeek;
                    uint8_t seconds = (guid313).seconds;
                    uint8_t minutes = (guid313).minutes;
                    uint8_t hours = (guid313).hours;
                    uint32_t year = (guid313).year;
                    uint8_t day = (guid313).day;
                    CWatch::month month = (guid313).month;
                    
                    int32_t guid314 = toInt32<uint8_t>((((bool) (hours == ((uint8_t) 0))) ? 
                        ((uint8_t) 12)
                    :
                        (((bool) (hours > ((uint8_t) 12))) ? 
                            ((uint8_t) (hours - ((uint8_t) 12)))
                        :
                            hours)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    int32_t displayHours = guid314;
                    
                    Gfx::setTextColor(Color::white);
                    Gfx::setFont(Gfx::freeSans24());
                    Gfx::setTextSize(((uint8_t) 1));
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid315 = CharList::i32ToCharList<2>(displayHours);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> timeHourStr = guid315;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> guid316 = CharList::safeConcat<2, 1>(timeHourStr, (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid317;
                        guid317.data = (juniper::array<uint8_t, 2> { {((uint8_t) 58), ((uint8_t) 0)} });
                        guid317.length = ((uint32_t) 2);
                        return guid317;
                    })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 4>, uint32_t> timeHourStrColon = guid316;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid318 = (((bool) (minutes < ((uint8_t) 10))) ? 
                        CharList::safeConcat<1, 1>((([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid319;
                            guid319.data = (juniper::array<uint8_t, 2> { {((uint8_t) 48), ((uint8_t) 0)} });
                            guid319.length = ((uint32_t) 2);
                            return guid319;
                        })()), CharList::i32ToCharList<1>(toInt32<uint8_t>(minutes)))
                    :
                        CharList::i32ToCharList<2>(toInt32<uint8_t>(minutes)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> minutesStr = guid318;
                    
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> guid320 = CharList::safeConcat<3, 2>(timeHourStrColon, minutesStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 6>, uint32_t> timeStr = guid320;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid321 = Gfx::getCharListBounds<5>(((int16_t) 0), ((int16_t) 0), timeStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h = (guid321).e4;
                    uint16_t w = (guid321).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2)))))));
                    Gfx::printCharList<5>(timeStr);
                    Gfx::setTextSize(((uint8_t) 1));
                    Gfx::setFont(Gfx::freeSans9());
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid322 = (((bool) (hours < ((uint8_t) 12))) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid323;
                            guid323.data = (juniper::array<uint8_t, 3> { {((uint8_t) 65), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid323.length = ((uint32_t) 3);
                            return guid323;
                        })())
                    :
                        (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                            juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid324;
                            guid324.data = (juniper::array<uint8_t, 3> { {((uint8_t) 80), ((uint8_t) 77), ((uint8_t) 0)} });
                            guid324.length = ((uint32_t) 3);
                            return guid324;
                        })()));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> ampm = guid322;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid325 = Gfx::getCharListBounds<2>(((int16_t) 0), ((int16_t) 0), ampm);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h2 = (guid325).e4;
                    uint16_t w2 = (guid325).e3;
                    
                    Gfx::setCursor(toInt16<uint16_t>(((uint16_t) (((uint16_t) (Arcada::displayWidth() / ((uint16_t) 2))) - ((uint16_t) (w2 / ((uint16_t) 2)))))), toInt16<uint16_t>(((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (Arcada::displayHeight() / ((uint16_t) 2))) + ((uint16_t) (h / ((uint16_t) 2))))) + h2)) + ((uint16_t) 5)))));
                    Gfx::printCharList<2>(ampm);
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> guid326 = CharList::safeConcat<9, 2>(CharList::safeConcat<8, 1>(CharList::safeConcat<5, 3>(CharList::safeConcat<3, 2>(dayOfWeekToCharList(dayOfWeek), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 3>, uint32_t> guid327;
                        guid327.data = (juniper::array<uint8_t, 3> { {((uint8_t) 44), ((uint8_t) 32), ((uint8_t) 0)} });
                        guid327.length = ((uint32_t) 3);
                        return guid327;
                    })())), monthToCharList(month)), (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<uint8_t, 2>, uint32_t> guid328;
                        guid328.data = (juniper::array<uint8_t, 2> { {((uint8_t) 32), ((uint8_t) 0)} });
                        guid328.length = ((uint32_t) 2);
                        return guid328;
                    })())), CharList::i32ToCharList<2>(toInt32<uint8_t>(day)));
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_0<juniper::array<uint8_t, 12>, uint32_t> dateStr = guid326;
                    
                    juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid329 = Gfx::getCharListBounds<11>(((int16_t) 0), ((int16_t) 0), dateStr);
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint16_t h3 = (guid329).e4;
                    uint16_t w3 = (guid329).e3;
                    
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
            uint16_t guid330 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid330;
            
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
            uint16_t guid331 = rgbToRgb565(c);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint16_t cPrime = guid331;
            
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
    template<int c255>
    juniper::unit centerCursor(int16_t x, int16_t y, Gfx::align align, juniper::records::recordt_0<juniper::array<uint8_t, c255>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c255;
            return (([&]() -> juniper::unit {
                juniper::tuple4<int16_t, int16_t, uint16_t, uint16_t> guid332 = Gfx::getCharListBounds<(-1)+(c255)>(((int16_t) 0), ((int16_t) 0), cl);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint16_t h = (guid332).e4;
                uint16_t w = (guid332).e3;
                
                int16_t guid333 = toInt16<uint16_t>(w);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t ws = guid333;
                
                int16_t guid334 = toInt16<uint16_t>(h);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int16_t hs = guid334;
                
                return (([&]() -> juniper::unit {
                    Gfx::align guid335 = align;
                    return (((bool) (((bool) ((guid335).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            return Gfx::setCursor(((int16_t) (x - ((int16_t) (ws / ((int16_t) 2))))), y);
                        })())
                    :
                        (((bool) (((bool) ((guid335).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::unit {
                                return Gfx::setCursor(x, ((int16_t) (y - ((int16_t) (hs / ((int16_t) 2))))));
                            })())
                        :
                            (((bool) (((bool) ((guid335).id() == ((uint8_t) 2))) && true)) ? 
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
