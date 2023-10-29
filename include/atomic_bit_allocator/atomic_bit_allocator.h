/*
 * Copyright 2023 Joerg Peter Schaefer
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <type_traits>
#include <thread>
#include <memory>
#include <cassert>
#include <cstdint>
#include <atomic>
#include <bit>


namespace jps {

template<typename W>
struct _reentrant_lock_free_bit_allocator {
    using WordT = W;

    static constexpr uint8_t bits_per_byte = 8;
    static constexpr size_t bytes_per_word = sizeof( WordT );
    static constexpr uint8_t bits_per_word = bits_per_byte*bytes_per_word;

    constexpr _reentrant_lock_free_bit_allocator() { bitmap_[0] = 0; }
    constexpr _reentrant_lock_free_bit_allocator& operator=( const W bitset ) {
        bitmap_[0] = bitset;
        return *this;
    }

    [[nodiscard]] size_t alloc( size_t len, size_t start_pos = 0, size_t end_pos = bits_per_word,
                                std::memory_order mo = std::memory_order::acquire ) noexcept {

        do {
            // find a free range
            start_pos = find_unset_range( start_pos, end_pos, len, mo );

            if( start_pos + len > end_pos )
                return end_pos;

            // try to allocate it
            const auto first_word = _which_word( start_pos );
            const auto start_bit_in_word = _which_bit_in_word( start_pos );
            const auto last_word = _which_word( start_pos + len - 1 );
            const auto last_bit_in_word = _which_bit_in_word( start_pos + len - 1 );

            // just alter one word
            if( first_word == last_word ) {
                const auto mask = get_mask( start_bit_in_word, last_bit_in_word );

                const auto prev = bitmap_[first_word].fetch_or( mask, mo );

                if(( prev & mask ) == WordT( 0 ))
                    return start_pos;

                // on failure, rollback and try another range
                bitmap_[first_word].fetch_and( ~mask | ( prev & mask ), mo);
            }

                // altering multiple words required
            else {
                size_t w;
                WordT tmp;
                const WordT mask_first = ( WordT( ~WordT( 0 )) >> start_bit_in_word );
                const WordT mask_last = ( WordT( ~WordT( 0 )) << ( bits_per_word - last_bit_in_word - 1 ));

                WordT prev_first;
                WordT prev_last;

                // alter first word: bits range to the least significant bit
                {
                    prev_first = bitmap_[first_word].fetch_or( mask_first, mo );
                    if(( prev_first & mask_first ) != WordT( 0 ))
                        goto rollback_first;
                }

                // alter mid-range words: they shall all be zero and be set to ~0
                {
                    w = first_word + 1;
                    for( ; w < last_word; ++w ) {
                        auto prev = bitmap_[w].fetch_or( ~WordT( 0 ), mo );

                        // rollback on failure
                        if( prev != 0 ) {
                            bitmap_[w--].fetch_and( prev, mo );
                            break;
                        }
                    }
                    if( w < last_word )
                        goto rollback_mid;
                }

                // now care for the last word
                {
                    prev_last = bitmap_[last_word].fetch_or( mask_last, mo );

                    // on success, return the location of the range
                    if(( prev_last & mask_last ) == WordT( 0 ))
                        return start_pos;
                }

            [[maybe_unused]] rollback_last:
                // get the mask of the bits to keep
                tmp = WordT( ~mask_last ) | ( prev_last & mask_last );

                // zero out the bits we have to rollback. track the bits we actually reset in *tmp*
                tmp = WordT( ~tmp ) & bitmap_[last_word].fetch_and( tmp, mo );

                // check if the bits we rolled back are indeed the ones we had to roll back
                assert( ( tmp & prev_last ) == 0 );
                --w;

            rollback_mid:
                for( ; w > first_word; --w ) {
                    assert( ( tmp = bitmap_[w].load( std::memory_order::acquire )) == WordT( ~WordT( 0 )));
                    bitmap_[w].store( 0, std::memory_order::release );
                }

            rollback_first:
                // get the mask of the bits to keep
                tmp = WordT( ~mask_first ) | ( prev_first & mask_first );

                // zero out the bits we have to rollback. track the bits we actually reset in *tmp*
                tmp = WordT( ~tmp ) & bitmap_[first_word].fetch_and( tmp, mo );

                // check if the bits we rolled back are indeed the ones we had to roll back
                assert( ( tmp & prev_first ) == 0 );
            }
        } while( true );
    }
    void free( size_t start_pos, size_t len, std::memory_order mo = std::memory_order::release ) {
        // try to allocate it
        const auto start_word = _which_word( start_pos );
        const auto start_bit_in_word = _which_bit_in_word( start_pos );
        const auto last_word = _which_word( start_pos + len - 1 );
        const auto last_bit_in_word = _which_bit_in_word( start_pos + len - 1 );

        // just alter one word
        if( start_word == last_word ) {
            const auto mask = get_mask( start_bit_in_word, last_bit_in_word );

            bitmap_[start_word].fetch_and( ~mask, mo );
        }

        // altering multiple words required
        else {
            size_t w;
            const WordT mask_first = ( WordT( ~WordT( 0 )) >> start_bit_in_word );
            const WordT mask_last = ( WordT( ~WordT( 0 )) << ( bits_per_word - last_bit_in_word - 1 ));

            // alter first word: bits range to the least significant bit
            bitmap_[start_word].fetch_and( ~mask_first, mo );

            // alter mid-range words: they shall all be zero and be set to ~0
            w = start_word + 1;
            for( ; w < last_word; ++w )
                bitmap_[w].store( WordT( 0 ), mo );

            // now care for the last word
            bitmap_[last_word].fetch_and( ~mask_last, mo );
        }
    }
    [[nodiscard]] size_t usage( std::memory_order memory_order = std::memory_order::relaxed ) const {
        const auto w = bitmap_[0].load( memory_order );
        return std::popcount( w );
    }

    static constexpr bool is_reentrant() { return true; }

protected:
    /*
     * Gets a hint for where there might be a first unset bit.
     */
    [[nodiscard]] size_t find_first_unset( size_t start_pos = 0, size_t end_pos = bits_per_word,
                                           std::memory_order mo = std::memory_order::acquire ) const {
        const auto start_word = _which_word( start_pos );
        auto start_bit_in_word = _which_bit_in_word( start_pos );

        WordT bits = bitmap_[start_word].load( mo ) << start_bit_in_word;
        start_bit_in_word += std::countl_one( bits );
        if( start_bit_in_word < bits_per_word )
            return bits_per_word*start_word + start_bit_in_word;

        const size_t end_word = _which_word( end_pos );
        for( size_t w = start_word+1; w < end_word; ++w ) {
            bits = bitmap_[w].load( mo );
            if( bits != ~static_cast<WordT>( 0 ) )
                return w*bits_per_word + std::countl_one( bits );
        }

        return end_pos;
    }

    /*
     * Gets a hint for where there might be a first set bit.
     */
    [[nodiscard]] size_t find_first_set( size_t start_pos = 0, size_t end_pos = bits_per_word,
                                         std::memory_order mo = std::memory_order::acquire ) const {
        const auto start_word = _which_word( start_pos );
        auto start_bit_in_word = _which_bit_in_word( start_pos );

        WordT bits = bitmap_[start_word].load( mo ) << start_bit_in_word;
        start_bit_in_word += std::countl_zero( bits );
        if( start_bit_in_word < bits_per_word )
            return bits_per_word*start_word + start_bit_in_word;

        const size_t end_word = _which_word( end_pos );
        for( size_t w = start_word+1; w < end_word; ++w ) {
            bits = bitmap_[w].load( mo );
            if( bits != static_cast<WordT>( 0 ) )
                return w*bits_per_word + std::countl_zero( bits );
        }

        return end_pos;
    }


    [[nodiscard]] size_t find_unset_range( size_t start_pos = 0, size_t end_pos = bits_per_word, size_t len = 1,
                                           std::memory_order mo = std::memory_order::acquire ) const {
        do {
            // find a possible start
            start_pos = find_first_unset( start_pos, end_pos, mo );
            if( start_pos + len > end_pos )
                return end_pos;

            // see if the range is large enough
            const auto range_end = find_first_set( start_pos+1, start_pos + len, mo );
            if( start_pos + len <= range_end )
                return start_pos;

            start_pos = range_end+1;
        } while( start_pos + len <= end_pos );

        return end_pos;
    }

    static constexpr WordT get_mask( size_t first_bit, size_t last_bit ) {
        return
                WordT( ~WordT( 0 )) >> first_bit &
                WordT( ~WordT( 0 )) << ( bits_per_word - last_bit - 1 );
    }

    /**
     * Return the number of words necessary to store a particular number of bits.
     * @param end_pos The number of bits
     * @return The required number of words
     */
    static constexpr size_t _sizeof_array( size_t end_pos ) {
        return end_pos == 0 ? 0 : _which_word( end_pos-1 )+1;
    }
    /**
     * Return the size of a _atomic_bitset array necessary to store a particular number of bits.
     * @param end_pos The number of bits
     * @return The required memory usage for the _atomic_bitset array
     */
    static constexpr size_t _sizeof( size_t end_pos ) { return sizeof( WordT ) * _sizeof_array( end_pos ); }

    /**
     * Return the index to the word containing a particular bit.
     * @param pos The bit position
     * @return The index of the word containing the required bit
     */
    static constexpr size_t _which_word( size_t pos ) { return pos/bits_per_word; }
    /**
     * Return the index in a word containing a particular bit.
     * @param pos The bit position
     * @return The index of the bit in the word
     */
    static constexpr uint8_t _which_bit_in_word( size_t pos ) { return pos%bits_per_word; }
    /**
     * Return the index of the byte in a word containing the required bit.
     * @param pos The index of the bit
     * @return The byte index in the word containing this particular bit
     */
    static constexpr size_t _which_byte_in_word( size_t pos ) { return _which_bit_in_word( pos )/bits_per_byte; }

    std::atomic<WordT> bitmap_[1];
    static_assert( std::atomic<WordT>::is_always_lock_free );
};

