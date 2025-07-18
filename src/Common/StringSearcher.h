#pragma once

#include <base/getPageSize.h>
#include <Common/Exception.h>
#include <Common/StringUtils.h>
#include <Common/UTF8Helpers.h>
#include <Core/Defines.h>
#include <Poco/Unicode.h>
#include <cstdint>
#include <cstring>


// #ifdef __SSE4_1__
    #include <emmintrin.h>
    #include <smmintrin.h>
// #endif

#include <immintrin.h>

namespace DB
{

/** Variants for searching a substring in a string.
  * In most cases, performance is less than Volnitsky (see Volnitsky.h).
  */

namespace impl
{

class StringSearcherBase
{
public:
    bool force_fallback = false;

#ifdef __SSE4_1__
protected:
    static constexpr size_t N = sizeof(__m256i);

    bool isPageSafe(const void * const ptr) const
    {
        return ((page_size - 1) & reinterpret_cast<std::uintptr_t>(ptr)) <= page_size - N;
    }

private:
    const Int64 page_size = ::getPageSize();
#endif
};


/// Performs case-sensitive or case-insensitive search of ASCII or UTF-8 strings
template <bool CaseSensitive, bool ASCII> class StringSearcher;


/// Case-sensitive ASCII and UTF8 searcher
template <bool ASCII>
class StringSearcher<true, ASCII> : public StringSearcherBase
{
private:
    /// string to be searched for
    const uint8_t * const needle;
    const uint8_t * const needle_end;
    /// first character in `needle`
    uint8_t first_needle_character = 0;

#ifdef __SSE4_1__
    /// second character of "needle" (if its length is > 1)
    uint8_t second_needle_character = 0;
    /// first/second needle character broadcast into a 16 bytes vector
    __m256i first_needle_character_vec;
    __m256i second_needle_character_vec;
    /// vector of first 32 characters of `needle`
    __m256i cache;
    uint32_t cachemask = 0;
#endif

public:
    template <typename CharT>
    requires (sizeof(CharT) == 1)
    StringSearcher(const CharT * needle_, size_t needle_size)
        : needle(reinterpret_cast<const uint8_t *>(needle_))
        , needle_end(needle + needle_size)
    {
        if (needle_size == 0)
            return;

        first_needle_character = *needle;

#ifdef __SSE4_1__
        first_needle_character_vec = _mm256_set1_epi8(first_needle_character);
        if (needle_size > 1)
        {
            second_needle_character = *(needle + 1);
            second_needle_character_vec = _mm256_set1_epi8(second_needle_character);
        }
        const auto * needle_pos = needle;

        uint8_t needle_copy[sizeof(N)] = {0};
        memcpy(needle_copy, needle, std::min(sizeof(N), needle_size));
        cache = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(needle_copy));

        for (uint8_t i = 0; i < N && (needle_pos != needle_end); ++i)
        {
            cachemask |= 1 << i;
            ++needle_pos;
        }
#endif
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    ALWAYS_INLINE bool compare(const CharT * /*haystack*/, const CharT * /*haystack_end*/, const CharT * pos) const
    {
#ifdef __SSE4_1__
        if (isPageSafe(pos))
        {
            const __m256i haystack_characters = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(pos));
            const __m256i comparison_result = _mm256_cmpeq_epi8(haystack_characters, cache);
            const uint32_t comparison_result_mask = _mm256_movemask_epi8(comparison_result);

            if (0xffffffff == cachemask)
            {
                if (comparison_result_mask == cachemask)
                {
                    pos += N;
                    const auto * needle_pos = needle + N;

                    while (needle_pos < needle_end && *pos == *needle_pos)
                        ++pos, ++needle_pos;

                    if (needle_pos == needle_end)
                        return true;
                }
            }
            else if ((comparison_result_mask & cachemask) == cachemask)
                return true;

            return false;
        }
#endif

        if (*pos == first_needle_character)
        {
            ++pos;
            const auto * needle_pos = needle + 1;

            while (needle_pos < needle_end && *pos == *needle_pos)
                ++pos, ++needle_pos;

            if (needle_pos == needle_end)
                return true;
        }

