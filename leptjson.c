#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free() */
#include <stdio.h>
#include <string.h>  /* memcpy() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>

/* 判断 c 数组的第一个元素是否 ch，并返回下一个指针 */
#define EXPECT(c, ch)\
    do {\
        assert(*c->json == (ch));\
        c->json++;\
    } while(0)

#define ISDIGIT(ch) \
    ((ch) >= '0' && (ch) <= '9')

#define ISDIGIT1TO9(ch) \
    ((ch) >= '1' && (ch) <= '9')


/**
 * 函数间传递参数：减少解析函数之间传递多个参数
 */
typedef struct {
    const char *json;
} lept_context;

/**
 * 去除有效文本前面所有的空白字符
 *
 * @param c
 */
static void lept_parse_whitespace(lept_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

/**
 * 解析 null
 *
 * @param c
 * @param v
 * @param judge_arr
 * @return
 */
static int lept_parse_null(lept_context *c, lept_value *v) {
    EXPECT(c, 'n');
    const char *p = "ull";
    while (*p != '\0') {
        if (c->json[0] != *p) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        c->json++;
        p++;
    }
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}

/**
 * 解析 null、true、false
 *
 * @param c
 * @param v
 * @param literal
 * @param type
 * @return
 */
static int lept_parse_literal(lept_context *c, lept_value *v, const char *literal, lept_type type) {

    /* C 语言中表示数组长度、索引值 */
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}


/**
 * 解析 number
 *
 * @param c
 * @param v
 * @return
 */
static int lept_parse_number(lept_context *c, lept_value *v) {
    const char *p = c->json;

    /* 负号 */
    if (*p == '-') {
        p++;
    }

    /* 整数 */
    if (*p == '0') {
        p++;
    } else {
        if (!ISDIGIT1TO9(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    /* 小数 */
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    /* 指数 */
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    /* 转换成二进制，并处理过大的值 */
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

/**
 * 解析 JSON 值
 *
 * JSON-text = ws value ws
 * ws = *(%x20 / %x09 / %x0A / %x0D)
 * value = null / false / true
 * null  = "null"
 * false = "false"
 * true  = "true"
 * number = [ "-" ] int [ frac ] [ exp ]
 * int = "0" / digit1-9 *digit
 * frac = "." 1*digit
 * exp = ("e" / "E") ["-" / "+"] 1*digit
 *
 * @param c
 * @param v
 * @return
 */
static int lept_parse_value(lept_context *c, lept_value *v) {
    /* 根据首字符选择判断分支 */
    switch (*c->json) {
        case 'n':
            return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't':
            return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':
            return lept_parse_literal(c, v, "false", LEPT_FALSE);
        default:
            return lept_parse_number(c, v);
        case '\0':
            return LEPT_PARSE_EXPECT_VALUE;
    }
}

/**
 * 解析 JSON
 *
 * @param v
 * @param json
 * @return
 */
int lept_parse(lept_value *v, const char *json) {
    assert(v != NULL);

    lept_context c;
    int ret;
    c.json = json;
    v->type = LEPT_NULL;

    /* 去除空白、换行符、制表符 */
    lept_parse_whitespace(&c);

    /**
     * 解析结果并判断结果
     * JSON 文本应该有 3 部分：JSON-text = ws value ws；
     * 以下判断第三部分，即解析空白然后检查 JSON 文本是否完结
     */
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        /* 解析成功后，再跳过后面的空白，判断是否已到末尾 */
        lept_parse_whitespace(&c);
        if (*(c.json) != '\0') {
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

/**
 * 获取 JSON 类型（包括 null、true、false）
 *
 * @param v
 * @return
 */
lept_type lept_get_type(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}

/**
 * 获取 JSON 值 boolean
 *
 * @param v
 * @return
 */
int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type;
}

/**
 * 设置 JSON 值 boolean
 *
 * @param v
 * @param b
 */
void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = b? LEPT_TRUE: LEPT_FALSE;
}

/**
 * 获取 JSON 值 number
 *
 * @param v
 * @return
 */
double lept_get_number(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

/**
 * 设置 JSON 值 number
 *
 * @param v
 * @param n
 */
void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

/**
 * 释放内存
 *
 * @param v
 */
void lept_free(lept_value *v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING) {
        free(v->u.s.s);
    }
    /* 置空，避免重复释放 */
    v->type = LEPT_NULL;
}

/**
 * 获取 JSON 值 string
 *
 * @param v
 * @return
 */
const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

/**
 * 获取 JSON 值 string 长度
 *
 * @param v
 * @return
 */
size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

/**
 *
 * @param v
 * @param s
 * @param len
 */
void lept_set_string(lept_value *v, const char *s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    /* 为字符串分配内存，把内容从 s 复制到 v->u.s.s，最后一位置为 '\0' */
    v->u.s.s = (char *) malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}