template<typename W>
struct _single_threaded_bit_allocator {
    using WordT = W;

    static constexpr uint8_t bits_per_byte = 8;
    static constexpr size_t bytes_per_word = sizeof( WordT );
    static constexpr uint8_t bits_per_word = bits_per_byte*bytes_per_word;

    constexpr _single_threaded_bit_allocator() { bitmap_[0] = 0; }
    constexpr _single_threaded_bit_allocator& operator=( const W bitset ) {
        bitmap_[0] = bitset;
        return *this;
    }

    [[nodiscard]] size_t alloc( size_t len, size_t start_pos = 0, size_t end_pos = bits_per_word,
                                std::memory_order mo = std::memory_order::acquire ) noexcept {

        // find a free range
        start_pos = find_unset_range( start_pos, end_pos, len, mo );

        if( start_pos + len > end_pos )
            return end_pos;

        // try to allocate it
        const auto first_word = _which_word( start_pos );
        const auto start_bit_in_word = _which_bit_in_word( start_pos );
        const auto last_word = _which_word( start_pos + len - 1 );
        const auto last_bit_in_word = _which_bit_in_word( start_pos + len - 1 );

        // just alter one word
        if( first_word == last_word ) {
            bitmap_[first_word] |= get_mask( start_bit_in_word, last_bit_in_word );
            return start_pos;
        }

            // altering multiple words required
        else {
            const WordT mask_first = ( WordT( ~WordT( 0 )) >> start_bit_in_word );
            const WordT mask_last = ( WordT( ~WordT( 0 )) << ( bits_per_word - last_bit_in_word - 1 ));

            // alter first word: bits range to the least significant bit
            bitmap_[first_word] |= mask_first;

            // alter mid-range words: they shall all be zero and be set to ~0
            for( auto w = first_word+1; w < last_word; ++w )
                bitmap_[w] |= ~WordT( 0 );

            // now care for the last word
            bitmap_[last_word] |= mask_last;

            return start_pos;
        }
    }
    void free( size_t start_pos, size_t len, [[maybe_unused]] std::memory_order mo = std::memory_order::release ) {
        // try to allocate it
        const auto start_word = _which_word( start_pos );
        const auto start_bit_in_word = _which_bit_in_word( start_pos );
        const auto last_word = _which_word( start_pos + len - 1 );
        const auto last_bit_in_word = _which_bit_in_word( start_pos + len - 1 );

        // just alter one word
        if( start_word == last_word ) {
            const auto mask = get_mask( start_bit_in_word, last_bit_in_word );

            bitmap_[start_word] &= ~mask;
        }

        // altering multiple words required
        else {
            const WordT mask_first = ( WordT( ~WordT( 0 )) >> start_bit_in_word );
            const WordT mask_last = ( WordT( ~WordT( 0 )) << ( bits_per_word - last_bit_in_word - 1 ));

            // alter first word: bits range to the least significant bit
            bitmap_[start_word] &= ~mask_first;

            // alter mid-range words: they shall all be zero and be set to ~0
            for( auto w = start_word+1; w < last_word; ++w )
                bitmap_[w] = 0;

            // now care for the last word
            bitmap_[last_word] &= ~mask_last;
        }
    }
    [[nodiscard]] size_t usage( [[maybe_unused]] std::memory_order memory_order = std::memory_order::relaxed ) const {
        return std::popcount( bitmap_[0] );
    }