        return false;
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack, const CharT * const haystack_end) const
    {
        const auto needle_size = needle_end - needle;

        if (needle == needle_end)
            return haystack;

#ifdef __SSE4_1__
        /// Fast path for single-character needles. Compare 32 characters of the haystack against the needle character at once.
        if (needle_size == 1)
        {
            while (haystack < haystack_end)
            {
                if (haystack + N <= haystack_end && isPageSafe(haystack))
                {
                    const __m256i haystack_characters = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack));
                    const __m256i comparison_result = _mm256_cmpeq_epi8(haystack_characters, first_needle_character_vec);
                    if (_mm256_testz_si256(comparison_result, comparison_result))
                    {
                        haystack += N;
                        continue;
                    }

                    const int comparison_result_mask = _mm256_movemask_epi8(comparison_result);
                    const int offset = std::countr_zero(static_cast<UInt32>(comparison_result_mask));
                    haystack += offset;

                    return haystack;
                }

                if (haystack == haystack_end)
                    return haystack_end;

                if (*haystack == first_needle_character)
                    return haystack;

                ++haystack;
            }

            return haystack_end;
        }
#endif

        while (haystack < haystack_end && haystack_end - haystack >= needle_size)
        {
#ifdef __SSE4_1__
            /// Compare the [0:31] bytes from haystack and broadcast 16 bytes vector from first character of needle.
            /// Compare the [1:32] bytes from haystack and broadcast 16 bytes vector from second character of needle.
            /// Bit AND the results of above two comparisons and get the mask.
            if ((haystack + 1 + N) <= haystack_end && isPageSafe(haystack + 1))
            {
                const __m256i haystack_characters_from_1st = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack));
                const __m256i haystack_characters_from_2nd = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack + 1));
                const __m256i comparison_result_1st = _mm256_cmpeq_epi8(haystack_characters_from_1st, first_needle_character_vec);
                const __m256i comparison_result_2nd = _mm256_cmpeq_epi8(haystack_characters_from_2nd, second_needle_character_vec);
                const __m256i comparison_result_combined = _mm256_and_si256(comparison_result_1st, comparison_result_2nd);
                const int comparison_result_mask = _mm256_movemask_epi8(comparison_result_combined);
                /// If the mask = 0, then first two characters [0:1] from needle are not in the [0:31] bytes of haystack.
                if (comparison_result_mask == 0)
                {
                    haystack += N;
                    continue;
                }

                const int offset = std::countr_zero(static_cast<uint32_t>(comparison_result_mask));
                haystack += offset;

                if (haystack + N <= haystack_end && isPageSafe(haystack))
                {
                    /// Already find the haystack position where the [pos:pos + 1] two characters exactly match the first two characters of needle.
                    /// Compare the 32 bytes from needle (cache) and the first 32 bytes from haystack at once if the haystack size >= 32 bytes.
                    const __m256i haystack_characters = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack));
                    const __m256i comparison_result_cache = _mm256_cmpeq_epi8(haystack_characters, cache);
                    const uint32_t mask_offset = _mm256_movemask_epi8(comparison_result_cache);

                    if (0xffffffff == cachemask)
                    {
                        if (mask_offset == cachemask)
                        {
                            const auto * haystack_pos = haystack + N;
                            const auto * needle_pos = needle + N;

                            while (haystack_pos < haystack_end && needle_pos < needle_end &&
                                   *haystack_pos == *needle_pos)
                                ++haystack_pos, ++needle_pos;

                            if (needle_pos == needle_end)
                                return haystack;
                        }
                    }
                    else if ((mask_offset & cachemask) == cachemask)
                        return haystack;

                    ++haystack;
                    continue;
                }
            }
#endif

            if (haystack == haystack_end)
                return haystack_end;

            if (*haystack == first_needle_character)
            {
                const auto * haystack_pos = haystack + 1;
                const auto * needle_pos = needle + 1;

                while (haystack_pos < haystack_end && needle_pos < needle_end &&
                       *haystack_pos == *needle_pos)
                    ++haystack_pos, ++needle_pos;

                if (needle_pos == needle_end)
                    return haystack;
            }

            ++haystack;
        }

        return haystack_end;
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack, size_t haystack_size) const
    {
        return search(haystack, haystack + haystack_size);
    }
};


