#include "leptjson.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#define EXPECT(c, ch) do { assert(*(c)->json == (ch)); (c)->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PASS_DIGITS(p) do { while (ISDIGIT(*(p))) { (p)++;} } while(0)

typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
} lept_context;

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

static void *lept_context_push(lept_context *c, size_t size) {
    void *ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char *) realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void *lept_context_pop(lept_context *c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

/* ws = *(%x20 / %x09 / %x0A / %x0D) */
static void lept_parse_whitespace(lept_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

/* "null" / "true" / "false" */
static int lept_parse_literal(lept_context *c, lept_value *v, const char *literal, lept_type t) {
    EXPECT(c, literal[0]);
    literal++;
    int i;
    for (i = 0; literal[i]; ++i) {
        if (literal[i] != c->json[i]) {
            return LEPT_PARSE_INVALID_VALUE;
        }
    }
    c->json += i;
    v->type = t;
    return LEPT_PARSE_OK;
}

/*
 * number = [ "-" ] int [ frac ] [ exp ]
 * int = "0" / digit1-9 *digit
 * frac = "." 1*digit
 * exp = ("e" / "E") ["-" / "+"] 1*digit
 * */
static int lept_parse_number(lept_context *c, lept_value *v) {
    const char *p = c->json;

    /* validate number begin */
    /* negative */
    if (*p == '-') {
        p++;
    }

    /* int part */
    if (ISDIGIT1TO9(*p)) {
        p++;
        PASS_DIGITS(p);
    } else if (*p == '0') {
        p++;
    } else {
        return LEPT_PARSE_INVALID_VALUE;
    }

    /* frac part */
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        p++;
        PASS_DIGITS(p);
    }

    /* exp part */
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        p++;
        PASS_DIGITS(p);
    }
    /* validate number end */

    v->u.n = strtod(c->json, NULL);
    c->json = p;
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL)) {
        v->type = LEPT_NULL;
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

#define PUTC(c, ch) do { *(char *)lept_context_push((c), 1) = (ch);} while(0)

static int lept_parse_string(lept_context *c, lept_value *v) {
    size_t head = c->top, len;
    const char *p;
    EXPECT(c, '\"');
    p = c->json;
    while (1) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char *) lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0':
                c->top = head;
                return LEPT_PARSE_MISS_QUOTATION_MARK;
            default:
                PUTC(c, ch);
        }
    }
}

/* value = null / false / true / number / string */
static int lept_parse_value(lept_context *c, lept_value *v) {
    switch (*c->json) {
        case 'n' :
            return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't' :
            return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f' :
            return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '\"' :
            return lept_parse_string(c, v);
        case '\0' :
            return LEPT_PARSE_EXPECT_VALUE;
        default:
            return lept_parse_number(c, v);
    }
}

/* json-text = ws value ws */
int lept_parse(lept_value *v, const char *json) {
    lept_context c;
    int ret;

    assert(v != NULL);
    assert(json != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json) {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

lept_type lept_get_type(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}

void lept_free(lept_value *v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING) {
        free(v->u.s.s);
    }
    v->type = LEPT_NULL;
}

int lept_get_boolean(const lept_value *v) {
    assert(v != NULL);
    if (v->type == LEPT_TRUE) {
        return 1;
    } else if (v->type == LEPT_FALSE) {
        return 0;
    } else {
        assert(0);
    }
}

void lept_set_boolean(lept_value *v, int b) {
    assert(v != NULL);
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value *v, double n) {
    assert(v != NULL);
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

const char *lept_get_string(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

void lept_set_string(lept_value *v, const char *s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char *) malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}
