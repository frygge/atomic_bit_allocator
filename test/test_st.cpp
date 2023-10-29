//
// Created by Joerg P. Schaefer on 29.10.2023.
//

#include <chrono>
#include <bitset>
#include <cstring>
#include "atomic_bit_allocator.h"

using namespace std::chrono_literals;


template<typename W, size_t N>
struct bit_allocator_buffer {
    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        for( const auto b: buf )
            ss << std::bitset( b ).to_string();

        return ss.str();
    }

    size_t end_pos_;
    W buf[(N + sizeof( W ) - 1) / sizeof( W )];
};


template<typename W, size_t N_>
void bufferlen_tests() {
    bit_allocator_buffer<W, N_> buffer;
    std::memset( &buffer, 0, sizeof( buffer ));
    auto* ballocator = new ( &buffer ) jps::serialized_bit_allocator<W>( sizeof( buffer ));
    const auto N = ballocator->size();
    const auto NW = N / ( 8*sizeof( W ));

    {
        assert( buffer.end_pos_ >= 8 * sizeof( W ) * N_ );
        assert( buffer.end_pos_ <= 8*( sizeof( buffer ) - sizeof( size_t ) ) );

        assert( N == buffer.end_pos_ );
    }

    for( auto w = 0ul; w < NW; ++w ) {
        assert( buffer.buf[w] == 0 );
    }
}

template<typename T, size_t N>
struct loop_bufferlen_tests {
    void operator()() {
        loop_bufferlen_tests<T, N - 1>();
        bufferlen_tests<T, N>();
    }
};

template<typename T>
struct loop_bufferlen_tests<T, 1> {
    void operator()() {
        bufferlen_tests<T, 1>();
    }
};


template<size_t N_>
void simple_tests_uint8() {
    using W = uint8_t;

    bit_allocator_buffer<W, N_> buffer{ 0, { 0 }};
    std::memset( &buffer, 0, sizeof( buffer ));
    auto* ballocator = new ( &buffer ) jps::serialized_bit_allocator<W>( sizeof( buffer ));
    const auto N = ballocator->size();
    const auto NW = N / ( 8*sizeof( W ));

    for( auto w = 0ul; w < NW; ++w ) {
        assert( buffer.buf[w] == 0 );
    }

    size_t p1, p2, p3;

    p1 = ballocator->alloc( 2 );
    assert( p1 == 0 );
    assert( buffer.buf[0] == 0b11000000 );
    assert( buffer.buf[1] == 0b00000000 );

    p2 = ballocator->alloc( 1 );
    assert( p2 == 2 );
    assert( buffer.buf[0] == 0b11100000 );
    assert( buffer.buf[1] == 0b00000000 );

    p3 = ballocator->alloc( 5 );
    assert( p3 == 3 );
    assert( buffer.buf[0] == 0b11111111 );
    assert( buffer.buf[1] == 0b00000000 );

    // reallocate p1
    {
        ballocator->free( p1, 2 );
        assert( buffer.buf[0] == 0b00111111 );
        assert( buffer.buf[1] == 0b00000000 );

        p1 = ballocator->alloc( 2 );
        assert( p1 == 0 );
        assert( buffer.buf[0] == 0b11111111 );
        assert( buffer.buf[1] == 0b00000000 );
    }

    // reallocate p2
    {
        ballocator->free( p2, 1 );
        assert( buffer.buf[0] == 0b11011111 );
        assert( buffer.buf[1] == 0b00000000 );

        p2 = ballocator->alloc( 1 );
        assert( p2 == 2 );
        assert( buffer.buf[0] == 0b11111111 );
        assert( buffer.buf[1] == 0b00000000 );
    }

    // free p3
    {
        ballocator->free( p3, 5 );
        assert( buffer.buf[0] == 0b11100000 );
        assert( buffer.buf[1] == 0b00000000 );
    }

    // allocate larger area for p3
    {
        p3 = ballocator->alloc( 16 );
        assert( p3 == 3 );
        assert( buffer.buf[0] == 0b11111111 );
        assert( buffer.buf[1] == 0b11111111 );
        assert( buffer.buf[2] == 0b11100000 );
    }

    // reallocate p2 but larger
    {
        ballocator->free( p2, 1 );
        assert( buffer.buf[0] == 0b11011111 );
        assert( buffer.buf[1] == 0b11111111 );
        assert( buffer.buf[2] == 0b11100000 );

        p2 = ballocator->alloc( 2 );
        assert( p2 == 19 );
        assert( buffer.buf[0] == 0b11011111 );
        assert( buffer.buf[1] == 0b11111111 );
        assert( buffer.buf[2] == 0b11111000 );
    }
}