/// Case-insensitive ASCII searcher
template <>
class StringSearcher<false, true> : public StringSearcherBase
{
private:
    /// string to be searched for
    const uint8_t * const needle;
    const uint8_t * const needle_end;
    /// lower and uppercase variants of the first character in `needle`
    uint8_t l = 0;
    uint8_t u = 0;

#ifdef __SSE4_1__
    /// vectors filled with `l` and `u`, for determining leftmost position of the first symbol
    __m256i patl, patu;
    /// lower and uppercase vectors of first 16 characters of `needle`
    __m256i cachel = _mm256_setzero_si256(), cacheu = _mm256_setzero_si256();
    int cachemask = 0;
#endif

public:
    template <typename CharT>
    requires (sizeof(CharT) == 1)
    StringSearcher(const CharT * needle_, size_t needle_size)
        : needle(reinterpret_cast<const uint8_t *>(needle_))
        , needle_end(needle + needle_size)
    {
        if (needle_size == 0)
            return;

        l = static_cast<uint8_t>(std::tolower(*needle));
        u = static_cast<uint8_t>(std::toupper(*needle));

#ifdef __SSE4_1__
        patl = _mm256_set1_epi8(l);
        patu = _mm256_set1_epi8(u);

        const auto * needle_pos = needle;

        String lower_needle = Poco::toLower({needle_, needle_size});
        String upper_needle = Poco::toUpper({needle_, needle_size});

        uint8_t needle_copy[sizeof(N)] = {0};
        memcpy(needle_copy, lower_needle.c_str(), std::min(sizeof(N), needle_size));
        cachel = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(needle_copy));

        memcpy(needle_copy, upper_needle.c_str(), std::min(sizeof(N), needle_size));
        cacheu = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(needle_copy));

        for (size_t i = 0; i < N && needle_pos != needle_end; ++i)
        {
            cachemask |= 1 << i;
            ++needle_pos;
        }
#endif
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    ALWAYS_INLINE bool compare(const CharT * /*haystack*/, const CharT * /*haystack_end*/, const CharT * pos) const
    {
#ifdef __SSE4_1__
        if (isPageSafe(pos))
        {
            const auto v_haystack = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(pos));
            const auto v_against_l = _mm256_cmpeq_epi8(v_haystack, cachel);
            const auto v_against_u = _mm256_cmpeq_epi8(v_haystack, cacheu);
            const auto v_against_l_or_u = _mm256_or_si256(v_against_l, v_against_u);
            const auto mask = _mm256_movemask_epi8(v_against_l_or_u);

            if (-1 == cachemask)
            {
                if (mask == cachemask)
                {
                    pos += N;
                    const auto * needle_pos = needle + N;

                    while (needle_pos < needle_end && std::tolower(*pos) == std::tolower(*needle_pos))
                    {
                        ++pos;
                        ++needle_pos;
                    }

                    if (needle_pos == needle_end)
                        return true;
                }
            }
            else if ((mask & cachemask) == cachemask)
                return true;

            return false;
        }
#endif

        if (*pos == l || *pos == u)
        {
            ++pos;
            const auto * needle_pos = needle + 1;

            while (needle_pos < needle_end && std::tolower(*pos) == std::tolower(*needle_pos))
            {
                ++pos;
                ++needle_pos;
            }

            if (needle_pos == needle_end)
                return true;
        }

        return false;
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack, const CharT * const haystack_end) const
    {
        if (needle == needle_end)
            return haystack;

        while (haystack < haystack_end)
        {
#ifdef __SSE4_1__
            if (haystack + N <= haystack_end && isPageSafe(haystack))
            {
                const auto v_haystack = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack));
                const auto v_against_l = _mm256_cmpeq_epi8(v_haystack, patl);
                const auto v_against_u = _mm256_cmpeq_epi8(v_haystack, patu);
                const auto v_against_l_or_u = _mm256_or_si256(v_against_l, v_against_u);

                const auto mask = _mm256_movemask_epi8(v_against_l_or_u);

                if (mask == 0)
                {
                    haystack += N;
                    continue;
                }

                const auto offset = __builtin_ctz(mask);
                haystack += offset;

                if (haystack + N <= haystack_end && isPageSafe(haystack))
                {
                    const auto v_haystack_offset = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack));
                    const auto v_against_l_offset = _mm256_cmpeq_epi8(v_haystack_offset, cachel);
                    const auto v_against_u_offset = _mm256_cmpeq_epi8(v_haystack_offset, cacheu);
                    const auto v_against_l_or_u_offset = _mm256_or_si256(v_against_l_offset, v_against_u_offset);
                    const auto mask_offset = _mm256_movemask_epi8(v_against_l_or_u_offset);

                    if (-1 == cachemask)
                    {
                        if (mask_offset == cachemask)
                        {
                            const auto * haystack_pos = haystack + N;
                            const auto * needle_pos = needle + N;

                            while (haystack_pos < haystack_end && needle_pos < needle_end &&
                                   std::tolower(*haystack_pos) == std::tolower(*needle_pos))
                            {
                                ++haystack_pos;
                                ++needle_pos;
                            }

                            if (needle_pos == needle_end)
                                return haystack;
                        }
                    }
                    else if ((mask_offset & cachemask) == cachemask)
                        return haystack;

                    ++haystack;
                    continue;
                }
            }
