/***************************************************************************************************
 * Copyright 2025 NVIDIA Corporation. All rights reserved.
 **************************************************************************************************/
/// \file  mi/base/atom.h
/// \brief 32-bit unsigned counter with atomic arithmetic, increments, and decrements.
///
/// See \ref mi_base_threads.

#ifndef MI_BASE_ATOM_H
#define MI_BASE_ATOM_H

#include <mi/base/config.h>
#include <mi/base/types.h>

// Select implementation to use
#if defined( MI_ARCH_X86) && (defined( MI_COMPILER_GCC) || defined( MI_COMPILER_ICC))
#  define MI_ATOM32_X86GCC
#else
#  include <atomic>
#endif

namespace mi {

namespace base {

/** \addtogroup mi_base_threads
@{
*/

/// A 32-bit unsigned counter with atomic arithmetic, increments, and decrements.
class Atom32
{
public:
    /// The default constructor initializes the counter to zero.
    Atom32() : m_value( 0) { }

    /// This constructor initializes the counter to \p value.
    Atom32( const Uint32 value) : m_value( value) { }

#ifndef MI_ATOM32_X86GCC
    /// The copy constructor assigns the value of \p other to the counter.
    Atom32( const Atom32& other);

    /// Assigns the value of \p rhs to the counter.
    Atom32& operator=( const Atom32& rhs);
#endif

    /// Assigns \p rhs to the counter.
    Uint32 operator=( const Uint32 rhs) { m_value = rhs; return rhs; }

    /// Adds \p rhs to the counter.
    Uint32 operator+=( const Uint32 rhs);

    /// Subtracts \p rhs from the counter.
    Uint32 operator-=( const Uint32 rhs);

    /// Increments the counter by one (pre-increment).
    Uint32 operator++();

    /// Increments the counter by one (post-increment).
    Uint32 operator++( int);

    /// Decrements the counter by one (pre-decrement).
    Uint32 operator--();

    /// Decrements the counter by one (post-decrement).
    Uint32 operator--( int);

    /// Conversion operator to #mi::Uint32.
    operator Uint32() const { return m_value; }

    /// Assigns \p rhs to the counter and returns the old value of counter.
    Uint32 swap( const Uint32 rhs);

private:
#ifdef MI_ATOM32_X86GCC
    // The counter.
    volatile Uint32 m_value;
#else
    // The counter.
    std::atomic_uint32_t m_value;
#endif
};

#ifndef MI_FOR_DOXYGEN_ONLY

#ifdef MI_ATOM32_X86GCC

inline Uint32 Atom32::operator+=( const Uint32 rhs)
{
    Uint32 retval;
    asm volatile(
        "movl %2,%0\n"
        "lock; xaddl %0,%1\n"
        "addl %2,%0\n"
        : "=&r"( retval), "+m"( m_value)
        : "r"( rhs)
        : "cc"
        );
    return retval;
}

inline Uint32 Atom32::operator-=( const Uint32 rhs)
{
    Uint32 retval;
    asm volatile(
        "neg %2\n"
        "movl %2,%0\n"
        "lock; xaddl %0,%1\n"
        "addl %2,%0\n"
        : "=&r"( retval), "+m"( m_value)
        : "r"( rhs)
        : "cc", "%2"
        );
    return retval;
}

inline Uint32 Atom32::operator++()
{
    Uint32 retval;
    asm volatile(
        "movl $1,%0\n"
        "lock; xaddl %0,%1\n"
        "addl $1,%0\n"
        : "=&r"( retval), "+m"( m_value)
        :
        : "cc"
        );
    return retval;
}

inline Uint32 Atom32::operator++( int)
{
    Uint32 retval;
    asm volatile(
        "movl $1,%0\n"
        "lock; xaddl %0,%1\n"
        : "=&r"( retval), "+m"( m_value)
        :
        : "cc"
        );
    return retval;
}

inline Uint32 Atom32::operator--()
{
    Uint32 retval;
    asm volatile(
        "movl $-1,%0\n"
        "lock; xaddl %0,%1\n"
        "addl $-1,%0\n"
        : "=&r"( retval), "+m"( m_value)
        :
        : "cc"
        );
    return retval;
}

inline Uint32 Atom32::operator--( int)
{
    Uint32 retval;
    asm volatile(
        "movl $-1,%0\n"
        "lock; xaddl %0,%1\n"
        : "=&r"( retval), "+m"( m_value)
        :
        : "cc"
        );
    return retval;
}

inline Uint32 Atom32::swap( const Uint32 rhs)
{
    Uint32 retval;
    asm volatile(
    "0:\n"
        "movl %1,%0\n"
        "lock; cmpxchg %2,%1\n"
        "jnz 0b\n"
        : "=&a"( retval), "+m"( m_value)
        : "r"( rhs)
        : "cc"
        );
    return retval;
}

#else

inline Atom32::Atom32( const Atom32& other) : m_value( other.m_value.load()) { }

inline Atom32& Atom32::operator=( const Atom32& rhs)
{
    m_value = rhs.m_value.load();
    return *this;
}

inline Uint32 Atom32::operator+=( const Uint32 rhs)
{
    m_value += rhs;
    return m_value;
}

inline Uint32 Atom32::operator-=( const Uint32 rhs)
{
    m_value -= rhs;
    return m_value;
}

inline Uint32 Atom32::operator++()
{
    return ++m_value;
}

inline Uint32 Atom32::operator++( int)
{
    return m_value++;
}

inline Uint32 Atom32::operator--()
{
    return --m_value;
}

inline Uint32 Atom32::operator--( int)
{
    return m_value--;
}

inline Uint32 Atom32::swap( const Uint32 rhs)
{
    return m_value.exchange( rhs);
}

#endif // MI_ATOM32_X86GCC

#undef MI_ATOM32_X86GCC

#endif // !MI_FOR_DOXYGEN_ONLY

/*@}*/ // end group mi_base_threads

} // namespace base

} // namespace mi

#endif // MI_BASE_ATOM_H
