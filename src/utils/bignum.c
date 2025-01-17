#include "bignum.h"

#include <assert.h>

BigNum bignum_new(u64 const cap) {
    return u64vec_new_init(cap, 0);
}

BigNum bignum_clone(BigNum const n) {
    return u64vec_clone(n);
}

void bignum_shrink(BigNum *const n) {
    return u64vec_shrink(n);
}

void bignum_resize(BigNum *const n, u64 const cap) {
    return u64vec_resize(n, cap);
}

void bignum_clean(BigNum *const n) {
    for (u64 i = n->len - 1;; i--) {
        if (n->ptr[i] != 0) {
            n->len = i + 1;
            break;
        } else if (i == 0) {
            n->len = 0;
            break;
        }
    }
}

void bignum_free(BigNum const n) {
    return u64vec_free(n);
}

void bignum_force_bit(BigNum *const n, u64 const pos, u8 const val) {
    assert(pos < n->cap << 6 && "pos > cap");
    n->len         = max(n->len, (pos + 0x3f) >> 6);
    u64 const idx  = pos >> 6;
    n->ptr[idx]   ^= val - 1;
    n->ptr[idx]   |= 1ull << (pos & 0x3f);
    n->ptr[idx]   ^= val - 1;
}

void bignum_set_bit(BigNum *const n, u64 const pos) {
    assert(pos < n->cap << 6 && "pos > cap");
    n->len            = max(n->len, (pos + 0x3f) >> 6);
    n->ptr[pos >> 6] |= 1ull << (pos & 0x3f);
}

void bignum_unset_bit(BigNum *const n, u64 const pos) {
    assert(pos < n->len << 6 && "pos > cap");
    n->ptr[pos >> 6] &= ~(1ull << (pos & 0x3f));
}

u64 bignum_is_set_bit(BigNum const n, u64 const pos) {
    assert(pos < n.len << 6 && "pos > cap");
    return !!(n.ptr[pos >> 6] & 1ull << (pos & 0x3f));
}

BigNum bignum_read(FILE *const stream) {
    String s = string_read(stream);
    for (u64 i = 0; i < s.len - 1; i++)
        assert(s.ptr[i] >= '0' && s.ptr[i] <= '9' && "not a decimal digit");
    BigNum n         = bignum_new(((s.len << 2) + 63) >> 6);  // 4 > log2(10)
    u64    left_most = 0, pos = 0;
    while (left_most <= s.len - 2) {
        bignum_force_bit(&n, pos++, (s.ptr[s.len - 2] - '0') & 1);
        s.ptr[s.len - 2] = ((s.ptr[s.len - 2] - '0') >> 1) + '0';
        for (u64 i = s.len - 3; i >= left_most && i != UINT64_MAX; i--) {
            s.ptr[i + 1] += 5 * ((s.ptr[i] - '0') & 1);
            s.ptr[i]      = ((s.ptr[i] - '0') >> 1) + '0';
        }
        left_most += (s.ptr[left_most] == '0');
    }
    string_free(s);
    bignum_shrink(&n);
    return n;
}

BigNum bignum_read_hex(FILE *const stream) {
    String s = string_read(stream);
    for (u64 i = 0; i < s.len - 1; i++)
        assert(((s.ptr[i] >= '0' && s.ptr[i] <= '9') || (s.ptr[i] >= 'a' && s.ptr[i] <= 'f')) &&
               "not a hexadeciaml digit");
    BigNum n = bignum_new((s.len + 15) >> 4);
    n.len    = n.cap;
    for (u64 i = 0; i < n.len - 1; i++) {
        sscanf(s.ptr + s.len - ((i + 1) << 4), "%lx", &n.ptr[i]);
        s.ptr[s.len - ((i + 1) << 4)] = '\0';
    }
    sscanf(s.ptr, "%lx", &n.ptr[n.len - 1]);
    string_free(s);
    return n;
}

String bignum_to_string(BigNum const n) {
    u64 const b = 10;
    BigNum    m = bignum_clone(n);
    u64 const cap =
        (m.len << 6) / (64 - __builtin_clzl(b - 1));  // log2(n) / log2(base) == log_base(n)
    String s = string_new(cap);
    while (m.len) {
        u64 carry = bignum_div_eq_u64(&m, b);
        string_push(&s, carry + '0');
    }
    bignum_free(m);
    if (s.len == 0) string_push(&s, '0');
    string_push(&s, '\0');
    string_shrink(&s);
    string_rev(&s);
    return s;
}

String bignum_to_string_hex(BigNum const n, u32 const space) {
    String s;
    s = string_new(n.len * (space ? 17 : 16) + 2);
    if (!n.len) sprintf(s.ptr, "0");
    else {
        for (u32 i = n.len - 1; i != UINT32_MAX; i--)
            sprintf(s.ptr + ((n.len - 1 - i) * (space ? 17 : 16)), "%016lx ", n.ptr[i]);
    }
    s.len = s.cap - 1;
    return s;
}

void bignum_print(FILE *const stream, BigNum const n) {
    String const s = bignum_to_string(n);
    fprintf(stream, "%s", s.ptr);
    string_free(s);
}

void bignum_print_hex(FILE *const stream, BigNum const n, u32 const dbg) {
    String const s = bignum_to_string_hex(n, dbg);
    fprintf(stream, "%s", s.ptr);
    string_free(s);
}

void bignum_print_base(FILE *const stream, BigNum const n, u64 const b) {
    u64Vec const v = bignum_to_base(n, b);
    u64vec_print_rev(stream, v, "%lu");
    u64vec_free(v);
}

u64 bignum_div_eq_u64(BigNum *const n, u64 const d) {
    if (!n->len || d == 1) return 0;
    u64 const quo   = UINT64_MAX / d + (UINT64_MAX % d == d - 1);
    u64 const car   = 0 - quo * d;
    u64       carry = 0;
    for (u32 i = n->len - 1; i != UINT32_MAX; i--) {
        u64 m          = n->ptr[i] / d;
        u64 new_carry  = n->ptr[i] - m * d;
        m             += quo * carry;
        new_carry     += car * carry;
        u64 tmp        = new_carry / d;
        n->ptr[i]      = m + tmp;
        carry          = new_carry - tmp * d;
    }
    n->len -= n->ptr[n->len - 1] == 0;
    assert((!n->len || !n->ptr[n->len - 1]) && "division algorithm is wrong");
    return carry;
}

u64Vec bignum_to_base(BigNum const n, u64 const b) {
    BigNum    m = bignum_clone(n);
    u64 const cap =
        (m.len << 6) / (64 - __builtin_clzl(b - 1));  // log_2(n) / log_2(base) == log_base(n)
    u64Vec v = u64vec_new(cap);
    while (m.len) {
        u64 carry = bignum_div_eq_u64(&m, b);
        u64vec_push(&v, carry);
    }
    bignum_free(m);
    return v;
}