    static constexpr bool is_reentrant() { return false; }

protected:
    /*
     * Gets a hint for where there might be a first unset bit.
     */
    [[nodiscard]] size_t find_first_unset( size_t start_pos = 0, size_t end_pos = bits_per_word,
                                           [[maybe_unused]] std::memory_order mo = std::memory_order::acquire ) const {
        const auto start_word = _which_word( start_pos );
        auto start_bit_in_word = _which_bit_in_word( start_pos );

        WordT bits = bitmap_[start_word] << start_bit_in_word;
        start_bit_in_word += std::countl_one( bits );
        if( start_bit_in_word < bits_per_word )
            return bits_per_word*start_word + start_bit_in_word;

        const size_t end_word = _which_word( end_pos );
        for( size_t w = start_word+1; w < end_word; ++w ) {
            bits = bitmap_[w];
            if( bits != ~static_cast<WordT>( 0 ) )
                return w*bits_per_word + std::countl_one( bits );
        }

        return end_pos;
    }

    /*
     * Gets a hint for where there might be a first set bit.
     */
    [[nodiscard]] size_t find_first_set( size_t start_pos = 0, size_t end_pos = bits_per_word,
                                         [[maybe_unused]] std::memory_order mo = std::memory_order::acquire ) const {
        const auto start_word = _which_word( start_pos );
        auto start_bit_in_word = _which_bit_in_word( start_pos );

        WordT bits = bitmap_[start_word] << start_bit_in_word;
        start_bit_in_word += std::countl_zero( bits );
        if( start_bit_in_word < bits_per_word )
            return bits_per_word*start_word + start_bit_in_word;

        const size_t end_word = _which_word( end_pos );
        for( size_t w = start_word+1; w < end_word; ++w ) {
            bits = bitmap_[w];
            if( bits != static_cast<WordT>( 0 ) )
                return w*bits_per_word + std::countl_zero( bits );
        }

        return end_pos;
    }


