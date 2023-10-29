//
// Created by Joerg P. Schaefer on 29.10.2023.
//

#include <chrono>
#include <bitset>
#include <cstring>
#include <vector>
#include <iostream>
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

struct alignas(128) aligned_ctr {
    std::atomic<size_t> ctr;
};

template<size_t T>
void stress_test( const size_t num_ops ) {
    const auto MAX_ALLOC = 16;
    bit_allocator_buffer<uint8_t, MAX_ALLOC*MAX_ALLOC*T> buffer;
    std::memset( &buffer, 0, sizeof( buffer ));
    auto* ballocator = new ( &buffer ) jps::serialized_bit_allocator<uint8_t>( sizeof( buffer ));

    // manage workers
    std::vector<std::thread> workers;
    workers.reserve( T );

    // manage test result
    std::vector<aligned_ctr> ctrs( ballocator->size() );
    std::atomic<size_t> total_sum;

    // the worker's work
    auto worker = [&]( size_t thread_id ) {
        thread_local uint64_t local_val = 0;

        for( auto i = 0ul; i < num_ops; ++i ) {
            // allocate some bits and interpret it as a lock into the ctrs vector
            const auto n = ( i*thread_id ) % ( MAX_ALLOC-1 ) + 1;
            const auto p = ballocator->alloc( n );

            // increment locked ctrs values
            for( auto c = p; c < p+n; ++c ) {
                ctrs[c].ctr.store( ctrs[c].ctr.load() + 1 );
                local_val += 1;
            }

            // free / release the allocated bits
            ballocator->free( p, n );
        }

        // add this thread's sum to the total_sum atomic variable, which serves as the ground truth for the test
        total_sum.fetch_add( local_val );
    };

    // start all threads
    for( auto i = 0u; i < T; ++i ) {
        std::thread t( worker, i );
        workers.push_back( std::move( t ) );
    }

    // wait for all threads to finish
    for( auto& w: workers ) {
        w.join();
    }

    // compare the results
    size_t locked_sum = 0;
    for( auto& c: ctrs ) {
        locked_sum += c.ctr.load();
        if( c.ctr != 0 )
            std::cout << c.ctr << " ";
    }
    if( locked_sum != total_sum )
        throw std::exception();
}


int main( [[maybe_unused]] int argc, [[maybe_unused]] char* argv[] ) {
    stress_test<16>( 1000000 );

    return 0;
}