template<size_t N_>
void simple_tests_uint16() {
    using W = uint16_t;

    bit_allocator_buffer<W, N_> buffer{ 0, { 0 }};
    std::memset( &buffer, 0, sizeof( buffer ));
    auto* ballocator = new ( &buffer ) jps::serialized_bit_allocator<W>( sizeof( buffer ));
    const auto N = ballocator->size();
    const auto NW = N / ( 8*sizeof( W ));

    for( auto w = 0ul; w < NW; ++w ) {
        assert( buffer.buf[w] == 0 );
    }

    size_t p1, p2, p3;

    p1 = ballocator->alloc( 5 );
    assert( p1 == 0 );
    assert( buffer.buf[0] == 0b11111000'00000000 );
    assert( buffer.buf[1] == 0b00000000'00000000 );

    p2 = ballocator->alloc( 2 );
    assert( p2 == 5 );
    assert( buffer.buf[0] == 0b11111110'00000000 );
    assert( buffer.buf[1] == 0b00000000'00000000 );

    p3 = ballocator->alloc( 9 );
    assert( p3 == 7 );
    assert( buffer.buf[0] == 0b11111111'11111111 );
    assert( buffer.buf[1] == 0b00000000'00000000 );

    // reallocate p1
    {
        ballocator->free( p1, 5 );
        assert( buffer.buf[0] == 0b00000111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000 );

        p1 = ballocator->alloc( 5 );
        assert( p1 == 0 );
        assert( buffer.buf[0] == 0b11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000 );
    }

    // reallocate p2
    {
        ballocator->free( p2, 2 );
        assert( buffer.buf[0] == 0b11111001'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000 );

        p2 = ballocator->alloc( 2 );
        assert( p2 == 5 );
        assert( buffer.buf[0] == 0b11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000 );
    }

    // free p3
    {
        ballocator->free( p3, 9 );
        assert( buffer.buf[0] == 0b11111110'00000000 );
        assert( buffer.buf[1] == 0b00000000'00000000 );
    }

    // allocate larger area for p3
    {
        p3 = ballocator->alloc( 9 + 16 + 3 );
        assert( p3 == 7 );
        assert( buffer.buf[0] == 0b11111111'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111 );
        assert( buffer.buf[2] == 0b11100000'00000000 );
    }

    // reallocate p2 but larger
    {
        ballocator->free( p2, 2 );
        assert( buffer.buf[0] == 0b11111001'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111 );
        assert( buffer.buf[2] == 0b11100000'00000000 );

        p2 = ballocator->alloc( 3 );
        assert( p2 == 35 );
        assert( buffer.buf[0] == 0b11111001'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111 );
        assert( buffer.buf[2] == 0b11111100'00000000 );
    }
}

template<size_t N_>
void simple_tests_uint32() {
    using W = uint32_t;

    bit_allocator_buffer<W, N_> buffer{ 0, { 0 }};
    std::memset( &buffer, 0, sizeof( buffer ));
    auto* ballocator = new ( &buffer ) jps::serialized_bit_allocator<W>( sizeof( buffer ));
    const auto N = ballocator->size();
    const auto NW = N / ( 8*sizeof( W ));

    for( auto w = 0ul; w < NW; ++w ) {
        assert( buffer.buf[w] == 0 );
    }

    size_t p1, p2, p3;

    p1 = ballocator->alloc( 17 );
    assert( p1 == 0 );
    assert( buffer.buf[0] == 0b11111111'11111111'10000000'00000000 );
    assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );

    p2 = ballocator->alloc( 7 );
    assert( p2 == 17 );
    assert( buffer.buf[0] == 0b11111111'11111111'11111111'00000000 );
    assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );

    p3 = ballocator->alloc( 8 );
    assert( p3 == 24 );
    assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111 );
    assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );

    // reallocate p1
    {
        ballocator->free( p1, 17 );
        assert( buffer.buf[0] == 0b00000000'00000000'01111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );

        p1 = ballocator->alloc( 17 );
        assert( p1 == 0 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );
    }

    // reallocate p2
    {
        ballocator->free( p2, 7 );
        assert( buffer.buf[0] == 0b11111111'11111111'10000000'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );

        p2 = ballocator->alloc( 7 );
        assert( p2 == 17 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );
    }

    // free p3
    {
        ballocator->free( p3, 8 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'00000000 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000 );
    }

    // allocate larger area for p3
    {
        p3 = ballocator->alloc( 8 + 32 + 3 );
        assert( p3 == 24 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111'11111111'11111111 );
        assert( buffer.buf[2] == 0b11100000'00000000'00000000'00000000 );
    }

    // reallocate p2 but larger
    {
        ballocator->free( p2, 7 );
        assert( buffer.buf[0] == 0b11111111'11111111'10000000'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111'11111111'11111111 );
        assert( buffer.buf[2] == 0b11100000'00000000'00000000'00000000 );

        p2 = ballocator->alloc( 8 );
        assert( p2 == 67 );
        assert( buffer.buf[0] == 0b11111111'11111111'10000000'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111'11111111'11111111 );
        assert( buffer.buf[2] == 0b11111111'11100000'00000000'00000000 );
    }
}

template<size_t N_>
void simple_tests_uint64() {
    using W = uint64_t;

    bit_allocator_buffer<W, N_> buffer;
    std::memset( &buffer, 0, sizeof( buffer ));
    auto* ballocator = new ( &buffer ) jps::serialized_bit_allocator<W>( sizeof( buffer ));
    const auto N = ballocator->size();
    const auto NW = N / ( 8*sizeof( W ));

    for( auto w = 0ul; w < NW; ++w ) {
        assert( buffer.buf[w] == 0 );
    }

    size_t p1, p2, p3;

    p1 = ballocator->alloc( 17 );
    assert( p1 == 0 );
    assert( buffer.buf[0] == 0b11111111'11111111'10000000'00000000'00000000'00000000'00000000'00000000 );
    assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );

    p2 = ballocator->alloc( 7 );
    assert( p2 == 17 );
    assert( buffer.buf[0] == 0b11111111'11111111'11111111'00000000'00000000'00000000'00000000'00000000 );
    assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );

    p3 = ballocator->alloc(40 );
    assert( p3 == 24 );
    assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 );
    assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );

    // reallocate p1
    {
        ballocator->free( p1, 17 );
        assert( buffer.buf[0] == 0b00000000'00000000'01111111'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );

        p1 = ballocator->alloc( 17 );
        assert( p1 == 0 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );
    }

    // reallocate p2
    {
        ballocator->free( p2, 7 );
        assert( buffer.buf[0] == 0b11111111'11111111'10000000'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );

        p2 = ballocator->alloc( 7 );
        assert( p2 == 17 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );
    }

    // free p3
    {
        ballocator->free( p3, 40 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'00000000'00000000'00000000'00000000'00000000 );
        assert( buffer.buf[1] == 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );
    }

    // allocate larger area for p3
    {
        p3 = ballocator->alloc( 40 + 64 + 3 );
        assert( p3 == 24 );
        assert( buffer.buf[0] == 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[2] == 0b11100000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );
    }

    // reallocate p2 but larger
    {
        ballocator->free( p2, 7 );
        assert( buffer.buf[0] == 0b11111111'11111111'10000000'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[2] == 0b11100000'00000000'00000000'00000000'00000000'00000000'00000000'00000000 );

        p2 = ballocator->alloc( 8 );
        assert( p2 == 131 );
        assert( buffer.buf[0] == 0b11111111'11111111'10000000'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[1] == 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111 );
        assert( buffer.buf[2] == 0b11111111'11100000'00000000'00000000'00000000'00000000'00000000'00000000 );
    }
}


int main( [[maybe_unused]] int argc, [[maybe_unused]] char* argv[] ) {
    {
        loop_bufferlen_tests<uint8_t, 8>();
        loop_bufferlen_tests<uint16_t, 8>();
        loop_bufferlen_tests<uint32_t, 8>();
        loop_bufferlen_tests<uint64_t, 8>();
    }

    {
        simple_tests_uint8<32>();
        simple_tests_uint16<32>();
        simple_tests_uint32<32>();
        simple_tests_uint64<32>();
    }

    return 0;
}