#endif

            if (haystack == haystack_end)
                return haystack_end;

            if (*haystack == l || *haystack == u)
            {
                const auto * haystack_pos = haystack + 1;
                const auto * needle_pos = needle + 1;

                while (haystack_pos < haystack_end && needle_pos < needle_end &&
                       std::tolower(*haystack_pos) == std::tolower(*needle_pos))
                {
                    ++haystack_pos;
                    ++needle_pos;
                }

                if (needle_pos == needle_end)
                    return haystack;
            }

            ++haystack;
        }

        return haystack_end;
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack, size_t haystack_size) const
    {
        return search(haystack, haystack + haystack_size);
    }
};


/// Case-insensitive UTF-8 searcher
template <>
class StringSearcher<false, false> : public StringSearcherBase
{
private:
    using UTF8SequenceBuffer = uint8_t[6];

    /// substring to be searched for
    const uint8_t * const needle;
    const size_t needle_size;
    const uint8_t * const needle_end = needle + needle_size;
    /// lower and uppercase variants of the first octet of the first character in `needle`
    bool first_needle_symbol_is_ascii = false;
    uint8_t l = 0;
    uint8_t u = 0;

#ifdef __SSE4_1__
    /// vectors filled with `l` and `u`, for determining leftmost position of the first symbol
    __m256i patl;
    __m256i patu;
    /// lower and uppercase vectors of first 16 characters of `needle`
    __m256i cachel = _mm256_setzero_si256();
    __m256i cacheu = _mm256_setzero_si256();
    int cachemask = 0;
    size_t cache_valid_len = 0;
    size_t cache_actual_len = 0;
#endif

public:
    template <typename CharT>
    requires (sizeof(CharT) == 1)
    StringSearcher(const CharT * needle_, size_t needle_size_)
        : needle(reinterpret_cast<const uint8_t *>(needle_))
        , needle_size(needle_size_)
    {
        if (needle_size == 0)
            return;

        UTF8SequenceBuffer l_seq;
        UTF8SequenceBuffer u_seq;

        if (*needle < 0x80u)
        {
            first_needle_symbol_is_ascii = true;
            l = std::tolower(*needle);
            u = std::toupper(*needle);
        }
        else
        {
            auto first_u32 = UTF8::convertUTF8ToCodePoint(reinterpret_cast<const char *>(needle), needle_size);

            /// Invalid UTF-8
            if (!first_u32)
            {
                /// Process it verbatim as a sequence of bytes.
                size_t src_len = UTF8::seqLength(*needle);

                memcpy(l_seq, needle, src_len);
                memcpy(u_seq, needle, src_len);
            }
            else
            {
                uint32_t first_l_u32 = Poco::Unicode::toLower(*first_u32);
                uint32_t first_u_u32 = Poco::Unicode::toUpper(*first_u32);

                /// lower and uppercase variants of the first octet of the first character in `needle`
                size_t length_l = UTF8::convertCodePointToUTF8(first_l_u32, reinterpret_cast<char *>(l_seq), sizeof(l_seq));
                size_t length_u = UTF8::convertCodePointToUTF8(first_u_u32, reinterpret_cast<char *>(u_seq), sizeof(u_seq));

                if (length_l != length_u)
                    force_fallback = true;
            }

            l = l_seq[0];
            u = u_seq[0];

            if (force_fallback)
                return;
        }

#ifdef __SSE4_1__
        /// for detecting leftmost position of the first symbol
        patl = _mm256_set1_epi8(l);
        patu = _mm256_set1_epi8(u);
        /// lower and uppercase vectors of first 16 octets of `needle`

        const auto * needle_pos = needle;

        for (size_t i = 0; i < N;)
        {
            if (needle_pos == needle_end)
            {
                cachel = _mm256_srli_si256(cachel, 1);
                cacheu = _mm256_srli_si256(cacheu, 1);
                ++i;

                continue;
            }

            size_t src_len = std::min<size_t>(needle_end - needle_pos, UTF8::seqLength(*needle_pos));
            auto c_u32 = UTF8::convertUTF8ToCodePoint(reinterpret_cast<const char *>(needle_pos), src_len);

            if (c_u32)
            {
                int c_l_u32 = Poco::Unicode::toLower(*c_u32);
                int c_u_u32 = Poco::Unicode::toUpper(*c_u32);

                size_t dst_l_len = UTF8::convertCodePointToUTF8(c_l_u32, reinterpret_cast<char *>(l_seq), sizeof(l_seq));
                size_t dst_u_len = UTF8::convertCodePointToUTF8(c_u_u32, reinterpret_cast<char *>(u_seq), sizeof(u_seq));

                /// @note Unicode standard states it is a rare but possible occasion
                if (!(dst_l_len == dst_u_len && dst_u_len == src_len))
                {
                    force_fallback = true;
                    return;
                }
            }

            cache_actual_len += src_len;
            if (cache_actual_len < N)
                cache_valid_len += src_len;

            for (size_t j = 0; j < src_len && i < N; ++j, ++i)
            {
                cachel = _mm256_srli_si256(cachel, 1);
                cacheu = _mm256_srli_si256(cacheu, 1);

                if (needle_pos != needle_end)
                {
                    cachel = _mm256_insert_epi8(cachel, l_seq[j], N - 1);
                    cacheu = _mm256_insert_epi8(cacheu, u_seq[j], N - 1);

                    cachemask |= 1 << i;
                    ++needle_pos;
                }
            }
        }
#endif
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    ALWAYS_INLINE bool compareTrivial(const CharT * haystack_pos, const CharT * const haystack_end, const uint8_t * needle_pos) const
    {
        while (haystack_pos < haystack_end && needle_pos < needle_end)
        {
            auto haystack_code_point = UTF8::convertUTF8ToCodePoint(reinterpret_cast<const char *>(haystack_pos), haystack_end - haystack_pos);
            auto needle_code_point = UTF8::convertUTF8ToCodePoint(reinterpret_cast<const char *>(needle_pos), needle_end - needle_pos);

            /// Invalid UTF-8, should not compare equals
            if (!haystack_code_point || !needle_code_point)
                break;

            /// Not equals case insensitive.
            if (Poco::Unicode::toLower(*haystack_code_point) != Poco::Unicode::toLower(*needle_code_point))
                break;

            auto len = UTF8::seqLength(*haystack_pos);
            haystack_pos += len;

            len = UTF8::seqLength(*needle_pos);
            needle_pos += len;
        }

        return needle_pos == needle_end;
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    ALWAYS_INLINE bool compare(const CharT * /*haystack*/, const CharT * haystack_end, const CharT * pos) const
    {

#ifdef __SSE4_1__
        if (isPageSafe(pos) && !force_fallback)
        {
            const auto v_haystack = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(pos));
            const auto v_against_l = _mm256_cmpeq_epi8(v_haystack, cachel);
            const auto v_against_u = _mm256_cmpeq_epi8(v_haystack, cacheu);
            const auto v_against_l_or_u = _mm256_or_si256(v_against_l, v_against_u);
            const auto mask = _mm256_movemask_epi8(v_against_l_or_u);

            if (-1 == cachemask)
            {
                if (mask == cachemask)
                {
                    if (compareTrivial(pos, haystack_end, needle))
                        return true;
                }
            }
            else if ((mask & cachemask) == cachemask)
            {
                if (compareTrivial(pos, haystack_end, needle))
                    return true;
            }

            return false;
        }
#endif

        if (*pos == l || *pos == u)
        {
            pos += first_needle_symbol_is_ascii;
            const auto * needle_pos = needle + first_needle_symbol_is_ascii;

            if (compareTrivial(pos, haystack_end, needle_pos))
                return true;
        }

        return false;
    }

    /** Returns haystack_end if not found.
      */
    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack, const CharT * const haystack_end) const
    {
        if (needle_size == 0)
            return haystack;

        while (haystack < haystack_end)
        {
#ifdef __SSE4_1__
            if (haystack + N <= haystack_end && isPageSafe(haystack) && !force_fallback)
            {
                const auto v_haystack = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack));
                const auto v_against_l = _mm256_cmpeq_epi8(v_haystack, patl);
                const auto v_against_u = _mm256_cmpeq_epi8(v_haystack, patu);
                const auto v_against_l_or_u = _mm256_or_si256(v_against_l, v_against_u);

                const auto mask = _mm256_movemask_epi8(v_against_l_or_u);

                if (mask == 0)
                {
                    haystack += N;
                    UTF8::syncForward(haystack, haystack_end);
                    continue;
                }

                const auto offset = __builtin_ctz(mask);
                haystack += offset;

                if (haystack + N <= haystack_end && isPageSafe(haystack))
                {
                    const auto v_haystack_offset = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(haystack));
                    const auto v_against_l_offset = _mm256_cmpeq_epi8(v_haystack_offset, cachel);
                    const auto v_against_u_offset = _mm256_cmpeq_epi8(v_haystack_offset, cacheu);
                    const auto v_against_l_or_u_offset = _mm256_or_si256(v_against_l_offset, v_against_u_offset);
                    const auto mask_offset_both = _mm256_movemask_epi8(v_against_l_or_u_offset);

                    if (-1 == cachemask)
                    {
                        if (mask_offset_both == cachemask)
                        {
                            if (compareTrivial(haystack, haystack_end, needle))
                                return haystack;
                        }
                    }
                    else if ((mask_offset_both & cachemask) == cachemask)
                    {
                        if (compareTrivial(haystack, haystack_end, needle))
                            return haystack;
                    }

                    /// first octet was ok, but not the first 16, move to start of next sequence and reapply
                    haystack += UTF8::seqLength(*haystack);
                    continue;
                }
            }