    [[nodiscard]] size_t find_unset_range( size_t start_pos = 0, size_t end_pos = bits_per_word, size_t len = 1,
                                           std::memory_order mo = std::memory_order::acquire ) const {
        do {
            // find a possible start
            start_pos = find_first_unset( start_pos, end_pos, mo );
            if( start_pos + len > end_pos )
                return end_pos;

            // see if the range is large enough
            const auto range_end = find_first_set( start_pos+1, start_pos + len, mo );
            if( start_pos + len <= range_end )
                return start_pos;

            start_pos = range_end+1;
        } while( start_pos + len <= end_pos );

        return end_pos;
    }

    static constexpr WordT get_mask( size_t first_bit, size_t last_bit ) {
        return
                WordT( ~WordT( 0 )) >> first_bit &
                WordT( ~WordT( 0 )) << ( bits_per_word - last_bit - 1 );
    }

    /**
     * Return the number of words necessary to store a particular number of bits.
     * @param end_pos The number of bits
     * @return The required number of words
     */
    static constexpr size_t _sizeof_array( size_t end_pos ) {
        return end_pos == 0 ? 0 : _which_word( end_pos-1 )+1;
    }
    /**
     * Return the size of a _atomic_bitset array necessary to store a particular number of bits.
     * @param end_pos The number of bits
     * @return The required memory usage for the _atomic_bitset array
     */
    static constexpr size_t _sizeof( size_t end_pos ) { return sizeof( WordT ) * _sizeof_array( end_pos ); }