#endif

            if (haystack == haystack_end)
                return haystack_end;

            if (*haystack == l || *haystack == u)
            {
                auto haystack_pos = haystack + first_needle_symbol_is_ascii;
                const auto * needle_pos = needle + first_needle_symbol_is_ascii;

                if (compareTrivial(haystack_pos, haystack_end, needle_pos))
                    return haystack;
            }

            /// advance to the start of the next sequence
            haystack += UTF8::seqLength(*haystack);
        }

        return haystack_end;
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack, size_t haystack_size) const
    {
        return search(haystack, haystack + haystack_size);
    }
};

}

using ASCIICaseSensitiveStringSearcher =   impl::StringSearcher<true, true>;
using ASCIICaseInsensitiveStringSearcher = impl::StringSearcher<false, true>;
using UTF8CaseSensitiveStringSearcher =    impl::StringSearcher<true, false>;
using UTF8CaseInsensitiveStringSearcher =  impl::StringSearcher<false, false>;

/// Use only with short haystacks where cheap initialization is required.
template <bool CaseInsensitive>
struct StdLibASCIIStringSearcher
{
    const char * const needle_start;
    const char * const needle_end;

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    StdLibASCIIStringSearcher(const CharT * const needle_start_, size_t needle_size_)
        : needle_start(reinterpret_cast<const char *>(needle_start_))
        , needle_end(reinterpret_cast<const char *>(needle_start) + needle_size_)
    {}

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack_start, const CharT * const haystack_end) const
    {
        if constexpr (CaseInsensitive)
            return std::search(
                haystack_start, haystack_end, needle_start, needle_end,
                [](char c1, char c2) { return std::toupper(c1) == std::toupper(c2); });
        else
            return std::search(
                haystack_start, haystack_end, needle_start, needle_end,
                [](char c1, char c2) { return c1 == c2; });
    }

    template <typename CharT>
    requires (sizeof(CharT) == 1)
    const CharT * search(const CharT * haystack_start, size_t haystack_length) const
    {
        return search(haystack_start, haystack_start + haystack_length);
    }
};

}