    /**
     * Return the index to the word containing a particular bit.
     * @param pos The bit position
     * @return The index of the word containing the required bit
     */
    static constexpr size_t _which_word( size_t pos ) { return pos/bits_per_word; }
    /**
     * Return the index in a word containing a particular bit.
     * @param pos The bit position
     * @return The index of the bit in the word
     */
    static constexpr uint8_t _which_bit_in_word( size_t pos ) { return pos%bits_per_word; }
    /**
     * Return the index of the byte in a word containing the required bit.
     * @param pos The index of the bit
     * @return The byte index in the word containing this particular bit
     */
    static constexpr size_t _which_byte_in_word( size_t pos ) { return _which_bit_in_word( pos )/bits_per_byte; }

    WordT bitmap_[1];
};


/**
 * This datastructure allows the allocation of bits and bitranges in a bitmap concurrently.
 *
 * An object of this type can and should be constructed in-place in a pre-allocated buffer given its size.
 *
 * The access to this data structure is lock-free and reentrant.
 *
 */
template<typename W = uint64_t, template<typename> typename bit_allocator = _reentrant_lock_free_bit_allocator>
struct serialized_bit_allocator {

    explicit constexpr serialized_bit_allocator( size_t buffer_len = 64 ) :
            end_pos_(
                    (       // remaining buffer for the bitmap ...
                            ( buffer_len-sizeof( serialized_bit_allocator<W> ) + sizeof( bit_allocator_ ) )
                            // ... rounded down to match full words ...
                            & ~size_t( bit_allocator<W>::bytes_per_word-1)
                    )
                    // ... and scaled to number of bits
                    *bit_allocator<W>::bits_per_byte
            )
    {}

    constexpr size_t size() const {
        return end_pos_;
    }

    [[nodiscard]] size_t alloc( size_t len, std::memory_order mo = std::memory_order::acquire ) {
        if( len == 0 )
            throw std::bad_alloc();

        if( !bit_allocator<W>::is_reentrant() )
            get_global_mutex().lock();
        auto start_pos = bit_allocator_[0].alloc( len, 0, end_pos_, mo );
        if( !bit_allocator<W>::is_reentrant() )
            get_global_mutex().unlock();

        if( start_pos == end_pos_ )
            throw std::bad_alloc();

        return start_pos;
    }
    void free( size_t start_pos, size_t len, std::memory_order mo = std::memory_order::release ) {
        if( !bit_allocator<W>::is_reentrant() )
            get_global_mutex().lock();
        bit_allocator_[0].free( start_pos, len, mo );
        if( !bit_allocator<W>::is_reentrant() )
            get_global_mutex().unlock();
    }
    [[nodiscard]] size_t usage( std::memory_order memory_order = std::memory_order::acquire ) const {
        size_t u = 0;

        if( !bit_allocator<W>::is_reentrant() )
            get_global_mutex().lock();
        for( auto w = 0ul; w <= ( end_pos_-1 )/bit_allocator<W>::bits_per_word; ++w )
            u += bit_allocator_[w].usage( std::memory_order_relaxed );
        if( !bit_allocator<W>::is_reentrant() )
            get_global_mutex().unlock();
        return u;
    }

protected:
    static std::mutex& get_global_mutex() {
        static std::mutex m;
        return m;
    }

    const size_t end_pos_;
    bit_allocator<W> bit_allocator_[1];
};

}
